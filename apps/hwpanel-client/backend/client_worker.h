#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "hwpanel/core/channel_manager.h"
#include "hwpanel/core/event_bus.h"
#include "hwpanel/core/telemetry_types.h"

#include <atomic>
#include <string>
#include <thread>

Q_DECLARE_METATYPE(hwpanel::core::TelemetrySample)
Q_DECLARE_METATYPE(hwpanel::core::EventMessage)

namespace hwpanel::client {

// Owns the gRPC client side on a dedicated QThread: connection watchdog
// (ChannelManager), telemetry subscription loop, notification stream loop and
// queued unary commands. Emits Qt signals consumed by UI-thread models.
class ClientWorker : public QObject {
  Q_OBJECT
 public:
  explicit ClientWorker(std::string endpoint, QObject* parent = nullptr);
  ~ClientWorker() override;

  void Start();
  void Stop();

  // Thread-safe entry points for QML (posted onto the worker thread).
  Q_INVOKABLE void RefreshProfiles();
  Q_INVOKABLE void SwitchProfile(const QString& profile_id);

 signals:
  void SampleReceived(const hwpanel::core::TelemetrySample& sample);
  void EventReceived(const hwpanel::core::EventMessage& event);
  void ProfilesReceived(const QVariantList& profiles, const QString& active_id);
  void SwitchResult(bool ok, const QString& message);
  void ConnectionChanged(int state);

 private:
  void TelemetryLoop();
  void EventsLoop();
  void RefreshProfilesInternal();
  void SwitchProfileInternal(const QString& profile_id);

  std::string endpoint_;
  core::ChannelManager manager_;
  std::atomic<bool> running_{false};
  std::thread telemetry_thread_;
  std::thread events_thread_;
  QString pending_switch_id_;
};

}  // namespace hwpanel::client
