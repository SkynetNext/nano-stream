// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/PullWithBatchedPoller.java

#include "disruptor/EventFactory.h"
#include "disruptor/EventPoller.h"
#include "disruptor/RingBuffer.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {

template <typename T, typename SequencerT> class BatchedPoller {
public:
  struct DataEvent {
    T data{};

    static std::shared_ptr<disruptor::EventFactory<DataEvent>> factory() {
      struct F final : public disruptor::EventFactory<DataEvent> {
        DataEvent newInstance() override { return DataEvent(); }
      };
      return std::make_shared<F>();
    }

    T copyOfData() { return data; }
    void set(const T &d) { data = d; }
  };

  using RingBufferT = disruptor::RingBuffer<DataEvent, SequencerT>;

  BatchedPoller(RingBufferT &ringBuffer, int batchSize)
      : poller_(ringBuffer.newPoller()), data_(batchSize) {
    // Java: ringBuffer.addGatingSequences(poller.getSequence());
    ringBuffer.addGatingSequences(poller_->getSequence());
  }

  T poll() {
    if (data_.msgCount() > 0) {
      return data_.pollMessage();
    }
    loadNextValues();
    return data_.msgCount() > 0 ? data_.pollMessage() : T{};
  }

private:
  class BatchedData {
  public:
    explicit BatchedData(int capacity) : capacity_(capacity) {
      data_.reserve(static_cast<size_t>(capacity));
    }

    int msgCount() const { return static_cast<int>(data_.size()) - cursor_; }

    bool addDataItem(const T &item) {
      if (static_cast<int>(data_.size()) >= capacity_) {
        throw std::out_of_range("Attempting to add item to full batch");
      }
      data_.push_back(item);
      return static_cast<int>(data_.size()) < capacity_;
    }

    T pollMessage() {
      T rt{};
      if (cursor_ < static_cast<int>(data_.size())) {
        rt = data_[static_cast<size_t>(cursor_++)];
      }
      if (cursor_ > 0 && cursor_ >= static_cast<int>(data_.size())) {
        clearCount();
      }
      return rt;
    }

  private:
    void clearCount() {
      data_.clear();
      cursor_ = 0;
    }

    int capacity_;
    std::vector<T> data_{};
    int cursor_{0};
  };

  template <typename PollerT> class Handler final : public PollerT::Handler {
  public:
    explicit Handler(BatchedData *batch) : batch_(batch) {}
    bool onEvent(DataEvent &event, int64_t /*sequence*/,
                 bool /*endOfBatch*/) override {
      T item = event.copyOfData();
      // Java: return item != null ? batch.addDataItem(item) : false;
      // In C++ example, treat default-constructed as "null".
      return batch_->addDataItem(item);
    }

  private:
    BatchedData *batch_;
  };

  void loadNextValues() {
    using PollerT = std::remove_reference_t<decltype(*poller_)>;
    Handler<PollerT> h(&data_);
    poller_->poll(h);
  }

  std::shared_ptr<disruptor::EventPoller<DataEvent, SequencerT>> poller_;
  BatchedData data_;
};

} // namespace

int main() {
  int batchSize = 40;
  using WS = disruptor::BlockingWaitStrategy;
  WS ws;
  using Seq = disruptor::MultiProducerSequencer<WS>;
  using Ev = typename BatchedPoller<void *, Seq>::DataEvent;
  using RB = disruptor::RingBuffer<Ev, Seq>;
  auto ringBuffer = RB::createMultiProducer(Ev::factory(), 1024, ws);

  BatchedPoller<void *, Seq> poller(*ringBuffer, batchSize);
  void *value = poller.poll();
  (void)value;
  return 0;
}
