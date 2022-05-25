#pragma once

#include <memory>

struct Free
{
    inline void operator()(auto* p) const
    {
        free(p);
    }
};

template <typename T>
std::unique_ptr<T, Free> malloc_unique(size_t n)
{
    return std::unique_ptr<T, Free>((T*)malloc(sizeof(T) * n));
}