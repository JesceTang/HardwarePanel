#include "hwpanel/core/telemetry_hub.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace hwpanel::core {

bool TelemetryHub::Subscriber::Pop(TelemetrySample& out, int timeout_ms) {
  std::unique_lock lock(mutex_);
  if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [this] { return closed_ || !queue_.empty(); })) {
    return false;
  }
  if (queue_.empty()) {
    return false;  // closed
  }
  out = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

void TelemetryHub::Subscriber::Close() {
  {
    std::lock_guard lock(mutex_);
    closed_ = true;
  }
  cv_.notify_all();
}

std::size_t TelemetryHub::Subscriber::QueueSize() const {
  std::lock_guard lock(mutex_);
  return queue_.size();
}

TelemetryHub::TelemetryHub(std::size_t history_capacity)
    : history_(history_capacity) {}

void TelemetryHub::Publish(const TelemetrySample& sample) {
  history_.Push(sample);

  std::vector<std::shared_ptr<Subscriber>> live;
  {
    std::lock_guard lock(mutex_);
    for (auto it = subscribers_.begin(); it != subscribers_.end();) {
      if (auto sub = it->lock()) {
        live.push_back(std::move(sub));
        ++it;
      } else {
        it = subscribers_.erase(it);
      }
    }
  }
  for (auto& sub : live) {
    std::lock_guard lock(sub->mutex_);
    if (sub->closed_) {
      continue;
    }
    if (sub->queue_.size() >= kSubscriberQueueCapacity) {
      sub->queue_.pop_front();  // slow consumer: drop oldest
    }
    sub->queue_.push_back(sample);
    sub->cv_.notify_one();
  }
}

std::shared_ptr<TelemetryHub::Subscriber> TelemetryHub::Subscribe() {
  auto sub = std::make_shared<Subscriber>();
  std::lock_guard lock(mutex_);
  subscribers_.push_back(sub);
  return sub;
}

std::vector<TelemetrySample> TelemetryHub::History(int seconds) const {
  const int capped = std::clamp(seconds, 0, static_cast<int>(history_.Capacity()));
  return history_.LastN(static_cast<std::size_t>(capped));
}

}  // namespace hwpanel::core
