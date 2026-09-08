#pragma once

#include <string>

namespace hwpanel::core {

inline constexpr const char* kPipeName = "hwpanel";
inline constexpr int kDefaultTcpPort = 50051;

// "np:\\.\pipe\hwpanel" - gRPC named-pipe transport (local-only, ACL-bound).
std::string NamedPipeAddress();

// "127.0.0.1:<port>" - loopback fallback transport.
std::string TcpAddress(int port = kDefaultTcpPort);

// HWPANEL_ENDPOINT override, otherwise the named pipe (plan decision #3).
std::string DefaultEndpoint();

bool IsNamedPipeAddress(const std::string& address);

}  // namespace hwpanel::core
