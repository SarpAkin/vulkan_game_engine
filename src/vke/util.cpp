#include "util.hpp"

#include <fmt/core.h>

FunctionTimer::FunctionTimer(const char* func_name)
{
    m_start     = std::chrono::steady_clock::now();
    m_func_name = func_name;
}

FunctionTimer ::~FunctionTimer()
{
    double elapsed_time = ((double)(std::chrono::steady_clock::now() - m_start).count()) / 1000000.0;
    fmt::print("[Function Timer] function: {} took {} ms to run\n", m_func_name, elapsed_time);
}