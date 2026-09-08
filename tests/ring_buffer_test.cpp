#include <gtest/gtest.h>

#include "hwpanel/core/ring_buffer.h"

using hwpanel::core::RingBuffer;

TEST(RingBufferTest, KeepsInsertionOrderBelowCapacity) {
  RingBuffer<int> buffer(4);
  for (int i = 1; i <= 3; ++i) {
    buffer.Push(i);
  }
  EXPECT_EQ(buffer.Size(), 3u);
  const auto all = buffer.All();
  ASSERT_EQ(all.size(), 3u);
  EXPECT_EQ(all[0], 1);
  EXPECT_EQ(all[2], 3);
}

TEST(RingBufferTest, OverwritesOldestWhenFull) {
  RingBuffer<int> buffer(3);
  for (int i = 1; i <= 5; ++i) {
    buffer.Push(i);
  }
  EXPECT_EQ(buffer.Size(), 3u);
  EXPECT_EQ(buffer.TotalPushed(), 5u);
  const auto all = buffer.All();
  ASSERT_EQ(all.size(), 3u);
  EXPECT_EQ(all[0], 3);
  EXPECT_EQ(all[1], 4);
  EXPECT_EQ(all[2], 5);
}

TEST(RingBufferTest, LastNClampsToSize) {
  RingBuffer<int> buffer(8);
  buffer.Push(42);
  EXPECT_EQ(buffer.LastN(100).size(), 1u);
  EXPECT_EQ(buffer.LastN(0).size(), 0u);
}
