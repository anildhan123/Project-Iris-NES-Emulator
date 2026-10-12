#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace iris {

void sha256(const std::uint8_t* data, std::size_t size, std::uint8_t out[32]);

[[nodiscard]] std::string sha256Hex(const std::uint8_t* data, std::size_t size);

}  // namespace iris
