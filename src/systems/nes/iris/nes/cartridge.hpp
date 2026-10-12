#pragma once

#include "iris/error.hpp"
#include "iris/nes/mapper.hpp"
#include "iris/nes/timing.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace iris::systems::nes {

class Cartridge {
public:
    [[nodiscard]] static Result<Cartridge> fromFile(const std::filesystem::path& path);
    [[nodiscard]] static Result<Cartridge> fromBytes(const std::uint8_t* data, std::size_t size, std::string name);

    [[nodiscard]] int mapper() const { return mapper_; }
    [[nodiscard]] int submapper() const { return submapper_; }
    [[nodiscard]] Region region() const { return region_; }
    [[nodiscard]] Mirroring mirroring() const { return mirroring_; }
    [[nodiscard]] bool battery() const { return battery_; }
    [[nodiscard]] bool nes20() const { return nes20_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] std::vector<std::uint8_t>& prg() { return prg_; }
    [[nodiscard]] const std::vector<std::uint8_t>& prg() const { return prg_; }
    [[nodiscard]] std::vector<std::uint8_t>& prgRam() { return prgRam_; }
    [[nodiscard]] const std::vector<std::uint8_t>& prgRam() const { return prgRam_; }
    [[nodiscard]] std::vector<std::uint8_t>& chr() { return chr_; }
    [[nodiscard]] const std::vector<std::uint8_t>& chr() const { return chr_; }
    [[nodiscard]] bool chrRam() const { return chrRam_; }

private:
    int mapper_ = 0;
    int submapper_ = 0;
    Region region_ = Region::Ntsc;
    Mirroring mirroring_ = Mirroring::Horizontal;
    bool battery_ = false;
    bool nes20_ = false;
    bool chrRam_ = false;
    std::string name_;
    std::vector<std::uint8_t> prg_;
    std::vector<std::uint8_t> prgRam_;
    std::vector<std::uint8_t> chr_;
};

}  // namespace iris::systems::nes
