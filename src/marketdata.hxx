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
    double bid;
    double ask;
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

}

#endif // MARKETDATA_H_
