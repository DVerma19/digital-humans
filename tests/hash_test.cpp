#include <catch2/catch_test_macros.hpp>
#include "dh/hash.hpp"

using namespace dh::hash;

TEST_CASE("BLAKE3 of empty input matches reference", "[hash][G2]") {
    auto h = blake3("", 0);
    const uint8_t expected[32] = {
        0xaf, 0x13, 0x49, 0xb9, 0xf5, 0xf9, 0xa1, 0xa6,
        0xa0, 0x40, 0x4d, 0xea, 0x36, 0xdc, 0xc9, 0x49,
        0x9b, 0xcb, 0x25, 0xc9, 0xad, 0xc1, 0x12, 0xb7,
        0xcc, 0x9a, 0x93, 0xca, 0xe4, 0x1f, 0x32, 0x62
    };
    for (int i = 0; i < 32; ++i) {
        CHECK(h.bytes[i] == expected[i]);
    }
}

TEST_CASE("field_seed is deterministic and sensitive", "[hash][G2]") {
    CHECK(field_seed(42, 1, 5) == field_seed(42, 1, 5));
    CHECK(field_seed(42, 1, 5) != field_seed(42, 1, 6));
    CHECK(field_seed(42, 1, 5) != field_seed(43, 1, 5));
    CHECK(field_seed(42, 1, 5) != field_seed(42, 2, 5));
}