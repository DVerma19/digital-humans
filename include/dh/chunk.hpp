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

    // Base terrain (always populated by generate()).
    std::vector<float> elevation;   // SIZE*SIZE, row-major z*SIZE + x
    std::vector<float> roughness;   // SIZE*SIZE, row-major z*SIZE + x

    // Hydro refinement (populated by hydro::refine_chunk if called).
    // Zero by default.
    std::vector<float> river_flow;  // [0, 1], SIZE*SIZE
    std::vector<float> lake_depth;  // meters, SIZE*SIZE

    Chunk();
};

// Pure function of world position, with halo support (lx/lz may be
// outside [0, SIZE)). Used for local flow and normal computation.
float elevation_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version);

float roughness_at(coords::ChunkAddress addr, int32_t lx, int32_t lz,
                   uint64_t world_seed, uint16_t generation_version);

// Fills elevation and roughness. Does NOT fill river_flow / lake_depth.
void generate(Chunk& c, uint64_t world_seed);

hash::Hash256 hash_of(const Chunk& c);

} // namespace dh::chunk