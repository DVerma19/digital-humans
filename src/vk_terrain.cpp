// G3: terrain with free camera. WASD + mouse.
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
#include "dh/vk/shader.hpp"
#include "dh/vk/math.hpp"
#include "dh/vk/camera.hpp"
#include "dh/chunk.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kWidth  = 1280;
constexpr int kHeight = 720;

constexpr int32_t GRID = 15;
constexpr int32_t S    = dh::chunk::SIZE;
constexpr int32_t N    = S * GRID;

#ifndef DH_SHADER_DIR
#define DH_SHADER_DIR "shaders"
#endif

struct Buffer {
    VkBuffer       handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t       count  = 0;
};

struct Renderer {
    SDL_Window*       window   = nullptr;
    VkInstance        instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug = VK_NULL_HANDLE;
    VkSurfaceKHR      surface  = VK_NULL_HANDLE;
    VkPhysicalDevice  physical = VK_NULL_HANDLE;
    VkDevice          device   = VK_NULL_HANDLE;
    VkQueue           graphics_q = VK_NULL_HANDLE;
    VkQueue           present_q  = VK_NULL_HANDLE;
    uint32_t          graphics_family = 0;
    uint32_t          present_family  = 0;

    VkSwapchainKHR             swapchain   = VK_NULL_HANDLE;
    VkFormat                   swap_format = VK_FORMAT_UNDEFINED;
    VkExtent2D                 extent      = {kWidth, kHeight};
    std::vector<VkImage>       images;
    std::vector<VkImageView>   views;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkSemaphore>   render_done_per_image;

    VkRenderPass      render_pass     = VK_NULL_HANDLE;
    VkPipelineLayout  pipeline_layout = VK_NULL_HANDLE;
    VkPipeline        pipeline        = VK_NULL_HANDLE;
    VkCommandPool     cmd_pool        = VK_NULL_HANDLE;

    VkCommandBuffer cmd         = VK_NULL_HANDLE;
    VkSemaphore     image_ready = VK_NULL_HANDLE;
    VkFence         in_flight   = VK_NULL_HANDLE;

    Buffer            vertex_buf;
    Buffer            index_buf;
    dh::vk::Camera    camera;
    dh::vk::Mat4      mvp;

    // Input state
    float  mouse_accum_x = 0.0f;
    float  mouse_accum_y = 0.0f;

    bool running = true;
};

void vk_check(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan error %d at %s\n", (int)r, what);
        std::exit(1);
    }
}

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

Buffer create_buffer(Renderer& r, VkDeviceSize size, VkBufferUsageFlags usage,
                     const void* data) {
    Buffer b;
    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_check(vkCreateBuffer(r.device, &bi, nullptr, &b.handle), "vkCreateBuffer");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(r.device, b.handle, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = find_memory_type(r.physical, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vk_check(vkAllocateMemory(r.device, &ai, nullptr, &b.memory), "vkAllocateMemory");
    vk_check(vkBindBufferMemory(r.device, b.handle, b.memory, 0), "vkBindBufferMemory");

    if (data) {
        void* mapped = nullptr;
        vk_check(vkMapMemory(r.device, b.memory, 0, size, 0, &mapped), "vkMapMemory");
        std::memcpy(mapped, data, size);
        vkUnmapMemory(r.device, b.memory);
    }
    return b;
}

void build_terrain_mesh(Renderer& r, uint64_t seed, uint16_t version,
                        dh::coords::ChunkAddress origin) {
    std::vector<float> elev(static_cast<size_t>(N) * N, 0.0f);
    for (int32_t cz = 0; cz < GRID; ++cz) {
        for (int32_t cx = 0; cx < GRID; ++cx) {
            dh::chunk::Chunk c;
            c.address = { origin.x + cx, origin.z + cz };
            c.generation_version = version;
            dh::chunk::generate(c, seed);
            for (int32_t lz = 0; lz < S; ++lz) {
                for (int32_t lx = 0; lx < S; ++lx) {
                    const int32_t gx = cx * S + lx;
                    const int32_t gz = cz * S + lz;
                    elev[static_cast<size_t>(gz) * N + gx] =
                        c.elevation[lz * S + lx];
                }
            }
        }
    }

    float  min_e = 1e30f, max_e = -1e30f;
    double sum_e = 0.0;
    for (float e : elev) {
        if (e < min_e) min_e = e;
        if (e > max_e) max_e = e;
        sum_e += static_cast<double>(e);
    }
    const float avg_e = static_cast<float>(sum_e / static_cast<double>(elev.size()));
    std::fprintf(stderr, "[terrain] elevation: min=%.1f max=%.1f avg=%.1f range=%.1f\n",
                 min_e, max_e, avg_e, max_e - min_e);

    std::vector<float> verts;
    verts.reserve(static_cast<size_t>(N) * N * 6);
    const float base_x = static_cast<float>(origin.x * static_cast<int32_t>(dh::coords::CHUNK_SIZE_XZ));
    const float base_z = static_cast<float>(origin.z * static_cast<int32_t>(dh::coords::CHUNK_SIZE_XZ));

    auto sample_elev = [&](int32_t x, int32_t z) -> float {
        if (x < 0) x = 0;
        if (z < 0) z = 0;
        if (x >= N) x = N - 1;
        if (z >= N) z = N - 1;
        return elev[static_cast<size_t>(z) * N + x];
    };

    for (int32_t gz = 0; gz < N; ++gz) {
        for (int32_t gx = 0; gx < N; ++gx) {
            const float hL = sample_elev(gx - 1, gz);
            const float hR = sample_elev(gx + 1, gz);
            const float hD = sample_elev(gx, gz - 1);
            const float hU = sample_elev(gx, gz + 1);
            float nx = (hL - hR) * 0.5f;
            float nz = (hD - hU) * 0.5f;
            float ny = 1.0f;
            const float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            nx /= len; ny /= len; nz /= len;

            verts.push_back(base_x + static_cast<float>(gx));
            verts.push_back(elev[static_cast<size_t>(gz) * N + gx]);
            verts.push_back(base_z + static_cast<float>(gz));
            verts.push_back(nx);
            verts.push_back(ny);
            verts.push_back(nz);
        }
    }

    std::vector<uint32_t> idx;
    idx.reserve(static_cast<size_t>(N - 1) * (N - 1) * 6);
    for (int32_t gz = 0; gz < N - 1; ++gz) {
        for (int32_t gx = 0; gx < N - 1; ++gx) {
            const uint32_t v00 = static_cast<uint32_t>(gz * N + gx);
            const uint32_t v10 = v00 + 1;
            const uint32_t v01 = v00 + static_cast<uint32_t>(N);
            const uint32_t v11 = v01 + 1;
            idx.push_back(v00); idx.push_back(v01); idx.push_back(v10);
            idx.push_back(v10); idx.push_back(v01); idx.push_back(v11);
        }
    }

    r.vertex_buf = create_buffer(r, verts.size() * sizeof(float),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data());
    r.vertex_buf.count = static_cast<uint32_t>(verts.size() / 6);

    r.index_buf = create_buffer(r, idx.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, idx.data());
    r.index_buf.count = static_cast<uint32_t>(idx.size());

    // Camera initial position: off to a corner, looking at grid center.
    const float cx = base_x + static_cast<float>(N / 2);
    const float cz = base_z + static_cast<float>(N / 2);
    const float dist = static_cast<float>(N) * 0.6f;
    r.camera.x = cx + dist;
    r.camera.y = avg_e + dist * 0.7f;
    r.camera.z = cz + dist;
    r.camera.look_at_world(cx, avg_e, cz);
}

void destroy_swapchain(Renderer& r) {
    for (auto s : r.render_done_per_image) vkDestroySemaphore(r.device, s, nullptr);
    r.render_done_per_image.clear();
    for (auto fb : r.framebuffers) vkDestroyFramebuffer(r.device, fb, nullptr);
    for (auto v : r.views)         vkDestroyImageView(r.device, v, nullptr);
    r.framebuffers.clear();
    r.views.clear();
    r.images.clear();
    if (r.swapchain) vkDestroySwapchainKHR(r.device, r.swapchain, nullptr);
    r.swapchain = VK_NULL_HANDLE;
}

void create_swapchain(Renderer& r) {
    vkb::SwapchainBuilder builder(r.physical, r.device, r.surface);
    auto ret = builder
        .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM,
                             VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(r.extent.width, r.extent.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build();
    if (!ret) {
        std::fprintf(stderr, "swapchain: %s\n", ret.error().message().c_str());
        std::exit(1);
    }
    vkb::Swapchain sc = ret.value();
    r.swapchain   = sc.swapchain;
    r.swap_format = sc.image_format;
    r.extent      = sc.extent;
    r.images      = sc.get_images().value();
    r.views       = sc.get_image_views().value();
    r.framebuffers.resize(r.views.size(), VK_NULL_HANDLE);
    for (size_t i = 0; i < r.views.size(); ++i) {
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = r.render_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &r.views[i];
        fi.width  = r.extent.width;
        fi.height = r.extent.height;
        fi.layers = 1;
        vk_check(vkCreateFramebuffer(r.device, &fi, nullptr, &r.framebuffers[i]),
                 "vkCreateFramebuffer");
    }
    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    r.render_done_per_image.resize(r.images.size(), VK_NULL_HANDLE);
    for (auto& s : r.render_done_per_image)
        vk_check(vkCreateSemaphore(r.device, &si, nullptr, &s), "vkCreateSemaphore");
}

void recreate_swapchain(Renderer& r) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(r.window, &w, &h);
    while (w == 0 || h == 0) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        SDL_GetWindowSizeInPixels(r.window, &w, &h);
        SDL_Delay(16);
    }
    vkDeviceWaitIdle(r.device);
    destroy_swapchain(r);
    r.extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    create_swapchain(r);
}

void create_render_pass(Renderer& r) {
    VkAttachmentDescription color{};
    color.format         = r.swap_format;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments    = &color;
    info.subpassCount    = 1;
    info.pSubpasses      = &sub;
    info.dependencyCount = 1;
    info.pDependencies   = &dep;
    vk_check(vkCreateRenderPass(r.device, &info, nullptr, &r.render_pass),
             "vkCreateRenderPass");
}

void create_pipeline(Renderer& r) {
    const std::string dir = DH_SHADER_DIR;
    auto vert_code = dh::vk::load_spirv(dir + "/terrain.vert.spv");
    auto frag_code = dh::vk::load_spirv(dir + "/terrain.frag.spv");
    VkShaderModule vert = dh::vk::create_shader_module(r.device, vert_code);
    VkShaderModule frag = dh::vk::create_shader_module(r.device, frag_code);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert; stages[0].pName = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag; stages[1].pName = "main";

    VkVertexInputBindingDescription bind{};
    bind.binding = 0; bind.stride = 6 * sizeof(float);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0].location = 0; attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[0].offset = 0;
    attrs[1].location = 1; attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = 3 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1; vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rast{};
    rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_BACK_BIT;
    rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rast.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2; dyn.pDynamicStates = dyn_states;

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc_range.offset = 0; pc_range.size = sizeof(dh::vk::Mat4);

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pushConstantRangeCount = 1; pl.pPushConstantRanges = &pc_range;
    vk_check(vkCreatePipelineLayout(r.device, &pl, nullptr, &r.pipeline_layout),
             "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2; info.pStages = stages;
    info.pVertexInputState = &vi;
    info.pInputAssemblyState = &ia;
    info.pViewportState = &vp;
    info.pRasterizationState = &rast;
    info.pMultisampleState = &ms;
    info.pColorBlendState = &cb;
    info.pDynamicState = &dyn;
    info.layout = r.pipeline_layout;
    info.renderPass = r.render_pass; info.subpass = 0;
    vk_check(vkCreateGraphicsPipelines(r.device, VK_NULL_HANDLE, 1, &info,
                                       nullptr, &r.pipeline), "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(r.device, vert, nullptr);
    vkDestroyShaderModule(r.device, frag, nullptr);
}

void init_vulkan(Renderer& r, uint64_t seed, uint16_t version,
                 dh::coords::ChunkAddress origin) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        std::exit(1);
    }
    r.window = SDL_CreateWindow("digital-humans | G3",
                                kWidth, kHeight,
                                SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!r.window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        std::exit(1);
    }

    uint32_t ext_count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);

    vkb::InstanceBuilder ib;
    auto ibld = ib.set_app_name("digital-humans")
                   .require_api_version(1, 3, 0)
                   .request_validation_layers(true)
                   .use_default_debug_messenger();
    for (uint32_t i = 0; i < ext_count; ++i) ibld.enable_extension(exts[i]);
    auto instance = ibld.build();
    if (!instance) {
        std::fprintf(stderr, "instance: %s\n", instance.error().message().c_str());
        std::exit(1);
    }
    r.instance = instance.value().instance;
    r.debug    = instance.value().debug_messenger;

    if (!SDL_Vulkan_CreateSurface(r.window, r.instance, nullptr, &r.surface)) {
        std::fprintf(stderr, "surface: %s\n", SDL_GetError());
        std::exit(1);
    }

    auto phys_ret = vkb::PhysicalDeviceSelector(instance.value())
                        .set_surface(r.surface)
                        .select();
    if (!phys_ret) {
        std::fprintf(stderr, "phys: %s\n", phys_ret.error().message().c_str());
        std::exit(1);
    }
    r.physical = phys_ret.value().physical_device;

    auto dev_ret = vkb::DeviceBuilder(phys_ret.value()).build();
    if (!dev_ret) {
        std::fprintf(stderr, "dev: %s\n", dev_ret.error().message().c_str());
        std::exit(1);
    }
    r.device          = dev_ret.value().device;
    r.graphics_q      = dev_ret.value().get_queue(vkb::QueueType::graphics).value();
    r.graphics_family = dev_ret.value().get_queue_index(vkb::QueueType::graphics).value();
    r.present_q       = dev_ret.value().get_queue(vkb::QueueType::present).value();
    r.present_family  = dev_ret.value().get_queue_index(vkb::QueueType::present).value();

    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(r.window, &w, &h);
        r.extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    }

    r.swap_format = VK_FORMAT_B8G8R8A8_UNORM;
    create_render_pass(r);
    create_swapchain(r);
    create_pipeline(r);
    build_terrain_mesh(r, seed, version, origin);

    // Capture the mouse for camera look.
    SDL_SetWindowRelativeMouseMode(r.window, true);
}

void init_commands(Renderer& r) {
    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = r.graphics_family;
    vk_check(vkCreateCommandPool(r.device, &ci, nullptr, &r.cmd_pool), "pool");

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = r.cmd_pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vk_check(vkAllocateCommandBuffers(r.device, &ai, &r.cmd), "cmdbuf");

    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vk_check(vkCreateSemaphore(r.device, &si, nullptr, &r.image_ready), "sem");

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vk_check(vkCreateFence(r.device, &fi, nullptr, &r.in_flight), "fence");
}

void record_command(Renderer& r, uint32_t image_index) {
    vkResetCommandBuffer(r.cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_check(vkBeginCommandBuffer(r.cmd, &bi), "begin");

    VkClearValue clear{};
    clear.color = {{ 0.05f, 0.07f, 0.10f, 1.0f }};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = r.render_pass;
    rp.framebuffer = r.framebuffers[image_index];
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = r.extent;
    rp.clearValueCount = 1; rp.pClearValues = &clear;
    vkCmdBeginRenderPass(r.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(r.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.pipeline);
    vkCmdPushConstants(r.cmd, r.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(dh::vk::Mat4), r.mvp.m);

    VkViewport vp{};
    vp.width = static_cast<float>(r.extent.width);
    vp.height = static_cast<float>(r.extent.height);
    vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
    vkCmdSetViewport(r.cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.extent = r.extent;
    vkCmdSetScissor(r.cmd, 0, 1, &sc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(r.cmd, 0, 1, &r.vertex_buf.handle, &offset);
    vkCmdBindIndexBuffer(r.cmd, r.index_buf.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(r.cmd, r.index_buf.count, 1, 0, 0, 0);

    vkCmdEndRenderPass(r.cmd);
    vk_check(vkEndCommandBuffer(r.cmd), "end");
}

void draw_frame(Renderer& r) {
    vkWaitForFences(r.device, 1, &r.in_flight, VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult acq = vkAcquireNextImageKHR(r.device, r.swapchain, UINT64_MAX,
                                         r.image_ready, VK_NULL_HANDLE,
                                         &image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreate_swapchain(r); return; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) vk_check(acq, "acquire");

    vkResetFences(r.device, 1, &r.in_flight);
    record_command(r, image_index);

    VkSemaphore rd = r.render_done_per_image[image_index];
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &r.image_ready;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1; submit.pCommandBuffers = &r.cmd;
    submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &rd;
    vk_check(vkQueueSubmit(r.graphics_q, 1, &submit, r.in_flight), "submit");

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1; present.pWaitSemaphores = &rd;
    present.swapchainCount = 1; present.pSwapchains = &r.swapchain;
    present.pImageIndices = &image_index;
    VkResult pr = vkQueuePresentKHR(r.present_q, &present);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) recreate_swapchain(r);
    else if (pr != VK_SUCCESS) vk_check(pr, "present");
}

void poll_events(Renderer& r) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            r.running = false;
        } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            r.running = false;
        } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
            r.mouse_accum_x += e.motion.xrel;
            r.mouse_accum_y += e.motion.yrel;
        }
    }
}

void update_camera(Renderer& r, float dt) {
    // Look
    if (r.mouse_accum_x != 0.0f || r.mouse_accum_y != 0.0f) {
        r.camera.rotate(r.mouse_accum_x, r.mouse_accum_y);
        r.mouse_accum_x = 0.0f;
        r.mouse_accum_y = 0.0f;
    }

    // Move
    float forward = 0.0f, right = 0.0f, up = 0.0f;
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_W]) forward += 1.0f;
    if (keys[SDL_SCANCODE_S]) forward -= 1.0f;
    if (keys[SDL_SCANCODE_D]) right   += 1.0f;
    if (keys[SDL_SCANCODE_A]) right   -= 1.0f;
    if (keys[SDL_SCANCODE_E]) up      += 1.0f;
    if (keys[SDL_SCANCODE_Q]) up      -= 1.0f;
    if (keys[SDL_SCANCODE_LSHIFT]) r.camera.speed = 900.0f;
    else                           r.camera.speed = 300.0f;

    if (forward != 0.0f || right != 0.0f || up != 0.0f) {
        r.camera.move(forward, right, up, dt);
    }

    // Rebuild MVP
    const dh::vk::Mat4 view = r.camera.view();
    const dh::vk::Mat4 proj = dh::vk::perspective_vk(
        60.0f * 3.14159265f / 180.0f,
        static_cast<float>(r.extent.width) / static_cast<float>(r.extent.height),
        1.0f, 20000.0f);
    r.mvp = dh::vk::mul(proj, view);
}

void shutdown(Renderer& r) {
    vkDeviceWaitIdle(r.device);

    vkDestroyBuffer(r.device, r.vertex_buf.handle, nullptr);
    vkFreeMemory(r.device, r.vertex_buf.memory, nullptr);
    vkDestroyBuffer(r.device, r.index_buf.handle, nullptr);
    vkFreeMemory(r.device, r.index_buf.memory, nullptr);

    vkDestroyPipeline(r.device, r.pipeline, nullptr);
    vkDestroyPipelineLayout(r.device, r.pipeline_layout, nullptr);
    vkDestroyFence(r.device, r.in_flight, nullptr);
    vkDestroySemaphore(r.device, r.image_ready, nullptr);
    vkDestroyCommandPool(r.device, r.cmd_pool, nullptr);
    destroy_swapchain(r);
    vkDestroyRenderPass(r.device, r.render_pass, nullptr);
    vkDestroyDevice(r.device, nullptr);
    vkDestroySurfaceKHR(r.instance, r.surface, nullptr);
    if (r.debug) {
        auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(r.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) fn(r.instance, r.debug, nullptr);
    }
    vkDestroyInstance(r.instance, nullptr);
    SDL_DestroyWindow(r.window);
    SDL_Quit();
}

} // namespace

int main(int, char**) {
    Renderer r;
    try {
        init_vulkan(r, 42, 1, { -7, -7 });
        init_commands(r);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "init failed: %s\n", e.what());
        return 1;
    }

    std::printf("G3: terrain + camera. WASD move, QE up/down, mouse look, "
                "Shift=fast, ESC=quit.\n");
    std::fflush(stdout);

    Uint64 prev = SDL_GetTicks();

    while (r.running) {
        poll_events(r);

        const Uint64 now = SDL_GetTicks();
        float dt = static_cast<float>(now - prev) / 1000.0f;
        prev = now;
        if (dt > 0.1f) dt = 0.1f;   // clamp after a stall

        update_camera(r, dt);
        draw_frame(r);
    }
    shutdown(r);
    return 0;
}