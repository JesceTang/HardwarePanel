#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "plugins/collectors/collector.h"

namespace hwpanel::collectors {

// Single-threaded tick loop that polls every registered collector once per
// interval and emits one aggregated TelemetrySample per tick
// (collect -> aggregate -> push, plan M2).
class CollectorPipeline {
 public:
  using TickCallback = std::function<void(const core::TelemetrySample&)>;

  CollectorPipeline() = default;
  ~CollectorPipeline();

  CollectorPipeline(const CollectorPipeline&) = delete;
  CollectorPipeline& operator=(const CollectorPipeline&) = delete;

  void Register(std::unique_ptr<ICollector> collector);
  void Start(std::chrono::milliseconds interval, TickCallback callback);
  void Stop();

  std::size_t CollectorCount() const;

 private:
  void Loop(std::chrono::milliseconds interval, const TickCallback& callback);

  mutable std::mutex mutex_;
  std::vector<std::unique_ptr<ICollector>> collectors_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

}  // namespace hwpanel::collectors
