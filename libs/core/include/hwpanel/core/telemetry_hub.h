#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "hwpanel/core/ring_buffer.h"
#include "hwpanel/core/telemetry_types.h"

namespace hwpanel::core {

// Fan-out point of the telemetry pipeline: the collector pipeline publishes
// 1 Hz samples here; the ring buffer keeps history for GetHistory while every
// live gRPC subscriber gets its own bounded queue (slow consumers drop their
// oldest samples instead of blocking producers).
class TelemetryHub {
 public:
  static constexpr std::size_t kDefaultHistoryCapacity = 300;  // 5 min @ 1 Hz
  static constexpr std::size_t kSubscriberQueueCapacity = 64;

  class Subscriber {
   public:
    // Blocks up to |timeout_ms| for the next sample. Returns false on timeout
    // or when the subscriber was closed.
    bool Pop(TelemetrySample& out, int timeout_ms);
    void Close();
    std::size_t QueueSize() const;

   private:
    friend class TelemetryHub;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<TelemetrySample> queue_;
    bool closed_ = false;
  };

  explicit TelemetryHub(std::size_t history_capacity = kDefaultHistoryCapacity);

  void Publish(const TelemetrySample& sample);
  std::shared_ptr<Subscriber> Subscribe();
  std::vector<TelemetrySample> History(int seconds) const;
  std::size_t HistorySize() const { return history_.Size(); }

 private:
  RingBuffer<TelemetrySample> history_;
  mutable std::mutex mutex_;
  std::vector<std::weak_ptr<Subscriber>> subscribers_;
};

}  // namespace hwpanel::core
