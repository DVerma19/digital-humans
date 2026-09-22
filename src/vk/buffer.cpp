#include "dh/vk/buffer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace dh::vk {

namespace {

uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t type_bits,
                          VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw std::runtime_error("no suitable memory type");
}

void vk_check(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan error %d at %s\n", (int)r, what);
        std::exit(1);
    }
}

} // namespace

GpuBuffer create_buffer(VkDevice device, VkPhysicalDevice phys,
                        VkDeviceSize size, VkBufferUsageFlags usage,
                        const void* data) {
    GpuBuffer b;
    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_check(vkCreateBuffer(device, &bi, nullptr, &b.handle), "vkCreateBuffer");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, b.handle, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = find_memory_type(phys, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vk_check(vkAllocateMemory(device, &ai, nullptr, &b.memory), "vkAllocateMemory");
    vk_check(vkBindBufferMemory(device, b.handle, b.memory, 0), "vkBindBufferMemory");

    if (data) {
        void* mapped = nullptr;
        vk_check(vkMapMemory(device, b.memory, 0, size, 0, &mapped), "vkMapMemory");
        std::memcpy(mapped, data, size);
        vkUnmapMemory(device, b.memory);
    }
    return b;
}

void destroy_buffer(VkDevice device, GpuBuffer& b) {
    if (b.handle) { vkDestroyBuffer(device, b.handle, nullptr); b.handle = VK_NULL_HANDLE; }
    if (b.memory) { vkFreeMemory(device, b.memory, nullptr);    b.memory = VK_NULL_HANDLE; }
    b.count = 0;
}

} // namespace dh::vk