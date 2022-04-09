#pragma once

#include <stdexcept>
#include <vector>
#include <chrono>
#include <memory>

#define STRINGIZING(x) #x
#define STR(x) STRINGIZING(x)
#define FILE_LINE __FILE__ ":" STR(__LINE__)

#define EXPECTION(mes) std::runtime_error("runtime error at: " FILE_LINE "\n\t" mes)


template <typename T>
auto map_vec(const std::vector<T>& vec, auto&& map_function)
{
    std::vector<decltype(map_function(vec.back()))> mapped_vec;
    mapped_vec.reserve(vec.size());
    for (auto&& e : vec)
        mapped_vec.push_back(map_function(e));

    return mapped_vec;
}

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

class FunctionTimer
{
public:
    FunctionTimer(const char* func_name);
    ~FunctionTimer();

private:
    std::chrono::steady_clock::time_point m_start;
    const char* m_func_name;
};

#define BENCHMARK_FUNCTION() FunctionTimer ______function_timer(__PRETTY_FUNCTION__);
