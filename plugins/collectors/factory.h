#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "plugins/collectors/collector.h"

namespace hwpanel::collectors {

std::unique_ptr<ICollector> MakePdhCollector();
std::unique_ptr<ICollector> MakeWmiCollector();
std::unique_ptr<ICollector> MakeNvmlCollector();

}  // namespace hwpanel::collectors
