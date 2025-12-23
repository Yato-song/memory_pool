#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <iomanip>
#include "thread_cache.h"

// Configuration for the benchmark
const int NUM_THREADS = 15;           // Number of concurrent threads
const int NUM_ROUNDS = 500000;        // Number of allocation/deallocation rounds per thread
const int MAX_ALLOC_SIZE = 10 * 1024;      // Maximum size of allocation (bytes)

// Helper function to generate random sizes
size_t get_random_size() {
    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::uniform_int_distribution<size_t> dist(8, MAX_ALLOC_SIZE);
    return dist(gen);
}

// Benchmark function for Standard Allocator (new/delete)
void benchmark_std(std::atomic<long long>& total_duration_ns) {
    std::vector<void*> ptrs;
    ptrs.reserve(NUM_ROUNDS);
    std::vector<size_t> sizes;
    sizes.reserve(NUM_ROUNDS);

    // Pre-calculate sizes to ensure fair comparison (exclude RNG time from measurement if possible,
    // but here we include it to simulate real workload overhead, or pre-calc it).
    // Let's pre-calc to measure pure allocation speed.
    for (int i = 0; i < NUM_ROUNDS; ++i) {
        sizes.push_back(get_random_size());
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ROUNDS; ++i) {
        // Allocate
        void* p = ::operator new(sizes[i]);
        ptrs.push_back(p);
    }

    for (int i = 0; i < NUM_ROUNDS; ++i) {
        // Deallocate
        ::operator delete(ptrs[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    total_duration_ns += duration;
}

// Benchmark function for Memory Pool (Thread_cache)
void benchmark_pool(std::atomic<long long>& total_duration_ns) {
    auto& tc = memory_pool::Thread_cache::get_instance();
    std::vector<void*> ptrs;
    ptrs.reserve(NUM_ROUNDS);
    std::vector<size_t> sizes;
    sizes.reserve(NUM_ROUNDS);

    for (int i = 0; i < NUM_ROUNDS; ++i) {
        sizes.push_back(get_random_size());
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ROUNDS; ++i) {
        // Allocate
        auto res = tc.allocate(sizes[i]);
        if (res.has_value()) {
            ptrs.push_back(res.value());
        }
    }

    for (int i = 0; i < ptrs.size(); ++i) {
        // Deallocate (using the corresponding size)
        tc.deallocate(ptrs[i], sizes[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    total_duration_ns += duration;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "      Memory Pool Performance Benchmark" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Threads: " << NUM_THREADS << std::endl;
    std::cout << "Rounds per thread: " << NUM_ROUNDS << std::endl;
    std::cout << "Total Allocations: " << NUM_THREADS * NUM_ROUNDS << std::endl;
    std::cout << "Max Allocation Size: " << MAX_ALLOC_SIZE << " bytes" << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    // --- Run Standard Allocator Benchmark ---
    {
        std::atomic<long long> total_duration_ns = 0;
        std::vector<std::thread> threads;

        std::cout << "Running Standard Allocator (new/delete)... " << std::flush;

        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(benchmark_std, std::ref(total_duration_ns));
        }

        for (auto& t : threads) {
            t.join();
        }

        double avg_time_ms = (double)total_duration_ns / NUM_THREADS / 1000000.0;
        std::cout << "Done." << std::endl;
        std::cout << "Standard Allocator Avg Time: " << std::fixed << std::setprecision(4) << avg_time_ms << " ms" << std::endl;

        // Store for comparison
        // Note: This is a simplified metric. Total throughput is usually Total Ops / Max(Thread Time).
        // Here we just look at average thread execution time.
    }

    std::cout << "--------------------------------------------------" << std::endl;

    // --- Run Memory Pool Benchmark ---
    {
        std::atomic<long long> total_duration_ns = 0;
        std::vector<std::thread> threads;

        std::cout << "Running Memory Pool (Thread_cache)...      " << std::flush;

        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(benchmark_pool, std::ref(total_duration_ns));
        }

        for (auto& t : threads) {
            t.join();
        }

        double avg_time_ms = (double)total_duration_ns / NUM_THREADS / 1000000.0;
        std::cout << "Done." << std::endl;
        std::cout << "Memory Pool Avg Time:        " << std::fixed << std::setprecision(4) << avg_time_ms << " ms" << std::endl;
    }

    std::cout << "==================================================" << std::endl;

    return 0;
}
