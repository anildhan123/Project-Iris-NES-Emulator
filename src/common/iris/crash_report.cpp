#include "iris/crash_report.hpp"

#include <cstdio>

namespace iris {

std::string formatCrashSummary(std::uint32_t code, std::uint64_t address) {
    const char* name = "Unexpected exception";
    if (code == 0xC0000005u) {
        name = "Access violation";
    } else if (code == 0xC0000094u) {
        name = "Integer division by zero";
    } else if (code == 0xC00000FDu) {
        name = "Stack overflow";
    }
    char buffer[160] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%s (0x%08X) at 0x%llX.",
        name,
        code,
        static_cast<unsigned long long>(address));
    return buffer;
}

}  // namespace iris
