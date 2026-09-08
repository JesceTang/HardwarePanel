#include "plugins/executors/brightness_executor.h"

#include <string>
#include <vector>

#include <windows.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

namespace hwpanel::executors {

namespace {

struct PhysicalMonitors {
  std::vector<PHYSICAL_MONITOR> monitors;
  ~PhysicalMonitors() {
    for (auto& m : monitors) {
      DestroyPhysicalMonitor(m.hPhysicalMonitor);
    }
  }
};

BOOL CALLBACK EnumMonitorCallback(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
  auto* out = reinterpret_cast<PhysicalMonitors*>(data);
  DWORD count = 0;
  if (!GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count) || count == 0) {
    return TRUE;
  }
  std::vector<PHYSICAL_MONITOR> batch(count);
  if (GetPhysicalMonitorsFromHMONITOR(monitor, count, batch.data())) {
    for (auto& m : batch) {
      out->monitors.push_back(m);
    }
  }
  return TRUE;
}

PhysicalMonitors Enumerate() {
  PhysicalMonitors result;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorCallback,
                      reinterpret_cast<LPARAM>(&result));
  return result;
}

}  // namespace

bool BrightnessExecutor::Available() const {
  auto monitors = Enumerate();
  for (auto& m : monitors.monitors) {
    DWORD min = 0, cur = 0, max = 0;
    if (GetMonitorBrightness(m.hPhysicalMonitor, &min, &cur, &max)) {
      return true;
    }
  }
  return false;
}

std::string BrightnessExecutor::CurrentValue() {
  auto monitors = Enumerate();
  for (auto& m : monitors.monitors) {
    DWORD min = 0, cur = 0, max = 0;
    if (GetMonitorBrightness(m.hPhysicalMonitor, &min, &cur, &max)) {
      return std::to_string(cur);
    }
  }
  return "";
}

ApplyResult BrightnessExecutor::Apply(const std::string& desired) {
  const int value = std::atoi(desired.c_str());
  if (value < 0 || value > 100) {
    return {false, "brightness out of range: " + desired};
  }
  auto monitors = Enumerate();
  int applied = 0;
  for (auto& m : monitors.monitors) {
    if (SetMonitorBrightness(m.hPhysicalMonitor, static_cast<DWORD>(value))) {
      ++applied;
    }
  }
  if (applied == 0) {
    return {false, "no monitor accepted DDC/CI brightness"};
  }
  return {true, "brightness -> " + desired + " on " + std::to_string(applied) + " monitor(s)"};
}

}  // namespace hwpanel::executors
