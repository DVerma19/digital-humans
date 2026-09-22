#include "dh/noise.hpp"
#include "dh/hash.hpp"
#include <cmath>
#include <cstdint>

namespace dh::noise {

namespace {

inline uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

inline double lattice(uint64_t field_seed, int64_t ix, int64_t iz) {
    uint64_t h = field_seed;
    h ^= splitmix64(static_cast<uint64_t>(ix));
    h ^= splitmix64(static_cast<uint64_t>(iz));
    h = splitmix64(h);
    constexpr double INV_53 = 1.0 / static_cast<double>(1ULL << 53);
    return static_cast<double>(h >> 11) * INV_53;
}

inline double quintic(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

inline double value_noise(uint64_t field_seed, double x, double z) {
    const double xf = std::floor(x);
    const double zf = std::floor(z);
    const int64_t ix = static_cast<int64_t>(xf);
    const int64_t iz = static_cast<int64_t>(zf);
    const double tx = x - xf;
    const double tz = z - zf;

    const double v00 = lattice(field_seed, ix,     iz);
    const double v10 = lattice(field_seed, ix + 1, iz);
    const double v01 = lattice(field_seed, ix,     iz + 1);
    const double v11 = lattice(field_seed, ix + 1, iz + 1);

    const double u = quintic(tx);
    const double v = quintic(tz);

    const double a = v00 + (v10 - v00) * u;
    const double b = v01 + (v11 - v01) * u;
    return a + (b - a) * v;
}

inline double to_signed(double v) { return v * 2.0 - 1.0; }

inline double fbm(uint64_t field_seed, double x, double z,
                  uint32_t octaves, double lacunarity, double gain) {
    double sum  = 0.0;
    double amp  = 1.0;
    double freq = 1.0;
    double norm = 0.0;
    for (uint32_t i = 0; i < octaves; ++i) {
        uint64_t oct_seed =
            splitmix64(field_seed ^ (0x9E3779B97F4A7C15ULL * (uint64_t)(i + 1u)));
        const double v = to_signed(value_noise(oct_seed, x * freq, z * freq));
        sum  += amp * v;
        norm += amp;
        amp  *= gain;
        freq *= lacunarity;
    }
    return (norm > 0.0) ? (sum / norm) : 0.0;
}

NoiseParams defaults_for(FieldId field) {
    switch (field) {
        case FieldId::LAND_MASK:          return {0.0002, 4u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::ELEVATION_BASE:     return {0.0004, 6u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::TEMPERATURE_BASE:   return {0.0002, 3u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::RAINFALL_BASE:      return {0.0003, 4u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::ELEVATION_DETAIL:   return {0.0020, 6u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::ROUGHNESS:          return {0.0010, 4u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::RIVER_WARP:         return {0.0006, 3u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::BIOME_JITTER:       return {0.0030, 2u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::VEGETATION_DENSITY: return {0.0050, 3u, 2.0, 0.5, 1.0, 0.0};
        case FieldId::SURFACE_OFFSET:     return {0.0200, 4u, 2.0, 0.5, 1.0, 0.0};
    }
    return {};
}

} // namespace

float sample(FieldId field, double x, double z,
             uint64_t world_seed, uint16_t gen_version,
             const NoiseParams& p) noexcept {
    const uint64_t seed =
        hash::field_seed(world_seed, gen_version, static_cast<uint16_t>(field));
    const double sx = x * p.frequency;
    const double sz = z * p.frequency;
    const double v  = fbm(seed, sx, sz, p.octaves, p.lacunarity, p.gain);
    const double out = p.bias + p.amplitude * v;
    return static_cast<float>(out);
}

float sample_default(FieldId field, double x, double z,
                     uint64_t world_seed, uint16_t gen_version) noexcept {
    return sample(field, x, z, world_seed, gen_version, defaults_for(field));
}

} // namespace dh::noise