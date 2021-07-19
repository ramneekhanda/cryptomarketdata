#ifndef KRAKEN_H_
#define KRAKEN_H_

#include <rapidjson/document.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "../exchange_connect.hxx"
#include "../exchange_eventbus.hxx"
#include "../marketdata.hxx"
#include "../utils.hxx"

namespace EC {

  class Kraken : public Exchange {
  protected:
    static const std::string kraken_endpoint;
    static const std::string ticker_subscribe_template;
    static const std::string ticker_unsubscribe_template;
    static const std::string name;
    static int registerSelf;

    rapidjson::Document d;
    websocketpp::connection_hdl conHandle;
    ExchangeConnectorPtr exCon;
    ExchangeEventBusPtr evBus;
    std::map<MD::Channel, std::set<std::string>> subscriptions;
    bool isDisconnectIssued;
    std::mutex m;

    bool isConnectedOrConnecting() {
      ErrorCode ec;
      if (conHandle.lock()) {
        ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
        return (con->get_state() == websocketpp::session::state::open) || (con->get_state() == websocketpp::session::state::connecting);
      }
      return false;
    }

    void subscribeInternal(const std::string& product, MD::Channel chan) {
      using namespace std;

      ErrorCode ec;
      string ticker_subs = fmt::format(ticker_subscribe_template, product);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        SPDLOG_INFO("sending subscription {}/{}/{}", getName(), MD::ChannelName[chan], product);
        exCon->sendOnHandle(conHandle, ticker_subs.c_str(), ec);
        if (ec) {
          SPDLOG_ERROR("error in response to subscription request for {} on channel {} - {}", product, MD::ChannelName[chan], ec.message());
          return;
        }
      }
      subscriptions[chan].insert(product);
    }

    void unsubscribeInternal(const std::string& product, MD::Channel chan) {
      using namespace std;

      ErrorCode ec;
      string ticker_unsubs = fmt::format(ticker_unsubscribe_template, product);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        SPDLOG_INFO("sending unsubscription {}/{}/{}", getName(), MD::ChannelName[chan], product);
        exCon->sendOnHandle(conHandle, ticker_unsubs.c_str(), ec);
        if (ec) {
          SPDLOG_ERROR("error in response to unsubscription request for {} on channel {} - {}", product, MD::ChannelName[chan], ec.message());
        }
      }
      subscriptions[chan].erase(product);
    }

    void openHandler(websocketpp::connection_hdl) {
      SPDLOG_INFO("{} connected", getName());

      evBus->publish(getName(), MD::EventPtr(new MD::ConnectEvent()));
      for (auto chan_prod : subscriptions) {
        for (auto prod : chan_prod.second)
          this->subscribeInternal(prod, chan_prod.first);
      }
    }

    void closeHandler(websocketpp::connection_hdl) {
      SPDLOG_INFO("{} disconnected", getName());

      evBus->publish(getName(), MD::EventPtr(new MD::DisconnectEvent()));
    }

    void onMessageHandler(websocketpp::connection_hdl, ASIOClient::message_ptr msg) {
      SPDLOG_DEBUG("message on {} - {}", getName(), msg->get_payload());

      d.Parse(msg->get_payload().c_str());
      if (d.IsObject() && d.HasMember("event") && std::string(d["event"].GetString()) == "heartbeat") {
        // do nothing
      }
      if (d.IsObject() && d.HasMember("event") && std::string(d["event"].GetString()) == "subscriptionStatus") {
        onSubscriptionsMessage();
      }
      else if (d.IsArray() && d.GetArray().Size() == 4 && d[2].IsString() && (d[2].GetString() == std::string("trade"))) { // one of trade/ohlc/spread/book
        onTickerMessage();
      } else {
        SPDLOG_WARN("received unhandled message on  {} - {}", getName(), msg->get_payload());
      }
    }

    void onSubscriptionsMessage() {
      using namespace std;
      return;
      std::set<string> symbolsSub, symbolsUnsub;
      SPDLOG_DEBUG("subscriptions message on {}", getName());

      for (auto &s : symbolsSub)
        subscribeInternal(s, MD::Channel::TICKER);
      for (auto &s : symbolsUnsub)
        unsubscribe(s, MD::Channel::TICKER);
    }

    void onTickerMessage() {
      using namespace std;
      if (d.GetArray().Size() != 4 || !d[1].IsArray()) {
        SPDLOG_ERROR("unparsable message received from kraken");
        return;
      }
      const std::string& product = d[3].GetString();
      for (auto& trade : d[1].GetArray()) {
          MD::TradeEventPtr evPtr = MD::TradeEventPtr(new MD::TradeEvent());
          evPtr->eventType = MD::Event::EventType::TRADE;
          MD::Trade &t = evPtr->t;

          t.price = stod(trade[0].GetString());
          t.volume = stod(trade[1].GetString());
          t.time = stod(trade[2].GetString()) * 1000000; // time example -- "time": "2017-09-02T17:05:49.250000Z"
          t.side = (trade[3].GetString() == std::string("b")) ? MD::Side::BUY : MD::Side::SELL;
          t.bid = 0;
          t.ask = 0;

          SPDLOG_INFO("tradeevent {}/{}/{} - {}", getName(), MD::ChannelName[MD::Channel::TICKER], product, t);

          evBus->publish(getName(), product, MD::Channel::TICKER, evPtr);
      }
    }

  public:
    Kraken() {

    }

    std::string const& getName() {
      return name;
    }

    void connect() {
      using namespace std;

      ErrorCode ec;
      exCon = ExchangeConnector::getInstance();
      evBus = ExchangeEventBus::getInstance();
      std::unique_lock<std::mutex> lk(m);
      isDisconnectIssued = false;
      if (isConnectedOrConnecting())
        return;

      ASIOClient::connection_ptr con;
      con = exCon->getConnection(kraken_endpoint, ec);

      if (ec) {
        SPDLOG_ERROR("connection error on {}", getName());
        return;
      }

      con->set_open_handler(bind(&Kraken::openHandler, this, std::placeholders::_1));
      con->set_close_handler(bind(&Kraken::closeHandler, this, std::placeholders::_1));
      con->set_message_handler(bind(&Kraken::onMessageHandler, this, std::placeholders::_1, std::placeholders::_2));
      con->set_fail_handler(bind(&Kraken::closeHandler, this, std::placeholders::_1));

      conHandle = con->get_handle();
      exCon->connect(con);
    }

    void disconnect() {
      using namespace std;

      if (conHandle.expired()) return;

      SPDLOG_INFO("disconnecting {}", getName());

      std::unique_lock<std::mutex> lk(m);
      isDisconnectIssued = true;
      websocketpp::lib::error_code ec;
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      ec.clear();
      con->close(websocketpp::close::status::normal, "goodbye!");
    }

    void subscribe(const std::string& product, MD::Channel chan) {
      using namespace std;

      if (subscriptions.find(chan) != subscriptions.end()
          && std::find(subscriptions[chan].begin(), subscriptions[chan].end(), product) != subscriptions[chan].end()) {
          return; // we are already subscribed
      }
      subscribeInternal(product, chan);
    }

    void unsubscribe(const std::string& product, MD::Channel chan) {
      using namespace std;
      if (subscriptions.find(chan) == subscriptions.end()
          || find(subscriptions[chan].begin(), subscriptions[chan].end(), product) == subscriptions[chan].end()) {
          return; // we are already unsubscribed
      }
      unsubscribeInternal(product, chan);
    }
  };

  const std::string Kraken::kraken_endpoint("wss://ws.kraken.com");
  const std::string Kraken::ticker_subscribe_template("{{\"event\": \"subscribe\",\"pair\": [\"{}\"],\"subscription\": {{\"name\": \"trade\"}}}}");
  const std::string Kraken::ticker_unsubscribe_template("{{\"event\": \"unsubscribe\",\"pair\": [\"{}\"],\"subscription\": {{\"name\": \"trade\"}}}}");

  const std::string Kraken::name("KRAKEN");
  int Kraken::registerSelf = ExchangeConnector::registerExchange(Kraken::name, ExchangePtr(new Kraken()));
}

#endif // KRAKEN_H_
