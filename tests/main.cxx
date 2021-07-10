#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <chrono>
#include <condition_variable>

#include <doctest/doctest.h>

#include "../src/exchange_connect.hxx"
#include "../src/exchange_eventbus.hxx"
#include "../src/exchanges/coinbase.hxx"

TEST_CASE("test coinbase btcusd feed") {
    std::mutex m;
    std::condition_variable flag;

    EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [&flag, &m](MD::EventPtr p) {
        using namespace std::chrono;
        microseconds us = duration_cast<microseconds>(system_clock::now().time_since_epoch());

        REQUIRE(p->t.price > 0);
        REQUIRE(p->t.volume > 0.);
        WARN(us.count() < (2000000 + p->t.time));
        REQUIRE(p->t.ask > p->t.bid);
        flag.notify_all();
    });

    EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [&flag, &m](MD::EventPtr p) {
        using namespace std::chrono;
        microseconds us = duration_cast<microseconds>(system_clock::now().time_since_epoch());

        REQUIRE(p->t.price > 0);
        REQUIRE(p->t.volume > 0.);
        WARN(us.count() < (2000000 + p->t.time));
        REQUIRE(p->t.ask > p->t.bid);
        flag.notify_all();
    });

    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk);
    flag.wait(lk);

    EC::ExchangeConnector::getInstance()->shutdown();
}
