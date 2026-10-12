#include "iris/application.hpp"

#include "iris/desktop_shell.hpp"
#include "iris/log.hpp"
#include "iris/nes/nes_system.hpp"
#include "iris/paths.hpp"
#include "iris/version.hpp"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace iris {

int Application::run(int argc, char** argv) {
    std::string initialRom;
    if (argc > 2) {
        std::cerr << "Usage: iris [--version] [rom.nes]\n";
        return 1;
    }
    if (argc == 2) {
        if (std::strcmp(argv[1], "--version") == 0) {
            std::cout << kProductName << ' ' << kVersionString << '\n';
            return 0;
        }
        if (argv[1][0] == '-') {
            std::cerr << "Unknown argument: " << argv[1] << '\n';
            return 1;
        }
        initialRom = argv[1];
    }

    const auto paths = resolveAppPaths();
    if (!paths.has_value()) {
        std::cerr << "Could not resolve the application data directory.\n";
        return 1;
    }
    if (!ensureAppDirectories(*paths)) {
        std::cerr << "Could not create application data directories.\n";
        return 1;
    }
    if (!log::initialize(paths->logs, LogLevel::Info)) {
        std::cerr << "Could not initialize logging.\n";
        return 1;
    }

    Config config = ConfigService::defaults();
    bool persistConfig = false;
    const auto loaded = loadConfigFile(paths->configFile);
    if (loaded.ok()) {
        config = loaded.value();
        persistConfig = true;
        log::setLevel(config.logLevel);
        log::info("Loaded configuration.");
    } else if (loaded.error().code == ErrorCode::NotFound) {
        if (saveConfigFile(paths->configFile, config)) {
            persistConfig = true;
            log::info("Wrote the default configuration.");
        } else {
            log::error("Could not write the default configuration.");
        }
    } else {
        log::error(std::string("Configuration was rejected: ") + loaded.error().message);
        log::error("Using default settings for this session. The configuration file was not modified.");
    }

    log::info(std::string("Data directory: ") + paths->root.string());
    const auto system = systems::nes::createNesSystem();
    DesktopShell shell(config, *system, initialRom);
    const int code = shell.run();
    if (code == 0 && persistConfig) {
        config.windowWidth = shell.windowWidth();
        config.windowHeight = shell.windowHeight();
        if (!saveConfigFile(paths->configFile, config)) {
            log::error("Could not save configuration.");
        }
    }
    log::shutdown();
    return code;
}

}  // namespace iris
