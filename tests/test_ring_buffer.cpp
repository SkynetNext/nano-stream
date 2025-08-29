#include "nano_stream/event_translator.h"
#include "nano_stream/ring_buffer.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>

using namespace nano_stream;

// Simple test event
struct TestEvent {
  int64_t value = 0;

  TestEvent() = default;
  TestEvent(int64_t v) : value(v) {}
};

class RingBufferTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a ring buffer with default-constructed events
    ring_buffer = std::make_unique<RingBuffer<TestEvent>>(
        1024, []() { return TestEvent(); });
  }

  std::unique_ptr<RingBuffer<TestEvent>> ring_buffer;
};

TEST_F(RingBufferTest, Construction) {
  EXPECT_EQ(ring_buffer->get_buffer_size(), 1024);
  EXPECT_EQ(ring_buffer->get_cursor(),
            RingBuffer<TestEvent>::INITIAL_CURSOR_VALUE);
}

TEST_F(RingBufferTest, InvalidBufferSize) {
  // Buffer size must be power of 2
  // Constructors should throw exceptions for invalid buffer sizes
  EXPECT_THROW((RingBuffer<TestEvent>(1023, []() { return TestEvent(); })),
               std::invalid_argument);

  // Buffer size must be > 0
  EXPECT_THROW((RingBuffer<TestEvent>(0, []() { return TestEvent(); })),
               std::invalid_argument);
}

TEST_F(RingBufferTest, BasicProducerConsumer) {
  // Publish a single event
  int64_t sequence = ring_buffer->next();
  EXPECT_NE(sequence, -1); // Check for error sentinel value

  TestEvent &event = ring_buffer->get(sequence);
  event.value = 42;
  ring_buffer->publish(sequence);

  EXPECT_EQ(ring_buffer->get_cursor(), sequence);
  EXPECT_EQ(ring_buffer->get(sequence).value, 42);
}

TEST_F(RingBufferTest, MultipleEvents) {
  const int num_events = 100;

  for (int i = 0; i < num_events; ++i) {
    int64_t sequence = ring_buffer->next();
    EXPECT_NE(sequence, -1); // Check for error sentinel value

    TestEvent &event = ring_buffer->get(sequence);
    event.value = i;
    ring_buffer->publish(sequence);
  }

  // Verify all events
  for (int i = 0; i < num_events; ++i) {
    const TestEvent &event = ring_buffer->get(i);
    EXPECT_EQ(event.value, i);
  }
}

TEST_F(RingBufferTest, TryNext) {
  // Should succeed when buffer is empty
  int64_t sequence;
  auto result = ring_buffer->try_next(sequence);
  EXPECT_EQ(result, RingBufferError::SUCCESS);
  EXPECT_GE(sequence, 0);

  TestEvent &event = ring_buffer->get(sequence);
  event.value = 123;
  ring_buffer->publish(sequence);

  EXPECT_EQ(ring_buffer->get(sequence).value, 123);
}

TEST_F(RingBufferTest, BatchClaiming) {
  const int batch_size = 10;
  int64_t sequence = ring_buffer->next(batch_size);
  EXPECT_NE(sequence, -1); // Check for error sentinel value

  // Fill batch
  for (int i = 0; i < batch_size; ++i) {
    int64_t seq = sequence - batch_size + 1 + i;
    TestEvent &event = ring_buffer->get(seq);
    event.value = i * 10;
  }

  ring_buffer->publish(sequence);

  // Verify batch
  for (int i = 0; i < batch_size; ++i) {
    int64_t seq = sequence - batch_size + 1 + i;
    const TestEvent &event = ring_buffer->get(seq);
    EXPECT_EQ(event.value, i * 10);
  }
}

TEST_F(RingBufferTest, HasAvailableCapacity) {
  EXPECT_TRUE(ring_buffer->has_available_capacity(1));
  EXPECT_TRUE(ring_buffer->has_available_capacity(1024));

  // After publishing many events without gating sequences,
  // should still have capacity
  for (int i = 0; i < 500; ++i) {
    int64_t sequence = ring_buffer->next();
    EXPECT_NE(sequence, -1); // Check for error sentinel value
    ring_buffer->publish(sequence);
  }

  EXPECT_TRUE(ring_buffer->has_available_capacity(1));
}

TEST_F(RingBufferTest, RemainingCapacity) {
  size_t initial_capacity = ring_buffer->remaining_capacity();
  EXPECT_EQ(initial_capacity, 1024);

  // Publish some events
  for (int i = 0; i < 10; ++i) {
    int64_t sequence = ring_buffer->next();
    EXPECT_NE(sequence, -1); // Check for error sentinel value
    ring_buffer->publish(sequence);
  }

  // With a consumer at initial position, capacity should be reduced
  // The ring buffer tracks the minimum consumer position
  size_t new_capacity = ring_buffer->remaining_capacity();
  EXPECT_LE(new_capacity, initial_capacity);
}

TEST_F(RingBufferTest, IsAvailable) {
  // Initially no sequences are available
  EXPECT_FALSE(ring_buffer->is_available(0));

  // Publish one event
  int64_t sequence = ring_buffer->next();
  EXPECT_NE(sequence, -1); // Check for error sentinel value
  ring_buffer->publish(sequence);

  // Now that sequence should be available
  EXPECT_TRUE(ring_buffer->is_available(sequence));
  EXPECT_FALSE(ring_buffer->is_available(sequence + 1));
}

TEST_F(RingBufferTest, ConcurrentProducerSingleConsumer) {
  const int num_events = 10000;
  std::atomic<int> events_consumed{0};
  std::atomic<bool> stop_consumer{false};

  // Consumer thread
  std::thread consumer([&]() {
    int64_t next_to_read = 0;

    while (!stop_consumer.load() || events_consumed.load() < num_events) {
      if (ring_buffer->is_available(next_to_read)) {
        const TestEvent &event = ring_buffer->get(next_to_read);
        EXPECT_EQ(event.value, next_to_read);
        events_consumed.fetch_add(1);
        next_to_read++;
      } else {
        std::this_thread::yield();
      }
    }
  });

  // Producer thread
  std::thread producer([&]() {
    for (int i = 0; i < num_events; ++i) {
      int64_t sequence = ring_buffer->next();
      EXPECT_NE(sequence, -1); // Check for error sentinel value

      TestEvent &event = ring_buffer->get(sequence);
      event.value = sequence;
      ring_buffer->publish(sequence);
    }
    stop_consumer.store(true);
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(events_consumed.load(), num_events);
}

TEST_F(RingBufferTest, MemoryAlignment) {
  // Test that RingBuffer is properly aligned
  EXPECT_EQ(alignof(RingBuffer<TestEvent>),
            std::hardware_destructive_interference_size);
}

TEST_F(RingBufferTest, ErrorHandling) {
  // Test invalid arguments
  int64_t sequence;
  auto result = ring_buffer->try_next(0, sequence);
  EXPECT_EQ(result, RingBufferError::INVALID_ARGUMENT);

  result = ring_buffer->try_next(-1, sequence);
  EXPECT_EQ(result, RingBufferError::INVALID_ARGUMENT);
}

TEST_F(RingBufferTest, EventTranslator) {
  // Test event translator functionality
  struct TestTranslator : public EventTranslator<TestEvent> {
    void translate_to(TestEvent &event, int64_t sequence) override {
      event.value = sequence * 2;
    }
  };

  TestTranslator translator;
  auto result = ring_buffer->publish_event(translator);
  EXPECT_EQ(result, RingBufferError::SUCCESS);

  // Verify the event was published correctly
  EXPECT_EQ(ring_buffer->get(0).value, 0); // sequence 0 * 2 = 0
}
