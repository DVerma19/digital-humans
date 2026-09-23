#include "dh/biome.hpp"
#include "dh/noise.hpp"
#include "dh/coords.hpp"
#include <algorithm>
#include <cmath>

namespace dh::biome {

namespace {

// --- thresholds (see BIOMES.md §4) ---
constexpr float SEA_LEVEL         = 0.0f;
constexpr float BEACH_ELEV        = 5.0f;
constexpr float FLAT_SLOPE        = 3.0f;
constexpr float STEEP_SLOPE       = 30.0f;
constexpr float MOUNTAIN_LINE     = 1200.0f;
constexpr float SNOW_LINE         = 1500.0f;

constexpr float HOT_TEMP          = 20.0f;
constexpr float COMFORTABLE_LOW   = 0.0f;
constexpr float FREEZING_TEMP     = -10.0f;

constexpr float ARID_RAIN         = 250.0f;
constexpr float FOREST_RAIN       = 600.0f;
constexpr float SAVANNA_RAIN      = 800.0f;
constexpr float WETLAND_RAIN      = 1500.0f;

constexpr float RIVERBANK_FLOW    = 0.5f;

// --- climate model ---
constexpr float BASE_TEMP_C       = 15.0f;
constexpr float LAT_SPAN_C        = 15.0f;
constexpr float TEMP_NOISE_AMP_C  = 10.0f;
constexpr float LAPSE_C_PER_M     = 0.0065f;
constexpr float MAX_RAIN_MMYR     = 2000.0f;

// --- fertility base by biome (0..1) ---
float biome_base_fertility(uint16_t b) {
    switch (b) {
        case OCEAN:            return 0.00f;
        case LAKE:             return 0.00f;
        case BEACH:            return 0.20f;
        case DESERT:           return 0.10f;
        case SAVANNA:          return 0.40f;
        case TROPICAL_FOREST:  return 0.90f;
        case GRASSLAND:        return 0.70f;
        case TEMPERATE_FOREST: return 0.80f;
        case BOREAL_FOREST:    return 0.50f;
        case TUNDRA:           return 0.20f;
        case SNOW:             return 0.00f;
        case MOUNTAIN:         return 0.10f;
        case WETLAND:          return 0.85f;
        case RIVERBANK:        return 0.95f;
    }
    return 0.50f;
}

} // namespace

uint16_t classify(float elevation, float slope_deg, float temperature_c,
                  float rainfall_mmyr, float river_flow, float lake_depth) {
    // BIOMES.md §4 order is fixed. Do not reorder without a version bump.
    if (elevation < SEA_LEVEL) return OCEAN;
    if (lake_depth > 0.0f)     return LAKE;
    if (elevation > SNOW_LINE && temperature_c < 0.0f) return SNOW;
    if (elevation > MOUNTAIN_LINE || slope_deg > STEEP_SLOPE) return MOUNTAIN;
    if (elevation < BEACH_ELEV && slope_deg < FLAT_SLOPE) return BEACH;
    if (river_flow > RIVERBANK_FLOW) return RIVERBANK;
    if (rainfall_mmyr > WETLAND_RAIN && slope_deg < FLAT_SLOPE) return WETLAND;
    if (temperature_c > HOT_TEMP && rainfall_mmyr < ARID_RAIN)    return DESERT;
    if (temperature_c > HOT_TEMP && rainfall_mmyr < SAVANNA_RAIN) return SAVANNA;
    if (temperature_c > HOT_TEMP)                                 return TROPICAL_FOREST;
    if (rainfall_mmyr < FOREST_RAIN)                              return GRASSLAND;
    if (temperature_c > COMFORTABLE_LOW)                          return TEMPERATE_FOREST;
    if (temperature_c > FREEZING_TEMP)                            return BOREAL_FOREST;
    return TUNDRA;
}

void classify_chunk(chunk::Chunk& c, uint64_t world_seed) {
    constexpr int32_t S = chunk::SIZE;
    const float base_x = static_cast<float>(c.address.x) * static_cast<float>(coords::CHUNK_SIZE_XZ);
    const float base_z = static_cast<float>(c.address.z) * static_cast<float>(coords::CHUNK_SIZE_XZ);

    auto sample_elev = [&](int32_t x, int32_t z) -> float {
        if (x < 0) x = 0;
        if (x >= S) x = S - 1;
        if (z < 0) z = 0;
        if (z >= S) z = S - 1;
        return c.elevation[z * S + x];
    };

    for (int32_t lz = 0; lz < S; ++lz) {
        for (int32_t lx = 0; lx < S; ++lx) {
            const int32_t o = lz * S + lx;
            const float wx = base_x + static_cast<float>(lx) + 0.5f;
            const float wz = base_z + static_cast<float>(lz) + 0.5f;

            const float elevation = c.elevation[o];

            const float dzdx = (sample_elev(lx + 1, lz) - sample_elev(lx - 1, lz)) * 0.5f;
            const float dzdy = (sample_elev(lx, lz + 1) - sample_elev(lx, lz - 1)) * 0.5f;
            const float slope_deg = std::atan(std::sqrt(dzdx*dzdx + dzdy*dzdy))
                                    * 57.2957795f;

            const float temp_noise = noise::sample_default(
                noise::FieldId::TEMPERATURE_BASE,
                static_cast<double>(wx), static_cast<double>(wz),
                world_seed, c.generation_version);

            const float lat = (wz / static_cast<float>(coords::WORLD_SIZE_Z)) * 2.0f - 1.0f;
            const float base_temp = BASE_TEMP_C - lat * LAT_SPAN_C
                                  + temp_noise * TEMP_NOISE_AMP_C;
            const float temperature = base_temp - elevation * LAPSE_C_PER_M;

            const float rain_noise = noise::sample_default(
                noise::FieldId::RAINFALL_BASE,
                static_cast<double>(wx), static_cast<double>(wz),
                world_seed, c.generation_version);
            const float rainfall = (rain_noise + 1.0f) * 0.5f * MAX_RAIN_MMYR;

            const uint16_t biome_id = classify(
                elevation, slope_deg, temperature, rainfall,
                c.river_flow[o], c.lake_depth[o]);

            c.biome_id[o] = biome_id;

            // Fertility: base * slope factor * water factor * temperature factor
            const float slope_factor = std::clamp(1.0f - slope_deg / 45.0f, 0.0f, 1.0f);
            const float water_factor = 1.0f
                + c.river_flow[o] * 0.5f
                + std::min(c.lake_depth[o], 1.0f) * 0.3f;

            float temp_factor;
            if      (temperature <  0.0f) temp_factor = 0.0f;
            else if (temperature < 10.0f) temp_factor = temperature / 10.0f;
            else if (temperature < 25.0f) temp_factor = 1.0f;
            else if (temperature < 35.0f) temp_factor = (35.0f - temperature) / 10.0f;
            else                          temp_factor = 0.0f;

            float fert = biome_base_fertility(biome_id)
                       * slope_factor * water_factor * temp_factor;
            c.fertility[o] = std::clamp(fert, 0.0f, 1.0f);
        }
    }
}

} // namespace dh::biome