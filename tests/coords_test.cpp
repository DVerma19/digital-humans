#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "dh/coords.hpp"

using namespace dh::coords;
using Catch::Approx;

TEST_CASE("world_to_chunk uses floor division", "[coords][G1]") {
    SECTION("positive") {
        CHECK(world_to_chunk({  0.0,   0.0, 0.0}) == ChunkAddress{ 0, 0});
        CHECK(world_to_chunk({ 63.999, 0.0, 0.0}) == ChunkAddress{ 0, 0});
        CHECK(world_to_chunk({ 64.0,   0.0, 0.0}) == ChunkAddress{ 1, 0});
    }
    SECTION("negative") {
        CHECK(world_to_chunk({ -0.001, 0.0, 0.0}) == ChunkAddress{-1, 0});
        CHECK(world_to_chunk({ -1.0,   0.0, 0.0}) == ChunkAddress{-1, 0});
        CHECK(world_to_chunk({ -64.0,  0.0, 0.0}) == ChunkAddress{-1, 0});
        CHECK(world_to_chunk({ -64.001,0.0, 0.0}) == ChunkAddress{-2, 0});
    }
}

TEST_CASE("world_to_local stays in [0, CHUNK_SIZE_XZ)", "[coords][G1]") {
    CHECK(world_to_local({  63.5, 7.0, 0.5}).x == Approx(63.5f));
    CHECK(world_to_local({  -0.5, 7.0, 0.5}).x == Approx(63.5f));
    CHECK(world_to_local({ -64.5, 7.0, 0.5}).x == Approx(63.5f));
}

TEST_CASE("world -> chunk -> world round trip", "[coords][G1]") {
    constexpr double tol = 1e-9 * CHUNK_SIZE_XZ;
    WorldPos original { 123.25, 7.5, 456.75 };
    auto c = world_to_chunk(original);
    auto l = world_to_local(original);
    auto back = chunk_local_to_world(c, l);
    CHECK(back.x == Approx(original.x).margin(tol));
    CHECK(back.y == Approx(original.y).margin(1e-6));
    CHECK(back.z == Approx(original.z).margin(tol));
}

TEST_CASE("world -> render -> world round trip", "[coords][G4]") {
    constexpr double tol = 1e-3; // 1 mm
    WorldPos camera { 1000.0, 50.0, 2000.0 };
    WorldPos p      { 1001.5, 51.0, 2002.5 };
    auto r    = world_to_render(p, camera);
    auto back = render_to_world(r, camera);
    CHECK(back.x == Approx(p.x).margin(tol));
    CHECK(back.y == Approx(p.y).margin(tol));
    CHECK(back.z == Approx(p.z).margin(tol));
}