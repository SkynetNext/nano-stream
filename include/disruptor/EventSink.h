#pragma once
// 1:1 port of com.lmax.disruptor.EventSink
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventSink.java

#include <cstdint>
#include <vector>

namespace disruptor {

template <typename E> class EventTranslator;
template <typename E> class EventTranslatorVararg;

template <typename E>
class EventSink {
public:
  virtual ~EventSink() = default;

  virtual void publishEvent(EventTranslator<E>& translator) = 0;
  virtual bool tryPublishEvent(EventTranslator<E>& translator) = 0;

  virtual void publishEvent(EventTranslatorVararg<E>& translator, const std::vector<void*>& args) = 0;
  virtual bool tryPublishEvent(EventTranslatorVararg<E>& translator, const std::vector<void*>& args) = 0;

  virtual void publishEvents(std::vector<EventTranslator<E>*>& translators) = 0;
  virtual void publishEvents(std::vector<EventTranslator<E>*>& translators, int batchStartsAt, int batchSize) = 0;
  virtual bool tryPublishEvents(std::vector<EventTranslator<E>*>& translators) = 0;
  virtual bool tryPublishEvents(std::vector<EventTranslator<E>*>& translators, int batchStartsAt, int batchSize) = 0;

protected:
  // C++ does not allow virtual member function templates. Java models these as overloads on generic interfaces;
  // in C++ we implement the overloads directly in RingBuffer, and keep EventSink as the common non-templated surface.
};

} // namespace disruptor
