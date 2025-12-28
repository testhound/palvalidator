// NormalBootstrapConcurrencyTest.cpp
//
// Comprehensive concurrency tests for NormalBootstrap to verify thread-safety
// of the mutex-protected implementation.
//
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - randutils.hpp
//  - number.h (DecimalType, num::to_double)
//  - StationaryMaskResamplers.h (StationaryMaskValueResampler)
//  - NormalBootstrap.h
//  - ParallelExecutors.h
//  - ParallelFor.h
//  - C++11 threading support (<thread>, <mutex>, <atomic>)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>

#include "number.h"
#include "randutils.hpp"
#include "StationaryMaskResamplers.h"
#include "NormalBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::NormalBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Simple arithmetic mean sampler
struct MeanSamplerConcurrency
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
struct IIDResamplerConcurrency
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
using NormalBootstrapExec = NormalBootstrap<
    double,                    // Decimal
    MeanSamplerConcurrency,    // Sampler
    IIDResamplerConcurrency,   // Resampler
    std::mt19937_64,           // Rng
    Exec                       // Executor
>;

TEST_CASE("NormalBootstrap: concurrent run() calls on same instance", 
          "[Bootstrap][Normal][Concurrency][Run]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    // Create test data
    const std::size_t n = 50;
    std::vector<D> x; 
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    const std::size_t B  = 500;
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    SECTION("Two threads calling run() concurrently")
    {
        std::atomic<int> completed{0};
        std::atomic<bool> had_exception{false};

        auto worker = [&]() {
            try {
                randutils::seed_seq_fe128 seed{
                    static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())),
                    11u, 22u, 33u
                };
                std::mt19937_64 rng(seed);
                
                auto result = nb.run(x, mean_sampler, rng);
                
                // Verify result is valid
                REQUIRE(result.B == B);
                REQUIRE(result.n == n);
                REQUIRE(std::isfinite(num::to_double(result.mean)));
                REQUIRE(std::isfinite(num::to_double(result.lower)));
                REQUIRE(std::isfinite(num::to_double(result.upper)));
                REQUIRE(result.lower <= result.mean);
                REQUIRE(result.mean <= result.upper);
                
                completed.fetch_add(1);
            } catch (...) {
                had_exception.store(true);
            }
        };

        std::thread t1(worker);
        std::thread t2(worker);

        t1.join();
        t2.join();

        REQUIRE_FALSE(had_exception.load());
        REQUIRE(completed.load() == 2);
        REQUIRE(nb.hasDiagnostics());  // Should have diagnostics from one of the runs
    }

    SECTION("Four threads calling run() concurrently")
    {
        const int num_threads = 4;
        std::atomic<int> completed{0};
        std::atomic<bool> had_exception{false};
        std::vector<std::thread> threads;

        auto worker = [&](int thread_id) {
            try {
                randutils::seed_seq_fe128 seed{
                    static_cast<uint32_t>(thread_id),
                    11u, 22u, 33u
                };
                std::mt19937_64 rng(seed);
                
                auto result = nb.run(x, mean_sampler, rng);
                
                REQUIRE(result.B == B);
                REQUIRE(result.n == n);
                REQUIRE(std::isfinite(num::to_double(result.mean)));
                
                completed.fetch_add(1);
            } catch (...) {
                had_exception.store(true);
            }
        };

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, i);
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE_FALSE(had_exception.load());
        REQUIRE(completed.load() == num_threads);
    }
}

TEST_CASE("NormalBootstrap: concurrent diagnostic reads during run()", 
          "[Bootstrap][Normal][Concurrency][Diagnostics]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t n = 50;
    std::vector<D> x; 
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    const std::size_t B  = 1000;  // Longer run to increase chance of overlap
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    // Initialize with one run so diagnostics are available
    randutils::seed_seq_fe128 initial_seed{1u, 2u, 3u, 4u};
    std::mt19937_64 initial_rng(initial_seed);
    nb.run(x, mean_sampler, initial_rng);
    REQUIRE(nb.hasDiagnostics());

    SECTION("Read diagnostics while run() is executing")
    {
        std::atomic<bool> run_thread_started{false};
        std::atomic<bool> run_thread_finished{false};
        std::atomic<bool> had_exception{false};
        std::atomic<int> successful_reads{0};

        // Thread that calls run()
        auto run_worker = [&]() {
            try {
                run_thread_started.store(true);
                
                randutils::seed_seq_fe128 seed{99u, 88u, 77u, 66u};
                std::mt19937_64 rng(seed);
                
                auto result = nb.run(x, mean_sampler, rng);
                REQUIRE(result.B == B);
                
                run_thread_finished.store(true);
            } catch (...) {
                had_exception.store(true);
            }
        };

        // Thread that reads diagnostics repeatedly
        auto read_worker = [&]() {
            try {
                // Wait for run to start
                while (!run_thread_started.load()) {
                    std::this_thread::yield();
                }

                // Read diagnostics repeatedly while run() is (likely) executing
                for (int i = 0; i < 50 && !run_thread_finished.load(); ++i) {
                    if (nb.hasDiagnostics()) {
                        auto stats = nb.getBootstrapStatistics();
                        double mean = nb.getBootstrapMean();
                        double var = nb.getBootstrapVariance();
                        double se = nb.getBootstrapSe();
                        
                        // Verify consistency
                        REQUIRE(std::isfinite(mean));
                        REQUIRE(std::isfinite(var));
                        REQUIRE(std::isfinite(se));
                        REQUIRE(var >= 0.0);
                        REQUIRE(se >= 0.0);
                        REQUIRE(!stats.empty());
                        
                        successful_reads.fetch_add(1);
                    }
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            } catch (...) {
                had_exception.store(true);
            }
        };

        std::thread t1(run_worker);
        std::thread t2(read_worker);

        t1.join();
        t2.join();

        REQUIRE_FALSE(had_exception.load());
        REQUIRE(successful_reads.load() > 0);  // Should have read at least once
    }
}

TEST_CASE("NormalBootstrap: concurrent setChunkSizeHint() during run()", 
          "[Bootstrap][Normal][Concurrency][ChunkHint]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t n = 50;
    std::vector<D> x; 
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    const std::size_t B  = 1000;
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    SECTION("setChunkSizeHint() while run() is executing")
    {
        std::atomic<bool> run_started{false};
        std::atomic<bool> run_finished{false};
        std::atomic<bool> had_exception{false};

        auto run_worker = [&]() {
            try {
                run_started.store(true);
                
                randutils::seed_seq_fe128 seed{11u, 22u, 33u, 44u};
                std::mt19937_64 rng(seed);
                
                auto result = nb.run(x, mean_sampler, rng);
                REQUIRE(result.B == B);
                
                run_finished.store(true);
            } catch (...) {
                had_exception.store(true);
            }
        };

        auto hint_worker = [&]() {
            try {
                while (!run_started.load()) {
                    std::this_thread::yield();
                }

                // Change chunk hint repeatedly while run() is executing
                for (uint32_t hint = 10; hint < 100 && !run_finished.load(); hint += 10) {
                    nb.setChunkSizeHint(hint);
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            } catch (...) {
                had_exception.store(true);
            }
        };

        std::thread t1(run_worker);
        std::thread t2(hint_worker);

        t1.join();
        t2.join();

        REQUIRE_FALSE(had_exception.load());
    }
}

TEST_CASE("NormalBootstrap: stress test with many concurrent operations", 
          "[Bootstrap][Normal][Concurrency][Stress]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t n = 30;
    std::vector<D> x; 
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i % 10)));
    }

    const std::size_t B  = 500;
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    // Initialize with one run
    randutils::seed_seq_fe128 initial_seed{1u, 2u, 3u, 4u};
    std::mt19937_64 initial_rng(initial_seed);
    nb.run(x, mean_sampler, initial_rng);

    SECTION("Multiple threads doing mixed operations")
    {
        const int num_threads = 8;
        const int operations_per_thread = 10;
        std::atomic<int> completed_operations{0};
        std::atomic<bool> had_exception{false};
        std::vector<std::thread> threads;

        auto worker = [&](int thread_id) {
            try {
                for (int op = 0; op < operations_per_thread; ++op) {
                    int operation_type = (thread_id + op) % 4;
                    
                    if (operation_type == 0) {
                        // Call run()
                        randutils::seed_seq_fe128 seed{
                            static_cast<uint32_t>(thread_id * 1000 + op),
                            11u, 22u, 33u
                        };
                        std::mt19937_64 rng(seed);
                        auto result = nb.run(x, mean_sampler, rng);
                        REQUIRE(result.B == B);
                        
                    } else if (operation_type == 1) {
                        // Read diagnostics
                        if (nb.hasDiagnostics()) {
                            auto stats = nb.getBootstrapStatistics();
                            double mean = nb.getBootstrapMean();
                            REQUIRE(std::isfinite(mean));
                            REQUIRE(!stats.empty());
                        }
                        
                    } else if (operation_type == 2) {
                        // Set chunk hint
                        nb.setChunkSizeHint(static_cast<uint32_t>(10 + thread_id));
                        
                    } else {
                        // Check hasDiagnostics
                        bool has = nb.hasDiagnostics();
                        (void)has;  // Suppress unused warning
                    }
                    
                    completed_operations.fetch_add(1);
                    
                    // Small delay to increase overlap
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            } catch (...) {
                had_exception.store(true);
            }
        };

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, i);
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE_FALSE(had_exception.load());
        REQUIRE(completed_operations.load() == num_threads * operations_per_thread);
    }
}

TEST_CASE("NormalBootstrap: verify diagnostic data integrity under concurrent access", 
          "[Bootstrap][Normal][Concurrency][Integrity]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t n = 40;
    std::vector<D> x; 
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    const std::size_t B  = 500;
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    SECTION("Diagnostic consistency across multiple concurrent runs")
    {
        const int num_runs = 5;
        std::vector<std::thread> threads;
        std::atomic<bool> had_exception{false};
        std::mutex results_mutex;
        std::vector<std::pair<double, std::size_t>> results;  // (mean, effective_B)

        auto worker = [&](int thread_id) {
            try {
                randutils::seed_seq_fe128 seed{
                    static_cast<uint32_t>(thread_id * 12345),
                    11u, 22u, 33u
                };
                std::mt19937_64 rng(seed);
                
                auto result = nb.run(x, mean_sampler, rng);
                
                // After run completes, read diagnostics
                if (nb.hasDiagnostics()) {
                    auto stats = nb.getBootstrapStatistics();
                    double mean = nb.getBootstrapMean();
                    double var = nb.getBootstrapVariance();
                    double se = nb.getBootstrapSe();
                    
                    // Verify internal consistency
                    REQUIRE(stats.size() == result.effective_B);
                    REQUIRE(std::isfinite(mean));
                    REQUIRE(std::isfinite(var));
                    REQUIRE(std::isfinite(se));
                    REQUIRE(var >= 0.0);
                    REQUIRE(se == Catch::Approx(std::sqrt(var)).margin(1e-9));
                    
                    // Manually verify mean computation
                    double computed_mean = 0.0;
                    for (double v : stats) {
                        computed_mean += v;
                    }
                    computed_mean /= static_cast<double>(stats.size());
                    REQUIRE(mean == Catch::Approx(computed_mean).margin(1e-9));
                    
                    {
                        std::lock_guard<std::mutex> lock(results_mutex);
                        results.emplace_back(mean, stats.size());
                    }
                }
            } catch (...) {
                had_exception.store(true);
            }
        };

        for (int i = 0; i < num_runs; ++i) {
            threads.emplace_back(worker, i);
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE_FALSE(had_exception.load());
        REQUIRE(results.size() > 0);  // At least some runs completed
        
        // All results should be valid
        for (const auto& [mean, effective_B] : results) {
            REQUIRE(std::isfinite(mean));
            REQUIRE(effective_B > 0);
            REQUIRE(effective_B <= B);
        }
    }
}

TEST_CASE("NormalBootstrap: concurrent access with ThreadPoolExecutor", 
          "[Bootstrap][Normal][Concurrency][ThreadPool]")
{
    // Test that concurrent run() calls work when the bootstrap itself
    // is using a ThreadPoolExecutor internally
    
    const std::size_t n = 50;
    std::vector<double> x;
    x.reserve(n);
    std::mt19937_64 gen_data(12345);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(dist(gen_data));
    }

    const std::size_t B  = 500;
    const double      CL = 0.95;

    IIDResamplerConcurrency resampler{};
    MeanSamplerConcurrency sampler{};

    NormalBootstrapExec<concurrency::ThreadPoolExecutor<4>> nb(B, CL, resampler);

    SECTION("Multiple threads calling run() on ThreadPoolExecutor-based bootstrap")
    {
        const int num_threads = 4;
        std::atomic<int> completed{0};
        std::atomic<bool> had_exception{false};
        std::vector<std::thread> threads;

        auto worker = [&](int thread_id) {
            try {
                std::mt19937_64 rng(static_cast<uint64_t>(thread_id * 9999));
                
                auto result = nb.run(x, sampler, rng);
                
                REQUIRE(result.B == B);
                REQUIRE(result.n == n);
                REQUIRE(std::isfinite(result.mean));
                REQUIRE(result.lower <= result.mean);
                REQUIRE(result.mean <= result.upper);
                
                completed.fetch_add(1);
            } catch (...) {
                had_exception.store(true);
            }
        };

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, i);
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE_FALSE(had_exception.load());
        REQUIRE(completed.load() == num_threads);
    }
}

TEST_CASE("NormalBootstrap: no data races under TSAN", 
          "[Bootstrap][Normal][Concurrency][TSAN]")
{
    // This test is designed to be run with ThreadSanitizer (TSAN)
    // to detect any remaining data races in the implementation.
    // Under TSAN, any data race will cause the test to fail.
    
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t n = 30;
    std::vector<D> x; 
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<int>(i)));
    }

    const std::size_t B  = 400;  // Minimum B to keep test fast
    const double      CL = 0.95;

    NormalBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        nb(B, CL, res);

    // Initialize
    randutils::seed_seq_fe128 initial_seed{1u, 2u, 3u, 4u};
    std::mt19937_64 initial_rng(initial_seed);
    nb.run(x, mean_sampler, initial_rng);

    SECTION("Hammer all operations concurrently")
    {
        const int num_threads = 6;
        const int iterations = 20;
        std::atomic<bool> had_exception{false};
        std::vector<std::thread> threads;

        auto worker = [&](int thread_id) {
            try {
                for (int i = 0; i < iterations; ++i) {
                    switch (i % 5) {
                        case 0: {
                            // run()
                            randutils::seed_seq_fe128 seed{
                                static_cast<uint32_t>(thread_id * 100 + i),
                                11u, 22u, 33u
                            };
                            std::mt19937_64 rng(seed);
                            auto result = nb.run(x, mean_sampler, rng);
                            (void)result;
                            break;
                        }
                        case 1: {
                            // getBootstrapStatistics()
                            if (nb.hasDiagnostics()) {
                                auto stats = nb.getBootstrapStatistics();
                                (void)stats;
                            }
                            break;
                        }
                        case 2: {
                            // getBootstrapMean()
                            if (nb.hasDiagnostics()) {
                                double m = nb.getBootstrapMean();
                                (void)m;
                            }
                            break;
                        }
                        case 3: {
                            // getBootstrapSe()
                            if (nb.hasDiagnostics()) {
                                double se = nb.getBootstrapSe();
                                (void)se;
                            }
                            break;
                        }
                        case 4: {
                            // setChunkSizeHint()
                            nb.setChunkSizeHint(static_cast<uint32_t>(thread_id + 5));
                            break;
                        }
                    }
                }
            } catch (...) {
                had_exception.store(true);
            }
        };

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, i);
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE_FALSE(had_exception.load());
    }
}