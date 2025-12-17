#pragma once
// 1:1 port of com.lmax.disruptor.RingBuffer
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/RingBuffer.java

#include "BlockingWaitStrategy.h"
#include "DataProvider.h"
#include "EventFactory.h"
#include "EventPoller.h"
#include "EventTranslator.h"
#include "EventTranslatorOneArg.h"
#include "EventTranslatorThreeArg.h"
#include "EventTranslatorTwoArg.h"
#include "EventTranslatorVararg.h"
#include "MultiProducerSequencer.h"
#include "Sequence.h"
#include "Sequencer.h"
#include "SingleProducerSequencer.h"
#include "WaitStrategy.h"

#include "dsl/ProducerType.h"

#include "AbstractSequencer.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace disruptor {

// Template RingBuffer: parameterized by the concrete Sequencer type.
template <typename E, typename SequencerT>
class RingBuffer final : public DataProvider<E> {
public:
  static constexpr int64_t INITIAL_CURSOR_VALUE = Sequence::INITIAL_VALUE;
  using SequencerType = SequencerT;

  // Factory methods
  template <typename WaitStrategyT>
  static std::shared_ptr<RingBuffer<E, MultiProducerSequencer<WaitStrategyT>>>
  createMultiProducer(std::shared_ptr<EventFactory<E>> factory, int bufferSize,
                      WaitStrategyT &waitStrategy) {
    using Seq = MultiProducerSequencer<WaitStrategyT>;
    auto seq = std::make_unique<Seq>(bufferSize, waitStrategy);
    return std::shared_ptr<RingBuffer<E, Seq>>(
        new RingBuffer<E, Seq>(std::move(factory), std::move(seq)));
  }

  template <typename WaitStrategyT>
  static std::shared_ptr<RingBuffer<E, SingleProducerSequencer<WaitStrategyT>>>
  createSingleProducer(std::shared_ptr<EventFactory<E>> factory, int bufferSize,
                       WaitStrategyT &waitStrategy) {
    using Seq = SingleProducerSequencer<WaitStrategyT>;
    auto seq = std::make_unique<Seq>(bufferSize, waitStrategy);
    return std::shared_ptr<RingBuffer<E, Seq>>(
        new RingBuffer<E, Seq>(std::move(factory), std::move(seq)));
  }

  // DataProvider
  E &get(int64_t sequence) override { return elementAt(sequence); }

  int64_t getCursor() const { return sequencer().getCursor(); }

  // RingBuffer-specific helpers from Java
  void addGatingSequences(Sequence *const *gatingSequences, int count) {
    sequencer().addGatingSequences(gatingSequences, count);
  }

  void addGatingSequences(Sequence &gatingSequence) {
    Sequence *arr[1] = {&gatingSequence};
    addGatingSequences(arr, 1);
  }

  int64_t getMinimumGatingSequence() {
    return sequencer().getMinimumSequence();
  }

  bool removeGatingSequence(Sequence &sequence) {
    return sequencer().removeGatingSequence(sequence);
  }

  auto newBarrier(Sequence *const *sequencesToTrack, int count) {
    return sequencer().newBarrier(sequencesToTrack, count);
  }

  // Java convenience overload: newBarrier() with no dependent sequences.
  auto newBarrier() { return newBarrier(nullptr, 0); }

  std::shared_ptr<EventPoller<E, SequencerT>>
  newPoller(Sequence *const *gatingSequences, int count) {
    auto pollerSequence = std::make_shared<Sequence>();
    return EventPoller<E, SequencerT>::newInstance(
        *this, sequencer(), std::move(pollerSequence),
        sequencer().cursorSequence(), gatingSequences, count);
  }

  std::shared_ptr<EventPoller<E, SequencerT>> newPoller() {
    return newPoller(nullptr, 0);
  }

  int getBufferSize() const { return bufferSize_; }
  bool hasAvailableCapacity(int requiredCapacity) {
    return sequencer().hasAvailableCapacity(requiredCapacity);
  }
  int64_t remainingCapacity() { return sequencer().remainingCapacity(); }
  int64_t next() { return sequencer().next(); }
  int64_t next(int n) { return sequencer().next(n); }
  int64_t tryNext() { return sequencer().tryNext(); }
  int64_t tryNext(int n) { return sequencer().tryNext(n); }
  void publish(int64_t sequence) { sequencer().publish(sequence); }
  void publish(int64_t lo, int64_t hi) { sequencer().publish(lo, hi); }

  // EventSink-like helpers
  void publishEvent(EventTranslator<E> &translator) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence);
    publish(sequence);
  }

  bool tryPublishEvent(EventTranslator<E> &translator) {
    if (!hasAvailableCapacity(1)) {
      return false;
    }
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence);
    publish(sequence);
    return true;
  }

  void publishEvent(EventTranslatorVararg<E> &translator,
                    const std::vector<void *> &args) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, args);
    publish(sequence);
  }

  bool tryPublishEvent(EventTranslatorVararg<E> &translator,
                       const std::vector<void *> &args) {
    if (!hasAvailableCapacity(1)) {
      return false;
    }
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, args);
    publish(sequence);
    return true;
  }

  template <typename A>
  void publishEvent(EventTranslatorOneArg<E, A> &translator, A arg0) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, arg0);
    publish(sequence);
  }

  template <typename A>
  bool tryPublishEvent(EventTranslatorOneArg<E, A> &translator, A arg0) {
    if (!hasAvailableCapacity(1))
      return false;
    publishEvent(translator, arg0);
    return true;
  }

  template <typename A, typename B>
  void publishEvent(EventTranslatorTwoArg<E, A, B> &translator, A arg0,
                    B arg1) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, arg0, arg1);
    publish(sequence);
  }

  template <typename A, typename B>
  bool tryPublishEvent(EventTranslatorTwoArg<E, A, B> &translator, A arg0,
                       B arg1) {
    if (!hasAvailableCapacity(1))
      return false;
    publishEvent(translator, arg0, arg1);
    return true;
  }

  template <typename A, typename B, typename C>
  void publishEvent(EventTranslatorThreeArg<E, A, B, C> &translator, A arg0,
                    B arg1, C arg2) {
    int64_t sequence = next();
    translator.translateTo(get(sequence), sequence, arg0, arg1, arg2);
    publish(sequence);
  }

  template <typename A, typename B, typename C>
  bool tryPublishEvent(EventTranslatorThreeArg<E, A, B, C> &translator, A arg0,
                       B arg1, C arg2) {
    if (!hasAvailableCapacity(1))
      return false;
    publishEvent(translator, arg0, arg1, arg2);
    return true;
  }

  void publishEvents(std::vector<EventTranslator<E> *> &translators) {
    publishEvents(translators, 0, static_cast<int>(translators.size()));
  }

  void publishEvents(std::vector<EventTranslator<E> *> &translators,
                     int batchStartsAt, int batchSize) {
    if (batchSize == 0) {
      return;
    }
    int64_t finalSequence = next(batchSize);
    int64_t initialSequence = finalSequence - (batchSize - 1);
    try {
      for (int i = 0; i < batchSize; ++i) {
        auto *tr = translators[static_cast<size_t>(batchStartsAt + i)];
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

  bool tryPublishEvents(std::vector<EventTranslator<E> *> &translators) {
    return tryPublishEvents(translators, 0,
                            static_cast<int>(translators.size()));
  }

  bool tryPublishEvents(std::vector<EventTranslator<E> *> &translators,
                        int batchStartsAt, int batchSize) {
    if (batchSize == 0) {
      return true;
    }
    if (!hasAvailableCapacity(batchSize)) {
      return false;
    }
    publishEvents(translators, batchStartsAt, batchSize);
    return true;
  }

  // Java exposes a public constructor RingBuffer(EventFactory, Sequencer). This
  // is required by some tests (e.g. RingBufferWithAssertingStubTest) that
  // inject custom Sequencer implementations.
  // Constructor accepting SequencerT construction arguments (for non-movable
  // sequencers). We construct SequencerT in-place in the optional to avoid
  // move/copy.
  template <typename... SequencerArgs>
  RingBuffer(std::shared_ptr<EventFactory<E>> eventFactory, std::in_place_t,
             SequencerArgs &&...sequencerArgs)
      : sequencerValue_(std::in_place,
                        std::forward<SequencerArgs>(sequencerArgs)...),
        sequencerOwner_(nullptr), usingValue_(true) {
    bufferSize_ = sequencerValue_->getBufferSize();
    indexMask_ = bufferSize_ - 1;
    entries_.resize(static_cast<size_t>(bufferSize_ + 2 * BUFFER_PAD));
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

  // Legacy constructor accepting unique_ptr (for backward compatibility with
  // tests).
  RingBuffer(std::shared_ptr<EventFactory<E>> eventFactory,
             std::unique_ptr<SequencerT> sequencer)
      : indexMask_(sequencer->getBufferSize() - 1),
        entries_(
            static_cast<size_t>(sequencer->getBufferSize() + 2 * BUFFER_PAD)),
        bufferSize_(sequencer->getBufferSize()), sequencerValue_(std::nullopt),
        sequencerOwner_(std::move(sequencer)), usingValue_(false) {
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
  // (No protected template hooks; overloads are implemented directly in
  // RingBuffer.)

private:
  static constexpr int BUFFER_PAD = 32;

  void fill(EventFactory<E> &eventFactory) {
    for (int i = 0; i < bufferSize_; ++i) {
      entries_[static_cast<size_t>(BUFFER_PAD + i)] =
          eventFactory.newInstance();
    }
  }

  E &elementAt(int64_t sequence) {
    return entries_[static_cast<size_t>(
        BUFFER_PAD +
        (static_cast<int>(sequence) & static_cast<int>(indexMask_)))];
  }

  int64_t indexMask_;
  std::vector<E> entries_;
  int bufferSize_;
  // For value-based constructor: sequencer_ is stored by value in optional.
  // For unique_ptr-based constructor: sequencerOwner_ holds the sequencer.
  std::optional<SequencerT> sequencerValue_;
  std::unique_ptr<SequencerT> sequencerOwner_;
  bool usingValue_;

  SequencerT &sequencer() {
    return usingValue_ ? *sequencerValue_ : *sequencerOwner_;
  }
  const SequencerT &sequencer() const {
    return usingValue_ ? *sequencerValue_ : *sequencerOwner_;
  }
};

} // namespace disruptor
