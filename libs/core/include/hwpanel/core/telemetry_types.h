#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hwpanel::core {

// In-memory representation of one numeric measurement. Mirrors proto
// hwpanel.v1.MetricSample but stays dependency-free so collectors and tests
// can use it without linking protobuf.
struct MetricSample {
  std::string metric_id;
  double value = 0.0;
  std::string unit;
};

// One sampling tick: everything the collectors produced at timestamp_ms.
struct TelemetrySample {
  int64_t timestamp_ms = 0;
  std::vector<MetricSample> metrics;
};

}  // namespace hwpanel::core
