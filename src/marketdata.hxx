#ifndef MARKETDATA_H_
#define MARKETDATA_H_

#include <string>
#include <list>
#include <vector>
#include <mutex>
#include <tuple>

#include <fmt/format.h>

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

  enum L2UpdateType {
    SNAPSHOT,
    CHANGES
  };

  struct Trade {
    Side side;
    uint64_t time;
    double price;
    double volume;
    double bid;
    double ask;
  };

  struct Level2 {
    L2UpdateType type;
    uint64_t time;
    //std::vector<std::pair<double, double>> bids;
    //std::vector<std::pair<double, double>> asks;
  };

  struct Event {
    enum EventType {
      TRADE,
      L2UPDATE,
      CONNECT,
      DISCONNECT
    } eventType;
    virtual ~Event() {}
  };

  struct TradeEvent : public Event {
    Trade t;

    TradeEvent() { eventType = TRADE; }
  };

  struct Level2Event : public Event {
      Level2 l2;

      Level2Event() { eventType = L2UPDATE; }
  };

  struct ConnectEvent : public Event {
      ConnectEvent() { eventType = CONNECT; }
  };

  struct DisconnectEvent : public Event {
      DisconnectEvent() { eventType = DISCONNECT; }
  };

  typedef std::shared_ptr<Event> EventPtr;
  typedef std::shared_ptr<TradeEvent> TradeEventPtr;
  typedef std::shared_ptr<Level2Event> Level2EventPtr;

}

template <> struct fmt::formatter<MD::Trade>: formatter<string_view> {
  // parse is inherited from formatter<string_view>.
  template <typename FormatContext>
  auto format(const MD::Trade& t, FormatContext& ctx) {
    string_view data = "";
    data = fmt::format("[Trade time={} side={} price={} volume={} bid={} ask={}]", t.time, t.side, t.price, t.volume, t.bid, t.ask);

    return formatter<string_view>::format(data, ctx);
  }
};

#endif // MARKETDATA_H_
