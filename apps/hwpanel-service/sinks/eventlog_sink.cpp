#include "apps/hwpanel-service/sinks/eventlog_sink.h"

#include <vector>

namespace hwpanel::service {

EventLogSink::EventLogSink(const std::string& source_name) {
  const std::wstring wide(source_name.begin(), source_name.end());
  handle_ = ::RegisterEventSourceW(nullptr, wide.c_str());
}

EventLogSink::~EventLogSink() {
  if (handle_ != nullptr) {
    ::DeregisterEventSource(handle_);
  }
}

void EventLogSink::sink_it_(const spdlog::details::log_msg& msg) {
  if (handle_ == nullptr) {
    return;
  }
  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);

  const std::string text(formatted.data(), formatted.size());
  const std::wstring wide(text.begin(), text.end());
  LPCWSTR strings[1] = {const_cast<LPWSTR>(wide.c_str())};

  WORD type = EVENTLOG_INFORMATION_TYPE;
  if (msg.level >= spdlog::level::err) {
    type = EVENTLOG_ERROR_TYPE;
  } else if (msg.level == spdlog::level::warn) {
    type = EVENTLOG_WARNING_TYPE;
  }
  ::ReportEventW(handle_, type, 0, 0, nullptr, 1, 0, strings, nullptr);
}

}  // namespace hwpanel::service
