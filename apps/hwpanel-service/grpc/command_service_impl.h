#pragma once

#include <functional>

#include <grpcpp/grpcpp.h>

#include "hwpanel/config/profile_manager.h"
#include "hwpanel/core/event_bus.h"
#include "hwpanel/v1/command.grpc.pb.h"

namespace hwpanel::service {

// Unary command surface plus the streaming notification channel.
class CommandServiceImpl final : public v1::CommandService::Service {
 public:
  using StatusProvider = std::function<v1::ServiceStatus()>;

  CommandServiceImpl(config::ProfileManager& profiles, core::EventBus& events,
                     StatusProvider status_provider);

  grpc::Status ListProfiles(grpc::ServerContext* context,
                            const v1::ListProfilesRequest* request,
                            v1::ListProfilesResponse* response) override;

  grpc::Status SwitchProfile(grpc::ServerContext* context,
                             const v1::SwitchProfileRequest* request,
                             v1::SwitchProfileResponse* response) override;

  grpc::Status GetStatus(grpc::ServerContext* context,
                         const v1::GetStatusRequest* request,
                         v1::GetStatusResponse* response) override;

  grpc::Status WatchEvents(grpc::ServerContext* context,
                           const v1::WatchEventsRequest* request,
                           grpc::ServerWriter<v1::EventMessage>* writer) override;

 private:
  config::ProfileManager& profiles_;
  core::EventBus& events_;
  StatusProvider status_provider_;
};

}  // namespace hwpanel::service
