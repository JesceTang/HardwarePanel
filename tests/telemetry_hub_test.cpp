#include <gtest/gtest.h>

#include <thread>

#include "hwpanel/core/telemetry_hub.h"

using hwpanel::core::TelemetryHub;
using hwpanel::core::TelemetrySample;

namespace {
TelemetrySample MakeSample(int64_t ts, double value) {
  TelemetrySample sample;
  sample.timestamp_ms = ts;
  sample.metrics.push_back({"cpu.usage_pct", value, "percent"});
  return sample;
}
}  // namespace

TEST(TelemetryHubTest, SubscriberReceivesPublishedSamples) {
  TelemetryHub hub(10);
  auto sub = hub.Subscribe();
  hub.Publish(MakeSample(1, 10.0));
  hub.Publish(MakeSample(2, 20.0));

  TelemetrySample out;
  ASSERT_TRUE(sub->Pop(out, 500));
  EXPECT_EQ(out.timestamp_ms, 1);
  ASSERT_TRUE(sub->Pop(out, 500));
  EXPECT_EQ(out.timestamp_ms, 2);
  EXPECT_FALSE(sub->Pop(out, 100));  // queue drained -> timeout
}

TEST(TelemetryHubTest, HistoryIsCappedAndOrdered) {
  TelemetryHub hub(5);
  for (int64_t i = 1; i <= 9; ++i) {
    hub.Publish(MakeSample(i, static_cast<double>(i)));
  }
  EXPECT_EQ(hub.HistorySize(), 5u);
  const auto history = hub.History(100);
  ASSERT_EQ(history.size(), 5u);
  EXPECT_EQ(history.front().timestamp_ms, 5);
  EXPECT_EQ(history.back().timestamp_ms, 9);
  EXPECT_EQ(hub.History(2).size(), 2u);
}

TEST(TelemetryHubTest, SlowConsumerDropsOldestInsteadOfBlocking) {
  TelemetryHub hub(10);
  auto sub = hub.Subscribe();
  for (int64_t i = 1; i <= TelemetryHub::kSubscriberQueueCapacity + 5; ++i) {
    hub.Publish(MakeSample(i, 1.0));
  }
  EXPECT_EQ(sub->QueueSize(), TelemetryHub::kSubscriberQueueCapacity);
}

TEST(TelemetryHubTest, CloseUnblocksPop) {
  TelemetryHub hub(10);
  auto sub = hub.Subscribe();
  std::thread closer([&sub] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sub->Close();
  });
  TelemetrySample out;
  EXPECT_FALSE(sub->Pop(out, 2000));
  closer.join();
}
