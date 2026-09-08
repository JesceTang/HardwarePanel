#pragma once

#include <string>
#include <string_view>

namespace hwpanel::executors {

struct ApplyResult {
  bool ok = false;
  std::string message;
};

// Executor plugin contract: applies one OS-level setting and can read the
// current value back (needed for snapshot/rollback transactions, plan M4).
class IExecutor {
 public:
  virtual ~IExecutor() = default;

  // Setting key used in profiles, e.g. "power.plan".
  virtual std::string_view Key() const = 0;

  virtual bool Available() const = 0;

  // Reads the live OS state; returns "" when unreadable.
  virtual std::string CurrentValue() = 0;

  virtual ApplyResult Apply(const std::string& desired) = 0;
};

}  // namespace hwpanel::executors
