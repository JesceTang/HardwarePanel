#pragma once

#include "plugins/executors/executor.h"

namespace hwpanel::executors {

// Monitor brightness executor (DDC/CI physical monitors, dxva2). Degrades to
// Available()==false when no monitor exposes brightness control.
class BrightnessExecutor : public IExecutor {
 public:
  std::string_view Key() const override { return "display.brightness"; }
  bool Available() const override;
  std::string CurrentValue() override;
  ApplyResult Apply(const std::string& desired) override;
};

}  // namespace hwpanel::executors
