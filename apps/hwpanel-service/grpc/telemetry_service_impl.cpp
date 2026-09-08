#include "apps/hwpanel-service/grpc/telemetry_service_impl.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "apps/hwpanel-service/grpc/convert.h"

namespace hwpanel::service {

TelemetryServiceImpl::TelemetryServiceImpl(core::TelemetryHub& hub) : hub_(hub) {}

grpc::Status TelemetryServiceImpl::Subscribe(
    grpc::ServerContext* context, const v1::SubscribeTelemetryRequest* request,
    grpc::ServerWriter<v1::TelemetrySample>* writer) {
  std::set<std::string> filter(request->metric_filter().begin(),
                               request->metric_filter().end());
  int decimate = 1;
  if (request->interval_hz() > 0.0 && request->interval_hz() < 1.0) {
    decimate = static_cast<int>(std::lround(1.0 / request->interval_hz()));
    decimate = std::max(decimate, 1);
  }

  auto subscriber = hub_.Subscribe();
  long index = 0;
  core::TelemetrySample sample;
  while (!context->IsCancelled()) {
    if (!subscriber->Pop(sample, /*timeout_ms=*/250)) {
      continue;
    }
    if (index++ % decimate != 0) {
      continue;
    }
    if (!writer->Write(convert::ToProto(sample, filter))) {
      break;  // client went away
    }
  }
  subscriber->Close();
  return grpc::Status::OK;
}

grpc::Status TelemetryServiceImpl::GetHistory(grpc::ServerContext* /*context*/,
                                              const v1::GetHistoryRequest* request,
                                              v1::GetHistoryResponse* response) {
  const std::set<std::string> no_filter;
  for (const auto& sample : hub_.History(request->seconds())) {
    *response->add_samples() = convert::ToProto(sample, no_filter);
  }
  return grpc::Status::OK;
}

}  // namespace hwpanel::service
