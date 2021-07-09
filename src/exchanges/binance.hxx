#ifndef BINANCE_H_
#define BINANCE_H_

#include "../exchange_connect.hxx"
#include "../marketdata.hxx"

namespace EC {

class Binance {
  const std::string binance_endpoint = "wss://stream.binance.com:9443/ws/ethbtc@aggTrade/btcusdt@aggTrade";
  std::shared_ptr<ExchangeConnector> exCon;
public:

  Binance(std::shared_ptr<ExchangeConnector> exCon) {
    this->exCon = exCon;
    createConnection();
  }

  void createConnection() {
    ErrorCode ec;

    ASIOClient::connection_ptr con = exCon->getClient().get_connection(binance_endpoint, ec);

    if (ec) {
      std::cout << "> Connect initialization error: " << ec.message() << std::endl;
      return;
    }

    con->set_open_handler([con](std::weak_ptr<void>) {
      std::cout << "Connection is now open" << std::endl;
    });

    con->set_fail_handler([](std::weak_ptr<void>) {
      std::cout << "Connection error" << std::endl;
    });

    exCon->getClient().connect(con);
  }
};

}

#endif // BINANCE_H_
