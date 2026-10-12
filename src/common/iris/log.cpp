#include "iris/log.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <memory>
#include <vector>

namespace iris::log {
namespace {

spdlog::level::level_enum toSpdlog(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warn:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Critical:
        return spdlog::level::critical;
    }
    return spdlog::level::info;
}

void write(spdlog::level::level_enum level, std::string_view message) {
    if (const auto logger = spdlog::default_logger()) {
        logger->log(level, "{}", message);
    }
}

}  // namespace

bool initialize(const std::filesystem::path& logDirectory, LogLevel level) {
    shutdown();
    try {
#if defined(_WIN32)
        const auto fileName = (logDirectory / "iris.log").wstring();
#else
        const auto fileName = (logDirectory / "iris.log").string();
#endif
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(fileName, false);
        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        auto logger = std::make_shared<spdlog::logger>("iris", sinks.begin(), sinks.end());
        logger->set_level(toSpdlog(level));
        logger->flush_on(spdlog::level::trace);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::set_default_logger(std::move(logger));
        return true;
    } catch (const spdlog::spdlog_ex& exception) {
        std::fprintf(stderr, "Logging failed: %s\n", exception.what());
        return false;
    }
}

void setLevel(LogLevel level) {
    if (const auto logger = spdlog::default_logger()) {
        logger->set_level(toSpdlog(level));
    }
}

void shutdown() {
    spdlog::shutdown();
}

void trace(std::string_view message) { write(spdlog::level::trace, message); }
void debug(std::string_view message) { write(spdlog::level::debug, message); }
void info(std::string_view message) { write(spdlog::level::info, message); }
void warn(std::string_view message) { write(spdlog::level::warn, message); }
void error(std::string_view message) { write(spdlog::level::err, message); }
void critical(std::string_view message) { write(spdlog::level::critical, message); }

}  // namespace iris::log
