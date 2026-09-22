#include <catch2/catch_test_macros.hpp>
#include "dh/rng.hpp"

using namespace dh::rng;

TEST_CASE("PCG64 is deterministic", "[rng][G2]") {
    PCG64 a(42, 0xda3e39cb94b95bdbULL);
    PCG64 b(42, 0xda3e39cb94b95bdbULL);
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.next() == b.next());
    }
}

TEST_CASE("PCG64 diverges with different seeds", "[rng][G2]") {
    PCG64 a(42, 1);
    PCG64 b(43, 1);
    bool differs = false;
    for (int i = 0; i < 100; ++i) {
        if (a.next() != b.next()) { differs = true; break; }
    }
    CHECK(differs);
}

TEST_CASE("field_stream is deterministic", "[rng][G2]") {
    auto a = field_stream(42, 1, 5);
    auto b = field_stream(42, 1, 5);
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.next() == b.next());
    }
}

TEST_CASE("field_stream separates fields", "[rng][G2]") {
    auto a = field_stream(42, 1, 5);
    auto b = field_stream(42, 1, 6);
    bool differs = false;
    for (int i = 0; i < 100; ++i) {
        if (a.next() != b.next()) { differs = true; break; }
    }
    CHECK(differs);
}