#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "dh/noise.hpp"

using namespace dh::noise;

TEST_CASE("noise sample is deterministic", "[noise][G2]") {
    for (double x = -100.0; x <= 100.0; x += 3.7) {
        for (double z = -100.0; z <= 100.0; z += 3.7) {
            const float a = sample_default(FieldId::ELEVATION_BASE, x, z, 42, 1);
            const float b = sample_default(FieldId::ELEVATION_BASE, x, z, 42, 1);
            CHECK(a == b);
        }
    }
}

TEST_CASE("noise differs by field", "[noise][G2]") {
    const float a = sample_default(FieldId::ELEVATION_BASE, 123.4, 567.8, 42, 1);
    const float b = sample_default(FieldId::ROUGHNESS,        123.4, 567.8, 42, 1);
    CHECK(a != b);
}

TEST_CASE("noise differs by seed", "[noise][G2]") {
    const float a = sample_default(FieldId::ELEVATION_BASE, 123.4, 567.8, 42, 1);
    const float b = sample_default(FieldId::ELEVATION_BASE, 123.4, 567.8, 43, 1);
    CHECK(a != b);
}

TEST_CASE("noise differs by generation version", "[noise][G2]") {
    const float a = sample_default(FieldId::ELEVATION_BASE, 123.4, 567.8, 42, 1);
    const float b = sample_default(FieldId::ELEVATION_BASE, 123.4, 567.8, 42, 2);
    CHECK(a != b);
}

TEST_CASE("noise is bounded by amplitude", "[noise][G2]") {
    const NoiseParams p{0.001, 4u, 2.0, 0.5, 1.0, 0.0};
    for (double x = -500.0; x <= 500.0; x += 17.3) {
        for (double z = -500.0; z <= 500.0; z += 17.3) {
            const float v = sample(FieldId::ELEVATION_BASE, x, z, 7, 1, p);
            const double d = static_cast<double>(v);
            CHECK(d >= -p.amplitude * 1.01);
            CHECK(d <=  p.amplitude * 1.01);
        }
    }
}

TEST_CASE("noise is continuous", "[noise][G2]") {
    const NoiseParams p{0.001, 4u, 2.0, 0.5, 1.0, 0.0};
    const float a = sample(FieldId::ELEVATION_BASE, 100.0,   100.0, 7, 1, p);
    const float b = sample(FieldId::ELEVATION_BASE, 100.001, 100.0, 7, 1, p);
    CHECK(std::abs(a - b) < 0.01f);
}