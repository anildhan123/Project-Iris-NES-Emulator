#pragma once

#include "iris/config.hpp"
#include "iris/emulated_system.hpp"
#include "iris/system_kind.hpp"

#include <memory>
#include <string>

namespace iris {

using SystemFactory = std::unique_ptr<IEmulatedSystem> (*)(SystemKind);

class DesktopShell {
public:
    DesktopShell(const Config& config, SystemFactory factory, std::string initialRom = {});

    // Returns 0 after an orderly close. Returns 1 if the window cannot be created.
    int run();

    [[nodiscard]] int windowWidth() const { return windowWidth_; }
    [[nodiscard]] int windowHeight() const { return windowHeight_; }
    [[nodiscard]] const Config& config() const { return config_; }

private:
    Config config_;
    SystemFactory factory_ = nullptr;
    std::unique_ptr<IEmulatedSystem> system_;
    std::string initialRom_;
    int windowWidth_;
    int windowHeight_;
};

}  // namespace iris
