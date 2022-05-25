#pragma once

#include <vke/core/core.hpp>
#include <vke/renderpass.hpp>

class IRenderSystem
{
public:
    // virtual void render(vke::RenderPass* render_pass,int subpass = 0){};
    virtual void cleanup(){};
    
};