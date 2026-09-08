#pragma once

#include <mutex>

#include <spdlog/sinks/base_sink.h>

namespace hwpanel::service {

// ETW TraceLogging sink: every log record becomes an ETW event on provider
// "HWPanel.Service" (capture with tracelog/PerfView, plan M3 observability).
class EtwSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  EtwSink();
  ~EtwSink() override;

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override {}

 private:
  bool registered_ = false;
};

}  // namespace hwpanel::service
