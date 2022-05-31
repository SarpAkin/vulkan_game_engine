#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include <vulkan/vulkan.h>

#include "../iinput.hpp"

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
    const VkBuffer& buffer() const { return m_buffer; }
    auto size() const { return m_buffer_size; }

    void clean_up();

    template <typename T = void>
    inline T* get_data()
    {
        return reinterpret_cast<T*>(m_mapped_data);
    }

    template <typename T = void>
    inline T* get_data_end()
    {
        return reinterpret_cast<T*>(m_mapped_data) + (m_buffer_size / sizeof(T));
    }

private:
    VkBuffer m_buffer;
    VmaAllocation m_allocation;
    VmaAllocator m_allocator = nullptr;
    void* m_mapped_data      = nullptr;
    size_t m_buffer_size     = 0;
};

class Image
{
    friend Core;

public:
    VkImage image;
    VkImageView view;

public:
    void clean_up();
    inline void* get_data() { return m_mapped_data; }

private:
    VmaAllocation m_allocation;
    Core* m_core        = nullptr;
    void* m_mapped_data = nullptr;
};

class RenderPass;

class Core : public IInput
{
public:
    Core(uint32_t width, uint32_t height, const std::string& app_name);
    ~Core();

    constexpr static int FRAME_OVERLAP = 2;

    //
    inline size_t gpu_allignment()const { return 256; }
    size_t pad_buffer(size_t bsize) const;

    inline auto instance() { return m_instance; }
    inline auto device() { return m_device; }
    inline VkPipelineCache pipeline_cache() { return m_pipeline_cache; }

    auto width() { return m_width; }
    auto height() { return m_height; }

    VkAttachmentDescription get_color_attachment();

    // createing stuff
    VkCommandPool create_command_pool(VkCommandPoolCreateFlags flags);
    VkCommandBuffer create_command_buffer(VkCommandPool pool, VkCommandBufferLevel level);
    VkFence create_fence(bool signalled = false);
    VkSemaphore create_semaphore();
    VkSampler create_sampler(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // buffers
    std::unique_ptr<Buffer> allocate_buffer(VkBufferUsageFlagBits usage, uint32_t buffer_size, bool host_visible);
    std::unique_ptr<Image> allocate_image(VkFormat format, VkImageUsageFlags usageFlags, uint32_t width, uint32_t height, bool cpu_read_write);
    std::unique_ptr<Image> load_png(const char* path, VkCommandBuffer cmd, std::vector<std::function<void()>>& cleanup_queue);
    std::unique_ptr<Image> buffer_to_image(VkCommandBuffer cmd, Buffer* buffer, VkFormat format, VkImageUsageFlags usageFlags, uint32_t width, uint32_t height);

    //
    auto swapchain_image_format() { return m_swapchain_image_format; }
    auto swapchain_image_views() { return m_swapchain_image_views; }

    inline IInput* input() { return dynamic_cast<IInput*>(this); }

    auto allocator() { return m_allocator; }

    inline void set_window_renderpass(RenderPass* renderpass) { m_window_renderpass = renderpass; };

    inline auto frame_index() { return m_frame_index; }

    struct FrameArgs
    {
        float delta_t;
        uint32_t frame_index;
        VkCommandBuffer cmd;
        std::vector<std::function<void()>>& cleanup_queue;
    };

    void run(std::function<void(FrameArgs)>);

private:
    bool is_key_pressed(uint32_t key) override { return m_keystates[key]; }
    glm::vec2 mouse_delta() const override { return glm::vec2(m_mouse_delta_x, m_mouse_delta_y); }

    void handle_input();
    void draw_frame(float delta_t, std::function<void(FrameArgs)>& frame_func);

    void init_frame_data();
    void cleanup_frame_data();

    void init_swapchain(VkSwapchainKHR old_swapchain = nullptr);
    void cleanup_swapchain();

    void init_allocator();
    void cleanup_allocator();

    void init_pipeline_cache();
    void cleanup_pipeline_cache();

private:
    struct Data;
    std::unique_ptr<Data> m_data;

    VkInstance m_instance;
    VkDevice m_device;
    VkPhysicalDevice m_chosen_gpu;

    uint32_t m_graphics_queue_family; // family of that queue
    VkQueue m_graphics_queue;         // queue we will submit to

    VkPipelineCache m_pipeline_cache = nullptr;

    struct FrameData
    {
        VkCommandPool cmd_pool;
        VkCommandBuffer cmd;
        VkSemaphore present_semaphore, render_semaphore;
        VkFence render_fence;
        std::vector<std::function<void()>> last_cleanup_queue;
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

    RenderPass* m_window_renderpass = nullptr;

    SDL_Window* m_window = nullptr;
    uint32_t m_height, m_width;

    // buffers and memory
    VmaAllocator m_allocator = nullptr;

    // input
    float m_mouse_x, m_mouse_y, m_mouse_delta_x, m_mouse_delta_y;
    std::unordered_map<uint32_t, bool> m_keystates;
    std::unordered_map<uint32_t, bool> m_mouse_button_states;
    std::unordered_map<uint32_t, std::function<void()>> m_on_mouse_click;
    std::unordered_map<uint32_t, std::function<void()>> m_on_key_down;
    bool m_mouse_captured = false;

    // stats
    bool m_running = true;
    float m_fps    = 0;
};

void begin_cmd(VkCommandBuffer);

} // namespace vke