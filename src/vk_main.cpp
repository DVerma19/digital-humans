// G0 milestone: SDL3 window + Vulkan swapchain + solid-color clear.
// No terrain. No pipeline. Just proving the toolchain works.
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int kWidth  = 1280;
constexpr int kHeight = 720;

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

    // One render_done semaphore per swapchain image.
    std::vector<VkSemaphore> render_done_per_image;

    VkRenderPass  render_pass = VK_NULL_HANDLE;
    VkCommandPool cmd_pool    = VK_NULL_HANDLE;

    // One frame in flight.
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
    if (!imgs || !vs) {
        std::fprintf(stderr, "swapchain images failed\n");
        std::exit(1);
    }
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

void init_vulkan(Renderer& r) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    r.window = SDL_CreateWindow("digital-humans | G0",
                                kWidth, kHeight,
                                SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!r.window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        std::exit(1);
    }

    uint32_t ext_count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);
    if (!exts || ext_count == 0) {
        std::fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions failed: %s\n",
                     SDL_GetError());
        std::exit(1);
    }

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
    init_vulkan(r);
    init_commands(r);

    std::printf("G0: window open, swapchain %ux%u. ESC to quit.\n",
                r.extent.width, r.extent.height);
    std::fflush(stdout);

    while (r.running) {
        poll_events(r);
        draw_frame(r);
    }

    shutdown(r);
    return 0;
}