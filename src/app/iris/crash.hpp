#pragma once

#include <filesystem>
#include <string_view>

namespace iris {

void installCrashHandler();
void setCrashLogFile(const std::filesystem::path& path);
void reportFatalError(std::string_view message);

}  // namespace iris
