#include <catch2/catch_test_macros.hpp>
#include "dh/hydro_chunk.hpp"
#include "dh/hydro.hpp"

using namespace dh;
using namespace dh::hydro;

namespace {

hydro::BasinGrid basin42;
bool basin_ready = false;

const hydro::BasinGrid& get_basin() {
    if (!basin_ready) {
        basin42 = compute_basin_grid(42, 1);
        basin_ready = true;
    }
    return basin42;
}

chunk::Chunk make_chunk(int32_t x, int32_t z) {
    chunk::Chunk c;
    c.address = { x, z };
    c.generation_version = 1;
    chunk::generate(c, 42);
    refine_chunk(c, 42, get_basin());
    return c;
}

bool same_water(const chunk::Chunk& a, const chunk::Chunk& b) {
    for (size_t i = 0; i < a.river_flow.size(); ++i) {
        if (a.river_flow[i] != b.river_flow[i]) return false;
        if (a.lake_depth[i] != b.lake_depth[i]) return false;
    }
    return true;
}

} // namespace

TEST_CASE("refine_chunk is deterministic", "[hydro_chunk][G6]") {
    auto a = make_chunk(2, 3);
    auto b = make_chunk(2, 3);
    CHECK(same_water(a, b));
}

TEST_CASE("river_flow in [0,1]", "[hydro_chunk][G6]") {
    for (int32_t x = -5; x <= 5; ++x) {
        for (int32_t z = -5; z <= 5; ++z) {
            auto c = make_chunk(x, z);
            for (float f : c.river_flow) {
                CHECK(f >= 0.0f);
                CHECK(f <= 1.0f);
            }
        }
    }
}

TEST_CASE("lake_depth is non-negative", "[hydro_chunk][G6]") {
    for (int32_t x = -5; x <= 5; ++x) {
        for (int32_t z = -5; z <= 5; ++z) {
            auto c = make_chunk(x, z);
            for (float f : c.lake_depth) {
                CHECK(f >= 0.0f);
            }
        }
    }
}

TEST_CASE("out-of-world chunks have no water", "[hydro_chunk][G6]") {
    auto c = make_chunk(-100, 0);
    for (float f : c.river_flow) CHECK(f == 0.0f);
    for (float f : c.lake_depth) CHECK(f == 0.0f);

    auto d = make_chunk(200, 0);
    for (float f : d.river_flow) CHECK(f == 0.0f);
    for (float f : d.lake_depth) CHECK(f == 0.0f);
}

TEST_CASE("some chunk has river or lake water", "[hydro_chunk][G6]") {
    // Across a wide area, at least some cells must be wet.
    int wet_cells = 0;
    for (int32_t x = 40; x < 60; ++x) {
        for (int32_t z = 20; z < 40; ++z) {
            auto c = make_chunk(x, z);
            for (size_t i = 0; i < c.river_flow.size(); ++i) {
                if (c.river_flow[i] > 0.0f) ++wet_cells;
                if (c.lake_depth[i] > 0.0f) ++wet_cells;
            }
        }
    }
    CHECK(wet_cells > 0);
}