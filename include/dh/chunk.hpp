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

    std::vector<float>    elevation;   // meters
    std::vector<float>    roughness;   // [0, 1]
    std::vector<float>    river_flow;  // [0, 1]
    std::vector<float>    lake_depth;  // meters
    std::vector<uint16_t> biome_id;    // see dh::biome::Id
    std::vector<float>    fertility;   // [0, 1]

    Chunk();
};

float elevation_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version);

float roughness_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version);

void generate(Chunk& c, uint64_t world_seed);

hash::Hash256 hash_of(const Chunk& c);

} // namespace dh::chunk