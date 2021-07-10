#ifndef COINBASE_H_
#define COINBASE_H_

#include <chrono>
#include <sstream>

#include <rapidjson/document.h>
#include <date/tz.h>

#include "../exchange_connect.hxx"
#include "../exchange_eventbus.hxx"
#include "../marketdata.hxx"

namespace EC {
  using namespace std;
  using namespace rapidjson;

  class CoinBase : public Exchange {
  protected:
    static const string coinbase_endpoint;
    static const string ticker_str;
    static const string name;
    static int registerSelf;

    Document d;
    websocketpp::connection_hdl conHandle;
    ExchangeConnectorPtr exCon = ExchangeConnector::getInstance();
    ExchangeEventBusPtr evBus = ExchangeEventBus::getInstance();
    map<string, vector<MD::Channel>> subscriptions;

    uint64_t microsFromDate(const std::string& s) {
      using namespace std::chrono;
      using sys_micros = time_point<system_clock, microseconds>;
      sys_micros pt;
      std::istringstream is(s);
      date::from_stream(is, "%FT%TZ", pt);
      return pt.time_since_epoch().count();
    }

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
      string ticker_subs = fmt::format(ticker_str, product);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        exCon->sendOnHandle(conHandle, ticker_subs.c_str(), ec);
        if (ec) {
          cout << "Subscription send error on coinbase: " << ec.message() << endl;
        }
      }
      subscriptions[product].push_back(chan);
    }
  public:
    CoinBase() {

    }

    std::string const& getName() {
      return name;
    }

    void connect() {
      ErrorCode ec;
      if (isConnectedOrConnecting())
        return;

      ASIOClient::connection_ptr con;
      con = exCon->getConnection(coinbase_endpoint, ec);

      if (ec) {
        cout << "COINBASE: connect initialization error: " << ec.message() << endl;
        return;
      }

      con->set_open_handler([this](weak_ptr<void>) {
        cout << "COINBASE: connection is now open" << endl;
        for (auto product_chan : subscriptions) {
          for (auto chan : product_chan.second)
            this->subscribeInternal(product_chan.first, chan);
        }
      });

      con->set_close_handler([](weak_ptr<void>) {
        cout << "COINBASE: connection closed" << endl;
      });

      con->set_termination_handler([](weak_ptr<void>) {
        cout << "COINBASE: connection terminated" << endl;
      });

      con->set_message_handler([this](std::weak_ptr<void>, ASIOClient::message_ptr msg) {
        d.Parse(msg->get_payload().c_str());
        if (d.HasMember("type") && std::string(d["type"].GetString()) == "ticker") {
          MD::EventPtr evPtr = MD::EventPtr(new MD::Event());

          const string symbol(d["product_id"].GetString());

          evPtr->eventType = MD::Event::EventType::TRADE;
          MD::Trade &t = evPtr->t;

          t.price = stod(d["price"].GetString());
          t.time = microsFromDate(d["time"].GetString()); // time example -- "time": "2017-09-02T17:05:49.250000Z"
          t.volume = stod(d["last_size"].GetString());
          t.side = (d["side"].GetString() == std::string("buy")) ? MD::Side::BUY : MD::Side::SELL;
          t.bid = stod(d["best_bid"].GetString());
          t.ask = stod(d["best_ask"].GetString());
          evBus->publish(getName(), symbol, MD::Channel::TICKER, evPtr);
        }
      });

      con->set_fail_handler([](weak_ptr<void>) {
        cout << name << ": Connection error" << endl;
      });

      conHandle = con->get_handle();
      exCon->connect(con);
    }

    void disconnect() {
      websocketpp::lib::error_code ec;
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      ec.clear();

      con->terminate(ec);
      if (ec) {
        cout << "COINBASE: terminating connection caused an error: " << ec.message() << endl;
      }
    }

    void subscribe(const string& product, MD::Channel chan) {
      if (subscriptions.find(product) != subscriptions.end()
          && std::find(subscriptions[product].begin(), subscriptions[product].end(), chan) != subscriptions[product].end()) {
          return; // we are already subscribed
      }
      subscribeInternal(product, chan);
    }
  };
  const string CoinBase::coinbase_endpoint("wss://ws-feed.pro.coinbase.com");
  const string CoinBase::ticker_str("{{\"type\":\"subscribe\",\"channels\":[{{\"name\":\"ticker\",\"product_ids\":[\"{}\"]}}]}}");
  const string CoinBase::name("COINBASE");
  int CoinBase::registerSelf = ExchangeConnector::registerExchange(CoinBase::name, ExchangePtr(new CoinBase()));
}
#endif // COINBASE_H_
