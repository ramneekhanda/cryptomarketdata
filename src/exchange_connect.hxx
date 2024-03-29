#ifndef EXCHANGE_CONNECT_H_
#define EXCHANGE_CONNECT_H_

#include <memory>
#include <thread>
#include <unordered_map>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <spdlog/spdlog.h>

#include "marketdata.hxx"

namespace EC {

  class ExchangeEventBus;

  class ExchangeConnector;
  typedef std::shared_ptr<ExchangeConnector> ExchangeConnectorPtr;

  typedef websocketpp::lib::asio::ssl::context SSLContext;
  typedef websocketpp::client<websocketpp::config::asio_tls_client> ASIOClient;

  typedef websocketpp::lib::error_code ErrorCode;
  typedef std::shared_ptr<SSLContext> ContextPtr;

  /**
   * Abstract class that represent an instance of an exchange
   **/
  class Exchange {
    public:
      virtual void connect() = 0;
      virtual void disconnect() = 0;
      virtual void subscribe(const std::string&, MD::Channel) = 0;
      virtual void unsubscribe(const std::string&, MD::Channel) = 0;
      virtual std::string const& getName() = 0;
      virtual ~Exchange() {}
  };
  typedef std::unique_ptr<Exchange> ExchangePtr;
  typedef std::unordered_map<std::string, std::unique_ptr<Exchange>> ExchangeMap;

  class ExchangeConnector {

    std::shared_ptr<ASIOClient> client;
    friend ExchangeEventBus;
    static std::mutex muExchangeMap;
  protected:
    std::shared_ptr<std::thread> feedThread;
    static ExchangeConnectorPtr self;
    static ExchangeMap exchangeMap; // guarded by muExchangeMap

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
      std::unique_lock<std::mutex> lk(muExchangeMap);
      for (auto &conn : exchangeMap) {
        conn.second->disconnect();
      }
      getClient().stop_perpetual();
      feedThread->join();
      self.reset(); //- we do not do this as this wou
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
      using namespace std;
      std::unique_lock<mutex> lk(muExchangeMap);
      exchangeMap[name] = std::move(exchange);
      SPDLOG_INFO("exchange registered - {}", name);
      return 0;
    }

    bool ensureConnected(const std::string& exchange) {
      using namespace std;
      // FIXME unnecessary allocation
      std::unique_lock<mutex> lk(muExchangeMap);
      ExchangePtr &p = exchangeMap[exchange];
      lk.unlock();
      if (p) {
        p->connect();
        return true;
      }
      else
        return false;
    }


    void subscribe(const std::string& exchange, const std::string& symbol, MD::Channel chan) {
      using namespace std;
      std::unique_lock<mutex> lk(muExchangeMap);
      ExchangePtr &p = exchangeMap[exchange];
      lk.unlock();
      // FIXME silent return
      if (p) {
        p->subscribe(symbol, chan);
      }
    }

    void unsubscribe(const std::string& exchange, const std::string& symbol, MD::Channel chan) {
      using namespace std;
      std::unique_lock<mutex> lk(muExchangeMap);
      ExchangePtr &p = exchangeMap[exchange];
      lk.unlock();
      // FIXME silent return
      if (p) {
        p->unsubscribe(symbol, chan);
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

  std::mutex ExchangeConnector::muExchangeMap;
  ExchangeConnectorPtr ExchangeConnector::self = nullptr;
  ExchangeMap ExchangeConnector::exchangeMap;
}

#endif // EXCHANGE_CONNECT_H_
