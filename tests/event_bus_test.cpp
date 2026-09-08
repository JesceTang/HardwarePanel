#include <gtest/gtest.h>

#include <thread>

#include "hwpanel/core/event_bus.h"

using hwpanel::core::EventBus;
using hwpanel::core::EventMessage;

TEST(EventBusTest, PublishReachesSubscriber) {
  EventBus bus;
  auto sub = bus.Subscribe();
  bus.PublishNow("info", "title", "body");

  EventMessage out;
  ASSERT_TRUE(sub->Pop(out, 500));
  EXPECT_EQ(out.level, "info");
  EXPECT_EQ(out.title, "title");
  EXPECT_EQ(out.message, "body");
  EXPECT_GT(out.timestamp_ms, 0);
}

TEST(EventBusTest, MultipleSubscribersAreIndependent) {
  EventBus bus;
  auto a = bus.Subscribe();
  auto b = bus.Subscribe();
  bus.PublishNow("warn", "t", "m");

  EventMessage out;
  ASSERT_TRUE(a->Pop(out, 500));
  ASSERT_TRUE(b->Pop(out, 500));
  EXPECT_FALSE(a->Pop(out, 50));
  EXPECT_FALSE(b->Pop(out, 50));
}

TEST(EventBusTest, DroppedSubscriberIsPruned) {
  EventBus bus;
  {
    auto sub = bus.Subscribe();
    bus.PublishNow("info", "t", "m");
  }
  // Publishing after the subscriber died must not crash or leak.
  bus.PublishNow("info", "t2", "m2");
  SUCCEED();
}
