#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "dh/coords.hpp"
#include "dh/vk/buffer.hpp"

namespace dh::vk {

struct RenderChunk {
    coords::ChunkAddress addr{0, 0};
    GpuBuffer vertex;
    GpuBuffer index;
};

// Build a GPU mesh for a single chunk, using a 1-cell halo for correct
// boundary normals. Vertex format: (x, y, z, nx, ny, nz) floats, world coords.
RenderChunk create_render_chunk(VkDevice device, VkPhysicalDevice phys,
                                coords::ChunkAddress addr,
                                uint64_t seed, uint16_t version);

void destroy_render_chunk(VkDevice device, RenderChunk& rc);

} // namespace dh::vk