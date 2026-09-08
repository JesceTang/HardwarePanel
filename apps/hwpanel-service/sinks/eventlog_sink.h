#pragma once

#include <mutex>
#include <string>

#include <spdlog/sinks/base_sink.h>
#include <windows.h>

namespace hwpanel::service {

// Mirrors warning-and-above log records into the Windows Application event
// log under source "HWPanel" (plan M3 observability).
class EventLogSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  explicit EventLogSink(const std::string& source_name);
  ~EventLogSink() override;

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override {}

 private:
  HANDLE handle_ = nullptr;
};

}  // namespace hwpanel::service
