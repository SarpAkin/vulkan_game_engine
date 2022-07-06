#pragma once

#include <type_traits>

#include <glm/vec3.hpp>

#include <fmt/format.h>



template <typename T, size_t N>
struct fmt::formatter<glm::vec<N, T>>
{
    constexpr auto parse(const format_parse_context& ctx) const
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const glm::vec<N, T>& vec, FormatContext& ctx) -> decltype(ctx.out())
    {

        auto it = format_to(ctx.out(), std::is_floating_point<T>() ? "[{:.2f}" : "[{}", vec[0]);

        for (int i = 1; i < N; ++i)
        {
            it = format_to(it, std::is_floating_point<T>() ? ",{:.2f}" : ",{}", vec[i]);
        }

        return format_to(it, "]");
    }
};