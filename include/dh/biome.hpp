#pragma once
#include <cstdint>
#include "dh/chunk.hpp"

namespace dh::biome {

enum Id : uint16_t {
    OCEAN = 0, LAKE, BEACH, DESERT, SAVANNA, TROPICAL_FOREST,
    GRASSLAND, TEMPERATE_FOREST, BOREAL_FOREST, TUNDRA,
    SNOW, MOUNTAIN, WETLAND, RIVERBANK,
    COUNT
};

// Pure classifier. Order of checks is fixed (see BIOMES.md §4).
uint16_t classify(float elevation, float slope_deg, float temperature_c,
                  float rainfall_mmyr, float river_flow, float lake_depth);

// Fill c.biome_id and c.fertility.
// Requires c.elevation already populated.
// Reads c.river_flow, c.lake_depth if present (zero is fine).
void classify_chunk(chunk::Chunk& c, uint64_t world_seed);

} // namespace dh::biome