#pragma once

#include <cstdint>    // for uint32_t
#include <thread>     // for std::thread::hardware_concurrency()
#include <vector>     // for std::vector
#include <future>     // for std::future
#include <algorithm>  // for std::min

namespace concurrency
{
  /**
   * @brief Executes a for-loop over an index range in parallel using large, static chunks.
   *
   * This function partitions the index range [0, total) into a small number of large chunks,
   * typically one for each hardware thread. Each chunk is submitted as a single task to the
   * provided executor. This approach is best suited for workloads where each loop iteration
   * takes a roughly uniform amount of time.
   *
   * @tparam Executor The type of the parallel executor, which must have `submit()` and `waitAll()` methods.
   * @tparam Body The type of the callable function/lambda to execute for each index.
   *
   * @param total The total number of iterations, defining the loop range [0, total).
   * @param exec A reference to the parallel executor instance.
   * @param body A callable that accepts a uint32_t index, e.g., `[](uint32_t i) { ... }`.
   *
   * @note This function uses static partitioning, which can lead to poor load balancing if
   * iteration costs are highly variable. For such cases, consider `parallel_for_chunked`.
   * @see parallel_for_chunked
   *
   * ### Algorithm
   * 1.  Determine the number of concurrent tasks, defaulting to the hardware concurrency or 2.
   * 2.  Calculate a chunk size that divides the `total` iterations among the tasks. This creates a
   * small number of large, coarse-grained chunks.
   * 3.  Iterate through the index range, creating one task for each large chunk.
   * 4.  Submit a lambda for each chunk to the executor. The lambda internally loops from the
   * chunk's `start` to `end` index, calling the user-provided `body` for each index.
   * 5.  Collect a `std::future` for each submitted task.
   * 6.  Block until all futures have completed using `exec.waitAll()`.
   */
  template<typename Executor, typename Body>
    void parallel_for(uint32_t total, Executor& exec, Body body) {
    if (total == 0) return;

    const unsigned hw = std::thread::hardware_concurrency();
    const unsigned numTasks = hw ? hw : 2;
    const uint32_t chunkSize = (total + numTasks - 1) / numTasks; // ceil-divide

    std::vector<std::future<void>> futures;
    for (uint32_t start = 0; start < total; start += chunkSize)
      {
	uint32_t end = std::min(total, start + chunkSize);
	futures.emplace_back(
			     exec.submit([=]() {
				 for (uint32_t p = start; p < end; ++p) {
				   body(p);
				 }
			       })
			     );
      }
    exec.waitAll(futures);
  }

  /**
   * @brief Executes a for-each loop over a container in parallel.
   *
   * This function processes each element of a random-access container in parallel. It partitions
   * the container by index into large, static chunks (one per hardware thread) and submits
   * each chunk as a single task to the executor.
   *
   * @tparam Executor The type of the parallel executor.
   * @tparam Container The type of the container, which must support `size()` and `operator[]`.
   * @tparam Body The type of the callable to execute for each element.
   *
   * @param exec A reference to the parallel executor.
   * @param container The container to iterate over.
   * @param body A callable that accepts an element from the container, e.g., `[](const auto& item) { ... }`.
   *
   * ### Algorithm
   * 1.  Get the total size of the container.
   * 2.  Calculate a chunk size that divides the total number of elements among a number of tasks
   * equal to the hardware concurrency.
   * 3.  Iterate through the indices [0, total), creating one task for each chunk of indices.
   * 4.  Submit a lambda for each chunk. The lambda captures the container by reference and
   * loops from its `start` to `end` index, calling `body(container[p])` for each element.
   * 5.  Collect futures for all submitted tasks and wait for them to complete.
   */
  template<typename Executor, typename Container, typename Body>
  void parallel_for_each(Executor& exec, const Container& container, Body body) {
    if (container.empty()) return;

    const uint32_t total = container.size();
    const unsigned numTasks = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 2;
    const uint32_t chunkSize = (total + numTasks - 1) / numTasks;

    std::vector<std::future<void>> futures;
    for (uint32_t start = 0; start < total; start += chunkSize) {
      uint32_t end = std::min(total, start + chunkSize);
      // FIXED: Capture body by value for robustness
      // Previously: [&, start, end]() which captured body by reference (fragile)
      // Now: [=, &container]() which captures body by value (robust)
      futures.emplace_back(
        exec.submit([=, &container]() {
          for (uint32_t p = start; p < end; ++p) {
            body(container[p]);
          }
        })
      );
    }
    exec.waitAll(futures);
  }
  /**
   * @brief Executes a for-loop in parallel using many small, dynamically-scheduled chunks for better load balancing.
   *
   * This function partitions the index range [0, total) into many small, fine-grained chunks.
   * By creating significantly more chunks than hardware threads, a thread-pool based executor
   * can dynamically schedule them, ensuring that all threads remain busy even if the work per
   * iteration is highly variable. This is the preferred parallel-for implementation for
   * non-uniform workloads.
   *
   * @tparam Executor The type of the parallel executor.
   * @tparam Body The type of the callable to execute for each index.
   *
   * @param total The total number of iterations, defining the loop range [0, total).
   * @param exec A reference to the parallel executor instance.
   * @param body A callable that accepts a uint32_t index.
   * @param chunkSizeHint If non-zero, forces a specific chunk size. Otherwise, an optimal size is auto-calculated.
   *
   * @note The lambda submitted to the executor is an ideal place to initialize thread-local state
   * (e.g., random number generators, caches) that can be reused across all iterations
   * processed by that task.
   * @see parallel_for
   *
   * ### Algorithm
   * 1.  Determine an optimal chunk size. The goal is to create several chunks per hardware thread
   * (e.g., 4-8) to facilitate dynamic load balancing by the executor.
   * 2.  The auto-calculated chunk size is clamped to a reasonable range (e.g., [512, 8192]) to
   * balance between scheduling overhead and responsiveness. A user-provided `chunkSizeHint`
   * can override this logic.
   * 3.  Iterate through the index range [0, total), creating one task for each small chunk.
   * 4.  Submit a lambda for each chunk to the executor. The lambda internally loops through its
   * assigned sub-range and calls the user-provided `body`.
   * 5.  Reserve space in the futures vector to avoid reallocations.
   * 6.  Block until all submitted tasks have completed.
   */
  template<typename Executor, typename Body>
  void parallel_for_chunked(uint32_t total, Executor& exec, Body body, uint32_t chunkSizeHint = 0)
  {
    if (total == 0)
      return;

    const unsigned hw = std::thread::hardware_concurrency();
    //    const unsigned numTasks = hw ? hw : 2;

    const unsigned hwThreads = std::max(1u, hw);
    auto div = 6u; // target ~6 chunks per thread (tune 4–8)
    uint32_t chunk = (total + (hwThreads*div - 1)) / (hwThreads*div);    // ceil(N / (hwThreads*div))
    const uint32_t autoChunk = std::min<uint32_t>(std::max<uint32_t>(chunk, 512), 8192);
    const uint32_t chunkSize = chunkSizeHint ? chunkSizeHint : autoChunk;

    std::vector<std::future<void>> futures;
    futures.reserve((total + chunkSize - 1) / chunkSize);

    for (uint32_t start = 0; start < total; start += chunkSize)
      {
	uint32_t end = std::min(total, start + chunkSize);
	futures.emplace_back(
			   exec.submit([=]() {
			     // NOTE: do per-task TLS init here (rng/cache/portfolio) once
			     for (uint32_t p = start; p < end; ++p) {
			       body(p);
			     }
			   })
			     );
      }
    exec.waitAll(futures);
  }
}
