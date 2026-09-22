#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <string>
#include "dh/chunk.hpp"

using namespace dh;
using namespace dh::chunk;

namespace {

Chunk make(int32_t x, int32_t z, uint16_t version, uint64_t seed) {
    Chunk c;
    c.address = {x, z};
    c.generation_version = version;
    generate(c, seed);
    return c;
}

bool eq(const hash::Hash256& a, const hash::Hash256& b) {
    for (int i = 0; i < 32; ++i)
        if (a.bytes[i] != b.bytes[i]) return false;
    return true;
}

std::string hex_of(const hash::Hash256& h) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string s;
    s.reserve(64);
    for (int i = 0; i < 32; ++i) {
        s.push_back(kHex[(h.bytes[i] >> 4) & 0xf]);
        s.push_back(kHex[h.bytes[i] & 0xf]);
    }
    return s;
}

struct GoldenCase {
    int32_t  x, z;
    uint16_t version;
    uint64_t seed;
    const char* expected_hex;
};

// Pinned 2026-09-22 on first passing run.
// Reference machine: AMD Ryzen 7, Windows 11, GCC 13.2.0 (Strawberry), -O0.
// If this fails: DO NOT edit the hashes. Find the regression.
// See docs/DECISIONS.md - Golden hash protocol.
constexpr GoldenCase kGolden[] = {
    {0, 0, 1, 42, "a4194e572772056f2f818d239f51b87a9846eac7e7449d6f2d00e2d14de043e7"},
    {1, 0, 1, 42, "afd4d7eaa12fe57dbbf9e57fdc888fd7bd8cbb7cfdf7e0f629c7dd2a1038d25a"},
    {0, 1, 1, 42, "b9ec8f020aa8ec55238b0f065ac11dc0a1bd225e35f10dae2a61695bfcb3dc90"},
    {0, 0, 1, 99, "09ddb0d63fa0d59fad31760a84b5c36fb0f6c21bb19b7095135c74339969a88b"},
    {0, 0, 2, 42, "4cb48a9ea20c1f565ba5a862f4107f3b7eec8b14b1c2b7d0b3ef3e857db9995a"},
};

} // namespace

TEST_CASE("chunk generation is deterministic", "[chunk][G2]") {
    auto a = make(0, 0, 1, 42);
    auto b = make(0, 0, 1, 42);
    CHECK(eq(hash_of(a), hash_of(b)));
}

TEST_CASE("chunk hashes differ by seed", "[chunk][G2]") {
    CHECK_FALSE(eq(hash_of(make(0, 0, 1, 42)),
                   hash_of(make(0, 0, 1, 43))));
}

TEST_CASE("chunk hashes differ by generation version", "[chunk][G2]") {
    CHECK_FALSE(eq(hash_of(make(0, 0, 1, 42)),
                   hash_of(make(0, 0, 2, 42))));
}

TEST_CASE("chunk hashes differ by address", "[chunk][G2]") {
    CHECK_FALSE(eq(hash_of(make(0, 0, 1, 42)),
                   hash_of(make(1, 0, 1, 42))));
}

TEST_CASE("chunk values are bit-identical across (addr, local) pairs that share a world position",
          "[chunk][G3]") {
    constexpr uint64_t seed = 42;
    constexpr uint16_t ver  = 1;

    // (addr={1,0}, lx=-1) and (addr={0,0}, lx=63) both refer to world x = 63.5.
    // (addr={0,1}, lz=-1) and (addr={0,0}, lz=63) both refer to world z = 63.5.
    for (int32_t lz = 0; lz < SIZE; ++lz) {
        const float a = elevation_at({0, 0}, 63, lz, seed, ver);
        const float b = elevation_at({1, 0}, -1, lz, seed, ver);
        CHECK(a == b);
    }
    for (int32_t lx = 0; lx < SIZE; ++lx) {
        const float a = elevation_at({0, 0}, lx, 63, seed, ver);
        const float b = elevation_at({0, 1}, lx, -1, seed, ver);
        CHECK(a == b);
    }
}

TEST_CASE("adjacent chunks are continuous at the shared boundary", "[chunk][G3]") {
    constexpr uint64_t seed = 42;
    constexpr uint16_t ver  = 1;

    for (int32_t lz = 0; lz < SIZE; ++lz) {
        const float left  = elevation_at({0, 0}, 63, lz, seed, ver);
        const float right = elevation_at({1, 0},  0, lz, seed, ver);
        CHECK(std::abs(left - right) < 100.0f);
    }
    for (int32_t lx = 0; lx < SIZE; ++lx) {
        const float low  = elevation_at({0, 0}, lx, 63, seed, ver);
        const float high = elevation_at({0, 1}, lx,  0, seed, ver);
        CHECK(std::abs(low - high) < 100.0f);
    }
}

TEST_CASE("golden chunk hashes are pinned", "[chunk][G2][golden]") {
    for (const auto& g : kGolden) {
        auto c = make(g.x, g.z, g.version, g.seed);
        auto h = hash_of(c);
        INFO("chunk(" << g.x << "," << g.z
             << ") seed=" << g.seed << " v=" << g.version);
        CHECK(hex_of(h) == std::string(g.expected_hex));
    }
}