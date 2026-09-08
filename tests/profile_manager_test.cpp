#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "hwpanel/config/profile_manager.h"
#include "hwpanel/core/event_bus.h"
#include "plugins/executors/executor.h"

using hwpanel::config::ProfileManager;
using hwpanel::core::EventBus;
using hwpanel::core::Profile;
using hwpanel::core::ProfileStore;
using hwpanel::executors::ApplyResult;
using hwpanel::executors::IExecutor;

namespace {

class FakeExecutor : public IExecutor {
 public:
  explicit FakeExecutor(std::string key) : key_(std::move(key)) {}
  std::string_view Key() const override { return key_; }
  bool Available() const override { return available_; }
  std::string CurrentValue() override { return value_; }
  ApplyResult Apply(const std::string& desired) override {
    ++apply_calls_;
    if (fail_on_apply_) {
      return {false, "injected failure"};
    }
    value_ = desired;
    return {true, "ok"};
  }
  void set_value(std::string v) { value_ = std::move(v); }
  void set_available(bool v) { available_ = v; }
  void set_fail_on_apply(bool v) { fail_on_apply_ = v; }

 private:
  std::string key_;
  bool available_ = true;
  bool fail_on_apply_ = false;
  std::string value_ = "initial";
  int apply_calls_ = 0;
};

class ProfileManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = std::filesystem::temp_directory_path() /
           ("hwpanel_pm_test_" +
            std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::filesystem::remove_all(dir_);
    store_ = std::make_unique<ProfileStore>(dir_);

    auto owned_a = std::make_unique<FakeExecutor>("fake.a");
    auto owned_b = std::make_unique<FakeExecutor>("fake.b");
    executor_a_ = owned_a.get();
    executor_b_ = owned_b.get();

    std::vector<std::unique_ptr<IExecutor>> executors;
    executors.push_back(std::move(owned_a));
    executors.push_back(std::move(owned_b));
    // The manager gets its own store instance over the same directory so the
    // test fixture can keep writing profiles afterwards.
    manager_ = std::make_unique<ProfileManager>(ProfileStore(dir_), std::move(executors),
                                                &bus_);
  }
  void TearDown() override { std::filesystem::remove_all(dir_); }

  void AddProfile(const std::string& id, std::map<std::string, std::string> settings) {
    Profile profile;
    profile.id = id;
    profile.name = id;
    profile.settings = std::move(settings);
    ASSERT_TRUE(store_->Save(profile));
  }

  std::filesystem::path dir_;
  std::unique_ptr<ProfileStore> store_;
  EventBus bus_;
  FakeExecutor* executor_a_ = nullptr;
  FakeExecutor* executor_b_ = nullptr;
  std::unique_ptr<ProfileManager> manager_;
};

}  // namespace

TEST_F(ProfileManagerTest, SwitchAppliesAllExecutorsAndSetsActive) {
  AddProfile("p1", {{"fake.a", "1"}, {"fake.b", "2"}});
  const auto result = manager_->SwitchTo("p1");
  EXPECT_TRUE(result.ok) << result.message;
  EXPECT_EQ(executor_a_->CurrentValue(), "1");
  EXPECT_EQ(executor_b_->CurrentValue(), "2");
  EXPECT_EQ(manager_->ActiveId(), "p1");
}

TEST_F(ProfileManagerTest, FailureRollsBackPreviouslyAppliedExecutors) {
  AddProfile("p2", {{"fake.a", "1"}, {"fake.b", "2"}});
  executor_b_->set_fail_on_apply(true);  // fails mid-transaction (b > a ordering)

  executor_a_->set_value("snapshot-a");
  const auto result = manager_->SwitchTo("p2");
  EXPECT_FALSE(result.ok);
  ASSERT_EQ(result.rolled_back_keys.size(), 1u);
  EXPECT_EQ(result.rolled_back_keys[0], "fake.a");
  EXPECT_EQ(executor_a_->CurrentValue(), "snapshot-a");  // snapshot restored
  EXPECT_NE(manager_->ActiveId(), "p2");
}

TEST_F(ProfileManagerTest, UnavailableExecutorTriggersRollbackToo) {
  AddProfile("p5", {{"fake.a", "1"}, {"fake.b", "2"}});
  executor_b_->set_available(false);
  executor_a_->set_value("before");

  const auto result = manager_->SwitchTo("p5");
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(executor_a_->CurrentValue(), "before");
}

TEST_F(ProfileManagerTest, UnknownExecutorKeyFailsCleanly) {
  AddProfile("p3", {{"no.such.executor", "x"}});
  const auto result = manager_->SwitchTo("p3");
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.message.find("no executor"), std::string::npos);
}

TEST_F(ProfileManagerTest, MissingProfileIsReported) {
  const auto result = manager_->SwitchTo("ghost");
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.message.find("not found"), std::string::npos);
}

TEST_F(ProfileManagerTest, SuccessPublishesEvent) {
  AddProfile("p4", {{"fake.a", "9"}});
  auto subscriber = bus_.Subscribe();
  EXPECT_TRUE(manager_->SwitchTo("p4").ok);
  hwpanel::core::EventMessage event;
  ASSERT_TRUE(subscriber->Pop(event, 500));
  EXPECT_EQ(event.level, "info");
  EXPECT_EQ(event.title, "Profile applied");
}
