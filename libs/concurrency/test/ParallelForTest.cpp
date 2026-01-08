#include <catch2/catch_test_macros.hpp>
#include "ParallelFor.h"
#include "ParallelExecutors.h"
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>
#include <set>
#include <chrono>
#include <cmath>

using namespace concurrency;

TEST_CASE("parallel_for basic operations", "[parallel_for]")
{
  SECTION("Basic execution with SingleThreadExecutor")
  {
    SingleThreadExecutor executor;
    std::vector<int> results(10, 0);
    
    parallel_for(10, executor, [&results](uint32_t i) {
      results[i] = i * 2;
    });
    
    for (uint32_t i = 0; i < 10; ++i) {
      REQUIRE(results[i] == static_cast<int>(i * 2));
    }
  }
  
  SECTION("Basic execution with ThreadPoolExecutor")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    parallel_for(100, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    });
    
    REQUIRE(counter.load() == 100);
  }
  
  SECTION("Zero iterations")
  {
    SingleThreadExecutor executor;
    std::atomic<int> counter{0};
    
    parallel_for(0, executor, [&counter](uint32_t) {
      counter.fetch_add(1);
    });
    
    REQUIRE(counter.load() == 0);
  }
  
  SECTION("Single iteration")
  {
    SingleThreadExecutor executor;
    std::vector<int> results(1, 0);
    
    parallel_for(1, executor, [&results](uint32_t i) {
      results[i] = 42;
    });
    
    REQUIRE(results[0] == 42);
  }
  
  SECTION("All indices are visited exactly once")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<std::atomic<int>> visited(1000);
    for (auto& v : visited) v.store(0);
    
    parallel_for(1000, executor, [&visited](uint32_t i) {
      visited[i].fetch_add(1, std::memory_order_relaxed);
    });
    
    for (uint32_t i = 0; i < 1000; ++i) {
      REQUIRE(visited[i].load() == 1);
    }
  }
  
  SECTION("Deterministic execution with SingleThreadExecutor")
  {
    SingleThreadExecutor executor;
    std::vector<int> executionOrder;
    
    parallel_for(10, executor, [&executionOrder](uint32_t i) {
      executionOrder.push_back(i);
    });
    
    // With single thread, should execute in order
    REQUIRE(executionOrder.size() == 10);
    REQUIRE(std::is_sorted(executionOrder.begin(), executionOrder.end()));
  }
  
  SECTION("Concurrent execution with ThreadPoolExecutor")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> concurrentCount{0};
    std::atomic<int> maxConcurrent{0};
    
    parallel_for(8, executor, [&concurrentCount, &maxConcurrent](uint32_t) {
      int current = concurrentCount.fetch_add(1) + 1;
      
      int expected = maxConcurrent.load();
      while (expected < current && 
             !maxConcurrent.compare_exchange_weak(expected, current)) {
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      concurrentCount.fetch_sub(1);
    });
    
    // With 4 threads and 8 tasks, should see some concurrency
    REQUIRE(maxConcurrent.load() >= 2);
  }
  
  SECTION("Large iteration count")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<uint64_t> sum{0};
    const uint32_t N = 10000;
    
    parallel_for(N, executor, [&sum](uint32_t i) {
      sum.fetch_add(i, std::memory_order_relaxed);
    });
    
    uint64_t expectedSum = (uint64_t)N * (N - 1) / 2;
    REQUIRE(sum.load() == expectedSum);
  }
  
  SECTION("Body function with mutable state")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<std::atomic<int>> counters(100);
    for (auto& c : counters) c.store(0);
    
    parallel_for(100, executor, [&counters](uint32_t i) {
      // Each iteration increments its own counter
      counters[i].fetch_add(1);
      counters[i].fetch_add(1);
      counters[i].fetch_add(1);
    });
    
    for (const auto& c : counters) {
      REQUIRE(c.load() == 3);
    }
  }
  
  SECTION("Works with different executor types")
  {
    std::atomic<int> counter{0};
    const uint32_t N = 50;
    
    // SingleThreadExecutor
    {
      SingleThreadExecutor exec;
      counter.store(0);
      parallel_for(N, exec, [&counter](uint32_t) { counter.fetch_add(1); });
      REQUIRE(counter.load() == N);
    }
    
    // StdAsyncExecutor
    {
      StdAsyncExecutor exec;
      counter.store(0);
      parallel_for(N, exec, [&counter](uint32_t) { counter.fetch_add(1); });
      REQUIRE(counter.load() == N);
    }
    
    // ThreadPoolExecutor
    {
      ThreadPoolExecutor<2> exec;
      counter.store(0);
      parallel_for(N, exec, [&counter](uint32_t) { counter.fetch_add(1); });
      REQUIRE(counter.load() == N);
    }
  }
}

TEST_CASE("parallel_for_each basic operations", "[parallel_for_each]")
{
  SECTION("Basic vector iteration")
  {
    SingleThreadExecutor executor;
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::atomic<int> sum{0};
    
    parallel_for_each(executor, data, [&sum](int value) {
      sum.fetch_add(value, std::memory_order_relaxed);
    });
    
    REQUIRE(sum.load() == 15);
  }
  
  SECTION("Empty container")
  {
    SingleThreadExecutor executor;
    std::vector<int> empty;
    std::atomic<int> counter{0};
    
    parallel_for_each(executor, empty, [&counter](int) {
      counter.fetch_add(1);
    });
    
    REQUIRE(counter.load() == 0);
  }
  
  SECTION("Single element container")
  {
    SingleThreadExecutor executor;
    std::vector<int> single = {42};
    std::atomic<int> result{0};
    
    parallel_for_each(executor, single, [&result](int value) {
      result.store(value);
    });
    
    REQUIRE(result.load() == 42);
  }
  
  SECTION("Large container with ThreadPoolExecutor")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 0);
    std::atomic<uint64_t> sum{0};
    
    parallel_for_each(executor, data, [&sum](int value) {
      sum.fetch_add(value, std::memory_order_relaxed);
    });
    
    uint64_t expectedSum = 1000ULL * 999 / 2;
    REQUIRE(sum.load() == expectedSum);
  }
  
  SECTION("All elements visited exactly once")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<int> data(500);
    std::iota(data.begin(), data.end(), 0);
    std::vector<std::atomic<int>> visited(500);
    for (auto& v : visited) v.store(0);
    
    parallel_for_each(executor, data, [&visited](int value) {
      visited[value].fetch_add(1, std::memory_order_relaxed);
    });
    
    for (int i = 0; i < 500; ++i) {
      REQUIRE(visited[i].load() == 1);
    }
  }
  
  SECTION("Works with const container")
  {
    ThreadPoolExecutor<2> executor;
    const std::vector<int> data = {10, 20, 30, 40, 50};
    std::atomic<int> sum{0};
    
    parallel_for_each(executor, data, [&sum](int value) {
      sum.fetch_add(value, std::memory_order_relaxed);
    });
    
    REQUIRE(sum.load() == 150);
  }
  
  SECTION("Works with different container element types")
  {
    ThreadPoolExecutor<2> executor;
    std::vector<double> doubles = {1.5, 2.5, 3.5, 4.5};
    std::atomic<int> count{0};
    
    parallel_for_each(executor, doubles, [&count](double) {
      count.fetch_add(1);
    });
    
    REQUIRE(count.load() == 4);
  }
  
  SECTION("Body function can read element values")
  {
    SingleThreadExecutor executor;
    std::vector<int> data = {5, 10, 15, 20};
    std::vector<int> results(4, 0);
    
    parallel_for_each(executor, data, [&results, &data](int value) {
      auto it = std::find(data.begin(), data.end(), value);
      size_t idx = std::distance(data.begin(), it);
      results[idx] = value * 2;
    });
    
    REQUIRE(results[0] == 10);
    REQUIRE(results[1] == 20);
    REQUIRE(results[2] == 30);
    REQUIRE(results[3] == 40);
  }
  
  SECTION("Concurrent processing with ThreadPoolExecutor")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<int> data(8, 1);
    std::atomic<int> concurrentCount{0};
    std::atomic<int> maxConcurrent{0};
    
    parallel_for_each(executor, data, [&concurrentCount, &maxConcurrent](int) {
      int current = concurrentCount.fetch_add(1) + 1;
      
      int expected = maxConcurrent.load();
      while (expected < current && 
             !maxConcurrent.compare_exchange_weak(expected, current)) {
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      concurrentCount.fetch_sub(1);
    });
    
    REQUIRE(maxConcurrent.load() >= 2);
  }
  
  SECTION("Works with string container")
  {
    ThreadPoolExecutor<2> executor;
    std::vector<std::string> strings = {"hello", "world", "test"};
    std::atomic<size_t> totalLength{0};
    
    parallel_for_each(executor, strings, [&totalLength](const std::string& s) {
      totalLength.fetch_add(s.length(), std::memory_order_relaxed);
    });
    
    REQUIRE(totalLength.load() == 14); // 5 + 5 + 4
  }
}

TEST_CASE("parallel_for_chunked basic operations", "[parallel_for_chunked]")
{
  SECTION("Basic execution with auto chunk size")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    parallel_for_chunked(100, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    });
    
    REQUIRE(counter.load() == 100);
  }
  
  SECTION("Custom chunk size")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<std::atomic<int>> visited(1000);
    for (auto& v : visited) v.store(0);
    
    parallel_for_chunked(1000, executor, [&visited](uint32_t i) {
      visited[i].fetch_add(1, std::memory_order_relaxed);
    }, 50); // chunk size = 50
    
    for (uint32_t i = 0; i < 1000; ++i) {
      REQUIRE(visited[i].load() == 1);
    }
  }
  
  SECTION("Zero iterations")
  {
    SingleThreadExecutor executor;
    std::atomic<int> counter{0};
    
    parallel_for_chunked(0, executor, [&counter](uint32_t) {
      counter.fetch_add(1);
    });
    
    REQUIRE(counter.load() == 0);
  }
  
  SECTION("Single iteration")
  {
    SingleThreadExecutor executor;
    std::vector<int> results(1, 0);
    
    parallel_for_chunked(1, executor, [&results](uint32_t i) {
      results[i] = 99;
    });
    
    REQUIRE(results[0] == 99);
  }
  
  SECTION("Very small chunk size creates many tasks")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    // 1000 iterations with chunk size 10 = 100 tasks
    parallel_for_chunked(1000, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    }, 10);
    
    REQUIRE(counter.load() == 1000);
  }
  
  SECTION("Very large chunk size creates few tasks")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    // 1000 iterations with chunk size 500 = 2 tasks
    parallel_for_chunked(1000, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    }, 500);
    
    REQUIRE(counter.load() == 1000);
  }
  
  SECTION("All indices visited exactly once")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<std::atomic<int>> visited(2000);
    for (auto& v : visited) v.store(0);
    
    parallel_for_chunked(2000, executor, [&visited](uint32_t i) {
      visited[i].fetch_add(1, std::memory_order_relaxed);
    }, 100);
    
    for (uint32_t i = 0; i < 2000; ++i) {
      REQUIRE(visited[i].load() == 1);
    }
  }
  
  SECTION("Auto-calculated chunk size for small total")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    // Small total should still use minimum chunk size (512)
    parallel_for_chunked(100, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    });
    
    REQUIRE(counter.load() == 100);
  }
  
  SECTION("Auto-calculated chunk size for large total")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<uint64_t> sum{0};
    const uint32_t N = 100000;
    
    parallel_for_chunked(N, executor, [&sum](uint32_t i) {
      sum.fetch_add(i, std::memory_order_relaxed);
    });
    
    uint64_t expectedSum = (uint64_t)N * (N - 1) / 2;
    REQUIRE(sum.load() == expectedSum);
  }
  
  SECTION("Load balancing with variable work")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<std::atomic<int>> counters(1000);
    for (auto& c : counters) c.store(0);
    
    // Simulate variable work - indices divisible by 100 take longer
    parallel_for_chunked(1000, executor, [&counters](uint32_t i) {
      if (i % 100 == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      counters[i].fetch_add(1, std::memory_order_relaxed);
    }, 50); // Small chunks for better load balancing
    
    for (uint32_t i = 0; i < 1000; ++i) {
      REQUIRE(counters[i].load() == 1);
    }
  }
  
  SECTION("Works with different executor types")
  {
    std::atomic<int> counter{0};
    const uint32_t N = 100;
    
    // SingleThreadExecutor
    {
      SingleThreadExecutor exec;
      counter.store(0);
      parallel_for_chunked(N, exec, [&counter](uint32_t) { 
        counter.fetch_add(1); 
      }, 10);
      REQUIRE(counter.load() == N);
    }
    
    // StdAsyncExecutor
    {
      StdAsyncExecutor exec;
      counter.store(0);
      parallel_for_chunked(N, exec, [&counter](uint32_t) { 
        counter.fetch_add(1); 
      }, 10);
      REQUIRE(counter.load() == N);
    }
    
    // ThreadPoolExecutor
    {
      ThreadPoolExecutor<2> exec;
      counter.store(0);
      parallel_for_chunked(N, exec, [&counter](uint32_t) { 
        counter.fetch_add(1); 
      }, 10);
      REQUIRE(counter.load() == N);
    }
  }
  
  SECTION("Chunk size of 1 creates one task per iteration")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<std::atomic<int>> visited(50);
    for (auto& v : visited) v.store(0);
    
    parallel_for_chunked(50, executor, [&visited](uint32_t i) {
      visited[i].fetch_add(1, std::memory_order_relaxed);
    }, 1);
    
    for (uint32_t i = 0; i < 50; ++i) {
      REQUIRE(visited[i].load() == 1);
    }
  }
  
  SECTION("Body function with thread-local state simulation")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<std::atomic<int>> results(100);
    for (auto& r : results) r.store(0);
    
    parallel_for_chunked(100, executor, [&results](uint32_t i) {
      // Simulate thread-local computation
      int localSum = 0;
      for (int j = 0; j < 10; ++j) {
        localSum += j;
      }
      results[i].store(localSum, std::memory_order_relaxed);
    }, 25);
    
    for (uint32_t i = 0; i < 100; ++i) {
      REQUIRE(results[i].load() == 45); // sum(0..9) = 45
    }
  }
}

TEST_CASE("parallel_for edge cases and stress tests", "[parallel_for][stress]")
{
  SECTION("Very large iteration count")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<uint64_t> sum{0};
    const uint32_t N = 1000000;
    
    parallel_for(N, executor, [&sum](uint32_t i) {
      if (i % 1000 == 0) { // Sample every 1000th element to reduce overhead
        sum.fetch_add(i, std::memory_order_relaxed);
      }
    });
    
    // Verify some processing occurred
    REQUIRE(sum.load() > 0);
  }
  
  SECTION("Index values are correct at boundaries")
  {
    ThreadPoolExecutor<4> executor;
    const uint32_t N = 100;
    std::vector<std::atomic<bool>> visited(N);
    for (auto& v : visited) v.store(false);
    
    parallel_for(N, executor, [&visited, N](uint32_t i) {
      REQUIRE(i < N); // Should never be out of bounds
      visited[i].store(true, std::memory_order_relaxed);
    });
    
    for (uint32_t i = 0; i < N; ++i) {
      REQUIRE(visited[i].load());
    }
  }
  
  SECTION("Non-power-of-two iteration counts")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<uint32_t> testSizes = {7, 13, 99, 101, 997, 1001};
    
    for (uint32_t size : testSizes) {
      std::atomic<int> counter{0};
      parallel_for(size, executor, [&counter](uint32_t) {
        counter.fetch_add(1, std::memory_order_relaxed);
      });
      REQUIRE(counter.load() == static_cast<int>(size));
    }
  }
}

TEST_CASE("parallel_for_chunked edge cases and stress tests", "[parallel_for_chunked][stress]")
{
  SECTION("Total not evenly divisible by chunk size")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    // 1000 iterations, chunk size 333 = 4 chunks (333, 333, 333, 1)
    parallel_for_chunked(1000, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    }, 333);
    
    REQUIRE(counter.load() == 1000);
  }
  
  SECTION("Chunk size larger than total")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    // 100 iterations, chunk size 1000 = 1 chunk
    parallel_for_chunked(100, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    }, 1000);
    
    REQUIRE(counter.load() == 100);
  }
  
  SECTION("Prime number iterations with various chunk sizes")
  {
    ThreadPoolExecutor<4> executor;
    const uint32_t prime = 997;
    std::vector<uint32_t> chunkSizes = {1, 10, 100, 500, 1000};
    
    for (uint32_t chunkSize : chunkSizes) {
      std::atomic<int> counter{0};
      parallel_for_chunked(prime, executor, [&counter](uint32_t) {
        counter.fetch_add(1, std::memory_order_relaxed);
      }, chunkSize);
      REQUIRE(counter.load() == prime);
    }
  }
}

TEST_CASE("Comparison: parallel_for vs parallel_for_chunked", "[comparison]")
{
  SECTION("Both produce same results for uniform work")
  {
    ThreadPoolExecutor<4> executor;
    const uint32_t N = 1000;
    
    std::atomic<uint64_t> sum1{0};
    parallel_for(N, executor, [&sum1](uint32_t i) {
      sum1.fetch_add(i, std::memory_order_relaxed);
    });
    
    std::atomic<uint64_t> sum2{0};
    parallel_for_chunked(N, executor, [&sum2](uint32_t i) {
      sum2.fetch_add(i, std::memory_order_relaxed);
    }, 100);
    
    REQUIRE(sum1.load() == sum2.load());
    
    uint64_t expectedSum = (uint64_t)N * (N - 1) / 2;
    REQUIRE(sum1.load() == expectedSum);
  }
  
  SECTION("parallel_for_chunked better handles variable work")
  {
    ThreadPoolExecutor<4> executor;
    const uint32_t N = 100;
    
    auto variableWork = [](uint32_t i) {
      // Some indices take longer
      if (i % 10 == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    };
    
    // Both should complete successfully
    std::atomic<int> counter1{0};
    parallel_for(N, executor, [&counter1, variableWork](uint32_t i) {
      variableWork(i);
      counter1.fetch_add(1);
    });
    REQUIRE(counter1.load() == N);
    
    std::atomic<int> counter2{0};
    parallel_for_chunked(N, executor, [&counter2, variableWork](uint32_t i) {
      variableWork(i);
      counter2.fetch_add(1);
    }, 10);
    REQUIRE(counter2.load() == N);
  }
}

TEST_CASE("Thread safety verification", "[thread_safety]")
{
  SECTION("Concurrent writes to separate indices are safe")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<int> data(1000, 0);
    
    parallel_for(1000, executor, [&data](uint32_t i) {
      data[i] = i * 2; // Each thread writes to distinct index
    });
    
    for (uint32_t i = 0; i < 1000; ++i) {
      REQUIRE(data[i] == static_cast<int>(i * 2));
    }
  }
  
  SECTION("Atomic operations are thread-safe")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    parallel_for(10000, executor, [&counter](uint32_t) {
      counter.fetch_add(1, std::memory_order_relaxed);
    });
    
    REQUIRE(counter.load() == 10000);
  }
}
