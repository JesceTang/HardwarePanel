#include "plugins/collectors/factory.h"

#include <memory>

#include "plugins/collectors/nvml_collector.h"
#include "plugins/collectors/pdh_collector.h"
#include "plugins/collectors/wmi_collector.h"

namespace hwpanel::collectors {

std::unique_ptr<ICollector> MakePdhCollector() {
  return std::make_unique<PdhCollector>();
}

std::unique_ptr<ICollector> MakeWmiCollector() {
  return std::make_unique<WmiCollector>();
}

std::unique_ptr<ICollector> MakeNvmlCollector() {
  return std::make_unique<NvmlCollector>();
}

}  // namespace hwpanel::collectors
