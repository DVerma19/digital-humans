#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "dh/coords.hpp"
#include "dh/vk/buffer.hpp"
#include "dh/vk/mesh_data.hpp"
#include "dh/hydro.hpp"

namespace dh::vk {

struct RenderChunk {
    coords::ChunkAddress addr{0, 0};
    GpuBuffer vertex;
    GpuBuffer index;
};

// CPU-only: build vertex/index arrays from chunk data. Thread-safe, no Vulkan.
// Can be called from worker threads.
MeshData build_mesh_data(coords::ChunkAddress addr,
                         uint64_t seed, uint16_t version,
                         const dh::hydro::BasinGrid* basin,
                         uint16_t lod);

// Main thread only: upload MeshData to GPU.
RenderChunk upload_mesh_data(VkDevice device, VkPhysicalDevice phys,
                             const MeshData& md);

void destroy_render_chunk(VkDevice device, RenderChunk& rc);

} // namespace dh::vk