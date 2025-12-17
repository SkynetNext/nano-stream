#pragma once
// 1:1 port of com.lmax.disruptor.RingBuffer
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/RingBuffer.java

#include "BlockingWaitStrategy.h"
#include "Cursored.h"
#include "EventFactory.h"
#include "EventSequencer.h"
#include "EventPoller.h"
#include "EventTranslator.h"
#include "EventTranslatorOneArg.h"
#include "EventTranslatorTwoArg.h"
#include "EventTranslatorThreeArg.h"
#include "EventTranslatorVararg.h"
#include "EventSink.h"
#include "MultiProducerSequencer.h"
#include "Sequence.h"
#include "Sequencer.h"
#include "SingleProducerSequencer.h"
#include "WaitStrategy.h"

#include "dsl/ProducerType.h"

#include "AbstractSequencer.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace disruptor {

template <typename E>
class RingBuffer final : public Cursored, public EventSequencer<E>, public EventSink<E> {
public:
  static constexpr int64_t INITIAL_CURSOR_VALUE = Sequence::INITIAL_VALUE;

  // Factory methods
  static std::shared_ptr<RingBuffer<E>> createMultiProducer(std::shared_ptr<EventFactory<E>> factory,
                                                            int bufferSize,
                                                            std::unique_ptr<WaitStrategy> waitStrategy) {
    auto sequencer = std::make_unique<MultiProducerSequencer>(bufferSize, std::move(waitStrategy));
    return std::shared_ptr<RingBuffer<E>>(new RingBuffer<E>(std::move(factory), std::move(sequencer)));
  }

  static std::shared_ptr<RingBuffer<E>> createMultiProducer(std::shared_ptr<EventFactory<E>> factory, int bufferSize) {
    return createMultiProducer(std::move(factory), bufferSize, std::make_unique<BlockingWaitStrategy>());
  }

  static std::shared_ptr<RingBuffer<E>> createSingleProducer(std::shared_ptr<EventFactory<E>> factory,
                                                             int bufferSize,
                                                             std::unique_ptr<WaitStrategy> waitStrategy) {
    auto sequencer = std::make_unique<SingleProducerSequencer>(bufferSize, std::move(waitStrategy));
    return std::shared_ptr<RingBuffer<E>>(new RingBuffer<E>(std::move(factory), std::move(sequencer)));
  }

  static std::shared_ptr<RingBuffer<E>> createSingleProducer(std::shared_ptr<EventFactory<E>> factory, int bufferSize) {
    return createSingleProducer(std::move(factory), bufferSize, std::make_unique<BlockingWaitStrategy>());
  }

  static std::shared_ptr<RingBuffer<E>> create(disruptor::dsl::ProducerType producerType,
                                               std::shared_ptr<EventFactory<E>> factory,
                                               int bufferSize,
                                               std::unique_ptr<WaitStrategy> waitStrategy) {
    switch (producerType) {
      case disruptor::dsl::ProducerType::SINGLE:
        return createSingleProducer(std::move(factory), bufferSize, std::move(waitStrategy));
      case disruptor::dsl::ProducerType::MULTI:
        return createMultiProducer(std::move(factory), bufferSize, std::move(waitStrategy));
      default:
        throw std::runtime_error("IllegalStateException");
    }
  }

  // DataProvider
  E& get(int64_t sequence) override { return elementAt(sequence); }

  // Cursored
  int64_t getCursor() const override { return sequencer_->getCursor(); }

  // RingBuffer-specific helpers from Java
  void addGatingSequences(Sequence* const* gatingSequences, int count) {
    sequencer_->addGatingSequences(gatingSequences, count);
  }

  void addGatingSequences(Sequence& gatingSequence) {
    Sequence* arr[1] = {&gatingSequence};
    addGatingSequences(arr, 1);
  }

  int64_t getMinimumGatingSequence() {
    return sequencer_->getMinimumSequence();
  }

  bool removeGatingSequence(Sequence& sequence) {
    return sequencer_->removeGatingSequence(sequence);
  }

  std::shared_ptr<SequenceBarrier> newBarrier(Sequence* const* sequencesToTrack, int count) {
    return sequencer_->newBarrier(sequencesToTrack, count);
  }

  // Java convenience overload: newBarrier() with no dependent sequences.
  std::shared_ptr<SequenceBarrier> newBarrier() {
    return newBarrier(nullptr, 0);
  }

  std::shared_ptr<EventPoller<E>> newPoller(Sequence* const* gatingSequences, int count) {
    // Sequencer::newPoller is a Java generic API; in C++ the real implementation lives in AbstractSequencer.
    // Calling through Sequencer base would hit the placeholder template and return nullptr.
    auto* abstractSequencer = dynamic_cast<AbstractSequencer*>(sequencer_);
    if (abstractSequencer == nullptr) {
      throw std::runtime_error("Sequencer does not support newPoller (not an AbstractSequencer)");
    }
    return abstractSequencer->newPoller(*this, gatingSequences, count);
  }

  std::shared_ptr<EventPoller<E>> newPoller() {
    return newPoller(nullptr, 0);
  }

  // Sequenced (delegate to Sequencer)
  int getBufferSize() const override { return bufferSize_; }
  bool hasAvailableCapacity(int requiredCapacity) override { return sequencer_->hasAvailableCapacity(requiredCapacity); }
  int64_t remainingCapacity() override { return sequencer_->remainingCapacity(); }
  int64_t next() override { return sequencer_->next(); }
  int64_t next(int n) override { return sequencer_->next(n); }
  int64_t tryNext() override { return sequencer_->tryNext(); }
  int64_t tryNext(int n) override { return sequencer_->tryNext(n); }
  void publish(int64_t sequence) override { sequencer_->publish(sequence); }
  void publish(int64_t lo, int64_t hi) override { sequencer_->publish(lo, hi); }

  // EventSink core helpers
  void publishEvent(EventTranslator<E>& translator) override {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence);
    publish(sequence);
  }

  bool tryPublishEvent(EventTranslator<E>& translator) override {
    if (!hasAvailableCapacity(1)) {
      return false;
    }
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence);
    publish(sequence);
    return true;
  }

  void publishEvent(EventTranslatorVararg<E>& translator, const std::vector<void*>& args) override {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, args);
    publish(sequence);
  }

  bool tryPublishEvent(EventTranslatorVararg<E>& translator, const std::vector<void*>& args) override {
    if (!hasAvailableCapacity(1)) {
      return false;
    }
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, args);
    publish(sequence);
    return true;
  }

  template <typename A>
  void publishEvent(EventTranslatorOneArg<E, A>& translator, A arg0) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, arg0);
    publish(sequence);
  }

  template <typename A>
  bool tryPublishEvent(EventTranslatorOneArg<E, A>& translator, A arg0) {
    if (!hasAvailableCapacity(1)) return false;
    publishEvent(translator, arg0);
    return true;
  }

  template <typename A, typename B>
  void publishEvent(EventTranslatorTwoArg<E, A, B>& translator, A arg0, B arg1) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, arg0, arg1);
    publish(sequence);
  }

  template <typename A, typename B>
  bool tryPublishEvent(EventTranslatorTwoArg<E, A, B>& translator, A arg0, B arg1) {
    if (!hasAvailableCapacity(1)) return false;
    publishEvent(translator, arg0, arg1);
    return true;
  }

  template <typename A, typename B, typename C>
  void publishEvent(EventTranslatorThreeArg<E, A, B, C>& translator, A arg0, B arg1, C arg2) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, arg0, arg1, arg2);
    publish(sequence);
  }

  template <typename A, typename B, typename C>
  bool tryPublishEvent(EventTranslatorThreeArg<E, A, B, C>& translator, A arg0, B arg1, C arg2) {
    if (!hasAvailableCapacity(1)) return false;
    publishEvent(translator, arg0, arg1, arg2);
    return true;
  }

  void publishEvents(std::vector<EventTranslator<E>*>& translators) override {
    publishEvents(translators, 0, static_cast<int>(translators.size()));
  }

  void publishEvents(std::vector<EventTranslator<E>*>& translators, int batchStartsAt, int batchSize) override {
    if (batchSize == 0) {
      return;
    }
    int64_t finalSequence = next(batchSize);
    int64_t initialSequence = finalSequence - (batchSize - 1);
    try {
      for (int i = 0; i < batchSize; ++i) {
        auto* tr = translators[static_cast<size_t>(batchStartsAt + i)];
        if (tr) {
          tr->translateTo(get(initialSequence + i), initialSequence + i);
        }
      }
    } catch (...) {
      publish(initialSequence, finalSequence);
      throw;
    }
    publish(initialSequence, finalSequence);
  }

  bool tryPublishEvents(std::vector<EventTranslator<E>*>& translators) override {
    return tryPublishEvents(translators, 0, static_cast<int>(translators.size()));
  }

  bool tryPublishEvents(std::vector<EventTranslator<E>*>& translators, int batchStartsAt, int batchSize) override {
    if (batchSize == 0) {
      return true;
    }
    if (!hasAvailableCapacity(batchSize)) {
      return false;
    }
    publishEvents(translators, batchStartsAt, batchSize);
    return true;
  }

  // Java exposes a public constructor RingBuffer(EventFactory, Sequencer). This is required by some tests
  // (e.g. RingBufferWithAssertingStubTest) that inject custom Sequencer implementations.
  RingBuffer(std::shared_ptr<EventFactory<E>> eventFactory, std::unique_ptr<Sequencer> sequencer)
      : indexMask_(sequencer->getBufferSize() - 1),
        entries_(static_cast<size_t>(sequencer->getBufferSize() + 2 * BUFFER_PAD)),
        bufferSize_(sequencer->getBufferSize()),
        sequencerOwner_(std::move(sequencer)),
        sequencer_(sequencerOwner_.get()) {
    if (!eventFactory) {
      throw std::invalid_argument("eventFactory must not be null");
    }
    if (bufferSize_ < 1) {
      throw std::invalid_argument("bufferSize must not be less than 1");
    }
    if ((bufferSize_ & (bufferSize_ - 1)) != 0) {
      throw std::invalid_argument("bufferSize must be a power of 2");
    }
    fill(*eventFactory);
  }

protected:
  // (No protected template hooks; overloads are implemented directly in RingBuffer.)

private:
  static constexpr int BUFFER_PAD = 32;

  void fill(EventFactory<E>& eventFactory) {
    for (int i = 0; i < bufferSize_; ++i) {
      entries_[static_cast<size_t>(BUFFER_PAD + i)] = eventFactory.newInstance();
    }
  }

  E& elementAt(int64_t sequence) {
    return entries_[static_cast<size_t>(BUFFER_PAD + (static_cast<int>(sequence) & static_cast<int>(indexMask_)))];
  }

  int64_t indexMask_;
  std::vector<E> entries_;
  int bufferSize_;
  std::unique_ptr<Sequencer> sequencerOwner_;
  Sequencer* sequencer_;
};

} // namespace disruptor
