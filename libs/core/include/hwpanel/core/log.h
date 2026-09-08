#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace hwpanel::core::log {

// Creates the process-wide logger with a console sink plus a rotating file
// sink under <data_dir>/logs/<component>.log. Safe to call once at startup.
void Init(const std::string& component, const std::string& data_dir);

// Attaches an extra sink (e.g. Windows Event Log / ETW sink added in M3).
void AddSink(const spdlog::sink_ptr& sink);

std::shared_ptr<spdlog::logger>& Logger();

}  // namespace hwpanel::core::log

#define HWLOG_INFO(...) ::hwpanel::core::log::Logger()->info(__VA_ARGS__)
#define HWLOG_WARN(...) ::hwpanel::core::log::Logger()->warn(__VA_ARGS__)
#define HWLOG_ERROR(...) ::hwpanel::core::log::Logger()->error(__VA_ARGS__)
#define HWLOG_DEBUG(...) ::hwpanel::core::log::Logger()->debug(__VA_ARGS__)
