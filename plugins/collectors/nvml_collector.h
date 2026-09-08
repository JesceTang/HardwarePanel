#pragma once

#include <windows.h>

#include "plugins/collectors/collector.h"

namespace hwpanel::collectors {

// NVIDIA NVML collector, loaded dynamically from the driver-installed
// nvml.dll (never redistributed, plan risk #3). Degrades to Available()==false
// on machines without an NVIDIA GPU.
class NvmlCollector : public ICollector {
 public:
  NvmlCollector();
  ~NvmlCollector() override;

  NvmlCollector(const NvmlCollector&) = delete;
  NvmlCollector& operator=(const NvmlCollector&) = delete;

  std::string_view Id() const override { return "nvml"; }
  bool Available() const override { return ready_; }
  std::vector<core::MetricSample> Collect() override;

 private:
  using NvmlReturn = int;
  using NvmlDevice = void*;
  struct NvmlUtilization {
    unsigned int gpu;
    unsigned int memory;
  };
  struct NvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
  };

  using FnInit = NvmlReturn (*)();
  using FnShutdown = NvmlReturn (*)();
  using FnGetCount = NvmlReturn (*)(unsigned int*);
  using FnGetHandle = NvmlReturn (*)(unsigned int, NvmlDevice*);
  using FnGetUtilization = NvmlReturn (*)(NvmlDevice, NvmlUtilization*);
  using FnGetTemperature = NvmlReturn (*)(NvmlDevice, int, unsigned int*);
  using FnGetMemoryInfo = NvmlReturn (*)(NvmlDevice, NvmlMemory*);
  using FnGetPowerUsage = NvmlReturn (*)(NvmlDevice, unsigned int*);
  using FnGetClockInfo = NvmlReturn (*)(NvmlDevice, int, unsigned int*);

  HMODULE module_ = nullptr;
  FnInit init_ = nullptr;
  FnShutdown shutdown_ = nullptr;
  FnGetCount get_count_ = nullptr;
  FnGetHandle get_handle_ = nullptr;
  FnGetUtilization get_utilization_ = nullptr;
  FnGetTemperature get_temperature_ = nullptr;
  FnGetMemoryInfo get_memory_info_ = nullptr;
  FnGetPowerUsage get_power_usage_ = nullptr;
  FnGetClockInfo get_clock_info_ = nullptr;
  bool ready_ = false;
};

}  // namespace hwpanel::collectors
