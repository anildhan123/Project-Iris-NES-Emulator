#include "iris.h"

#include "iris/nes/nes_system.hpp"
#include "iris/version.hpp"

#include <cstring>
#include <exception>
#include <filesystem>
#include <new>
#include <string>
#include <vector>

struct IrisSystem {
    iris::systems::nes::NesSystem nes;
};

namespace {

void copyMessage(char* error, size_t errorSize, const std::string& text) {
    if (error == nullptr || errorSize == 0) {
        return;
    }
    const size_t count = text.size() < errorSize - 1 ? text.size() : errorSize - 1;
    if (count > 0) {
        std::memcpy(error, text.data(), count);
    }
    error[count] = '\0';
}

}  // namespace

extern "C" {

IrisSystem* iris_create(void) {
    try {
        return new IrisSystem();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void iris_destroy(IrisSystem* system) {
    delete system;
}

int iris_load_bytes(
    IrisSystem* system, const uint8_t* data, size_t size, const char* name, char* error, size_t errorSize) {
    if (system == nullptr || data == nullptr) {
        copyMessage(error, errorSize, "A ROM is required.");
        return 0;
    }
    try {
        std::string message;
        const std::string title = name == nullptr ? "game" : name;
        if (!system->nes.loadBytes(data, size, title, message)) {
            copyMessage(error, errorSize, message);
            return 0;
        }
        copyMessage(error, errorSize, "");
        return 1;
    } catch (const std::exception& exception) {
        copyMessage(error, errorSize, exception.what());
        return 0;
    }
}

int iris_load_file(IrisSystem* system, const char* path, char* error, size_t errorSize) {
    if (system == nullptr || path == nullptr || path[0] == '\0') {
        copyMessage(error, errorSize, "A ROM path is required.");
        return 0;
    }
    try {
        std::string message;
        if (!system->nes.loadContent(std::filesystem::path(path), message)) {
            copyMessage(error, errorSize, message);
            return 0;
        }
        copyMessage(error, errorSize, "");
        return 1;
    } catch (const std::exception& exception) {
        copyMessage(error, errorSize, exception.what());
        return 0;
    }
}

void iris_unload(IrisSystem* system) {
    if (system != nullptr) {
        system->nes.unload();
    }
}

int iris_loaded(const IrisSystem* system) {
    return system != nullptr && system->nes.loaded() ? 1 : 0;
}

void iris_reset(IrisSystem* system) {
    if (system != nullptr) {
        system->nes.reset();
    }
}

void iris_set_paused(IrisSystem* system, int paused) {
    if (system != nullptr) {
        system->nes.setPaused(paused != 0);
    }
}

void iris_run_frame(IrisSystem* system) {
    if (system != nullptr) {
        system->nes.runFrame();
    }
}

void iris_set_controller(IrisSystem* system, int port, uint8_t buttons) {
    if (system != nullptr) {
        system->nes.setController(port, buttons);
    }
}

const uint8_t* iris_frame(const IrisSystem* system, int* width, int* height) {
    if (system == nullptr) {
        if (width != nullptr) {
            *width = 0;
        }
        if (height != nullptr) {
            *height = 0;
        }
        return nullptr;
    }
    const iris::FrameView frame = system->nes.frame();
    if (width != nullptr) {
        *width = frame.width;
    }
    if (height != nullptr) {
        *height = frame.height;
    }
    return frame.rgba;
}

const float* iris_audio(const IrisSystem* system, int* count, int* sampleRate) {
    if (system == nullptr) {
        if (count != nullptr) {
            *count = 0;
        }
        if (sampleRate != nullptr) {
            *sampleRate = 0;
        }
        return nullptr;
    }
    const iris::AudioView audio = system->nes.audio();
    if (count != nullptr) {
        *count = audio.count;
    }
    if (sampleRate != nullptr) {
        *sampleRate = audio.sampleRate;
    }
    return audio.samples;
}

int64_t iris_frame_period_ns(const IrisSystem* system) {
    if (system == nullptr) {
        return 0;
    }
    return system->nes.framePeriodNs();
}

int iris_content_id(const IrisSystem* system, char* out, size_t outSize) {
    if (system == nullptr || out == nullptr || outSize < 65) {
        return 0;
    }
    const std::string id = system->nes.contentId();
    if (id.size() != 64) {
        return 0;
    }
    std::memcpy(out, id.data(), 64);
    out[64] = '\0';
    return 1;
}

int iris_save_state(
    IrisSystem* system, uint8_t* out, size_t* size, const char* description, char* error, size_t errorSize) {
    if (system == nullptr || size == nullptr) {
        copyMessage(error, errorSize, "A state buffer is required.");
        return 0;
    }
    try {
        std::vector<std::uint8_t> bytes;
        std::string message;
        if (!system->nes.saveState(bytes, description == nullptr ? "" : description, message)) {
            copyMessage(error, errorSize, message);
            return 0;
        }
        if (out == nullptr || *size < bytes.size()) {
            *size = bytes.size();
            if (out != nullptr) {
                copyMessage(error, errorSize, "The state buffer is too small.");
                return 0;
            }
            return 1;
        }
        if (!bytes.empty()) {
            std::memcpy(out, bytes.data(), bytes.size());
        }
        *size = bytes.size();
        copyMessage(error, errorSize, "");
        return 1;
    } catch (const std::exception& exception) {
        copyMessage(error, errorSize, exception.what());
        return 0;
    }
}

int iris_load_state(IrisSystem* system, const uint8_t* data, size_t size, char* error, size_t errorSize) {
    if (system == nullptr || data == nullptr) {
        copyMessage(error, errorSize, "A state is required.");
        return 0;
    }
    try {
        std::string message;
        if (!system->nes.loadState(data, size, message)) {
            copyMessage(error, errorSize, message);
            return 0;
        }
        copyMessage(error, errorSize, "");
        return 1;
    } catch (const std::exception& exception) {
        copyMessage(error, errorSize, exception.what());
        return 0;
    }
}

int iris_copy_battery(const IrisSystem* system, uint8_t* out, size_t* size, char* error, size_t errorSize) {
    if (system == nullptr || size == nullptr) {
        copyMessage(error, errorSize, "A battery buffer is required.");
        return 0;
    }
    try {
        std::vector<std::uint8_t> bytes;
        if (!system->nes.copyBattery(bytes)) {
            copyMessage(error, errorSize, "This cartridge has no battery-backed RAM.");
            return 0;
        }
        if (out == nullptr || *size < bytes.size()) {
            *size = bytes.size();
            if (out != nullptr) {
                copyMessage(error, errorSize, "The battery buffer is too small.");
                return 0;
            }
            return 1;
        }
        if (!bytes.empty()) {
            std::memcpy(out, bytes.data(), bytes.size());
        }
        *size = bytes.size();
        copyMessage(error, errorSize, "");
        return 1;
    } catch (const std::exception& exception) {
        copyMessage(error, errorSize, exception.what());
        return 0;
    }
}

int iris_restore_battery(IrisSystem* system, const uint8_t* data, size_t size, char* error, size_t errorSize) {
    if (system == nullptr || data == nullptr) {
        copyMessage(error, errorSize, "A battery save is required.");
        return 0;
    }
    try {
        std::string message;
        if (!system->nes.restoreBattery(data, size, message)) {
            copyMessage(error, errorSize, message);
            return 0;
        }
        copyMessage(error, errorSize, "");
        return 1;
    } catch (const std::exception& exception) {
        copyMessage(error, errorSize, exception.what());
        return 0;
    }
}

const char* iris_version(void) {
    return iris::kVersionString;
}

}  // extern "C"
