//
// Created by DXM on 2025/11/4.
//

#ifndef MEMORY_POOL_XS_CENTRAL_CACHE_H
#define MEMORY_POOL_XS_CENTRAL_CACHE_H
#include <list>
#include <array>
#include <optional>
#include <atomic>
#include <map>

#include "utils.h"

namespace memory_pool {
    class Central_cache {
        public:
        // 单例类
        static Central_cache& get_instance() {
            static Central_cache instance;
            return instance;
        }
        // 禁用拷贝构造&&移动构造
        Central_cache(const Central_cache&) = delete;
        Central_cache(Central_cache&&) = delete;

        // 接口
        std::optional<std::list<Memory_span>> allocate(size_t memory_size, size_t span_number);
        void deallocate(std::list<Memory_span>& memory_spans);

        // --------------测试接口-------------------
        // 接口1: 获取m_page_span_set中map的size
        size_t get_map_size(size_t memory_size);
        // 接口2: 获取m_free_cache中list的size
        size_t get_list_size(size_t memory_size);

        private:
        Central_cache() = default;
        // 存储空闲span链表的数组
        std::array<std::list<Memory_span>, Size_utils::CACHE_LINE_SIZE> m_free_cache;
        // 访问每个span链表的锁
        std::array<std::atomic_flag, Size_utils::CACHE_LINE_SIZE> m_cache_lock;
        // 存储page_span的map的集合
        std::array<std::map<void*, Page_span>, Size_utils::CACHE_LINE_SIZE> m_page_span_set;

    };
}

#endif //MEMORY_POOL_XS_CENTRAL_CACHE_H