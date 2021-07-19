#ifndef BINANCE_H_
#define BINANCE_H_

#define FMT_HEADER_ONLY

#include <functional>

#include <rapidjson/document.h>
#include <fmt/core.h>

#include "../exchange_connect.hxx"
#include "../exchange_eventbus.hxx"
#include "../marketdata.hxx"
#include "../utils.hxx"

namespace EC {
  using namespace std;
  using namespace rapidjson;

  class Binance : public Exchange {
  protected:
    static const string binance_endpoint;
    static const string ticker_subscribe_template;
    static const string ticker_unsubscribe_template;
    static const string name;
    static int registerSelf;

    Document d;
    websocketpp::connection_hdl conHandle;
    ExchangeConnectorPtr exCon;
    ExchangeEventBusPtr evBus;
    map<MD::Channel, set<string>> subscriptions;
    map<uint64_t, tuple<bool, MD::Channel, string>> reqAwaiting;
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

    void subscribeInternal(const string& symbol, MD::Channel chan) {
      ErrorCode ec;
      string product = symbol;
      std::transform(product.begin(), product.end(), product.begin(), ::tolower);

      auto time_now_nanos = std::chrono::high_resolution_clock::now().time_since_epoch().count();
      string ticker_subs = fmt::format(ticker_subscribe_template, product, time_now_nanos);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        std::cout << "sending - " << ticker_subs << std::endl;

        exCon->sendOnHandle(conHandle, ticker_subs.c_str(), ec);
        if (ec) {
          cout << "Subscription send error on binance: " << ec.message() << endl;
        }
      }
      reqAwaiting.insert(pair<uint64_t, tuple<bool, MD::Channel, string>>(time_now_nanos, tuple<bool, MD::Channel, string>(true, chan, product)));
    }

    void unsubscribeInternal(const string& product, MD::Channel chan) {
      ErrorCode ec;
      auto time_now_nanos = std::chrono::high_resolution_clock::now().time_since_epoch().count();
      string ticker_unsubs = fmt::format(ticker_unsubscribe_template, product, time_now_nanos);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        exCon->sendOnHandle(conHandle, ticker_unsubs.c_str(), ec);
        if (ec) {
          cout << "Unsubscription send error on binance: " << ec.message() << endl;
        }
      }
      reqAwaiting.insert(pair<uint64_t, tuple<bool, MD::Channel, string>>(time_now_nanos, tuple<bool, MD::Channel, string>(false, chan, product)));
    }

    void openHandler(websocketpp::connection_hdl) {
      std::cout << "connected" <<std::endl;
      evBus->publish(getName(), MD::EventPtr(new MD::ConnectEvent()));
      auto reqs = reqAwaiting;
      reqAwaiting.clear();
      for (auto chan_prod : subscriptions) {
        for (auto prod : chan_prod.second)
          this->subscribeInternal(prod, chan_prod.first);
      }

      for (auto uuid_req : reqs) {
        if (std::get<0>(uuid_req.second))
          this->subscribeInternal(std::get<2>(uuid_req.second), std::get<1>(uuid_req.second));
      }

      // FIXME reemit reqsawaiting and remove unsubscription requests
    }

    void closeHandler(websocketpp::connection_hdl) {
      std::cout << "disconnected" <<std::endl;

      evBus->publish(getName(), MD::EventPtr(new MD::DisconnectEvent()));
    }

    void onMessageHandler(websocketpp::connection_hdl, ASIOClient::message_ptr msg) {
      std::cout << "message - " << msg->get_payload() << std::endl;

      d.Parse(msg->get_payload().c_str());

      if (d.HasMember("e") && std::string(d["e"].GetString()) == "aggTrade") {
        onTickerMessage();
      }
      else if (d.HasMember("result") || d.HasMember("error"))  {
        onResponseMessage();
      } else {
        std::cout << "error message" << std::endl;
      }
    }

    void onResponseMessage() {
      // FIXME Deal with response messages
      std::cout << "response message" << std::endl;
    }

    void onTickerMessage() {

      if (!d.HasMember("p") || !d.HasMember("T") || !d.HasMember("q") || !d.HasMember("m"))
        return;

      MD::TradeEventPtr evPtr = MD::TradeEventPtr(new MD::TradeEvent());
      string symbol(d["s"].GetString());

      evPtr->eventType = MD::Event::EventType::TRADE;
      MD::Trade &t = evPtr->t;

      t.price = stod(d["p"].GetString());
      t.time = d["T"].GetUint64();
      t.volume = stod(d["q"].GetString());
      t.side = (d["m"].GetBool() == true) ? MD::Side::BUY : MD::Side::SELL;
      t.bid = 0;
      t.ask = 0;
      cout << "received event on " << getName() << symbol << endl;
      evBus->publish(getName(), symbol, MD::Channel::TICKER, evPtr);
    }

  public:
    Binance() {

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
      con = exCon->getConnection(binance_endpoint, ec);

      if (ec) {
        cout << "BINANCE: connect initialization error: " << ec.message() << endl;
        return;
      }

      con->set_open_handler(bind(&Binance::openHandler, this, std::placeholders::_1));
      con->set_close_handler(bind(&Binance::closeHandler, this, std::placeholders::_1));
      con->set_message_handler(bind(&Binance::onMessageHandler, this, std::placeholders::_1, std::placeholders::_2));
      con->set_fail_handler(bind(&Binance::closeHandler, this, std::placeholders::_1));

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
  const string Binance::binance_endpoint("wss://stream.binance.com:9443/ws/stream");
  const string Binance::ticker_subscribe_template("{{\"method\": \"SUBSCRIBE\",\"params\": [\"{}@aggTrade\"], \"id\": {}}}");
  const string Binance::ticker_unsubscribe_template("{{\"method\": \"UNSUBSCRIBE\",\"params\": [\"{}@aggTrade\"], \"id\": {}}}");

  const string Binance::name("BINANCE");
  int Binance::registerSelf = ExchangeConnector::registerExchange(Binance::name, ExchangePtr(new Binance()));
}
#endif // BINANCE_H_
