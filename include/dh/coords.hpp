#pragma once
#include <cstdint>

namespace dh::coords {

// --- World frame constants (v1) ---
inline constexpr double  CHUNK_SIZE_XZ = 64.0;
inline constexpr double  WORLD_SIZE_X  = 8192.0;
inline constexpr double  WORLD_SIZE_Z  = 5120.0;
inline constexpr int32_t CHUNK_COUNT_X = 128;
inline constexpr int32_t CHUNK_COUNT_Z = 80;

// --- Spaces ---
struct WorldPos { double x, y, z; };

struct ChunkAddress {
    int32_t x, z;
    bool operator==(const ChunkAddress&) const = default;
};

struct LocalCoord { float x, y, z; };   // y = world Y; x,z in [0, CHUNK_SIZE_XZ)
struct RenderPos  { float x, y, z; };
struct ScreenPos  { float x, y, depth; };

// --- Conversions (Step 2 scope) ---
ChunkAddress world_to_chunk(WorldPos p);
LocalCoord   world_to_local(WorldPos p);
WorldPos     chunk_local_to_world(ChunkAddress c, LocalCoord l);

RenderPos    world_to_render(WorldPos p, WorldPos camera);
WorldPos     render_to_world(RenderPos p, WorldPos camera);

// world_to_screen / screen_to_world_ray are intentionally absent.
// They require Camera and Viewport contracts (see CAMERA.md, not yet written).

} // namespace dh::coords