//
// Created by DXM on 2025/10/29.
//
#include "../include/thread_cache.h"
//#include "../include/utils.h"
#include "../include/central_cache.h"
#include "../include/page_cache.h"


namespace memory_pool {
    std::optional<void*> Thread_cache::allocate(size_t memory_size) {
        if (memory_size == 0)
            return std::nullopt;
        memory_size = Size_utils::align(memory_size);
        // 如果申请的大小超过内存池缓存的最大内存块，则直接向page_cache申请
        if (memory_size > Size_utils::MAX_CACHED_UNIT_SIZE) {
            return Page_Cache::get_instance().allocate_direct(memory_size)->GetAddr();
        }
        // 从thread_cache中取
        size_t index = Size_utils::get_index(memory_size);
        if (!m_free_cache[index].empty()) {
            auto res = m_free_cache[index].front();
            m_free_cache[index].pop_front();
            return res.GetAddr();
        }
        // thread_cache中没有去central_cache中取
        return allocate_from_central(memory_size).and_then([](Memory_span && span){return std::optional<void*>(span.GetAddr());});
    }

    void Thread_cache::deallocate(void *ptr, size_t memory_size) {
        if (memory_size == 0)
            return;
        memory_size = Size_utils::align(memory_size);
        // 如果大小超过MAX_CACHED_UNIT_SIZE 直接还给page_cache
        if (memory_size > Size_utils::MAX_CACHED_UNIT_SIZE) {
            // 直接返回给操作系统
            Page_Cache::get_instance().deallocate_direct(Memory_span(ptr, memory_size));
            return;
        }
        // 返还内存到对应链表
        const auto index = Size_utils::get_index(memory_size);
        Memory_span span(ptr, memory_size);
        m_free_cache[index].push_front(span);

        // 检测thread_cache是否缓存过多span, 如果超过MAX_SIZE_CACHED_IN_THREAD_CACHE，则需要将一部分还给central_cache
        if (m_free_cache[index].size() * memory_size > Size_utils::MAX_SIZE_CACHED_IN_THREAD_CACHE) {
            std::list<Memory_span> memory_deallocate;
            // 返还当前链表上一半的memory_span给central_cache
            const size_t dealloc_size = m_free_cache[index].size() / 2;
            auto it_begin = m_free_cache[index].end();
            auto it_end = m_free_cache[index].end();
            for (int i = 0; i < dealloc_size; i++) {
                --it_begin;
            }
            memory_deallocate.splice(memory_deallocate.begin(), m_free_cache[index], it_begin, it_end);

            // 调用central_cache的deallocate接口归还memory_span
            Central_cache::get_instance().deallocate(memory_deallocate);
            m_allocate_number[index] /= 2;
        }
    }

    std::optional<Memory_span> Thread_cache::allocate_from_central(size_t memory_size) {
        // 计算要申请多少个memory_span
        size_t apply_number = Next_alloc_number(memory_size);
        return Central_cache::get_instance().allocate(memory_size, apply_number).and_then(
            [this, memory_size](std::list<Memory_span> && span_list) {
                // 取出span_list中第一个span
                Memory_span span = span_list.front();
                span_list.pop_front();
                const size_t index = Size_utils::get_index(memory_size);
                m_free_cache[index].splice(m_free_cache[index].end(), span_list);
                return std::optional<Memory_span>(span);
            });
    }


    size_t Thread_cache::Next_alloc_number(const size_t memory_size) {
        /* 规则：
         * 1. 至少申请4个
         * 2. 申请数量*span大小 不能超过每个链表能缓存的最大限制
         * 3. 申请数量不能超过page_span能管理的span数量上限
        */
        const size_t index = Size_utils::get_index(memory_size);

        size_t span_num = std::max(static_cast<size_t>(4), m_allocate_number[index]);
        span_num *= 2;
        // span_num要小于page_span能管理的最大span数
        span_num = std::min(span_num, Page_span::MAX_UNIT_COUNT);

        span_num = std::min(span_num, Size_utils::MAX_SIZE_CACHED_IN_THREAD_CACHE/memory_size);

        // update m_allocate_number
        m_allocate_number[index] = span_num;
        return span_num;
    }


}
