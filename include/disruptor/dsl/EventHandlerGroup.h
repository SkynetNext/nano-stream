#pragma once
// 1:1-ish port of com.lmax.disruptor.dsl.EventHandlerGroup
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/dsl/EventHandlerGroup.java
//
// Note: Java method name `and` is a keyword alternative token in C++ (`and` ==
// `&&`), so we expose it as `and_` while keeping behavior 1:1.

#include "../EventHandler.h"
#include "../EventProcessor.h"
#include "../RewindableEventHandler.h"
#include "../Sequence.h"
#include "ConsumerRepository.h"
#include "EventProcessorFactory.h"
#include "ProducerType.h"


#include <algorithm>
#include <vector>

namespace disruptor::dsl {

template <typename, ProducerType, typename> class Disruptor;

template <typename T, ProducerType Producer, typename WaitStrategyT>
class EventHandlerGroup final {
public:
  using DisruptorT = Disruptor<T, Producer, WaitStrategyT>;
  using BarrierPtr = typename DisruptorT::BarrierPtr;

  EventHandlerGroup(DisruptorT &disruptor,
                    ConsumerRepository<BarrierPtr> &consumerRepository,
                    Sequence *const *sequences, int count)
      : disruptor_(&disruptor), consumerRepository_(&consumerRepository),
        sequences_(sequences, sequences + count) {}

  EventHandlerGroup<T, Producer, WaitStrategyT>
  and_(EventHandlerGroup<T, Producer, WaitStrategyT> &otherHandlerGroup) {
    std::vector<Sequence *> combined;
    combined.reserve(sequences_.size() + otherHandlerGroup.sequences_.size());
    combined.insert(combined.end(), sequences_.begin(), sequences_.end());
    combined.insert(combined.end(), otherHandlerGroup.sequences_.begin(),
                    otherHandlerGroup.sequences_.end());
    return EventHandlerGroup<T, Producer, WaitStrategyT>(
        *disruptor_, *consumerRepository_, combined.data(),
        static_cast<int>(combined.size()));
  }

  EventHandlerGroup<T, Producer, WaitStrategyT>
  and_(EventProcessor *const *processors, int count) {
    std::vector<Sequence *> combined;
    combined.reserve(sequences_.size() + static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      consumerRepository_->add(*processors[i]);
      combined.push_back(&processors[i]->getSequence());
    }
    combined.insert(combined.end(), sequences_.begin(), sequences_.end());
    return EventHandlerGroup<T, Producer, WaitStrategyT>(
        *disruptor_, *consumerRepository_, combined.data(),
        static_cast<int>(combined.size()));
  }

  template <typename... Handlers>
  EventHandlerGroup<T, Producer, WaitStrategyT> then(Handlers &...handlers) {
    return handleEventsWith(handlers...);
  }

  template <typename... Handlers>
  EventHandlerGroup<T, Producer, WaitStrategyT>
  handleEventsWith(Handlers &...handlers) {
    return disruptor_->createEventProcessors(
        sequences_.data(), static_cast<int>(sequences_.size()), handlers...);
  }

  template <typename... Factories>
  EventHandlerGroup<T, Producer, WaitStrategyT>
  thenFactories(Factories &...factories) {
    return handleEventsWithFactories(factories...);
  }

  template <typename... Factories>
  EventHandlerGroup<T, Producer, WaitStrategyT>
  handleEventsWithFactories(Factories &...factories) {
    return disruptor_->createEventProcessors(
        sequences_.data(), static_cast<int>(sequences_.size()), factories...);
  }

  BarrierPtr asSequenceBarrier() {
    auto barrier = disruptor_->getRingBuffer().newBarrier(
        sequences_.data(), static_cast<int>(sequences_.size()));
    disruptor_->keepBarrierAlive_(barrier);
    return barrier;
  }

private:
  DisruptorT *disruptor_;
  ConsumerRepository<BarrierPtr> *consumerRepository_;
  std::vector<Sequence *> sequences_;
};

} // namespace disruptor::dsl
