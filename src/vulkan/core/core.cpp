#include "core.hpp"

#include <chrono>

#include <VkBootstrap.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "../vkutil.hpp"

namespace vke
{

struct Core::Data
{
    vkb::Instance vkb_instance;
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkb::Swapchain vkb_swapchain;
};

const uint32_t vk_ver_major = 1;
const uint32_t vk_ver_minor = 2;
const uint32_t vk_ver_patch = 0;

Core::Core(uint32_t width, uint32_t height, const std::string& app_name)
{
    m_data = std::make_unique<Core::Data>();

    m_data->vkb_instance =
        vkb::InstanceBuilder()
            .set_app_name(app_name.c_str())
            .require_api_version(vk_ver_major, vk_ver_minor, vk_ver_patch)
#ifndef NDEBUG
            .request_validation_layers(true)
    // .set_debug_callback(validation_callback)
    // .set_debug_callback_user_data_pointer(m_data)
#endif
            .build()
            .value();

    m_instance = m_data->vkb_instance.instance;

    // window creation
    //
    m_width  = width;
    m_height = height;

    m_window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_width, m_height, (SDL_WindowFlags)(SDL_WINDOW_VULKAN));

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Vulkan_CreateSurface(m_window, instance(), &m_surface);

    // device
    //
    VkPhysicalDeviceFeatures req_features  = {};
    req_features.fillModeNonSolid          = true;
    req_features.multiDrawIndirect         = true;
    req_features.drawIndirectFirstInstance = true;

    VkPhysicalDeviceVulkan12Features req_features12 = {};
    req_features12.drawIndirectCount                = true;

    vkb::PhysicalDeviceSelector selector{m_data->vkb_instance};

    if (m_surface != nullptr) selector.set_surface(m_surface);
    vkb::PhysicalDevice physical_device =
        selector
            .set_minimum_version(vk_ver_major, vk_ver_minor)
            .set_required_features(req_features)
            .set_required_features_12(req_features12)
            .select()
            .value();

    vkb::DeviceBuilder device_builder{physical_device};

    vkb::Device vkb_device = device_builder.build().value();

    // Get the VkDevice handle used in the rest of a Vulkan application
    m_device     = vkb_device.device;
    m_chosen_gpu = physical_device.physical_device;

    // std::cout << physical_device.properties.deviceName << '\n';

    // use vkbootstrap to get a Graphics queue
    m_graphics_queue        = vkb_device.get_queue(vkb::QueueType::graphics).value();
    m_graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // init other stuff
    vkGetPhysicalDeviceMemoryProperties(m_chosen_gpu, &m_data->mem_properties);

    init_allocator();
}

Core::~Core()
{
    cleanup_allocator();

    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void Core::init_swapchain(VkSwapchainKHR old_swapchain)
{
    vkb::Swapchain vkb_swapchain =
        vkb::SwapchainBuilder(m_chosen_gpu, device(), m_surface)
            .use_default_format_selection()
            // use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(m_width, m_height)
            .set_old_swapchain(old_swapchain)
            .build()
            .value();

    m_swapchain_image_format = vkb_swapchain.image_format;
    m_swapchain              = vkb_swapchain.swapchain;
    m_swapchain_image_views  = vkb_swapchain.get_image_views().value();
    m_swapchain_images       = vkb_swapchain.get_images().value();

    // m_data->vkb_swapchain = std::move(vkb_swapchain);
}

void Core::cleanup_swapchain()
{

    vkDestroySwapchainKHR(device(), m_swapchain, nullptr);

    // destroy swapchain resources
    for (auto& iv : m_swapchain_image_views)
        vkDestroyImageView(device(), iv, nullptr);
}

VkAttachmentDescription Core::get_color_attachment()
{
    // the renderpass will use this color attachment.
    VkAttachmentDescription color_attachment = {};
    // the attachment will have the format needed by the swapchain
    color_attachment.format = m_swapchain_image_format;
    // 1 sample, we won't be doing MSAA
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // we Clear when this attachment is loaded
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // we keep the attachment stored when the renderpass ends
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // we don't care about stencil
    color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // we don't know or care about the starting layout of the attachment
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // after the renderpass ends, the image has to be on a layout ready for display
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return color_attachment;
}

void Core::run(std::function<void(FrameArgs)> frame_draw)
{
    m_running = true;

    auto timer_begin = std::chrono::steady_clock::now();

    double delta_t       = 1.0f / 60;
    double frame_timer   = 0.0;
    double frame_counter = 0.0;

    while (m_running)
    {
        handle_input();
        draw_frame(delta_t,frame_draw);

        auto timer_end = std::chrono::steady_clock::now();

        delta_t = static_cast<double>((timer_end - timer_begin).count()) / 1'000'000'000;

        frame_timer += delta_t;
        frame_counter++;

        if (frame_timer > 1.0)
        {
            m_fps = frame_counter / frame_timer;

            frame_timer   = 0;
            frame_counter = 0;
        }

        timer_begin = timer_end;
    }
}

void Core::handle_input()
{
    m_mouse_delta_x = m_mouse_delta_y = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT:
            m_running = false;
            break;
        case SDL_KEYDOWN:
            m_keystates[e.key.keysym.sym] = true;
            if (auto it = m_on_key_down.find(e.key.keysym.sym); it != m_on_key_down.end()) it->second();
            break;
        case SDL_KEYUP:
            m_keystates[e.key.keysym.sym] = false;
            break;

        case SDL_MOUSEBUTTONDOWN:
            m_mouse_button_states[e.button.button] = true;
            if (auto it = m_on_mouse_click.find(e.button.button); it != m_on_mouse_click.end()) it->second();

            break;

        case SDL_MOUSEBUTTONUP:
            m_mouse_button_states[e.button.button] = false;
            break;
        case SDL_MOUSEMOTION:

            m_mouse_delta_x = m_mouse_x - e.motion.x;
            m_mouse_delta_y = m_mouse_y - e.motion.y;

            m_mouse_x = e.motion.x;
            m_mouse_y = e.motion.y;
            break;
        }
    }

    if (m_mouse_captured)
    {

        SDL_WarpMouseInWindow(m_window, width() / 2, height() / 2);
        m_mouse_x = (float)width() / 2;
        m_mouse_y = (float)height() / 2;
    }
}

void begin_cmd(VkCommandBuffer cmd)
{
    VkCommandBufferBeginInfo cmd_begin_info = {};
    cmd_begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.pNext                    = nullptr;

    cmd_begin_info.pInheritanceInfo = nullptr;
    cmd_begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
}

void Core::draw_frame(float delta_t, std::function<void(FrameArgs)>& frame_func)
{
    auto& current_frame     = get_current_frame();
    const uint64_t time_out = 10'000'000'000;//10 sec

    VK_CHECK(vkWaitForFences(device(), 1, &current_frame.render_fence, true, time_out));
    VK_CHECK(vkResetFences(device(), 1, &current_frame.render_fence));

    uint32_t swapchain_image_index;
    VK_CHECK(vkAcquireNextImageKHR(device(), m_swapchain, time_out, current_frame.present_semaphore, nullptr, &swapchain_image_index));

    VkCommandBuffer cmd = nullptr;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    begin_cmd(cmd);

    std::vector<std::function<void()>> cleanup_queue;

    FrameArgs frame_args{
        .delta_t       = delta_t,
        .frame_index   = 0,
        .cmd           = cmd,
        .cleanup_queue = cleanup_queue,
    };
    frame_func(frame_args);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {};
    submit.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext        = nullptr;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &wait_stage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores    = &current_frame.present_semaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &current_frame.render_semaphore;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    // submit command buffer to the queue and execute it.
    VK_CHECK(vkQueueSubmit(m_graphics_queue, 1, &submit, current_frame.render_fence));
}

} // namespace vke