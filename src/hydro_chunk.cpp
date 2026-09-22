#include "dh/hydro_chunk.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace dh::hydro {

namespace {

constexpr float RIVER_MIN_ACCUM  = 4.0f;
constexpr float RIVER_FULL_ACCUM = 40.0f;
constexpr float LAKE_EPSILON     = 0.05f;

constexpr int32_t S = chunk::SIZE;
constexpr int32_t H = S + 2;

inline bool in_basin(int32_t bx, int32_t bz) {
    return bx >= 0 && bx < BASIN_W && bz >= 0 && bz < BASIN_H;
}

} // namespace

void refine_chunk(chunk::Chunk& c, uint64_t world_seed, const BasinGrid& basin) {
    const int32_t bx = c.address.x;
    const int32_t bz = c.address.z;

    if (!in_basin(bx, bz)) {
        std::fill(c.river_flow.begin(), c.river_flow.end(), 0.0f);
        std::fill(c.lake_depth.begin(), c.lake_depth.end(), 0.0f);
        return;
    }

    const BasinCell& bc = basin.at(bx, bz);

    std::vector<float> elev(static_cast<size_t>(H) * H, 0.0f);
    for (int32_t hz = 0; hz < H; ++hz) {
        for (int32_t hx = 0; hx < H; ++hx) {
            elev[static_cast<size_t>(hz) * H + hx] =
                chunk::elevation_at(c.address, hx - 1, hz - 1,
                                    world_seed, c.generation_version);
        }
    }

    std::vector<uint8_t> dir(static_cast<size_t>(H) * H, DIR_NONE);
    for (int32_t hz = 1; hz <= S; ++hz) {
        for (int32_t hx = 1; hx <= S; ++hx) {
            const int32_t i = hz * H + hx;
            const float   e = elev[i];

            float   best_e = e;
            uint8_t best_d = DIR_NONE;
            for (uint8_t d = 0; d < 8; ++d) {
                const int32_t nx = hx + D8_DX[d];
                const int32_t nz = hz + D8_DZ[d];
                const float   ne = elev[nz * H + nx];
                if (ne < best_e) {
                    best_e = ne;
                    best_d = d;
                }
            }
            dir[i] = best_d;
        }
    }

    std::vector<float> accum(static_cast<size_t>(H) * H, 1.0f);

    std::vector<int32_t> order;
    order.reserve(static_cast<size_t>(S) * S);
    for (int32_t hz = 1; hz <= S; ++hz)
        for (int32_t hx = 1; hx <= S; ++hx)
            order.push_back(hz * H + hx);

    std::sort(order.begin(), order.end(), [&](int32_t a, int32_t b) {
        if (elev[a] != elev[b]) return elev[a] > elev[b];
        return a < b;
    });

    for (int32_t i : order) {
        const uint8_t d = dir[i];
        if (d == DIR_NONE) continue;
        const int32_t hx = i % H;
        const int32_t hz = i / H;
        const int32_t nx = hx + D8_DX[d];
        const int32_t nz = hz + D8_DZ[d];
        accum[nz * H + nx] += accum[i];
    }

    std::fill(c.river_flow.begin(), c.river_flow.end(), 0.0f);
    std::fill(c.lake_depth.begin(), c.lake_depth.end(), 0.0f);

    for (int32_t lz = 0; lz < S; ++lz) {
        for (int32_t lx = 0; lx < S; ++lx) {
            const int32_t i = (lz + 1) * H + (lx + 1);
            const int32_t o = lz * S + lx;

            if (bc.is_ocean) continue;

            if (bc.is_lake) {
                const float depth = bc.filled_elevation - elev[i];
                if (depth > LAKE_EPSILON) {
                    c.lake_depth[o] = depth;
                }
                continue;
            }

            const float a = accum[i];
            const float t = (a - RIVER_MIN_ACCUM) /
                            (RIVER_FULL_ACCUM - RIVER_MIN_ACCUM);
            if (t > 0.0f) {
                c.river_flow[o] = std::min(t, 1.0f);
            }
        }
    }
}

} // namespace dh::hydro