#include <iostream>
#include <unistd.h>
#include "exchange_eventbus.hxx"
#include "exchanges/coinbase.hxx"

int main(int, char**)
{
    EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "ETH-USD", MD::Channel::TICKER, [](MD::EventPtr e) {
        MD::TradeEventPtr p = std::dynamic_pointer_cast<MD::TradeEvent>(e);
        std::cout << "ETH-USD ticked at " << p->t.price << "@" << p->t.volume << " at " << p->t.time << std::endl;
    });

    while (true) {
        sleep(1000);
    }

    return 0;
}
