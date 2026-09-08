#pragma once

#include <QObject>
#include <QVariantList>
#include <QVector>

#include "hwpanel/core/telemetry_types.h"

namespace hwpanel::client {

// One ring of chart points exposed to QML as a QVariantList property.
class MetricSeries : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList values READ values NOTIFY valuesChanged)
  Q_PROPERTY(double latest READ latest NOTIFY valuesChanged)
 public:
  MetricSeries(QString id, QString unit, int capacity, QObject* parent = nullptr);

  void Append(double value);
  QVariantList values() const;
  double latest() const { return latest_; }
  const QString& id() const { return id_; }
  const QString& unit() const { return unit_; }

 signals:
  void valuesChanged();

 private:
  QString id_;
  QString unit_;
  int capacity_;
  QVector<double> values_;
  double latest_ = 0.0;
};

// Routes incoming telemetry samples into the per-metric series the QML
// charts bind to (lives on the UI thread, fed via queued signals).
class TelemetryStore : public QObject {
  Q_OBJECT
  Q_PROPERTY(hwpanel::client::MetricSeries* cpuUsage READ cpuUsage CONSTANT)
  Q_PROPERTY(hwpanel::client::MetricSeries* gpuUsage READ gpuUsage CONSTANT)
  Q_PROPERTY(hwpanel::client::MetricSeries* memUsedMb READ memUsedMb CONSTANT)
  Q_PROPERTY(hwpanel::client::MetricSeries* diskIoBps READ diskIoBps CONSTANT)
  Q_PROPERTY(hwpanel::client::MetricSeries* cpuTempC READ cpuTempC CONSTANT)
  Q_PROPERTY(hwpanel::client::MetricSeries* gpuTempC READ gpuTempC CONSTANT)
 public:
  explicit TelemetryStore(QObject* parent = nullptr);

  MetricSeries* cpuUsage() { return cpu_usage_; }
  MetricSeries* gpuUsage() { return gpu_usage_; }
  MetricSeries* memUsedMb() { return mem_used_mb_; }
  MetricSeries* diskIoBps() { return disk_io_bps_; }
  MetricSeries* cpuTempC() { return cpu_temp_c_; }
  MetricSeries* gpuTempC() { return gpu_temp_c_; }

 public slots:
  void HandleSample(const hwpanel::core::TelemetrySample& sample);

 private:
  MetricSeries* cpu_usage_;
  MetricSeries* gpu_usage_;
  MetricSeries* mem_used_mb_;
  MetricSeries* disk_io_bps_;
  MetricSeries* cpu_temp_c_;
  MetricSeries* gpu_temp_c_;
};

}  // namespace hwpanel::client
