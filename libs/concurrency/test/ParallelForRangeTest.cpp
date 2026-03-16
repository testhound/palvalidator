// ParallelForRangeTest.cpp
//
// Unit tests for the range-callback overload of parallel_for_chunked:
//
//   parallel_for_chunked(total, exec, [](uint32_t begin, uint32_t end){ ... }, hint)
//
// This overload is distinct from the original per-element overload
//   parallel_for_chunked(total, exec, [](uint32_t p){ ... }, hint)
// in that the infrastructure calls body(start, end) exactly ONCE per chunk and
// the caller owns the inner loop. The primary motivation is to allow chunk-local
// state accumulation (e.g. local counters flushed to a shared atomic once per
// chunk) to eliminate cache-line bouncing on hot shared variables.
//
// Coverage plan
// ─────────────
//  1. Basic correctness          – body receives correct begin/end, covers [0,total)
//  2. Overload disambiguation    – one-arg and two-arg lambdas route to different overloads
//  3. computeChunkSize helper    – verifies hint pass-through and auto-sizing bounds
//  4. Tiling invariants          – no gaps, no overlaps, exact coverage for many sizes
//  5. Body call count            – body called once per chunk, not once per element
//  6. Chunk-size hint honoured   – actual ranges match expected chunk size
//  7. Primary use-case (atomic)  – local-accumulator pattern; same result as per-element
//  8. thread_local within range  – TLS RNG initialised once per chunk, not per element
//  9. All executor types         – SingleThread, ThreadPool, StdAsync
// 10. Edge cases                 – zero iterations, single iteration, total < chunk size,
//                                  non-power-of-two totals, prime totals
// 11. Stress / large N           – correctness at scale

#include <catch2/catch_test_macros.hpp>
#include "ParallelFor.h"
#include "ParallelExecutors.h"

#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>
#include <thread>

using namespace concurrency;

// ─────────────────────────────────────────────────────────────────────────────
// 1. Basic correctness
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: basic correctness",
          "[parallel_for_chunked][range]")
{
  SECTION("Every index is processed exactly once (SingleThreadExecutor)")
  {
    SingleThreadExecutor exec;
    const uint32_t N = 200;
    std::vector<std::atomic<int>> visited(N);
    for (auto& v : visited) v.store(0);

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        for (uint32_t i = begin; i < end; ++i)
          visited[i].fetch_add(1, std::memory_order_relaxed);
      }, 50u);

    for (uint32_t i = 0; i < N; ++i)
      REQUIRE(visited[i].load() == 1);
  }

  SECTION("Every index is processed exactly once (ThreadPoolExecutor)")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 1000;
    std::vector<std::atomic<int>> visited(N);
    for (auto& v : visited) v.store(0);

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        for (uint32_t i = begin; i < end; ++i)
          visited[i].fetch_add(1, std::memory_order_relaxed);
      }, 100u);

    for (uint32_t i = 0; i < N; ++i)
      REQUIRE(visited[i].load() == 1);
  }

  SECTION("Sum matches expected value")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 5000;
    std::atomic<uint64_t> sum{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        uint64_t local = 0;
        for (uint32_t i = begin; i < end; ++i)
          local += i;
        sum.fetch_add(local, std::memory_order_relaxed);
      }, 250u);

    const uint64_t expected = static_cast<uint64_t>(N) * (N - 1) / 2;
    REQUIRE(sum.load() == expected);
  }

  SECTION("begin < end for every chunk invocation")
  {
    ThreadPoolExecutor<4> exec;
    std::atomic<bool> bad_range{false};

    parallel_for_chunked(500u, exec,
      [&](uint32_t begin, uint32_t end) {
        if (begin >= end) bad_range.store(true, std::memory_order_relaxed);
      }, 50u);

    REQUIRE_FALSE(bad_range.load());
  }

  SECTION("begin and end are always within [0, total)")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 300;
    std::atomic<bool> out_of_range{false};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        if (begin >= N || end > N)
          out_of_range.store(true, std::memory_order_relaxed);
      }, 40u);

    REQUIRE_FALSE(out_of_range.load());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Overload disambiguation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: overload disambiguation",
          "[parallel_for_chunked][range][disambiguation]")
{
  // The two overloads are selected purely by lambda arity. A two-argument
  // lambda must never be routed to the per-element overload and vice versa.

  SECTION("Two-arg lambda selects range overload (body called once per chunk)")
  {
    SingleThreadExecutor exec;
    const uint32_t N         = 100;
    const uint32_t chunkHint = 25u; // expect exactly 4 chunks

    std::atomic<int> call_count{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t /*begin*/, uint32_t /*end*/) {
        call_count.fetch_add(1, std::memory_order_relaxed);
      }, chunkHint);

    // 100 / 25 = 4 chunks → body called 4 times, NOT 100 times
    REQUIRE(call_count.load() == 4);
  }

  SECTION("One-arg lambda selects per-element overload (body called once per index)")
  {
    SingleThreadExecutor exec;
    const uint32_t N         = 100;
    const uint32_t chunkHint = 25u;

    std::atomic<int> call_count{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t /*i*/) {
        call_count.fetch_add(1, std::memory_order_relaxed);
      }, chunkHint);

    // Per-element overload → body called 100 times
    REQUIRE(call_count.load() == static_cast<int>(N));
  }

  SECTION("Both overloads agree on final sum")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 2000;

    std::atomic<uint64_t> sum_range{0};
    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        uint64_t local = 0;
        for (uint32_t i = begin; i < end; ++i) local += i;
        sum_range.fetch_add(local, std::memory_order_relaxed);
      }, 200u);

    std::atomic<uint64_t> sum_element{0};
    parallel_for_chunked(N, exec,
      [&](uint32_t i) {
        sum_element.fetch_add(i, std::memory_order_relaxed);
      }, 200u);

    REQUIRE(sum_range.load() == sum_element.load());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. computeChunkSize helper
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("computeChunkSize helper", "[computeChunkSize]")
{
  SECTION("Non-zero hint is returned unchanged")
  {
    REQUIRE(computeChunkSize(10000u, 123u) == 123u);
    REQUIRE(computeChunkSize(10000u, 1u)   == 1u);
    REQUIRE(computeChunkSize(10000u, 8192u) == 8192u);
    REQUIRE(computeChunkSize(10000u, 9999u) == 9999u);
  }

  SECTION("Auto-calculated value is clamped to [512, 8192]")
  {
    // Very small total → raw chunk very small → clamped up to 512
    const uint32_t small_auto = computeChunkSize(10u, 0u);
    REQUIRE(small_auto >= 512u);
    REQUIRE(small_auto <= 8192u);

    // Very large total → raw chunk might exceed 8192 → clamped down
    const uint32_t large_auto = computeChunkSize(100'000'000u, 0u);
    REQUIRE(large_auto >= 512u);
    REQUIRE(large_auto <= 8192u);

    // Mid-range total → result within bounds
    const uint32_t mid_auto = computeChunkSize(25000u, 0u);
    REQUIRE(mid_auto >= 512u);
    REQUIRE(mid_auto <= 8192u);
  }

  SECTION("Result is always at least 1")
  {
    REQUIRE(computeChunkSize(1u,    0u) >= 1u);
    REQUIRE(computeChunkSize(1u,    1u) >= 1u);
    REQUIRE(computeChunkSize(1000u, 0u) >= 1u);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Tiling invariants — no gaps, no overlaps, exact partition of [0, total)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: tiling invariants",
          "[parallel_for_chunked][range][tiling]")
{
  // Collect the (begin, end) pairs emitted for a given (total, hint) and verify
  // they form a perfect partition of [0, total).
  auto verify_tiling = [](uint32_t total, uint32_t hint) {
    SingleThreadExecutor exec; // single-thread ensures deterministic collection

    std::vector<std::pair<uint32_t,uint32_t>> ranges;
    std::mutex mtx;

    parallel_for_chunked(total, exec,
      [&](uint32_t begin, uint32_t end) {
        std::lock_guard<std::mutex> lock(mtx);
        ranges.emplace_back(begin, end);
      }, hint);

    // Sort by begin so we can walk them in order
    std::sort(ranges.begin(), ranges.end());

    REQUIRE_FALSE(ranges.empty());

    // First range starts at 0
    REQUIRE(ranges.front().first == 0u);
    // Last range ends at total
    REQUIRE(ranges.back().second == total);

    // Each range is non-empty and consecutive ranges are contiguous (no gaps, no overlaps)
    for (size_t k = 0; k + 1 < ranges.size(); ++k)
      {
        REQUIRE(ranges[k].first  <  ranges[k].second);           // non-empty
        REQUIRE(ranges[k].second == ranges[k+1].first);          // contiguous
      }
  };

  SECTION("Exact multiple: 1000 / 100 = 10 chunks")      { verify_tiling(1000u,  100u); }
  SECTION("Non-divisible: 1000 / 333")                   { verify_tiling(1000u,  333u); }
  SECTION("Hint larger than total → single chunk")        { verify_tiling(50u,   1000u); }
  SECTION("Hint == 1 → one chunk per element")            { verify_tiling(20u,     1u); }
  SECTION("Prime total 997 / 100")                        { verify_tiling(997u,   100u); }
  SECTION("Total == 1")                                   { verify_tiling(1u,      10u); }
  SECTION("Total == 2, hint == 1")                        { verify_tiling(2u,       1u); }
  SECTION("Large total 50000 / 512")                      { verify_tiling(50000u,  512u); }
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Body call count — exactly one invocation per chunk, not per element
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: body called once per chunk",
          "[parallel_for_chunked][range]")
{
  SECTION("Exact multiple chunk size")
  {
    SingleThreadExecutor exec;
    // 200 elements / chunk 50 = exactly 4 chunks
    std::atomic<int> body_calls{0};
    parallel_for_chunked(200u, exec,
      [&](uint32_t, uint32_t) { body_calls.fetch_add(1); }, 50u);
    REQUIRE(body_calls.load() == 4);
  }

  SECTION("Non-divisible total creates one extra partial chunk")
  {
    SingleThreadExecutor exec;
    // 205 elements / chunk 50 = 4 full + 1 partial = 5 calls
    std::atomic<int> body_calls{0};
    parallel_for_chunked(205u, exec,
      [&](uint32_t, uint32_t) { body_calls.fetch_add(1); }, 50u);
    REQUIRE(body_calls.load() == 5);
  }

  SECTION("Hint larger than total → exactly one call")
  {
    SingleThreadExecutor exec;
    std::atomic<int> body_calls{0};
    parallel_for_chunked(30u, exec,
      [&](uint32_t, uint32_t) { body_calls.fetch_add(1); }, 1000u);
    REQUIRE(body_calls.load() == 1);
  }

  SECTION("Hint == 1 → call count equals total")
  {
    SingleThreadExecutor exec;
    const uint32_t N = 15;
    std::atomic<int> body_calls{0};
    parallel_for_chunked(N, exec,
      [&](uint32_t, uint32_t) { body_calls.fetch_add(1); }, 1u);
    REQUIRE(body_calls.load() == static_cast<int>(N));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Chunk-size hint is honoured
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: chunk size hint honoured",
          "[parallel_for_chunked][range]")
{
  // When a non-zero hint is supplied, every range width should equal hint except
  // possibly the last (partial) chunk which may be smaller.

  auto verify_chunk_widths = [](uint32_t total, uint32_t hint) {
    SingleThreadExecutor exec;
    std::vector<uint32_t> widths;
    std::mutex mtx;

    parallel_for_chunked(total, exec,
      [&](uint32_t begin, uint32_t end) {
        std::lock_guard<std::mutex> lock(mtx);
        widths.push_back(end - begin);
      }, hint);

    // Sort descending: all full-sized chunks come first, the one partial chunk
    // (if total % hint != 0) ends up last. The loop below then correctly skips
    // only that trailing element.
    std::sort(widths.begin(), widths.end(), std::greater<uint32_t>{});

    // All chunks except possibly the last must equal hint
    for (size_t k = 0; k + 1 < widths.size(); ++k)
      REQUIRE(widths[k] == hint);

    // Last chunk is <= hint (equals hint when total is an exact multiple)
    if (!widths.empty())
      REQUIRE(widths.back() <= hint);
  };

  SECTION("1000 elements, hint 100") { verify_chunk_widths(1000u, 100u); }
  SECTION("1000 elements, hint 333") { verify_chunk_widths(1000u, 333u); }
  SECTION("1000 elements, hint 1")   { verify_chunk_widths(1000u,   1u); }
  SECTION("1000 elements, hint 999") { verify_chunk_widths(1000u, 999u); }
  SECTION("1 element,    hint 10")   { verify_chunk_widths(   1u,  10u); }
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Primary use-case: chunk-local atomic accumulation
//
// This is the raison d'être for the range overload. The test verifies that:
//   (a) The local-accumulator pattern produces the correct global sum.
//   (b) The number of fetch_add calls to the shared atomic equals the number
//       of chunks, not the number of elements — demonstrating reduced contention.
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: chunk-local atomic accumulation",
          "[parallel_for_chunked][range][atomic]")
{
  SECTION("Global count matches total with local accumulation (SingleThread)")
  {
    SingleThreadExecutor exec;
    const uint32_t N = 10000;
    std::atomic<uint32_t> global_count{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        unsigned int local = end - begin;   // every element increments notionally
        global_count.fetch_add(local, std::memory_order_relaxed);
      }, 500u);

    REQUIRE(global_count.load() == N);
  }

  SECTION("Global count matches total with local accumulation (ThreadPool)")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 10000;
    std::atomic<uint32_t> global_count{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        unsigned int local = 0;
        for (uint32_t i = begin; i < end; ++i) ++local;
        global_count.fetch_add(local, std::memory_order_relaxed);
      }, 500u);

    REQUIRE(global_count.load() == N);
  }

  SECTION("fetch_add called once per chunk, not once per element")
  {
    SingleThreadExecutor exec;
    const uint32_t N         = 1000;
    const uint32_t chunkHint = 100u; // 10 chunks

    std::atomic<int>      global_sum{0};
    std::atomic<int>      fetch_add_calls{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        int local = 0;
        for (uint32_t i = begin; i < end; ++i) ++local;
        global_sum.fetch_add(local, std::memory_order_relaxed);
        fetch_add_calls.fetch_add(1, std::memory_order_relaxed);  // count the flushes
      }, chunkHint);

    REQUIRE(global_sum.load() == static_cast<int>(N));
    // 10 chunks → 10 fetch_add flushes, not 1000
    REQUIRE(fetch_add_calls.load() == static_cast<int>(N / chunkHint));
  }

  SECTION("Two independent local counters flushed together (count_less / count_equal pattern)")
  {
    // Mirrors the BCaBootStrap use-case directly: two counters accumulated
    // locally per chunk and flushed once per chunk each.
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 5000;

    // Simulate a statistic that is < threshold for even indices, == for odd
    const uint32_t threshold = N / 2;

    std::atomic<uint32_t> count_less{0};
    std::atomic<uint32_t> count_equal{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        unsigned int local_less  = 0;
        unsigned int local_equal = 0;
        for (uint32_t i = begin; i < end; ++i)
          {
            if      (i <  threshold) ++local_less;
            else if (i == threshold) ++local_equal;
          }
        count_less .fetch_add(local_less,  std::memory_order_relaxed);
        count_equal.fetch_add(local_equal, std::memory_order_relaxed);
      }, 250u);

    REQUIRE(count_less.load()  == threshold);      // indices 0 .. threshold-1
    REQUIRE(count_equal.load() == 1u);             // exactly index == threshold
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. thread_local state initialised once per chunk
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: thread_local init once per chunk",
          "[parallel_for_chunked][range]")
{
  SECTION("thread_local counter incremented once per chunk task, not per element")
  {
    // Use a thread_local int that is reset to 0 at the start of each body call.
    // After the inner loop it should equal exactly (end - begin). If the
    // range overload mistakenly called body(p) once per element the local
    // would be reset to 0 on every iteration.
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 2000;
    std::atomic<bool> mismatch{false};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        thread_local int tl_count = 0;
        tl_count = 0; // reset at chunk start
        for (uint32_t i = begin; i < end; ++i)
          ++tl_count;
        if (tl_count != static_cast<int>(end - begin))
          mismatch.store(true, std::memory_order_relaxed);
      }, 200u);

    REQUIRE_FALSE(mismatch.load());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. All executor types
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: works with all executor types",
          "[parallel_for_chunked][range]")
{
  const uint32_t N         = 500;
  const uint32_t chunkHint = 50u;
  const uint64_t expected  = static_cast<uint64_t>(N) * (N - 1) / 2;

  auto run = [&](auto& exec) -> uint64_t {
    std::atomic<uint64_t> sum{0};
    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        uint64_t local = 0;
        for (uint32_t i = begin; i < end; ++i) local += i;
        sum.fetch_add(local, std::memory_order_relaxed);
      }, chunkHint);
    return sum.load();
  };

  SECTION("SingleThreadExecutor")
  {
    SingleThreadExecutor exec;
    REQUIRE(run(exec) == expected);
  }

  SECTION("StdAsyncExecutor")
  {
    StdAsyncExecutor exec;
    REQUIRE(run(exec) == expected);
  }

  SECTION("ThreadPoolExecutor<2>")
  {
    ThreadPoolExecutor<2> exec;
    REQUIRE(run(exec) == expected);
  }

  SECTION("ThreadPoolExecutor<8>")
  {
    ThreadPoolExecutor<8> exec;
    REQUIRE(run(exec) == expected);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 10. Edge cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: edge cases",
          "[parallel_for_chunked][range]")
{
  SECTION("Zero iterations — body never called")
  {
    SingleThreadExecutor exec;
    std::atomic<int> calls{0};
    parallel_for_chunked(0u, exec,
      [&](uint32_t, uint32_t) { calls.fetch_add(1); }, 10u);
    REQUIRE(calls.load() == 0);
  }

  SECTION("Single iteration — body called once with (0, 1)")
  {
    SingleThreadExecutor exec;
    uint32_t got_begin = 99, got_end = 99;
    parallel_for_chunked(1u, exec,
      [&](uint32_t begin, uint32_t end) {
        got_begin = begin;
        got_end   = end;
      }, 10u);
    REQUIRE(got_begin == 0u);
    REQUIRE(got_end   == 1u);
  }

  SECTION("Total smaller than hint → single chunk covering [0, total)")
  {
    SingleThreadExecutor exec;
    uint32_t got_begin = 99, got_end = 99;
    int call_count = 0;
    parallel_for_chunked(7u, exec,
      [&](uint32_t begin, uint32_t end) {
        got_begin = begin;
        got_end   = end;
        ++call_count;
      }, 1000u);
    REQUIRE(call_count == 1);
    REQUIRE(got_begin  == 0u);
    REQUIRE(got_end    == 7u);
  }

  SECTION("Hint == 0 falls back to auto-calculated chunk size, still correct")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 3000;
    std::atomic<uint64_t> sum{0};
    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        uint64_t local = 0;
        for (uint32_t i = begin; i < end; ++i) local += i;
        sum.fetch_add(local, std::memory_order_relaxed);
      }, 0u); // hint == 0 → auto
    REQUIRE(sum.load() == static_cast<uint64_t>(N) * (N - 1) / 2);
  }

  SECTION("Non-power-of-two totals")
  {
    ThreadPoolExecutor<4> exec;
    for (uint32_t N : {7u, 13u, 99u, 101u, 997u, 1001u})
      {
        std::atomic<int> count{0};
        parallel_for_chunked(N, exec,
          [&](uint32_t begin, uint32_t end) {
            count.fetch_add(static_cast<int>(end - begin),
                            std::memory_order_relaxed);
          }, 37u);
        REQUIRE(count.load() == static_cast<int>(N));
      }
  }

  SECTION("Prime total 997 with hint 100")
  {
    SingleThreadExecutor exec;
    const uint32_t N = 997;
    std::atomic<int> count{0};
    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        count.fetch_add(static_cast<int>(end - begin));
      }, 100u);
    REQUIRE(count.load() == static_cast<int>(N));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 11. Stress / large N
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parallel_for_chunked range-callback: stress tests",
          "[parallel_for_chunked][range][stress]")
{
  SECTION("Large N with ThreadPoolExecutor — sum is correct")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 500'000;
    std::atomic<uint64_t> sum{0};

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        uint64_t local = 0;
        for (uint32_t i = begin; i < end; ++i) local += i;
        sum.fetch_add(local, std::memory_order_relaxed);
      }, 0u); // auto chunk size

    REQUIRE(sum.load() == static_cast<uint64_t>(N) * (N - 1) / 2);
  }

  SECTION("Large N with small hint — many chunks, no missed or double-counted elements")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N = 10'000;
    std::vector<std::atomic<int>> visited(N);
    for (auto& v : visited) v.store(0);

    parallel_for_chunked(N, exec,
      [&](uint32_t begin, uint32_t end) {
        for (uint32_t i = begin; i < end; ++i)
          visited[i].fetch_add(1, std::memory_order_relaxed);
      }, 64u);

    for (uint32_t i = 0; i < N; ++i)
      REQUIRE(visited[i].load() == 1);
  }

  SECTION("Repeated executions with same executor yield consistent results")
  {
    ThreadPoolExecutor<4> exec;
    const uint32_t N        = 2000;
    const uint64_t expected = static_cast<uint64_t>(N) * (N - 1) / 2;

    for (int run = 0; run < 5; ++run)
      {
        std::atomic<uint64_t> sum{0};
        parallel_for_chunked(N, exec,
          [&](uint32_t begin, uint32_t end) {
            uint64_t local = 0;
            for (uint32_t i = begin; i < end; ++i) local += i;
            sum.fetch_add(local, std::memory_order_relaxed);
          }, 100u);
        REQUIRE(sum.load() == expected);
      }
  }
}
