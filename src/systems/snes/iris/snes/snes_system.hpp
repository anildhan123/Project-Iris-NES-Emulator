#pragma once

#include "iris/emulated_system.hpp"
#include "iris/region_choice.hpp"
#include "iris/snes/bus.hpp"
#include "iris/snes/cartridge.hpp"
#include "iris/snes/cpu.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace iris::systems::snes {

class SnesSystem final : public IEmulatedSystem {
public:
    SnesSystem();

    bool loadContent(const std::filesystem::path& path, std::string& error) override;
    bool loadBytes(const std::uint8_t* data, std::size_t size, std::string& error);
    void unload() override;
    [[nodiscard]] bool loaded() const override { return loaded_; }
    void reset() override;
    void powerCycle() override;
    void setRegionChoice(iris::RegionChoice choice) override;
    [[nodiscard]] iris::RegionChoice regionChoice() const override { return regionChoice_; }
    void setPaused(bool paused) override { paused_ = paused; }
    [[nodiscard]] bool paused() const override { return paused_; }
    void runFrame() override;
    [[nodiscard]] FrameView frame() const override;
    [[nodiscard]] AudioView audio() const override;
    void setController(int port, std::uint8_t buttons) override;
    [[nodiscard]] SystemInfo info() const override;
    bool describeContent(
        const std::filesystem::path& path, ContentDescription& description, std::string& error) const override;
    [[nodiscard]] std::int64_t framePeriodNs() const override;
    [[nodiscard]] std::string contentId() const override { return contentId_; }
    bool saveState(std::vector<std::uint8_t>& bytes, std::string_view description, std::string& error) override;
    bool loadState(const std::uint8_t* data, std::size_t size, std::string& error) override;
    void inspectState(const std::uint8_t* data, std::size_t size, StatePreview& preview) const override;
    bool copyBattery(std::vector<std::uint8_t>& bytes) const override;
    bool restoreBattery(const std::uint8_t* data, std::size_t size, std::string& error) override;
    [[nodiscard]] std::uint8_t debugRead(std::uint32_t address);

private:
    bool adopt(Cartridge cartridge, std::string& error);
    void applyRegion();
    [[nodiscard]] std::string detailText() const;

    Cartridge cartridge_;
    Bus bus_;
    Cpu cpu_;
    std::string contentId_;
    TvStandard standard_ = TvStandard::Ntsc;
    iris::RegionChoice regionChoice_ = iris::RegionChoice::Automatic;
    bool loaded_ = false;
    bool paused_ = false;
};

[[nodiscard]] std::unique_ptr<IEmulatedSystem> createSnesSystem();

}  // namespace iris::systems::snes
