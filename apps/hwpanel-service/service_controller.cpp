#include "apps/hwpanel-service/service_controller.h"

#include <mutex>
#include <string>

#include "hwpanel/core/log.h"

namespace hwpanel::service {

ServiceController* ServiceController::instance_ = nullptr;

ServiceController::ServiceController(std::string name, Handler on_start, Handler on_stop,
                                     Handler on_pause, Handler on_continue)
    : name_(std::move(name)),
      on_start_(std::move(on_start)),
      on_stop_(std::move(on_stop)),
      on_pause_(std::move(on_pause)),
      on_continue_(std::move(on_continue)) {
  exit_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

void ServiceController::ReportState(DWORD state, DWORD wait_hint_ms) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  ReportStateLocked(state, wait_hint_ms);
}

void ServiceController::ReportStateLocked(DWORD state, DWORD wait_hint_ms) {
  status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  status_.dwCurrentState = state;
  status_.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE |
                               SERVICE_ACCEPT_PRESHUTDOWN;
  status_.dwWin32ExitCode = NO_ERROR;
  status_.dwServiceSpecificExitCode = 0;
  status_.dwCheckPoint = 0;
  status_.dwWaitHint = wait_hint_ms;
  ::SetServiceStatus(status_handle_, &status_);
}

DWORD WINAPI ServiceController::HandlerThunk(DWORD control, DWORD /*event_type*/,
                                             void* /*event_data*/, void* context) {
  auto* self = static_cast<ServiceController*>(context);
  switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_PRESHUTDOWN: {
      std::lock_guard<std::mutex> lock(self->state_mutex_);
      self->stop_requested_ = true;
      if (self->start_done_) {
        self->ReportStateLocked(SERVICE_STOP_PENDING, 5000);
        if (self->on_stop_) {
          self->on_stop_();
        }
      }
      // If startup is still running, FinalizeStart performs the stop; either way
      // signal ServiceMain to fall through to SERVICE_STOPPED.
      ::SetEvent(self->exit_event_);
      return NO_ERROR;
    }
    case SERVICE_CONTROL_PAUSE: {
      std::lock_guard<std::mutex> lock(self->state_mutex_);
      self->pause_requested_ = true;
      if (!self->start_done_) {
        // Startup in progress: FinalizeStart applies the pause so the post-start
        // SERVICE_RUNNING report can never clobber it.
        return NO_ERROR;
      }
      self->ReportStateLocked(SERVICE_PAUSE_PENDING, 5000);
      if (self->on_pause_) {
        self->on_pause_();
      }
      self->ReportStateLocked(SERVICE_PAUSED, 0);
      return NO_ERROR;
    }
    case SERVICE_CONTROL_CONTINUE: {
      std::lock_guard<std::mutex> lock(self->state_mutex_);
      self->pause_requested_ = false;
      if (!self->start_done_) {
        return NO_ERROR;  // FinalizeStart reports SERVICE_RUNNING.
      }
      self->ReportStateLocked(SERVICE_CONTINUE_PENDING, 5000);
      if (self->on_continue_) {
        self->on_continue_();
      }
      self->ReportStateLocked(SERVICE_RUNNING, 0);
      return NO_ERROR;
    }
    case SERVICE_CONTROL_INTERROGATE:
      return NO_ERROR;
    default:
      return ERROR_CALL_NOT_IMPLEMENTED;
  }
}

void ServiceController::FinalizeStart() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  start_done_ = true;
  if (stop_requested_) {
    ReportStateLocked(SERVICE_STOP_PENDING, 5000);
    if (on_stop_) {
      on_stop_();
    }
    // exit_event_ already signaled by the STOP handler; ServiceMain reports STOPPED.
  } else if (pause_requested_) {
    ReportStateLocked(SERVICE_PAUSE_PENDING, 5000);
    if (on_pause_) {
      on_pause_();
    }
    ReportStateLocked(SERVICE_PAUSED, 0);
  } else {
    ReportStateLocked(SERVICE_RUNNING, 0);
  }
}

void WINAPI ServiceController::ServiceMainThunk(DWORD /*argc*/, LPWSTR* /*argv*/) {
  auto* self = instance_;
  if (self == nullptr) {
    return;
  }
  self->status_handle_ = ::RegisterServiceCtrlHandlerExW(
      std::wstring(self->name_.begin(), self->name_.end()).c_str(), HandlerThunk, self);
  if (self->status_handle_ == nullptr) {
    return;
  }
  self->ReportState(SERVICE_START_PENDING, 10000);
  if (self->on_start_) {
    self->on_start_();
  }
  self->FinalizeStart();
  ::WaitForSingleObject(self->exit_event_, INFINITE);
  self->ReportState(SERVICE_STOPPED, 0);
}

bool ServiceController::Run() {
  instance_ = this;
  std::wstring wide_name(name_.begin(), name_.end());
  SERVICE_TABLE_ENTRYW table[] = {{wide_name.data(), ServiceMainThunk},
                                  {nullptr, nullptr}};
  const BOOL ok = ::StartServiceCtrlDispatcherW(table);
  instance_ = nullptr;
  if (!ok) {
    HWLOG_WARN("SCM dispatch failed, rc={}", ::GetLastError());
  }
  return ok != FALSE;
}

}  // namespace hwpanel::service
