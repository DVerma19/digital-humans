#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>

namespace dh::vk {

struct GpuBuffer {
    VkBuffer       handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t       count  = 0;   // user-defined element count
};

// Create a host-visible, host-coherent buffer, copying `data` into it.
GpuBuffer create_buffer(VkDevice device, VkPhysicalDevice phys,
                        VkDeviceSize size, VkBufferUsageFlags usage,
                        const void* data);

void destroy_buffer(VkDevice device, GpuBuffer& b);

} // namespace dh::vk