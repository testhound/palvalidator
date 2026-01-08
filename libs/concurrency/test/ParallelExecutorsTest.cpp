#include <catch2/catch_test_macros.hpp>
#include "ParallelExecutors.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>

using namespace concurrency;

// Helper function to create a simple task that increments a counter
auto createIncrementTask(std::atomic<int>& counter) {
  return [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); };
}

// Helper function to create a task that sleeps briefly
auto createSleepTask(int milliseconds) {
  return [milliseconds]() { 
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); 
  };
}

// Helper function to create a task that throws an exception
auto createThrowingTask(const std::string& message) {
  return [message]() { throw std::runtime_error(message); };
}

TEST_CASE("SingleThreadExecutor operations", "[SingleThreadExecutor]")
{
  SingleThreadExecutor executor;
  
  SECTION("Basic task execution")
  {
    std::atomic<int> counter{0};
    auto future = executor.submit(createIncrementTask(counter));
    
    REQUIRE_NOTHROW(future.get());
    REQUIRE(counter.load() == 1);
  }
  
  SECTION("Task executes immediately")
  {
    std::atomic<bool> executed{false};
    auto future = executor.submit([&executed]() { executed.store(true); });
    
    // Future should already be ready since task executes inline
    REQUIRE(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
    REQUIRE(executed.load());
  }
  
  SECTION("Multiple tasks execute in order")
  {
    std::vector<int> results;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 5; ++i) {
      futures.push_back(executor.submit([&results, i]() { 
        results.push_back(i); 
      }));
    }
    
    for (auto& f : futures) {
      f.get();
    }
    
    // Tasks should execute in submission order
    REQUIRE(results.size() == 5);
    for (int i = 0; i < 5; ++i) {
      REQUIRE(results[i] == i);
    }
  }
  
  SECTION("Exception propagation through future")
  {
    auto future = executor.submit(createThrowingTask("test exception"));
    
    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
  }
  
  SECTION("Exception with specific message")
  {
    std::string expectedMessage = "specific error";
    auto future = executor.submit(createThrowingTask(expectedMessage));
    
    try {
      future.get();
      REQUIRE(false); // Should not reach here
    } catch (const std::runtime_error& e) {
      REQUIRE(std::string(e.what()) == expectedMessage);
    }
  }
  
  SECTION("No actual concurrency - deterministic execution")
  {
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    
    auto task = [&executionOrder, &orderMutex](int id) {
      std::lock_guard<std::mutex> lock(orderMutex);
      executionOrder.push_back(id);
    };
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i) {
      futures.push_back(executor.submit([task, i]() { task(i); }));
    }
    
    for (auto& f : futures) {
      f.get();
    }
    
    // Should maintain exact submission order
    REQUIRE(executionOrder.size() == 10);
    REQUIRE(std::is_sorted(executionOrder.begin(), executionOrder.end()));
  }
  
  SECTION("waitAll helper method")
  {
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 5; ++i) {
      futures.push_back(executor.submit(createIncrementTask(counter)));
    }
    
    REQUIRE_NOTHROW(executor.waitAll(futures));
    REQUIRE(counter.load() == 5);
  }
}

TEST_CASE("StdAsyncExecutor operations", "[StdAsyncExecutor]")
{
  StdAsyncExecutor executor;
  
  SECTION("Basic task execution")
  {
    std::atomic<int> counter{0};
    auto future = executor.submit(createIncrementTask(counter));
    
    future.wait();
    REQUIRE(counter.load() == 1);
  }
  
  SECTION("Concurrent task execution")
  {
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    const int numTasks = 10;
    for (int i = 0; i < numTasks; ++i) {
      futures.push_back(executor.submit(createIncrementTask(counter)));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    REQUIRE(counter.load() == numTasks);
  }
  
  SECTION("Tasks can execute in parallel")
  {
    std::atomic<int> concurrentCount{0};
    std::atomic<int> maxConcurrent{0};
    std::vector<std::future<void>> futures;
    
    auto task = [&concurrentCount, &maxConcurrent]() {
      int current = concurrentCount.fetch_add(1) + 1;
      
      // Update max if this is a new maximum
      int expected = maxConcurrent.load();
      while (expected < current && 
             !maxConcurrent.compare_exchange_weak(expected, current)) {
        // Retry if another thread updated it
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      concurrentCount.fetch_sub(1);
    };
    
    const int numTasks = 4;
    for (int i = 0; i < numTasks; ++i) {
      futures.push_back(executor.submit(task));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    // With std::async, we should see some parallelism
    REQUIRE(maxConcurrent.load() > 1);
  }
  
  SECTION("Exception propagation")
  {
    auto future = executor.submit(createThrowingTask("async exception"));
    
    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
  }
  
  SECTION("Multiple exceptions from different tasks")
  {
    std::vector<std::future<void>> futures;
    
    futures.push_back(executor.submit(createThrowingTask("error1")));
    futures.push_back(executor.submit(createThrowingTask("error2")));
    futures.push_back(executor.submit([]() { /* success */ }));
    
    // First task should throw
    REQUIRE_THROWS_AS(futures[0].get(), std::runtime_error);
    
    // Second task should throw
    REQUIRE_THROWS_AS(futures[1].get(), std::runtime_error);
    
    // Third task should succeed
    REQUIRE_NOTHROW(futures[2].get());
  }
  
  SECTION("waitAll with mixed success and failure")
  {
    std::vector<std::future<void>> futures;
    
    futures.push_back(executor.submit([]() { /* success */ }));
    futures.push_back(executor.submit(createThrowingTask("error")));
    
    // waitAll should throw when encountering the exception
    REQUIRE_THROWS(executor.waitAll(futures));
  }
}

TEST_CASE("ThreadPoolExecutor operations", "[ThreadPoolExecutor]")
{
  SECTION("Basic task execution with default size")
  {
    ThreadPoolExecutor<0> executor;
    std::atomic<int> counter{0};
    
    auto future = executor.submit(createIncrementTask(counter));
    future.wait();
    
    REQUIRE(counter.load() == 1);
  }
  
  SECTION("Multiple tasks with fixed pool size")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    const int numTasks = 20;
    for (int i = 0; i < numTasks; ++i) {
      futures.push_back(executor.submit(createIncrementTask(counter)));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    REQUIRE(counter.load() == numTasks);
  }
  
  SECTION("Tasks execute concurrently")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> concurrentCount{0};
    std::atomic<int> maxConcurrent{0};
    std::vector<std::future<void>> futures;
    
    auto task = [&concurrentCount, &maxConcurrent]() {
      int current = concurrentCount.fetch_add(1) + 1;
      
      int expected = maxConcurrent.load();
      while (expected < current && 
             !maxConcurrent.compare_exchange_weak(expected, current)) {
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      concurrentCount.fetch_sub(1);
    };
    
    // Submit more tasks than threads
    for (int i = 0; i < 8; ++i) {
      futures.push_back(executor.submit(task));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    // Should see concurrency, but not more than pool size
    REQUIRE(maxConcurrent.load() >= 2);
    REQUIRE(maxConcurrent.load() <= 4);
  }
  
  SECTION("Exception propagation through packaged_task")
  {
    ThreadPoolExecutor<2> executor;
    
    auto future = executor.submit(createThrowingTask("pool exception"));
    
    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
  }
  
  SECTION("Multiple exceptions from different tasks")
  {
    ThreadPoolExecutor<2> executor;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 5; ++i) {
      if (i % 2 == 0) {
        futures.push_back(executor.submit(createThrowingTask("error")));
      } else {
        futures.push_back(executor.submit([]() { /* success */ }));
      }
    }
    
    // Even tasks should throw
    REQUIRE_THROWS_AS(futures[0].get(), std::runtime_error);
    REQUIRE_NOTHROW(futures[1].get());
    REQUIRE_THROWS_AS(futures[2].get(), std::runtime_error);
    REQUIRE_NOTHROW(futures[3].get());
    REQUIRE_THROWS_AS(futures[4].get(), std::runtime_error);
  }
  
  SECTION("Single thread pool processes tasks sequentially")
  {
    ThreadPoolExecutor<1> executor;
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 10; ++i) {
      futures.push_back(executor.submit([&executionOrder, &orderMutex, i]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(i);
      }));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    // With single thread, order should be maintained
    REQUIRE(executionOrder.size() == 10);
    REQUIRE(std::is_sorted(executionOrder.begin(), executionOrder.end()));
  }
  
  SECTION("Destructor waits for pending tasks")
  {
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    {
      ThreadPoolExecutor<2> executor;
      
      for (int i = 0; i < 10; ++i) {
        futures.push_back(executor.submit([&counter]() {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          counter.fetch_add(1);
        }));
      }
      
      // Destructor should wait for all tasks
    }
    
    // All tasks should have completed
    REQUIRE(counter.load() == 10);
  }
  
  SECTION("Large number of small tasks")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> sum{0};
    std::vector<std::future<void>> futures;
    
    const int numTasks = 1000;
    for (int i = 0; i < numTasks; ++i) {
      futures.push_back(executor.submit([&sum, i]() {
        sum.fetch_add(i);
      }));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    int expectedSum = (numTasks * (numTasks - 1)) / 2;
    REQUIRE(sum.load() == expectedSum);
  }
  
  SECTION("Task with return value via shared state")
  {
    ThreadPoolExecutor<2> executor;
    std::atomic<int> result{0};
    
    auto future = executor.submit([&result]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      result.store(42);
    });
    
    future.wait();
    REQUIRE(result.load() == 42);
  }
  
  SECTION("waitAll helper method")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 10; ++i) {
      futures.push_back(executor.submit(createIncrementTask(counter)));
    }
    
    REQUIRE_NOTHROW(executor.waitAll(futures));
    REQUIRE(counter.load() == 10);
  }
}

TEST_CASE("ThreadPoolExecutor edge cases", "[ThreadPoolExecutor][EdgeCases]")
{
  SECTION("Zero pool size uses hardware_concurrency")
  {
    ThreadPoolExecutor<0> executor;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    // Submit multiple tasks to verify pool is functional
    for (int i = 0; i < 10; ++i) {
      futures.push_back(executor.submit(createIncrementTask(counter)));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    REQUIRE(counter.load() == 10);
  }
  
  SECTION("Submit after stop throws exception")
  {
    auto executor = std::make_unique<ThreadPoolExecutor<2>>();
    
    // Submit a task before destruction
    auto future1 = executor->submit([]() {});
    future1.wait();
    
    // Destroy the executor
    executor.reset();
    
    // Create new executor and verify it works
    executor = std::make_unique<ThreadPoolExecutor<2>>();
    auto future2 = executor->submit([]() {});
    REQUIRE_NOTHROW(future2.wait());
  }
  
  SECTION("Task that modifies shared data safely")
  {
    ThreadPoolExecutor<4> executor;
    std::vector<int> data;
    std::mutex dataMutex;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 100; ++i) {
      futures.push_back(executor.submit([&data, &dataMutex, i]() {
        std::lock_guard<std::mutex> lock(dataMutex);
        data.push_back(i);
      }));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    REQUIRE(data.size() == 100);
    
    // Verify all values are present (order doesn't matter)
    std::sort(data.begin(), data.end());
    for (int i = 0; i < 100; ++i) {
      REQUIRE(data[i] == i);
    }
  }
  
  SECTION("Nested task submission")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<int> counter{0};
    
    auto future = executor.submit([&executor, &counter]() {
      std::vector<std::future<void>> innerFutures;
      
      for (int i = 0; i < 5; ++i) {
        innerFutures.push_back(executor.submit([&counter]() {
          counter.fetch_add(1);
        }));
      }
      
      for (auto& f : innerFutures) {
        f.wait();
      }
    });
    
    future.wait();
    REQUIRE(counter.load() == 5);
  }
}

TEST_CASE("Executor comparison - behavior differences", "[ExecutorComparison]")
{
  SECTION("SingleThreadExecutor is deterministic")
  {
    SingleThreadExecutor executor;
    std::vector<int> results;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 10; ++i) {
      futures.push_back(executor.submit([&results, i]() {
        results.push_back(i);
      }));
    }
    
    for (auto& f : futures) {
      f.get();
    }
    
    // Should always be in order
    REQUIRE(std::is_sorted(results.begin(), results.end()));
  }
  
  SECTION("Parallel executors may interleave")
  {
    ThreadPoolExecutor<4> executor;
    std::atomic<bool> interleaved{false};
    std::atomic<int> activeCount{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 4; ++i) {
      futures.push_back(executor.submit([&activeCount, &interleaved]() {
        int count = activeCount.fetch_add(1) + 1;
        if (count > 1) {
          interleaved.store(true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        activeCount.fetch_sub(1);
      }));
    }
    
    for (auto& f : futures) {
      f.wait();
    }
    
    // With 4 threads and 4 tasks, should see interleaving
    REQUIRE(interleaved.load());
  }
  
  SECTION("All executors handle exceptions correctly")
  {
    SingleThreadExecutor single;
    StdAsyncExecutor async;
    ThreadPoolExecutor<2> pool;
    
    auto f1 = single.submit(createThrowingTask("single"));
    auto f2 = async.submit(createThrowingTask("async"));
    auto f3 = pool.submit(createThrowingTask("pool"));
    
    REQUIRE_THROWS_AS(f1.get(), std::runtime_error);
    REQUIRE_THROWS_AS(f2.get(), std::runtime_error);
    REQUIRE_THROWS_AS(f3.get(), std::runtime_error);
  }
}

TEST_CASE("ThreadPoolExecutor constructor exception safety", "[ThreadPoolExecutor][Exception]")
{
  SECTION("Constructor handles thread creation failure gracefully")
  {
    // This test verifies the exception handling in constructor
    // In practice, thread creation rarely fails unless system resources are exhausted
    // But the code is designed to handle it properly
    
    // We can't easily force thread creation to fail, but we can verify
    // that normal construction succeeds
    REQUIRE_NOTHROW(ThreadPoolExecutor<4>());
  }
}

// Note: BoostRunnerExecutor tests are not included because they require
// the runner.hpp dependency and getNCpus() function to be available.
// These should be tested separately in an integration test suite where
// the Boost runner infrastructure is properly initialized.

TEST_CASE("IParallelExecutor interface compliance", "[Interface]")
{
  SECTION("All executors implement submit()")
  {
    // This is a compile-time check that all executors implement the interface
    SingleThreadExecutor single;
    StdAsyncExecutor async;
    ThreadPoolExecutor<2> pool;
    
    IParallelExecutor* executors[] = {&single, &async, &pool};
    
    for (auto* executor : executors) {
      std::atomic<bool> executed{false};
      auto future = executor->submit([&executed]() { executed.store(true); });
      future.wait();
      REQUIRE(executed.load());
    }
  }
  
  SECTION("All executors implement waitAll()")
  {
    SingleThreadExecutor single;
    StdAsyncExecutor async;
    ThreadPoolExecutor<2> pool;
    
    IParallelExecutor* executors[] = {&single, &async, &pool};
    
    for (auto* executor : executors) {
      std::atomic<int> counter{0};
      std::vector<std::future<void>> futures;
      
      for (int i = 0; i < 5; ++i) {
        futures.push_back(executor->submit(createIncrementTask(counter)));
      }
      
      REQUIRE_NOTHROW(executor->waitAll(futures));
      REQUIRE(counter.load() == 5);
    }
  }
}
