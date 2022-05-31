#pragma once

#include "core/core.hpp"

#include <utility>
#include <string.h>

namespace vke
{
class DynamicUBO
{
public:
    DynamicUBO(vke::Core* core, uint32_t cap)
    {
        m_core   = core;
        m_cap    = cap;
        m_buffer = core->allocate_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, cap, true);
    }


    std::pair<uint32_t,uint32_t> push_data(uint32_t size, const void* data)
    {
        if (m_top + size > m_cap) return {-1,0};

        memcpy(m_buffer->get_data<uint8_t>() + m_top, data, size);

        uint32_t offset = m_top;

        m_top += m_core->pad_buffer(size);

        return {offset,size};
    }

    inline std::pair<uint32_t, uint32_t> push_data(const auto& data) { return push_data(sizeof(data), &data); }

    const auto& buffer()
    {
        return *m_buffer;
    }

    void cleanup()
    {
        m_buffer->clean_up();
    }

    void reset() { m_top = 0; };

private:
    vke::Core* m_core;
    std::unique_ptr<vke::Buffer> m_buffer;
    uint32_t m_top = 0;
    uint32_t m_cap;
};

} // namespace vke