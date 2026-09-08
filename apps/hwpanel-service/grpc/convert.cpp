#include "apps/hwpanel-service/grpc/convert.h"

namespace hwpanel::service::convert {

v1::TelemetrySample ToProto(const core::TelemetrySample& sample,
                            const std::set<std::string>& filter) {
  v1::TelemetrySample out;
  out.set_timestamp_ms(sample.timestamp_ms);
  for (const auto& metric : sample.metrics) {
    if (!filter.empty() && filter.count(metric.metric_id) == 0) {
      continue;
    }
    auto* m = out.add_metrics();
    m->set_metric_id(metric.metric_id);
    m->set_value(metric.value);
    m->set_unit(metric.unit);
  }
  return out;
}

v1::Profile ToProto(const core::Profile& profile) {
  v1::Profile out;
  out.set_id(profile.id);
  out.set_name(profile.name);
  out.set_description(profile.description);
  for (const auto& [key, value] : profile.settings) {
    (*out.mutable_settings())[key] = value;
  }
  return out;
}

v1::EventMessage ToProto(const core::EventMessage& event) {
  v1::EventMessage out;
  out.set_timestamp_ms(event.timestamp_ms);
  out.set_level(event.level);
  out.set_title(event.title);
  out.set_message(event.message);
  return out;
}

}  // namespace hwpanel::service::convert
