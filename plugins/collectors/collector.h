#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hwpanel/core/telemetry_types.h"

namespace hwpanel::collectors {

// Collector plugin contract. Implementations read one hardware/OS subsystem
// (PDH, WMI, NVML...) and must degrade gracefully: Available() == false means
// "skip me this tick", never throw out of Collect().
class ICollector {
 public:
  virtual ~ICollector() = default;

  // Stable identifier used in logs, e.g. "pdh", "wmi", "nvml".
  virtual std::string_view Id() const = 0;

  // Cheap probe; called once per tick before Collect().
  virtual bool Available() const = 0;

  // Reads the subsystem and returns zero or more metrics.
  virtual std::vector<core::MetricSample> Collect() = 0;
};

}  // namespace hwpanel::collectors
