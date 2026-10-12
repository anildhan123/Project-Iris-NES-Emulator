#include "iris/sha256.hpp"

#include <vector>

namespace iris {
namespace {

std::uint32_t rotateRight(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
}

constexpr std::uint32_t kRound[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2,
};

}  // namespace

void sha256(const std::uint8_t* data, std::size_t size, std::uint8_t out[32]) {
    std::uint32_t hash[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                             0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::vector<std::uint8_t> message;
    if (data != nullptr && size > 0) {
        message.assign(data, data + size);
    }
    message.push_back(0x80);
    while ((message.size() % 64) != 56) {
        message.push_back(0);
    }
    const std::uint64_t bits = static_cast<std::uint64_t>(size) * 8;
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>((bits >> shift) & 0xFF));
    }

    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        std::uint32_t words[64] = {};
        for (int index = 0; index < 16; ++index) {
            const std::size_t at = offset + static_cast<std::size_t>(index) * 4;
            words[index] = (static_cast<std::uint32_t>(message[at]) << 24) |
                           (static_cast<std::uint32_t>(message[at + 1]) << 16) |
                           (static_cast<std::uint32_t>(message[at + 2]) << 8) | message[at + 3];
        }
        for (int index = 16; index < 64; ++index) {
            const std::uint32_t small0 = rotateRight(words[index - 15], 7) ^ rotateRight(words[index - 15], 18) ^
                                         (words[index - 15] >> 3);
            const std::uint32_t small1 = rotateRight(words[index - 2], 17) ^ rotateRight(words[index - 2], 19) ^
                                         (words[index - 2] >> 10);
            words[index] = words[index - 16] + small0 + words[index - 7] + small1;
        }

        std::uint32_t a = hash[0];
        std::uint32_t b = hash[1];
        std::uint32_t c = hash[2];
        std::uint32_t d = hash[3];
        std::uint32_t e = hash[4];
        std::uint32_t f = hash[5];
        std::uint32_t g = hash[6];
        std::uint32_t h = hash[7];
        for (int index = 0; index < 64; ++index) {
            const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + sum1 + choice + kRound[index] + words[index];
            const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    for (int index = 0; index < 8; ++index) {
        out[index * 4] = static_cast<std::uint8_t>((hash[index] >> 24) & 0xFF);
        out[index * 4 + 1] = static_cast<std::uint8_t>((hash[index] >> 16) & 0xFF);
        out[index * 4 + 2] = static_cast<std::uint8_t>((hash[index] >> 8) & 0xFF);
        out[index * 4 + 3] = static_cast<std::uint8_t>(hash[index] & 0xFF);
    }
}

std::string sha256Hex(const std::uint8_t* data, std::size_t size) {
    std::uint8_t digest[32] = {};
    sha256(data, size, digest);
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(64, '0');
    for (int index = 0; index < 32; ++index) {
        text[static_cast<std::size_t>(index) * 2] = kDigits[digest[index] >> 4];
        text[static_cast<std::size_t>(index) * 2 + 1] = kDigits[digest[index] & 0x0F];
    }
    return text;
}

}  // namespace iris
