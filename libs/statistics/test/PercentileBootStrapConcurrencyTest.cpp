// PercentileBootstrapThreadSafetyTest.cpp
//
// Thread-safety unit tests for PercentileBootstrap (synchronized version).
// Place in: libs/statistics/test/
//
// These tests verify the thread-safety guarantees added via mutex synchronization:
//  - Concurrent run() calls
//  - Concurrent diagnostic access
//  - Concurrent setChunkSizeHint() calls
//  - RNG protection under concurrent access
//  - Diagnostic consistency under concurrent updates
//
// Requires:
//  - Catch2 v3
//  - randutils.hpp
//  - number.h (DecimalType, num::to_double)
//  - StationaryMaskResamplers.h (StationaryMaskValueResampler)
//  - PercentileBootstrap.h (synchronized version with mutexes)
//  - ParallelExecutors.h
//  - ParallelFor.h
//  - <thread>, <atomic>, <barrier> (C++20)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <barrier>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <random>
#include <chrono>

#include "number.h"
#include "randutils.hpp"
#include "StationaryMaskResamplers.h"
#include "PercentileBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::PercentileBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple sampler: arithmetic mean
struct MeanSamplerPBTS
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(v);
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// Minimal IID value resampler for tests
struct IIDResamplerForTestPBTS
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
        for (std::size_t i = 0; i < m; ++i)
        {
            dst[i] = src[pick(rng)];
        }
    }
};


TEST_CASE("PercentileBootstrap: concurrent run() calls are thread-safe",
          "[Bootstrap][Percentile][ThreadSafety][ConcurrentRun]")
{
    using D = DecimalType;

    // Create test data
    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);
    const std::size_t B  = 400;  // Minimum allowed
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    // Run multiple concurrent run() calls
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successful_runs{0};
    std::atomic<int> exceptions{0};

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&, thread_id = i]()
        {
            try
            {
                // Each thread uses its own RNG with different seed
                randutils::seed_seq_fe128 seed{
                    static_cast<unsigned>(thread_id * 100 + 1),
                    static_cast<unsigned>(thread_id * 100 + 2),
                    static_cast<unsigned>(thread_id * 100 + 3),
                    static_cast<unsigned>(thread_id * 100 + 4)
                };
                std::mt19937_64 rng(seed);

                auto result = pb.run(x, mean_sampler, rng);

                // Verify result is valid
                if (std::isfinite(num::to_double(result.mean)) &&
                    std::isfinite(num::to_double(result.lower)) &&
                    std::isfinite(num::to_double(result.upper)) &&
                    result.effective_B >= B / 2)
                {
                    successful_runs.fetch_add(1, std::memory_order_relaxed);
                }
            }
            catch (...)
            {
                exceptions.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(successful_runs.load() == num_threads);
    REQUIRE(exceptions.load() == 0);
}


TEST_CASE("PercentileBootstrap: concurrent diagnostic access during run()",
          "[Bootstrap][Percentile][ThreadSafety][DiagnosticAccess]")
{
    using D = DecimalType;

    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);
    const std::size_t B  = 400;
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    // First, do one run to populate diagnostics
    randutils::seed_seq_fe128 init_seed{1u, 2u, 3u, 4u};
    std::mt19937_64 init_rng(init_seed);
    pb.run(x, mean_sampler, init_rng);

    REQUIRE(pb.hasDiagnostics());

    std::atomic<bool> stop_flag{false};
    std::atomic<int> reader_successes{0};
    std::atomic<int> writer_successes{0};
    std::atomic<int> exceptions{0};

    // Start reader threads that continuously access diagnostics
    std::vector<std::thread> threads;
    const int num_readers = 3;
    const int num_writers = 2;

    for (int i = 0; i < num_readers; ++i)
    {
        threads.emplace_back([&]()
        {
            try
            {
                while (!stop_flag.load(std::memory_order_relaxed))
                {
                    // Access all diagnostic methods
                    bool has_diag = pb.hasDiagnostics();
                    if (has_diag)
                    {
                        auto stats = pb.getBootstrapStatistics();
                        double mean_boot = pb.getBootstrapMean();
                        double var_boot = pb.getBootstrapVariance();
                        double se_boot = pb.getBootstrapSe();

                        // Verify internal consistency
                        if (!stats.empty() &&
                            std::isfinite(mean_boot) &&
                            std::isfinite(var_boot) &&
                            std::isfinite(se_boot) &&
                            var_boot >= 0.0 &&
                            se_boot >= 0.0)
                        {
                            reader_successes.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
            catch (...)
            {
                exceptions.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Start writer threads that call run()
    for (int i = 0; i < num_writers; ++i)
    {
        threads.emplace_back([&, thread_id = i]()
        {
            try
            {
                for (int j = 0; j < 5; ++j)  // Each writer does 5 runs
                {
                    randutils::seed_seq_fe128 seed{
                        static_cast<unsigned>(thread_id * 1000 + j * 10 + 1),
                        static_cast<unsigned>(thread_id * 1000 + j * 10 + 2),
                        static_cast<unsigned>(thread_id * 1000 + j * 10 + 3),
                        static_cast<unsigned>(thread_id * 1000 + j * 10 + 4)
                    };
                    std::mt19937_64 rng(seed);

                    auto result = pb.run(x, mean_sampler, rng);

                    if (result.effective_B >= B / 2)
                    {
                        writer_successes.fetch_add(1, std::memory_order_relaxed);
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            catch (...)
            {
                exceptions.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop_flag.store(true, std::memory_order_relaxed);

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(writer_successes.load() == num_writers * 5);
    REQUIRE(reader_successes.load() > 0);  // At least some reads succeeded
    REQUIRE(exceptions.load() == 0);
}


TEST_CASE("PercentileBootstrap: concurrent setChunkSizeHint() is thread-safe",
          "[Bootstrap][Percentile][ThreadSafety][ChunkHint]")
{
    using D = DecimalType;

    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);
    const std::size_t B  = 400;
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    std::atomic<bool> stop_flag{false};
    std::atomic<int> successes{0};
    std::atomic<int> exceptions{0};

    std::vector<std::thread> threads;
    const int num_threads = 6;

    // Mix of threads setting chunk hints and running bootstrap
    for (int i = 0; i < num_threads; ++i)
    {
        if (i % 2 == 0)
        {
            // Chunk hint setter threads
            threads.emplace_back([&]()
            {
                try
                {
                    while (!stop_flag.load(std::memory_order_relaxed))
                    {
                        pb.setChunkSizeHint(static_cast<uint32_t>(10 + (i * 5)));
                        successes.fetch_add(1, std::memory_order_relaxed);
                        std::this_thread::sleep_for(std::chrono::microseconds(50));
                    }
                }
                catch (...)
                {
                    exceptions.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        else
        {
            // Runner threads
            threads.emplace_back([&, thread_id = i]()
            {
                try
                {
                    for (int j = 0; j < 3; ++j)
                    {
                        randutils::seed_seq_fe128 seed{
                            static_cast<unsigned>(thread_id * 100 + j + 1),
                            static_cast<unsigned>(thread_id * 100 + j + 2),
                            static_cast<unsigned>(thread_id * 100 + j + 3),
                            static_cast<unsigned>(thread_id * 100 + j + 4)
                        };
                        std::mt19937_64 rng(seed);

                        auto result = pb.run(x, mean_sampler, rng);
                        if (result.effective_B >= B / 2)
                        {
                            successes.fetch_add(1, std::memory_order_relaxed);
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                }
                catch (...)
                {
                    exceptions.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop_flag.store(true, std::memory_order_relaxed);

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(successes.load() > 0);
    REQUIRE(exceptions.load() == 0);
}


TEST_CASE("PercentileBootstrap: RNG mutex protects shared RNG state",
          "[Bootstrap][Percentile][ThreadSafety][RNG]")
{
    using D = DecimalType;

    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);
    const std::size_t B  = 400;
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    // SHARED RNG - this is the critical test
    randutils::seed_seq_fe128 seed{99u, 88u, 77u, 66u};
    std::mt19937_64 shared_rng(seed);

    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successful_runs{0};
    std::atomic<int> exceptions{0};

    // All threads share the same RNG - mutex should protect it
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]()
        {
            try
            {
                // All threads use the SAME shared_rng reference
                auto result = pb.run(x, mean_sampler, shared_rng);

                if (std::isfinite(num::to_double(result.mean)) &&
                    result.effective_B >= B / 2)
                {
                    successful_runs.fetch_add(1, std::memory_order_relaxed);
                }
            }
            catch (...)
            {
                exceptions.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All runs should succeed without data races
    REQUIRE(successful_runs.load() == num_threads);
    REQUIRE(exceptions.load() == 0);
}


TEST_CASE("PercentileBootstrap: diagnostic consistency under concurrent updates",
          "[Bootstrap][Percentile][ThreadSafety][DiagnosticConsistency]")
{
    using D = DecimalType;

    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);
    const std::size_t B  = 400;
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    std::atomic<bool> stop_flag{false};
    std::atomic<int> consistency_checks{0};
    std::atomic<int> inconsistencies{0};
    std::atomic<int> exceptions{0};

    std::vector<std::thread> threads;
    const int num_updaters = 2;
    const int num_checkers = 3;

    // Threads that update diagnostics by calling run()
    for (int i = 0; i < num_updaters; ++i)
    {
        threads.emplace_back([&, thread_id = i]()
        {
            try
            {
                for (int j = 0; j < 10; ++j)
                {
                    randutils::seed_seq_fe128 seed{
                        static_cast<unsigned>(thread_id * 1000 + j + 1),
                        static_cast<unsigned>(thread_id * 1000 + j + 2),
                        static_cast<unsigned>(thread_id * 1000 + j + 3),
                        static_cast<unsigned>(thread_id * 1000 + j + 4)
                    };
                    std::mt19937_64 rng(seed);

                    pb.run(x, mean_sampler, rng);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
            catch (...)
            {
                exceptions.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Threads that check diagnostic consistency
    for (int i = 0; i < num_checkers; ++i)
    {
        threads.emplace_back([&]()
        {
            try
            {
                while (!stop_flag.load(std::memory_order_relaxed))
                {
                    if (pb.hasDiagnostics())
                    {
                        // Get all diagnostics atomically to ensure consistency
                        auto diagnostics = pb.getAllDiagnostics();

                        // Verify internal consistency
                        if (!diagnostics.bootstrapStats.empty() && diagnostics.valid)
                        {
                            // Recompute mean
                            double computed_mean = 0.0;
                            for (double v : diagnostics.bootstrapStats) {
                                computed_mean += v;
                            }
                            computed_mean /= static_cast<double>(diagnostics.bootstrapStats.size());

                            // Recompute variance
                            double computed_var = 0.0;
                            if (diagnostics.bootstrapStats.size() > 1) {
                                for (double v : diagnostics.bootstrapStats) {
                                    double d = v - computed_mean;
                                    computed_var += d * d;
                                }
                                computed_var /= static_cast<double>(diagnostics.bootstrapStats.size() - 1);
                            }

                            double computed_se = std::sqrt(computed_var);

                            // Check consistency (with tolerance for floating point)
                            const double tol = 1e-10;
                            bool mean_ok = std::fabs(diagnostics.meanBoot - computed_mean) < tol;
                            bool var_ok = std::fabs(diagnostics.varBoot - computed_var) < tol;
                            bool se_ok = std::fabs(diagnostics.seBoot - computed_se) < tol;

                            if (mean_ok && var_ok && se_ok)
                            {
                                consistency_checks.fetch_add(1, std::memory_order_relaxed);
                            }
                            else
                            {
                                inconsistencies.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            catch (...)
            {
                exceptions.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Let the test run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop_flag.store(true, std::memory_order_relaxed);

    for (auto& t : threads) {
        t.join();
    }

    // Should have many consistency checks and NO inconsistencies
    REQUIRE(consistency_checks.load() > 0);
    REQUIRE(inconsistencies.load() == 0);
    REQUIRE(exceptions.load() == 0);
}


TEST_CASE("PercentileBootstrap: stress test with many concurrent operations",
          "[Bootstrap][Percentile][ThreadSafety][Stress]")
{
    using D = DecimalType;

    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);
    const std::size_t B  = 400;
    const double      CL = 0.95;

    PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pb(B, CL, res);

    // Do initial run
    randutils::seed_seq_fe128 init_seed{1u, 2u, 3u, 4u};
    std::mt19937_64 init_rng(init_seed);
    pb.run(x, mean_sampler, init_rng);

    std::atomic<bool> stop_flag{false};
    std::atomic<int> total_operations{0};
    std::atomic<int> exceptions{0};

    std::vector<std::thread> threads;
    const int num_threads = 8;

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&, thread_id = i]()
        {
            try
            {
                std::mt19937_64 local_gen(thread_id * 12345);
                std::uniform_int_distribution<int> op_dist(0, 5);

                while (!stop_flag.load(std::memory_order_relaxed))
                {
                    int op = op_dist(local_gen);

                    switch (op)
                    {
                        case 0:  // run()
                        {
                            randutils::seed_seq_fe128 seed{
                                static_cast<unsigned>(thread_id * 100 + 1),
                                static_cast<unsigned>(thread_id * 100 + 2),
                                static_cast<unsigned>(thread_id * 100 + 3),
                                static_cast<unsigned>(thread_id * 100 + 4)
                            };
                            std::mt19937_64 rng(seed);
                            pb.run(x, mean_sampler, rng);
                            break;
                        }
                        case 1:  // hasDiagnostics()
                            pb.hasDiagnostics();
                            break;
                        case 2:  // getBootstrapStatistics()
                            if (pb.hasDiagnostics()) {
                                pb.getBootstrapStatistics();
                            }
                            break;
                        case 3:  // getBootstrapMean()
                            if (pb.hasDiagnostics()) {
                                pb.getBootstrapMean();
                            }
                            break;
                        case 4:  // getBootstrapVariance()
                            if (pb.hasDiagnostics()) {
                                pb.getBootstrapVariance();
                            }
                            break;
                        case 5:  // setChunkSizeHint()
                            pb.setChunkSizeHint(static_cast<uint32_t>(10 + thread_id));
                            break;
                    }

                    total_operations.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            catch (...)
            {
                exceptions.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Run stress test
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop_flag.store(true, std::memory_order_relaxed);

    for (auto& t : threads) {
        t.join();
    }

    // Should complete many operations without exceptions
    REQUIRE(total_operations.load() > 100);  // Should do many operations
    REQUIRE(exceptions.load() == 0);
}


TEST_CASE("PercentileBootstrap: synchronized access preserves determinism",
          "[Bootstrap][Percentile][ThreadSafety][Determinism]")
{
    using D = DecimalType;

    const std::size_t n = 50;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(3);
    const std::size_t B  = 400;
    const double      CL = 0.95;

    // Run same bootstrap multiple times with same seed
    std::vector<typename PercentileBootstrap<D, decltype(mean_sampler), 
                StationaryMaskValueResampler<D>>::Result> results;

    for (int run = 0; run < 5; ++run)
    {
        PercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pb(B, CL, res);

        randutils::seed_seq_fe128 seed{11u, 22u, 33u, 44u};
        std::mt19937_64 rng(seed);

        results.push_back(pb.run(x, mean_sampler, rng));
    }

    // All results should be identical (deterministic)
    for (size_t i = 1; i < results.size(); ++i)
    {
        REQUIRE(num::to_double(results[i].mean) == 
                Catch::Approx(num::to_double(results[0].mean)).margin(1e-12));
        REQUIRE(num::to_double(results[i].lower) == 
                Catch::Approx(num::to_double(results[0].lower)).margin(1e-12));
        REQUIRE(num::to_double(results[i].upper) == 
                Catch::Approx(num::to_double(results[0].upper)).margin(1e-12));
        REQUIRE(results[i].effective_B == results[0].effective_B);
        REQUIRE(results[i].skipped == results[0].skipped);
    }
}
