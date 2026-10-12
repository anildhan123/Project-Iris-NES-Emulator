#pragma once

#include <cstdint>
#include <vector>

namespace iris {

class ByteWriter {
public:
    void u8(std::uint8_t value);
    void u16(std::uint16_t value);
    void u32(std::uint32_t value);
    void u64(std::uint64_t value);
    void boolean(bool value);
    void bytes(const std::uint8_t* data, std::size_t size);

    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return data_; }

private:
    std::vector<std::uint8_t> data_;
};

class ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size);

    [[nodiscard]] std::uint8_t u8();
    [[nodiscard]] std::uint16_t u16();
    [[nodiscard]] std::uint32_t u32();
    [[nodiscard]] std::uint64_t u64();
    [[nodiscard]] bool boolean();
    void bytes(std::uint8_t* destination, std::size_t size);
    void skip(std::size_t size);

    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] bool exhausted() const { return ok_ && position_ == size_; }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t position_ = 0;
    bool ok_ = true;
};

[[nodiscard]] std::uint32_t crc32(const std::uint8_t* data, std::size_t size);

}  // namespace iris
