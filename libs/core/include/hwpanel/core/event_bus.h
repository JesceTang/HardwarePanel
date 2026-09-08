#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace hwpanel::core {

// A user-facing notification (profile applied, rollback, collector error...).
struct EventMessage {
  int64_t timestamp_ms = 0;
  std::string level;   // info / warn / error
  std::string title;
  std::string message;
};

// Publish/subscribe bus for EventMessage. Mirrors TelemetryHub's subscriber
// semantics: bounded per-subscriber queue, slow consumers drop oldest.
class EventBus {
 public:
  static constexpr std::size_t kSubscriberQueueCapacity = 32;

  class Subscriber {
   public:
    bool Pop(EventMessage& out, int timeout_ms);
    void Close();

   private:
    friend class EventBus;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<EventMessage> queue_;
    bool closed_ = false;
  };

  void Publish(const EventMessage& event);
  std::shared_ptr<Subscriber> Subscribe();

  // Convenience: publishes with the current wall-clock timestamp.
  void PublishNow(const std::string& level, const std::string& title,
                  const std::string& message);

 private:
  std::mutex mutex_;
  std::vector<std::weak_ptr<Subscriber>> subscribers_;
};

}  // namespace hwpanel::core
