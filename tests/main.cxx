#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <chrono>
#include <condition_variable>

#include <doctest/doctest.h>

#include "../src/exchange_connect.hxx"
#include "../src/exchange_eventbus.hxx"
#include "../src/exchanges/coinbase.hxx"
using namespace std::chrono_literals;

TEST_CASE("test coinbase btcusd feed") {

    std::mutex m;
    std::condition_variable flag;
    bool data_received = false;
    EC::ExchangeConnector::getInstance()->init();
    EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [&flag, &m, &data_received](MD::EventPtr e) {
        using namespace std::chrono;
        MD::TradeEventPtr p = std::dynamic_pointer_cast<MD::TradeEvent>(e);
        microseconds us = duration_cast<microseconds>(system_clock::now().time_since_epoch());
        std::unique_lock<std::mutex> lk(m);
        if (data_received) return; // dont assert more than once
        data_received = true;
        REQUIRE(p->t.price > 0);
        REQUIRE(p->t.volume > 0.);
        WARN(us.count() < (2000000 + p->t.time));
        REQUIRE(p->t.ask > p->t.bid);
        flag.notify_one();
    });

    std::unique_lock<std::mutex> lk(m);
    flag.wait(lk);
    EC::ExchangeConnector::getInstance()->shutdown();
}

TEST_CASE("test coinbase btcusd subscribe unsubscribe") {
    std::mutex m;
    bool flag_me = true;
    std::condition_variable flag;
    bool data_received = false;
    EC::ExchangeConnector::getInstance()->init();
    std::cout << "subscribing now" << std::endl;
    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [&flag, &m, &flag_me, &data_received](MD::EventPtr) {
        std::cout << "data received" << std::endl;
        std::unique_lock<std::mutex> lk(m);
        if (flag_me) {
            std::cout << "notifying" << std::endl;
            data_received = true;
            flag.notify_all();
        }
    });
    std::cout << "acq lock 1" << std::endl;
    std::unique_lock<std::mutex> lk(m);
    std::cout << "acqrd lock 1" << std::endl;
    flag.wait(lk, [&data_received]{return data_received == true;});
    std::cout << "first wait done" << std::endl;

    data_received = false;
    flag_me = false;
    lk.unlock();

    unsubscribe();
    sleep(1); // give it time to rest

    std::cout << "acq lock 2" << std::endl;
    lk.lock();
    std::cout << "acqrd lock 2" << std::endl;

    flag_me = true;
    REQUIRE(flag.wait_for(lk, 2s, [&data_received]{ return data_received == true; }) == false);
    std::cout << "second wait done" << std::endl;

    EC::ExchangeConnector::getInstance()->shutdown();
}
