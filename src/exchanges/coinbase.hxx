#ifndef COINBASE_H_
#define COINBASE_H_

#define FMT_HEADER_ONLY

#include <rapidjson/document.h>
#include <fmt/core.h>

#include "../exchange_connect.hxx"
#include "../exchange_eventbus.hxx"
#include "../marketdata.hxx"
#include "../utils.hxx"

namespace EC {
  using namespace std;
  using namespace rapidjson;

  class CoinBase : public Exchange {
  protected:
    static const string coinbase_endpoint;
    static const string ticker_subscribe_template;
    static const string ticker_unsubscribe_template;
    static const string name;
    static int registerSelf;

    Document d;
    websocketpp::connection_hdl conHandle;
    ExchangeConnectorPtr exCon;
    ExchangeEventBusPtr evBus;
    map<MD::Channel, set<string>> subscriptions;
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

    void subscribeInternal(const string& product, MD::Channel chan) {
      ErrorCode ec;
      string ticker_subs = fmt::format(ticker_subscribe_template, product);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        exCon->sendOnHandle(conHandle, ticker_subs.c_str(), ec);
        if (ec) {
          cout << "Subscription send error on coinbase: " << ec.message() << endl;
        }
      }
      subscriptions[chan].insert(product);
    }

    void unsubscribeInternal(const string& product, MD::Channel chan) {
      ErrorCode ec;
      string ticker_unsubs = fmt::format(ticker_unsubscribe_template, product);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        exCon->sendOnHandle(conHandle, ticker_unsubs.c_str(), ec);
        if (ec) {
          cout << "Unsubscription send error on coinbase: " << ec.message() << endl;
        }
      }
      subscriptions[chan].erase(product);
      //remove_if(subscriptions[chan].begin(), subscriptions[chan].end(), [product](auto val){ return val == product; });
    }

    void openHandler(websocketpp::connection_hdl) {

      evBus->publish(getName(), MD::EventPtr(new MD::ConnectEvent()));
      for (auto chan_prod : subscriptions) {
        for (auto prod : chan_prod.second)
          this->subscribeInternal(prod, chan_prod.first);
      }
    }

    void closeHandler(websocketpp::connection_hdl) {
      evBus->publish(getName(), MD::EventPtr(new MD::DisconnectEvent()));
    }

    void onMessageHandler(websocketpp::connection_hdl, ASIOClient::message_ptr msg) {
      d.Parse(msg->get_payload().c_str());

      if (d.HasMember("type") && std::string(d["type"].GetString()) == "ticker") {
        onTickerMessage();
      }
      else if (d.HasMember("type") && std::string(d["type"].GetString()) == "subscriptions") {
        onSubscriptionsMessage();
      }
    }

    void onSubscriptionsMessage() {
      std::set<string> symbolsSub, symbolsUnsub;

      if (!d.HasMember("channels") || !d["channels"].IsArray())
        return;
      const Value &chans = d["channels"];
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
      evBus->publish(getName(), symbol, MD::Channel::TICKER, evPtr);
    }

  public:
    CoinBase() {

    }

    std::string const& getName() {
      return name;
    }

    void connect() {
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
        cout << "COINBASE: connect initialization error: " << ec.message() << endl;
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
      if (conHandle.expired()) return;

      std::unique_lock<std::mutex> lk(m);
      isDisconnectIssued = true;
      websocketpp::lib::error_code ec;
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      ec.clear();
      con->close(websocketpp::close::status::normal, "goodbye!");
    }

    void subscribe(const string& product, MD::Channel chan) {
      if (subscriptions.find(chan) != subscriptions.end()
          && std::find(subscriptions[chan].begin(), subscriptions[chan].end(), product) != subscriptions[chan].end()) {
          return; // we are already subscribed
      }
      subscribeInternal(product, chan);
    }

    void unsubscribe(const string& product, MD::Channel chan) {
      if (subscriptions.find(chan) == subscriptions.end()
          || find(subscriptions[chan].begin(), subscriptions[chan].end(), product) == subscriptions[chan].end()) {
          return; // we are already unsubscribed
      }
      unsubscribeInternal(product, chan);
    }
  };
  const string CoinBase::coinbase_endpoint("wss://ws-feed.pro.coinbase.com");
  const string CoinBase::ticker_subscribe_template("{{\"type\":\"subscribe\",\"channels\":[{{\"name\":\"ticker\",\"product_ids\":[\"{}\"]}}]}}");
  const string CoinBase::ticker_unsubscribe_template("{{\"type\":\"unsubscribe\",\"channels\":[{{\"name\":\"ticker\",\"product_ids\":[\"{}\"]}}]}}");

  const string CoinBase::name("COINBASE");
  int CoinBase::registerSelf = ExchangeConnector::registerExchange(CoinBase::name, ExchangePtr(new CoinBase()));
}
#endif // COINBASE_H_
