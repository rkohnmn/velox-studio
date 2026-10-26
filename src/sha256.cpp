#include "velox/sha256.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace velox {

namespace {
constexpr std::array<uint32_t, 64> K = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

constexpr uint32_t rotr(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32 - n));
}
} // namespace

Sha256::Sha256() {
    state_[0] = 0x6a09e667UL;
    state_[1] = 0xbb67ae85UL;
    state_[2] = 0x3c6ef372UL;
    state_[3] = 0xa54ff53aUL;
    state_[4] = 0x510e527fUL;
    state_[5] = 0x9b05688cUL;
    state_[6] = 0x1f83d9abUL;
    state_[7] = 0x5be0cd19UL;
}

void Sha256::transform(const uint8_t block[64]) {
    uint32_t m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (static_cast<uint32_t>(block[j]) << 24) |
               (static_cast<uint32_t>(block[j + 1]) << 16) |
               (static_cast<uint32_t>(block[j + 2]) << 8) |
               (static_cast<uint32_t>(block[j + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
        uint32_t s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + m[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(const uint8_t* data, std::size_t len) {
    std::size_t i = 0;
    if (buflen_ > 0 && buflen_ + len >= 64) {
        std::size_t take = 64 - buflen_;
        std::memcpy(buffer_ + buflen_, data, take);
        transform(buffer_);
        bitlen_ += 512;
        buflen_ = 0;
        i += take;
    }
    while (i + 64 <= len) {
        transform(data + i);
        bitlen_ += 512;
        i += 64;
    }
    if (i < len) {
        std::size_t rem = len - i;
        std::memcpy(buffer_ + buflen_, data + i, rem);
        buflen_ += rem;
    }
}

void Sha256::update(std::string_view sv) {
    update(reinterpret_cast<const uint8_t*>(sv.data()), sv.size());
}

std::array<uint8_t, 32> Sha256::digest() {
    if (finalized_) return final_digest_;

    std::size_t i = buflen_;
    bitlen_ += buflen_ * 8;

    buffer_[i++] = 0x80;
    if (buflen_ < 56) {
        while (i < 56) buffer_[i++] = 0x00;
    } else {
        while (i < 64) buffer_[i++] = 0x00;
        transform(buffer_);
        std::memset(buffer_, 0, 56);
    }

    for (int j = 0; j < 8; ++j) {
        buffer_[63 - j] = static_cast<uint8_t>((bitlen_ >> (j * 8)) & 0xFFU);
    }
    transform(buffer_);

    for (int j = 0; j < 8; ++j) {
        final_digest_[j * 4 + 0] = static_cast<uint8_t>((state_[j] >> 24) & 0xFFU);
        final_digest_[j * 4 + 1] = static_cast<uint8_t>((state_[j] >> 16) & 0xFFU);
        final_digest_[j * 4 + 2] = static_cast<uint8_t>((state_[j] >> 8) & 0xFFU);
        final_digest_[j * 4 + 3] = static_cast<uint8_t>((state_[j]) & 0xFFU);
    }
    finalized_ = true;
    return final_digest_;
}

std::string Sha256::hexdigest() {
    auto d = digest();
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t b : d) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::string Sha256::hashHex(std::string_view input) {
    Sha256 hasher;
    hasher.update(input);
    return hasher.hexdigest();
}

std::string Sha256::hashBytesHex(const uint8_t* data, std::size_t len) {
    Sha256 hasher;
    hasher.update(data, len);
    return hasher.hexdigest();
}

} // namespace velox
