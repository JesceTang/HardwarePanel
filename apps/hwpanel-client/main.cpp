#include <QDateTime>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QThread>
#include <QTimer>

#include "apps/hwpanel-client/backend/client_worker.h"
#include "apps/hwpanel-client/backend/connection_status.h"
#include "apps/hwpanel-client/backend/line_chart.h"
#include "apps/hwpanel-client/backend/notification_model.h"
#include "apps/hwpanel-client/backend/profile_model.h"
#include "apps/hwpanel-client/backend/telemetry_store.h"

#include "hwpanel/core/endpoint.h"

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  app.setOrganizationName("HWPanel");
  app.setApplicationName("HWPanel Client");

  qRegisterMetaType<hwpanel::core::TelemetrySample>("hwpanel::core::TelemetrySample");
  qRegisterMetaType<hwpanel::core::EventMessage>("hwpanel::core::EventMessage");
  qmlRegisterType<hwpanel::client::LineChart>("HWPanel.Charts", 1, 0, "LineChart");

  const std::string endpoint = hwpanel::core::DefaultEndpoint();

  hwpanel::client::TelemetryStore telemetry;
  hwpanel::client::ProfileModel profiles;
  hwpanel::client::NotificationModel notifications;
  hwpanel::client::ConnectionStatus connection;

  // gRPC I/O lives on its own thread; UI-thread models consume queued signals.
  QThread worker_thread;
  worker_thread.setObjectName("hwpanel-client-worker");
  hwpanel::client::ClientWorker worker(endpoint);
  worker.moveToThread(&worker_thread);
  QObject::connect(&worker_thread, &QThread::started, &worker,
                   [&worker] { worker.Start(); });

  QObject::connect(&worker, &hwpanel::client::ClientWorker::SampleReceived,
                   &telemetry, &hwpanel::client::TelemetryStore::HandleSample);
  QObject::connect(&worker, &hwpanel::client::ClientWorker::EventReceived,
                   &notifications, &hwpanel::client::NotificationModel::Append);
  QObject::connect(&worker, &hwpanel::client::ClientWorker::ProfilesReceived,
                   &profiles, &hwpanel::client::ProfileModel::SetProfiles);
  QObject::connect(&worker, &hwpanel::client::ClientWorker::ConnectionChanged,
                   &connection, &hwpanel::client::ConnectionStatus::SetState);
  QObject::connect(&worker, &hwpanel::client::ClientWorker::SwitchResult,
                   &notifications,
                   [&notifications](bool ok, const QString& message) {
                     hwpanel::core::EventMessage event;
                     event.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
                     event.level = ok ? "info" : "warn";
                     event.title = ok ? "Profile switch" : "Profile switch failed";
                     event.message = message.toStdString();
                     notifications.Append(event);
                   });

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("telemetry", &telemetry);
  engine.rootContext()->setContextProperty("profiles", &profiles);
  engine.rootContext()->setContextProperty("notifications", &notifications);
  engine.rootContext()->setContextProperty("connection", &connection);
  engine.rootContext()->setContextProperty("worker", &worker);

  // qt_add_qml_module places QML_FILES under RESOURCE_PREFIX + URI path, so the
  // generated resource path is /HWPanel/qml/Main.qml (see .qt/rcc/*_raw_qml_0.qrc).
  engine.load(QUrl(QStringLiteral("qrc:/HWPanel/qml/Main.qml")));
  if (engine.rootObjects().isEmpty()) {
    return -1;
  }

  worker_thread.start();
  // Poll the profile list until the service answers (reconnect-tolerant).
  QTimer refresh_timer;
  QObject::connect(&refresh_timer, &QTimer::timeout, &worker,
                   &hwpanel::client::ClientWorker::RefreshProfiles);
  refresh_timer.start(5000);

  const int rc = app.exec();
  worker.Stop();
  worker_thread.quit();
  worker_thread.wait(3000);
  return rc;
}
