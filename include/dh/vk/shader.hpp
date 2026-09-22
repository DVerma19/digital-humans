#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace dh::vk {

// Load a SPIR-V file into a byte buffer. Throws std::runtime_error on failure.
std::vector<uint32_t> load_spirv(const std::string& path);

// Create a shader module from a SPIR-V byte buffer. Caller must destroy.
VkShaderModule create_shader_module(VkDevice device,
                                    const std::vector<uint32_t>& code);

} // namespace dh::vk