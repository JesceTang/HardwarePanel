#pragma once

#include "plugins/executors/executor.h"

namespace hwpanel::executors {

// Windows power plan executor. Accepted values: "balanced", "high", "saver"
// or a raw scheme GUID.
class PowerPlanExecutor : public IExecutor {
 public:
  std::string_view Key() const override { return "power.plan"; }
  bool Available() const override { return true; }
  std::string CurrentValue() override;
  ApplyResult Apply(const std::string& desired) override;
};

}  // namespace hwpanel::executors
