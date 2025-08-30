#include <chrono>
#include <cstddef>
#include <cstdint>
#include <disruptor/disruptor.h>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace disruptor;

// 定义事件类型
struct TradeEvent {
  uint64_t order_id;
  double price;
  uint32_t quantity;
  std::string symbol;
  uint64_t timestamp;

  TradeEvent() : order_id(0), price(0.0), quantity(0), timestamp(0) {}
};

// 自定义事件翻译器
class TradeEventTranslator : public EventTranslator<TradeEvent> {
private:
  uint64_t order_id_;
  double price_;
  uint32_t quantity_;
  std::string symbol_;

public:
  TradeEventTranslator(uint64_t order_id, double price, uint32_t quantity,
                       std::string symbol)
      : order_id_(order_id), price_(price), quantity_(quantity),
        symbol_(std::move(symbol)) {}

  void translate_to(TradeEvent &event, int64_t /*sequence*/) override {
    event.order_id = order_id_;
    event.price = price_;
    event.quantity = quantity_;
    event.symbol = symbol_;
    event.timestamp =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
  }
};

// 自定义事件处理器
class TradeEventHandler : public EventHandler<TradeEvent> {
private:
  std::string name_;

public:
  explicit TradeEventHandler(std::string name) : name_(std::move(name)) {}

  void on_event(TradeEvent &event, int64_t sequence,
                bool end_of_batch) override {
    std::cout << "[" << name_ << "] Processing trade: "
              << "ID=" << event.order_id << ", Symbol=" << event.symbol
              << ", Price=" << event.price << ", Quantity=" << event.quantity
              << ", Sequence=" << sequence
              << ", EndOfBatch=" << (end_of_batch ? "true" : "false")
              << std::endl;
  }
};

// 简化的异常处理器（仅用于向后兼容）
class LoggingExceptionHandler : public ExceptionHandler<TradeEvent> {
public:
  void handle_exception(const std::exception &e, TradeEvent & /*event*/,
                        int64_t sequence) override {
    std::cerr << "Error processing event at sequence " << sequence << ": "
              << e.what() << std::endl;
  }
};

int main() {
  std::cout << "Nano-Stream Advanced Example" << std::endl;
  std::cout << "Version: " << disruptor::Version::get_version_string()
            << std::endl;

  // 创建环形缓冲区
  const size_t buffer_size = 1024;
  RingBuffer<TradeEvent> ring_buffer(buffer_size,
                                     []() { return TradeEvent{}; });

  // 创建消费者
  auto event_handler = std::make_unique<TradeEventHandler>("TradeProcessor");
  Consumer<TradeEvent> consumer(ring_buffer, std::move(event_handler), 10,
                                std::chrono::milliseconds(1));

  // 设置异常处理器
  consumer.set_exception_handler(std::make_unique<LoggingExceptionHandler>());

  // 启动消费者
  auto start_result = consumer.start();
  if (start_result != ConsumerError::SUCCESS) {
    std::cerr << "Failed to start consumer" << std::endl;
    return 1;
  }

  std::cout << "Consumer started successfully" << std::endl;

  // 生产者线程
  std::thread producer([&ring_buffer]() {
    std::cout << "Producer started" << std::endl;

    for (int i = 0; i < 100; ++i) {
      // 方法1: 使用自定义事件翻译器
      TradeEventTranslator translator(i, 100.0 + i * 0.1, 100 + i * 10, "AAPL");

      auto result = ring_buffer.publish_event(translator);
      if (result != RingBufferError::SUCCESS) {
        std::cerr << "Failed to publish event " << i << std::endl;
        continue;
      }

      // 方法2: 使用Lambda事件翻译器
      auto lambda_translator = LambdaEventTranslator<TradeEvent>(
          [](TradeEvent &event, int64_t sequence) {
            event.order_id = sequence + 1000;
            event.price = 200.0 + sequence * 0.05;
            event.quantity = 50 + sequence * 5;
            event.symbol = "GOOGL";
            event.timestamp =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now()
                        .time_since_epoch())
                    .count();
          });

      auto lambda_result = ring_buffer.publish_event(lambda_translator);

      if (lambda_result != RingBufferError::SUCCESS) {
        std::cerr << "Failed to publish lambda event " << i << std::endl;
        continue;
      }

      // 方法3: 直接操作（展示基本用法）
      int64_t sequence = ring_buffer.next();
      if (sequence != -1) {
        TradeEvent &event = ring_buffer.get(sequence);
        event.order_id = i + 2000;
        event.price = 150.0 + i * 0.02;
        event.quantity = 75 + i * 3;
        event.symbol = "MSFT";
        event.timestamp =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch())
                .count();
        ring_buffer.publish(sequence);
      }

      // 批量发布示例
      if (i % 10 == 0) {
        std::vector<TradeEventTranslator> translators;
        for (int j = 0; j < 5; ++j) {
          translators.emplace_back(i * 10 + j, 100.0 + j * 0.5, 100 + j * 20,
                                   "BATCH");
        }

        auto batch_result =
            ring_buffer.publish_events(translators.data(), 0, 5);
        if (batch_result != RingBufferError::SUCCESS) {
          std::cerr << "Failed to publish batch " << i << std::endl;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Producer finished" << std::endl;
  });

  // 等待生产者完成
  producer.join();

  // 等待一段时间让消费者处理完所有事件
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // 停止消费者
  auto stop_result = consumer.stop();
  if (stop_result != ConsumerError::SUCCESS) {
    std::cerr << "Failed to stop consumer" << std::endl;
    return 1;
  }

  // 打印统计信息
  std::cout << "\n=== Statistics ===" << std::endl;
  std::cout << "Events processed: " << consumer.get_events_processed()
            << std::endl;
  std::cout << "Batches processed: " << consumer.get_batches_processed()
            << std::endl;
  std::cout << "Final sequence: " << consumer.get_sequence() << std::endl;
  std::cout << "Ring buffer cursor: " << ring_buffer.get_cursor() << std::endl;
  std::cout << "Remaining capacity: " << ring_buffer.remaining_capacity()
            << std::endl;

  std::cout << "\nAdvanced example completed successfully!" << std::endl;
  return 0;
}
