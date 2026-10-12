#pragma once

#include "iris/nes/cartridge.hpp"

#include <memory>
#include <string>

namespace iris::systems::nes {

[[nodiscard]] bool mapperSupported(const Cartridge& cartridge, std::string& error);
[[nodiscard]] std::unique_ptr<Mapper> createMapper(Cartridge& cartridge);
[[nodiscard]] const char* mapperLabel(int mapper);

}  // namespace iris::systems::nes
