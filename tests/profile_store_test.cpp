#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "hwpanel/core/profile_store.h"

using hwpanel::core::Profile;
using hwpanel::core::ProfileStore;

namespace {

class ProfileStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = std::filesystem::temp_directory_path() /
           ("hwpanel_store_test_" + std::to_string(::testing::UnitTest::GetInstance()
                                                        ->current_test_info()
                                                        ->line()));
    std::filesystem::remove_all(dir_);
  }
  void TearDown() override { std::filesystem::remove_all(dir_); }
  std::filesystem::path dir_;
};

}  // namespace

TEST_F(ProfileStoreTest, EnsureDefaultsSeedsThreeProfiles) {
  ProfileStore store(dir_);
  store.EnsureDefaults();
  const auto profiles = store.List();
  ASSERT_EQ(profiles.size(), 3u);
  EXPECT_EQ(profiles[0].id, "balanced");
  EXPECT_EQ(store.ActiveId(), "balanced");
}

TEST_F(ProfileStoreTest, SaveGetRoundTrip) {
  ProfileStore store(dir_);
  Profile profile;
  profile.id = "custom";
  profile.name = "Custom";
  profile.description = "desc";
  profile.settings = {{"power.plan", "high"}, {"display.brightness", "70"}};
  ASSERT_TRUE(store.Save(profile));

  Profile loaded;
  ASSERT_TRUE(store.Get("custom", loaded));
  EXPECT_EQ(loaded.name, "Custom");
  EXPECT_EQ(loaded.settings.at("power.plan"), "high");
  EXPECT_EQ(loaded.settings.at("display.brightness"), "70");
}

TEST_F(ProfileStoreTest, CorruptFileIsSkippedNotFatal) {
  ProfileStore store(dir_);
  store.EnsureDefaults();
  {
    std::ofstream bad(dir_ / "broken.json", std::ios::trunc);
    bad << "{ this is not json ]";
  }
  const auto profiles = store.List();
  EXPECT_EQ(profiles.size(), 3u);  // broken.json ignored
  Profile missing;
  EXPECT_FALSE(store.Get("broken", missing));
}

TEST_F(ProfileStoreTest, ActiveIdPersists) {
  ProfileStore store(dir_);
  store.EnsureDefaults();
  store.SetActiveId("performance");
  EXPECT_EQ(ProfileStore(dir_).ActiveId(), "performance");
}

TEST_F(ProfileStoreTest, RemoveDeletesProfile) {
  ProfileStore store(dir_);
  store.EnsureDefaults();
  ASSERT_TRUE(store.Remove("powersaver"));
  EXPECT_EQ(store.List().size(), 2u);
}
