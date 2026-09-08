#include "plugins/executors/factory.h"

#include <memory>

#include "plugins/executors/brightness_executor.h"
#include "plugins/executors/power_plan_executor.h"

namespace hwpanel::executors {

std::unique_ptr<IExecutor> MakePowerPlanExecutor() {
  return std::make_unique<PowerPlanExecutor>();
}

std::unique_ptr<IExecutor> MakeBrightnessExecutor() {
  return std::make_unique<BrightnessExecutor>();
}

}  // namespace hwpanel::executors
