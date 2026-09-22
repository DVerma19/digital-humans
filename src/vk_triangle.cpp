// G1 milestone: triangle on screen. Same window as G0 plus graphics pipeline.
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
#include "dh/vk/shader.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int kWidth  = 1280;
constexpr int kHeight = 720;

#ifndef DH_SHADER_DIR
#define DH_SHADER_DIR "shaders"
#endif

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

    VkRenderPass      render_pass    = VK_NULL_HANDLE;
    VkPipelineLayout  pipeline_layout = VK_NULL_HANDLE;
    VkPipeline        pipeline        = VK_NULL_HANDLE;
    VkCommandPool     cmd_pool        = VK_NULL_HANDLE;

    VkCommandBuffer cmd         = VK_NULL_HANDLE;
    VkSemaphore     image_ready = VK_NULL_HANDLE;
    VkFence         in_flight   = VK_NULL_HANDLE;

    bool running = true;
};

void vk_check(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan error %d at %s\n", (int)r, what);
        std::exit(1);
    }
}

void destroy_swapchain(Renderer& r) {
    for (auto s : r.render_done_per_image)
        vkDestroySemaphore(r.device, s, nullptr);
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
        std::fprintf(stderr, "swapchain build failed: %s\n",
                     ret.error().message().c_str());
        std::exit(1);
    }
    vkb::Swapchain sc = ret.value();
    r.swapchain   = sc.swapchain;
    r.swap_format = sc.image_format;
    r.extent      = sc.extent;

    auto imgs = sc.get_images();
    auto vs   = sc.get_image_views();
    r.images = imgs.value();
    r.views  = vs.value();

    r.framebuffers.resize(r.views.size(), VK_NULL_HANDLE);
    for (size_t i = 0; i < r.views.size(); ++i) {
        VkFramebufferCreateInfo fi{};
        fi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass      = r.render_pass;
        fi.attachmentCount = 1;
        fi.pAttachments    = &r.views[i];
        fi.width           = r.extent.width;
        fi.height          = r.extent.height;
        fi.layers          = 1;
        vk_check(vkCreateFramebuffer(r.device, &fi, nullptr, &r.framebuffers[i]),
                 "vkCreateFramebuffer");
    }

    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    r.render_done_per_image.resize(r.images.size(), VK_NULL_HANDLE);
    for (auto& s : r.render_done_per_image) {
        vk_check(vkCreateSemaphore(r.device, &si, nullptr, &s),
                 "vkCreateSemaphore(render_done)");
    }
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
    r.extent = { (uint32_t)w, (uint32_t)h };
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
    dep.srcAccessMask = 0;
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
    // Load SPIR-V
    std::string shader_dir = DH_SHADER_DIR;
    auto vert_code = dh::vk::load_spirv(shader_dir + "/triangle.vert.spv");
    auto frag_code = dh::vk::load_spirv(shader_dir + "/triangle.frag.spv");

    VkShaderModule vert = dh::vk::create_shader_module(r.device, vert_code);
    VkShaderModule frag = dh::vk::create_shader_module(r.device, frag_code);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rast{};
    rast.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode    = VK_CULL_MODE_NONE;
    rast.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rast.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dyn_states;

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vk_check(vkCreatePipelineLayout(r.device, &pl, nullptr, &r.pipeline_layout),
             "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo info{};
    info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount          = 2;
    info.pStages             = stages;
    info.pVertexInputState   = &vi;
    info.pInputAssemblyState = &ia;
    info.pViewportState      = &vp;
    info.pRasterizationState = &rast;
    info.pMultisampleState   = &ms;
    info.pColorBlendState    = &cb;
    info.pDynamicState       = &dyn;
    info.layout              = r.pipeline_layout;
    info.renderPass          = r.render_pass;
    info.subpass             = 0;

    vk_check(vkCreateGraphicsPipelines(r.device, VK_NULL_HANDLE, 1, &info,
                                       nullptr, &r.pipeline),
             "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(r.device, vert, nullptr);
    vkDestroyShaderModule(r.device, frag, nullptr);
}

void init_vulkan(Renderer& r) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    r.window = SDL_CreateWindow("digital-humans | G1",
                                kWidth, kHeight,
                                SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!r.window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    uint32_t ext_count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);

    vkb::InstanceBuilder ib;
    auto inst_builder = ib
        .set_app_name("digital-humans")
        .require_api_version(1, 3, 0)
        .request_validation_layers(true)
        .use_default_debug_messenger();
    for (uint32_t i = 0; i < ext_count; ++i) {
        inst_builder.enable_extension(exts[i]);
    }
    auto instance = inst_builder.build();
    if (!instance) {
        std::fprintf(stderr, "instance build failed: %s\n",
                     instance.error().message().c_str());
        std::exit(1);
    }
    r.instance = instance.value().instance;
    r.debug    = instance.value().debug_messenger;

    if (!SDL_Vulkan_CreateSurface(r.window, r.instance, nullptr, &r.surface)) {
        std::fprintf(stderr, "SDL_Vulkan_CreateSurface failed: %s\n",
                     SDL_GetError());
        std::exit(1);
    }

    auto phys_ret = vkb::PhysicalDeviceSelector(instance.value())
        .set_surface(r.surface)
        .select();
    if (!phys_ret) {
        std::fprintf(stderr, "no suitable GPU: %s\n",
                     phys_ret.error().message().c_str());
        std::exit(1);
    }
    r.physical = phys_ret.value().physical_device;

    auto dev_ret = vkb::DeviceBuilder(phys_ret.value()).build();
    if (!dev_ret) {
        std::fprintf(stderr, "device build failed: %s\n",
                     dev_ret.error().message().c_str());
        std::exit(1);
    }
    r.device = dev_ret.value().device;

    r.graphics_q      = dev_ret.value().get_queue(vkb::QueueType::graphics).value();
    r.graphics_family = dev_ret.value().get_queue_index(vkb::QueueType::graphics).value();
    r.present_q       = dev_ret.value().get_queue(vkb::QueueType::present).value();
    r.present_family  = dev_ret.value().get_queue_index(vkb::QueueType::present).value();

    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(r.window, &w, &h);
        r.extent = { (uint32_t)w, (uint32_t)h };
    }

    r.swap_format = VK_FORMAT_B8G8R8A8_UNORM;
    create_render_pass(r);
    create_swapchain(r);
    create_pipeline(r);
}

void init_commands(Renderer& r) {
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = r.graphics_family;
    vk_check(vkCreateCommandPool(r.device, &ci, nullptr, &r.cmd_pool),
             "vkCreateCommandPool");

    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = r.cmd_pool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vk_check(vkAllocateCommandBuffers(r.device, &ai, &r.cmd),
             "vkAllocateCommandBuffers");

    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vk_check(vkCreateSemaphore(r.device, &si, nullptr, &r.image_ready),
             "vkCreateSemaphore");

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vk_check(vkCreateFence(r.device, &fi, nullptr, &r.in_flight),
             "vkCreateFence");
}

void record_command(Renderer& r, uint32_t image_index) {
    vkResetCommandBuffer(r.cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_check(vkBeginCommandBuffer(r.cmd, &bi), "vkBeginCommandBuffer");

    VkClearValue clear{};
    clear.color = {{ 0.05f, 0.07f, 0.10f, 1.0f }};

    VkRenderPassBeginInfo rp{};
    rp.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass        = r.render_pass;
    rp.framebuffer       = r.framebuffers[image_index];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = r.extent;
    rp.clearValueCount   = 1;
    rp.pClearValues      = &clear;

    vkCmdBeginRenderPass(r.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(r.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.pipeline);

    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)r.extent.width;
    vp.height   = (float)r.extent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(r.cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = r.extent;
    vkCmdSetScissor(r.cmd, 0, 1, &sc);

    vkCmdDraw(r.cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(r.cmd);

    vk_check(vkEndCommandBuffer(r.cmd), "vkEndCommandBuffer");
}

void draw_frame(Renderer& r) {
    vkWaitForFences(r.device, 1, &r.in_flight, VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult acq = vkAcquireNextImageKHR(r.device, r.swapchain, UINT64_MAX,
                                         r.image_ready, VK_NULL_HANDLE,
                                         &image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreate_swapchain(r); return; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        vk_check(acq, "vkAcquireNextImageKHR");
    }

    vkResetFences(r.device, 1, &r.in_flight);
    record_command(r, image_index);

    VkSemaphore render_done = r.render_done_per_image[image_index];

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &r.image_ready;
    submit.pWaitDstStageMask    = &wait_stage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &r.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &render_done;

    vk_check(vkQueueSubmit(r.graphics_q, 1, &submit, r.in_flight),
             "vkQueueSubmit");

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &render_done;
    present.swapchainCount     = 1;
    present.pSwapchains        = &r.swapchain;
    present.pImageIndices      = &image_index;

    VkResult pr = vkQueuePresentKHR(r.present_q, &present);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain(r);
    } else if (pr != VK_SUCCESS) {
        vk_check(pr, "vkQueuePresentKHR");
    }
}

void poll_events(Renderer& r) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            r.running = false;
        } else if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) r.running = false;
        }
    }
}

void shutdown(Renderer& r) {
    vkDeviceWaitIdle(r.device);

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
        init_vulkan(r);
        init_commands(r);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "init failed: %s\n", e.what());
        return 1;
    }

    std::printf("G1: triangle. ESC to quit.\n");
    std::fflush(stdout);

    while (r.running) {
        poll_events(r);
        draw_frame(r);
    }

    shutdown(r);
    return 0;
}