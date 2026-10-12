#include "iris/application.hpp"

#include "iris/crash.hpp"
#include "iris/desktop_shell.hpp"
#include "iris/log.hpp"
#include "iris/nes/nes_system.hpp"
#include "iris/paths.hpp"
#include "iris/version.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace iris {
namespace {

int profileContent(const std::filesystem::path& path) {
    constexpr int kFrames = 300;
    auto system = systems::nes::createNesSystem();
    std::string error;
    if (!system->loadContent(path, error)) {
        std::cerr << error << '\n';
        log::error(error);
        return 1;
    }
    const double targetMs = static_cast<double>(system->framePeriodNs()) / 1000000.0;
    double slowestMs = 0.0;
    const auto started = std::chrono::steady_clock::now();
    for (int frame = 0; frame < kFrames; ++frame) {
        const auto frameStart = std::chrono::steady_clock::now();
        system->runFrame();
        const double frameMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart).count();
        if (frameMs > slowestMs) {
            slowestMs = frameMs;
        }
    }
    const double averageMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count() / kFrames;
    const SystemInfo info = system->info();
    std::printf("Iris Emulator %s\n", kVersionString);
    std::printf("Profile: %s (%s)\n", info.title.c_str(), info.detail.c_str());
    std::printf("Frames: %d\n", kFrames);
    std::printf("Target: %.3f ms\n", targetMs);
    std::printf("Average: %.3f ms\n", averageMs);
    std::printf("Slowest: %.3f ms\n", slowestMs);
    std::fflush(stdout);
    log::info("Profile average " + std::to_string(averageMs) + " ms over " + std::to_string(kFrames) + " frames.");
    return 0;
}

}  // namespace

int Application::run(int argc, char** argv) {
    installCrashHandler();
    try {
        std::string initialRom;
        bool profile = false;
        if (argc > 3) {
            std::cerr << "Usage: iris [--version] [--profile rom.nes] [rom.nes]\n";
            return 1;
        }
        if (argc == 2) {
            if (std::strcmp(argv[1], "--version") == 0) {
                std::cout << kProductName << ' ' << kVersionString << '\n';
                return 0;
            }
            if (std::strcmp(argv[1], "--profile") == 0) {
                std::cerr << "Usage: iris --profile rom.nes\n";
                return 1;
            }
            if (argv[1][0] == '-') {
                std::cerr << "Unknown argument: " << argv[1] << '\n';
                return 1;
            }
            initialRom = argv[1];
        } else if (argc == 3) {
            if (std::strcmp(argv[1], "--profile") != 0) {
                std::cerr << "Usage: iris [--version] [--profile rom.nes] [rom.nes]\n";
                return 1;
            }
            profile = true;
            initialRom = argv[2];
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
        setCrashLogFile(paths->logs / "crash.log");
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
        if (profile) {
            const int code = profileContent(std::filesystem::path(initialRom));
            log::shutdown();
            return code;
        }

        const auto system = systems::nes::createNesSystem();
        DesktopShell shell(config, *system, initialRom);
        const int code = shell.run();
        if (code == 0 && persistConfig) {
            if (!saveConfigFile(paths->configFile, shell.config())) {
                log::error("Could not save configuration.");
            }
        }
        log::shutdown();
        return code;
    } catch (const std::exception& exception) {
        reportFatalError(exception.what());
        log::shutdown();
        return 1;
    } catch (...) {
        reportFatalError("An unexpected error stopped Iris.");
        log::shutdown();
        return 1;
    }
}

}  // namespace iris
