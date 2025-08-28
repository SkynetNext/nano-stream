#include "nano_stream/sequence.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace nano_stream;

class SequenceTest : public ::testing::Test {
protected:
  void SetUp() override { sequence = std::make_unique<Sequence>(); }

  std::unique_ptr<Sequence> sequence;
};

TEST_F(SequenceTest, InitialValue) {
  EXPECT_EQ(sequence->get(), Sequence::INITIAL_VALUE);
}

TEST_F(SequenceTest, InitialValueConstructor) {
  Sequence seq(42);
  EXPECT_EQ(seq.get(), 42);
}

TEST_F(SequenceTest, SetAndGet) {
  sequence->set(100);
  EXPECT_EQ(sequence->get(), 100);
}

TEST_F(SequenceTest, SetVolatile) {
  sequence->set_volatile(200);
  EXPECT_EQ(sequence->get(), 200);
}

TEST_F(SequenceTest, CompareAndSet) {
  sequence->set(50);

  // Successful CAS
  EXPECT_TRUE(sequence->compare_and_set(50, 75));
  EXPECT_EQ(sequence->get(), 75);

  // Failed CAS
  EXPECT_FALSE(sequence->compare_and_set(50, 100));
  EXPECT_EQ(sequence->get(), 75);
}

TEST_F(SequenceTest, IncrementAndGet) {
  sequence->set(10);
  EXPECT_EQ(sequence->increment_and_get(), 11);
  EXPECT_EQ(sequence->get(), 11);
}

TEST_F(SequenceTest, AddAndGet) {
  sequence->set(20);
  EXPECT_EQ(sequence->add_and_get(5), 25);
  EXPECT_EQ(sequence->get(), 25);
}

TEST_F(SequenceTest, GetAndAdd) {
  sequence->set(30);
  EXPECT_EQ(sequence->get_and_add(5), 30);
  EXPECT_EQ(sequence->get(), 35);
}

TEST_F(SequenceTest, ConcurrentAccess) {
  const int num_threads = 4;
  const int increments_per_thread = 1000;

  sequence->set(0);

  std::vector<std::thread> threads;
  std::atomic<int> completed_threads{0};

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&]() {
      for (int j = 0; j < increments_per_thread; ++j) {
        sequence->increment_and_get();
      }
      completed_threads.fetch_add(1);
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(completed_threads.load(), num_threads);
  EXPECT_EQ(sequence->get(), num_threads * increments_per_thread);
}

TEST_F(SequenceTest, MemoryAlignment) {
  // Test that Sequence is properly aligned
  EXPECT_EQ(alignof(Sequence), std::hardware_destructive_interference_size);
  EXPECT_GE(sizeof(Sequence), std::hardware_destructive_interference_size);
}
