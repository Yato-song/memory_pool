#include "gtest/gtest.h"
#include "central_cache.h"
#include <thread>
#include <vector>
#include <list>
#include <numeric>
#include <random>
#include <chrono>
#include <iostream>


// TEST(SanityCheck, CentralCacheFileIsSeen) {
//     SUCCEED();
// }

// Test fixture for Central_cache tests
class CentralCacheFixture : public ::testing::Test {
protected:
    // Central_cache is a singleton, so we get its instance in each test.
    // State can persist between tests. For complex scenarios, a reset mechanism
    // on the singleton would be beneficial.
    void SetUp() override {
        // Optional: Add setup logic here if needed in the future.
    }

    void TearDown() override {
        // Optional: Add teardown logic here.
    }
};

// Test case for a single, basic allocation.
// This test verifies that Central_cache can successfully get memory from Page_cache.
TEST_F(CentralCacheFixture, BasicAllocation) {
    auto& cc = memory_pool::Central_cache::get_instance();
    const size_t alloc_size = 16; // Requesting 16-byte chunks
    const size_t span_count = 5;  // Requesting 5 chunks

    std::optional<std::list<memory_pool::Memory_span>> result = cc.allocate(alloc_size, span_count);

    // Assert that the allocation was successful
    ASSERT_TRUE(result.has_value()) << "Allocation failed, optional should have a value.";

    // Assert that we received the correct number of spans
    ASSERT_EQ(result.value().size(), span_count) << "Expected " << span_count << " spans, but got " << result.value().size();

    // Deallocate to clean up
    cc.deallocate(result.value());
}

// Test case to verify that deallocated memory is reused.
TEST_F(CentralCacheFixture, DeallocateAndReallocate) {
    /*
     * 检验是否重复使用的逻辑：最一开始向central_cache申请内存时，central_cache一定会向page_cache申请一个page（4096 bytes）。
     * step1: 申请一小块内存，不归还，来保证central_cache不会把整个page归还给page_cache
     * step2: 循环10000次申请同样大小的长度为1的span list，将每次申请到的span list的地址存储到set中，然后释放这个span list
     * step3: 判断set的size是否符合预期
     *
     * 正常情况下，申请再多次span list，set的size也不会超过某个值
     */
    auto& c_cache = memory_pool::Central_cache::get_instance();
    const size_t memory_size = 64;
    const size_t span_size = 1;
    std::set<void*> addr_set;

    // step1
    std::optional<std::list<memory_pool::Memory_span>> span_keep = c_cache.allocate(memory_size, span_size);
    ASSERT_TRUE(span_keep.has_value());

    // step2
    for (int i = 0; i < 10000; ++i) {
        std::optional<std::list<memory_pool::Memory_span>> span = c_cache.allocate(memory_size, span_size);
        ASSERT_TRUE(span.has_value());
        addr_set.insert(span.value().front().GetAddr());
        c_cache.deallocate(span.value());
    }

    // step3
    // page_size = 4096; alloc_page_number 512*64/4096 = 8；total_span 8*4096/64 - 1 = 511
    EXPECT_LE(addr_set.size(), 511) << "address num: " << addr_set.size() << " is greater then 63!" << std::endl;

    // step4
    c_cache.deallocate(span_keep.value());
}

// A simple concurrency test to check for race conditions.
TEST_F(CentralCacheFixture, ConcurrentAllocations) {
    auto& cc = memory_pool::Central_cache::get_instance();
    const size_t num_threads = 40;
    const int num_allocs_per_thread = 1000;
    const size_t alloc_size = 128;
    const size_t span_count_per_alloc = 50;

    auto worker = [&]() {
        for (int i = 0; i < num_allocs_per_thread; ++i) {
            std::optional<std::list<memory_pool::Memory_span>> result = cc.allocate(alloc_size, span_count_per_alloc);

            if (result.has_value()) {
                // sleep随机事件 模拟进程处理过程 随机sleep 1-10ms
                thread_local std::mt19937 gen{std::random_device{}()};
                std::uniform_int_distribution<int> dist(1, 10);

                using namespace std::chrono_literals;
                std::this_thread::sleep_for(dist(gen) * 1ms);
                // Deallocate immediately to stress the lock and cache reuse.
                cc.deallocate(result.value());
            }
        }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // The test passes if it completes without crashing or deadlocking.
    SUCCEED() << "Concurrent test completed without crashing.";
}

// 测试当一整个page_span空闲后 是否会退还给page_cache
TEST_F(CentralCacheFixture, DeallocTotalPage) {
    auto& cc = memory_pool::Central_cache::get_instance();
    const size_t memory_size = 64;
    const size_t span_num = 10;
    std::optional<std::list<memory_pool::Memory_span>> spans = cc.allocate(memory_size, span_num);
    // 此时page_span应该为1
    EXPECT_EQ(cc.get_map_size(memory_size), 1) << "map size should be 1, but is " << cc.get_map_size(memory_size) << std::endl;

    cc.deallocate(spans.value());
    EXPECT_EQ(cc.get_map_size(memory_size), 0) << "map size should be 0, but is " << cc.get_map_size(memory_size) << std::endl;
}

// Test for concurrent allocations of different sizes to verify fine-grained locking.
TEST_F(CentralCacheFixture, MixedSizeConcurrentAllocations) {
    auto& cc = memory_pool::Central_cache::get_instance();

    // Define a generic worker lambda that takes an allocation size.
    auto worker = [&](const size_t alloc_size, const int num_allocs, const size_t span_count) {
        for (int i = 0; i < num_allocs; ++i) {
            std::optional<std::list<memory_pool::Memory_span>> result = cc.allocate(alloc_size, span_count);
            if (result.has_value()) {
                // Simulate some work
                thread_local std::mt19937 gen{std::random_device{}()};
                std::uniform_int_distribution<int> dist(1, 5);
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(dist(gen) * 1ms);

                cc.deallocate(result.value());
            }
        }
    };

    // Define different allocation profiles
    const int num_allocs_per_thread = 500;
    std::vector<std::thread> threads;

    // Group 1: Small objects (e.g., 8 bytes)
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, 8, num_allocs_per_thread, 20);
    }

    // Group 2: Medium objects (e.g., 64 bytes)
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, 64, num_allocs_per_thread / 2, 10);
    }

    // Group 3: Large objects (e.g., 256 bytes)
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, 256, num_allocs_per_thread / 4, 5);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    SUCCEED() << "Mixed size concurrent test completed without crashing or deadlocking.";
}

// Long-running stress test with randomized allocation sizes.
// This is marked as DISABLED_ by default to prevent it from running on every build.
// You can enable it in CLion by clicking the run icon next to it.
TEST_F(CentralCacheFixture, SoakTest) {
    std::cout << "\n[          ] Starting Soak Test. This will take a while..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    auto& cc = memory_pool::Central_cache::get_instance();

    // A list of common allocation sizes to choose from randomly.
    const std::vector<size_t> alloc_sizes = {8, 16, 32, 64, 128, 256, 512, 1024};

    auto worker = [&]() {
        // Each thread gets its own random number generator.
        thread_local std::mt19937 gen{std::random_device{}()};
        std::uniform_int_distribution<size_t> size_dist(0, alloc_sizes.size() - 1);
        std::uniform_int_distribution<size_t> count_dist(1, 20); // Request 1 to 20 spans at a time
        std::uniform_int_distribution<int> sleep_dist(1, 5);

        // High number of iterations to make the test long-running.
        const int num_allocs = 50000;

        for (int i = 0; i < num_allocs; ++i) {
            size_t alloc_size = alloc_sizes[size_dist(gen)];
            size_t span_count = count_dist(gen);

            std::optional<std::list<memory_pool::Memory_span>> result = cc.allocate(alloc_size, span_count);

            if (result.has_value()) {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(sleep_dist(gen) * 1ms);
                cc.deallocate(result.value());
            }
        }
    };

    const size_t num_threads = std::thread::hardware_concurrency(); // Use all available cores
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

    std::cout << "[          ] Soak Test Finished in " << duration.count() << " seconds." << std::endl;
    SUCCEED() << "Soak test completed without crashing or deadlocking.";
}
