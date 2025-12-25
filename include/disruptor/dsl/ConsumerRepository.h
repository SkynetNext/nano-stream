#pragma once
// 1:1 port of com.lmax.disruptor.dsl.ConsumerRepository (package-private class)
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/dsl/ConsumerRepository.java

#include "../EventHandlerIdentity.h"
#include "../EventProcessor.h"
#include "../Sequence.h"
#include "ConsumerInfo.h"
#include "EventProcessorInfo.h"
#include "ThreadFactory.h"

#include <latch>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace disruptor::dsl {

template <typename BarrierPtrT> class ConsumerRepository final {
public:
  void add(EventProcessor &eventprocessor,
           EventHandlerIdentity &handlerIdentity, BarrierPtrT barrier) {
    auto consumerInfo = std::make_shared<EventProcessorInfo<BarrierPtrT>>(
        eventprocessor, barrier);
    eventProcessorInfoByEventHandler_[&handlerIdentity] = consumerInfo;
    eventProcessorInfoBySequence_[&eventprocessor.getSequence()] = consumerInfo;
    consumerInfos_.push_back(std::move(consumerInfo));
  }

  void add(EventProcessor &processor) {
    auto consumerInfo = std::make_shared<EventProcessorInfo<BarrierPtrT>>(
        processor, BarrierPtrT{});
    eventProcessorInfoBySequence_[&processor.getSequence()] = consumerInfo;
    consumerInfos_.push_back(std::move(consumerInfo));
  }

  void startAll(ThreadFactory &threadFactory,
                std::latch *startupLatch = nullptr) {
    for (auto &c : consumerInfos_) {
      c->start(threadFactory, startupLatch);
    }
  }

  int getProcessorCount() const {
    return static_cast<int>(consumerInfos_.size());
  }

  void haltAll() {
    for (auto &c : consumerInfos_) {
      c->halt();
    }
  }

  void joinAll() {
    for (auto &c : consumerInfos_) {
      c->join();
    }
  }

  bool hasBacklog(int64_t cursor, bool includeStopped) {
    for (auto &consumerInfo : consumerInfos_) {
      if ((includeStopped || consumerInfo->isRunning()) &&
          consumerInfo->isEndOfChain()) {
        Sequence *const *sequences = consumerInfo->getSequences();
        const int count = consumerInfo->getSequenceCount();
        for (int i = 0; i < count; ++i) {
          if (cursor > sequences[i]->get()) {
            return true;
          }
        }
      }
    }
    return false;
  }

  EventProcessor &getEventProcessorFor(EventHandlerIdentity &handlerIdentity) {
    auto *info = getEventProcessorInfo(handlerIdentity);
    if (info == nullptr) {
      throw std::invalid_argument(
          "The event handler is not processing events.");
    }
    return info->getEventProcessor();
  }

  Sequence &getSequenceFor(EventHandlerIdentity &handlerIdentity) {
    return getEventProcessorFor(handlerIdentity).getSequence();
  }

  void
  unMarkEventProcessorsAsEndOfChain(Sequence *const *barrierEventProcessors,
                                    int count) {
    for (int i = 0; i < count; ++i) {
      auto *info = getEventProcessorInfo(*barrierEventProcessors[i]);
      if (info != nullptr) {
        info->markAsUsedInBarrier();
      }
    }
  }

  BarrierPtrT getBarrierFor(EventHandlerIdentity &handlerIdentity) {
    auto *consumerInfo = getEventProcessorInfo(handlerIdentity);
    return consumerInfo != nullptr ? consumerInfo->getBarrier() : BarrierPtrT{};
  }

private:
  using InfoPtr = std::shared_ptr<EventProcessorInfo<BarrierPtrT>>;

  EventProcessorInfo<BarrierPtrT> *
  getEventProcessorInfo(EventHandlerIdentity &handlerIdentity) {
    auto it = eventProcessorInfoByEventHandler_.find(&handlerIdentity);
    return it == eventProcessorInfoByEventHandler_.end() ? nullptr
                                                         : it->second.get();
  }

  ConsumerInfo<BarrierPtrT> *
  getEventProcessorInfo(Sequence &barrierEventProcessor) {
    auto it = eventProcessorInfoBySequence_.find(&barrierEventProcessor);
    return it == eventProcessorInfoBySequence_.end() ? nullptr
                                                     : it->second.get();
  }

  std::unordered_map<EventHandlerIdentity *, InfoPtr>
      eventProcessorInfoByEventHandler_;
  std::unordered_map<Sequence *, std::shared_ptr<ConsumerInfo<BarrierPtrT>>>
      eventProcessorInfoBySequence_;
  std::vector<std::shared_ptr<ConsumerInfo<BarrierPtrT>>> consumerInfos_;
};

} // namespace disruptor::dsl
