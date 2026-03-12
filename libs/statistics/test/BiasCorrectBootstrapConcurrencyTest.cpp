// BCaBootStrapConcurrencyTest.cpp
//
// Unit tests for the Executor policy template parameter of BCaBootStrap.
//
// Each test targets one specific correctness or contract property of the
// parallel implementation introduced by the 6th template parameter:
//
//   template <class Decimal,
//             class Sampler    = IIDResampler<Decimal>,
//             class Rng        = randutils::mt19937_rng,
//             class Provider   = void,
//             class SampleType = Decimal,
//             class Executor   = concurrency::SingleThreadExecutor>  // NEW
//   class BCaBootStrap;
//
// Tests are organized as follows:
//
//  §1  Each executor policy compiles and yields a structurally valid interval.
//  §2  ThreadPoolExecutor<N> for several fixed N values.
//  §3  getMean() is bit-identical across all executors (computed from the
//      original sample before any RNG is touched).
//  §4  CRN (Provider != void) path: bit-identical results across executors
//      because each replicate b always gets make_engine(b) regardless of
//      scheduling order.
//  §5  Degenerate dataset (all-same returns) collapses to a point under
//      parallel execution.
//  §6  BCaAnnualizer accepts BCaBootStrap<..., Executor> and invokes the
//      4th constructor overload added for the Executor parameter.
//  §7  StationaryBlockResampler is correctly copied per-task and produces
//      valid intervals under parallel execution.
//  §8  Efron diagnostics (z0, acceleration, boot_stats) are finite and
//      well-formed under parallel execution.
//  §9  boot_stats has exactly B entries for several B values — validates
//      the adaptive chunkSizeHint arithmetic does not lose or duplicate
//      any bootstrap replicates.
//  §10 getSampleSize() is invariant across executor policies (requires no
//      calculation, should hold before and after compute).
//  §11 CRN path + ThreadPool is order-independent: a permuted replicate
//      schedule produces bit-identical bounds to the identity schedule.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <type_traits>

#include "BiasCorrectedBootstrap.h"
#include "ParallelExecutors.h"
#include "TestUtils.h"
#include "number.h"
#include "randutils.hpp"
#include "RngUtils.h"

using namespace mkc_timeseries;

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

namespace
{
    // Moderate, non-trivial return series: 4 positive days then 1 negative
    // → positive expectation, mild skew.  n = 120.
    static std::vector<DecimalType> buildTestReturns()
    {
        std::vector<DecimalType> x;
        x.reserve(120);
        for (int i = 0; i < 120; ++i)
            x.push_back(i % 5 == 0 ? createDecimal("-0.008") : createDecimal("0.003"));
        return x;
    }

    // Larger series for B-count stress tests.  n = 300.
    static std::vector<DecimalType> buildLargeTestReturns()
    {
        std::vector<DecimalType> x;
        x.reserve(300);
        for (int i = 0; i < 300; ++i)
            x.push_back(i % 5 == 0 ? createDecimal("-0.010") : createDecimal("0.004"));
        return x;
    }

    // Autocorrelated series: clusters of positives and negatives.  n = 200.
    static std::vector<DecimalType> buildClusteredReturns()
    {
        std::vector<DecimalType> x;
        x.reserve(200);
        for (int k = 0; k < 40; ++k)
        {
            x.push_back(createDecimal("0.005"));
            x.push_back(createDecimal("0.005"));
            x.push_back(createDecimal("-0.004"));
            x.push_back(createDecimal("-0.004"));
            x.push_back(createDecimal("0.002"));
        }
        return x;
    }

    // Structural validity: lo <= mu <= hi, all finite.
    template<class BcaT>
    void requireValidInterval(BcaT& bca)
    {
        const auto lo = bca.getLowerBound();
        const auto mu = bca.getMean();
        const auto hi = bca.getUpperBound();

        REQUIRE(std::isfinite(num::to_double(lo)));
        REQUIRE(std::isfinite(num::to_double(mu)));
        REQUIRE(std::isfinite(num::to_double(hi)));
        REQUIRE(lo <= hi);
        REQUIRE(mu >= lo);
        REQUIRE(mu <= hi);
    }

    // boot_stats has exactly B finite entries.
    template<class BcaT>
    void requireValidBootStats(BcaT& bca, unsigned int B)
    {
        const auto& boot = bca.getBootstrapStatistics();
        REQUIRE(boot.size() == static_cast<std::size_t>(B));
        for (const auto& v : boot)
            REQUIRE(std::isfinite(num::to_double(v)));
    }

    // PermutingProvider: wraps a CRN provider and remaps replicate index b
    // to perm[b] before calling make_engine.  Used to simulate parallel
    // out-of-order execution with deterministic per-replicate engines.
    template <class BaseProvider>
    struct PermutingProvider
    {
        using Engine = typename BaseProvider::Engine;

        PermutingProvider(BaseProvider base, std::vector<std::size_t> perm)
            : base_(std::move(base)), perm_(std::move(perm)) {}

        Engine make_engine(std::size_t b) const
        {
            return base_.make_engine(perm_[b]);
        }

        BaseProvider           base_;
        std::vector<std::size_t> perm_;
    };

} // anonymous namespace


// ===========================================================================
// §1 — Every executor policy compiles and yields a structurally valid interval
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §1a: SingleThreadExecutor — valid interval",
          "[BCaBootStrap][Concurrency][SingleThread]")
{
    using D    = DecimalType;
    using Exec = concurrency::SingleThreadExecutor;

    GeoMeanStat<D> stat;
    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(buildTestReturns(), 1000, 0.95, stat);

    requireValidInterval(bca);
    requireValidBootStats(bca, 1000);
}

TEST_CASE("BCaBootStrap[Concurrency] §1b: ThreadPoolExecutor<0> (auto-sized) — valid interval",
          "[BCaBootStrap][Concurrency][ThreadPool]")
{
    using D    = DecimalType;
    using Exec = concurrency::ThreadPoolExecutor<0>;

    GeoMeanStat<D> stat;
    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(buildTestReturns(), 1000, 0.95, stat);

    requireValidInterval(bca);
    requireValidBootStats(bca, 1000);
}

TEST_CASE("BCaBootStrap[Concurrency] §1c: StdAsyncExecutor — valid interval",
          "[BCaBootStrap][Concurrency][StdAsync]")
{
    using D    = DecimalType;
    using Exec = concurrency::StdAsyncExecutor;

    // Lower B: StdAsync may spawn one thread per chunk; keep pressure modest.
    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(buildTestReturns(), 500, 0.95);

    requireValidInterval(bca);
    requireValidBootStats(bca, 500);
}


// ===========================================================================
// §2 — ThreadPoolExecutor<N> for several fixed N values
// ===========================================================================

TEMPLATE_TEST_CASE("BCaBootStrap[Concurrency] §2: ThreadPoolExecutor<N> — valid for multiple N",
                   "[BCaBootStrap][Concurrency][ThreadPool][FixedN]",
                   concurrency::ThreadPoolExecutor<1>,
                   concurrency::ThreadPoolExecutor<2>,
                   concurrency::ThreadPoolExecutor<4>,
                   concurrency::ThreadPoolExecutor<8>)
{
    using D    = DecimalType;
    using Exec = TestType;

    GeoMeanStat<D> stat;
    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(buildTestReturns(), 800, 0.90, stat);

    requireValidInterval(bca);
    requireValidBootStats(bca, 800);
}


// ===========================================================================
// §3 — getMean() is bit-identical across all executor policies
//      getMean() returns stat(original_sample), computed before any RNG is
//      touched — it must never vary with the executor.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §3: getMean() is executor-independent",
          "[BCaBootStrap][Concurrency][MeanInvariance]")
{
    using D = DecimalType;

    const auto returns = buildTestReturns();
    GeoMeanStat<D> stat;

    // Compute the expected mean directly so the test is self-contained.
    const D expected_mean = stat(returns);

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D,
                 concurrency::SingleThreadExecutor>   bca_st(returns, 500, 0.90, stat);

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D,
                 concurrency::ThreadPoolExecutor<0>>  bca_tp(returns, 500, 0.90, stat);

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D,
                 concurrency::StdAsyncExecutor>       bca_as(returns, 500, 0.90, stat);

    // All three must match the analytic value exactly (no floating-point rounding
    // variance — stat(returns) is computed identically on the original sample).
    REQUIRE(bca_st.getMean() == expected_mean);
    REQUIRE(bca_tp.getMean() == expected_mean);
    REQUIRE(bca_as.getMean() == expected_mean);
}


// ===========================================================================
// §4 — CRN (Provider != void) path: bit-identical results across executors
//
//      With a CRN provider, replicate b always uses make_engine(b).  Combined
//      with the indexed write boot_stats[b] = stat_b, both the per-replicate
//      statistics and the derived bounds must be exactly equal regardless of
//      which executor schedules the work.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §4: CRN path gives bit-identical results across executors",
          "[BCaBootStrap][Concurrency][CRN][Determinism]")
{
    using D       = DecimalType;
    using Eng     = randutils::mt19937_rng;
    using Resamp  = IIDResampler<D, Eng>;
    using CRNProv = mkc_timeseries::rng_utils::CRNRng<Eng>;

    const auto returns = buildTestReturns();
    const unsigned B   = 600;
    const double   cl  = 0.95;

    const uint64_t masterSeed = 0xDEADBEEFCAFEBABEull;
    const uint64_t strategyId = 0x0102030405060708ull;
    const uint64_t stageTag   = 42u;

    // Two independent CRN providers with the same key → same per-replicate engines.
    auto make_crn = [&]() {
        return CRNProv(mkc_timeseries::rng_utils::CRNKey(masterSeed)
                       .with_tags({ strategyId, stageTag, 0ull, 0ull }));
    };

    Resamp sampler;
    GeoMeanStat<D> stat;

    // SingleThreadExecutor run
    BCaBootStrap<D, Resamp, Eng, CRNProv, D,
                 concurrency::SingleThreadExecutor>
        bca_st(returns, B, cl, stat, sampler, make_crn());

    // ThreadPoolExecutor<0> run — must be bit-identical
    BCaBootStrap<D, Resamp, Eng, CRNProv, D,
                 concurrency::ThreadPoolExecutor<0>>
        bca_tp(returns, B, cl, stat, sampler, make_crn());

    // Bounds must be bit-identical (CRN + indexed writes guarantee this).
    REQUIRE(num::to_double(bca_st.getLowerBound()) ==
            Catch::Approx(num::to_double(bca_tp.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bca_st.getUpperBound()) ==
            Catch::Approx(num::to_double(bca_tp.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bca_st.getMean()) ==
            Catch::Approx(num::to_double(bca_tp.getMean())).epsilon(0));

    // boot_stats must also be bit-identical (same replicate b → same engine →
    // same resample → same stat_b, always written to index b).
    const auto& boot_st = bca_st.getBootstrapStatistics();
    const auto& boot_tp = bca_tp.getBootstrapStatistics();
    REQUIRE(boot_st.size() == boot_tp.size());
    for (std::size_t i = 0; i < boot_st.size(); ++i)
        REQUIRE(num::to_double(boot_st[i]) ==
                Catch::Approx(num::to_double(boot_tp[i])).epsilon(0));
}


// ===========================================================================
// §5 — Degenerate dataset (all-same returns) collapses to a point under
//      parallel execution.  Tests the early-exit path in calculateBCaBounds()
//      which must fire correctly even when tasks run in parallel.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §5: degenerate dataset collapses under ThreadPoolExecutor",
          "[BCaBootStrap][Concurrency][Degenerate][ThreadPool]")
{
    using D    = DecimalType;
    using Exec = concurrency::ThreadPoolExecutor<0>;

    const D val = createDecimal("0.0123");
    std::vector<D> x(50, val);

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(x, 1000, 0.95);

    const auto lo = bca.getLowerBound();
    const auto mu = bca.getMean();
    const auto hi = bca.getUpperBound();

    REQUIRE(num::to_double(hi - lo) == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(num::to_double(mu - lo) == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(num::to_double(hi - mu) == Catch::Approx(0.0).margin(1e-15));

    // Diagnostics should still be well-formed even on the degenerate path.
    REQUIRE(std::isfinite(bca.getZ0()));
    REQUIRE(std::isfinite(num::to_double(bca.getAcceleration())));
    REQUIRE(bca.getBootstrapStatistics().size() == 1000u);
}


// ===========================================================================
// §6 — BCaAnnualizer works with a non-default Executor, exercising the 4th
//      constructor overload introduced when Executor was added as the 6th
//      template parameter.  All three previous overloads match 1-5 explicit
//      template arguments; this one fires when Executor != SingleThreadExecutor
//      is specified explicitly.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §6: BCaAnnualizer 4th overload with ThreadPoolExecutor",
          "[BCaBootStrap][Concurrency][BCaAnnualizer][ThreadPool]")
{
    using D    = DecimalType;
    using Exec = concurrency::ThreadPoolExecutor<0>;

    GeoMeanStat<D> stat;
    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(buildTestReturns(), 800, 0.95, stat);

    // This must compile and dispatch to the 4th BCaAnnualizer overload.
    BCaAnnualizer<D> ann(bca, 252.0);

    const auto ann_lo = ann.getAnnualizedLowerBound();
    const auto ann_mu = ann.getAnnualizedMean();
    const auto ann_hi = ann.getAnnualizedUpperBound();

    REQUIRE(std::isfinite(num::to_double(ann_lo)));
    REQUIRE(std::isfinite(num::to_double(ann_mu)));
    REQUIRE(std::isfinite(num::to_double(ann_hi)));

    // Annualization must preserve ordering.
    REQUIRE(ann_lo <= ann_mu);
    REQUIRE(ann_mu <= ann_hi);

    // Annualized mean at k=252 must exceed the per-period mean for a positive
    // return series (geometric compounding).
    REQUIRE(num::to_double(ann_mu) > num::to_double(bca.getMean()));
}

TEST_CASE("BCaBootStrap[Concurrency] §6b: BCaAnnualizer 4th overload — annualization factor 1.0 is idempotent",
          "[BCaBootStrap][Concurrency][BCaAnnualizer][ThreadPool]")
{
    using D    = DecimalType;
    using Exec = concurrency::ThreadPoolExecutor<0>;

    GeoMeanStat<D> stat;
    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(buildTestReturns(), 600, 0.95, stat);

    // At k=1 annualization is the identity transformation.
    BCaAnnualizer<D> ann(bca, 1.0);

    REQUIRE(num::to_double(ann.getAnnualizedLowerBound()) ==
            Catch::Approx(num::to_double(bca.getLowerBound())).epsilon(1e-12));
    REQUIRE(num::to_double(ann.getAnnualizedMean()) ==
            Catch::Approx(num::to_double(bca.getMean())).epsilon(1e-12));
    REQUIRE(num::to_double(ann.getAnnualizedUpperBound()) ==
            Catch::Approx(num::to_double(bca.getUpperBound())).epsilon(1e-12));
}


// ===========================================================================
// §7 — StationaryBlockResampler works correctly under parallel execution.
//      The resampler holds mutable state (m_geo distribution); the parallel
//      loop copies it per-task so each task drives its own distribution with
//      its own thread_local RNG — verifies no shared-state corruption.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §7: StationaryBlockResampler with ThreadPoolExecutor",
          "[BCaBootStrap][Concurrency][StationaryBlock][ThreadPool]")
{
    using D      = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    using Exec   = concurrency::ThreadPoolExecutor<0>;

    const unsigned B = 1000;
    const double  cl = 0.95;

    Policy pol(3);
    GeoMeanStat<D> stat;

    BCaBootStrap<D, Policy, randutils::mt19937_rng, void, D, Exec>
        bca(buildClusteredReturns(), B, cl, stat, pol);

    requireValidInterval(bca);
    requireValidBootStats(bca, B);

    // The interval must be non-degenerate for this non-constant dataset.
    REQUIRE(num::to_double(bca.getUpperBound() - bca.getLowerBound()) > 0.0);
}

TEST_CASE("BCaBootStrap[Concurrency] §7b: StationaryBlockResampler — parallel width >= IID width (soft)",
          "[BCaBootStrap][Concurrency][StationaryBlock][ThreadPool]")
{
    // For autocorrelated data, block bootstrap typically produces wider intervals
    // than IID.  This is a soft check (0.5× lower bound) to avoid flakiness while
    // still confirming both code paths run and produce non-degenerate results.
    using D      = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    using Exec   = concurrency::ThreadPoolExecutor<0>;

    const auto returns = buildClusteredReturns();
    GeoMeanStat<D> stat;
    const unsigned B = 1500;
    const double  cl = 0.95;

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca_iid(returns, B, cl, stat);

    Policy pol(3);
    BCaBootStrap<D, Policy, randutils::mt19937_rng, void, D, Exec>
        bca_blk(returns, B, cl, stat, pol);

    const double wid_iid = num::to_double(bca_iid.getUpperBound() - bca_iid.getLowerBound());
    const double wid_blk = num::to_double(bca_blk.getUpperBound() - bca_blk.getLowerBound());

    REQUIRE(wid_iid > 0.0);
    REQUIRE(wid_blk > 0.0);
    REQUIRE(wid_blk >= 0.50 * wid_iid);
}


// ===========================================================================
// §8 — Efron diagnostics (z0, acceleration, boot_stats variance) are finite
//      and well-formed under parallel execution.
//      Mirrors the existing "Efron diagnostics" test but with ThreadPool.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §8: Efron diagnostics are valid under ThreadPoolExecutor",
          "[BCaBootStrap][Concurrency][Diagnostics][ThreadPool]")
{
    using D    = DecimalType;
    using Exec = concurrency::ThreadPoolExecutor<0>;

    // Non-trivial dataset: mild skew so z0 and a are non-trivial.
    std::vector<D> returns = {
        createDecimal("0.01"),  createDecimal("0.02"),  createDecimal("0.015"),
        createDecimal("-0.01"), createDecimal("0.03"),  createDecimal("-0.005"),
        createDecimal("0.025"), createDecimal("0.00"),  createDecimal("-0.02"),
        createDecimal("0.018"), createDecimal("0.011"), createDecimal("0.027")
    };

    const unsigned B = 2000;
    const double  cl = 0.95;

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
        bca(returns, B, cl);

    SECTION("boot_stats: correct size and all finite")
    {
        requireValidBootStats(bca, B);

        // Non-degenerate: the bootstrap distribution should have positive variance.
        const auto& boot = bca.getBootstrapStatistics();
        double mean_b = 0.0;
        for (const auto& v : boot) mean_b += num::to_double(v);
        mean_b /= static_cast<double>(B);

        double var_b = 0.0;
        for (const auto& v : boot)
        {
            const double d = num::to_double(v) - mean_b;
            var_b += d * d;
        }
        var_b /= static_cast<double>(B);
        REQUIRE(var_b > 0.0);
    }

    SECTION("z0 and acceleration: finite and not extreme")
    {
        const double z0 = bca.getZ0();
        const D      a  = bca.getAcceleration();

        REQUIRE(std::isfinite(z0));
        REQUIRE(std::isfinite(num::to_double(a)));

        REQUIRE(std::fabs(z0) < 10.0);
        REQUIRE(std::fabs(num::to_double(a)) < 10.0);
    }

    SECTION("getBootstrapStatistics() is stable across repeated calls")
    {
        const auto& boot1 = bca.getBootstrapStatistics();
        const auto& boot2 = bca.getBootstrapStatistics();
        REQUIRE(boot1.size() == boot2.size());
        for (std::size_t i = 0; i < boot1.size(); ++i)
            REQUIRE(boot1[i] == boot2[i]);
    }
}


// ===========================================================================
// §9 — boot_stats has exactly B entries for several values of B.
//      Validates that the adaptive chunkSizeHint arithmetic and the
//      parallel_for_chunked loop together cover exactly [0, B) with no
//      duplicates or gaps.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §9: boot_stats has exactly B entries for various B",
          "[BCaBootStrap][Concurrency][BootStatsCount][ThreadPool]")
{
    using D    = DecimalType;
    using Exec = concurrency::ThreadPoolExecutor<0>;

    const auto returns = buildLargeTestReturns();

    // Values chosen to exercise different quotient/remainder combinations
    // in the chunk arithmetic and to stress-test boundary cases:
    //   100  : below the 512 auto-clamp (hint overrides this correctly)
    //   512  : exactly the auto-clamp floor
    //   1000 : above the floor, non-power-of-two
    //   2000 : common production value
    //   5000 : 5× hardware_concurrency on most machines
    for (unsigned B : { 100u, 512u, 1000u, 2000u, 5000u })
    {
        BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
            bca(returns, B, 0.95);

        const auto& boot = bca.getBootstrapStatistics();
        INFO("B = " << B << "  boot_stats.size() = " << boot.size());
        REQUIRE(boot.size() == static_cast<std::size_t>(B));
    }
}

TEST_CASE("BCaBootStrap[Concurrency] §9b: boot_stats has exactly B entries under SingleThreadExecutor",
          "[BCaBootStrap][Concurrency][BootStatsCount][SingleThread]")
{
    // SingleThreadExecutor uses chunkSizeHint = B (one chunk for the whole loop).
    // Verify the hint doesn't accidentally truncate or over-run.
    using D    = DecimalType;
    using Exec = concurrency::SingleThreadExecutor;

    const auto returns = buildLargeTestReturns();

    for (unsigned B : { 100u, 512u, 1000u, 5000u })
    {
        BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D, Exec>
            bca(returns, B, 0.95);

        INFO("B = " << B);
        REQUIRE(bca.getBootstrapStatistics().size() == static_cast<std::size_t>(B));
    }
}


// ===========================================================================
// §10 — getSampleSize() is invariant across executor policies.
//       This accessor reads m_returns.size() which is set at construction and
//       never modified — it must be correct and must not require a bootstrap
//       calculation to be triggered.
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §10: getSampleSize() is executor-independent",
          "[BCaBootStrap][Concurrency][SampleSize]")
{
    using D = DecimalType;

    const auto returns    = buildTestReturns();
    const std::size_t n   = returns.size();

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D,
                 concurrency::SingleThreadExecutor>
        bca_st(returns, 500, 0.95);

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D,
                 concurrency::ThreadPoolExecutor<0>>
        bca_tp(returns, 500, 0.95);

    BCaBootStrap<D, IIDResampler<D>, randutils::mt19937_rng, void, D,
                 concurrency::ThreadPoolExecutor<4>>
        bca_t4(returns, 500, 0.95);

    // getSampleSize() must be available before calculation is triggered
    // and must return the correct value.
    REQUIRE(bca_st.getSampleSize() == n);
    REQUIRE(bca_tp.getSampleSize() == n);
    REQUIRE(bca_t4.getSampleSize() == n);

    // Trigger calculation and verify getSampleSize() is still correct afterwards.
    (void)bca_tp.getMean();
    REQUIRE(bca_tp.getSampleSize() == n);
}


// ===========================================================================
// §11 — CRN path + ThreadPool is order-independent.
//       A permuted replicate schedule (make_engine called in a different order)
//       must produce bit-identical bounds to the identity schedule, because
//       each task writes to its own fixed index boot_stats[b] using make_engine(b).
// ===========================================================================

TEST_CASE("BCaBootStrap[Concurrency] §11: CRN + ThreadPool is order-independent (permuted == identity)",
          "[BCaBootStrap][Concurrency][CRN][OrderIndependence][ThreadPool]")
{
    using D       = DecimalType;
    using Eng     = randutils::mt19937_rng;
    using Resamp  = StationaryBlockResampler<D, Eng>;
    using CRNProv = mkc_timeseries::rng_utils::CRNRng<Eng>;
    using PermProv= PermutingProvider<CRNProv>;

    const auto returns = buildClusteredReturns();
    const unsigned B   = 1000;
    const double   cl  = 0.95;
    const unsigned L   = 3;

    const uint64_t masterSeed = 0xBADC0FFEE0DDF00Dull;
    const uint64_t strategyId = 0x1234567890ABCDEFull;
    const uint64_t stageTag   = 2u;

    auto make_crn = [&]() {
        return CRNProv(mkc_timeseries::rng_utils::CRNKey(masterSeed)
                       .with_tags({ strategyId, stageTag,
                                    static_cast<uint64_t>(L), 0ull }));
    };

    // Identity permutation
    std::vector<std::size_t> idperm(B);
    std::iota(idperm.begin(), idperm.end(), 0u);

    // Scrambled permutation: reversed, then rotated — simulates a different
    // chunk scheduling order in the thread pool.
    std::vector<std::size_t> scrperm = idperm;
    std::reverse(scrperm.begin(), scrperm.end());
    std::rotate(scrperm.begin(), scrperm.begin() + 13, scrperm.end());

    PermProv prov_id (make_crn(), idperm);
    PermProv prov_scr(make_crn(), scrperm);

    Resamp sampler(L);

    // Both instances use ThreadPoolExecutor<0> — the only difference is the
    // replicate permutation, which should not affect the final bounds.
    BCaBootStrap<D, Resamp, Eng, PermProv, D, concurrency::ThreadPoolExecutor<0>>
        bca_id(returns, B, cl, &StatUtils<D>::computeMean, sampler, prov_id);

    BCaBootStrap<D, Resamp, Eng, PermProv, D, concurrency::ThreadPoolExecutor<0>>
        bca_scr(returns, B, cl, &StatUtils<D>::computeMean, sampler, prov_scr);

    REQUIRE(num::to_double(bca_id.getLowerBound()) ==
            Catch::Approx(num::to_double(bca_scr.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bca_id.getUpperBound()) ==
            Catch::Approx(num::to_double(bca_scr.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bca_id.getMean()) ==
            Catch::Approx(num::to_double(bca_scr.getMean())).epsilon(0));
}

TEST_CASE("BCaBootStrap[Concurrency] §11b: CRN + SingleThread vs CRN + ThreadPool — identical bounds",
          "[BCaBootStrap][Concurrency][CRN][CrossExecutor]")
{
    // Complements §11 by comparing across executor types rather than across
    // permutations within the same executor type.
    using D       = DecimalType;
    using Eng     = randutils::mt19937_rng;
    using Resamp  = IIDResampler<D, Eng>;
    using CRNProv = mkc_timeseries::rng_utils::CRNRng<Eng>;

    const auto returns = buildTestReturns();
    const unsigned B   = 800;
    const double   cl  = 0.95;

    auto make_crn = [&]() {
        return CRNProv(mkc_timeseries::rng_utils::CRNKey(0xFACEFEEDull)
                       .with_tags({ 0xABCDull, 7ull, 0ull, 0ull }));
    };

    Resamp sampler;
    GeoMeanStat<D> stat;

    BCaBootStrap<D, Resamp, Eng, CRNProv, D,
                 concurrency::SingleThreadExecutor>
        bca_st(returns, B, cl, stat, sampler, make_crn());

    BCaBootStrap<D, Resamp, Eng, CRNProv, D,
                 concurrency::ThreadPoolExecutor<0>>
        bca_tp(returns, B, cl, stat, sampler, make_crn());

    BCaBootStrap<D, Resamp, Eng, CRNProv, D,
                 concurrency::ThreadPoolExecutor<4>>
        bca_t4(returns, B, cl, stat, sampler, make_crn());

    REQUIRE(num::to_double(bca_st.getLowerBound()) ==
            Catch::Approx(num::to_double(bca_tp.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bca_st.getUpperBound()) ==
            Catch::Approx(num::to_double(bca_tp.getUpperBound())).epsilon(0));

    REQUIRE(num::to_double(bca_st.getLowerBound()) ==
            Catch::Approx(num::to_double(bca_t4.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bca_st.getUpperBound()) ==
            Catch::Approx(num::to_double(bca_t4.getUpperBound())).epsilon(0));
}
