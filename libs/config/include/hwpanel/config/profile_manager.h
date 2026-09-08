#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "hwpanel/core/event_bus.h"
#include "hwpanel/core/profile_store.h"
#include "plugins/executors/executor.h"

namespace hwpanel::config {

// Applies profiles through executor plugins with snapshot/rollback semantics:
// snapshot current values -> apply in order -> on first failure restore the
// snapshot in reverse order and report the rolled-back keys (plan M4).
class ProfileManager {
 public:
  struct SwitchResult {
    bool ok = false;
    std::string message;
    std::vector<std::string> rolled_back_keys;
  };

  ProfileManager(core::ProfileStore store,
                 std::vector<std::unique_ptr<executors::IExecutor>> executors,
                 core::EventBus* events);
  ~ProfileManager();

  ProfileManager(const ProfileManager&) = delete;
  ProfileManager& operator=(const ProfileManager&) = delete;

  std::vector<core::Profile> List() const;
  std::string ActiveId() const;
  SwitchResult SwitchTo(const std::string& profile_id);

  // ReadDirectoryChangesW watch on the profile dir; publishes
  // "profiles-changed" events so clients can hot-reload (plan M4).
  void StartFileWatch();
  void StopFileWatch();

 private:
  executors::IExecutor* FindExecutor(const std::string& key);
  void WatchLoop();

  mutable std::mutex mutex_;
  core::ProfileStore store_;
  std::vector<std::unique_ptr<executors::IExecutor>> executors_;
  core::EventBus* events_;

  std::atomic<bool> watching_{false};
  std::thread watch_thread_;
  HANDLE watch_dir_ = INVALID_HANDLE_VALUE;
  OVERLAPPED overlapped_ = {};
};

}  // namespace hwpanel::config
