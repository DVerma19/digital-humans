#pragma once
#include <cstdint>
#include <vector>
#include "dh/coords.hpp"

namespace dh::hydro {

constexpr float   SEA_LEVEL_METERS = 0.0f;
constexpr int32_t BASIN_W = static_cast<int32_t>(coords::CHUNK_COUNT_X); // 128
constexpr int32_t BASIN_H = static_cast<int32_t>(coords::CHUNK_COUNT_Z); // 80

constexpr uint8_t DIR_NONE = 255;

extern const int8_t D8_DX[8];
extern const int8_t D8_DZ[8];

struct BasinCell {
    uint8_t  direction        = DIR_NONE;
    uint32_t accum            = 1;
    uint32_t basin_id         = 0;
    uint8_t  order            = 0;
    bool     is_ocean         = false;
    bool     is_lake          = false;
    float    elevation        = 0.0f;   // raw sampled elevation
    float    filled_elevation = 0.0f;   // after priority-flood
};

struct BasinGrid {
    std::vector<BasinCell> cells;

    BasinCell&       at(int32_t bx, int32_t bz)       { return cells[bz * BASIN_W + bx]; }
    const BasinCell& at(int32_t bx, int32_t bz) const { return cells[bz * BASIN_W + bx]; }
};

BasinGrid compute_basin_grid(uint64_t world_seed, uint16_t generation_version);

} // namespace dh::hydro