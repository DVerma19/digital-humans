#include <catch2/catch_test_macros.hpp>
#include "dh/chunk_manager.hpp"

using namespace dh;
using namespace dh::chunk;

TEST_CASE("manager generates on demand and caches", "[chunk_manager][G2]") {
    Manager m(42, 1);
    CHECK(m.size() == 0);

    const Chunk* a = m.load({0, 0});
    REQUIRE(a != nullptr);
    CHECK(m.size() == 1);
    CHECK(m.state_of({0, 0}) == State::Ready);

    const Chunk* b = m.load({0, 0});
    CHECK(b == a);              // same pointer, not regenerated
    CHECK(m.size() == 1);
}

TEST_CASE("manager.get returns nullptr for unloaded", "[chunk_manager]") {
    Manager m(42, 1);
    CHECK(m.get({5, 5}) == nullptr);
    m.load({5, 5});
    CHECK(m.get({5, 5}) != nullptr);
}

TEST_CASE("manager.unload removes", "[chunk_manager]") {
    Manager m(42, 1);
    m.load({0, 0});
    CHECK(m.size() == 1);
    m.unload({0, 0});
    CHECK(m.size() == 0);
    CHECK(m.state_of({0, 0}) == State::Unloaded);
}

TEST_CASE("manager.load_around loads a square", "[chunk_manager]") {
    Manager m(42, 1);
    m.load_around({0, 0}, 2);
    CHECK(m.size() == 25);      // (2*2+1)^2 = 25
    CHECK(m.get({-2, -2}) != nullptr);
    CHECK(m.get({ 2,  2}) != nullptr);
    CHECK(m.get({ 3,  0}) == nullptr);
}

TEST_CASE("manager.unload_beyond keeps only the ring", "[chunk_manager]") {
    Manager m(42, 1);
    m.load_around({0, 0}, 5);   // 121 chunks
    CHECK(m.size() == 121);
    m.unload_beyond({0, 0}, 2);
    CHECK(m.size() == 25);
    CHECK(m.get({3, 0}) == nullptr);
    CHECK(m.get({0, 2}) != nullptr);
}

TEST_CASE("manager chunk matches direct generation", "[chunk_manager][G2]") {
    Manager m(42, 1);
    const Chunk* c = m.load({3, 4});
    REQUIRE(c != nullptr);

    Chunk direct;
    direct.address = {3, 4};
    direct.generation_version = 1;
    generate(direct, 42);

    auto h1 = hash_of(*c);
    auto h2 = hash_of(direct);
    CHECK(h1 == h2);
}