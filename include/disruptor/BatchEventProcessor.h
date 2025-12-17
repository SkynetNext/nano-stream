#pragma once
// 1:1 port of com.lmax.disruptor.BatchEventProcessor
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/BatchEventProcessor.java

#include "AlertException.h"
#include "BatchRewindStrategy.h"
#include "DataProvider.h"
#include "EventHandlerBase.h"
#include "EventProcessor.h"
#include "ExceptionHandler.h"
#include "ExceptionHandlers.h"
#include "RewindAction.h"
#include "RewindHandler.h"
#include "RewindableEventHandler.h"
#include "RewindableException.h"
#include "Sequence.h"
#include "Sequencer.h"
#include "TimeoutException.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace disruptor {

template <typename T, typename BarrierT>
class BatchEventProcessor final : public EventProcessor {
public:
  BatchEventProcessor(DataProvider<T>& dataProvider,
                      BarrierT& sequenceBarrier,
                      EventHandlerBase<T>& eventHandler,
                      int maxBatchSize,
                      BatchRewindStrategy* batchRewindStrategy)
      : running_(IDLE),
        exceptionHandler_(nullptr),
        dataProvider_(&dataProvider),
        sequenceBarrier_(&sequenceBarrier),
        eventHandler_(&eventHandler),
        batchLimitOffset_(maxBatchSize - 1),
        sequence_(SEQUENCER_INITIAL_CURSOR_VALUE),
        retriesAttempted_(0) {
    if (maxBatchSize < 1) {
      throw std::invalid_argument("maxBatchSize must be greater than 0");
    }

    // Java: if eventHandler instanceof RewindableEventHandler -> TryRewindHandler(batchRewindStrategy) else NoRewindHandler
    if (dynamic_cast<RewindableEventHandler<T>*>(&eventHandler) != nullptr) {
      rewindHandler_ = std::make_unique<TryRewindHandler>(*this, batchRewindStrategy);
    } else {
      rewindHandler_ = std::make_unique<NoRewindHandler>();
    }
  }

  Sequence& getSequence() override { return sequence_; }

  void halt() override {
    running_.store(HALTED, std::memory_order_release);
    sequenceBarrier_->alert();
  }

  bool isRunning() override { return running_.load(std::memory_order_acquire) != IDLE; }

  void setExceptionHandler(ExceptionHandler<T>& exceptionHandler) {
    exceptionHandler_ = &exceptionHandler;
    if (exceptionHandler_ == nullptr) {
      throw std::invalid_argument("exceptionHandler must not be null");
    }
  }

  void run() override {
    int expected = IDLE;
    if (running_.compare_exchange_strong(expected, RUNNING, std::memory_order_acq_rel)) {
      sequenceBarrier_->clearAlert();

      notifyStart();
      try {
        if (running_.load(std::memory_order_acquire) == RUNNING) {
          processEvents();
        }
      } catch (...) {
        notifyShutdown();
        running_.store(IDLE, std::memory_order_release);
        throw;
      }
      notifyShutdown();
      running_.store(IDLE, std::memory_order_release);
    } else {
      if (expected == RUNNING) {
        throw std::runtime_error("Thread is already running");
      }
      earlyExit();
    }
  }

private:
  static constexpr int IDLE = 0;
  static constexpr int HALTED = IDLE + 1;
  static constexpr int RUNNING = HALTED + 1;

  std::atomic<int> running_;
  ExceptionHandler<T>* exceptionHandler_;
  DataProvider<T>* dataProvider_;
  BarrierT* sequenceBarrier_;
  EventHandlerBase<T>* eventHandler_;
  int batchLimitOffset_;
  Sequence sequence_;
  std::unique_ptr<RewindHandler> rewindHandler_;
  int retriesAttempted_;

  void processEvents() {
    T* event = nullptr;
    int64_t nextSequence = sequence_.get() + 1;

    while (true) {
      const int64_t startOfBatchSequence = nextSequence;
      try {
        try {
          const int64_t availableSequence = sequenceBarrier_->waitFor(nextSequence);
          if (availableSequence < nextSequence) {
            // Java: if insufficient available, continue waiting without moving the processor sequence backwards.
            continue;
          }
          const int64_t endOfBatchSequence = std::min(nextSequence + batchLimitOffset_, availableSequence);

          if (nextSequence <= endOfBatchSequence) {
            eventHandler_->onBatchStart(endOfBatchSequence - nextSequence + 1, availableSequence - nextSequence + 1);
          }

          while (nextSequence <= endOfBatchSequence) {
            event = &dataProvider_->get(nextSequence);
            eventHandler_->onEvent(*event, nextSequence, nextSequence == endOfBatchSequence);
            ++nextSequence;
          }

          retriesAttempted_ = 0;
          sequence_.set(endOfBatchSequence);
        } catch (const RewindableException& e) {
          nextSequence = rewindHandler_->attemptRewindGetNextSequence(e, startOfBatchSequence);
        }
      } catch (const TimeoutException&) {
        notifyTimeout(sequence_.get());
      } catch (const AlertException&) {
        if (running_.load(std::memory_order_acquire) != RUNNING) {
          break;
        }
      } catch (const std::exception& ex) {
        handleEventException(ex, nextSequence, event);
        sequence_.set(nextSequence);
        ++nextSequence;
      }
    }
  }

  void earlyExit() {
    notifyStart();
    notifyShutdown();
  }

  void notifyTimeout(int64_t availableSequence) {
    try {
      eventHandler_->onTimeout(availableSequence);
    } catch (const std::exception& e) {
      handleEventException(e, availableSequence, nullptr);
    }
  }

  void notifyStart() {
    try {
      eventHandler_->onStart();
    } catch (const std::exception& ex) {
      handleOnStartException(ex);
    }
  }

  void notifyShutdown() {
    try {
      eventHandler_->onShutdown();
    } catch (const std::exception& ex) {
      handleOnShutdownException(ex);
    }
  }

  void handleEventException(const std::exception& ex, int64_t sequence, T* event) {
    // In Java, an ExceptionHandler throwing only kills the consumer thread.
    // In C++, an exception escaping a std::thread calls std::terminate (kills the process),
    // so we must guard against that to preserve Java semantics.
    try {
      getExceptionHandler()->handleEventException(ex, sequence, event);
    } catch (...) {
      // Stop processing and wake any waiters.
      halt();
    }
  }

  void handleOnStartException(const std::exception& ex) {
    getExceptionHandler()->handleOnStartException(ex);
  }

  void handleOnShutdownException(const std::exception& ex) {
    getExceptionHandler()->handleOnShutdownException(ex);
  }

  std::shared_ptr<ExceptionHandler<T>> getExceptionHandler() {
    // Java: handler == null ? ExceptionHandlers.defaultHandler() : handler;
    if (exceptionHandler_ == nullptr) {
      return ExceptionHandlers::defaultHandler<T>();
    }
    // Non-owning pointer -> wrap without deleting.
    return std::shared_ptr<ExceptionHandler<T>>(exceptionHandler_, [](ExceptionHandler<T>*) {});
  }

  class TryRewindHandler final : public RewindHandler {
  public:
    TryRewindHandler(BatchEventProcessor& owner, BatchRewindStrategy* strategy)
        : owner_(&owner), strategy_(strategy) {}

    int64_t attemptRewindGetNextSequence(const RewindableException& e, int64_t startOfBatchSequence) override {
      if (strategy_ == nullptr) {
        throw std::runtime_error("batchRewindStrategy cannot be null when building a BatchEventProcessor");
      }
      // Java: if handleRewindException(e, ++retriesAttempted) == REWIND -> return start; else reset and throw e
      if (strategy_->handleRewindException(e, ++owner_->retriesAttempted_) == RewindAction::REWIND) {
        return startOfBatchSequence;
      }
      owner_->retriesAttempted_ = 0;
      throw e;
    }

  private:
    BatchEventProcessor* owner_;
    BatchRewindStrategy* strategy_;
  };

  class NoRewindHandler final : public RewindHandler {
  public:
    int64_t attemptRewindGetNextSequence(const RewindableException& /*e*/, int64_t /*startOfBatchSequence*/) override {
      throw std::runtime_error("Rewindable Exception thrown from a non-rewindable event handler");
    }
  };
};

} // namespace disruptor
