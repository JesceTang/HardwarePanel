#pragma once

#include <memory>

#include "plugins/executors/executor.h"

namespace hwpanel::executors {

std::unique_ptr<IExecutor> MakePowerPlanExecutor();
std::unique_ptr<IExecutor> MakeBrightnessExecutor();

}  // namespace hwpanel::executors
