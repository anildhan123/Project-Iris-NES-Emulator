#pragma once

#include "iris/config.hpp"
#include "iris/emulated_system.hpp"

#include <string>

namespace iris {

class DesktopShell {
public:
    DesktopShell(const Config& config, IEmulatedSystem& system, std::string initialRom = {});

    // Returns 0 after an orderly close. Returns 1 if the window cannot be created.
    int run();

    [[nodiscard]] int windowWidth() const { return windowWidth_; }
    [[nodiscard]] int windowHeight() const { return windowHeight_; }
    [[nodiscard]] const Config& config() const { return config_; }

private:
    Config config_;
    IEmulatedSystem& system_;
    std::string initialRom_;
    int windowWidth_;
    int windowHeight_;
};

}  // namespace iris
