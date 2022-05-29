#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include <glm/vec2.hpp>

#include "../../util/concurent_queue.hpp"
#include "chunk.hpp"

class WorldGen
{
    WorldGen(const WorldGen&) = delete;
public:
    WorldGen(uint64_t seed);
    ~WorldGen();

    void init(int worker_count = 1);

public:
    ConcurentQueue<glm::ivec2> in_chunk_poses;
    ConcurentQueue<std::pair<glm::ivec2, std::unique_ptr<Chunk>>> out_chunks;

private:
    void worker_func();
    void gen_func_init();

private:
    std::atomic_bool m_running;

    const uint32_t m_max_batch_size = 4;
    const uint64_t m_seed;

    std::vector<std::jthread> m_workers;
    std::function<std::unique_ptr<Chunk>(glm::ivec2)> m_gen_func;
};