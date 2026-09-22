#pragma once
#include <cstdint>
#include <cstddef>

namespace dh::hash {

struct Hash256 {
    uint8_t bytes[32];
    bool operator==(const Hash256&) const = default;
};

Hash256 blake3(const void* data, std::size_t len);
uint64_t blake3_64(const void* data, std::size_t len);

// Deterministic stream seed for a noise field.
// Implements NOISE.md section 3.
uint64_t field_seed(uint64_t world_seed, uint16_t generation_version,
                    uint16_t field_id);

} // namespace dh::hash