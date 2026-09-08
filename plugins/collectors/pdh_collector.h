#pragma once

#include <string>
#include <vector>

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <winperf.h>

#include "plugins/collectors/collector.h"

namespace hwpanel::collectors {

// PDH performance-counter collector. Counter object/counter names are resolved
// through PdhLookupPerfIndexByEnglishName so the collector is immune to the
// localized (Chinese) counter names on this machine (plan risk #2).
class PdhCollector : public ICollector {
 public:
  PdhCollector();
  ~PdhCollector() override;

  PdhCollector(const PdhCollector&) = delete;
  PdhCollector& operator=(const PdhCollector&) = delete;

  std::string_view Id() const override { return "pdh"; }
  bool Available() const override { return ready_; }
  std::vector<core::MetricSample> Collect() override;

 private:
  struct CounterSpec {
    std::string metric_id;
    std::string unit;
    double scale = 1.0;
    bool sum_instances = false;  // wildcard counters are summed into one metric
  };
  struct Counter {
    HCOUNTER handle = nullptr;
    std::size_t spec_index = 0;
  };

  bool AddCounter(const std::wstring& english_path, std::size_t spec_index);
  static std::wstring LocalizePath(const std::wstring& english_path);
  static std::wstring LocalizedName(const std::wstring& english_name);

  HQUERY query_ = nullptr;
  std::vector<CounterSpec> specs_;
  std::vector<Counter> counters_;
  bool ready_ = false;
};

}  // namespace hwpanel::collectors
