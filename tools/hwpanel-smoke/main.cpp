// hwpanel-smoke: console client used for IPC acceptance runs
// (e.g. "named pipe 1 Hz streaming for 10 minutes, zero disconnects").
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "hwpanel/core/endpoint.h"
#include "hwpanel/v1/command.grpc.pb.h"
#include "hwpanel/v1/telemetry.grpc.pb.h"

int main(int argc, char** argv) {
  std::string endpoint = hwpanel::core::DefaultEndpoint();
  int seconds = 10;
  bool check_status = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--endpoint") == 0 && i + 1 < argc) {
      endpoint = argv[++i];
    } else if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      seconds = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--status") == 0) {
      check_status = true;
    } else {
      std::fprintf(stderr,
                   "usage: hwpanel-smoke [--endpoint <addr>] [--seconds N] [--status]\n");
      return 2;
    }
  }

  auto channel = grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  auto telemetry = hwpanel::v1::TelemetryService::NewStub(channel);
  auto command = hwpanel::v1::CommandService::NewStub(channel);

  if (check_status) {
    grpc::ClientContext status_ctx;
    status_ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    hwpanel::v1::GetStatusRequest request;
    hwpanel::v1::GetStatusResponse response;
    const auto status = command->GetStatus(&status_ctx, request, &response);
    if (!status.ok()) {
      std::fprintf(stderr, "GetStatus failed: %s\n", status.error_message().c_str());
      return 1;
    }
    std::printf("state=%s version=%s uptime_ms=%lld endpoint=%s\n",
                response.status().state().c_str(), response.status().version().c_str(),
                static_cast<long long>(response.status().uptime_ms()),
                response.status().endpoint().c_str());
  }

  grpc::ClientContext ctx;
  ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(seconds + 5));
  hwpanel::v1::SubscribeTelemetryRequest request;
  request.set_interval_hz(1.0);
  auto reader = telemetry->Subscribe(&ctx, request);

  hwpanel::v1::TelemetrySample sample;
  int received = 0;
  int64_t last_ts = 0;
  bool monotonic = true;
  while (reader->Read(&sample)) {
    ++received;
    if (sample.timestamp_ms() < last_ts) {
      monotonic = false;
    }
    last_ts = sample.timestamp_ms();
    std::printf("[%lld] %d metrics:", static_cast<long long>(sample.timestamp_ms()),
                sample.metrics_size());
    for (const auto& metric : sample.metrics()) {
      std::printf(" %s=%.1f%s", metric.metric_id().c_str(), metric.value(),
                  metric.unit().c_str());
    }
    std::printf("\n");
  }
  const auto status = reader->Finish();
  const bool deadline_ok =
      status.ok() || status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED;

  std::printf("received=%d expected>=%d monotonic=%d rpc=%s\n", received,
              std::max(1, seconds - 2), monotonic ? 1 : 0, status.ok() ? "ok" : status.error_message().c_str());
  return (deadline_ok && monotonic && received >= std::max(1, seconds - 2)) ? 0 : 1;
}
