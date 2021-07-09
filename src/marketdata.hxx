#ifndef MARKETDATA_H_
#define MARKETDATA_H_
#include <string>
#include <list>
#include <vector>
#include <mutex>
#include <tuple>

namespace MD {
  enum Channel {
    TICKER,
    L2UPDATE
  };

  const std::string ChannelName[] = {
    "TICKER",
    "L2UPDATE"
  };

  enum Side {
    BUY,
    SELL
  };

  struct Trade {
    Side side;
    uint64_t time;
    double price;
    double volume;
  };

  struct L2Update {

  };

  struct Event {
    enum EventType {
      TRADE,
      L2UPDATE
    } eventType;

    union {
      Trade t;
      L2Update l2;
    };
  };
  typedef std::shared_ptr<Event> EventPtr;

  class OrderBook {
  private:
    std::mutex mtx;
    std::string symbol;

    std::vector<double> bids;
    std::vector<double> bidVolumes;

    std::vector<double> asks;
    std::vector<double> askVolumes;

    int64_t lastUpdateTime;

  public:
    OrderBook() {

    }

    void push(OrderBook &) {
      const std::lock_guard<std::mutex> lock(mtx);

    }

  };


  class TradeHistory {
  private:
    std::string symbol;
    std::vector<Side> side;
    std::vector<int64_t> time;
    std::vector<double> volume;
    std::vector<double> price;

    std::mutex mtx;
  public:
    TradeHistory() {

    }

    void push(Trade &trade) {

      side.push_back(trade.side);
      time.push_back(trade.time);
      volume.push_back(trade.volume);
      price.push_back(trade.price);
    }
  };

}

#endif // MARKETDATA_H_
