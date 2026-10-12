#pragma once

#include "iris/emulated_system.hpp"
#include "iris/nes/bus.hpp"
#include "iris/nes/cartridge.hpp"
#include "iris/nes/nrom.hpp"

#include <memory>
#include <string>
#include <vector>

namespace iris::systems::nes {

class NesSystem final : public IEmulatedSystem {
public:
    NesSystem();

    bool loadContent(const std::filesystem::path& path, std::string& error) override;
    bool loadBytes(const std::uint8_t* data, std::size_t size, const std::string& name, std::string& error);
    void unload() override;
    [[nodiscard]] bool loaded() const override { return loaded_; }
    void reset() override;
    void setPaused(bool paused) override { paused_ = paused; }
    [[nodiscard]] bool paused() const override { return paused_; }
    void runFrame() override;
    [[nodiscard]] FrameView frame() const override;
    [[nodiscard]] AudioView audio() const override;
    void setController(int port, std::uint8_t buttons) override;
    [[nodiscard]] SystemInfo info() const override;

private:
    bool adopt(Cartridge cartridge, std::string& error);
    void tick();

    Cartridge cartridge_;
    std::unique_ptr<Nrom> mapper_;
    Ppu ppu_;
    Apu apu_;
    Controller port1_;
    Controller port2_;
    Bus bus_;
    Cpu cpu_;
    std::vector<float> audio_;
    std::string title_;
    std::uint64_t cpuCycles_ = 0;
    bool loaded_ = false;
    bool paused_ = false;
    bool nmiLevel_ = false;
};

[[nodiscard]] std::unique_ptr<IEmulatedSystem> createNesSystem();

}  // namespace iris::systems::nes
