//
// Created by DXM on 2025/11/15.
//

#ifndef MEMORY_POOL_XS_PAGE_CACHE_H
#define MEMORY_POOL_XS_PAGE_CACHE_H

#include <optional>
#include <map>
#include <mutex>
#include <vector>
#include <set>

#include "./utils.h"

namespace memory_pool {
    class Page_Cache {
    public:
        static Page_Cache& get_instance() {
            static Page_Cache instance;
            return instance;
        }
        ~Page_Cache();

        // 大内存，直接申请
        std::optional<Memory_span> allocate_direct(size_t size);
        // 大内存，回收
        void deallocate_direct(Memory_span span);
        // 申请page接口
        std::optional<Memory_span> allocate(size_t page_num);
        // 返还page接口
        void deallocate(Memory_span span);
        // 单次向操作系统申请的内存大小 固定为8MB
        static constexpr size_t SYSTEM_ALLOCATE_SIZE = Size_utils::MAX_CACHED_UNIT_SIZE * Page_span::MAX_UNIT_COUNT;

        // ---------------测试接口-----------------
        // 接口1: 获取m_free_page_list中指定size的set中的span数量
        size_t get_free_page_list_size(size_t index);
    private:
        // page_cache的page缓存
        std::map<size_t, std::set<Memory_span>> m_free_page_list;
        // 用于记录m_free_page_list中的span和地址的映射关系，便于做page_span的合并
        std::map<void*, Memory_span> m_page_set;
        // 多线程互斥锁
        std::mutex m_mutex;
        // 存储page_cache中所有的向操作系统申请的page，用于stop时释放
        std::vector<Memory_span> span_vector;
        // flag
        bool is_stop = false;

        std::optional<Memory_span> allocate_from_system();
        void deallocate_from_system(Memory_span span);
        // 关闭内存池
        void stop();

    };
}
#endif //MEMORY_POOL_XS_PAGE_CACHE_H