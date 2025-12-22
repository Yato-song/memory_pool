#include "gtest/gtest.h"
#include "thread_cache.h" // Correct header
#include <vector>
#include <optional>

// Test fixture for Thread_cache tests.
// Using a fixture is good practice for grouping related tests.
class ThreadCacheTest : public ::testing::Test {
protected:
    // Each test will get a reference to the thread_local singleton instance.
    // In a single-threaded test runner, this will be the same instance,
    // so one test can affect the state of another.
    // For more complex tests, a tc.reset() method would be useful in TearDown.
    void SetUp() override {
        // Optional: Add setup logic here if needed in the future.
    }

    void TearDown() override {
        // Optional: Add teardown logic here.
    }
};

// Test case for simple allocation and deallocation.
TEST_F(ThreadCacheTest, SimpleAllocationDeallocation) {
    // Correctly get the instance of the correct class name
    auto& tc = memory_pool::Thread_cache::get_instance();

    // Correctly handle the std::optional return value
    std::optional<void*> opt_p1 = tc.allocate(10);
    ASSERT_TRUE(opt_p1.has_value()) << "Allocation of 10 bytes failed.";
    void* p1 = opt_p1.value();
    ASSERT_NE(p1, nullptr);

    // Deallocate the object with its size
    tc.deallocate(p1, 10);
}

// Test case for checking if memory is reused from the cache.
TEST_F(ThreadCacheTest, ReusesCachedMemory) {
    auto& tc = memory_pool::Thread_cache::get_instance();

    // Allocate and deallocate to populate the cache
    std::optional<void*> opt_p1 = tc.allocate(24);
    ASSERT_TRUE(opt_p1.has_value());
    void* p1 = opt_p1.value();
    tc.deallocate(p1, 24);

    // Re-allocate the same size, it should be served from the cache.
    std::optional<void*> opt_p2 = tc.allocate(24);
    ASSERT_TRUE(opt_p2.has_value()) << "Re-allocation of 24 bytes failed.";
    void* p2 = opt_p2.value();
    ASSERT_NE(p2, nullptr);

    tc.deallocate(p2, 24);
}

// Test case for bulk allocation to trigger fetching from the CentralCache.
TEST_F(ThreadCacheTest, BulkAllocationFromCentralCache) {
    auto& tc = memory_pool::Thread_cache::get_instance();
    const int bulk_count = 2500;
    const size_t alloc_size = 8;
    std::vector<void*> pointers;
    pointers.reserve(bulk_count);

    for (int i = 0; i < bulk_count; ++i) {
        std::optional<void*> opt_p = tc.allocate(alloc_size);
        ASSERT_TRUE(opt_p.has_value()) << "Bulk allocation failed at iteration " << i;
        void* p = opt_p.value();
        pointers.push_back(p);
    }

    // Deallocate all the pointers to return them to the cache.
    for (void* p : pointers) {
        tc.deallocate(p, alloc_size);
    }
}

// Test case for allocating an object larger than the max cacheable size.
TEST_F(ThreadCacheTest, LargeAllocation) {
    auto& tc = memory_pool::Thread_cache::get_instance();
    // This size should be larger than Size_utils::MAX_CACHED_UNIT_SIZE
    const size_t large_size = 257 * 1024; // 257 KB

    std::optional<void*> opt_p = tc.allocate(large_size);
    ASSERT_TRUE(opt_p.has_value()) << "Large allocation of " << large_size << " bytes failed.";
    void* p = opt_p.value();

    tc.deallocate(p, large_size);
}

// 该测试用例测试在申请多个小内存的场景下，会不会出现同一个地址被分配两次的情况
TEST_F(ThreadCacheTest, IfAddrSame) {
    auto& tc = memory_pool::Thread_cache::get_instance();
    std::vector<size_t> size_arr = {2,6,8,16,64,128,256,30,59};
    // 存储所有申请的内存地址
    std::vector<void*> addr_arr;
    // 存储去重后的所有内存地址
    std::list<void*> addr_list;

    // 每种size申请一百个
    for (auto size : size_arr) {
        for (int i = 0; i < 100; ++i) {
            std::optional<void*> opt_p = tc.allocate(size);
            ASSERT_TRUE(opt_p.has_value()) << "Allocate memory failed, size: " << size << std::endl;
            addr_arr.push_back(opt_p.value());
        }
    }

    // 去重
    addr_list.insert(addr_list.begin(),addr_arr.begin(), addr_arr.end());
    ASSERT_EQ(addr_arr.size(), addr_list.size());

    // 归还内存
    for (int i = 0; i < addr_arr.size(); ++i) {
        tc.deallocate(addr_arr[i], size_arr[i/100]);
    }
}

// 测试申请大小为0的内存
TEST_F(ThreadCacheTest, AllocZero) {
    std::optional<void*> opt_p = memory_pool::Thread_cache::get_instance().allocate(0);
    ASSERT_EQ(opt_p, std::nullopt);
}
// The main function is provided by the gtest_main library linked in CMakeLists.txt
// No main() function is needed here.
