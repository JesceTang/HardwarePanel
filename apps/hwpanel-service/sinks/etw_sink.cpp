#include "apps/hwpanel-service/sinks/etw_sink.h"

#include <windows.h>
#include <evntrace.h>

#include <TraceLoggingProvider.h>

namespace hwpanel::service {

TRACELOGGING_DEFINE_PROVIDER(
    g_hwpanel_provider, "HWPanel.Service",
    // {8F3A2B1C-4D5E-4F6A-9B8C-7D6E5F4A3B2C}
    (0x8f3a2b1c, 0x4d5e, 0x4f6a, 0x9b, 0x8c, 0x7d, 0x6e, 0x5f, 0x4a, 0x3b, 0x2c));

EtwSink::EtwSink() { registered_ = TraceLoggingRegister(g_hwpanel_provider) == 0; }

EtwSink::~EtwSink() {
  if (registered_) {
    TraceLoggingUnregister(g_hwpanel_provider);
  }
}

void EtwSink::sink_it_(const spdlog::details::log_msg& msg) {
  if (!registered_) {
    return;
  }
  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);
  const std::string text(formatted.data(), formatted.size());
  const std::wstring wide(text.begin(), text.end());

  // TraceLoggingLevel must be a compile-time constant (it is baked into the
  // event descriptor at build time), so dispatch the runtime severity into
  // fixed-level TraceLoggingWrite calls.
  const wchar_t* message = wide.c_str();
  if (msg.level >= spdlog::level::err) {
    TraceLoggingWrite(g_hwpanel_provider, "Log",
                      TraceLoggingLevel(TRACE_LEVEL_ERROR),
                      TraceLoggingWideString(message, "message"));
  } else if (msg.level == spdlog::level::warn) {
    TraceLoggingWrite(g_hwpanel_provider, "Log",
                      TraceLoggingLevel(TRACE_LEVEL_WARNING),
                      TraceLoggingWideString(message, "message"));
  } else if (msg.level == spdlog::level::debug) {
    TraceLoggingWrite(g_hwpanel_provider, "Log",
                      TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
                      TraceLoggingWideString(message, "message"));
  } else {
    TraceLoggingWrite(g_hwpanel_provider, "Log",
                      TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
                      TraceLoggingWideString(message, "message"));
  }
}

}  // namespace hwpanel::service
