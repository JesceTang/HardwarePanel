#pragma once

#include <set>
#include <string>

#include "hwpanel/core/event_bus.h"
#include "hwpanel/core/profile_store.h"
#include "hwpanel/core/telemetry_types.h"
#include "hwpanel/v1/command.pb.h"
#include "hwpanel/v1/telemetry.pb.h"

namespace hwpanel::service::convert {

// Core -> proto conversions. |filter| empty means "keep every metric".
v1::TelemetrySample ToProto(const core::TelemetrySample& sample,
                            const std::set<std::string>& filter);
v1::Profile ToProto(const core::Profile& profile);
v1::EventMessage ToProto(const core::EventMessage& event);

}  // namespace hwpanel::service::convert
