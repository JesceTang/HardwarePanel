#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "hwpanel/config/profile_manager.h"
#include "hwpanel/core/event_bus.h"
#include "hwpanel/core/telemetry_hub.h"
#include "plugins/collectors/pipeline.h"

namespace hwpanel::service {

class TelemetryServiceImpl;
class CommandServiceImpl;

// Composition root of the service process: owns the telemetry hub, event bus,
// profile manager, collector pipeline and the gRPC server (named pipe with a
// loopback-TCP fallback, plan decision #3).
class ServiceHost {
 public:
  struct Options {
    std::string endpoint;
  };

  ServiceHost();
  ~ServiceHost();

  ServiceHost(const ServiceHost&) = delete;
  ServiceHost& operator=(const ServiceHost&) = delete;

  bool Start(const Options& options);
  void Stop();

  // SCM pause/continue: freezes the collector pipeline but keeps the gRPC
  // server answering (clients see stale-but-valid history).
  void PauseCollectors();
  void ResumeCollectors();

  bool running() const { return running_.load(); }
  bool paused() const { return paused_.load(); }
  std::string StateName() const;
  int64_t UptimeMs() const;
  const std::string& endpoint() const { return endpoint_; }

  core::TelemetryHub& hub() { return hub_; }
  core::EventBus& events() { return events_; }
  config::ProfileManager& profiles() { return *profile_manager_; }

 private:
  std::unique_ptr<grpc::Server> BuildServer(const std::string& address);

  core::TelemetryHub hub_;
  core::EventBus events_;
  std::unique_ptr<config::ProfileManager> profile_manager_;
  collectors::CollectorPipeline pipeline_;
  collectors::CollectorPipeline::TickCallback tick_callback_;
  std::unique_ptr<TelemetryServiceImpl> telemetry_impl_;
  std::unique_ptr<CommandServiceImpl> command_impl_;
  std::unique_ptr<grpc::Server> server_;
  std::string endpoint_;
  std::chrono::steady_clock::time_point started_at_;
  std::atomic<bool> running_{false};
  std::atomic<bool> paused_{false};
};

}  // namespace hwpanel::service
