//
// Created by DXM on 2025/10/29.
//

#ifndef MEMORY_POOL_XS_THREAD_CACHE_H
#define MEMORY_POOL_XS_THREAD_CACHE_H
#include <optional>
#include <list>
#include <array>
#include "../include/utils.h"


namespace memory_pool {
    // 单例类 但是线程独有的
    class Thread_cache {
    public:
        static Thread_cache& get_instance() {
            static thread_local Thread_cache instance;
            return instance;
        }
        // 禁用拷贝构造 移动构造
        Thread_cache(const Thread_cache&) = delete;
        Thread_cache(Thread_cache&&) = delete;

        // 两个接口 allocate和deallocate，分别用于申请内存和释放内存
        [[nodiscard("返回值未使用")]] std::optional<void*> allocate(size_t memory_size);
        void deallocate(void* ptr, size_t memory_size);
        // 从central-cache申请一组memory_span,取出一个返回，其余的挂在thread_cache中
        std::optional<Memory_span> allocate_from_central(size_t memory_size);
    private:
        // 默认构造函数私有化
        Thread_cache() = default;

        size_t Next_alloc_number(size_t memory_size);

        // 存储空闲内存链表的array
        std::array<std::list<Memory_span>, Size_utils::CACHE_LINE_SIZE> m_free_cache;
        // 存储申请数量的数组
        std::array<size_t, Size_utils::CACHE_LINE_SIZE> m_allocate_number;
    };
}

#endif //MEMORY_POOL_XS_THREAD_CACHE_H
