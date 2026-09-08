#include "hwpanel/core/endpoint.h"

#include <cstdlib>

namespace hwpanel::core {

std::string NamedPipeAddress() { return "np:\\\\.\\pipe\\" + std::string(kPipeName); }

std::string TcpAddress(int port) { return "127.0.0.1:" + std::to_string(port); }

std::string DefaultEndpoint() {
  const char* override = std::getenv("HWPANEL_ENDPOINT");
  if (override != nullptr && override[0] != '\0') {
    return override;
  }
  return NamedPipeAddress();
}

bool IsNamedPipeAddress(const std::string& address) {
  return address.rfind("np:", 0) == 0;
}

}  // namespace hwpanel::core
