#pragma once

#include "IParallelExecutor.h"
#include <queue>
#include <future>
#include <thread>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <exception>
#include "runner.hpp"  // for BoostRunnerExecutor

/**
 * @file ParallelExecutors.h
 * @brief Provides a set of executor policies for parallel task execution.
 *
 * This file defines several implementations of the IParallelExecutor interface:
 *  - SingleThreadExecutor: runs tasks inline on the calling thread (deterministic, no concurrency).
 *  - StdAsyncExecutor: uses std::async(std::launch::async) to spawn tasks (portable but may oversubscribe).
 *  - BoostRunnerExecutor: delegates tasks to a Boost-based thread pool (requires Boost runner).
 *  - ThreadPoolExecutor<N>: a fixed-size thread pool with N worker threads (lowest overhead for many small tasks).
 *
 * @section usage Guidance on choosing an executor policy
 * - SingleThreadExecutor: Use in unit tests or when debugging, or when concurrency must be disabled.
 * - StdAsyncExecutor: Easy and dependency-free; good for a small number of long-running tasks.
 * - BoostRunnerExecutor: Integrates with an existing Boost-based runner thread-pool; good if already using Boost runner.
 * - ThreadPoolExecutor<N>: Best for high-throughput scenarios with many small tasks; amortizes thread creation cost.
 *
 * @section tradeoffs
 * - Thread creation overhead: std::async and BoostRunnerExecutor may create/destroy threads per task, which can dominate
 *   execution time when tasks are short or numerous.
 * - Resource contention: unbounded task submission to std::async can oversubscribe CPU and lead to contention.
 * - Determinism: SingleThreadExecutor yields deterministic, reproducible execution, useful for tests.
 * - Integration: BoostRunnerExecutor fits existing Boost-based task systems, avoiding new thread pools.
 * - Control: ThreadPoolExecutor gives fine-grained control over number of threads and queue behavior.
 */
namespace concurrency
{
  /**
   * @brief Executes tasks synchronously on the calling thread.
   *
   * All tasks run inline, with no actual concurrency. Useful for deterministic unit tests
   * or single-threaded fallbacks where concurrency should be disabled.
   */
  class SingleThreadExecutor : public IParallelExecutor {
  public:
    std::future<void> submit(std::function<void()> task) override {
      std::promise<void> prom;
      auto fut = prom.get_future();
      try {
	task();
	prom.set_value();
      } catch (...) {
	prom.set_exception(std::current_exception());
      }
      return fut;
    }
  };

  /**
   * @brief Executor policy using std::async for each task.
   *
   * StdAsyncExecutor implements IParallelExecutor by calling
   * std::async(std::launch::async, task) for every submitted task.
   *
   * Characteristics:
   *   - Portability: part of the C++ standard library, no extra dependencies.
   *   - Unbounded: each submit may spawn a new thread (or hand off to
   *     an implementation-defined pool), with no hard limit on concurrent tasks.
   *   - High per-task overhead: thread creation, context switching, and teardown
   *     can dominate for short-lived or numerous tasks.
   *
   * Differences from other policies:
   *   - SingleThreadExecutor runs tasks inline (no concurrency, very low overhead).
   *   - BoostRunnerExecutor posts to an existing Boost thread pool, reusing threads,
   *     but requires Boost runner integration.
   *   - ThreadPoolExecutor<N> uses a fixed-size pool of N workers, amortizing
   *     thread startup cost and capping concurrency for many small tasks.
   *
   * When to use StdAsyncExecutor:
   *   - Prototyping or quick parallelism for a small number (e.g. <50) of
   *     long-running tasks, where thread startup cost is negligible.
   *   - Environments where only the C++ standard library is available.
   *
   * Trade-offs:
   *   - Ease of use vs. performance: simplest to write, but may oversubscribe
   *     CPU and incur high overhead if tasks are numerous or fine-grained.
   */
  class StdAsyncExecutor : public IParallelExecutor {
  public:
    std::future<void> submit(std::function<void()> task) override {
      return std::async(std::launch::async, std::move(task));
    }
  };

  /**
   * @brief Submits tasks to the existing Boost-based runner thread pool.
   * Wraps boost::unique_future<void> into std::future<void>.
   */
  class BoostRunnerExecutor : public IParallelExecutor {
  public:
    std::future<void> submit(std::function<void()> task) override
    {
      runner::ensure_initialized(getNCpus());

      auto prom = std::make_shared<std::promise<void>>();
      auto fut  = prom->get_future();

      runner::instance().post([task = std::move(task), prom]() {
	try {
	  task();
	  prom->set_value();
	}
	catch (...) {
	  prom->set_exception(std::current_exception());
	}
      });

      return fut;
    }
  };
  
  /**
   * @brief Fixed-size thread pool executor.
   * Tasks submitted are queued and executed by a pool of worker threads.
   * Template parameter N specifies the number of threads in the pool.
   *
   * If N == 0, at runtime we pick std::thread::hardware_concurrency()
   * (falling back to 2 if that returns 0).
   */
  template <std::size_t N = 0>
  class ThreadPoolExecutor : public IParallelExecutor {
  public:
    // Explicitly delete copy and move
    ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor(ThreadPoolExecutor&&) = delete;
    ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) = delete;
    
    ThreadPoolExecutor() : stop_(false)
    {
      const std::size_t threads =
	N > 0 ? N : (std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 2);

      try {
	for (std::size_t i = 0; i < threads; ++i) {
	  workers_.emplace_back([this] { workerLoop(); });
	}
      }
      catch (...) {
	{
	  std::lock_guard<std::mutex> lock(tasksMutex_);
	  stop_ = true;
	}
	condition_.notify_all();
	for (auto& w : workers_) if (w.joinable()) w.join();
	throw;
      }
    }

    void workerLoop()
    {
      for (;;) {
	std::function<void()> task;
	{
	  std::unique_lock<std::mutex> lock(tasksMutex_);
	  condition_.wait(lock, [this]{ return stop_ || !tasks_.empty(); });
	  if (stop_ && tasks_.empty()) return;
	  task = std::move(tasks_.front());
	  tasks_.pop();
	}
	task();
      }
    }

    ~ThreadPoolExecutor()
    {
      {
	std::unique_lock<std::mutex> lock(tasksMutex_);
	stop_ = true;
      }
      condition_.notify_all();
      for (auto &worker : workers_) {
	if (worker.joinable())
	  worker.join();
      }
    }

    // override the pure virtual submit() from IParallelExecutor
    std::future<void> submit(std::function<void()> task) override
    {
      auto packaged = std::make_shared<std::packaged_task<void()>>(std::move(task));
      auto fut = packaged->get_future();
      {
	std::unique_lock<std::mutex> lock(tasksMutex_);
	if (stop_)
	  throw std::runtime_error("enqueue on stopped ThreadPoolExecutor");
	tasks_.emplace([packaged]() { (*packaged)(); });
      }
      condition_.notify_one();
      return fut;
    }

  private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        tasksMutex_;
    std::condition_variable           condition_;
    bool                              stop_;
  };
} // namespace concurrency
