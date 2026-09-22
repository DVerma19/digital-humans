#include "dh/vk/shader.hpp"
#include <fstream>
#include <stdexcept>

namespace dh::vk {

std::vector<uint32_t> load_spirv(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        throw std::runtime_error("cannot open SPIR-V file: " + path);
    }
    const std::streamsize size = f.tellg();
    if (size <= 0 || (size % 4) != 0) {
        throw std::runtime_error("invalid SPIR-V size: " + path);
    }
    std::vector<uint32_t> code(static_cast<size_t>(size) / 4u);
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(code.data()), size);
    if (!f) {
        throw std::runtime_error("failed to read SPIR-V: " + path);
    }
    return code;
}

VkShaderModule create_shader_module(VkDevice device,
                                    const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule failed");
    }
    return mod;
}

} // namespace dh::vk