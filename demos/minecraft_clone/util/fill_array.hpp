#pragma once

#include <array>

template <std::size_t N>
auto fill_array(auto&& func) -> std::array<decltype(func(0)), N>
{
    std::array<decltype(func(0)), N> arr;

    for (std::size_t i = 0; i < N; ++i)
    {
        arr[i] = func(i); 
    }

    return arr;
}