// HWPanel service entry point.
//
// Launched by the SCM: full Windows-service lifecycle (start/stop/pause/
// continue, crash recovery configured via `sc failure`).
// Launched manually (or with --console): foreground console mode for dev.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <windows.h>

#include "apps/hwpanel-service/service_controller.h"
#include "apps/hwpanel-service/service_host.h"
#include "apps/hwpanel-service/sinks/eventlog_sink.h"
#include "apps/hwpanel-service/sinks/etw_sink.h"
#include "hwpanel/core/endpoint.h"
#include "hwpanel/core/log.h"
#include "hwpanel/core/paths.h"

namespace {

constexpr const char* kServiceName = "HWPanelService";

void InitLogging() {
  hwpanel::core::log::Init("hwpanel-service",
                           hwpanel::core::paths::DataDir().string());
  hwpanel::core::log::AddSink(
      std::make_shared<hwpanel::service::EventLogSink>("HWPanel"));
  hwpanel::core::log::AddSink(std::make_shared<hwpanel::service::EtwSink>());
}

int RunConsole(const std::string& endpoint) {
  InitLogging();
  HWLOG_INFO("console mode, endpoint {}", endpoint);
  hwpanel::service::ServiceHost host;
  if (!host.Start({endpoint})) {
    return 1;
  }
  const HANDLE stop_event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
  ::SetConsoleCtrlHandler(
      +[](DWORD type) -> BOOL {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
            type == CTRL_CLOSE_EVENT) {
          // The host is stopped from the wait below; signal only.
          return TRUE;
        }
        return FALSE;
      },
      TRUE);
  // Ctrl+C terminates the process; SCM-style graceful stop is exercised in
  // service mode. Keep the main thread parked here.
  ::WaitForSingleObject(stop_event, INFINITE);
  host.Stop();
  return 0;
}

int RunService(const std::string& endpoint) {
  static auto host = std::make_unique<hwpanel::service::ServiceHost>();
  hwpanel::service::ServiceController controller(
      kServiceName,
      [endpoint] {
        InitLogging();
        if (!host->Start({endpoint})) {
          HWLOG_ERROR("service start failed");
        }
      },
      [] { host->Stop(); },
      [] { host->PauseCollectors(); },
      [] { host->ResumeCollectors(); });
  if (controller.Run()) {
    return 0;
  }
  // Not launched by the SCM: fall back to console mode for convenience.
  std::fprintf(stderr, "not started by SCM, falling back to console mode\n");
  return RunConsole(endpoint);
}

}  // namespace

int main(int argc, char** argv) {
  std::string endpoint = hwpanel::core::DefaultEndpoint();
  bool console = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--console") == 0) {
      console = true;
    } else if (std::strcmp(argv[i], "--endpoint") == 0 && i + 1 < argc) {
      endpoint = argv[++i];
    } else {
      std::fprintf(stderr, "usage: hwpanel-service [--console] [--endpoint <addr>]\n");
      return 2;
    }
  }
  return console ? RunConsole(endpoint) : RunService(endpoint);
}
