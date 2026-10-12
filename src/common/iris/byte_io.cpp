#include "iris/byte_io.hpp"

#include <cstring>

namespace iris {

void ByteWriter::u8(std::uint8_t value) { data_.push_back(value); }

void ByteWriter::u16(std::uint16_t value) {
    data_.push_back(static_cast<std::uint8_t>(value & 0xFF));
    data_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void ByteWriter::u32(std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        data_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
}

void ByteWriter::u64(std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        data_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
}

void ByteWriter::boolean(bool value) { u8(value ? 1 : 0); }

void ByteWriter::bytes(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return;
    }
    data_.insert(data_.end(), data, data + size);
}

ByteReader::ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size), ok_(data != nullptr || size == 0) {}

std::uint8_t ByteReader::u8() {
    if (!ok_ || position_ >= size_) {
        ok_ = false;
        return 0;
    }
    return data_[position_++];
}

std::uint16_t ByteReader::u16() {
    const std::uint16_t low = u8();
    const std::uint16_t high = u8();
    return static_cast<std::uint16_t>(low | (high << 8));
}

std::uint32_t ByteReader::u32() {
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        value |= static_cast<std::uint32_t>(u8()) << shift;
    }
    return value;
}

std::uint64_t ByteReader::u64() {
    std::uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        value |= static_cast<std::uint64_t>(u8()) << shift;
    }
    return value;
}

bool ByteReader::boolean() {
    const std::uint8_t value = u8();
    if (value > 1) {
        ok_ = false;
    }
    return value != 0;
}

void ByteReader::bytes(std::uint8_t* destination, std::size_t size) {
    if (!ok_ || size > size_ - position_) {
        ok_ = false;
        if (destination != nullptr && size > 0) {
            std::memset(destination, 0, size);
        }
        return;
    }
    if (destination != nullptr && size > 0) {
        std::memcpy(destination, data_ + position_, size);
    }
    position_ += size;
}

void ByteReader::skip(std::size_t size) { bytes(nullptr, size); }

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data == nullptr ? 0 : data[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

}  // namespace iris
