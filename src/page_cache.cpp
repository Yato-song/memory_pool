//
// Created by DXM on 2025/11/15.
//
#include "../include/page_cache.h"

#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace memory_pool {
    Page_Cache::~Page_Cache() {
        stop();
    }

    std::optional<Memory_span> Page_Cache::allocate(size_t page_num) {
        if (page_num == 0) {
            return std::nullopt;
        }
        // 加锁
        std::lock_guard<std::mutex> lock(m_mutex);
        // case1: m_free_page_list中有足够的span
        auto it = m_free_page_list.lower_bound(page_num);
        while (it != m_free_page_list.end()) {
            if (!it->second.empty()) {
                // 取出第一个
                auto it_first = it->second.begin();
                Memory_span page_span = *it_first;
                it->second.erase(it_first);
                m_page_set.erase(page_span.GetAddr());

                // split
                Memory_span span_r = page_span.subspan(page_num * Size_utils::PAGE_SIZE);
                assert(page_span.size() % Size_utils::PAGE_SIZE == 0);
                size_t index = page_span.size() / Size_utils::PAGE_SIZE;
                if (index != 0) {
                    // 如果split后还有剩余page，插入到m_free_page_list中
                    m_free_page_list[index].emplace(page_span);
                    m_page_set.emplace(page_span.GetAddr(), page_span);
                }

                return span_r;
            }
            ++it;
        }
        // case2: 需要向操作系统申请
        return allocate_from_system().transform([this, page_num](Memory_span span) {
            span_vector.push_back(span);
            // 从向操作系统申请的内存中切割出来对应大小的span
            Memory_span span_r = span.subspan(page_num * Size_utils::PAGE_SIZE);
            // 其余的放到m_free_page_list中
            if (span.size() > 0) {
                const size_t index = span.size() / Size_utils::PAGE_SIZE;
                m_free_page_list[index].emplace(span);
                m_page_set.emplace(span.GetAddr(), span);
            }

            return span_r;
        });
    }

    void Page_Cache::deallocate(Memory_span span) {
        assert(span.size() % Size_utils::PAGE_SIZE == 0);
        std::lock_guard<std::mutex> lock(m_mutex);

        // 向前合并
        while (!m_page_set.empty()) {
            auto it = m_page_set.lower_bound(span.GetAddr());
            if (it != m_page_set.begin()) {
                --it;
                if (static_cast<char*>(it->second.GetAddr()) + it->second.size() == static_cast<char*>(span.GetAddr())) {
                    // 从m_free_page_list和m_page_set去除对应span
                    size_t index = it->second.size() / Size_utils::PAGE_SIZE;
                    const Memory_span span_tmp = it->second;
                    m_free_page_list[index].erase(span_tmp);
                    // 更新span
                    span = Memory_span(it->second.GetAddr(), it->second.size() + span.size());
                    m_page_set.erase(it);
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        // 向后合并
        while (!m_page_set.empty()) {
            auto it = m_page_set.upper_bound(span.GetAddr());
            if (it != m_page_set.end()) {
                if (static_cast<char*>(span.GetAddr()) + span.size() == static_cast<char*>(it->second.GetAddr())) {
                    size_t index = it->second.size() / Size_utils::PAGE_SIZE;
                    const Memory_span span_tmp = it->second;
                    m_free_page_list[index].erase(span_tmp);
                    // 更新span
                    span = Memory_span(span.GetAddr(), it->second.size() + span.size());
                    m_page_set.erase(it);
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        // 将span插入回对应的m_free_page_list
        size_t index = span.size() / Size_utils::PAGE_SIZE;
        m_free_page_list[index].emplace(span);
        m_page_set.emplace(span.GetAddr(), span);
    }

    std::optional<Memory_span> Page_Cache::allocate_from_system() {
        const size_t alloc_size = SYSTEM_ALLOCATE_SIZE;

        // 使用mmap分配内存
        void* ptr = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) return std::nullopt;

        memset(ptr, 0, alloc_size);
        return Memory_span(ptr, alloc_size);
    }

    void Page_Cache::deallocate_from_system(Memory_span span) {
        munmap(span.GetAddr(), span.size());
    }

    std::optional<Memory_span> Page_Cache::allocate_direct(const size_t size) {
        auto mem = malloc(size);
        if (mem != nullptr) {
            return Memory_span(mem, size);
        }
        return std::nullopt;
    }

    void Page_Cache::deallocate_direct(const Memory_span span) {
        free(span.GetAddr());
    }

    void Page_Cache::stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!is_stop) {
            for (auto& i : span_vector) {
                deallocate_from_system(i);
            }
        }
    }

    // ---------------测试接口-------------------
    size_t Page_Cache::get_free_page_list_size(size_t index) {
        return m_free_page_list[index].size();
    }



}