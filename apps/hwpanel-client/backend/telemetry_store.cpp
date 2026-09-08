#include "apps/hwpanel-client/backend/telemetry_store.h"

namespace hwpanel::client {

MetricSeries::MetricSeries(QString id, QString unit, int capacity, QObject* parent)
    : QObject(parent), id_(std::move(id)), unit_(std::move(unit)), capacity_(capacity) {
  values_.reserve(capacity_);
}

void MetricSeries::Append(double value) {
  if (values_.size() >= capacity_) {
    values_.removeFirst();
  }
  values_.push_back(value);
  latest_ = value;
  emit valuesChanged();
}

QVariantList MetricSeries::values() const {
  QVariantList out;
  out.reserve(values_.size());
  for (double v : values_) {
    out.push_back(v);
  }
  return out;
}

TelemetryStore::TelemetryStore(QObject* parent) : QObject(parent) {
  cpu_usage_ = new MetricSeries("cpu.usage_pct", "%", 60, this);
  gpu_usage_ = new MetricSeries("gpu.util_pct", "%", 60, this);
  mem_used_mb_ = new MetricSeries("mem.used_mb", "MB", 60, this);
  disk_io_bps_ = new MetricSeries("disk.io_bps", "B/s", 60, this);
  cpu_temp_c_ = new MetricSeries("cpu.temp_c", "°C", 60, this);
  gpu_temp_c_ = new MetricSeries("gpu.temp_c", "°C", 60, this);
}

void TelemetryStore::HandleSample(const core::TelemetrySample& sample) {
  double disk_io = 0.0;
  bool have_disk = false;
  for (const auto& metric : sample.metrics) {
    if (metric.metric_id == "cpu.usage_pct") {
      cpu_usage_->Append(metric.value);
    } else if (metric.metric_id == "gpu.util_pct") {
      gpu_usage_->Append(metric.value);
    } else if (metric.metric_id == "mem.used_mb") {
      mem_used_mb_->Append(metric.value);
    } else if (metric.metric_id == "cpu.temp_c") {
      cpu_temp_c_->Append(metric.value);
    } else if (metric.metric_id == "gpu.temp_c") {
      gpu_temp_c_->Append(metric.value);
    } else if (metric.metric_id == "disk.read_bps" ||
               metric.metric_id == "disk.write_bps") {
      disk_io += metric.value;
      have_disk = true;
    }
  }
  if (have_disk) {
    disk_io_bps_->Append(disk_io);
  }
}

}  // namespace hwpanel::client
