#include "core.hpp"

#include <fstream>

#include "../util.hpp"
#include "../vkutil.hpp"

namespace vke
{

const char* vk_cache_bin = ".vke_cache";

void Core::init_pipeline_cache()
{
    VkPipelineCacheCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    };

    std::unique_ptr<uint8_t,Free> buffer;

    auto cache_file = std::ifstream(vk_cache_bin, std::ios::binary);
    if (cache_file.is_open())
    {   
        cache_file.seekg(0,std::ios_base::end);
        size_t file_size = cache_file.tellg();
        cache_file.seekg(0,std::ios_base::beg);
        
        buffer = malloc_unique<uint8_t>(file_size);
        cache_file.read(reinterpret_cast<char*>(buffer.get()), file_size);

        info.initialDataSize = file_size;
        info.pInitialData = buffer.get();

    }

    VK_CHECK(vkCreatePipelineCache(device(), &info, nullptr, &m_pipeline_cache));
}

void Core::cleanup_pipeline_cache()
{
    size_t data_size;

    VK_CHECK(vkGetPipelineCacheData(device(), m_pipeline_cache, &data_size, nullptr));

    auto buffer = malloc_unique<uint8_t>(data_size);

    VK_CHECK(vkGetPipelineCacheData(device(), m_pipeline_cache, &data_size, buffer.get()));

    fmt::print("cache size {}\n", data_size);

    std::ofstream(vk_cache_bin, std::ios::binary).write(reinterpret_cast<char*>(buffer.get()), data_size);

    vkDestroyPipelineCache(device(), m_pipeline_cache, nullptr);
}
} // namespace vke