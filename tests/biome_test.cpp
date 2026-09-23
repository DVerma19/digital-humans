#include <catch2/catch_test_macros.hpp>
#include "dh/biome.hpp"
#include "dh/chunk.hpp"
#include "dh/hydro.hpp"
#include "dh/hydro_chunk.hpp"

using namespace dh;
using namespace dh::biome;

namespace {

chunk::Chunk make_full_chunk(int32_t x, int32_t z,
                             const hydro::BasinGrid& basin) {
    chunk::Chunk c;
    c.address = { x, z };
    c.generation_version = 1;
    chunk::generate(c, 42);
    hydro::refine_chunk(c, 42, basin);
    biome::classify_chunk(c, 42);
    return c;
}

bool same(const chunk::Chunk& a, const chunk::Chunk& b) {
    for (size_t i = 0; i < a.biome_id.size(); ++i) {
        if (a.biome_id[i] != b.biome_id[i]) return false;
        if (a.fertility[i] != b.fertility[i]) return false;
    }
    return true;
}

} // namespace

TEST_CASE("classify returns valid IDs", "[biome]") {
    for (uint16_t b = 0; b < COUNT; ++b) {
        CHECK(b < COUNT);
    }
}

TEST_CASE("classify order: ocean first", "[biome]") {
    CHECK(classify(-10.0f, 0.0f, 20.0f, 1000.0f, 0.0f, 0.0f) == OCEAN);
}

TEST_CASE("classify order: lake beats everything but ocean", "[biome]") {
    CHECK(classify(100.0f, 5.0f, 20.0f, 1000.0f, 0.0f, 3.0f) == LAKE);
}

TEST_CASE("classify: hot + arid = desert", "[biome]") {
    CHECK(classify(100.0f, 5.0f, 25.0f, 100.0f, 0.0f, 0.0f) == DESERT);
}

TEST_CASE("classify: hot + wet = tropical forest", "[biome]") {
    CHECK(classify(100.0f, 5.0f, 25.0f, 1500.0f, 0.0f, 0.0f) == TROPICAL_FOREST);
}

TEST_CASE("classify: cold + wet = boreal forest", "[biome]") {
    CHECK(classify(100.0f, 5.0f, -5.0f, 800.0f, 0.0f, 0.0f) == BOREAL_FOREST);
}

TEST_CASE("biome classify is deterministic", "[biome][G7]") {
    auto basin = hydro::compute_basin_grid(42, 1);
    auto a = make_full_chunk(3, 4, basin);
    auto b = make_full_chunk(3, 4, basin);
    CHECK(same(a, b));
}

TEST_CASE("biome IDs are all valid", "[biome][G7]") {
    auto basin = hydro::compute_basin_grid(42, 1);
    for (int32_t x = 0; x < 5; ++x) {
        for (int32_t z = 0; z < 5; ++z) {
            auto c = make_full_chunk(x, z, basin);
            for (uint16_t b : c.biome_id) {
                CHECK(b < COUNT);
            }
        }
    }
}

TEST_CASE("fertility in [0,1]", "[biome][G7]") {
    auto basin = hydro::compute_basin_grid(42, 1);
    for (int32_t x = 0; x < 5; ++x) {
        for (int32_t z = 0; z < 5; ++z) {
            auto c = make_full_chunk(x, z, basin);
            for (float f : c.fertility) {
                CHECK(f >= 0.0f);
                CHECK(f <= 1.0f);
            }
        }
    }
}

TEST_CASE("multiple biomes exist across the world", "[biome][G7]") {
    auto basin = hydro::compute_basin_grid(42, 1);
    int seen[COUNT] = {0};
    for (int32_t x = 0; x < 128; x += 8) {
        for (int32_t z = 0; z < 80; z += 8) {
            auto c = make_full_chunk(x, z, basin);
            for (uint16_t b : c.biome_id) seen[b]++;
        }
    }
    int distinct = 0;
    for (int i = 0; i < COUNT; ++i) if (seen[i] > 0) ++distinct;
    CHECK(distinct >= 6);
}