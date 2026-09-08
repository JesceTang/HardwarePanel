#include <gtest/gtest.h>

#include "hwpanel/v1/command.pb.h"
#include "hwpanel/v1/telemetry.pb.h"

TEST(ProtoRoundTripTest, TelemetrySampleSurvivesSerialization) {
  hwpanel::v1::TelemetrySample original;
  original.set_timestamp_ms(1710000000000);
  auto* metric = original.add_metrics();
  metric->set_metric_id("cpu.usage_pct");
  metric->set_value(42.5);
  metric->set_unit("percent");
  (*metric->mutable_labels())["core"] = "total";

  const std::string wire = original.SerializeAsString();
  ASSERT_FALSE(wire.empty());

  hwpanel::v1::TelemetrySample decoded;
  ASSERT_TRUE(decoded.ParseFromString(wire));
  EXPECT_EQ(decoded.timestamp_ms(), original.timestamp_ms());
  ASSERT_EQ(decoded.metrics_size(), 1);
  EXPECT_EQ(decoded.metrics(0).metric_id(), "cpu.usage_pct");
  EXPECT_DOUBLE_EQ(decoded.metrics(0).value(), 42.5);
  EXPECT_EQ(decoded.metrics(0).labels().at("core"), "total");
}

TEST(ProtoRoundTripTest, ProfileSettingsMapSurvivesSerialization) {
  hwpanel::v1::Profile original;
  original.set_id("performance");
  original.set_name("Performance");
  (*original.mutable_settings())["power.plan"] = "high";
  (*original.mutable_settings())["display.brightness"] = "80";

  hwpanel::v1::Profile decoded;
  ASSERT_TRUE(decoded.ParseFromString(original.SerializeAsString()));
  EXPECT_EQ(decoded.id(), "performance");
  EXPECT_EQ(decoded.settings().at("power.plan"), "high");
  EXPECT_EQ(decoded.settings().size(), 2u);
}

TEST(ProtoRoundTripTest, SwitchResponseCarriesRollbackKeys) {
  hwpanel::v1::SwitchProfileResponse original;
  original.set_ok(false);
  original.set_message("injected failure");
  original.add_rolled_back_keys("power.plan");

  hwpanel::v1::SwitchProfileResponse decoded;
  ASSERT_TRUE(decoded.ParseFromString(original.SerializeAsString()));
  EXPECT_FALSE(decoded.ok());
  ASSERT_EQ(decoded.rolled_back_keys_size(), 1);
  EXPECT_EQ(decoded.rolled_back_keys(0), "power.plan");
}
