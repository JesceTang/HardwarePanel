#include "apps/hwpanel-service/service_host.h"

#include <utility>

#include "apps/hwpanel-service/grpc/command_service_impl.h"
#include "apps/hwpanel-service/grpc/telemetry_service_impl.h"
#include "hwpanel/core/endpoint.h"
#include "hwpanel/core/log.h"
#include "hwpanel/core/paths.h"
#include "plugins/collectors/factory.h"
#include "plugins/executors/factory.h"

namespace hwpanel::service {

namespace {
constexpr auto kSampleInterval = std::chrono::seconds(1);
}

ServiceHost::ServiceHost() = default;

ServiceHost::~ServiceHost() { Stop(); }

std::unique_ptr<grpc::Server> ServiceHost::BuildServer(const std::string& address) {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(telemetry_impl_.get());
  builder.RegisterService(command_impl_.get());
  return builder.BuildAndStart();
}

bool ServiceHost::Start(const Options& options) {
  if (running_.load()) {
    return true;
  }

  core::ProfileStore store(core::paths::EnsureDir(core::paths::ProfilesDir()));
  store.EnsureDefaults();

  std::vector<std::unique_ptr<executors::IExecutor>> executors;
  executors.push_back(executors::MakePowerPlanExecutor());
  executors.push_back(executors::MakeBrightnessExecutor());
  profile_manager_ = std::make_unique<config::ProfileManager>(std::move(store),
                                                              std::move(executors), &events_);

  telemetry_impl_ = std::make_unique<TelemetryServiceImpl>(hub_);
  command_impl_ = std::make_unique<CommandServiceImpl>(
      *profile_manager_, events_, [this] {
        v1::ServiceStatus status;
        status.set_version("0.1.0");
        status.set_state(StateName());
        status.set_uptime_ms(UptimeMs());
        status.set_endpoint(endpoint_);
        return status;
      });

  pipeline_.Register(collectors::MakePdhCollector());
  pipeline_.Register(collectors::MakeWmiCollector());
  pipeline_.Register(collectors::MakeNvmlCollector());
  tick_callback_ = [this](const core::TelemetrySample& sample) { hub_.Publish(sample); };
  pipeline_.Start(kSampleInterval, tick_callback_);
  profile_manager_->StartFileWatch();

  server_ = BuildServer(options.endpoint);
  std::string actual = options.endpoint;
  if (server_ == nullptr && core::IsNamedPipeAddress(options.endpoint)) {
    HWLOG_WARN("named pipe bind failed, falling back to loopback TCP");
    actual = core::TcpAddress();
    server_ = BuildServer(actual);
  }
  if (server_ == nullptr) {
    HWLOG_ERROR("cannot bind endpoint {}", options.endpoint);
    return false;
  }
  endpoint_ = actual;
  started_at_ = std::chrono::steady_clock::now();
  running_ = true;
  HWLOG_INFO("service listening on {}", endpoint_);
  events_.PublishNow("info", "Service started", endpoint_);
  return true;
}

void ServiceHost::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (server_ != nullptr) {
    server_->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(1));
  }
  pipeline_.Stop();
  if (profile_manager_ != nullptr) {
    profile_manager_->StopFileWatch();
  }
  HWLOG_INFO("service stopped");
}

void ServiceHost::PauseCollectors() {
  if (paused_.exchange(true)) {
    return;
  }
  pipeline_.Stop();
  HWLOG_INFO("collectors paused");
}

void ServiceHost::ResumeCollectors() {
  if (!paused_.exchange(false)) {
    return;
  }
  pipeline_.Start(kSampleInterval, tick_callback_);
  HWLOG_INFO("collectors resumed");
}

std::string ServiceHost::StateName() const {
  if (!running_.load()) {
    return "STOPPED";
  }
  return paused_.load() ? "PAUSED" : "RUNNING";
}

int64_t ServiceHost::UptimeMs() const {
  if (!running_.load()) {
    return 0;
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - started_at_)
      .count();
}

}  // namespace hwpanel::service
