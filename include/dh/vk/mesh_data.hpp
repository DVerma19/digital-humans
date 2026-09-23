#pragma once
#include <cstdint>
#include <vector>
#include "dh/coords.hpp"

namespace dh::vk {

// CPU-side mesh data. Workers fill this. Main thread uploads it to GPU.
struct MeshData {
    coords::ChunkAddress addr{0, 0};
    std::vector<float>    vertices;   // 9 floats per vertex
    std::vector<uint32_t> indices;
    uint32_t              vertex_count = 0;
    uint32_t              index_count  = 0;
};

} // namespace dh::vk