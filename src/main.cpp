#include <fmt/format.h>

#include <iostream>

#include "vke/core/core.hpp"
#include "vke/renderpass.hpp"

int main()
{
    fmt::print("hello world\n");

    uint32_t width = 800, height = 500;

    auto core = vke::Core(width, height, "app");

    auto renderpass = [&] {
        auto rp_builder = vke::RenderPassBuilder();
        uint32_t sw_att = rp_builder.add_swapchain_attachment(core,VkClearValue{.color={1.f,0.f,0.f,0.f}});
        uint32_t dp_att = rp_builder.add_attachment(VK_FORMAT_D32_SFLOAT);
        rp_builder.add_subpass({sw_att}, dp_att);
        return rp_builder.build(core, width, height);
    }();

    core.run([&](vke::Core::FrameArgs args) {
        // fmt::print("{}\n", args.delta_t);
        auto cmd = args.cmd;

        renderpass->begin(cmd);

        

        renderpass->end(cmd);
    });

    renderpass->clean();


    return 0;
}