#include "hwpanel/core/log.h"

#include <filesystem>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hwpanel::core::log {

namespace {
std::shared_ptr<spdlog::logger> g_logger = spdlog::default_logger();
}

void Init(const std::string& component, const std::string& data_dir) {
  std::error_code ec;
  const auto log_dir = std::filesystem::path(data_dir) / "logs";
  std::filesystem::create_directories(log_dir, ec);

  auto logger = spdlog::get(component);
  if (!logger) {
    logger = std::make_shared<spdlog::logger>(component);
    spdlog::register_logger(logger);
  }
  logger->sinks().clear();
  logger->sinks().push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  if (!ec) {
    const auto file = log_dir / (component + ".log");
    logger->sinks().push_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file.string(),
                                                               5 * 1024 * 1024, 3));
  }
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
  logger->set_level(spdlog::level::debug);
  logger->flush_on(spdlog::level::warn);
  g_logger = logger;
}

void AddSink(const spdlog::sink_ptr& sink) {
  g_logger->sinks().push_back(sink);
}

std::shared_ptr<spdlog::logger>& Logger() { return g_logger; }

}  // namespace hwpanel::core::log
