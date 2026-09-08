#include "apps/hwpanel-client/backend/client_worker.h"

#include <QMetaObject>
#include <QVariantMap>

#include <chrono>

#include "hwpanel/v1/command.grpc.pb.h"
#include "hwpanel/v1/telemetry.grpc.pb.h"

namespace hwpanel::client {

ClientWorker::ClientWorker(std::string endpoint, QObject* parent)
    : QObject(parent), endpoint_(std::move(endpoint)), manager_(endpoint_) {
  manager_.SetStateCallback([this](core::ConnectionState state) {
    emit ConnectionChanged(static_cast<int>(state));
  });
}

ClientWorker::~ClientWorker() { Stop(); }

void ClientWorker::Start() {
  if (running_.exchange(true)) {
    return;
  }
  manager_.Start();
  telemetry_thread_ = std::thread([this] { TelemetryLoop(); });
  events_thread_ = std::thread([this] { EventsLoop(); });
}

void ClientWorker::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (telemetry_thread_.joinable()) {
    telemetry_thread_.join();
  }
  if (events_thread_.joinable()) {
    events_thread_.join();
  }
  manager_.Stop();
}

void ClientWorker::RefreshProfiles() {
  QMetaObject::invokeMethod(this, [this] { RefreshProfilesInternal(); },
                            Qt::QueuedConnection);
}

void ClientWorker::SwitchProfile(const QString& profile_id) {
  QMetaObject::invokeMethod(this, [this, profile_id] { SwitchProfileInternal(profile_id); },
                            Qt::QueuedConnection);
}

void ClientWorker::TelemetryLoop() {
  while (running_.load()) {
    auto channel = manager_.channel();
    if (channel == nullptr ||
        channel->GetState(false) != GRPC_CHANNEL_READY) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    auto stub = v1::TelemetryService::NewStub(channel);
    grpc::ClientContext context;
    v1::SubscribeTelemetryRequest request;
    request.set_interval_hz(1.0);
    auto reader = stub->Subscribe(&context, request);
    if (reader == nullptr) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      continue;
    }
    v1::TelemetrySample message;
    while (running_.load() && reader->Read(&message)) {
      core::TelemetrySample sample;
      sample.timestamp_ms = message.timestamp_ms();
      for (const auto& metric : message.metrics()) {
        sample.metrics.push_back(
            {metric.metric_id(), metric.value(), metric.unit()});
      }
      emit SampleReceived(sample);
    }
    // Stream ended (service restart?): back off and reconnect.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

void ClientWorker::EventsLoop() {
  while (running_.load()) {
    auto channel = manager_.channel();
    if (channel == nullptr ||
        channel->GetState(false) != GRPC_CHANNEL_READY) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    auto stub = v1::CommandService::NewStub(channel);
    grpc::ClientContext context;
    v1::WatchEventsRequest request;
    auto reader = stub->WatchEvents(&context, request);
    if (reader == nullptr) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      continue;
    }
    v1::EventMessage message;
    while (running_.load() && reader->Read(&message)) {
      core::EventMessage event;
      event.timestamp_ms = message.timestamp_ms();
      event.level = message.level();
      event.title = message.title();
      event.message = message.message();
      emit EventReceived(event);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

void ClientWorker::RefreshProfilesInternal() {
  auto channel = manager_.channel();
  if (channel == nullptr) {
    return;
  }
  auto stub = v1::CommandService::NewStub(channel);
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
  v1::ListProfilesRequest request;
  v1::ListProfilesResponse response;
  const auto status = stub->ListProfiles(&context, request, &response);
  if (!status.ok()) {
    return;
  }
  QVariantList profiles;
  for (const auto& profile : response.profiles()) {
    QVariantMap map;
    map["id"] = QString::fromStdString(profile.id());
    map["name"] = QString::fromStdString(profile.name());
    map["description"] = QString::fromStdString(profile.description());
    profiles.push_back(map);
  }
  emit ProfilesReceived(profiles,
                        QString::fromStdString(response.active_profile_id()));
}

void ClientWorker::SwitchProfileInternal(const QString& profile_id) {
  auto channel = manager_.channel();
  if (channel == nullptr) {
    emit SwitchResult(false, tr("not connected"));
    return;
  }
  auto stub = v1::CommandService::NewStub(channel);
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
  v1::SwitchProfileRequest request;
  request.set_profile_id(profile_id.toStdString());
  v1::SwitchProfileResponse response;
  const auto status = stub->SwitchProfile(&context, request, &response);
  if (!status.ok()) {
    emit SwitchResult(false, QString::fromStdString(status.error_message()));
    return;
  }
  emit SwitchResult(response.ok(), QString::fromStdString(response.message()));
  if (response.ok()) {
    RefreshProfilesInternal();
  }
}

}  // namespace hwpanel::client
