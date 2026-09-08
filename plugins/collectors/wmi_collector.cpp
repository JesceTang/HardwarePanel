#include "plugins/collectors/wmi_collector.h"

#include <comdef.h>
#include <wbemidl.h>

#include "hwpanel/core/log.h"

#pragma comment(lib, "wbemuuid.lib")

namespace hwpanel::collectors {

namespace {

class ComScope {
 public:
  ComScope() { ok_ = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
  ~ComScope() {
    if (ok_) {
      ::CoUninitialize();
    }
  }
  bool ok() const { return ok_; }

 private:
  bool ok_ = false;
};

}  // namespace

std::vector<core::MetricSample> WmiCollector::Collect() {
  std::vector<core::MetricSample> out;
  ComScope com;
  if (!com.ok()) {
    return out;
  }

  IWbemLocator* locator = nullptr;
  HRESULT hr = ::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWbemLocator, reinterpret_cast<void**>(&locator));
  if (FAILED(hr) || locator == nullptr) {
    return out;
  }

  IWbemServices* services = nullptr;
  hr = locator->ConnectServer(_bstr_t(L"root\\WMI"), nullptr, nullptr, nullptr, 0,
                              nullptr, nullptr, &services);
  if (FAILED(hr) || services == nullptr) {
    locator->Release();
    if (++consecutive_failures_ >= kMaxConsecutiveFailures) {
      disabled_ = true;
      HWLOG_WARN("wmi: thermal zone unavailable, collector disabled");
    }
    return out;
  }

  // Impersonation level needed for most WMI namespaces.
  ::CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                      EOAC_NONE);

  IEnumWbemClassObject* enumerator = nullptr;
  hr = services->ExecQuery(
      _bstr_t(L"WQL"),
      _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature WHERE Active = TRUE"),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);

  double sum = 0.0;
  int count = 0;
  if (SUCCEEDED(hr) && enumerator != nullptr) {
    IWbemClassObject* object = nullptr;
    ULONG returned = 0;
    while (SUCCEEDED(enumerator->Next(WBEM_INFINITE, 1, &object, &returned)) &&
           returned != 0) {
      VARIANT variant;
      VariantInit(&variant);
      if (SUCCEEDED(object->Get(L"CurrentTemperature", 0, &variant, nullptr, nullptr)) &&
          variant.vt == VT_I4) {
        sum += static_cast<double>(variant.lVal) / 10.0 - 273.2;  // tenths of Kelvin
        ++count;
      }
      VariantClear(&variant);
      object->Release();
    }
    enumerator->Release();
  }

  if (services != nullptr) {
    services->Release();
  }
  locator->Release();

  if (count == 0) {
    if (++consecutive_failures_ >= kMaxConsecutiveFailures) {
      disabled_ = true;
      HWLOG_WARN("wmi: no readable thermal zone, collector disabled");
    }
    return out;
  }

  consecutive_failures_ = 0;
  core::MetricSample sample;
  sample.metric_id = "cpu.temp_c";
  sample.unit = "celsius";
  sample.value = sum / count;
  out.push_back(std::move(sample));
  return out;
}

}  // namespace hwpanel::collectors
