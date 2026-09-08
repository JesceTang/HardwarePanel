#include "plugins/collectors/pdh_collector.h"

#include <map>
#include <unordered_map>
#include <cwchar>
#include <cwctype>

#include <windows.h>

#include "hwpanel/core/log.h"

namespace hwpanel::collectors {

namespace {

struct PathParts {
  std::wstring object;
  std::wstring instance;  // empty when absent, wildcards kept as-is
  std::wstring counter;
};

bool ParsePath(const std::wstring& path, PathParts& out) {
  // Expected: \<object>[(<instance>)]\<counter>
  if (path.size() < 3 || path[0] != L'\\') {
    return false;
  }
  const std::size_t open = path.find(L'(', 1);
  const std::size_t last_bs = path.rfind(L'\\');
  if (last_bs == std::wstring::npos || last_bs == 0) {
    return false;
  }
  if (open != std::wstring::npos && open < last_bs) {
    const std::size_t close = path.find(L')', open);
    if (close == std::wstring::npos || close > last_bs) {
      return false;
    }
    out.object = path.substr(1, open - 1);
    out.instance = path.substr(open + 1, close - open - 1);
  } else {
    out.object = path.substr(1, last_bs - 1);
  }
  out.counter = path.substr(last_bs + 1);
  return !out.object.empty() && !out.counter.empty();
}

// Language-neutral counter resolution (plan risk #2): the counter *index* is
// identical on every locale, so read the English (009) name->index table from
// the registry; PdhLookupPerfNameByIndexW then yields the localized name.
bool EnglishNameToIndex(const std::wstring& english_name, DWORD& index_out) {
  HKEY key = nullptr;
  const LONG open = ::RegOpenKeyExW(
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009", 0,
      KEY_READ | KEY_WOW64_64KEY, &key);
  if (open != ERROR_SUCCESS) {
    return false;
  }
  DWORD type = 0;
  DWORD bytes = 0;
  LONG rc = ::RegQueryValueExW(key, L"Counter", nullptr, &type, nullptr, &bytes);
  if (rc != ERROR_SUCCESS || type != REG_MULTI_SZ || bytes < 2 * sizeof(wchar_t)) {
    ::RegCloseKey(key);
    return false;
  }
  std::wstring buf(bytes / sizeof(wchar_t), L'\0');
  rc = ::RegQueryValueExW(key, L"Counter", nullptr, &type,
                          reinterpret_cast<LPBYTE>(buf.data()), &bytes);
  ::RegCloseKey(key);
  if (rc != ERROR_SUCCESS) {
    return false;
  }
  const wchar_t* p = buf.c_str();
  const wchar_t* end = p + (bytes / sizeof(wchar_t));
  while (p < end && *p != L'\0') {
    const std::wstring idx_str(p);
    p += idx_str.size() + 1;
    if (p >= end || *p == L'\0') {
      break;
    }
    const std::wstring name(p);
    p += name.size() + 1;
    if (name.size() == english_name.size()) {
      bool same = true;
      for (std::size_t i = 0; i < name.size(); ++i) {
        if (std::towlower(name[i]) != std::towlower(english_name[i])) {
          same = false;
          break;
        }
      }
      if (same) {
        index_out = static_cast<DWORD>(std::wcstoul(idx_str.c_str(), nullptr, 10));
        return true;
      }
    }
  }
  return false;
}

}  // namespace

std::wstring PdhCollector::LocalizedName(const std::wstring& english_name) {
  DWORD index = 0;
  if (!EnglishNameToIndex(english_name, index)) {
    return english_name;  // best effort
  }
  DWORD size = 0;
  PdhLookupPerfNameByIndexW(nullptr, index, nullptr, &size);
  if (size == 0) {
    return english_name;
  }
  std::wstring name(size, L'\0');
  if (PdhLookupPerfNameByIndexW(nullptr, index, name.data(), &size) != ERROR_SUCCESS) {
    return english_name;
  }
  name.resize(size - 1);  // strip terminator
  return name;
}

std::wstring PdhCollector::LocalizePath(const std::wstring& english_path) {
  PathParts parts;
  if (!ParsePath(english_path, parts)) {
    return english_path;
  }
  std::wstring localized = L"\\" + LocalizedName(parts.object);
  if (!parts.instance.empty()) {
    localized += L"(" + parts.instance + L")";
  }
  localized += L"\\" + LocalizedName(parts.counter);
  return localized;
}

bool PdhCollector::AddCounter(const std::wstring& english_path, std::size_t spec_index) {
  const std::wstring localized = LocalizePath(english_path);
  if (localized.find(L'*') != std::wstring::npos) {
    DWORD list_size = 0;
    if (PdhExpandWildCardPathW(nullptr, localized.c_str(), nullptr, &list_size,
                               PDH_PATH_WBEM_RESULT) != PDH_MORE_DATA ||
        list_size == 0) {
      return false;
    }
    std::wstring buffer(list_size, L'\0');
    if (PdhExpandWildCardPathW(nullptr, localized.c_str(), buffer.data(), &list_size,
                               PDH_PATH_WBEM_RESULT) != ERROR_SUCCESS) {
      return false;
    }
    bool added = false;
    for (const wchar_t* p = buffer.c_str(); *p != L'\0';) {
      const std::wstring expanded(p);
      HCOUNTER handle = nullptr;
      if (PdhAddCounterW(query_, expanded.c_str(), 0, &handle) == ERROR_SUCCESS) {
        counters_.push_back({handle, spec_index});
        added = true;
      }
      p += expanded.size() + 1;
    }
    return added;
  }
  HCOUNTER handle = nullptr;
  if (PdhAddCounterW(query_, localized.c_str(), 0, &handle) != ERROR_SUCCESS) {
    return false;
  }
  counters_.push_back({handle, spec_index});
  return true;
}

PdhCollector::PdhCollector() {
  if (PdhOpenQueryW(nullptr, 0, &query_) != ERROR_SUCCESS) {
    HWLOG_WARN("pdh: PdhOpenQuery failed");
    return;
  }
  specs_ = {
      {"cpu.usage_pct", "percent", 1.0, false},
      {"cpu.freq_mhz", "mhz", 1.0, false},
      {"mem.available_mb", "mb", 1.0, false},
      {"disk.read_bps", "bps", 1.0, false},
      {"disk.write_bps", "bps", 1.0, false},
      {"gpu.util_pct", "percent", 1.0, true},
      {"gpu.mem_used_mb", "mb", 1.0 / (1024.0 * 1024.0), true},
  };
  const std::wstring paths[] = {
      L"\\Processor(_Total)\\% Processor Time",
      L"\\Processor Information(_Total)\\Processor Frequency",
      L"\\Memory\\Available MBytes",
      L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec",
      L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec",
      L"\\GPU Engine(*engtype_3D)\\Utilization Percentage",
      L"\\GPU Adapter Memory(*)\\Dedicated Usage",
  };
  for (std::size_t i = 0; i < specs_.size(); ++i) {
    if (!AddCounter(paths[i], i)) {
      HWLOG_WARN("pdh: counter unavailable: {}", std::string(paths[i].begin(), paths[i].end()));
    }
  }
  ready_ = !counters_.empty();
  if (ready_) {
    // Prime the query so the first Collect() returns valid data.
    PdhCollectQueryData(query_);
  }
}

PdhCollector::~PdhCollector() {
  if (query_ != nullptr) {
    PdhCloseQuery(query_);
  }
}

std::vector<core::MetricSample> PdhCollector::Collect() {
  std::vector<core::MetricSample> out;
  if (!ready_ || PdhCollectQueryData(query_) != ERROR_SUCCESS) {
    return out;
  }

  std::unordered_map<std::size_t, double> sums;
  for (const auto& counter : counters_) {
    PDH_FMT_COUNTERVALUE value = {};
    if (PdhGetFormattedCounterValue(counter.handle, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
                                    nullptr, &value) != ERROR_SUCCESS ||
        value.CStatus != PDH_CSTATUS_VALID_DATA) {
      continue;
    }
    sums[counter.spec_index] += value.doubleValue;
  }

  double available_mb = 0.0;
  bool have_available = false;
  for (std::size_t i = 0; i < specs_.size(); ++i) {
    const auto it = sums.find(i);
    if (it == sums.end()) {
      continue;
    }
    const double scaled = it->second * specs_[i].scale;
    if (specs_[i].metric_id == "mem.available_mb") {
      available_mb = scaled;
      have_available = true;
    }
    core::MetricSample sample;
    sample.metric_id = specs_[i].metric_id;
    sample.unit = specs_[i].unit;
    sample.value = scaled;
    out.push_back(std::move(sample));
  }

  if (have_available) {
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
      const double total_mb = static_cast<double>(status.ullTotalPhys) / (1024.0 * 1024.0);
      core::MetricSample used;
      used.metric_id = "mem.used_mb";
      used.unit = "mb";
      used.value = total_mb - available_mb;
      out.push_back(std::move(used));
      core::MetricSample total;
      total.metric_id = "mem.total_mb";
      total.unit = "mb";
      total.value = total_mb;
      out.push_back(std::move(total));
    }
  }
  return out;
}

}  // namespace hwpanel::collectors
