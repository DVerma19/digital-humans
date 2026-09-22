#pragma once
#include <cstdint>
#include <vector>
#include "dh/coords.hpp"
#include "dh/hash.hpp"

namespace dh::chunk {

inline constexpr int32_t SIZE       = 64;
inline constexpr int32_t CELL_COUNT = SIZE * SIZE;

struct Chunk {
    coords::ChunkAddress address{0, 0};
    uint16_t generation_version = 0;
    uint16_t lod                = 0;

    std::vector<float> elevation;  // SIZE*SIZE, row-major z*SIZE + x
    std::vector<float> roughness;  // SIZE*SIZE, row-major z*SIZE + x

    Chunk();
};

// Value at any local coordinate, including outside [0, SIZE).
// Pure function of world position.
float elevation_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version);

float roughness_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version);

void generate(Chunk& c, uint64_t world_seed);

hash::Hash256 hash_of(const Chunk& c);

} // namespace dh::chunk