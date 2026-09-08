#include "apps/hwpanel-service/grpc/command_service_impl.h"

#include "apps/hwpanel-service/grpc/convert.h"

namespace hwpanel::service {

CommandServiceImpl::CommandServiceImpl(config::ProfileManager& profiles,
                                       core::EventBus& events,
                                       StatusProvider status_provider)
    : profiles_(profiles),
      events_(events),
      status_provider_(std::move(status_provider)) {}

grpc::Status CommandServiceImpl::ListProfiles(grpc::ServerContext* /*context*/,
                                              const v1::ListProfilesRequest* /*request*/,
                                              v1::ListProfilesResponse* response) {
  for (const auto& profile : profiles_.List()) {
    *response->add_profiles() = convert::ToProto(profile);
  }
  response->set_active_profile_id(profiles_.ActiveId());
  return grpc::Status::OK;
}

grpc::Status CommandServiceImpl::SwitchProfile(grpc::ServerContext* /*context*/,
                                               const v1::SwitchProfileRequest* request,
                                               v1::SwitchProfileResponse* response) {
  const auto result = profiles_.SwitchTo(request->profile_id());
  response->set_ok(result.ok);
  response->set_message(result.message);
  for (const auto& key : result.rolled_back_keys) {
    response->add_rolled_back_keys(key);
  }
  return grpc::Status::OK;
}

grpc::Status CommandServiceImpl::GetStatus(grpc::ServerContext* /*context*/,
                                           const v1::GetStatusRequest* /*request*/,
                                           v1::GetStatusResponse* response) {
  if (status_provider_) {
    *response->mutable_status() = status_provider_();
  }
  return grpc::Status::OK;
}

grpc::Status CommandServiceImpl::WatchEvents(
    grpc::ServerContext* context, const v1::WatchEventsRequest* /*request*/,
    grpc::ServerWriter<v1::EventMessage>* writer) {
  auto subscriber = events_.Subscribe();
  core::EventMessage event;
  while (!context->IsCancelled()) {
    if (!subscriber->Pop(event, /*timeout_ms=*/250)) {
      continue;
    }
    if (!writer->Write(convert::ToProto(event))) {
      break;
    }
  }
  subscriber->Close();
  return grpc::Status::OK;
}

}  // namespace hwpanel::service
