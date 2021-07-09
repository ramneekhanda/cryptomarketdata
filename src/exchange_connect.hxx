#ifndef EXCHANGE_CONNECT_H_
#define EXCHANGE_CONNECT_H_

#include <memory>
#include <thread>
#include <unordered_map>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <rxcpp/rx.hpp>
#include <fmt/core.h>

#include "marketdata.hxx"

namespace EC {
  using namespace std;
  class ExchangeConnector;
  class ExchangeEventBus;

  typedef websocketpp::lib::asio::ssl::context SSLContext;
  typedef websocketpp::client<websocketpp::config::asio_tls_client> ASIOClient;

  typedef websocketpp::lib::error_code ErrorCode;
  typedef std::shared_ptr<SSLContext> ContextPtr;
  typedef shared_ptr<ExchangeConnector> ExchangeConnectorPtr;
  typedef shared_ptr<ExchangeEventBus> ExchangeEventBusPtr;

  class ExchangeConnector {
  private:
    ASIOClient client;
    std::shared_ptr<std::thread> feedThread;

    /** @Brief Initializes the Exchange connector
     */
    void init() {

      client.set_access_channels(websocketpp::log::alevel::all);
      client.clear_access_channels(websocketpp::log::alevel::frame_payload);
      client.set_error_channels(websocketpp::log::elevel::all);

      client.init_asio();
      client.start_perpetual();
      client.set_message_handler([](std::weak_ptr<void>, ASIOClient::message_ptr msg) {
        std::cout << "client message received: " << msg->get_payload() << std::endl;
      });

      client.set_tls_init_handler([](std::weak_ptr<void>) -> ContextPtr {
        std::cout << "init tls" << std::endl;
        ContextPtr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

        try {
          ctx->set_options(SSLContext::default_workarounds |
                           SSLContext::no_sslv2 |
                           SSLContext::no_sslv3 |
                           SSLContext::single_dh_use);
        } catch (std::exception &e) {
          std::cout << "Error in context pointer: " << e.what() << std::endl;
        }
        return ctx;
      });
      feedThread.reset(new std::thread(&ASIOClient::run, &client));
    }

  protected:

    /** @Brief returns the client
     *  @return <b>{nil}</b> Return value description
     */
    ASIOClient& getClient() {
      return client;
    }
  public:
    /** @Brief Constructor will initialize and setup the websocketpp context and create a thread
     *  that the client will need to process incoming messages on
     */
    ExchangeConnector() {
      init();
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

  class Exchange {
    public:
      void connect();
      void disconnect();
      std::string const& getName();
  };

  class ExchangeEventBus {
    typedef rxcpp::subjects::subject<MD::EventPtr> EventSubject;
    typedef std::shared_ptr<EventSubject> EventSubjectPtr;
    typedef std::unordered_map<std::string, EventSubjectPtr> EventBus;

    EventBus eventBus;

    void ensureTopicExists(const std::string &topic) {
      EventBus::iterator itr = eventBus.find(topic);
      if (itr == eventBus.end()) {
        eventBus.insert({topic, EventSubjectPtr(new EventSubject())});
      }
    }
  public:
    void publish(const std::string &exchange, const std::string &symbol, MD::Channel chan, MD::EventPtr event) {
      std::string topic = exchange + "_" + symbol + "_" + MD::ChannelName[chan];

      ensureTopicExists(topic);
      eventBus[topic]->get_subscriber().on_next(event);
    }

    template <typename T>
    void subscribe(const std::string &exchange, const std::string& symbol, MD::Channel chan, T subscriber) {
      std::string topic = exchange + "_" + symbol + "_" + MD::ChannelName[chan];

      ensureTopicExists(topic);
      auto observable = eventBus[topic]->get_observable();
      observable.subscribe(subscriber);
    }
  };
}
#endif // EXCHANGE_CONNECT_H_
