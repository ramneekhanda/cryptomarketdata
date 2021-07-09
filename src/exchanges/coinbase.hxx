#ifndef COINBASE_H_
#define COINBASE_H_

#include <chrono>
#include <sstream>

#include <rapidjson/document.h>
#include <date/tz.h>

#include "../exchange_connect.hxx"
#include "../marketdata.hxx"

namespace EC {
  using namespace std;
  using namespace rapidjson;

  class CoinBase : public Exchange {
    const string coinbase_endpoint = "wss://ws-feed.pro.coinbase.com";
    const string ticker_str = "{{\"type\":\"subscribe\",\"channels\":[{{\"name\":\"ticker\",\"product_ids\":[\"{}\"]}}]}}";
    const string name = "COINBASE";

    Document d;
    websocketpp::connection_hdl conHandle;
    ExchangeConnectorPtr exCon;
    ExchangeEventBusPtr evBus;
    map<string, vector<MD::Channel>> subscriptions;

    uint64_t millisFromDate(const std::string& s) {
      using namespace std::chrono;
      using sys_milliseconds = time_point<system_clock, microseconds>;
      sys_milliseconds pt;
      std::istringstream is(s);
      date::from_stream(is, "%FT%TZ", pt);
      cout << s << endl;
      return pt.time_since_epoch().count();
    }
  public:

    CoinBase(ExchangeConnectorPtr exCon, ExchangeEventBusPtr evBus) {
      this->exCon = exCon;
      this->evBus = evBus;
    }

    std::string const& getName() {
      return name;
    }

    void connect() {
      ErrorCode ec;
      ASIOClient::connection_ptr con;

      con = exCon->getConnection(coinbase_endpoint, ec);

      if (ec) {
        cout << "> Connect initialization error: " << ec.message() << endl;
        return;
      }

      con->set_open_handler([this](weak_ptr<void>) {
        cout << "Connection is now open" << endl;
        for (auto product_chan : subscriptions) {
          for (auto chan : product_chan.second)
            this->subscribe(product_chan.first, chan);
        }
      });

      con->set_message_handler([this](std::weak_ptr<void>, ASIOClient::message_ptr msg) {

        d.Parse(msg->get_payload().c_str());
        if (d.HasMember("type") && std::string(d["type"].GetString()) == "ticker") {
          MD::EventPtr evPtr = MD::EventPtr(new MD::Event());

          const string symbol(d["product_id"].GetString());

          evPtr->eventType = MD::Event::EventType::TRADE;
          MD::Trade &t = evPtr->t;

          t.price = stod(d["price"].GetString());
          t.time = millisFromDate(d["time"].GetString()); // time example -- "time": "2017-09-02T17:05:49.250000Z"
          t.volume = stod(d["last_size"].GetString());
          t.side = (d["side"].GetString() == std::string("buy")) ? MD::Side::BUY : MD::Side::SELL;

          evBus->publish(getName(), symbol, MD::Channel::TICKER, evPtr);
        }
      });

      con->set_fail_handler([](weak_ptr<void>) {
        cout << "Connection error" << endl;
      });

      conHandle = con->get_handle();
      exCon->connect(con);
    }

    void disconnect() {

    }

    void subscribe(const string& product, MD::Channel chan) {
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
  };

}

#endif // COINBASE_H_
