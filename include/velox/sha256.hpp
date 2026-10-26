#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace velox {

/**
 * Pure C++20 FIPS 180-4 SHA-256 cryptographic hasher for W3C PROV-DM
 * dataset artifact provenance verification (citation-provenance-verification §5.3).
 */
class Sha256 {
public:
    Sha256();

    void update(const uint8_t* data, std::size_t len);
    void update(std::string_view sv);
    std::array<uint8_t, 32> digest();
    std::string hexdigest();

    static std::string hashHex(std::string_view input);
    static std::string hashBytesHex(const uint8_t* data, std::size_t len);

private:
    void transform(const uint8_t block[64]);

    uint32_t state_[8];
    uint64_t bitlen_ = 0;
    uint8_t buffer_[64]{};
    std::size_t buflen_ = 0;
    bool finalized_ = false;
    std::array<uint8_t, 32> final_digest_{};
};

} // namespace velox
