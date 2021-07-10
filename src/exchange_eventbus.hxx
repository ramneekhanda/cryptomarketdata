#ifndef EXCHANGE_EVENTBUS_H_
#define EXCHANGE_EVENTBUS_H_

#include <memory>
#include <unordered_map>

#include <rxcpp/rx.hpp>

#include "marketdata.hxx"
#include "exchange_connect.hxx"

namespace EC {
  using namespace std;

  class ExchangeConnector;
  typedef shared_ptr<ExchangeConnector> ExchangeConnectorPtr;

  class ExchangeEventBus {
    typedef rxcpp::subjects::subject<MD::EventPtr> EventSubject;
    typedef std::shared_ptr<EventSubject> EventSubjectPtr;
    typedef std::unordered_map<std::string, EventSubjectPtr> EventBus;

    ExchangeConnectorPtr exCon;
    EventBus eventBus;
    static ExchangeEventBusPtr self;

    void ensureTopicExists(const std::string &topic) {
      EventBus::iterator itr = eventBus.find(topic);
      if (itr == eventBus.end()) {
        eventBus.insert({topic, EventSubjectPtr(new EventSubject())});
      }
    }
  public:
    ExchangeEventBus() {
      this->exCon = ExchangeConnector::getInstance();
    }

    void publish(const std::string &exchange, const std::string &symbol, MD::Channel chan, MD::EventPtr event) {
      std::string topic = exchange + "_" + symbol + "_" + MD::ChannelName[chan];

      ensureTopicExists(topic);
      eventBus[topic]->get_subscriber().on_next(event);
    }

    template <typename T>
    void subscribe(const std::string &exchange, const std::string& symbol, MD::Channel chan, T subscriber);

    static ExchangeEventBusPtr getInstance() {
      if (!self) {
        self = ExchangeEventBusPtr(new ExchangeEventBus());
      }
      return self;
    }
  };
  ExchangeEventBusPtr ExchangeEventBus::self = nullptr;
}

template <typename T>
void EC::ExchangeEventBus::subscribe(const std::string &exchange, const std::string& symbol, MD::Channel chan, T subscriber) {
  std::string topic = exchange + "_" + symbol + "_" + MD::ChannelName[chan];

  // FIXME silent return should be avoided
  if (!exCon->ensureConnected(exchange)) {
    return;
  }
  exCon->subscribe(exchange, symbol, chan);
  ensureTopicExists(topic);
  auto observable = eventBus[topic]->get_observable();
  observable.subscribe(subscriber);
}

#endif // EXCHANGE_EVENTBUS_H_
