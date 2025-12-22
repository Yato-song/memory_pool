//
// Created by DXM on 2025/10/29.
//

#ifndef MEMORY_POOL_XS_UTILS_H
#define MEMORY_POOL_XS_UTILS_H


#include <assert.h>
#include <bitset>
#include <cstddef>
#include <bitset>

namespace memory_pool {
    class Memory_span {
    public:
        // 构造函数
        Memory_span(void* addr, const size_t size): m_addr(addr), m_size(size) {};
        Memory_span(const Memory_span&) = default;
        // 运算符重载
        auto operator<=>(const Memory_span &other) const {
            return static_cast<char*>(m_addr) <=> static_cast<char*>(other.m_addr);
        }

        [[nodiscard("返回值未使用")]] void* GetAddr() const;
        void* data() {
            return m_addr;
        }
        size_t size() {
            return m_size;
        }

        // 从大的memory_span中切割出一个小的memory_span
        Memory_span subspan(const size_t memory_size) {
            assert(memory_size <= m_size);
            void* addr = m_addr;
            m_addr = static_cast<char*>(m_addr) + memory_size;
            m_size -= memory_size;
            return Memory_span(addr, memory_size);
        }
    private:
        // 当前memory_span管理的内存的起始地址
        void* m_addr;
        // 内存大小
        size_t m_size;


    };

    class Size_utils {
        public:
        // 内存对其的最小单位，也是内存分配的最小值
        static constexpr size_t ALIGNMENT = sizeof(void *);
        // 内存池缓存的最大内存大小
        static constexpr size_t MAX_CACHED_UNIT_SIZE = 16 * 1024;
        // 内存池链表数(桶数)
        static constexpr size_t CACHE_LINE_SIZE = MAX_CACHED_UNIT_SIZE/ALIGNMENT;
        // thread_cache缓存的内存总大小上限
        static constexpr size_t MAX_SIZE_CACHED_IN_THREAD_CACHE = 256 * 1024;
        // page大小
        static constexpr size_t PAGE_SIZE = 4096;

        // 计算内存向上对其的值
        static size_t align(size_t memory_size, size_t alignment = ALIGNMENT);
        // 获取对应桶的数组的下标
        static size_t get_index(size_t memory_size);
    };

    // 该类用于管理从page_cache分配下来的内存
    class Page_span {
    public:
        Page_span(Memory_span memory_span, size_t unit_size):m_memory_span(memory_span), m_unit_size(unit_size) {};

        static constexpr size_t MAX_UNIT_COUNT = Size_utils::PAGE_SIZE / Size_utils::ALIGNMENT;

        bool is_all_idle() const {
            return m_bitset.none();
        }
        Memory_span GetSpan() const{
            return m_memory_span;
        }

        size_t Getsize() const {
            return m_unit_size;
        }

        // 在page_span中注册一块内存
        void RegisterSpan(Memory_span span);
        // 在page_span中注销一块内存
        void UnregisterSpan(Memory_span span);

    private:
        Memory_span m_memory_span;
        size_t m_unit_size;
        std::bitset<MAX_UNIT_COUNT> m_bitset;
    };
}


#endif //MEMORY_POOL_XS_UTILS_H