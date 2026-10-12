#pragma once

#include <cstdint>
#include <string>

namespace iris {

// Text for an unexpected process failure. The core does not show a dialog.
[[nodiscard]] std::string formatCrashSummary(std::uint32_t code, std::uint64_t address);

}  // namespace iris
