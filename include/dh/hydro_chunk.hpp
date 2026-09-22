#pragma once
#include <cstdint>
#include "dh/chunk.hpp"
#include "dh/hydro.hpp"

namespace dh::hydro {

// Refine a chunk's water fields using the global basin grid.
// Populates c.river_flow and c.lake_depth. Does not touch c.elevation.
// Out-of-world chunks produce all-zero water.
void refine_chunk(chunk::Chunk& c, uint64_t world_seed, const BasinGrid& basin);

} // namespace dh::hydro