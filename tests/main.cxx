#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <chrono>
#include <condition_variable>

#include <spdlog/spdlog.h>
#include <doctest/doctest.h>

#include "../src/exchange_connect.hxx"
#include "../src/exchange_eventbus.hxx"
#include "../src/exchanges/coinbase.hxx"
#include "../src/exchanges/binance.hxx"
#include "../src/exchanges/kraken.hxx"

using namespace std::chrono_literals;

TEST_SUITE("COINBASE") {
  TEST_CASE("test coinbase btcusd feed") {
    std::mutex m;
    std::condition_variable flag;
    bool data_received = false;
    int connect_count = 0;
    int disconnect_count = 0;

    auto unsubscribeExEvents = EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", [&connect_count, &disconnect_count](MD::EventPtr e) {
      if (e->eventType == MD::Event::CONNECT) connect_count++;
      if (e->eventType == MD::Event::DISCONNECT) disconnect_count++;
    });

    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [&flag, &m, &data_received](MD::EventPtr e) {
      using namespace std::chrono;
      MD::TradeEventPtr p = std::dynamic_pointer_cast<MD::TradeEvent>(e);
      microseconds us = duration_cast<microseconds>(system_clock::now().time_since_epoch());
      std::unique_lock<std::mutex> lk(m);
      if (data_received) return; // dont assert more than once
      data_received = true;
      REQUIRE(p->t.price > 0);
      REQUIRE(p->t.volume > 0.);
      WARN(us.count() < (20000000 + p->t.time));
      REQUIRE(p->t.ask > p->t.bid);
      flag.notify_one();
    });

    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk);
    unsubscribe();
    EC::ExchangeEventBus::getInstance()->shutdown();
    REQUIRE(connect_count == 1);
    REQUIRE(disconnect_count == 1);
  }

  TEST_CASE("test coinbase btcusd subscribe unsubscribe") {
    std::mutex m;
    bool flag_me = true;
    bool data_received = false;
    std::condition_variable flag;

    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [&flag, &m, &flag_me, &data_received](MD::EventPtr) {
        std::unique_lock<std::mutex> lk(m);
        if (flag_me) {
            data_received = true;
            flag.notify_all();
        }
    });
    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk, [&data_received]{return data_received == true;});

    data_received = false;
    flag_me = false;
    lk.unlock();

    unsubscribe();
    sleep(1); // give it time to rest

    lk.lock();
    flag_me = true;
    REQUIRE(flag.wait_for(lk, 2s, [&data_received]{ return data_received == true; }) == false);

    EC::ExchangeEventBus::getInstance()->shutdown();
  }
}

TEST_SUITE("BINANCE") {
  TEST_CASE("test binance btcusdt feed") {
    std::mutex m;
    std::condition_variable flag;
    bool data_received = false;
    int connect_count = 0;
    int disconnect_count = 0;

    auto unsubscribeExEvents = EC::ExchangeEventBus::getInstance()->subscribe("BINANCE", [&connect_count, &disconnect_count](MD::EventPtr e) {
      if (e->eventType == MD::Event::CONNECT) connect_count++;
      if (e->eventType == MD::Event::DISCONNECT) disconnect_count++;
    });

    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("BINANCE", "BTCUSDT", MD::Channel::TICKER, [&flag, &m, &data_received](MD::EventPtr e) {
      using namespace std::chrono;
      MD::TradeEventPtr p = std::dynamic_pointer_cast<MD::TradeEvent>(e);
      microseconds us = duration_cast<microseconds>(system_clock::now().time_since_epoch());
      std::unique_lock<std::mutex> lk(m);
      if (data_received) return; // dont assert more than once
      data_received = true;
      REQUIRE(p->t.price > 0);
      REQUIRE(p->t.volume > 0.);
      WARN(us.count() < (20000000 + p->t.time));
      REQUIRE(p->t.ask == 0);
      REQUIRE(p->t.bid == 0);

      flag.notify_one();
    });

    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk);
    unsubscribe();
    EC::ExchangeEventBus::getInstance()->shutdown();
    REQUIRE(connect_count == 1);
    REQUIRE(disconnect_count == 1);
  }

  TEST_CASE("test binance btcusd subscribe unsubscribe") {
    std::mutex m;
    bool flag_me = true;
    bool data_received = false;
    std::condition_variable flag;

    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("BINANCE", "BTCUSDT", MD::Channel::TICKER, [&flag, &m, &flag_me, &data_received](MD::EventPtr) {
        std::unique_lock<std::mutex> lk(m);
        if (flag_me) {
            data_received = true;
            flag.notify_all();
        }
    });
    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk, [&data_received]{return data_received == true;});

    data_received = false;
    flag_me = false;
    lk.unlock();

    unsubscribe();
    sleep(1); // give it time to rest

    lk.lock();
    flag_me = true;
    REQUIRE(flag.wait_for(lk, 2s, [&data_received]{ return data_received == true; }) == false);

    EC::ExchangeEventBus::getInstance()->shutdown();
  }
}

TEST_SUITE("KRAKEN") {
  TEST_CASE("test kraken xbt/usd feed") {
    std::mutex m;
    std::condition_variable flag;
    bool data_received = false;
    int connect_count = 0;
    int disconnect_count = 0;

    auto unsubscribeExEvents = EC::ExchangeEventBus::getInstance()->subscribe("KRAKEN", [&connect_count, &disconnect_count](MD::EventPtr e) {
      if (e->eventType == MD::Event::CONNECT) connect_count++;
      if (e->eventType == MD::Event::DISCONNECT) disconnect_count++;
    });

    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("KRAKEN", "XBT/USD", MD::Channel::TICKER, [&flag, &m, &data_received](MD::EventPtr e) {
      using namespace std::chrono;
      MD::TradeEventPtr p = std::dynamic_pointer_cast<MD::TradeEvent>(e);
      microseconds us = duration_cast<microseconds>(system_clock::now().time_since_epoch());
      std::unique_lock<std::mutex> lk(m);
      if (data_received) return; // dont assert more than once
      data_received = true;
      REQUIRE(p->t.price > 0);
      REQUIRE(p->t.volume > 0.);
      WARN(us.count() < (20000000 + p->t.time));
      REQUIRE(p->t.ask == 0);
      REQUIRE(p->t.bid == 0);

      flag.notify_one();
    });

    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk);
    unsubscribe();
    EC::ExchangeEventBus::getInstance()->shutdown();
    REQUIRE(connect_count == 1);
    REQUIRE(disconnect_count == 1);
  }

  TEST_CASE("test binance btcusd subscribe unsubscribe") {
    std::mutex m;
    bool flag_me = true;
    bool data_received = false;
    std::condition_variable flag;

    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("KRAKEN", "XBT/USD", MD::Channel::TICKER, [&flag, &m, &flag_me, &data_received](MD::EventPtr) {
        std::unique_lock<std::mutex> lk(m);
        if (flag_me) {
            data_received = true;
            flag.notify_all();
        }
    });
    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk, [&data_received]{return data_received == true;});

    data_received = false;
    flag_me = false;
    lk.unlock();

    unsubscribe();
    sleep(1); // give it time to rest

    lk.lock();
    flag_me = true;
    REQUIRE(flag.wait_for(lk, 2s, [&data_received]{ return data_received == true; }) == false);

    EC::ExchangeEventBus::getInstance()->shutdown();
  }
}
