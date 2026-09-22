#include "dh/hydro.hpp"
#include "dh/chunk.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

namespace dh::hydro {

const int8_t D8_DX[8] = {  1,  1,  0, -1, -1, -1,  0,  1 };
const int8_t D8_DZ[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };

namespace {

constexpr float LAKE_EPSILON = 0.05f;

inline bool in_bounds(int32_t x, int32_t z) {
    return x >= 0 && x < BASIN_W && z >= 0 && z < BASIN_H;
}

inline float sample_center_elev(int32_t bx, int32_t bz,
                                uint64_t seed, uint16_t version) {
    coords::ChunkAddress addr{ bx, bz };
    return chunk::elevation_at(addr, 32, 32, seed, version);
}

uint8_t direction_between(int32_t ax, int32_t az, int32_t bx, int32_t bz) {
    const int32_t dx = bx - ax;
    const int32_t dz = bz - az;
    for (uint8_t d = 0; d < 8; ++d) {
        if (D8_DX[d] == dx && D8_DZ[d] == dz) return d;
    }
    return DIR_NONE;
}

} // namespace

BasinGrid compute_basin_grid(uint64_t world_seed, uint16_t generation_version) {
    const int32_t N = BASIN_W * BASIN_H;

    BasinGrid g;
    g.cells.resize(N);

    // 1. Raw elevation + ocean flag.
    for (int32_t bz = 0; bz < BASIN_H; ++bz) {
        for (int32_t bx = 0; bx < BASIN_W; ++bx) {
            auto& c = g.at(bx, bz);
            c.elevation = sample_center_elev(bx, bz, world_seed, generation_version);
            c.is_ocean  = c.elevation < SEA_LEVEL_METERS;
        }
    }

    // 2. Priority-flood depression filling.
    std::vector<float>   filled(N, std::numeric_limits<float>::infinity());
    std::vector<int32_t> downstream(N, -1);
    std::vector<uint8_t> is_outlet(N, 0);

    using QE = std::pair<float, int32_t>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;

    auto seed_cell = [&](int32_t i) {
        filled[i]    = g.cells[i].elevation;
        is_outlet[i] = 1;
        pq.push({ filled[i], i });
    };

    for (int32_t bz = 0; bz < BASIN_H; ++bz) {
        for (int32_t bx = 0; bx < BASIN_W; ++bx) {
            const int32_t i = bz * BASIN_W + bx;
            const bool boundary =
                (bx == 0 || bx == BASIN_W - 1 || bz == 0 || bz == BASIN_H - 1);
            if (g.cells[i].is_ocean || boundary) seed_cell(i);
        }
    }

    while (!pq.empty()) {
        const auto top = pq.top();
        pq.pop();
        const float   e = top.first;
        const int32_t i = top.second;
        if (e > filled[i] + 1e-6f) continue;   // stale

        const int32_t bx = i % BASIN_W;
        const int32_t bz = i / BASIN_W;
        for (uint8_t d = 0; d < 8; ++d) {
            const int32_t nx = bx + D8_DX[d];
            const int32_t nz = bz + D8_DZ[d];
            if (!in_bounds(nx, nz)) continue;
            const int32_t ni = nz * BASIN_W + nx;
            if (std::isfinite(filled[ni])) continue;

            const float ne = std::max(g.cells[ni].elevation, filled[i]);
            filled[ni]     = ne;
            downstream[ni] = i;
            pq.push({ ne, ni });
        }
    }

    // 3. Direction, filled elevation, lake flags.
    for (int32_t i = 0; i < N; ++i) {
        auto& c = g.cells[i];
        c.filled_elevation = std::isfinite(filled[i]) ? filled[i] : c.elevation;

        if (is_outlet[i]) {
            c.direction = DIR_NONE;
        } else if (downstream[i] >= 0) {
            const int32_t bx = i % BASIN_W;
            const int32_t bz = i / BASIN_W;
            const int32_t nx = downstream[i] % BASIN_W;
            const int32_t nz = downstream[i] / BASIN_W;
            c.direction = direction_between(bx, bz, nx, nz);
        } else {
            c.direction = DIR_NONE;
        }

        const float depth = c.filled_elevation - c.elevation;
        c.is_lake = (depth > LAKE_EPSILON);
    }

    // 4. Sort by filled elevation ascending.
    std::vector<int32_t> idx(N);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int32_t a, int32_t b) {
        if (filled[a] != filled[b]) return filled[a] < filled[b];
        return a < b;
    });

    // 5. Accumulation ascending (each cell adds to its downstream).
    for (int32_t i : idx) {
        if (is_outlet[i]) continue;
        const int32_t d = downstream[i];
        if (d < 0) continue;
        g.cells[d].accum += g.cells[i].accum;
    }

    // 6. Strahler order ascending.
    for (int32_t i : idx) {
        auto& c = g.cells[i];
        const int32_t bx = i % BASIN_W;
        const int32_t bz = i / BASIN_W;

        uint8_t max_in    = 0;
        int     count_max = 0;
        for (uint8_t d = 0; d < 8; ++d) {
            const int32_t nx = bx + D8_DX[d];
            const int32_t nz = bz + D8_DZ[d];
            if (!in_bounds(nx, nz)) continue;
            const int32_t ni = nz * BASIN_W + nx;
            if (downstream[ni] != i) continue;
            const uint8_t o = g.cells[ni].order;
            if (o > max_in)       { max_in = o; count_max = 1; }
            else if (o == max_in) { ++count_max; }
        }

        if (max_in == 0)         c.order = 1;
        else if (count_max >= 2) c.order = static_cast<uint8_t>(std::min<int>(max_in + 1, 12));
        else                     c.order = max_in;
    }

    // 7. Basin IDs ascending.
    uint32_t next_id = 1;
    for (int32_t i : idx) {
        auto& c = g.cells[i];
        if (c.is_ocean) {
            c.basin_id = next_id++;
        } else if (c.is_lake && c.direction == DIR_NONE) {
            c.basin_id = 0;
        } else {
            const int32_t d = downstream[i];
            c.basin_id = (d >= 0) ? g.cells[d].basin_id : 0;
        }
    }

    return g;
}

} // namespace dh::hydro