#ifndef COINBASE_H_
#define COINBASE_H_

#include <rapidjson/document.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "../exchange_connect.hxx"
#include "../exchange_eventbus.hxx"
#include "../marketdata.hxx"
#include "../utils.hxx"

namespace EC {

  class CoinBase : public Exchange {
  protected:
    static const std::string coinbase_endpoint;
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

      if (d.HasMember("type") && std::string(d["type"].GetString()) == "ticker") {
        onTickerMessage();
      }
      else if (d.HasMember("type") && std::string(d["type"].GetString()) == "subscriptions") {
        onSubscriptionsMessage();
      } else {
        SPDLOG_WARN("received unhandled message on  {} - {}", getName(), msg->get_payload());
      }
    }

    void onSubscriptionsMessage() {
      using namespace std;

      std::set<string> symbolsSub, symbolsUnsub;
      SPDLOG_DEBUG("subscriptions message on {}", getName());

      if (!d.HasMember("channels") || !d["channels"].IsArray())
        return;
      const rapidjson::Value &chans = d["channels"];
      for (auto &item : chans.GetArray()) {
        if (item.IsObject() && item.HasMember("name") && item["name"].IsString()  && item.HasMember("product_ids") && item["product_ids"].IsArray()) {
          if (item["name"] == "ticker") {
            set<string> ticker_set;
            auto &ticker_arr = item["product_ids"];
            auto &ticker_subs = subscriptions[MD::Channel::TICKER];
            for (auto &t : ticker_arr.GetArray()) ticker_set.insert(t.GetString());
            for (auto &tSym : ticker_subs) {
              if (ticker_set.find(tSym) == ticker_set.end()) {
                symbolsSub.insert(tSym);
              }
            }
            for (auto &tSym : ticker_set) {
              if (ticker_subs.find(tSym) == ticker_subs.end()) {
                symbolsUnsub.insert(tSym);
              }
            }
          }
        }
      }
      for (auto &s : symbolsSub)
        subscribeInternal(s, MD::Channel::TICKER);
      for (auto &s : symbolsUnsub)
        unsubscribe(s, MD::Channel::TICKER);
    }

    void onTickerMessage() {
      using namespace std;
      if (!d.HasMember("price") || !d.HasMember("time") || !d.HasMember("last_size") || !d.HasMember("side") || !d.HasMember("best_bid") || !d.HasMember("best_ask"))
        return;

      MD::TradeEventPtr evPtr = MD::TradeEventPtr(new MD::TradeEvent());
      const string symbol(d["product_id"].GetString());

      evPtr->eventType = MD::Event::EventType::TRADE;
      MD::Trade &t = evPtr->t;

      t.price = stod(d["price"].GetString());
      t.time = Utils::microsFromDate(d["time"].GetString()); // time example -- "time": "2017-09-02T17:05:49.250000Z"
      t.volume = stod(d["last_size"].GetString());
      t.side = (d["side"].GetString() == std::string("buy")) ? MD::Side::BUY : MD::Side::SELL;
      t.bid = stod(d["best_bid"].GetString());
      t.ask = stod(d["best_ask"].GetString());

      SPDLOG_INFO("tradeevent {}/{}/{} - {}", getName(), MD::ChannelName[MD::Channel::TICKER], symbol, t);

      evBus->publish(getName(), symbol, MD::Channel::TICKER, evPtr);
    }

  public:
    CoinBase() {

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
      con = exCon->getConnection(coinbase_endpoint, ec);

      if (ec) {
        SPDLOG_ERROR("connection error on {}", getName());
        return;
      }

      con->set_open_handler(bind(&CoinBase::openHandler, this, std::placeholders::_1));
      con->set_close_handler(bind(&CoinBase::closeHandler, this, std::placeholders::_1));
      con->set_message_handler(bind(&CoinBase::onMessageHandler, this, std::placeholders::_1, std::placeholders::_2));
      con->set_fail_handler(bind(&CoinBase::closeHandler, this, std::placeholders::_1));

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
  const std::string CoinBase::coinbase_endpoint("wss://ws-feed.pro.coinbase.com");
  const std::string CoinBase::ticker_subscribe_template("{{\"type\":\"subscribe\",\"channels\":[{{\"name\":\"ticker\",\"product_ids\":[\"{}\"]}}]}}");
  const std::string CoinBase::ticker_unsubscribe_template("{{\"type\":\"unsubscribe\",\"channels\":[{{\"name\":\"ticker\",\"product_ids\":[\"{}\"]}}]}}");

  const std::string CoinBase::name("COINBASE");
  int CoinBase::registerSelf = ExchangeConnector::registerExchange(CoinBase::name, ExchangePtr(new CoinBase()));
}
#endif // COINBASE_H_
