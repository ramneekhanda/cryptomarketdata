#include <iostream>
#include <unistd.h>
#include "exchange_connect.hxx"
#include "exchanges/coinbase.hxx"

int main(int, char**)
{
    EC::ExchangeConnectorPtr exCon(new EC::ExchangeConnector());
    EC::ExchangeEventBusPtr exEventBus(new EC::ExchangeEventBus());
    EC::CoinBase c(exCon, exEventBus);
    c.connect();
    c.subscribe("BTC-USD", MD::Channel::TICKER);
    c.subscribe("ETH-BTC", MD::Channel::TICKER);

    exEventBus->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [](MD::EventPtr p) {
        std::cout << "BTC-USD ticked at " << p->t.price << "@" << p->t.volume << " at " << p->t.time << std::endl;
    });

    exEventBus->subscribe("COINBASE", "ETH-BTC", MD::Channel::TICKER, [](MD::EventPtr p) {
        std::cout << "ETH-BTC ticked at " << p->t.price << "@" << p->t.volume << " at " << p->t.time << std::endl;
    });

    //EC::CoinBase c(exCon);
    while (true) {
        sleep(1000);
    }

    return 0;
}
