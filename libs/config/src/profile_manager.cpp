#include "hwpanel/config/profile_manager.h"

#include <algorithm>
#include <utility>

#include "hwpanel/core/log.h"

namespace hwpanel::config {

ProfileManager::ProfileManager(core::ProfileStore store,
                               std::vector<std::unique_ptr<executors::IExecutor>> executors,
                               core::EventBus* events)
    : store_(std::move(store)), executors_(std::move(executors)), events_(events) {}

ProfileManager::~ProfileManager() { StopFileWatch(); }

std::vector<core::Profile> ProfileManager::List() const {
  std::lock_guard lock(mutex_);
  return store_.List();
}

std::string ProfileManager::ActiveId() const {
  std::lock_guard lock(mutex_);
  return store_.ActiveId();
}

executors::IExecutor* ProfileManager::FindExecutor(const std::string& key) {
  for (auto& executor : executors_) {
    if (executor->Key() == key) {
      return executor.get();
    }
  }
  return nullptr;
}

ProfileManager::SwitchResult ProfileManager::SwitchTo(const std::string& profile_id) {
  std::lock_guard lock(mutex_);
  core::Profile profile;
  if (!store_.Get(profile_id, profile)) {
    return {false, "profile not found: " + profile_id, {}};
  }

  // Snapshot -> apply -> rollback-on-failure transaction.
  std::vector<std::pair<executors::IExecutor*, std::string>> applied;
  for (const auto& [key, desired] : profile.settings) {
    executors::IExecutor* executor = FindExecutor(key);
    if (executor == nullptr || !executor->Available()) {
      const std::string reason =
          executor == nullptr ? "no executor for key " + key
                              : "executor unavailable: " + key;
      std::vector<std::string> rolled;
      for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
        rolled.push_back(std::string(it->first->Key()));
        it->first->Apply(it->second);
      }
      if (events_ != nullptr) {
        events_->PublishNow("error", "Profile switch failed", reason);
      }
      return {false, reason, rolled};
    }
    const std::string previous = executor->CurrentValue();
    const executors::ApplyResult result = executor->Apply(desired);
    if (!result.ok) {
      std::vector<std::string> rolled;
      for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
        rolled.push_back(std::string(it->first->Key()));
        it->first->Apply(it->second);
      }
      HWLOG_WARN("profile {}: executor {} failed: {}", profile_id, key, result.message);
      if (events_ != nullptr) {
        events_->PublishNow("error", "Profile switch failed",
                            key + ": " + result.message + " (rolled back)");
      }
      return {false, result.message, rolled};
    }
    applied.emplace_back(executor, previous);
  }

  store_.SetActiveId(profile_id);
  HWLOG_INFO("profile applied: {}", profile_id);
  if (events_ != nullptr) {
    events_->PublishNow("info", "Profile applied", profile.name);
  }
  return {true, "ok", {}};
}

void ProfileManager::StartFileWatch() {
  if (watching_.exchange(true)) {
    return;
  }
  watch_dir_ = ::CreateFileW(store_.dir().wstring().c_str(), FILE_LIST_DIRECTORY,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (watch_dir_ == INVALID_HANDLE_VALUE) {
    watching_ = false;
    HWLOG_WARN("profile watch: cannot open dir {}", store_.dir().string());
    return;
  }
  overlapped_.hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
  watch_thread_ = std::thread([this] { WatchLoop(); });
}

void ProfileManager::StopFileWatch() {
  if (!watching_.exchange(false)) {
    return;
  }
  if (overlapped_.hEvent != nullptr) {
    ::CancelIoEx(watch_dir_, &overlapped_);
  }
  if (watch_thread_.joinable()) {
    watch_thread_.join();
  }
  if (watch_dir_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(watch_dir_);
    watch_dir_ = INVALID_HANDLE_VALUE;
  }
  if (overlapped_.hEvent != nullptr) {
    ::CloseHandle(overlapped_.hEvent);
    overlapped_.hEvent = nullptr;
  }
}

void ProfileManager::WatchLoop() {
  std::vector<BYTE> buffer(4096);
  auto arm = [&] {
    ::ResetEvent(overlapped_.hEvent);
    return ::ReadDirectoryChangesW(watch_dir_, buffer.data(),
                                   static_cast<DWORD>(buffer.size()), FALSE,
                                   FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                   nullptr, &overlapped_, nullptr) != FALSE;
  };
  if (!arm()) {
    return;
  }
  while (watching_.load()) {
    const DWORD wait = ::WaitForSingleObject(overlapped_.hEvent, 500);
    if (!watching_.load()) {
      break;
    }
    if (wait == WAIT_OBJECT_0) {
      DWORD bytes = 0;
      ::GetOverlappedResult(watch_dir_, &overlapped_, &bytes, FALSE);
      if (bytes > 0 && events_ != nullptr) {
        events_->PublishNow("info", "Profiles changed", "profile directory updated");
      }
      if (!arm()) {
        break;
      }
    }
  }
}

}  // namespace hwpanel::config
