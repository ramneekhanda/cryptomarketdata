#ifndef EXCHANGE_EVENTBUS_H_
#define EXCHANGE_EVENTBUS_H_

#include <memory>
#include <unordered_map>

#include <rxcpp/rx.hpp>

#include "marketdata.hxx"
#include "exchange_connect.hxx"

namespace EC {

  class ExchangeEventBus;
  typedef std::shared_ptr<ExchangeEventBus> ExchangeEventBusPtr;

  class ExchangeEventBus {

    typedef rxcpp::subjects::subject<MD::EventPtr> EventSubject;
    typedef std::shared_ptr<EventSubject> EventSubjectPtr;
    typedef std::unordered_map<std::string, EventSubjectPtr> EventBus;

    std::mutex muEventBus;
    ExchangeConnectorPtr exCon;
    EventBus eventBus;
    static ExchangeEventBusPtr self;

    void ensureTopicExists(const std::string &topic) {
      using namespace std;
      unique_lock<mutex> lk(muEventBus);
      EventBus::iterator itr = eventBus.find(topic);
      if (itr == eventBus.end()) {
        eventBus.insert({topic, EventSubjectPtr(new EventSubject())});
      }
    }

    void unsubscribe(const std::string &exchange, const std::string& symbol, MD::Channel chan, rxcpp::composite_subscription& cs);

    void unsubscribe(const std::string &exchange, rxcpp::composite_subscription& cs);

    ExchangeEventBus() {
      this->exCon = ExchangeConnector::getInstance();
      this->exCon->init();
    }

  public:

    static ExchangeEventBusPtr getInstance() {
      if (!self) {
        self = ExchangeEventBusPtr(new ExchangeEventBus());
      }
      return self;
    }

    void shutdown() {
      using namespace std;
      exCon->shutdown();
      unique_lock<mutex> lk(muEventBus);
      eventBus.clear();
      lk.unlock();
      exCon.reset();
      self.reset();
    }

    void publish(const std::string &exchange, const std::string &symbol, MD::Channel chan, MD::EventPtr event) {
      using namespace std;

      std::string topic = exchange + "_" + symbol + "_" + MD::ChannelName[chan];
      ensureTopicExists(topic);

      unique_lock<mutex> lk(muEventBus);
      auto subs = eventBus[topic]->get_subscriber();
      lk.unlock();

      subs.on_next(event);
    }

    void publish(const std::string &exchange, MD::EventPtr event) {
      using namespace std;

      std::string topic = exchange;

      ensureTopicExists(topic);
      unique_lock<mutex> lk(muEventBus);
      auto subs = eventBus[topic]->get_subscriber();
      lk.unlock();

      subs.on_next(event);
    }

    template <typename T>
    std::function<void ()> subscribe(const std::string &exchange, const std::string& symbol, MD::Channel chan, T subscriber);

    template <typename T>
    std::function<void ()> subscribe(const std::string &exchange, T subscriber);

  };
  ExchangeEventBusPtr ExchangeEventBus::self = nullptr;
}

void noop() {}

template <typename T>
std::function<void ()> EC::ExchangeEventBus::subscribe(const std::string &exchange, const std::string& symbol, MD::Channel chan, T subscriber)  {
  using namespace std;

  std::string topic = exchange + "_" + symbol + "_" + MD::ChannelName[chan];

  // FIXME silent return should be avoided
  if (!exCon->ensureConnected(exchange)) {
    return &noop;
  }
  exCon->subscribe(exchange, symbol, chan);
  ensureTopicExists(topic);

  unique_lock<mutex> lk(muEventBus);
  auto observable = eventBus[topic]->get_observable();
  auto subscription = observable.subscribe(subscriber);
  return std::bind(static_cast<void(EC::ExchangeEventBus::*)(const std::string&, const std::string&, MD::Channel, rxcpp::composite_subscription&)>(&EC::ExchangeEventBus::unsubscribe), this, exchange, symbol, chan, subscription);
}

template <typename T>
std::function<void ()> EC::ExchangeEventBus::subscribe(const std::string &exchange, T subscriber)  {
  using namespace std;

  std::string topic = exchange;

  // FIXME silent return should be avoided
  if (!exCon->ensureConnected(exchange)) {
    return &noop;
  }
  ensureTopicExists(topic);

  unique_lock<mutex> lk(muEventBus);
  auto observable = eventBus[topic]->get_observable();
  auto subscription = observable.subscribe(subscriber);
  return std::bind(static_cast<void(EC::ExchangeEventBus::*)(const std::string&, rxcpp::composite_subscription&)>(&EC::ExchangeEventBus::unsubscribe), this, exchange, subscription);
}

void EC::ExchangeEventBus::unsubscribe(const std::string &exchange, rxcpp::composite_subscription& cs) {
  using namespace std;
  std::string topic = exchange;

  unique_lock<mutex> lk(muEventBus);
  EventSubjectPtr sp = eventBus[topic];
  cs.unsubscribe();
}

void EC::ExchangeEventBus::unsubscribe(const std::string &exchange, const std::string& symbol, MD::Channel chan, rxcpp::composite_subscription& cs) {
  using namespace std;

  std::string topic = exchange + "_" + symbol + "_" + MD::ChannelName[chan];

  unique_lock<mutex> lk(muEventBus);
  EventSubjectPtr sp = eventBus[topic];
  cs.unsubscribe();
  if (!sp->has_observers()) {
    auto search = eventBus.find(topic);
    eventBus.erase(search);
    exCon->unsubscribe(exchange, symbol, chan);
  }
}

#endif // EXCHANGE_EVENTBUS_H_
