#pragma once

#include <atomic>

#include "plugins/collectors/collector.h"

namespace hwpanel::collectors {

// WMI collector: CPU/package temperature via MSAcpi_ThermalZoneTemperature.
// Many modern platforms expose no readable thermal zone; after repeated
// failures the collector disables itself instead of hammering WMI (graceful
// degradation, plan M2).
class WmiCollector : public ICollector {
 public:
  WmiCollector() = default;
  ~WmiCollector() override = default;

  std::string_view Id() const override { return "wmi"; }
  bool Available() const override { return !disabled_.load(); }
  std::vector<core::MetricSample> Collect() override;

 private:
  static constexpr int kMaxConsecutiveFailures = 10;
  std::atomic<bool> disabled_{false};
  int consecutive_failures_ = 0;
};

}  // namespace hwpanel::collectors
