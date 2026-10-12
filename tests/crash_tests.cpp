#include "iris/crash_report.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("crash summaries name the common Windows exception codes") {
    const auto access = iris::formatCrashSummary(0xC0000005u, 0x10);
    REQUIRE(access.find("Access violation") != std::string::npos);
    REQUIRE(access.find("0xC0000005") != std::string::npos);
    REQUIRE(access.find("0x10") != std::string::npos);

    REQUIRE(iris::formatCrashSummary(0xC0000094u, 0).find("Integer division by zero") != std::string::npos);
    REQUIRE(iris::formatCrashSummary(0xC00000FDu, 0).find("Stack overflow") != std::string::npos);

    const auto other = iris::formatCrashSummary(0x1u, 0);
    REQUIRE(other.find("Unexpected exception") != std::string::npos);
    REQUIRE(other.find("0x00000001") != std::string::npos);
}
