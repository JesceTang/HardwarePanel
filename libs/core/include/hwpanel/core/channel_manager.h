#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

namespace hwpanel::core {

enum class ConnectionState {
  kDisconnected,
  kConnecting,
  kConnected,
};

// Owns the client-side gRPC channel and watches its connectivity state on a
// background thread. Reconnection is handled by gRPC itself; the monitor adds
// exponential backoff between probes after failures and publishes state
// transitions so the UI can render a connection status bar.
class ChannelManager {
 public:
  explicit ChannelManager(std::string address);
  ~ChannelManager();

  ChannelManager(const ChannelManager&) = delete;
  ChannelManager& operator=(const ChannelManager&) = delete;

  void Start();
  void Stop();

  ConnectionState state() const;
  std::shared_ptr<grpc::Channel> channel() const;
  const std::string& address() const { return address_; }

  void SetStateCallback(std::function<void(ConnectionState)> callback);

 private:
  void MonitorLoop();
  void SetState(ConnectionState state);

  const std::string address_;
  mutable std::mutex mutex_;
  std::shared_ptr<grpc::Channel> channel_;
  ConnectionState state_ = ConnectionState::kDisconnected;
  std::function<void(ConnectionState)> callback_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

const char* ToString(ConnectionState state);

}  // namespace hwpanel::core
