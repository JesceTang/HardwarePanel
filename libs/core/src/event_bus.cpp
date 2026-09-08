#include "hwpanel/core/event_bus.h"

#include <chrono>
#include <utility>

namespace hwpanel::core {

bool EventBus::Subscriber::Pop(EventMessage& out, int timeout_ms) {
  std::unique_lock lock(mutex_);
  if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [this] { return closed_ || !queue_.empty(); })) {
    return false;
  }
  if (queue_.empty()) {
    return false;
  }
  out = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

void EventBus::Subscriber::Close() {
  {
    std::lock_guard lock(mutex_);
    closed_ = true;
  }
  cv_.notify_all();
}

void EventBus::Publish(const EventMessage& event) {
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
      sub->queue_.pop_front();
    }
    sub->queue_.push_back(event);
    sub->cv_.notify_one();
  }
}

std::shared_ptr<EventBus::Subscriber> EventBus::Subscribe() {
  auto sub = std::make_shared<Subscriber>();
  std::lock_guard lock(mutex_);
  subscribers_.push_back(sub);
  return sub;
}

void EventBus::PublishNow(const std::string& level, const std::string& title,
                          const std::string& message) {
  EventMessage event;
  event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
  event.level = level;
  event.title = title;
  event.message = message;
  Publish(event);
}

}  // namespace hwpanel::core
