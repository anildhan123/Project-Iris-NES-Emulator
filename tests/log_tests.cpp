#include "iris/log.hpp"

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("logger initialization honors the configured level") {
    iris::test::TemporaryDirectory directory;
    REQUIRE(iris::log::initialize(directory.path(), iris::LogLevel::Warn));
    iris::log::info("hidden info message");
    iris::log::warn("visible warn message");
    iris::log::shutdown();

    const std::string logText = readFile(directory.path() / "iris.log");
    REQUIRE(logText.find("hidden info message") == std::string::npos);
    REQUIRE(logText.find("visible warn message") != std::string::npos);
}
