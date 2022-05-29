#include "world_gen.hpp"

#include <random>

#include "../../util/noise.hpp"

#include <PerlinNoise.hpp>

WorldGen::WorldGen(uint64_t seed) : m_seed(seed) { gen_func_init(); }

namespace  {

void iterate_over_layers_in_vchunk(Tile* vchunk, uint32_t y_beg, uint32_t y_end, auto&& func)
{
    assert(y_beg <= Chunk::chunk_size && y_end <= Chunk::chunk_size);

    for (uint32_t y = y_beg; y < y_end; ++y)
    {
        for (uint32_t z = 0; z < Chunk::chunk_size; ++z)
        {
            uint32_t yz_offset = y * Chunk::chunk_surface_area + z * Chunk::chunk_size;
            for (uint32_t x = 0; x < Chunk::chunk_size; ++x)
            {
                func(vchunk[yz_offset + x], x, y, z);
            }
        }
    }
}

auto fill_air = [](Tile& t, uint32_t, uint32_t, uint32_t) { t = Tile::air; };

void iterate_over_layers(Chunk* chunk, uint32_t y_beg, uint32_t y_end, auto&& func)
{
    assert(chunk && y_beg <= Chunk::chunk_size * Chunk::vertical_chunk_count && y_end <= Chunk::chunk_size * Chunk::vertical_chunk_count);

    for (int y = y_beg / Chunk::chunk_size; y < y_end / Chunk::chunk_size + (y_end % Chunk::chunk_size != 0); ++y)
    {
        uint32_t vy_beg = std::clamp(static_cast<int>(y_beg) - y * Chunk::chunk_size, 0, Chunk::chunk_size);
        uint32_t vy_end = std::clamp(static_cast<int>(y_end) - y * Chunk::chunk_size, 0, Chunk::chunk_size);

        Tile* vchunk = chunk->get_tile_array(y);
        if (vchunk == nullptr)
        {
            chunk->set_vertical_chunk(malloc_unique<Tile>(Chunk::chunk_volume), y);
            vchunk = chunk->get_tile_array(y);

            iterate_over_layers_in_vchunk(vchunk, 0, vy_beg, fill_air);
            iterate_over_layers_in_vchunk(vchunk, vy_end, Chunk::chunk_size, fill_air);
        }

        iterate_over_layers_in_vchunk(vchunk, vy_beg, vy_end,
            [&func, cy = y * Chunk::chunk_size](Tile& t, uint32_t x, uint32_t y, uint32_t z) { func(t, x, y + cy, z); });
    }
}

}

void WorldGen::gen_func_init()
{
    double base_height = 60.7;

    // auto p      = AmplifiedNoise(0.006445, 70.3, m_seed + 92);
    // auto p1     = AmplifiedNoise(0.063000, 17.3, m_seed + 288);
    // auto p2     = AmplifiedNoise(0.001739, 02.7, m_seed + 2722);
    // auto psnow  = AmplifiedNoise(0.092272, 16.3, m_seed + 266);
    // auto pbiome = AmplifiedNoise(0.003378, 03.2, m_seed + 69420);

    auto seeder = std::mt19937(m_seed);

    m_gen_func =
        [              =,
            p          = AmplifiedNoise(0.006445, 70.3, seeder()),
            p1         = AmplifiedNoise(0.063000, 17.3, seeder()),
            p2         = AmplifiedNoise(0.001739, 02.7, seeder()),
            psnow      = AmplifiedNoise(0.092272, 16.3, seeder()),
            pbiome     = AmplifiedNoise(0.003378, 03.2, seeder()),
            cave_noise = AmplifiedNoise(0.023100, 01.0, seeder())

            //
    ](glm::ivec2 c_pos) {
        auto chunk = std::make_unique<Chunk>();

        double c_real_pos_x = c_pos.x * Chunk::chunk_size;
        double c_real_pos_z = c_pos.y * Chunk::chunk_size;

        // std::array<float, Chunk::chunk_surface_area> base_heights;

        float min_base_height = 1'000'000;
        float max_base_height = 0;

        for (int z = 0; z < Chunk::chunk_size; ++z)
        {
            double real_z = c_real_pos_z + z;

            for (int x = 0; x < Chunk::chunk_size; ++x)
            {
                float center_proximity = std::max(std::abs(z - (Chunk::chunk_size / 2)), std::abs(x - (Chunk::chunk_size / 2))) * 0.3f;

                double real_x = c_real_pos_x + x;

                float height = p.noise(real_x, real_z) * p2.noise(real_x, real_z) + base_height;

                // base_heights[z * Chunk::chunk_size + x] = height;

                min_base_height = std::min(height - center_proximity, min_base_height);
                max_base_height = std::max(height + center_proximity, max_base_height);
            }
        }

        float layer_bias = std::clamp<float>(std::abs(p2.noise(c_real_pos_x, c_real_pos_z)) + 3.4f, 3.f, 45.f);

        volatile uint32_t layer_beg = std::clamp<int>(static_cast<int>(min_base_height - layer_bias), 0, Chunk::chunk_size * Chunk::vertical_chunk_count);
        volatile uint32_t layer_end = std::clamp<int>(static_cast<int>(max_base_height + layer_bias), 1, Chunk::chunk_size * Chunk::vertical_chunk_count);

        iterate_over_layers(chunk.get(), 0, layer_beg, [&](Tile& t, uint32_t x, uint32_t y, uint32_t z) {
            t = Tile::stone;
        });

        iterate_over_layers(chunk.get(), layer_beg, layer_end, [&](Tile& t, uint32_t x, uint32_t y, uint32_t z) {
            double real_x = c_real_pos_x + x;
            double real_z = c_real_pos_z + z;

            double b_amp = p2.noise(real_x, real_z);

            double height = p.noise(
                                real_x + p1.noise(real_z, real_x, y) * b_amp * 0.9,
                                real_z + p1.noise(real_x, y, real_z) * b_amp * 1.1) *
                                b_amp +
                            base_height;

            t = height > y ? Tile::stone : Tile::air;
        });

        for (int z = 0; z < Chunk::chunk_size; ++z)
        {
            double real_z = c_real_pos_z + z;

            for (int x = 0; x < Chunk::chunk_size; ++x)
            {
                double real_x = c_real_pos_x + x;

                for (int y = layer_end - 1; y > layer_beg; --y)
                {
                    double snow_height = psnow.noise(real_x, real_z) + 90;

                    bool is_desert = pbiome.noise(real_x, real_z) - std::abs(p2.noise(real_x, real_z)) > 1.32;

                    Tile t = chunk->get_block(x, y, z);
                    if (t == Tile::stone)
                    {
                        chunk->set_block(y < snow_height ? (is_desert ? Tile::sand : Tile::grass) : Tile::snow, x, y, z);
                        for (int y1 = std::max(y - 3, 0); y1 < y; ++y1)
                        {
                            if (chunk->get_block(x, y1, z) == Tile::stone)
                            {
                                chunk->set_block((is_desert ? Tile::sand : Tile::dirt), x, y1, z);
                            }
                        }

                        break;
                    }
                }
            }
        }

        double cave_peek_y = 35.3;
        double bias        = 13.3;

        double bias2 = bias * bias;

        double clamp_max = bias2 / (bias + 1);

        iterate_over_layers(chunk.get(), 1, layer_end, [&](Tile& t, uint32_t x, uint32_t y, uint32_t z) {
            if (t != Tile::air)
            {
                double real_x = c_real_pos_x + x;
                double real_z = c_real_pos_z + z;

                double dist_to_cave_peek = std::abs(y - cave_peek_y);

                dist_to_cave_peek = std::min(dist_to_cave_peek, clamp_max);

                // simpler (dist_to_cave_peek / bias - dist_to_cave_peek) / bias
                t = cave_noise.noise(real_x, y, real_z) > 0.3 - (dist_to_cave_peek / (bias2 - dist_to_cave_peek * bias)) ? t : Tile::air;
            }
        });

        return chunk;
    };
}

WorldGen::~WorldGen()
{
    in_chunk_poses.notify_all();
    out_chunks.notify_all();
    m_running = false;
}

void WorldGen::init(int worker_count)
{
    m_running = true;

    for (int i = 0; i < worker_count; ++i)
    {
        m_workers.emplace_back([this] { worker_func(); });
    }
}

void WorldGen::worker_func()
{
    while (m_running)
    {
        std::vector<glm::ivec2> chunks_to_generate;
        std::vector<std::pair<glm::ivec2, std::unique_ptr<Chunk>>> generated_chunks;

        in_chunk_poses.fetch_some_blocking(chunks_to_generate, m_max_batch_size);

        for (auto& chunk_to_gen : chunks_to_generate)
        {
            generated_chunks.emplace_back(chunk_to_gen, m_gen_func(chunk_to_gen));
        }

        if (generated_chunks.size())
            out_chunks.push(std::move(generated_chunks));
    }
}
