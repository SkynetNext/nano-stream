#pragma once
// 1:1-ish port of com.lmax.disruptor.dsl.EventHandlerGroup
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/EventHandlerGroup.java
//
// Note: Java method name `and` is a keyword alternative token in C++ (`and` == `&&`),
// so we expose it as `and_` while keeping behavior 1:1.

#include "../EventHandler.h"
#include "../EventProcessor.h"
#include "../RewindableEventHandler.h"
#include "../Sequence.h"
#include "../SequenceBarrier.h"
#include "ConsumerRepository.h"
#include "EventProcessorFactory.h"

#include <algorithm>
#include <vector>

namespace disruptor::dsl {

template <typename T>
class Disruptor;

template <typename T>
class EventHandlerGroup final {
public:
  EventHandlerGroup(Disruptor<T>& disruptor, ConsumerRepository& consumerRepository, Sequence* const* sequences, int count)
      : disruptor_(&disruptor), consumerRepository_(&consumerRepository), sequences_(sequences, sequences + count) {}

  EventHandlerGroup<T> and_(EventHandlerGroup<T>& otherHandlerGroup) {
    std::vector<Sequence*> combined;
    combined.reserve(sequences_.size() + otherHandlerGroup.sequences_.size());
    combined.insert(combined.end(), sequences_.begin(), sequences_.end());
    combined.insert(combined.end(), otherHandlerGroup.sequences_.begin(), otherHandlerGroup.sequences_.end());
    return EventHandlerGroup<T>(*disruptor_, *consumerRepository_, combined.data(), static_cast<int>(combined.size()));
  }

  EventHandlerGroup<T> and_(EventProcessor* const* processors, int count) {
    std::vector<Sequence*> combined;
    combined.reserve(sequences_.size() + static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      consumerRepository_->add(*processors[i]);
      combined.push_back(&processors[i]->getSequence());
    }
    combined.insert(combined.end(), sequences_.begin(), sequences_.end());
    return EventHandlerGroup<T>(*disruptor_, *consumerRepository_, combined.data(), static_cast<int>(combined.size()));
  }

  template <typename... Handlers>
  EventHandlerGroup<T> then(Handlers&... handlers) {
    return handleEventsWith(handlers...);
  }

  template <typename... Handlers>
  EventHandlerGroup<T> handleEventsWith(Handlers&... handlers) {
    return disruptor_->createEventProcessors(sequences_.data(), static_cast<int>(sequences_.size()), handlers...);
  }

  template <typename... Factories>
  EventHandlerGroup<T> thenFactories(Factories&... factories) {
    return handleEventsWithFactories(factories...);
  }

  template <typename... Factories>
  EventHandlerGroup<T> handleEventsWithFactories(Factories&... factories) {
    return disruptor_->createEventProcessors(sequences_.data(), static_cast<int>(sequences_.size()), factories...);
  }

  // NOTE: Returning a reference here is dangerous unless the Disruptor owns the barrier.
  // Disruptor stores barriers created by DSL methods, so this reference remains valid.
  SequenceBarrier& asSequenceBarrier() {
    auto barrier = disruptor_->getRingBuffer().newBarrier(sequences_.data(), static_cast<int>(sequences_.size()));
    // Reuse Disruptor's ownership mechanism by publishing through handleEventsWithFactories path is overkill;
    // instead, rely on Disruptor holding onto barriers created for processors and explicitly keep this one too.
    // (Disruptor exposes ownership via internal vector; this is safe because EventHandlerGroup is tightly coupled.)
    disruptor_->keepBarrierAlive_(barrier);
    return *barrier;
  }

private:
  Disruptor<T>* disruptor_;
  ConsumerRepository* consumerRepository_;
  std::vector<Sequence*> sequences_;
};

} // namespace disruptor::dsl


