#pragma once
// 1:1 port of com.lmax.disruptor.dsl.Disruptor (header-only)
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/dsl/Disruptor.java
//
// This DSL is intentionally close to Java structure. Some Java-specific
// overload surface (varargs, generics wildcards, Thread lifecycle) is adapted
// to C++ idioms while keeping semantics.

#include "../BatchEventProcessor.h"
#include "../EventFactory.h"
#include "../EventHandlerIdentity.h"
#include "../EventProcessor.h"
#include "../EventTranslator.h"
#include "../EventTranslatorOneArg.h"
#include "../ExceptionHandler.h"
#include "../RingBuffer.h"
#include "../Sequence.h"
#include "../TimeoutException.h"
#include "../WaitStrategy.h"
#include "../util/Util.h"

#include "ConsumerRepository.h"
#include "EventHandlerGroup.h"
#include "EventProcessorFactory.h"
#include "ExceptionHandlerSetting.h"
#include "ExceptionHandlerWrapper.h"
#include "ProducerType.h"
#include "ThreadFactory.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace disruptor::dsl {

template <typename T, ProducerType Producer, typename WaitStrategyT>
class Disruptor {
public:
  ~Disruptor() {
    // In Java, processors run on daemon threads so tests can "leak" threads
    // without crashing the JVM. In C++, destroying a joinable std::thread
    // triggers std::terminate, so we must always join.
    try {
      halt();
      // After halt(), we still need to join threads to avoid std::terminate
      consumerRepository_.joinAll();
    } catch (...) {
      // Best-effort teardown; never throw from destructor.
    }
  }
  using SequencerT =
      std::conditional_t<Producer == ProducerType::SINGLE,
                         ::disruptor::SingleProducerSequencer<WaitStrategyT>,
                         ::disruptor::MultiProducerSequencer<WaitStrategyT>>;
  using RingBufferT = ::disruptor::RingBuffer<T, SequencerT>;
  using BarrierPtr = decltype(std::declval<RingBufferT &>().newBarrier(
      static_cast<Sequence *const *>(nullptr), 0));
  using BarrierT = typename BarrierPtr::element_type;

  Disruptor(std::shared_ptr<EventFactory<T>> eventFactory, int ringBufferSize,
            ThreadFactory &threadFactory)
      : ownedWaitStrategy_(std::in_place),
        ringBuffer_(makeRingBuffer_(std::move(eventFactory), ringBufferSize,
                                    *ownedWaitStrategy_)),
        threadFactory_(threadFactory), consumerRepository_(), started_(false),
        exceptionHandler_(std::make_unique<ExceptionHandlerWrapper<T>>()) {}

  Disruptor(std::shared_ptr<EventFactory<T>> eventFactory, int ringBufferSize,
            ThreadFactory &threadFactory, WaitStrategyT &waitStrategy)
      : ownedWaitStrategy_(std::nullopt),
        ringBuffer_(makeRingBuffer_(std::move(eventFactory), ringBufferSize,
                                    waitStrategy)),
        threadFactory_(threadFactory), consumerRepository_(), started_(false),
        exceptionHandler_(std::make_unique<ExceptionHandlerWrapper<T>>()) {}

private:
  static std::shared_ptr<RingBufferT>
  makeRingBuffer_(std::shared_ptr<EventFactory<T>> factory, int ringBufferSize,
                  WaitStrategyT &waitStrategy) {
    // Construct SequencerT in-place in RingBuffer to avoid move/copy
    return std::shared_ptr<RingBufferT>(new RingBufferT(
        std::move(factory), std::in_place, ringBufferSize, waitStrategy));
  }

public:
  // Set up event handlers (start of chain)
  template <typename... Handlers>
  EventHandlerGroup<T, Producer, WaitStrategyT>
  handleEventsWith(Handlers &...handlers) {
    Sequence *none[0]{};
    return createEventProcessors(none, 0, handlers...);
  }

  // Set up custom processors (start of chain)
  EventHandlerGroup<T, Producer, WaitStrategyT>
  handleEventsWith(EventProcessor *const *processors, int count) {
    for (int i = 0; i < count; ++i) {
      consumerRepository_.add(*processors[i]);
    }
    auto sequences = util::Util::getSequencesFor(processors, count);
    ringBuffer_->addGatingSequences(sequences.data(),
                                    static_cast<int>(sequences.size()));
    return EventHandlerGroup<T, Producer, WaitStrategyT>(
        *this, consumerRepository_, sequences.data(),
        static_cast<int>(sequences.size()));
  }

  // Exception handling
  void handleExceptionsWith(ExceptionHandler<T> &exceptionHandler) {
    // Release ownership of the default wrapper, switch to external handler
    exceptionHandler_.reset();
    // Store raw pointer for external handler (caller owns the lifetime)
    exceptionHandlerPtr_ = &exceptionHandler;
  }

  void setDefaultExceptionHandler(ExceptionHandler<T> &exceptionHandler) {
    checkNotStarted();
    ExceptionHandlerWrapper<T> *wrapper = nullptr;
    if (exceptionHandler_) {
      wrapper =
          dynamic_cast<ExceptionHandlerWrapper<T> *>(exceptionHandler_.get());
    }
    if (wrapper == nullptr) {
      throw std::runtime_error("setDefaultExceptionHandler can not be used "
                               "after handleExceptionsWith");
    }
    wrapper->switchTo(exceptionHandler);
  }

  ExceptionHandlerSetting<T, BarrierPtr, BarrierT>
  handleExceptionsFor(EventHandlerIdentity &eventHandler) {
    return ExceptionHandlerSetting<T, BarrierPtr, BarrierT>(
        eventHandler, consumerRepository_);
  }

  // Dependency setup
  EventHandlerGroup<T, Producer, WaitStrategyT>
  after(EventHandlerIdentity *const *handlers, int count) {
    std::vector<Sequence *> sequences;
    sequences.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      sequences.push_back(&consumerRepository_.getSequenceFor(*handlers[i]));
    }
    return EventHandlerGroup<T, Producer, WaitStrategyT>(
        *this, consumerRepository_, sequences.data(),
        static_cast<int>(sequences.size()));
  }

  // After EventProcessors (Java: after(EventProcessor... processors))
  EventHandlerGroup<T, Producer, WaitStrategyT>
  after(EventProcessor *const *processors, int count) {
    std::vector<Sequence *> sequences =
        util::Util::getSequencesFor(processors, count);
    return EventHandlerGroup<T, Producer, WaitStrategyT>(
        *this, consumerRepository_, sequences.data(),
        static_cast<int>(sequences.size()));
  }

  // Publishing helpers (subset used by codebase)
  void publishEvent(EventTranslator<T> &translator) {
    ringBuffer_->publishEvent(translator);
  }

  template <typename A>
  void publishEvent(EventTranslatorOneArg<T, A> &translator, A arg0) {
    ringBuffer_->publishEvent(translator, arg0);
  }

  // Start/stop lifecycle
  std::shared_ptr<RingBufferT> start(std::latch *startupLatch = nullptr) {
    checkOnlyStartedOnce();
    consumerRepository_.startAll(threadFactory_, startupLatch);
    return ringBuffer_;
  }

  void halt() { consumerRepository_.haltAll(); }
  void join() { consumerRepository_.joinAll(); }

  void shutdown() {
    try {
      shutdown(-1);
    } catch (const TimeoutException &e) {
      // Java: shutdown() swallows TimeoutException and delegates to exception
      // handler.
      getExceptionHandler().handleOnShutdownException(e);
    }
  }

  void shutdown(int64_t timeoutMillis) {
    // Java waits for backlog to drain, then halts. We keep same logic.
    const int64_t deadline =
        timeoutMillis < 0 ? -1
                          : (util::Util::currentTimeMillis() + timeoutMillis);
    while (hasBacklog()) {
      if (deadline >= 0 && util::Util::currentTimeMillis() >= deadline) {
        throw TimeoutException::INSTANCE();
      }
      // Yield while waiting.
      util::ThreadHints::onSpinWait();
    }
    halt();
  }

  bool hasStarted() const { return started_.load(std::memory_order_acquire); }

  RingBufferT &getRingBuffer() { return *ringBuffer_; }
  int64_t getCursor() const { return ringBuffer_->getCursor(); }
  int getBufferSize() const { return ringBuffer_->getBufferSize(); }
  T &get(int64_t sequence) { return ringBuffer_->get(sequence); }

  BarrierPtr getBarrierFor(EventHandlerIdentity &handlerIdentity) {
    return consumerRepository_.getBarrierFor(handlerIdentity);
  }
  int64_t getSequenceValueFor(EventHandlerIdentity &handlerIdentity) {
    return consumerRepository_.getSequenceFor(handlerIdentity).get();
  }

  bool hasBacklog() {
    return consumerRepository_.hasBacklog(ringBuffer_->getCursor(), false);
  }

  int getProcessorCount() const {
    return consumerRepository_.getProcessorCount();
  }

  // Core builder used by EventHandlerGroup
  template <typename... Handlers>
  EventHandlerGroup<T, Producer, WaitStrategyT>
  createEventProcessors(Sequence *const *barrierSequences, int barrierCount,
                        Handlers &...handlers) {
    checkNotStarted();

    // Mark previous end-of-chain processors as used-in-barrier.
    consumerRepository_.unMarkEventProcessorsAsEndOfChain(barrierSequences,
                                                          barrierCount);

    std::vector<Sequence *> processorSequences;
    createEventProcessorsImpl(barrierSequences, barrierCount,
                              processorSequences, handlers...);

    // New processors gate the ring buffer.
    ringBuffer_->addGatingSequences(
        processorSequences.data(), static_cast<int>(processorSequences.size()));

    // Remove old gating sequences for next-in-chain.
    updateGatingSequencesForNextInChain(barrierSequences, barrierCount,
                                        processorSequences);

    return EventHandlerGroup<T, Producer, WaitStrategyT>(
        *this, consumerRepository_, processorSequences.data(),
        static_cast<int>(processorSequences.size()));
  }

private:
  friend class EventHandlerGroup<T, Producer, WaitStrategyT>;

  void keepBarrierAlive_(const BarrierPtr &barrier) {
    ownedBarriers_.push_back(barrier);
  }

  // Owning wait strategy storage so non-movable strategies (mutex/cv) work.
  // Only used when no external WaitStrategy is provided (first constructor).
  // Must be declared before ringBuffer_ so it's initialized first (ringBuffer_
  // needs a reference to it).
  std::optional<WaitStrategyT> ownedWaitStrategy_;
  std::shared_ptr<RingBufferT> ringBuffer_;
  ThreadFactory &threadFactory_;
  // Own BatchEventProcessors created by DSL so their lifetime spans the
  // disruptor. Must be declared before consumerRepository_ so processors are
  // destroyed after EventProcessorInfo (which holds raw pointers to them).
  std::vector<std::shared_ptr<EventProcessor>> ownedProcessors_;
  ConsumerRepository<BarrierPtr> consumerRepository_;
  std::atomic<bool> started_;
  std::unique_ptr<ExceptionHandler<T>> exceptionHandler_;
  ExceptionHandler<T> *exceptionHandlerPtr_{
      nullptr}; // Points to external handler when handleExceptionsWith is used
  // Hold SequenceBarriers created by the DSL to ensure they outlive processors
  // that reference them.
  std::vector<BarrierPtr> ownedBarriers_;

  // Helper to get the current exception handler (either owned or external)
  ExceptionHandler<T> &getExceptionHandler() {
    if (exceptionHandlerPtr_ != nullptr) {
      return *exceptionHandlerPtr_;
    }
    return *exceptionHandler_;
  }

  void checkNotStarted() {
    if (started_.load(std::memory_order_acquire)) {
      throw std::runtime_error(
          "All event handlers must be added before calling start.");
    }
  }

  void checkOnlyStartedOnce() {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true,
                                          std::memory_order_acq_rel)) {
      throw std::runtime_error("Disruptor.start() must only be called once.");
    }
  }

  // Helper: add processors for handlers/factories; this is a reduced surface
  // sufficient for current port.
  void createEventProcessorsImpl(Sequence *const *barrierSequences,
                                 int barrierCount,
                                 std::vector<Sequence *> &outSequences) {
    (void)barrierSequences;
    (void)barrierCount;
    (void)outSequences;
  }

  template <typename Handler, typename... Rest>
  void createEventProcessorsImpl(Sequence *const *barrierSequences,
                                 int barrierCount,
                                 std::vector<Sequence *> &outSequences,
                                 Handler &handler, Rest &...rest) {
    createOne(barrierSequences, barrierCount, outSequences, handler);
    createEventProcessorsImpl(barrierSequences, barrierCount, outSequences,
                              rest...);
  }

  // Create for EventHandler<T> and RewindableEventHandler<T>
  void createOne(Sequence *const *barrierSequences, int barrierCount,
                 std::vector<Sequence *> &outSequences,
                 ::disruptor::EventHandlerBase<T> &handler) {
    auto barrier = ringBuffer_->newBarrier(barrierSequences, barrierCount);
    ownedBarriers_.push_back(barrier);
    // Java uses BatchEventProcessorBuilder to configure max batch size; we use
    // default max here. (If/when DSL exposes builder configuration, wire it
    // through.)
    auto processor = std::make_shared<BatchEventProcessor<T, BarrierT>>(
        *ringBuffer_, *barrier, handler, std::numeric_limits<int>::max(),
        nullptr);
    // Apply default exception handler if it is wrapper or concrete.
    processor->setExceptionHandler(getExceptionHandler());
    auto &seq = processor->getSequence();
    consumerRepository_.add(*processor, handler, barrier);
    ownedProcessors_.push_back(processor);
    outSequences.push_back(&seq);
  }

  // Factories
  void
  createOne(Sequence *const *barrierSequences, int barrierCount,
            std::vector<Sequence *> &outSequences,
            ::disruptor::dsl::EventProcessorFactory<T, RingBufferT> &factory) {
    // Create processor via factory and wire it into repository.
    auto processor = factory.createEventProcessor(
        *ringBuffer_, barrierSequences, barrierCount);
    ownedProcessors_.push_back(processor);
    consumerRepository_.add(*processor);
    outSequences.push_back(&processor->getSequence());
  }

  void updateGatingSequencesForNextInChain(
      Sequence *const *barrierSequences, int barrierCount,
      const std::vector<Sequence *> &processorSequences) {
    // Java logic: ringBuffer.addGatingSequences(processorSequences); then
    // remove barrierSequences gating and add processorSequences gating. Our
    // ring buffer already gates on new sequences; remove old barrier gating
    // sequences.
    for (int i = 0; i < barrierCount; ++i) {
      ringBuffer_->removeGatingSequence(*barrierSequences[i]);
    }
    (void)processorSequences;
  }
};

} // namespace disruptor::dsl
