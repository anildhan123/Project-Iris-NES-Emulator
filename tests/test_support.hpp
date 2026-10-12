#pragma once

#include "iris/paths.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <cstdlib>
#else
#include <cstdlib>
#endif

namespace iris::test {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::atomic<int> sequence{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("iris-test-" + std::to_string(sequence.fetch_add(1)) + "-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

class ScopedEnvironment {
public:
    ScopedEnvironment(const char* name, const std::string& value) : name_(name) {
        previous_ = environmentValue(name);
        set(value.c_str());
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

    ~ScopedEnvironment() {
        if (previous_.has_value()) {
            set(previous_->c_str());
        } else {
            set("");
        }
    }

private:
    void set(const char* value) const {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), value);
#else
        if (value[0] == '\0') {
            unsetenv(name_.c_str());
        } else {
            setenv(name_.c_str(), value, 1);
        }
#endif
    }

    std::string name_;
    std::optional<std::string> previous_;
};

}  // namespace iris::test
