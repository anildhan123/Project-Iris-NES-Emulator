#pragma once

#include "iris/emulated_system.hpp"
#include "iris/nes/bus.hpp"
#include "iris/nes/cartridge.hpp"
#include "iris/nes/mapper.hpp"
#include "iris/nes/timing.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
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
    [[nodiscard]] std::int64_t framePeriodNs() const override;
    [[nodiscard]] std::string contentId() const override { return contentId_; }
    bool saveState(std::vector<std::uint8_t>& bytes, std::string_view description, std::string& error) override;
    bool loadState(const std::uint8_t* data, std::size_t size, std::string& error) override;
    void inspectState(const std::uint8_t* data, std::size_t size, StatePreview& preview) const override;
    bool copyBattery(std::vector<std::uint8_t>& bytes) const override;
    bool restoreBattery(const std::uint8_t* data, std::size_t size, std::string& error) override;
    [[nodiscard]] std::uint8_t debugRead(std::uint16_t address);

private:
    bool adopt(Cartridge cartridge, std::string& error);
    void clockCpuCycle();
    [[nodiscard]] bool irqLine() const;
    void refreshContentId();
    [[nodiscard]] bool exportComponents(std::vector<std::uint8_t>& out) const;
    [[nodiscard]] bool importComponents(const std::uint8_t* data, std::size_t size, std::string& error);

    Cartridge cartridge_;
    std::unique_ptr<Mapper> mapper_;
    Ppu ppu_;
    Apu apu_;
    Controller port1_;
    Controller port2_;
    Bus bus_;
    Cpu cpu_;
    std::vector<float> audio_;
    std::string title_;
    std::string contentId_;
    Region region_ = Region::Ntsc;
    int ppuNumerator_ = 3;
    int ppuDenominator_ = 1;
    int ppuAccum_ = 0;
    int dmcStall_ = 0;
    std::uint64_t cpuCycles_ = 0;
    bool loaded_ = false;
    bool paused_ = false;
    bool nmiLevel_ = false;
};

[[nodiscard]] std::unique_ptr<IEmulatedSystem> createNesSystem();

}  // namespace iris::systems::nes
