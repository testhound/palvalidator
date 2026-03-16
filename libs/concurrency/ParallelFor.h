#pragma once

#include <cstdint>      // for uint32_t
#include <thread>       // for std::thread::hardware_concurrency()
#include <vector>       // for std::vector
#include <future>       // for std::future
#include <algorithm>    // for std::min, std::max

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
   * @tparam Body     The type of the callable function/lambda to execute for each index.
   *
   * @param total The total number of iterations, defining the loop range [0, total).
   * @param exec  A reference to the parallel executor instance.
   * @param body  A callable that accepts a uint32_t index, e.g., `[](uint32_t i) { ... }`.
   *
   * @note This function uses static partitioning, which can lead to poor load balancing if
   * iteration costs are highly variable. For such cases, consider `parallel_for_chunked`.
   * @see parallel_for_chunked
   *
   * ### Algorithm
   * 1.  Determine the number of concurrent tasks, defaulting to the hardware concurrency or 2.
   * 2.  Calculate a chunk size that divides the `total` iterations among the tasks. This creates a
   *     small number of large, coarse-grained chunks.
   * 3.  Iterate through the index range, creating one task for each large chunk.
   * 4.  Submit a lambda for each chunk to the executor. The lambda internally loops from the
   *     chunk's `start` to `end` index, calling the user-provided `body` for each index.
   * 5.  Collect a `std::future` for each submitted task.
   * 6.  Block until all futures have completed using `exec.waitAll()`.
   */
  template<typename Executor, typename Body>
  void parallel_for(uint32_t total, Executor& exec, Body body)
  {
    if (total == 0) return;

    const unsigned hw       = std::thread::hardware_concurrency();
    const unsigned numTasks = hw ? hw : 2;
    const uint32_t chunkSize = (total + numTasks - 1) / numTasks; // ceil-divide

    std::vector<std::future<void>> futures;
    for (uint32_t start = 0; start < total; start += chunkSize)
      {
        uint32_t end = std::min(total, start + chunkSize);
        futures.emplace_back(
          exec.submit([=]() {
            for (uint32_t p = start; p < end; ++p)
              body(p);
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
   * @tparam Executor   The type of the parallel executor.
   * @tparam Container  The type of the container, which must support `size()` and `operator[]`.
   * @tparam Body       The type of the callable to execute for each element.
   *
   * @param exec      A reference to the parallel executor.
   * @param container The container to iterate over.
   * @param body      A callable that accepts an element from the container,
   *                  e.g., `[](const auto& item) { ... }`.
   *
   * ### Algorithm
   * 1.  Get the total size of the container.
   * 2.  Calculate a chunk size that divides the total number of elements among a number of tasks
   *     equal to the hardware concurrency.
   * 3.  Iterate through the indices [0, total), creating one task for each chunk of indices.
   * 4.  Submit a lambda for each chunk. The lambda captures the container by reference and
   *     loops from its `start` to `end` index, calling `body(container[p])` for each element.
   * 5.  Collect futures for all submitted tasks and wait for them to complete.
   */
  template<typename Executor, typename Container, typename Body>
  void parallel_for_each(Executor& exec, const Container& container, Body body)
  {
    if (container.empty()) return;

    const uint32_t total    = static_cast<uint32_t>(container.size());
    const unsigned hw       = std::thread::hardware_concurrency();
    const unsigned numTasks = hw ? hw : 2;
    const uint32_t chunkSize = (total + numTasks - 1) / numTasks;

    std::vector<std::future<void>> futures;
    for (uint32_t start = 0; start < total; start += chunkSize)
      {
        uint32_t end = std::min(total, start + chunkSize);
        // Capture body by value: robust against the body going out of scope
        // before the task executes. Container is captured by reference because
        // its lifetime is guaranteed to exceed the waitAll() call below.
        futures.emplace_back(
          exec.submit([=, &container]() {
            for (uint32_t p = start; p < end; ++p)
              body(container[p]);
          })
        );
      }
    exec.waitAll(futures);
  }

  // ---------------------------------------------------------------------------
  // Chunk-size helper (shared between both parallel_for_chunked overloads)
  // ---------------------------------------------------------------------------

  /**
   * @brief Computes the effective chunk size for parallel_for_chunked.
   *
   * If @p hint is non-zero it is returned unchanged, giving the caller full
   * control. Otherwise an auto-calculated size is returned that targets
   * approximately @p chunksPerThread chunks per hardware thread, clamped to
   * [512, 8192] to bound scheduling overhead on both extremes.
   *
   * Extracted as a free function so that both overloads of parallel_for_chunked
   * stay in sync without duplicating the formula.
   *
   * @param total           Total number of iterations.
   * @param hint            Caller-supplied size override (0 = auto).
   * @param chunksPerThread Target number of chunks per hardware thread (default 6).
   * @return                Effective chunk size in [1, total].
   */
  inline uint32_t computeChunkSize(uint32_t total,
                                   uint32_t hint,
                                   unsigned chunksPerThread = 6u) noexcept
  {
    if (hint != 0u)
      return hint;

    const unsigned hwThreads = std::max(1u, std::thread::hardware_concurrency());
    const uint32_t divisor   = static_cast<uint32_t>(hwThreads) * chunksPerThread;
    const uint32_t raw       = (total + divisor - 1u) / divisor; // ceil(total / divisor)
    return std::min<uint32_t>(std::max<uint32_t>(raw, 512u), 8192u);
  }

  // ---------------------------------------------------------------------------
  // parallel_for_chunked — unified implementation
  //
  // Why a single function rather than two overloads
  // ────────────────────────────────────────────────
  // Overload resolution in C++ operates on the *declared* parameter types, not
  // on whether the body can actually be *called* inside the function body. Both
  // calling conventions (body(p) and body(begin,end)) produce the same four
  // declared parameter types — (uint32_t, Executor&, Body, uint32_t) — so the
  // compiler sees two equally viable candidates and rejects the call as
  // ambiguous, even when one of them would fail to compile if instantiated.
  //
  // The correct solution is a single template that uses std::is_invocable to
  // test the body's arity at compile time and then branches with if constexpr.
  // Because if constexpr discards the untaken branch before full instantiation,
  // neither branch needs to be valid for the other body signature.
  // ---------------------------------------------------------------------------

  /**
   * @brief Executes a for-loop in parallel using fine-grained chunks.
   *
   * A single template that supports two calling conventions, selected at
   * compile time by inspecting the arity of @p body:
   *
   * **Per-element callback** — `body(uint32_t p)`
   *   The infrastructure owns the inner loop. For each chunk a task is
   *   submitted that iterates `[start, end)` and calls `body(p)` for every
   *   index. Use for stateless or cheaply-replicated work.
   *
   * @code
   * parallel_for_chunked(N, exec, [](uint32_t i) { process(i); });
   * @endcode
   *
   * **Range callback** — `body(uint32_t begin, uint32_t end)`
   *   The caller owns the inner loop. For each chunk a task is submitted that
   *   calls `body(start, end)` exactly once. Use when the body needs to
   *   accumulate chunk-local state across its iterations and flush it once at
   *   the end — the primary motivation being atomic-contention reduction:
   *
   * @code
   * std::atomic<unsigned> count{0};
   * parallel_for_chunked(N, exec,
   *   [&](uint32_t begin, uint32_t end) {
   *     unsigned local = 0;
   *     for (uint32_t i = begin; i < end; ++i)
   *       if (predicate(i)) ++local;
   *     count.fetch_add(local, std::memory_order_relaxed); // one atomic op per chunk
   *   });
   * @endcode
   *
   * ### Dispatch mechanism
   * `std::is_invocable_v<Body, uint32_t, uint32_t>` is evaluated at compile
   * time. When true the range branch is taken; otherwise the per-element branch
   * is taken. `if constexpr` ensures the untaken branch is not instantiated, so
   * neither branch is required to compile for the other body signature.
   *
   * @tparam Executor The type of the parallel executor.
   * @tparam Body     Callable with signature `void(uint32_t)` or
   *                  `void(uint32_t, uint32_t)`.
   *
   * @param total         Total number of iterations, range [0, total).
   * @param exec          Reference to the parallel executor.
   * @param body          Per-element or range callable (see above).
   * @param chunkSizeHint If non-zero, overrides the auto-calculated chunk size.
   *
   * ### Algorithm
   * 1.  Compute an effective chunk size via `computeChunkSize()`.
   * 2.  Partition [0, total) into chunks of that size.
   * 3.  Per-element path: submit a task per chunk that calls body(p) for each p.
   *     Range path:        submit a task per chunk that calls body(start, end) once.
   * 4.  Reserve the futures vector upfront to avoid reallocations.
   * 5.  Block until all tasks complete via `exec.waitAll()`.
   */
  template<typename Executor, typename Body>
  void parallel_for_chunked(uint32_t total, Executor& exec, Body body,
                             uint32_t chunkSizeHint = 0u)
  {
    if (total == 0u) return;

    const uint32_t chunkSize = computeChunkSize(total, chunkSizeHint);

    std::vector<std::future<void>> futures;
    futures.reserve((total + chunkSize - 1u) / chunkSize);

    for (uint32_t start = 0u; start < total; start += chunkSize)
      {
        uint32_t end = std::min(total, start + chunkSize);

        if constexpr (std::is_invocable_v<Body, uint32_t, uint32_t>)
          {
            // Range callback: body owns the inner loop.
            // Called once per chunk with (begin, end).
            futures.emplace_back(
              exec.submit([=]() {
                body(start, end);
              })
            );
          }
        else
          {
            // Per-element callback: infrastructure owns the inner loop.
            // Called once per index p in [start, end).
            // NOTE: thread-local state (rng, caches) initialised inside body
            // will be reused for every element in the same chunk.
            futures.emplace_back(
              exec.submit([=]() {
                for (uint32_t p = start; p < end; ++p)
                  body(p);
              })
            );
          }
      }
    exec.waitAll(futures);
  }

} // namespace concurrency
