#include "hwpanel/core/paths.h"

#include <cstdlib>
#include <system_error>

#include <windows.h>

namespace hwpanel::core::paths {

std::filesystem::path DataDir() {
  const char* override = std::getenv("HWPANEL_DATA");
  if (override != nullptr && override[0] != '\0') {
    return std::filesystem::path(override);
  }
  wchar_t buf[MAX_PATH + 1] = {};
  const DWORD len = ::GetEnvironmentVariableW(L"PROGRAMDATA", buf, MAX_PATH);
  if (len == 0 || len > MAX_PATH) {
    return std::filesystem::path(L"C:\\ProgramData") / L"HWPanel";
  }
  return std::filesystem::path(buf) / L"HWPanel";
}

std::filesystem::path LogsDir() { return DataDir() / "logs"; }

std::filesystem::path ProfilesDir() { return DataDir() / "profiles"; }

std::filesystem::path EnsureDir(const std::filesystem::path& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

}  // namespace hwpanel::core::paths
