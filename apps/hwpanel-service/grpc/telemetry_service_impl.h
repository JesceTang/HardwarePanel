#pragma once

#include <set>
#include <string>

#include <grpcpp/grpcpp.h>

#include "hwpanel/core/telemetry_hub.h"
#include "hwpanel/v1/telemetry.grpc.pb.h"

namespace hwpanel::service {

// Server-streaming telemetry: one hub subscriber per RPC, optional metric
// filter and push-rate decimation.
class TelemetryServiceImpl final : public v1::TelemetryService::Service {
 public:
  explicit TelemetryServiceImpl(core::TelemetryHub& hub);

  grpc::Status Subscribe(grpc::ServerContext* context,
                         const v1::SubscribeTelemetryRequest* request,
                         grpc::ServerWriter<v1::TelemetrySample>* writer) override;

  grpc::Status GetHistory(grpc::ServerContext* context,
                          const v1::GetHistoryRequest* request,
                          v1::GetHistoryResponse* response) override;

 private:
  core::TelemetryHub& hub_;
};

}  // namespace hwpanel::service
