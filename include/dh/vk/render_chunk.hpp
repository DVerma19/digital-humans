#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "dh/coords.hpp"
#include "dh/vk/buffer.hpp"
#include "dh/hydro.hpp"

namespace dh::vk {

struct RenderChunk {
    coords::ChunkAddress addr{0, 0};
    GpuBuffer vertex;
    GpuBuffer index;
};

// Build a GPU mesh for a single chunk with water data.
// Vertex format: (x, y, z, nx, ny, nz, river_flow, lake_depth) floats.
// basin may be null; if so, water fields are zero.
RenderChunk create_render_chunk(VkDevice device, VkPhysicalDevice phys,
                                coords::ChunkAddress addr,
                                uint64_t seed, uint16_t version,
                                const dh::hydro::BasinGrid* basin);

void destroy_render_chunk(VkDevice device, RenderChunk& rc);

} // namespace dh::vk