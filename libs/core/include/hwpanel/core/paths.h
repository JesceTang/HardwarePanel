#pragma once

#include <filesystem>

namespace hwpanel::core::paths {

// %ProgramData%\HWPanel, overridable with the HWPANEL_DATA environment
// variable (used by unit tests and console-mode development).
std::filesystem::path DataDir();
std::filesystem::path LogsDir();
std::filesystem::path ProfilesDir();

// create_directories + return the path; never throws.
std::filesystem::path EnsureDir(const std::filesystem::path& dir);

}  // namespace hwpanel::core::paths
