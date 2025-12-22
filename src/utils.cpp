//
// Created by DXM on 2025/10/29.
//

#include "../include/utils.h"

namespace memory_pool {
    void *Memory_span::GetAddr() const{
        return m_addr;
    }

    size_t Size_utils::align(const size_t memory_size, const size_t alignment) {
        return (memory_size + alignment - 1) & ~(alignment - 1);
    }

    size_t Size_utils::get_index(const size_t memory_size) {
        return align(memory_size) / ALIGNMENT - 1;
    }

    void Page_span::RegisterSpan(Memory_span span) {
        assert((static_cast<char*>(span.GetAddr()) - static_cast<char*>(m_memory_span.GetAddr())) % m_unit_size == 0);
        size_t index = (static_cast<char*>(span.GetAddr()) - static_cast<char*>(m_memory_span.GetAddr())) / m_unit_size;
        m_bitset.set(index);
    }

    void Page_span::UnregisterSpan(Memory_span span) {
        assert((static_cast<char*>(span.GetAddr()) - static_cast<char*>(m_memory_span.GetAddr())) % m_unit_size == 0);
        size_t index = (static_cast<char*>(span.GetAddr()) - static_cast<char*>(m_memory_span.GetAddr())) / m_unit_size;
        m_bitset.reset(index);
    }

}