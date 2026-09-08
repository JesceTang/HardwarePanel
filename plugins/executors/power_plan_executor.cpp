#include "plugins/executors/power_plan_executor.h"

#include <cstring>
#include <map>

#include <windows.h>
#include <powrprof.h>

namespace hwpanel::executors {

namespace {

const GUID kBalanced = {0x381b4222, 0xf694, 0x41f0,
                        {0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e}};
const GUID kHighPerformance = {0x8c5e7fda, 0xe8bf, 0x4a96,
                               {0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c}};
const GUID kPowerSaver = {0xa1841308, 0x3541, 0x4fab,
                          {0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a}};

std::string GuidToString(const GUID& guid) {
  char buf[40] = {};
  snprintf(buf, sizeof(buf),
           "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1,
           guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2],
           guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
  return buf;
}

bool ParseGuid(const std::string& text, GUID& out) {
  unsigned int d1 = 0, d2 = 0, d3 = 0;
  unsigned int d4[8] = {};
  const int n = sscanf_s(text.c_str(), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                         &d1, &d2, &d3, &d4[0], &d4[1], &d4[2], &d4[3], &d4[4], &d4[5],
                         &d4[6], &d4[7]);
  if (n != 11) {
    return false;
  }
  out.Data1 = d1;
  out.Data2 = static_cast<unsigned short>(d2);
  out.Data3 = static_cast<unsigned short>(d3);
  for (int i = 0; i < 8; ++i) {
    out.Data4[i] = static_cast<unsigned char>(d4[i]);
  }
  return true;
}

bool AliasToGuid(const std::string& alias, GUID& out) {
  if (alias == "balanced") {
    out = kBalanced;
    return true;
  }
  if (alias == "high") {
    out = kHighPerformance;
    return true;
  }
  if (alias == "saver") {
    out = kPowerSaver;
    return true;
  }
  return false;
}

std::string GuidToAlias(const GUID& guid) {
  if (std::memcmp(&guid, &kBalanced, sizeof(GUID)) == 0) return "balanced";
  if (std::memcmp(&guid, &kHighPerformance, sizeof(GUID)) == 0) return "high";
  if (std::memcmp(&guid, &kPowerSaver, sizeof(GUID)) == 0) return "saver";
  return GuidToString(guid);
}

}  // namespace

std::string PowerPlanExecutor::CurrentValue() {
  GUID* active = nullptr;
  if (PowerGetActiveScheme(nullptr, &active) != ERROR_SUCCESS || active == nullptr) {
    return "";
  }
  const std::string alias = GuidToAlias(*active);
  LocalFree(active);
  return alias;
}

ApplyResult PowerPlanExecutor::Apply(const std::string& desired) {
  GUID guid = {};
  if (!AliasToGuid(desired, guid) && !ParseGuid(desired, guid)) {
    return {false, "unknown power plan: " + desired};
  }
  const DWORD rc = PowerSetActiveScheme(nullptr, &guid);
  if (rc != ERROR_SUCCESS) {
    return {false, "PowerSetActiveScheme failed rc=" + std::to_string(rc)};
  }
  return {true, "power plan -> " + desired};
}

}  // namespace hwpanel::executors
