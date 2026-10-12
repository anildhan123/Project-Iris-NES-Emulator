#pragma once

#include "iris/config.hpp"

#include <filesystem>
#include <string_view>

namespace iris::log {

[[nodiscard]] bool initialize(const std::filesystem::path& logDirectory, LogLevel level);
void setLevel(LogLevel level);
void shutdown();

void trace(std::string_view message);
void debug(std::string_view message);
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);
void critical(std::string_view message);

}  // namespace iris::log
