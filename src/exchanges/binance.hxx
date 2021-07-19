#ifndef BINANCE_H_
#define BINANCE_H_

#include <functional>

#include <rapidjson/document.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "../exchange_connect.hxx"
#include "../exchange_eventbus.hxx"
#include "../marketdata.hxx"
#include "../utils.hxx"

namespace EC {

  class Binance : public Exchange {
  protected:
    static const std::string binance_endpoint;
    static const std::string ticker_subscribe_template;
    static const std::string ticker_unsubscribe_template;
    static const std::string name;
    static int registerSelf;

    rapidjson::Document d;
    websocketpp::connection_hdl conHandle;
    ExchangeConnectorPtr exCon;
    ExchangeEventBusPtr evBus;
    std::map<MD::Channel, std::set<std::string>> subscriptions;
    std::map<uint64_t, std::tuple<bool, MD::Channel, std::string>> reqAwaiting;
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

    void subscribeInternal(const std::string& symbol, MD::Channel chan) {
      using namespace std;

      ErrorCode ec;
      string product = symbol;
      std::transform(product.begin(), product.end(), product.begin(), ::tolower);

      auto time_now_nanos = std::chrono::high_resolution_clock::now().time_since_epoch().count();
      string ticker_subs = fmt::format(ticker_subscribe_template, product, time_now_nanos);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        SPDLOG_INFO("sending subscription {}/{}/{}", getName(), MD::ChannelName[chan], product);
        exCon->sendOnHandle(conHandle, ticker_subs.c_str(), ec);
        if (ec) {
          SPDLOG_ERROR("error in response to subscription request for {} on channel {} - {}", product, MD::ChannelName[chan], ec.message());
          return;
        }
      }
      reqAwaiting.insert(pair<uint64_t, tuple<bool, MD::Channel, string>>(time_now_nanos, tuple<bool, MD::Channel, string>(true, chan, product)));
    }

    void unsubscribeInternal(const std::string& product, MD::Channel chan) {
      using namespace std;

      ErrorCode ec;
      auto time_now_nanos = std::chrono::high_resolution_clock::now().time_since_epoch().count();
      string ticker_unsubs = fmt::format(ticker_unsubscribe_template, product, time_now_nanos);
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      if (con->get_state() == websocketpp::session::state::open) {
        SPDLOG_INFO("sending unsubscription {}/{}/{}", getName(), MD::ChannelName[chan], product);
        exCon->sendOnHandle(conHandle, ticker_unsubs.c_str(), ec);
        if (ec) {
          SPDLOG_ERROR("error in response to unsubscription request for {} on channel {} - {}", product, MD::ChannelName[chan], ec.message());
        }
      }
      reqAwaiting.insert(pair<uint64_t, tuple<bool, MD::Channel, string>>(time_now_nanos, tuple<bool, MD::Channel, string>(false, chan, product)));
    }

    void openHandler(websocketpp::connection_hdl) {
      using namespace std;
      SPDLOG_INFO("{} connected", getName());

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
      SPDLOG_INFO("{} disconnected", getName());

      evBus->publish(getName(), MD::EventPtr(new MD::DisconnectEvent()));
    }

    void onMessageHandler(websocketpp::connection_hdl, ASIOClient::message_ptr msg) {
      SPDLOG_DEBUG("message on {} - {}", getName(), msg->get_payload());

      d.Parse(msg->get_payload().c_str());

      if (d.HasMember("e") && std::string(d["e"].GetString()) == "aggTrade") {
        onTickerMessage();
      }
      else if (d.HasMember("result") || d.HasMember("error"))  {
        onResponseMessage();
      } else {
        SPDLOG_WARN("received unhandled message on  {} - {}", getName(), msg->get_payload());
      }
    }

    void onResponseMessage() {
      // FIXME Deal with response messages
      SPDLOG_DEBUG("response message on {}", getName());
    }

    void onTickerMessage() {
      using namespace std;

      if (!d.HasMember("p") || !d.HasMember("T") || !d.HasMember("q") || !d.HasMember("m"))
        return;

      MD::TradeEventPtr evPtr = MD::TradeEventPtr(new MD::TradeEvent());
      string symbol(d["s"].GetString());
      std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

      evPtr->eventType = MD::Event::EventType::TRADE;
      MD::Trade &t = evPtr->t;

      t.price = stod(d["p"].GetString());
      t.time = d["T"].GetUint64() * 1000;
      t.volume = stod(d["q"].GetString());
      t.side = (d["m"].GetBool() == true) ? MD::Side::BUY : MD::Side::SELL;
      t.bid = 0;
      t.ask = 0;
      SPDLOG_INFO("tradeevent {}/{}/{} - {}", getName(), MD::ChannelName[MD::Channel::TICKER], symbol, t);
      evBus->publish(getName(), symbol, MD::Channel::TICKER, evPtr);
    }

  public:
    Binance() {

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
      con = exCon->getConnection(binance_endpoint, ec);

      if (ec) {
        SPDLOG_ERROR("connection error on {}", getName());
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
      SPDLOG_INFO("disconnecting {}", getName());

      std::unique_lock<std::mutex> lk(m);
      isDisconnectIssued = true;
      websocketpp::lib::error_code ec;
      ASIOClient::connection_ptr con = exCon->getConnectionFromHandle(conHandle, ec);
      ec.clear();
      con->close(websocketpp::close::status::normal, "goodbye!");
    }

    void subscribe(const std::string& product, MD::Channel chan) {
      if (subscriptions.find(chan) != subscriptions.end()
          && std::find(subscriptions[chan].begin(), subscriptions[chan].end(), product) != subscriptions[chan].end()) {
          return; // we are already subscribed
      }
      subscribeInternal(product, chan);
    }

    void unsubscribe(const std::string& product, MD::Channel chan) {
      if (subscriptions.find(chan) == subscriptions.end()
          || find(subscriptions[chan].begin(), subscriptions[chan].end(), product) == subscriptions[chan].end()) {
          return; // we are already unsubscribed
      }
      unsubscribeInternal(product, chan);
    }
  };
  const std::string Binance::binance_endpoint("wss://stream.binance.com:9443/ws/stream");
  const std::string Binance::ticker_subscribe_template("{{\"method\": \"SUBSCRIBE\",\"params\": [\"{}@aggTrade\"], \"id\": {}}}");
  const std::string Binance::ticker_unsubscribe_template("{{\"method\": \"UNSUBSCRIBE\",\"params\": [\"{}@aggTrade\"], \"id\": {}}}");

  const std::string Binance::name("BINANCE");
  int Binance::registerSelf = ExchangeConnector::registerExchange(Binance::name, ExchangePtr(new Binance()));
}
#endif // BINANCE_H_
