#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "plugins/collectors/pipeline.h"

using hwpanel::collectors::CollectorPipeline;
using hwpanel::collectors::ICollector;
using hwpanel::core::MetricSample;
using hwpanel::core::TelemetrySample;

namespace {

class FakeCollector : public ICollector {
 public:
  explicit FakeCollector(bool available = true) : available_(available) {}
  std::string_view Id() const override { return "fake"; }
  bool Available() const override { return available_; }
  std::vector<MetricSample> Collect() override {
    ++calls_;
    return {{"fake.value", static_cast<double>(calls_), "count"}};
  }
  int calls() const { return calls_; }

 private:
  bool available_;
  int calls_ = 0;
};

class ThrowingCollector : public ICollector {
 public:
  std::string_view Id() const override { return "thrower"; }
  bool Available() const override { return true; }
  std::vector<MetricSample> Collect() override { throw std::runtime_error("boom"); }
};

}  // namespace

TEST(CollectorPipelineTest, EmitsOneAggregatedSamplePerTick) {
  CollectorPipeline pipeline;
  auto fake = std::make_unique<FakeCollector>();
  auto* fake_ptr = fake.get();
  pipeline.Register(std::move(fake));

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<TelemetrySample> samples;
  pipeline.Start(std::chrono::milliseconds(20),
                 [&](const TelemetrySample& sample) {
                   std::lock_guard lock(mutex);
                   samples.push_back(sample);
                   cv.notify_one();
                 });

  {
    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(2),
                            [&] { return samples.size() >= 3; }));
  }
  pipeline.Stop();

  ASSERT_GE(samples.size(), 3u);
  for (std::size_t i = 1; i < samples.size(); ++i) {
    EXPECT_GE(samples[i].timestamp_ms, samples[i - 1].timestamp_ms);
  }
  EXPECT_EQ(samples[0].metrics.size(), 1u);
  EXPECT_EQ(samples[0].metrics[0].metric_id, "fake.value");
  EXPECT_GT(fake_ptr->calls(), 0);
}

TEST(CollectorPipelineTest, UnavailableCollectorsAreSkipped) {
  CollectorPipeline pipeline;
  pipeline.Register(std::make_unique<FakeCollector>(/*available=*/false));

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<TelemetrySample> samples;
  pipeline.Start(std::chrono::milliseconds(20),
                 [&](const TelemetrySample& sample) {
                   std::lock_guard lock(mutex);
                   samples.push_back(sample);
                   cv.notify_one();
                 });
  {
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(300), [&] { return !samples.empty(); });
  }
  pipeline.Stop();
  ASSERT_FALSE(samples.empty());
  EXPECT_TRUE(samples[0].metrics.empty());
}

TEST(CollectorPipelineTest, ThrowingCollectorDoesNotKillTheTick) {
  CollectorPipeline pipeline;
  pipeline.Register(std::make_unique<ThrowingCollector>());
  pipeline.Register(std::make_unique<FakeCollector>());

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<TelemetrySample> samples;
  pipeline.Start(std::chrono::milliseconds(20),
                 [&](const TelemetrySample& sample) {
                   std::lock_guard lock(mutex);
                   samples.push_back(sample);
                   cv.notify_one();
                 });
  {
    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(2),
                            [&] { return samples.size() >= 2; }));
  }
  pipeline.Stop();
  EXPECT_EQ(samples[0].metrics.size(), 1u);  // fake survived the thrower
}
