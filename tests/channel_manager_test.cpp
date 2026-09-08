#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "hwpanel/core/channel_manager.h"
#include "hwpanel/v1/telemetry.grpc.pb.h"

using hwpanel::core::ChannelManager;
using hwpanel::core::ConnectionState;

namespace {

// A gRPC sync server needs at least one registered service: with zero services
// it starts no completion-queue polling threads and BuildAndStart() returns null
// ("At least one of the completion queues must be frequently polled"). The
// connectivity under test is transport-level, so an all-UNIMPLEMENTED service
// is enough to make the server start.
class NoopTelemetryService final : public hwpanel::v1::TelemetryService::Service {};

std::unique_ptr<grpc::Server> StartLoopbackServer(int* port_out, int bind_port = 0) {
  static NoopTelemetryService service;  // must outlive the returned server
  int port = 0;
  grpc::ServerBuilder builder;
  builder.RegisterService(&service);
  const std::string address = "127.0.0.1:" + std::to_string(bind_port);
  builder.AddListeningPort(address, grpc::InsecureServerCredentials(), &port);
  auto server = builder.BuildAndStart();
  if (server != nullptr && port_out != nullptr) {
    *port_out = port;
  }
  return server;
}

bool WaitForState(const ChannelManager& manager, ConnectionState wanted, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (manager.state() == wanted) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return manager.state() == wanted;
}

}  // namespace

TEST(ChannelManagerTest, ConnectsDisconnectsAndReconnects) {
  int port = 0;
  auto server = StartLoopbackServer(&port);
  ASSERT_NE(server, nullptr);
  const std::string address = "127.0.0.1:" + std::to_string(port);

  ChannelManager manager(address);
  manager.Start();
  EXPECT_TRUE(WaitForState(manager, ConnectionState::kConnected, 5000))
      << "state=" << hwpanel::core::ToString(manager.state());

  server->Shutdown();
  server.reset();
  EXPECT_TRUE(WaitForState(manager, ConnectionState::kDisconnected, 10000))
      << "state=" << hwpanel::core::ToString(manager.state());

  // Rebind the SAME port so the manager's fixed address resolves again once the
  // old server released it (gRPC sets SO_REUSEADDR on the listen socket).
  const int original_port = port;
  auto restarted = StartLoopbackServer(&port, original_port);
  ASSERT_NE(restarted, nullptr);
  EXPECT_TRUE(WaitForState(manager, ConnectionState::kConnected, 15000))
      << "state=" << hwpanel::core::ToString(manager.state());

  manager.Stop();
  restarted->Shutdown();
}

TEST(ChannelManagerTest, StaysDisconnectedWithoutServer) {
  ChannelManager manager("127.0.0.1:1");  // nothing listens here
  manager.Start();
  EXPECT_TRUE(WaitForState(manager, ConnectionState::kDisconnected, 3000));
  EXPECT_NE(manager.state(), ConnectionState::kConnected);
  manager.Stop();
}

TEST(ChannelManagerTest, StateCallbackFiresOnTransitions) {
  int port = 0;
  auto server = StartLoopbackServer(&port);
  ASSERT_NE(server, nullptr);

  ChannelManager manager("127.0.0.1:" + std::to_string(port));
  std::atomic<int> connected_count{0};
  manager.SetStateCallback([&](ConnectionState state) {
    if (state == ConnectionState::kConnected) {
      ++connected_count;
    }
  });
  manager.Start();
  EXPECT_TRUE(WaitForState(manager, ConnectionState::kConnected, 5000));
  EXPECT_GE(connected_count.load(), 1);
  manager.Stop();
  server->Shutdown();
}
