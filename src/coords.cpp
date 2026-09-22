#include "dh/coords.hpp"
#include <cmath>

namespace dh::coords {

ChunkAddress world_to_chunk(WorldPos p) {
    return {
        static_cast<int32_t>(std::floor(p.x / CHUNK_SIZE_XZ)),
        static_cast<int32_t>(std::floor(p.z / CHUNK_SIZE_XZ)),
    };
}

LocalCoord world_to_local(WorldPos p) {
    const double lx = p.x - std::floor(p.x / CHUNK_SIZE_XZ) * CHUNK_SIZE_XZ;
    const double lz = p.z - std::floor(p.z / CHUNK_SIZE_XZ) * CHUNK_SIZE_XZ;
    return {
        static_cast<float>(lx),
        static_cast<float>(p.y),
        static_cast<float>(lz),
    };
}

WorldPos chunk_local_to_world(ChunkAddress c, LocalCoord l) {
    return {
        static_cast<double>(c.x) * CHUNK_SIZE_XZ + static_cast<double>(l.x),
        static_cast<double>(l.y),
        static_cast<double>(c.z) * CHUNK_SIZE_XZ + static_cast<double>(l.z),
    };
}

RenderPos world_to_render(WorldPos p, WorldPos camera) {
    // f64 subtraction first, then cast. This is the only f64 -> f32 boundary.
    return {
        static_cast<float>(p.x - camera.x),
        static_cast<float>(p.y - camera.y),
        static_cast<float>(p.z - camera.z),
    };
}

WorldPos render_to_world(RenderPos p, WorldPos camera) {
    return {
        camera.x + static_cast<double>(p.x),
        camera.y + static_cast<double>(p.y),
        camera.z + static_cast<double>(p.z),
    };
}

} // namespace dh::coords