#pragma once

#include <functional>
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
  void ReportState(DWORD state, DWORD wait_hint_ms);

  static ServiceController* instance_;

  std::string name_;
  Handler on_start_;
  Handler on_stop_;
  Handler on_pause_;
  Handler on_continue_;
  SERVICE_STATUS_HANDLE status_handle_ = nullptr;
  SERVICE_STATUS status_ = {};
  HANDLE exit_event_ = nullptr;
};

}  // namespace hwpanel::service
