#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

struct SDL_Window;

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

namespace vke
{

class Core;

class Buffer
{
    friend Core;

public:
    VkBuffer buffer;

public:
    void clean_up();
    inline void* get_data() { return m_mapped_data; }

private:
    VmaAllocation m_allocation;
    VmaAllocator m_allocator = nullptr;
    void* m_mapped_data      = nullptr;
};

class Core
{
public:
    Core(uint32_t width, uint32_t height, const std::string& app_name);
    ~Core();

    //
    inline size_t gpu_allignment() { return 256; }

    inline auto instance() { return m_instance; }
    inline auto device() { return m_device; }
    inline VkPipelineCache pipeline_cache() { return nullptr; }

    auto width() { return m_width; }
    auto height() { return m_height; }

    VkAttachmentDescription get_color_attachment();

    // buffers
    Buffer allocate_buffer(VkBufferUsageFlagBits usage, uint32_t buffer_size, bool host_visible);

    struct FrameArgs
    {
        float delta_t;
        uint32_t frame_index;
        VkCommandBuffer cmd;
        std::vector<std::function<void()>>& cleanup_queue;
    };

    void run(std::function<void(FrameArgs)>);

private:
    void handle_input();
    void draw_frame(float delta_t, std::function<void(FrameArgs)>& frame_func);

    void init_swapchain(VkSwapchainKHR old_swapchain = nullptr);
    void cleanup_swapchain();

    void init_allocator();
    void cleanup_allocator();

private:
    struct Data;
    std::unique_ptr<Data> m_data;

    VkInstance m_instance;
    VkDevice m_device;
    VkPhysicalDevice m_chosen_gpu;

    uint32_t m_graphics_queue_family; // family of that queue
    VkQueue m_graphics_queue;         // queue we will submit to

    struct FrameData
    {
        VkCommandBuffer cmd;
        VkSemaphore present_semaphore, render_semaphore;
        VkFence render_fence;
    };

    std::vector<FrameData> m_frame_data;
    uint32_t m_frame_index = 0;
    inline FrameData& get_current_frame() { return m_frame_data[m_frame_index]; }

    // window
    VkSurfaceKHR m_surface;
    VkFormat m_swapchain_image_format;
    VkSwapchainKHR m_swapchain;
    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;

    SDL_Window* m_window = nullptr;
    uint32_t m_height, m_width;



    // buffers and memory
    VmaAllocator m_allocator = nullptr;

    // input
    float m_mouse_x, m_mouse_y, m_mouse_delta_x, m_mouse_delta_y;
    bool m_mouse_captured;
    std::unordered_map<uint32_t, bool> m_keystates;
    std::unordered_map<uint32_t, bool> m_mouse_button_states;
    std::unordered_map<uint32_t, std::function<void()>> m_on_mouse_click;
    std::unordered_map<uint32_t, std::function<void()>> m_on_key_down;

    // stats
    bool m_running = true;
    float m_fps    = 0;
};

} // namespacze vke