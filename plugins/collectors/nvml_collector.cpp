#include "plugins/collectors/nvml_collector.h"

#include "hwpanel/core/log.h"

namespace hwpanel::collectors {

namespace {
constexpr int kNvmlTemperatureGpu = 0;
constexpr int kNvmlClockGraphics = 0;
constexpr int kNvmlSuccess = 0;
}  // namespace

NvmlCollector::NvmlCollector() {
  module_ = ::LoadLibraryW(L"nvml.dll");
  if (module_ == nullptr) {
    return;  // no NVIDIA driver present
  }
  init_ = reinterpret_cast<FnInit>(::GetProcAddress(module_, "nvmlInit_v2"));
  shutdown_ = reinterpret_cast<FnShutdown>(::GetProcAddress(module_, "nvmlShutdown"));
  get_count_ = reinterpret_cast<FnGetCount>(::GetProcAddress(module_, "nvmlDeviceGetCount_v2"));
  get_handle_ =
      reinterpret_cast<FnGetHandle>(::GetProcAddress(module_, "nvmlDeviceGetHandleByIndex_v2"));
  get_utilization_ =
      reinterpret_cast<FnGetUtilization>(::GetProcAddress(module_, "nvmlDeviceGetUtilizationRates"));
  get_temperature_ =
      reinterpret_cast<FnGetTemperature>(::GetProcAddress(module_, "nvmlDeviceGetTemperature"));
  get_memory_info_ =
      reinterpret_cast<FnGetMemoryInfo>(::GetProcAddress(module_, "nvmlDeviceGetMemoryInfo"));
  get_power_usage_ =
      reinterpret_cast<FnGetPowerUsage>(::GetProcAddress(module_, "nvmlDeviceGetPowerUsage"));
  get_clock_info_ =
      reinterpret_cast<FnGetClockInfo>(::GetProcAddress(module_, "nvmlDeviceGetClockInfo"));
  if (init_ == nullptr || get_count_ == nullptr || get_handle_ == nullptr ||
      init_() != kNvmlSuccess) {
    HWLOG_WARN("nvml: present but init failed");
    return;
  }
  ready_ = true;
}

NvmlCollector::~NvmlCollector() {
  if (ready_ && shutdown_ != nullptr) {
    shutdown_();
  }
  if (module_ != nullptr) {
    ::FreeLibrary(module_);
  }
}

std::vector<core::MetricSample> NvmlCollector::Collect() {
  std::vector<core::MetricSample> out;
  if (!ready_) {
    return out;
  }
  unsigned int count = 0;
  if (get_count_(&count) != kNvmlSuccess) {
    return out;
  }
  for (unsigned int i = 0; i < count; ++i) {
    NvmlDevice device = nullptr;
    if (get_handle_(i, &device) != kNvmlSuccess || device == nullptr) {
      continue;
    }
    const std::string gpu_label = std::to_string(i);

    if (get_utilization_ != nullptr) {
      NvmlUtilization utilization = {};
      if (get_utilization_(device, &utilization) == kNvmlSuccess) {
        out.push_back({"gpu.util_pct", static_cast<double>(utilization.gpu), "percent"});
        out.back().metric_id = "gpu.util_pct";
        out.back().unit = "percent";
      }
    }
    if (get_memory_info_ != nullptr) {
      NvmlMemory memory = {};
      if (get_memory_info_(device, &memory) == kNvmlSuccess) {
        out.push_back({"gpu.mem_used_mb",
                       static_cast<double>(memory.used) / (1024.0 * 1024.0), "mb"});
        out.push_back({"gpu.mem_total_mb",
                       static_cast<double>(memory.total) / (1024.0 * 1024.0), "mb"});
      }
    }
    if (get_temperature_ != nullptr) {
      unsigned int temp = 0;
      if (get_temperature_(device, kNvmlTemperatureGpu, &temp) == kNvmlSuccess) {
        out.push_back({"gpu.temp_c", static_cast<double>(temp), "celsius"});
      }
    }
    if (get_power_usage_ != nullptr) {
      unsigned int milliwatts = 0;
      if (get_power_usage_(device, &milliwatts) == kNvmlSuccess) {
        out.push_back({"gpu.power_w", static_cast<double>(milliwatts) / 1000.0, "watt"});
      }
    }
    if (get_clock_info_ != nullptr) {
      unsigned int mhz = 0;
      if (get_clock_info_(device, kNvmlClockGraphics, &mhz) == kNvmlSuccess) {
        out.push_back({"gpu.clock_mhz", static_cast<double>(mhz), "mhz"});
      }
    }
    (void)gpu_label;  // single-GPU machines: labels omitted, UI groups by id
  }
  return out;
}

}  // namespace hwpanel::collectors
