#ifndef EXCHANGE_CONNECT_H_
#define EXCHANGE_CONNECT_H_

#include <memory>
#include <thread>
#include <unordered_map>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <rxcpp/rx.hpp>

#include "marketdata.hxx"

namespace EC {
  using namespace std;

  class ExchangeEventBus;

  class ExchangeConnector;
  typedef shared_ptr<ExchangeConnector> ExchangeConnectorPtr;


  typedef websocketpp::lib::asio::ssl::context SSLContext;
  typedef websocketpp::client<websocketpp::config::asio_tls_client> ASIOClient;

  typedef websocketpp::lib::error_code ErrorCode;
  typedef shared_ptr<SSLContext> ContextPtr;

  /**
   * Abstract class that represent an instance of an exchange
   **/
  class Exchange {
    public:
      virtual void connect() = 0;
      virtual void disconnect() = 0;
      virtual void subscribe(const string&, MD::Channel) = 0;
      virtual std::string const& getName() = 0;
      virtual ~Exchange() {}
  };
  typedef unique_ptr<Exchange> ExchangePtr;
  typedef unordered_map<std::string, unique_ptr<Exchange>> ExchangeMap;

  // FIXME make this class threadsafe
  class ExchangeConnector {
    shared_ptr<ASIOClient> client;
    friend ExchangeEventBus;

  protected:
    std::shared_ptr<std::thread> feedThread;
    static ExchangeConnectorPtr self;
    static ExchangeMap exchangeMap;

    /** @Brief Initializes the Exchange connector
     */
    void initInternal() {
      client.reset(new ASIOClient());

      // disable logging completely
      getClient().set_access_channels(websocketpp::log::alevel::none);
      getClient().set_error_channels(websocketpp::log::elevel::none);

      getClient().init_asio();
      getClient().start_perpetual();
      getClient().set_tls_init_handler([](std::weak_ptr<void>) -> ContextPtr {
      ContextPtr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

        try {
          ctx->set_options(SSLContext::default_workarounds |
                           SSLContext::no_sslv2 |
                           SSLContext::no_sslv3 |
                           SSLContext::single_dh_use);
        } catch (std::exception &e) {
          std::cout << "error in context pointer: " << e.what() << std::endl;
        }
        return ctx;
      });
      feedThread.reset(new std::thread(&ASIOClient::run, client));
    }

    /** @Brief returns the client
     *  @return <b>{nil}</b> Return value description
     */
    inline ASIOClient& getClient() {
      return *client;
    }

    /** @Brief Constructor will initialize and setup the websocketpp context and create a thread
     *  that the client will need to process incoming messages on
     */
    ExchangeConnector() {

    }

    void shutdownInternal() {
      for (auto &conn : exchangeMap) {
        conn.second->disconnect();
      }
      getClient().stop_perpetual();
      feedThread->join();
      self.reset(); //- we do not do this as this wou
      for (auto &p : exchangeMap) {
        std::cout << p.first << "-" << p.second << endl;
      }
    }

    void shutdown() { shutdownInternal(); }

    void init() { initInternal(); }

  public:

    static ExchangeConnectorPtr getInstance() {
      if (!self) {
        self = ExchangeConnectorPtr(new ExchangeConnector());
      }
      return self;
    }

    static int registerExchange(const std::string& name, ExchangePtr && exchange) {
      exchangeMap[name] = std::move(exchange);
      cout << "registered exchange with name - " << name << endl;
      return 0;
    }

    bool ensureConnected(const std::string& exchange) {
      // FIXME unnecessary allocation
      ExchangePtr &p = exchangeMap[exchange];
      if (p) {
        p->connect();
        return true;
      }
      else
        return false;
    }


    void subscribe(const std::string& exchange, const std::string& symbol, MD::Channel chan) {
      ExchangePtr &p = exchangeMap[exchange];
      // FIXME silent return
      if (p) {
        p->subscribe(symbol, chan);
      }
    }

    ASIOClient::connection_ptr getConnection(const std::string& uri, ErrorCode &ec) {
      return getClient().get_connection(uri, ec);
    }

    void connect(ASIOClient::connection_ptr con) {
      getClient().connect(con);
    }

    ASIOClient::connection_ptr getConnectionFromHandle(websocketpp::connection_hdl hdl, ErrorCode &ec) {
      return getClient().get_con_from_hdl(hdl, ec);
    }

    void sendOnHandle(websocketpp::connection_hdl hdl, const std::string &msg, ErrorCode &ec) {
      getClient().send(hdl, msg.c_str(), msg.length(), websocketpp::frame::opcode::TEXT, ec);
    }
  };

  ExchangeConnectorPtr ExchangeConnector::self = nullptr;
  ExchangeMap ExchangeConnector::exchangeMap;
}

#endif // EXCHANGE_CONNECT_H_
