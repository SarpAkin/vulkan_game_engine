#pragma once

#include <stdexcept>
#include <vector>
#include <chrono>
#include <memory>

#define STRINGIZING(x) #x
#define STR(x) STRINGIZING(x)
#define FILE_LINE __FILE__ ":" STR(__LINE__)

#define EXPECTION(mes) std::runtime_error("runtime error at: " FILE_LINE "\n\t" mes)

auto map_vec(auto&& vec, auto&& func)
{
    std::vector<decltype(func(*vec.begin()))> ret_vec;
    ret_vec.reserve(vec.size());
    for (auto&& e : vec)
    {
        ret_vec.push_back(func(e));
    }
    return ret_vec;
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
