#pragma once

#include <stdexcept>
#include <vector>

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

