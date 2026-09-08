#include "plugins/collectors/pipeline.h"

#include <chrono>

#include "hwpanel/core/log.h"

namespace hwpanel::collectors {

namespace {
int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
}  // namespace

CollectorPipeline::~CollectorPipeline() { Stop(); }

void CollectorPipeline::Register(std::unique_ptr<ICollector> collector) {
  std::lock_guard lock(mutex_);
  collectors_.push_back(std::move(collector));
}

void CollectorPipeline::Start(std::chrono::milliseconds interval, TickCallback callback) {
  if (running_.exchange(true)) {
    return;
  }
  thread_ = std::thread([this, interval, callback = std::move(callback)] {
    Loop(interval, callback);
  });
}

void CollectorPipeline::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

std::size_t CollectorPipeline::CollectorCount() const {
  std::lock_guard lock(mutex_);
  return collectors_.size();
}

void CollectorPipeline::Loop(std::chrono::milliseconds interval,
                             const TickCallback& callback) {
  while (running_.load()) {
    const auto tick_start = std::chrono::steady_clock::now();

    core::TelemetrySample sample;
    sample.timestamp_ms = NowMs();
    {
      std::lock_guard lock(mutex_);
      for (auto& collector : collectors_) {
        if (!collector->Available()) {
          continue;
        }
        try {
          auto metrics = collector->Collect();
          sample.metrics.insert(sample.metrics.end(),
                                std::make_move_iterator(metrics.begin()),
                                std::make_move_iterator(metrics.end()));
        } catch (const std::exception& e) {
          HWLOG_WARN("collector {} threw: {}", collector->Id(), e.what());
        }
      }
    }

    if (callback) {
      callback(sample);
    }

    // Sleep the remainder of the tick so sampling stays at |interval|.
    const auto elapsed = std::chrono::steady_clock::now() - tick_start;
    const auto remaining = interval - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    if (remaining > std::chrono::milliseconds::zero()) {
      // Coarse slices keep Stop() responsive; a short spin tail removes the
      // final sleep-quantum overshoot (Windows timer resolution) so the tick
      // period tracks |interval| and the push rate holds at ~1 Hz.
      const auto deadline = tick_start + interval;
      while (running_.load()) {
        const auto rem = deadline - std::chrono::steady_clock::now();
        if (rem <= std::chrono::milliseconds::zero()) {
          break;
        }
        if (rem > std::chrono::milliseconds(30)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
        } else {
          std::this_thread::yield();
        }
      }
    }
  }
}

}  // namespace hwpanel::collectors
