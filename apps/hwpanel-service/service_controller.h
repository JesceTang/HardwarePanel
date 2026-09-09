#pragma once

#include <functional>
#include <mutex>
#include <string>

#include <windows.h>

namespace hwpanel::service {

// SCM (Service Control Manager) integration: dispatches ServiceMain, reports
// state transitions and routes STOP/PAUSE/CONTINUE controls to callbacks
// (plan M3). Crash recovery itself is configured externally via
// `sc failure` (installer + scripts/install-service.ps1).
class ServiceController {
 public:
  using Handler = std::function<void()>;

  ServiceController(std::string name, Handler on_start, Handler on_stop,
                    Handler on_pause, Handler on_continue);

  // Blocks inside StartServiceCtrlDispatcher; returns false when the process
  // was not launched by the SCM (e.g. double click / console).
  bool Run();

  const std::string& name() const { return name_; }

 private:
  static void WINAPI ServiceMainThunk(DWORD argc, LPWSTR* argv);
  static DWORD WINAPI HandlerThunk(DWORD control, DWORD event_type, void* event_data,
                                   void* context);
  // ReportState acquires state_mutex_; ReportStateLocked assumes it is held.
  void ReportState(DWORD state, DWORD wait_hint_ms);
  void ReportStateLocked(DWORD state, DWORD wait_hint_ms);
  // Reconciles the post-startup state with any STOP/PAUSE control that arrived
  // while on_start_ was still running. Without this, ServiceMain would report
  // SERVICE_RUNNING unconditionally after a slow start and clobber a PAUSED
  // state the control handler had already set (plan M3 pause/continue).
  void FinalizeStart();

  static ServiceController* instance_;

  std::string name_;
  Handler on_start_;
  Handler on_stop_;
  Handler on_pause_;
  Handler on_continue_;
  SERVICE_STATUS_HANDLE status_handle_ = nullptr;
  SERVICE_STATUS status_ = {};
  HANDLE exit_event_ = nullptr;

  // Serializes SCM state transitions between ServiceMain and the control
  // handler thread. start_done_ flips true once on_start_ has returned.
  std::mutex state_mutex_;
  bool start_done_ = false;
  bool pause_requested_ = false;
  bool stop_requested_ = false;
};

}  // namespace hwpanel::service
