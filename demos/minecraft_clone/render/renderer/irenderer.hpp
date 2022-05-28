#pragma once

#include <functional>
#include <vector>
#include <memory>

#include <vke/iinput.hpp>

class Game;

class IRenderer
{
public:
    virtual vke::IInput* input()=0;
    virtual void run(std::function<void(float)>&&)=0;
    virtual ~IRenderer(){};

    static std::unique_ptr<IRenderer> crate_vulkan_renderer(Game* game);
};
