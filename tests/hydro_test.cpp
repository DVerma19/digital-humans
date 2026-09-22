#include <catch2/catch_test_macros.hpp>
#include "dh/hydro.hpp"

using namespace dh::hydro;

TEST_CASE("basin grid is deterministic", "[hydro][G6]") {
    auto a = compute_basin_grid(42, 1);
    auto b = compute_basin_grid(42, 1);
    REQUIRE(a.cells.size() == b.cells.size());
    for (size_t i = 0; i < a.cells.size(); ++i) {
        CHECK(a.cells[i].direction        == b.cells[i].direction);
        CHECK(a.cells[i].accum            == b.cells[i].accum);
        CHECK(a.cells[i].basin_id         == b.cells[i].basin_id);
        CHECK(a.cells[i].order            == b.cells[i].order);
        CHECK(a.cells[i].is_ocean         == b.cells[i].is_ocean);
        CHECK(a.cells[i].is_lake          == b.cells[i].is_lake);
        CHECK(a.cells[i].filled_elevation == b.cells[i].filled_elevation);
    }
}

TEST_CASE("basin grid differs by seed", "[hydro][G6]") {
    auto a = compute_basin_grid(42, 1);
    auto b = compute_basin_grid(43, 1);
    bool differs = false;
    for (size_t i = 0; i < a.cells.size() && !differs; ++i) {
        if (a.cells[i].direction != b.cells[i].direction) differs = true;
    }
    CHECK(differs);
}

TEST_CASE("every cell has accumulation >= 1", "[hydro][G6]") {
    auto g = compute_basin_grid(42, 1);
    for (const auto& c : g.cells) {
        CHECK(c.accum >= 1);
    }
}

TEST_CASE("flow follows non-increasing filled elevation", "[hydro][G6]") {
    auto g = compute_basin_grid(42, 1);
    for (int32_t bz = 1; bz < BASIN_H - 1; ++bz) {
        for (int32_t bx = 1; bx < BASIN_W - 1; ++bx) {
            const auto& c = g.at(bx, bz);
            if (c.direction == DIR_NONE) continue;
            const int32_t nx = bx + D8_DX[c.direction];
            const int32_t nz = bz + D8_DZ[c.direction];
            const auto& n = g.at(nx, nz);
            CHECK(n.filled_elevation <= c.filled_elevation + 1e-4f);
        }
    }
}

TEST_CASE("filled elevation is at least raw elevation", "[hydro][G6]") {
    auto g = compute_basin_grid(42, 1);
    for (const auto& c : g.cells) {
        CHECK(c.filled_elevation + 1e-4f >= c.elevation);
    }
}

TEST_CASE("river order is bounded", "[hydro][G6]") {
    auto g = compute_basin_grid(42, 1);
    for (const auto& c : g.cells) {
        CHECK(c.order <= 12);
    }
}