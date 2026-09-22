#pragma once
#include <cstdint>

namespace dh::noise {

enum class FieldId : uint16_t {
    LAND_MASK          = 1,
    ELEVATION_BASE     = 2,
    TEMPERATURE_BASE   = 3,
    RAINFALL_BASE      = 4,
    ELEVATION_DETAIL   = 5,
    ROUGHNESS          = 6,
    RIVER_WARP         = 7,
    BIOME_JITTER       = 8,
    VEGETATION_DENSITY = 9,
    SURFACE_OFFSET     = 10,
};

struct NoiseParams {
    double   frequency  = 1.0;
    uint32_t octaves    = 4u;
    double   lacunarity = 2.0;
    double   gain       = 0.5;
    double   amplitude  = 1.0;
    double   bias       = 0.0;
};

float sample(FieldId field, double x, double z,
             uint64_t world_seed, uint16_t gen_version,
             const NoiseParams& p) noexcept;

float sample_default(FieldId field, double x, double z,
                     uint64_t world_seed, uint16_t gen_version) noexcept;

} // namespace dh::noise