//
// Created by DXM on 2025/11/5.
//

#include "../include/central_cache.h"
#include <thread>
#include "../include/page_cache.h"

namespace memory_pool {
    std::optional<std::list<Memory_span>> Central_cache::allocate(const size_t memory_size, const size_t span_number) {
        /* allocate逻辑
         * 1. 若空闲链表中数量满足，从自身空闲链表中分配
         * 2. 若不足，要从page_cache申请大页内存
        */
        std::list<Memory_span> spans;

        const size_t index = Size_utils::get_index(memory_size);
        // 加锁
        while (m_cache_lock[index].test_and_set()) {
            std::this_thread::yield();
        }
        if (m_free_cache[index].size() < span_number) {
            // 缓存中span数量不够
            // step1: 向page_cache申请
            // 申请的内存大小按能申请的最大大小来申请
            const size_t alloc_page_num = Size_utils::align(Page_span::MAX_UNIT_COUNT * memory_size, Size_utils::PAGE_SIZE) / Size_utils::PAGE_SIZE;
            auto alloc_page = Page_Cache::get_instance().allocate(alloc_page_num);
            if (!alloc_page.has_value()) {
                m_cache_lock[index].clear();
                return std::nullopt;
            }
            // 初始化一个page_span来管理从page_cache申请的内存
            Page_span sp(alloc_page.value(), memory_size);

            // step2: 切割出span_number个span返回，其余的挂到central_cache的m_free_cache中
            for (int i = 0; i < span_number; ++i) {
                Memory_span s = alloc_page.value().subspan(memory_size);
                spans.push_back(s);
                // 在page_span中注册这一块内存
                sp.RegisterSpan(s);
            }
            auto [_, res] = m_page_span_set[index].emplace(sp.GetSpan().GetAddr(), std::move(sp));
            assert(res == true);

            const size_t remain_num = alloc_page.value().size() / memory_size;
            for (int i = 0; i < remain_num; ++i) {
                Memory_span s = alloc_page.value().subspan(memory_size);
                m_free_cache[index].push_back(s);
            }

        } else {
            // 缓存中span数量足够
            auto it = m_free_cache[index].begin();
            for (int i = 0; i < span_number; i++) {
                // 为每一个memory_span在其对应的page_span中进行注册
                auto memory = *it;
                auto it_map = m_page_span_set[index].upper_bound(memory.data());
                --it_map;
                it_map->second.RegisterSpan(memory);

                ++it;
            }
            spans.splice(spans.begin(), m_free_cache[index], m_free_cache[index].begin(), it);

        }

        // check每个span的大小
        for (auto it = spans.begin(); it != spans.end(); ++it) {
            assert(it->size() == memory_size);
        }
        assert(spans.size() == span_number);

        // 释放锁
        m_cache_lock[index].clear(std::memory_order_release);

        return spans;
    }

    void Central_cache::deallocate(std::list<Memory_span> &memory_spans) {
        // 检查链表中memory_span大小是否全部一致
        for (auto it = memory_spans.begin(); it != memory_spans.end(); ++it) {
            assert(it->size() == memory_spans.begin()->size());
        }

        size_t index = Size_utils::get_index(memory_spans.begin()->size());
        // lock
        while (m_cache_lock[index].test_and_set()) {
            std::this_thread::yield();
        }
        // 1. 把span链表放回central_cache中
        for (Memory_span s : memory_spans) {
            m_free_cache[index].push_back(s);
            // 取消注册
            auto it_map = m_page_span_set[index].upper_bound(s.data());
            --it_map;
            it_map->second.UnregisterSpan(s);

            // 2. 判断是否有整页可以归还给page_cache
            if (it_map->second.is_all_idle()) {
                // 从链表中摘除属于该page_cache的所有span
                for (auto it = m_free_cache[index].begin(); it != m_free_cache[index].end();) {
                    // FIX: 使用 GetSpan().size() 来获取整个大页的大小，而不是 Getsize() (小块大小)
                    if (static_cast<char*>(it->data()) >= static_cast<char*>(it_map->second.GetSpan().GetAddr())
                        && static_cast<char*>(it->data()) < static_cast<char*>(it_map->second.GetSpan().GetAddr()) + it_map->second.GetSpan().size()) {
                        it = m_free_cache[index].erase(it);
                    } else {
                        ++it;
                    }
                }

                // 归还page_span管理的memory_span给page_cache
                Page_Cache::get_instance().deallocate(it_map->second.GetSpan());

                // 将page_span从m_page_span_set中移除
                m_page_span_set[index].erase(it_map);
            }
        }
        // unlock
        m_cache_lock[index].clear();
    }

/*-----------------测试接口实现------------------*/
    size_t Central_cache::get_map_size(const size_t memory_size) {
        const size_t index = Size_utils::get_index(memory_size);
        return m_page_span_set[index].size();
    }

    size_t Central_cache::get_list_size(const size_t memory_size) {
        const size_t index = Size_utils::get_index(memory_size);
        return m_free_cache[index].size();
    }

}