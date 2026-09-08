#include "hwpanel/core/channel_manager.h"

#include <algorithm>
#include <chrono>

namespace hwpanel::core {

const char* ToString(ConnectionState state) {
  switch (state) {
    case ConnectionState::kDisconnected:
      return "disconnected";
    case ConnectionState::kConnecting:
      return "connecting";
    case ConnectionState::kConnected:
      return "connected";
  }
  return "unknown";
}

ChannelManager::ChannelManager(std::string address) : address_(std::move(address)) {}

ChannelManager::~ChannelManager() { Stop(); }

void ChannelManager::Start() {
  if (running_.exchange(true)) {
    return;
  }
  thread_ = std::thread([this] { MonitorLoop(); });
}

void ChannelManager::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

ConnectionState ChannelManager::state() const {
  std::lock_guard lock(mutex_);
  return state_;
}

std::shared_ptr<grpc::Channel> ChannelManager::channel() const {
  std::lock_guard lock(mutex_);
  return channel_;
}

void ChannelManager::SetStateCallback(std::function<void(ConnectionState)> callback) {
  std::lock_guard lock(mutex_);
  callback_ = std::move(callback);
}

void ChannelManager::SetState(ConnectionState state) {
  std::function<void(ConnectionState)> callback;
  {
    std::lock_guard lock(mutex_);
    if (state_ == state) {
      return;
    }
    state_ = state;
    callback = callback_;
  }
  if (callback) {
    callback(state);
  }
}

void ChannelManager::MonitorLoop() {
  auto channel = grpc::CreateChannel(address_, grpc::InsecureChannelCredentials());
  {
    std::lock_guard lock(mutex_);
    channel_ = channel;
  }

  int backoff_s = 1;
  while (running_.load()) {
    const grpc_connectivity_state gstate = channel->GetState(/*try_to_connect=*/true);
    switch (gstate) {
      case GRPC_CHANNEL_READY:
        SetState(ConnectionState::kConnected);
        backoff_s = 1;
        break;
      case GRPC_CHANNEL_CONNECTING:
        SetState(ConnectionState::kConnecting);
        break;
      case GRPC_CHANNEL_TRANSIENT_FAILURE:
      case GRPC_CHANNEL_SHUTDOWN:
        SetState(ConnectionState::kDisconnected);
        break;
      case GRPC_CHANNEL_IDLE:
        // GetState(true) above already kicked the connection attempt.
        break;
    }

    std::chrono::milliseconds wait(500);
    if (gstate == GRPC_CHANNEL_TRANSIENT_FAILURE || gstate == GRPC_CHANNEL_SHUTDOWN) {
      wait = std::chrono::seconds(backoff_s);
      backoff_s = std::min(backoff_s * 2, 15);  // exponential backoff, cap 15 s
    }
    channel->WaitForStateChange(gstate, std::chrono::system_clock::now() + wait);
  }
  SetState(ConnectionState::kDisconnected);
}

}  // namespace hwpanel::core
