// BasicBootstrapConcurrencyTest.cpp
//
// Concurrency-focused unit tests for BasicBootstrap to verify that race conditions
// have been properly fixed.
//
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - randutils.hpp
//  - number.h (DecimalType, num::to_double)
//  - StationaryMaskResamplers.h (StationaryMaskValueResampler)
//  - BasicBootstrap.h
//  - ParallelExecutors.h
//  - ParallelFor.h
//  - C++11 threading support

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include <future>

#include "number.h"
#include "randutils.hpp"
#include "StationaryMaskResamplers.h"
#include "BasicBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::BasicBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple arithmetic mean sampler
struct MeanSamplerBBConcurrency
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(v);
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// IID-with-replacement resampler for tests
struct IIDResamplerForTestBBConcurrency
{
    std::size_t getL() const noexcept { return 0; }

    template <typename Decimal, typename Rng>
    void operator()(const std::vector<Decimal>& src,
                    std::vector<Decimal>&       dst,
                    std::size_t                 m,
                    Rng&                         rng) const
    {
        std::uniform_int_distribution<std::size_t> pick(0, src.size() - 1);
        dst.resize(m);
        for (std::size_t i = 0; i < m; ++i) {
            dst[i] = src[pick(rng)];
        }
    }
};

template <typename Exec>
using BasicBootstrapExec = BasicBootstrap<
    double,                            // Decimal
    MeanSamplerBBConcurrency,          // Sampler
    IIDResamplerForTestBBConcurrency,  // Resampler
    std::mt19937_64,                   // Rng
    Exec                               // Executor
>;

// ============================================================================
// TEST 1: RNG Protection with ThreadPoolExecutor
// ============================================================================
TEST_CASE("BasicBootstrap: RNG thread safety with ThreadPoolExecutor",
          "[Bootstrap][Basic][Concurrency][RNG]")
{
    // This test verifies that the RNG mutex properly protects concurrent access
    // during parallel bootstrap iterations. Previously, this would cause data races.

    std::mt19937_64 gen_data(12345);
    std::normal_distribution<double> dist(10.0, 2.0);
    std::vector<double> x(100);
    for (auto& v : x) {
        v = dist(gen_data);
    }

    const std::size_t B = 1000;
    const double CL = 0.95;

    IIDResamplerForTestBBConcurrency resampler{};
    MeanSamplerBBConcurrency sampler{};

    // Use ThreadPoolExecutor to stress-test RNG protection
    BasicBootstrapExec<concurrency::ThreadPoolExecutor<4>> bb(B, CL, resampler);

    // Run multiple times to increase chance of detecting race conditions
    const int num_runs = 10;
    std::vector<double> lower_bounds;
    std::vector<double> upper_bounds;
    
    lower_bounds.reserve(num_runs);
    upper_bounds.reserve(num_runs);

    for (int run = 0; run < num_runs; ++run) {
        std::mt19937_64 rng(static_cast<uint64_t>(42 + run));
        auto result = bb.run(x, sampler, rng);

        REQUIRE(result.B == B);
        REQUIRE(result.effective_B > B / 2);
        REQUIRE(std::isfinite(result.mean));
        REQUIRE(std::isfinite(result.lower));
        REQUIRE(std::isfinite(result.upper));
        REQUIRE(result.lower <= result.upper);

        lower_bounds.push_back(result.lower);
        upper_bounds.push_back(result.upper);
    }

    // Results should vary slightly due to different RNG seeds, but all should be valid
    SECTION("All runs produce valid finite results")
    {
        for (size_t i = 0; i < lower_bounds.size(); ++i) {
            REQUIRE(std::isfinite(lower_bounds[i]));
            REQUIRE(std::isfinite(upper_bounds[i]));
            REQUIRE(lower_bounds[i] <= upper_bounds[i]);
        }
    }
}

// ============================================================================
// TEST 2: Concurrent Calls to setChunkSizeHint (Atomic Protection)
// ============================================================================
TEST_CASE("BasicBootstrap: concurrent setChunkSizeHint calls are safe",
          "[Bootstrap][Basic][Concurrency][ChunkHint]")
{
    // This test verifies that setChunkSizeHint can be called concurrently
    // with run() without causing data races (via atomic protection).

    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> x;
    for (int i = 0; i < 50; ++i) {
        x.push_back(D(static_cast<double>(i)));
    }

    const std::size_t B = 500;
    const double CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    std::atomic<bool> stop_flag{false};
    std::atomic<int> error_count{0};

    // Thread 1: Repeatedly call setChunkSizeHint
    auto hint_setter = [&bb, &stop_flag]() {
        uint32_t hint = 1;
        while (!stop_flag.load(std::memory_order_relaxed)) {
            bb.setChunkSizeHint(hint);
            hint = (hint % 100) + 1; // Cycle through different values
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    // Thread 2: Run bootstrap
    auto bootstrap_runner = [&bb, &x, &mean_sampler, &error_count]() {
        try {
            randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
            std::mt19937_64 rng(seed);
            auto result = bb.run(x, mean_sampler, rng);
            
            if (!std::isfinite(num::to_double(result.mean)) ||
                !std::isfinite(num::to_double(result.lower)) ||
                !std::isfinite(num::to_double(result.upper)) ||
                result.lower > result.upper) {
                error_count.fetch_add(1, std::memory_order_relaxed);
            }
        } catch (...) {
            error_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::thread t1(hint_setter);
    std::thread t2(bootstrap_runner);

    t2.join(); // Wait for bootstrap to complete
    stop_flag.store(true, std::memory_order_relaxed);
    t1.join();

    REQUIRE(error_count.load() == 0);
}

// ============================================================================
// TEST 3: Multiple Sequential Runs Update Diagnostics Correctly
// ============================================================================
TEST_CASE("BasicBootstrap: sequential runs update diagnostics correctly",
          "[Bootstrap][Basic][Concurrency][Diagnostics]")
{
    // This test verifies that diagnostic members are properly updated on each run()
    // and that there are no stale values from previous runs.

    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B = 500;
    const double CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    // First dataset: mean around 5
    std::vector<D> x1;
    for (int i = 0; i < 50; ++i) {
        x1.push_back(D(static_cast<double>(i) / 10.0));
    }

    // Second dataset: mean around 50
    std::vector<D> x2;
    for (int i = 0; i < 50; ++i) {
        x2.push_back(D(static_cast<double>(i)));
    }

    randutils::seed_seq_fe128 seed1{1u, 2u, 3u, 4u};
    randutils::seed_seq_fe128 seed2{5u, 6u, 7u, 8u};
    std::mt19937_64 rng1(seed1);
    std::mt19937_64 rng2(seed2);

    // Run 1
    REQUIRE_FALSE(bb.hasDiagnostics());
    auto result1 = bb.run(x1, mean_sampler, rng1);
    REQUIRE(bb.hasDiagnostics());
    
    // Verify the first run was successful
    REQUIRE(result1.B == B);
    REQUIRE(std::isfinite(num::to_double(result1.mean)));

    double mean1 = bb.getBootstrapMean();
    double var1 = bb.getBootstrapVariance();
    double se1 = bb.getBootstrapSe();
    auto stats1 = bb.getBootstrapStatistics();

    // Run 2
    auto result2 = bb.run(x2, mean_sampler, rng2);
    REQUIRE(bb.hasDiagnostics());

    double mean2 = bb.getBootstrapMean();
    double var2 = bb.getBootstrapVariance();
    double se2 = bb.getBootstrapSe();
    auto stats2 = bb.getBootstrapStatistics();

    SECTION("Diagnostics are different between runs")
    {
        // The bootstrap means should be significantly different since x2 has much larger values
        REQUIRE(std::abs(mean2 - mean1) > 10.0);
        
        // Statistics vectors should have the same size (both are valid runs)
        REQUIRE(stats1.size() > B / 2);
        REQUIRE(stats2.size() > B / 2);
        
        // Variance should also be different
        REQUIRE(var2 != var1);
        REQUIRE(se2 != se1);
    }

    SECTION("Second run diagnostics match result")
    {
        // The diagnostic mean should be close to the actual result mean
        const double result_mean = num::to_double(result2.mean);
        REQUIRE(std::abs(mean2 - result_mean) < 5.0); // Reasonable tolerance
    }
}

// ============================================================================
// TEST 4: Diagnostic Getters Throw Before First Run
// ============================================================================
TEST_CASE("BasicBootstrap: diagnostic getters throw before run in concurrent context",
          "[Bootstrap][Basic][Concurrency][Diagnostics]")
{
    // Verifies that m_diagValid flag works correctly and prevents access to
    // uninitialized diagnostics.

    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B = 500;
    const double CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    REQUIRE_FALSE(bb.hasDiagnostics());

    SECTION("All diagnostic getters throw before run")
    {
        REQUIRE_THROWS_AS(bb.getBootstrapStatistics(), std::logic_error);
        REQUIRE_THROWS_AS(bb.getBootstrapMean(), std::logic_error);
        REQUIRE_THROWS_AS(bb.getBootstrapVariance(), std::logic_error);
        REQUIRE_THROWS_AS(bb.getBootstrapSe(), std::logic_error);
    }

    SECTION("Diagnostic getters work after run")
    {
        std::vector<D> x{D(1), D(2), D(3), D(4), D(5)};
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);

        auto result = bb.run(x, mean_sampler, rng);
        REQUIRE(bb.hasDiagnostics());
        
        // Verify the run was successful
        REQUIRE(result.B == B);
        REQUIRE(std::isfinite(num::to_double(result.mean)));

        REQUIRE_NOTHROW(bb.getBootstrapStatistics());
        REQUIRE_NOTHROW(bb.getBootstrapMean());
        REQUIRE_NOTHROW(bb.getBootstrapVariance());
        REQUIRE_NOTHROW(bb.getBootstrapSe());

        // Verify values are reasonable
        auto stats = bb.getBootstrapStatistics();
        REQUIRE(stats.size() > B / 2);
        REQUIRE(std::isfinite(bb.getBootstrapMean()));
        REQUIRE(std::isfinite(bb.getBootstrapVariance()));
        REQUIRE(std::isfinite(bb.getBootstrapSe()));
        REQUIRE(bb.getBootstrapVariance() >= 0.0);
        REQUIRE(bb.getBootstrapSe() >= 0.0);
    }
}

// ============================================================================
// TEST 5: ThreadPoolExecutor Consistency Across Multiple Runs
// ============================================================================
TEST_CASE("BasicBootstrap: ThreadPoolExecutor produces consistent results",
          "[Bootstrap][Basic][Concurrency][Consistency]")
{
    // This test verifies that using ThreadPoolExecutor doesn't introduce
    // non-determinism when using the same RNG seed.

    std::mt19937_64 gen_data(98765);
    std::normal_distribution<double> dist(100.0, 15.0);
    std::vector<double> x(200);
    for (auto& v : x) {
        v = dist(gen_data);
    }

    const std::size_t B = 1000;
    const double CL = 0.95;

    IIDResamplerForTestBBConcurrency resampler{};
    MeanSamplerBBConcurrency sampler{};

    BasicBootstrapExec<concurrency::ThreadPoolExecutor<4>> bb(B, CL, resampler);

    // Run multiple times with the same seed
    const int num_trials = 5;
    std::vector<double> means;
    std::vector<double> lowers;
    std::vector<double> uppers;

    for (int trial = 0; trial < num_trials; ++trial) {
        std::mt19937_64 rng(0xDEADBEEFu); // Same seed every time
        auto result = bb.run(x, sampler, rng);

        means.push_back(result.mean);
        lowers.push_back(result.lower);
        uppers.push_back(result.upper);
    }

    // All runs should produce identical results with same seed
    auto near = [](double a, double b, double tol = 1e-10) {
        return std::fabs(a - b) <= tol;
    };

    SECTION("Results are deterministic with same RNG seed")
    {
        for (int i = 1; i < num_trials; ++i) {
            REQUIRE(near(means[i], means[0]));
            REQUIRE(near(lowers[i], lowers[0]));
            REQUIRE(near(uppers[i], uppers[0]));
        }
    }
}

// ============================================================================
// TEST 6: Stress Test with Rapid Sequential Runs
// ============================================================================
TEST_CASE("BasicBootstrap: stress test with rapid sequential runs",
          "[Bootstrap][Basic][Concurrency][Stress]")
{
    // This test rapidly calls run() many times to stress-test the diagnostic
    // update mechanism and ensure no memory corruption or stale data issues.

    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> x;
    for (int i = 0; i < 30; ++i) {
        x.push_back(D(static_cast<double>(i)));
    }

    const std::size_t B = 400; // Minimum allowed
    const double CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    const int num_runs = 20;
    int successful_runs = 0;

    for (int run = 0; run < num_runs; ++run) {
        randutils::seed_seq_fe128 seed{
            static_cast<uint32_t>(run),
            static_cast<uint32_t>(run + 1),
            static_cast<uint32_t>(run + 2),
            static_cast<uint32_t>(run + 3)
        };
        std::mt19937_64 rng(seed);

        try {
            auto result = bb.run(x, mean_sampler, rng);

            REQUIRE(bb.hasDiagnostics());
            REQUIRE(std::isfinite(num::to_double(result.mean)));
            REQUIRE(std::isfinite(num::to_double(result.lower)));
            REQUIRE(std::isfinite(num::to_double(result.upper)));
            REQUIRE(result.lower <= result.upper);

            // Verify diagnostics are accessible
            auto stats = bb.getBootstrapStatistics();
            REQUIRE(stats.size() > B / 2);
            REQUIRE(std::isfinite(bb.getBootstrapMean()));
            REQUIRE(std::isfinite(bb.getBootstrapVariance()));
            REQUIRE(std::isfinite(bb.getBootstrapSe()));

            successful_runs++;
        } catch (const std::exception& e) {
            FAIL("Run " << run << " failed with exception: " << e.what());
        }
    }

    REQUIRE(successful_runs == num_runs);
}

// ============================================================================
// TEST 7: Verify No Data Races with Thread Sanitizer (TSan) Compatible Test
// ============================================================================
TEST_CASE("BasicBootstrap: TSan-compatible concurrent diagnostic access pattern",
          "[Bootstrap][Basic][Concurrency][TSan]")
{
    // This test is designed to be run with ThreadSanitizer (TSan) to detect
    // any remaining data races. It creates a pattern where one thread runs
    // bootstrap while another waits and then accesses diagnostics.

    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> x;
    for (int i = 0; i < 50; ++i) {
        x.push_back(D(static_cast<double>(i)));
    }

    const std::size_t B = 500;
    const double CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    std::atomic<bool> run_complete{false};
    bool access_success = false;

    auto runner = [&bb, &x, &mean_sampler, &run_complete]() {
        randutils::seed_seq_fe128 seed{1u, 2u, 3u, 4u};
        std::mt19937_64 rng(seed);
        bb.run(x, mean_sampler, rng);
        run_complete.store(true, std::memory_order_release);
    };

    auto accessor = [&bb, &run_complete, &access_success]() {
        // Wait for run to complete
        while (!run_complete.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        // Now safely access diagnostics
        if (bb.hasDiagnostics()) {
            auto stats = bb.getBootstrapStatistics();
            double mean = bb.getBootstrapMean();
            double var = bb.getBootstrapVariance();
            double se = bb.getBootstrapSe();
            
            access_success = (stats.size() > 0) && 
                           std::isfinite(mean) && 
                           std::isfinite(var) && 
                           std::isfinite(se);
        }
    };

    std::thread t1(runner);
    std::thread t2(accessor);

    t1.join();
    t2.join();

    REQUIRE(run_complete.load());
    REQUIRE(access_success);
}

// ============================================================================
// TEST 8: Parallel Runs on Different Instances (Should Be Safe)
// ============================================================================
TEST_CASE("BasicBootstrap: parallel runs on different instances are safe",
          "[Bootstrap][Basic][Concurrency][MultiInstance]")
{
    // This test verifies that running bootstrap on multiple independent instances
    // concurrently is safe (each instance has its own state).

    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> x1, x2, x3, x4;
    for (int i = 0; i < 40; ++i) {
        x1.push_back(D(static_cast<double>(i)));
        x2.push_back(D(static_cast<double>(i * 2)));
        x3.push_back(D(static_cast<double>(i * 3)));
        x4.push_back(D(static_cast<double>(i * 4)));
    }

    const std::size_t B = 500;
    const double CL = 0.95;

    using BBType = BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>;

    std::atomic<int> success_count{0};

    auto run_bootstrap = [&mean_sampler, &res, B, CL, &success_count]
                        (const std::vector<D>& x, uint32_t seed_val) {
        try {
            BBType bb(B, CL, res);
            randutils::seed_seq_fe128 seed{seed_val, seed_val + 1, seed_val + 2, seed_val + 3};
            std::mt19937_64 rng(seed);
            
            auto result = bb.run(x, mean_sampler, rng);
            
            if (std::isfinite(num::to_double(result.mean)) &&
                std::isfinite(num::to_double(result.lower)) &&
                std::isfinite(num::to_double(result.upper)) &&
                result.lower <= result.upper &&
                bb.hasDiagnostics()) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        } catch (...) {
            // Failure - don't increment success_count
        }
    };

    std::thread t1([&]() { run_bootstrap(x1, 1u); });
    std::thread t2([&]() { run_bootstrap(x2, 2u); });
    std::thread t3([&]() { run_bootstrap(x3, 3u); });
    std::thread t4([&]() { run_bootstrap(x4, 4u); });

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    REQUIRE(success_count.load() == 4);
}

// ============================================================================
// TEST 9: Provider-Based Run with Concurrent Execution
// ============================================================================
TEST_CASE("BasicBootstrap: provider-based run with ThreadPoolExecutor",
          "[Bootstrap][Basic][Concurrency][Provider]")
{
    // This test uses the provider-based run() method with parallel execution
    // to ensure it's also thread-safe.

    struct SimpleProvider {
        std::mt19937_64 make_engine(std::size_t b) const {
            randutils::seed_seq_fe128 seed{
                static_cast<uint32_t>(b),
                static_cast<uint32_t>(b >> 32),
                0xCAFEBABEu,
                0xDEADBEEFu
            };
            return std::mt19937_64(seed);
        }
    };

    std::vector<double> x(100);
    std::mt19937_64 gen(12345);
    std::normal_distribution<double> dist(50.0, 10.0);
    for (auto& v : x) {
        v = dist(gen);
    }

    const std::size_t B = 1000;
    const double CL = 0.95;

    IIDResamplerForTestBBConcurrency resampler{};
    MeanSamplerBBConcurrency sampler{};

    BasicBootstrapExec<concurrency::ThreadPoolExecutor<4>> bb(B, CL, resampler);

    SimpleProvider provider;
    auto result = bb.run(x, sampler, provider);

    REQUIRE(result.B == B);
    REQUIRE(result.effective_B > B / 2);
    REQUIRE(std::isfinite(result.mean));
    REQUIRE(std::isfinite(result.lower));
    REQUIRE(std::isfinite(result.upper));
    REQUIRE(result.lower <= result.upper);
    REQUIRE(bb.hasDiagnostics());

    // Verify diagnostics
    auto stats = bb.getBootstrapStatistics();
    REQUIRE(stats.size() == result.effective_B);
    REQUIRE(std::isfinite(bb.getBootstrapMean()));
    REQUIRE(std::isfinite(bb.getBootstrapVariance()));
    REQUIRE(std::isfinite(bb.getBootstrapSe()));
}

// ============================================================================
// TEST 10: Verify Atomic ChunkHint Doesn't Affect Correctness
// ============================================================================
TEST_CASE("BasicBootstrap: atomic chunk hint updates don't affect correctness",
          "[Bootstrap][Basic][Concurrency][ChunkHint]")
{
    // Verifies that dynamically changing chunk hints during execution
    // doesn't cause incorrect results.

    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> x;
    for (int i = 0; i < 100; ++i) {
        x.push_back(D(static_cast<double>(i)));
    }

    const std::size_t B = 1000;
    const double CL = 0.95;

    BasicBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        bb(B, CL, res);

    // Set initial chunk hint
    bb.setChunkSizeHint(10);

    randutils::seed_seq_fe128 seed1{1u, 2u, 3u, 4u};
    std::mt19937_64 rng1(seed1);
    auto result1 = bb.run(x, mean_sampler, rng1);

    // Change chunk hint and run again with same seed
    bb.setChunkSizeHint(50);

    randutils::seed_seq_fe128 seed2{1u, 2u, 3u, 4u}; // Same seed
    std::mt19937_64 rng2(seed2);
    auto result2 = bb.run(x, mean_sampler, rng2);

    // Results should be identical (chunk hint shouldn't affect randomness)
    auto near = [](double a, double b, double tol = 1e-10) {
        return std::fabs(a - b) <= tol;
    };

    REQUIRE(near(num::to_double(result1.mean), num::to_double(result2.mean)));
    REQUIRE(near(num::to_double(result1.lower), num::to_double(result2.lower)));
    REQUIRE(near(num::to_double(result1.upper), num::to_double(result2.upper)));
}
