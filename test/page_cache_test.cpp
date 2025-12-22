#include "gtest/gtest.h"
#include "page_cache.h"
#include "utils.h"
#include <thread>
#include <vector>
#include <numeric>

// Test fixture for Page_Cache tests
class PageCacheTest : public ::testing::Test {
protected:
    // Page_Cache is a singleton. We get its instance in each test.
    // Note: State will persist between tests. For truly isolated tests,
    // a 'reset' method on the singleton would be needed.
};

// Test case for a single, basic allocation.
TEST_F(PageCacheTest, BasicAllocation) {
    auto& pc = memory_pool::Page_Cache::get_instance();
    const size_t num_pages = 10;

    std::optional<memory_pool::Memory_span> span = pc.allocate(num_pages);

    ASSERT_TRUE(span.has_value()) << "Allocation of 1 page failed.";
    ASSERT_EQ(span.value().size(), num_pages * memory_pool::Size_utils::PAGE_SIZE);

    // Deallocate to clean up for other tests
    pc.deallocate(span.value());
}

// 测试dellocate是page_span是否会合并
TEST_F(PageCacheTest, DeallocateAndMerge) {
    auto& pc = memory_pool::Page_Cache::get_instance();

    // 1. Allocate a large chunk first to ensure there's a large free span.
    const size_t large_chunk_pages = 10;
    auto large_span = pc.allocate(large_chunk_pages);
    ASSERT_TRUE(large_span.has_value());
    size_t num_total = memory_pool::Page_Cache::SYSTEM_ALLOCATE_SIZE / memory_pool::Size_utils::PAGE_SIZE;
    EXPECT_EQ(pc.get_free_page_list_size(num_total - large_chunk_pages), 1);
    pc.deallocate(large_span.value()); // Now we have a 10-page span in the cache.
    EXPECT_EQ(pc.get_free_page_list_size(num_total), 1);
    EXPECT_EQ(pc.get_free_page_list_size(10), 0);

    // 2. Allocate a smaller chunk from it.
    const size_t small_chunk_pages = 3;
    auto small_span = pc.allocate(small_chunk_pages);
    ASSERT_TRUE(small_span.has_value());
    EXPECT_EQ(pc.get_free_page_list_size(3), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(7), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(10), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(num_total - small_chunk_pages), 1);
    ASSERT_EQ(small_span.value().size(), small_chunk_pages * memory_pool::Size_utils::PAGE_SIZE);

    // 3. Now, try to allocate the remainder. It should be available.
    const size_t remainder_pages = large_chunk_pages - small_chunk_pages; // 7 pages
    auto remainder_span = pc.allocate(remainder_pages);
    ASSERT_TRUE(small_span.has_value());
    EXPECT_EQ(pc.get_free_page_list_size(3), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(7), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(10), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(num_total - small_chunk_pages - remainder_pages), 1);

    // Cleanup
    pc.deallocate(small_span.value());
    EXPECT_EQ(pc.get_free_page_list_size(3), 1) ;
    EXPECT_EQ(pc.get_free_page_list_size(7), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(10), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(num_total - small_chunk_pages - remainder_pages), 1);

    pc.deallocate(remainder_span.value());
    EXPECT_EQ(pc.get_free_page_list_size(3), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(7), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(10), 0) ;
    EXPECT_EQ(pc.get_free_page_list_size(num_total), 1);
}

// 测试归还的page_span是否会被复用
TEST_F(PageCacheTest, Reuse) {
    auto& pc = memory_pool::Page_Cache::get_instance();

    auto span1 = pc.allocate(3);
    auto span2 = pc.allocate(3);
    auto span3 = pc.allocate(3);
    ASSERT_TRUE(span1.has_value() && span2.has_value() && span3.has_value());

    void* ptr_1 = span1.value().GetAddr();
    pc.deallocate(span1.value());

    auto span_tmp = pc.allocate(3);
    ASSERT_TRUE(span_tmp.has_value());
    ASSERT_EQ(static_cast<char*>(ptr_1), span_tmp.value().GetAddr());

    pc.deallocate(span2.value());
    pc.deallocate(span3.value());
    pc.deallocate(span_tmp.value());
}

// Test for direct allocation of large objects that bypass the page cache logic.
TEST_F(PageCacheTest, DirectAllocation) {
    auto& pc = memory_pool::Page_Cache::get_instance();
    // A size larger than the max cached size in central cache, but handled by PageCache's direct alloc
    const size_t large_size = 512 * 1024; // 512 KB

    auto span = pc.allocate_direct(large_size);
    ASSERT_TRUE(span.has_value());
    ASSERT_EQ(span.value().size(), large_size);

    pc.deallocate_direct(span.value());
}

// A simple concurrency test to ensure the mutex works.
TEST_F(PageCacheTest, ConcurrentAccess) {
    auto& pc = memory_pool::Page_Cache::get_instance();
    const size_t num_threads = 80;
    const int num_allocs_per_thread = 50;

    auto worker = [&]() {
        std::vector<memory_pool::Memory_span> spans;
        spans.reserve(num_allocs_per_thread);

        for (int i = 0; i < num_allocs_per_thread; ++i) {
            // Request a small number of pages
            auto span = pc.allocate(1);
            if (span.has_value()) {
                spans.push_back(span.value());
            }
        }

        for (const auto& span : spans) {
            pc.deallocate(span);
        }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    SUCCEED() << "Concurrent access test completed without crashing.";
}
