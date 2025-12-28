// PercentileTBootstrapTest.cpp
//
// Unit tests for PercentileTBootstrap (studentized) with a composable resampler.
// Place in: libs/statistics/test/
//
// Requires:
//  - Catch2 v3
//  - randutils.hpp
//  - number.h (DecimalType, createDecimal, num::to_double)
//  - StatUtils.h (mkc_timeseries::GeoMeanStat)
//  - StationaryMaskResamplers.h (StationaryMaskValueResampler)
//  - MOutOfNPercentileBootstrap.h (for quantile_type7_sorted dependency if included there)
//  - PercentileTBootstrap.h

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <random>
#include <functional>

#include "number.h"
#include "StatUtils.h"
#include "randutils.hpp"
#include "BiasCorrectedBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "PercentileTBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::PercentileTBootstrap;
using palvalidator::analysis::BCaCompatibleTBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using DecimalType = num::DefaultNumber;

// Round a double to the decimal<8> lattice to match production rounding.
static inline double round_to_decimal8(double x)
{
    return std::round(x * 1e8) / 1e8;
}

// Analytic annualization: (1 + r)^K - 1, computed in long double then cast to double.
static inline double annualize_expect(double r_per_period, long double K)
{
    // Guard tiny domain edges: for r near -1, log1p is stable.
    const long double g = std::log1pl(static_cast<long double>(r_per_period));
    const long double a = std::exp(K * g) - 1.0L;
    return static_cast<double>(a);
}

// ============================================================================
// Test Utilities
// ============================================================================

// Simple mean sampler
struct MeanSampler
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        if (x.empty()) return Decimal(0);
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(num::to_double(v));
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// Degenerate sampler that returns NaN based on data characteristics
// This is deterministic and thread-safe (no mutable state)
struct DegenerateSampler
{
    template <typename Decimal>
    double
    operator()(const std::vector<Decimal>& x) const
    {
        if (x.empty()) return 0.0;  // ✓ FIXED - returns double
        
        // Compute mean
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(num::to_double(v));
        const double mean = static_cast<double>(sum / static_cast<long double>(x.size()));
        
        // Use the sum of elements modulo operation to create variability
        // This ensures different resamples have different failure patterns
        long double int_sum = 0.0L;
        for (auto& v : x) int_sum += std::floor(static_cast<long double>(num::to_double(v)));
        const int sum_mod = static_cast<int>(int_sum) % 10;
        
        // Fail if sum_mod is 0, 1 (20% of cases for uniformly distributed data)
        if (sum_mod <= 1) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        
        // Otherwise return the computed mean
        return mean;  // ✓ FIXED - returns double directly
    }
};

// IID resampler for basic tests
struct IIDResamplerForTest
{
    std::size_t getL() const noexcept { return 0; }

    template <typename Decimal, typename Rng>
    void operator()(const std::vector<Decimal>& src,
                    std::vector<Decimal>&       dst,
                    std::size_t                 m,
                    Rng&                        rng) const
    {
        std::uniform_int_distribution<std::size_t> pick(0, src.size() - 1);
        dst.resize(m);
        for (std::size_t i = 0; i < m; ++i)
        {
            dst[i] = src[pick(rng)];
        }
    }
};

// Mock engine provider for CRN testing
struct MockEngineProvider
{
    mutable std::vector<std::size_t> called_indices;
    
    std::mt19937_64 make_engine(std::size_t b) const
    {
        called_indices.push_back(b);
        // Create deterministic engine based on index
        std::seed_seq seq{static_cast<uint32_t>(b), 
                          static_cast<uint32_t>(b >> 32),
                          static_cast<uint32_t>(b * 7919)};
        return std::mt19937_64(seq);
    }
};

// Convenience alias for our test instantiations
template <typename Exec>
using PctT = PercentileTBootstrap<
    double,                // Decimal
    MeanSampler,           // Sampler
    IIDResamplerForTest,   // Resampler
    std::mt19937_64,       // Rng
    Exec                   // Executor (template parameter on the class)
>;


TEST_CASE("PercentileTBootstrap: constructor validation", "[Bootstrap][PercentileT]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    // B_outer < 400
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(399, 150, 0.95, res)),
                      std::invalid_argument);

    // B_inner < 100
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(500, 99, 0.95, res)),
                      std::invalid_argument);

    // CL out of range
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(500, 150, 0.5, res)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(500, 150, 1.0, res)),
                      std::invalid_argument);

    // m_ratio bounds (outer/inner)
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(500, 150, 0.95, res, 0.0, 1.0)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(500, 150, 0.95, res, 1.1, 1.0)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(500, 150, 0.95, res, 1.0, 0.0)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((PercentileTBootstrap<D,
                       std::function<D(const std::vector<D>&)>,
                       StationaryMaskValueResampler<D>>(500, 150, 0.95, res, 1.0, 1.1)),
                      std::invalid_argument);
}

TEST_CASE("PercentileTBootstrap: run() input validation", "[Bootstrap][PercentileT]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    std::vector<D> tiny{ D(1), D(2) };
    randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
    std::mt19937_64 rng(seed);

    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> pt(500, 120, 0.95, res);
    REQUIRE_THROWS_AS(pt.run(tiny, mean_sampler, rng), std::invalid_argument);
}

TEST_CASE("PercentileTBootstrap: basic behavior with mean sampler (small-n)", "[Bootstrap][PercentileT][Mean][SmallN]")
{
    using D = DecimalType;

    // Small-n series (n=20), simple linear data 0..19 for deterministic baseline mean
    const std::size_t n = 20;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i)));

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    StationaryMaskValueResampler<D> res(/*L=*/3);
    randutils::seed_seq_fe128 seed{11u,22u,33u,44u};
    std::mt19937_64 rng(seed);

    // Keep runtime reasonable: B_outer=500, B_inner=150
    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(/*B_outer=*/500, /*B_inner=*/150, /*CL=*/0.95, res,
           /*m_ratio_outer=*/1.0, /*m_ratio_inner=*/1.0);

    auto out = pt.run(x, mean_sampler, rng);

    SECTION("Invariants and finiteness")
    {
        REQUIRE(out.B_outer == 500);
        REQUIRE(out.B_inner == 150);
        REQUIRE(out.n == n);
        REQUIRE(out.m_outer >= 2);
        REQUIRE(out.m_outer <= n);
        REQUIRE(out.m_inner >= 2);
        REQUIRE(out.m_inner <= out.m_outer);
        REQUIRE(out.effective_B + out.skipped_outer == out.B_outer);

        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));

        REQUIRE(out.lower <= out.mean);
        REQUIRE(out.mean  <= out.upper);

        REQUIRE(out.se_hat >= 0.0); // may be 0 in pathological deterministic cases, but typically > 0 here
    }

    SECTION("m overrides are respected")
    {
        randutils::seed_seq_fe128 seed2{11u,22u,33u,44u};
        std::mt19937_64 rng2(seed2);
        const std::size_t m_outer = 18; // <= n
        const std::size_t m_inner = 10; // <= m_outer
        auto out2 = pt.run(x, mean_sampler, rng2, m_outer, m_inner);
        REQUIRE(out2.m_outer == m_outer);
        REQUIRE(out2.m_inner == m_inner);
    }

    SECTION("Higher CL widens the interval (90% vs 95%)")
    {
        randutils::seed_seq_fe128 seedA{11u,22u,33u,44u};
        randutils::seed_seq_fe128 seedB{11u,22u,33u,44u};
        std::mt19937_64 rngA(seedA), rngB(seedB);
        PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pt90(500, 150, 0.90, res),
            pt95(500, 150, 0.95, res);

        auto r90 = pt90.run(x, mean_sampler, rngA);
        auto r95 = pt95.run(x, mean_sampler, rngB);

        const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
        const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);
        REQUIRE(w95 >= w90 - 1e-12);
    }

    SECTION("Changing inner m influences CI but remains stable and finite (no monotonic guarantee)")
      {
	using PT = PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>;

	const std::size_t Bouter = 500;
	const std::size_t Binner = 150;

	PT pt_m1(Bouter, Binner, 0.95, res, /*m_ratio_outer=*/1.0, /*m_ratio_inner=*/0.9);
	PT pt_m2(Bouter, Binner, 0.95, res, /*m_ratio_outer=*/1.0, /*m_ratio_inner=*/0.6);

	const int K = 10; // a few seeds for stability
	std::vector<double> widths1, widths2;
	widths1.reserve(K);
	widths2.reserve(K);

	for (int k = 0; k < K; ++k)
	  {
	    randutils::seed_seq_fe128 sA{11u,22u,33u,44u, static_cast<uint32_t>(k)};
	    randutils::seed_seq_fe128 sB{11u,22u,33u,44u, static_cast<uint32_t>(k)};
	    std::mt19937_64 rngA(sA), rngB(sB);

	    const auto a = pt_m1.run(x, mean_sampler, rngA);
	    const auto b = pt_m2.run(x, mean_sampler, rngB);

	    const double w1 = num::to_double(a.upper) - num::to_double(a.lower);
	    const double w2 = num::to_double(b.upper) - num::to_double(b.lower);

	    REQUIRE(std::isfinite(w1));
	    REQUIRE(std::isfinite(w2));
	    REQUIRE(w1 > 0.0);
	    REQUIRE(w2 > 0.0);
	    
	    widths1.push_back(w1);
	    widths2.push_back(w2);
	  }

    // Check that the two settings produce meaningfully different distributions of widths
	auto mean = [](const std::vector<double>& v){
	  double s=0; for(double x: v) s+=x; return s / std::max<std::size_t>(1, v.size());
	};
	const double avg1 = mean(widths1);
	const double avg2 = mean(widths2);

	// They should not be virtually identical across seeds (tolerance much smaller than the means)
	REQUIRE(std::fabs(avg1 - avg2) > 1e-6);
      }
}

TEST_CASE("PercentileTBootstrap: GeoMeanStat sampler (small- and moderate-n)", "[Bootstrap][PercentileT][GeoMean]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;

    // Small-n returns around a few bps with occasional mild negatives (> -1)
    const std::size_t n_small = 20;
    std::vector<D> r_small; r_small.reserve(n_small);
    const double pattern_small[] = { 0.0020, -0.0010, 0.0005, 0.0030, -0.0008 };
    for (std::size_t i = 0; i < n_small; ++i) r_small.emplace_back(D(pattern_small[i % 5]));

    // Moderate-n with gentle oscillation
    const std::size_t n_mod = 60;
    std::vector<D> r_mod; r_mod.reserve(n_mod);
    for (std::size_t i = 0; i < n_mod; ++i)
    {
        const double v = 0.0004 + 0.0003 * std::sin(static_cast<double>(i) / 6.0)
                         - 0.0002 * ((i % 7) == 0 ? 1.0 : 0.0);
        r_mod.emplace_back(D(v));
    }

    GeoMeanStat<D> geo(/*clip_ruin=*/true,
                       /*winsor_small_n=*/true,
                       /*winsor_alpha=*/0.02,
                       /*ruin_eps=*/1e-8);

    auto sampler = [&](const std::vector<D>& a) -> D { return geo(a); };

    StationaryMaskValueResampler<D> res_small(/*L=*/3);
    StationaryMaskValueResampler<D> res_mod  (/*L=*/4);

    randutils::seed_seq_fe128 seedA{2025u,10u,30u,1u};
    randutils::seed_seq_fe128 seedB{77u,88u,99u,11u};
    std::mt19937_64 rng_small(seedA), rng_mod(seedB);

    // Keep costs modest, but within guidelines
    PercentileTBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>>
        pt_small(/*B_outer=*/500, /*B_inner=*/150, /*CL=*/0.95, res_small,
                 /*m_ratio_outer=*/1.0, /*m_ratio_inner=*/1.0);

    PercentileTBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>>
        pt_mod  (/*B_outer=*/600, /*B_inner=*/150, /*CL=*/0.95, res_mod,
                 /*m_ratio_outer=*/1.0, /*m_ratio_inner=*/1.0);

    auto out_small = pt_small.run(r_small, sampler, rng_small);
    auto out_mod   = pt_mod.run  (r_mod,   sampler, rng_mod);

    SECTION("Small-n and moderate-n: finite, ordered, and effective outer reps")
    {
        // small-n
        REQUIRE(std::isfinite(num::to_double(out_small.lower)));
        REQUIRE(std::isfinite(num::to_double(out_small.mean)));
        REQUIRE(std::isfinite(num::to_double(out_small.upper)));
        REQUIRE(out_small.lower <= out_small.mean);
        REQUIRE(out_small.mean  <= out_small.upper);
        REQUIRE(out_small.effective_B >= out_small.B_outer / 2);

        // moderate-n
        REQUIRE(std::isfinite(num::to_double(out_mod.lower)));
        REQUIRE(std::isfinite(num::to_double(out_mod.mean)));
        REQUIRE(std::isfinite(num::to_double(out_mod.upper)));
        REQUIRE(out_mod.lower <= out_mod.mean);
        REQUIRE(out_mod.mean  <= out_mod.upper);
        REQUIRE(out_mod.effective_B >= out_mod.B_outer / 2);
    }

    SECTION("Confidence widening for GeoMeanStat (90% vs 95%)")
    {
        randutils::seed_seq_fe128 seedA_copy{2025u,10u,30u,1u};
        randutils::seed_seq_fe128 seedB_copy{2025u,10u,30u,1u};
        std::mt19937_64 rngA(seedA_copy), rngB(seedB_copy);

        PercentileTBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>>
            pt90(/*B_outer=*/500, /*B_inner=*/150, /*CL=*/0.90, res_small),
            pt95(/*B_outer=*/500, /*B_inner=*/150, /*CL=*/0.95, res_small);

        auto r90 = pt90.run(r_small, sampler, rngA);
        auto r95 = pt95.run(r_small, sampler, rngB);

        const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
        const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);
        REQUIRE(w95 >= w90 - 1e-12);
    }
}

TEST_CASE("PercentileT + GeoMean → BCaAnnualizer: ordering, finiteness, and analytic match",
          "[Bootstrap][PercentileT][GeoMean][Annualizer]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;

    // Build a small-n, realistic per-period returns vector (all > -1)
    const std::size_t n = 30;
    std::vector<D> r; r.reserve(n);
    // Mildly positive edge with occasional small negatives
    const double pattern[] = { 0.0012, 0.0008, -0.0005, 0.0015, 0.0003,
                               0.0010, -0.0007, 0.0011, 0.0009, 0.0004 };
    for (std::size_t i = 0; i < n; ++i)
    {
        r.emplace_back(D(pattern[i % (sizeof(pattern)/sizeof(pattern[0]))]));
    }

    // GeoMeanStat with your usual conservative switches
    GeoMeanStat<D> geo(/*clip_ruin=*/true,
                       /*winsor_small_n=*/true,
                       /*winsor_alpha=*/0.02,
                       /*ruin_eps=*/1e-8);

    auto sampler = [&](const std::vector<D>& a) -> D
    {
        return geo(a);
    };

    // Stationary resampler and studentized bootstrap
    StationaryMaskValueResampler<D> res(/*L=*/3);

    randutils::seed_seq_fe128 seed{2025u, 10u, 30u, 2u};
    std::mt19937_64 rng(seed);

    // Keep costs moderate; small-n so studentization is affordable
    PercentileTBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>>
        pt(/*B_outer=*/600, /*B_inner=*/150, /*CL=*/0.95, res,
           /*m_ratio_outer=*/1.0, /*m_ratio_inner=*/1.0);

    const auto out = pt.run(r, sampler, rng);

    // Sanity on per-period CI
    REQUIRE(std::isfinite(num::to_double(out.lower)));
    REQUIRE(std::isfinite(num::to_double(out.mean)));
    REQUIRE(std::isfinite(num::to_double(out.upper)));
    REQUIRE(out.lower <= out.mean);
    REQUIRE(out.mean  <= out.upper);

    // Choose an annualization factor K (e.g., daily bars → ~252/year)
    const long double K = 252.0L;

    // Create a mock BCaBootStrap object to pass to BCaAnnualizer

    class MockBCaBootStrap
      : public mkc_timeseries::BCaBootStrap<D, mkc_timeseries::StationaryBlockResampler<D>>
    {
      using Base = mkc_timeseries::BCaBootStrap<D, mkc_timeseries::StationaryBlockResampler<D>>;

    public:
      MockBCaBootStrap(const D& lower, const D& mean, const D& upper)
	// Give the base at least 2 points so even accidental compute paths are safe.
	: Base(/*returns*/ std::vector<D>{ D(0), D(0) },
	       /*num_resamples*/ 100,
	       /*CL*/ 0.95,
	       /*statistic*/ [](const std::vector<D>& v) { return v[0]; },
	       /*sampler*/ mkc_timeseries::StationaryBlockResampler<D>(3))
      {
        // Inject the values we want the annualizer to see.
        this->setLowerBound(lower);
        this->setMean(mean);
        this->setUpperBound(upper);

        // Mark as calculated so ensureCalculated() won’t call calculateBCaBounds().
        this->m_is_calculated = true;
      }

    protected:
      // Double-safety: if anything ever calls calculateBCaBounds(), make it a no-op.
      void calculateBCaBounds() override
      {
        this->m_is_calculated = true; // nothing else; keep our injected bounds
      }
    };
 
    MockBCaBootStrap mock_bca(out.lower, out.mean, out.upper);
    mkc_timeseries::BCaAnnualizer<D> ann(mock_bca, static_cast<double>(K));

    const double lo_ann = num::to_double(ann.getAnnualizedLowerBound());
    const double mu_ann = num::to_double(ann.getAnnualizedMean());
    const double hi_ann = num::to_double(ann.getAnnualizedUpperBound());

    SECTION("Annualized outputs are finite, ordered, and > -1")
    {
        REQUIRE(std::isfinite(lo_ann));
        REQUIRE(std::isfinite(mu_ann));
        REQUIRE(std::isfinite(hi_ann));

        REQUIRE(lo_ann <= mu_ann);
        REQUIRE(mu_ann <= hi_ann);

        // (1+r)^K - 1 >= -1 with equality only at r=-1; our inputs are > -1, so > -1 strictly.
        REQUIRE(lo_ann > -1.0);
    }

    SECTION("Annualizer matches analytic transform (rounded to decimal<8>)")
    {
        const double lo_exp = round_to_decimal8(annualize_expect(num::to_double(out.lower), K));
        const double mu_exp = round_to_decimal8(annualize_expect(num::to_double(out.mean ), K));
        const double hi_exp = round_to_decimal8(annualize_expect(num::to_double(out.upper), K));

        REQUIRE(lo_ann == Catch::Approx(lo_exp).margin(1e-12));
        REQUIRE(mu_ann == Catch::Approx(mu_exp).margin(1e-12));
        REQUIRE(hi_ann == Catch::Approx(hi_exp).margin(1e-12));
    }

    SECTION("Larger K (e.g., 504) weakly increases annualized mean for small positive returns")
    {
        MockBCaBootStrap mock_bca_252(out.lower, out.mean, out.upper);
        MockBCaBootStrap mock_bca_504(out.lower, out.mean, out.upper);
        mkc_timeseries::BCaAnnualizer<D> ann_252(mock_bca_252, 252.0);
        mkc_timeseries::BCaAnnualizer<D> ann_504(mock_bca_504, 504.0);

        const double m252 = num::to_double(ann_252.getAnnualizedMean());
        const double m504 = num::to_double(ann_504.getAnnualizedMean());

        // For small positive per-period GeoMean, higher compounding frequency yields >= annualized mean.
        REQUIRE(m504 >= m252 - 1e-12);
    }
}

TEST_CASE("PercentileTBootstrap runs correctly with ThreadPoolExecutor", "[bootstrap][threadpool]")
{
    // Synthetic data: mildly non-Gaussian to exercise studentization
    std::mt19937_64 gen_data(12345);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> x; x.reserve(1000);
    for (int i = 0; i < 1000; ++i)
    {
        double v = g(gen_data);
        if ((i % 25) == 0) v *= 1.5;     // small heteroskedastic bumps
        x.push_back(v);
    }

    const double CL = 0.95;

    // Choose modest bootstrap sizes to keep the test fast but meaningful
    const std::size_t B_outer = 500;
    const std::size_t B_inner = 160;

    // Resampler + sampler
    IIDResamplerForTest resampler{};
    MeanSampler         sampler{};

    // Construct single-threaded and thread-pooled variants
    PctT<concurrency::SingleThreadExecutor>   pct_single(B_outer, B_inner, CL, resampler, 0.6, 0.5);
    PctT<concurrency::ThreadPoolExecutor<4>>  pct_pool  (B_outer, B_inner, CL, resampler, 0.6, 0.5);

    // IMPORTANT: use identical RNG seeds for deterministic equivalence
    std::mt19937_64 rng1(0xBEEFu);
    std::mt19937_64 rng2(0xBEEFu);

    auto R1 = pct_single.run(x, sampler, rng1);
    auto R2 = pct_pool.run  (x, sampler, rng2);

    // Basic invariants
    REQUIRE(R1.n == R2.n);
    REQUIRE(R1.B_outer == R2.B_outer);
    REQUIRE(R1.B_inner == R2.B_inner);
    REQUIRE(R1.effective_B > 16);
    REQUIRE(R2.effective_B == R1.effective_B);

    // Numeric equivalence (parallel outer should be bit-stable here; allow tiny tolerance)
    auto near = [](double a, double b, double tol) {
        return std::fabs(a - b) <= tol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    };

    const double tight = 1e-12;

    // Compare core outputs
    REQUIRE( near(static_cast<double>(R1.mean),  static_cast<double>(R2.mean),  tight) );
    REQUIRE( near(static_cast<double>(R1.lower), static_cast<double>(R2.lower), tight) );
    REQUIRE( near(static_cast<double>(R1.upper), static_cast<double>(R2.upper), tight) );
    REQUIRE( near(R1.se_hat, R2.se_hat, tight) );

    // Sanity on skipped diagnostics (exact match expected under identical RNG usage)
    REQUIRE(R1.skipped_outer       == R2.skipped_outer);
    REQUIRE(R1.skipped_inner_total == R2.skipped_inner_total);

    // CI ordering & coverage sanity
    REQUIRE(static_cast<double>(R1.lower) <= static_cast<double>(R1.upper));
    REQUIRE(static_cast<double>(R2.lower) <= static_cast<double>(R2.upper));
}

TEST_CASE("PercentileTBootstrap: diagnostics unavailable before run",
          "[Bootstrap][PercentileT][Diagnostics]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B_outer = 500;
    const std::size_t B_inner = 150;
    const double      CL      = 0.95;

    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(B_outer, B_inner, CL, res);

    SECTION("hasDiagnostics is false before any run()")
    {
        REQUIRE_FALSE(pt.hasDiagnostics());
    }

    SECTION("Diagnostic getters throw before run()")
    {
        REQUIRE_THROWS_AS(pt.getTStatistics(),         std::logic_error);
        REQUIRE_THROWS_AS(pt.getThetaStarStatistics(), std::logic_error);
        REQUIRE_THROWS_AS(pt.getSeHat(),               std::logic_error);
    }
}

TEST_CASE("PercentileTBootstrap: diagnostics consistent with Result",
          "[Bootstrap][PercentileT][Diagnostics]")
{
    using D = DecimalType;

    // Simple nontrivial data: 0..19
    const std::size_t n = 20;
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

    const std::size_t B_outer = 500;
    const std::size_t B_inner = 150;
    const double      CL      = 0.95;

    randutils::seed_seq_fe128 seed{11u,22u,33u,44u};
    std::mt19937_64 rng(seed);

    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(B_outer, B_inner, CL, res);

    auto out = pt.run(x, mean_sampler, rng);

    REQUIRE(pt.hasDiagnostics());

    const auto& tvals   = pt.getTStatistics();
    const auto& thetas  = pt.getThetaStarStatistics();
    const double se_hat = pt.getSeHat();

    SECTION("Diagnostics sizes match effective_B")
    {
        REQUIRE(tvals.size()  == out.effective_B);
        REQUIRE(thetas.size() == out.effective_B);
        REQUIRE(out.effective_B + out.skipped_outer == out.B_outer);
    }
    
    SECTION("se_hat matches recomputation from theta* statistics")
    {
        REQUIRE_FALSE(thetas.empty());

        // Use the same StatUtils function that PercentileTBootstrap now uses
        const double se = mkc_timeseries::StatUtils<double>::computeStdDev(thetas);

        REQUIRE(se_hat      == Catch::Approx(se).margin(1e-12));
        REQUIRE(out.se_hat  == Catch::Approx(se).margin(1e-12));
    }
    
    SECTION("t-statistics are finite and non-degenerate")
    {
        REQUIRE_FALSE(tvals.empty());

        bool any_nonzero = false;
        for (double t : tvals) {
            REQUIRE(std::isfinite(t));
            if (std::fabs(t) > 1e-15) {
                any_nonzero = true;
            }
        }
        // For this nontrivial dataset we expect at least some nonzero t-values
        REQUIRE(any_nonzero);
    }
}

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("PercentileTBootstrap: Provider-based run() overload (CRN path)", 
          "[Bootstrap][PercentileT][Provider][CRN]")
{
    using D = DecimalType;
    
    // Create simple test data
    const std::size_t n = 30;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<double>(i) * 0.5));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    const std::size_t B_outer = 400;
    const std::size_t B_inner = 100;
    
    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(B_outer, B_inner, 0.95, res);
    
    MockEngineProvider provider;
    
    SECTION("Provider run() executes successfully")
    {
        auto out = pt.run(x, mean_sampler, provider);
        
        REQUIRE(out.B_outer == B_outer);
        REQUIRE(out.B_inner == B_inner);
        REQUIRE(out.n == n);
        REQUIRE(out.effective_B > 0);
        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));
        REQUIRE(out.lower <= out.upper);
        
        // Verify provider was called
        REQUIRE(!provider.called_indices.empty());
        REQUIRE(provider.called_indices.size() == B_outer);
    }
    
    SECTION("Provider run() is deterministic with same provider")
    {
        MockEngineProvider provider1;
        MockEngineProvider provider2;
        
        auto out1 = pt.run(x, mean_sampler, provider1);
        auto out2 = pt.run(x, mean_sampler, provider2);
        
        // Results should be identical
        REQUIRE(num::to_double(out1.mean)  == num::to_double(out2.mean));
        REQUIRE(num::to_double(out1.lower) == num::to_double(out2.lower));
        REQUIRE(num::to_double(out1.upper) == num::to_double(out2.upper));
        REQUIRE(out1.effective_B == out2.effective_B);
        REQUIRE(out1.se_hat == out2.se_hat);
    }
}

TEST_CASE("PercentileTBootstrap: Insufficient effective replicates throws",
          "[Bootstrap][PercentileT][Error]")
{
    using D = DecimalType;
    
    // Create data that will produce many degenerate samples
    // Using constant data should cause many zero SE* values
    std::vector<D> constant_data(100, D(5.0));
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    // Use very small subsample size to increase degeneracy
    StationaryMaskValueResampler<D> res(1);
    
    const std::size_t B_outer = 400;
    const std::size_t B_inner = 100;
    
    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(B_outer, B_inner, 0.95, res, 0.05, 0.05);  // Very small m_ratio
    
    randutils::seed_seq_fe128 seed{99u, 88u, 77u, 66u};
    std::mt19937_64 rng(seed);
    
    SECTION("Throws runtime_error when effective_B < minimum")
    {
        REQUIRE_THROWS_AS(pt.run(constant_data, mean_sampler, rng), 
                          std::runtime_error);
    }
    
    SECTION("Error message contains expected information")
    {
        try {
            pt.run(constant_data, mean_sampler, rng);
            REQUIRE(false);  // Should not reach here
        } catch (const std::runtime_error& e) {
            std::string msg(e.what());
            REQUIRE(msg.find("insufficient valid outer replicates") != std::string::npos);
            REQUIRE(msg.find("minimum required") != std::string::npos);
        }
    }
}

TEST_CASE("PercentileTBootstrap: m_ratio parameters affect subsample sizes",
          "[Bootstrap][PercentileT][MRatio]")
{
    using D = DecimalType;
    
    const std::size_t n = 100;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<double>(i) * 0.1));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    randutils::seed_seq_fe128 seed{123u, 456u};
    std::mt19937_64 rng(seed);
    
    SECTION("m_ratio_outer=0.5 produces m_outer ≈ n/2")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pt(400, 100, 0.95, res, 0.5, 1.0);
        
        auto out = pt.run(x, mean_sampler, rng);
        
        // Should be approximately half of n
        REQUIRE(out.m_outer >= n / 2 - 2);
        REQUIRE(out.m_outer <= n / 2 + 2);
    }
    
    SECTION("m_ratio_inner=0.3 produces m_inner ≈ 0.3 * m_outer")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pt(400, 100, 0.95, res, 1.0, 0.3);
        
        auto out = pt.run(x, mean_sampler, rng);
        
        // m_inner should be roughly 30% of m_outer
        const std::size_t expected_m_inner = static_cast<std::size_t>(out.m_outer * 0.3);
        REQUIRE(out.m_inner >= expected_m_inner - 2);
        REQUIRE(out.m_inner <= expected_m_inner + 2);
    }
    
    SECTION("Combined ratios work correctly")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pt(400, 100, 0.95, res, 0.6, 0.5);
        
        auto out = pt.run(x, mean_sampler, rng);
        
        const std::size_t expected_m_outer = static_cast<std::size_t>(n * 0.6);
        const std::size_t expected_m_inner = static_cast<std::size_t>(out.m_outer * 0.5);
        
        REQUIRE(out.m_outer >= expected_m_outer - 2);
        REQUIRE(out.m_outer <= expected_m_outer + 2);
        REQUIRE(out.m_inner >= expected_m_inner - 2);
        REQUIRE(out.m_inner <= expected_m_inner + 2);
    }
}

TEST_CASE("PercentileTBootstrap: Handles degenerate sampler outputs",
          "[Bootstrap][PercentileT][Degenerate]")
{
    // Use double as the Decimal type for this test since we need to preserve NaN values.
    // The decimal<8> fixed-point type cannot represent NaN (it truncates to a finite value).
    using D = double;
    
    const std::size_t n = 50;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(static_cast<double>(i));
    }
    
    DegenerateSampler sampler;  // Returns NaN frequently after first call
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B_outer = 500;
    const std::size_t B_inner = 150;
    
    PercentileTBootstrap<D, DegenerateSampler, StationaryMaskValueResampler<D>>
        pt(B_outer, B_inner, 0.95, res);
    
    randutils::seed_seq_fe128 seed{555u, 666u};
    std::mt19937_64 rng(seed);
    
    SECTION("Gracefully skips non-finite samples")
    {
        auto out = pt.run(x, sampler, rng);
        
        // With 20% NaN rate, we should get some skipped outer replicates
        // But this is probabilistic, so be conservative
        // Note: skipped_outer happens when theta_star is NaN OR when inner loop fails
        // skipped_inner happens when individual inner theta values are NaN
        
        // We should definitely have some skipped inner replicates due to NaN
        REQUIRE(out.skipped_inner_total > 0);
        
        // Outer skips are less certain but should happen sometimes
        // With 20% failure rate and 500 outer, expect ~100 outer failures
        // But some outer might succeed even with inner failures if MIN_INNER is met
        // So let's just verify the counts add up correctly
        REQUIRE(out.effective_B + out.skipped_outer == B_outer);
        
        // Should still have enough valid results (at least 4% = 20 out of 500)
        REQUIRE(out.effective_B >= 20);
        REQUIRE(out.effective_B < B_outer);  // But not all should be valid
        
        // Final outputs should be finite
        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));
    }
    
    SECTION("Diagnostic counts are consistent")
    {
        auto out = pt.run(x, sampler, rng);
        
        REQUIRE(out.effective_B + out.skipped_outer == B_outer);
        REQUIRE(out.inner_attempted_total >= out.effective_B * 100);  // At least MIN_INNER per effective
    }
}

TEST_CASE("PercentileTBootstrap: Multiple consecutive runs update diagnostics",
          "[Bootstrap][PercentileT][Diagnostics]")
{
    using D = DecimalType;
    
    const std::size_t n = 25;
    std::vector<D> x1, x2;
    x1.reserve(n);
    x2.reserve(n);
    
    for (std::size_t i = 0; i < n; ++i) {
        x1.emplace_back(D(static_cast<double>(i)));
        x2.emplace_back(D(static_cast<double>(i) * 2.0));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B_outer = 400;
    const std::size_t B_inner = 120;
    
    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(B_outer, B_inner, 0.95, res);
    
    randutils::seed_seq_fe128 seed1{111u, 222u};
    randutils::seed_seq_fe128 seed2{333u, 444u};
    std::mt19937_64 rng1(seed1);
    std::mt19937_64 rng2(seed2);
    
    SECTION("Diagnostics are updated after each run")
    {
        REQUIRE_FALSE(pt.hasDiagnostics());
        
        auto out1 = pt.run(x1, mean_sampler, rng1);
        REQUIRE(pt.hasDiagnostics());
        
        const auto tvals1 = pt.getTStatistics();
        const auto thetas1 = pt.getThetaStarStatistics();
        const double se1 = pt.getSeHat();
        
        REQUIRE(tvals1.size() == out1.effective_B);
        REQUIRE(thetas1.size() == out1.effective_B);
        REQUIRE(se1 == out1.se_hat);
        
        // Run again with different data
        auto out2 = pt.run(x2, mean_sampler, rng2);
        REQUIRE(pt.hasDiagnostics());
        
        const auto tvals2 = pt.getTStatistics();
        const auto thetas2 = pt.getThetaStarStatistics();
        const double se2 = pt.getSeHat();
        
        REQUIRE(tvals2.size() == out2.effective_B);
        REQUIRE(thetas2.size() == out2.effective_B);
        REQUIRE(se2 == out2.se_hat);
        
        // Diagnostics should have changed
        REQUIRE(se1 != se2);  // Different data should produce different se_hat
    }
}

TEST_CASE("PercentileTBootstrap: Different confidence levels",
          "[Bootstrap][PercentileT][ConfidenceLevel]")
{
    using D = DecimalType;
    
    const std::size_t n = 40;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<double>(i) * 0.2));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    const std::size_t B_outer = 500;
    const std::size_t B_inner = 150;
    
    SECTION("90% confidence produces wider intervals than 80%")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pt80(B_outer, B_inner, 0.80, res);
        
        PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pt90(B_outer, B_inner, 0.90, res);
        
        randutils::seed_seq_fe128 seed1{777u};
        randutils::seed_seq_fe128 seed2{777u};
        std::mt19937_64 rng1(seed1);
        std::mt19937_64 rng2(seed2);
        
        auto out80 = pt80.run(x, mean_sampler, rng1);
        auto out90 = pt90.run(x, mean_sampler, rng2);
        
        const double width80 = num::to_double(out80.upper - out80.lower);
        const double width90 = num::to_double(out90.upper - out90.lower);
        
        // Higher confidence level should produce wider interval
        REQUIRE(width90 > width80);
        
        REQUIRE(out80.cl == 0.80);
        REQUIRE(out90.cl == 0.90);
    }
    
    SECTION("99% confidence level works")
    {
        PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
            pt99(B_outer, B_inner, 0.99, res);
        
        randutils::seed_seq_fe128 seed{888u};
        std::mt19937_64 rng(seed);
        
        auto out = pt99.run(x, mean_sampler, rng);
        
        REQUIRE(out.cl == 0.99);
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));
        REQUIRE(out.lower < out.upper);
    }
}

TEST_CASE("PercentileTBootstrap: L diagnostic value is captured",
          "[Bootstrap][PercentileT][Diagnostics]")
{
    using D = DecimalType;
    
    const std::size_t n = 30;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<double>(i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    const std::size_t L = 5;
    StationaryMaskValueResampler<D> res(L);
    
    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(400, 100, 0.95, res);
    
    randutils::seed_seq_fe128 seed{999u};
    std::mt19937_64 rng(seed);
    
    SECTION("Result.L matches resampler's getL()")
    {
        auto out = pt.run(x, mean_sampler, rng);
        
        REQUIRE(out.L == L);
        REQUIRE(out.L == res.getL());
    }
}

TEST_CASE("PercentileTBootstrap: Inner loop early stopping",
          "[Bootstrap][PercentileT][EarlyStopping]")
{
    using D = DecimalType;
    
    // Create data with very consistent variance
    // so inner loops should stabilize early
    std::mt19937_64 gen(12345);
    std::normal_distribution<double> dist(10.0, 0.5);
    
    std::vector<D> x;
    x.reserve(200);
    for (int i = 0; i < 200; ++i) {
        x.emplace_back(D(dist(gen)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    IIDResamplerForTest res;
    
    // Use large B_inner to allow early stopping to matter
    const std::size_t B_outer = 400;
    const std::size_t B_inner = 500;  // Large enough for early stopping
    
    PercentileTBootstrap<D, decltype(mean_sampler), IIDResamplerForTest>
        pt(B_outer, B_inner, 0.95, res);
    
    randutils::seed_seq_fe128 seed{12121u};
    std::mt19937_64 rng(seed);
    
    SECTION("Early stopping reduces inner attempts")
    {
        auto out = pt.run(x, mean_sampler, rng);
        
        // If early stopping is working, inner_attempted_total should be
        // less than B_outer * B_inner
        const std::size_t max_possible = B_outer * B_inner;
        
        REQUIRE(out.inner_attempted_total < max_possible);
        
        // But we should have attempted at least MIN_INNER (100) per effective outer
        REQUIRE(out.inner_attempted_total >= out.effective_B * 100);
    }
}

TEST_CASE("BCaCompatibleTBootstrap: Basic functionality",
          "[Bootstrap][BCaCompatible]")
{
    using D = DecimalType;
    
    const std::size_t n = 50;
    std::vector<D> returns;
    returns.reserve(n);
    
    std::mt19937_64 gen(54321);
    std::normal_distribution<double> dist(0.01, 0.02);
    for (std::size_t i = 0; i < n; ++i) {
        returns.emplace_back(D(dist(gen)));
    }
    
    auto statistic = [](const std::vector<D>& x) -> D {
        double s = 0.0;
        for (const auto& v : x) s += num::to_double(v);
        return D(s / static_cast<double>(x.size()));
    };
    
    IIDResamplerForTest sampler;
    MockEngineProvider provider;
    
    const unsigned int num_resamples = 400;
    const double confidence_level = 0.95;
    
    SECTION("Constructor succeeds with valid parameters")
    {
        REQUIRE_NOTHROW((
            BCaCompatibleTBootstrap<D, IIDResamplerForTest, std::mt19937_64, MockEngineProvider>(
                returns, num_resamples, confidence_level, statistic, sampler, provider
            )
        ));
    }
    
    SECTION("Invalid constructor parameters throw")
    {
        std::vector<D> empty_returns;
        
        REQUIRE_THROWS_AS((
            BCaCompatibleTBootstrap<D, IIDResamplerForTest, std::mt19937_64, MockEngineProvider>(
                empty_returns, num_resamples, confidence_level, statistic, sampler, provider
            )
        ), std::invalid_argument);
        
        REQUIRE_THROWS_AS((
            BCaCompatibleTBootstrap<D, IIDResamplerForTest, std::mt19937_64, MockEngineProvider>(
                returns, 50, confidence_level, statistic, sampler, provider  // num_resamples < 100
            )
        ), std::invalid_argument);
        
        REQUIRE_THROWS_AS((
            BCaCompatibleTBootstrap<D, IIDResamplerForTest, std::mt19937_64, MockEngineProvider>(
                returns, num_resamples, 0.0, statistic, sampler, provider  // CL <= 0
            )
        ), std::invalid_argument);
        
        REQUIRE_THROWS_AS((
            BCaCompatibleTBootstrap<D, IIDResamplerForTest, std::mt19937_64, MockEngineProvider>(
                returns, num_resamples, 1.0, statistic, sampler, provider  // CL >= 1
            )
        ), std::invalid_argument);
    }
    
    SECTION("BCa-compatible interface methods work")
    {
        BCaCompatibleTBootstrap<D, IIDResamplerForTest, std::mt19937_64, MockEngineProvider>
            bca(returns, num_resamples, confidence_level, statistic, sampler, provider);
        
        // Get bounds triggers lazy calculation
        D lower = bca.getLowerBound();
        D upper = bca.getUpperBound();
        D stat = bca.getStatistic();
        
        REQUIRE(std::isfinite(num::to_double(lower)));
        REQUIRE(std::isfinite(num::to_double(upper)));
        REQUIRE(std::isfinite(num::to_double(stat)));
        REQUIRE(lower <= stat);
        REQUIRE(stat <= upper);
    }
    
    SECTION("Lazy calculation only runs once")
    {
        BCaCompatibleTBootstrap<D, IIDResamplerForTest, std::mt19937_64, MockEngineProvider>
            bca(returns, num_resamples, confidence_level, statistic, sampler, provider);
        
        // First call triggers calculation
        D lower1 = bca.getLowerBound();
        const std::size_t calls_after_first = provider.called_indices.size();
        
        // Subsequent calls should not trigger additional calculation
        D lower2 = bca.getLowerBound();
        D upper1 = bca.getUpperBound();
        D stat1 = bca.getStatistic();
        
        const std::size_t calls_after_all = provider.called_indices.size();
        
        // Should be the same number of provider calls
        REQUIRE(calls_after_first == calls_after_all);
        
        // Results should be identical
        REQUIRE(num::to_double(lower1) == num::to_double(lower2));
    }
}

TEST_CASE("PercentileTBootstrap: Override parameters work correctly",
          "[Bootstrap][PercentileT][Override]")
{
    using D = DecimalType;
    
    const std::size_t n = 80;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.emplace_back(D(static_cast<double>(i) / 10.0));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    PercentileTBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        pt(400, 100, 0.95, res, 0.8, 0.7);
    
    randutils::seed_seq_fe128 seed{424242u};
    std::mt19937_64 rng(seed);
    
    SECTION("m_outer_override takes precedence")
    {
        const std::size_t m_outer_override = 60;
        auto out = pt.run(x, mean_sampler, rng, m_outer_override, 0);
        
        REQUIRE(out.m_outer == m_outer_override);
    }
    
    SECTION("m_inner_override takes precedence")
    {
        const std::size_t m_inner_override = 35;
        auto out = pt.run(x, mean_sampler, rng, 0, m_inner_override);
        
        REQUIRE(out.m_inner == m_inner_override);
    }
    
    SECTION("Both overrides work together")
    {
        const std::size_t m_outer_override = 70;
        const std::size_t m_inner_override = 40;
        auto out = pt.run(x, mean_sampler, rng, m_outer_override, m_inner_override);
        
        REQUIRE(out.m_outer == m_outer_override);
        REQUIRE(out.m_inner == m_inner_override);
    }
}

TEST_CASE("PercentileTBootstrap: Edge case - very high skipped rate",
          "[Bootstrap][PercentileT][EdgeCase]")
{
    // Use double as the Decimal type for this test since we need to preserve NaN values.
    // The decimal<8> fixed-point type cannot represent NaN (it truncates to a finite value).
    using D = double;
    
    // Create varied data (not constant, to avoid SE*=0 issues)
    std::vector<D> x;
    for (int i = 0; i < 50; ++i) {
        x.push_back(static_cast<double>(i % 10));  // Some variation
    }
    
    // Sampler that frequently returns degenerate values
    // Use a mutable functor instead of static lambda to avoid shared state issues
    struct FlakySampler {
      double operator()(const std::vector<D>& a) const {
        if (a.empty()) return 0.0;
        
        // Compute mean and sum
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        const double mean = s / static_cast<double>(a.size());
        
        // Use a hash of the sum to decide failure (more evenly distributed than modulo)
        // This ensures we get approximately 3% failures across different data patterns
        const uint64_t hash = static_cast<uint64_t>(std::round(s * 1000000.0));
        const int hash_mod = hash % 33;
        
        // Fail if hash_mod is 0 (~3% of cases)
        if (hash_mod == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        
        return mean;
      }
    };
 
    FlakySampler flaky_sampler;
    IIDResamplerForTest res;
    
    PercentileTBootstrap<D, FlakySampler, IIDResamplerForTest>
        pt(400, 100, 0.95, res);
    
    randutils::seed_seq_fe128 seed{31415u};
    std::mt19937_64 rng(seed);
    
    SECTION("High skip rate is reported correctly")
    {
        auto out = pt.run(x, flaky_sampler, rng);
        
        // Should have significant skipping due to NaN values
        const double skip_rate = static_cast<double>(out.skipped_outer) / 
                                 static_cast<double>(out.B_outer);
        
        // With ~3% failure rate in sampler calls, we should get some skips
        REQUIRE(skip_rate > 0.01);  // At least 1% skipped (conservative threshold)
        
        // But should still produce valid results if effective_B is sufficient
        if (out.effective_B >= 16) {
            REQUIRE(std::isfinite(num::to_double(out.lower)));
            REQUIRE(std::isfinite(num::to_double(out.upper)));
        }
    }
}

TEST_CASE("PercentileTBootstrap: Minimum inner replicate threshold",
          "[Bootstrap][PercentileT][MinInner]")
{
    using D = DecimalType;
    
    // Use data that might cause some inner loops to have few valid samples
    std::vector<D> x;
    for (int i = 0; i < 30; ++i) {
        x.push_back(D(static_cast<double>(i % 5)));  // Limited variation
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    IIDResamplerForTest res;
    
    // Use small m_inner to approach MIN_INNER threshold
    PercentileTBootstrap<D, decltype(mean_sampler), IIDResamplerForTest>
        pt(400, 100, 0.95, res, 1.0, 0.3);
    
    randutils::seed_seq_fe128 seed{2718u};
    std::mt19937_64 rng(seed);
    
    SECTION("Outer replicates with insufficient inner samples are skipped")
    {
        auto out = pt.run(x, mean_sampler, rng);
        
        // Should have produced some valid results
        REQUIRE(out.effective_B > 0);
        
        // Check that skipping logic was potentially invoked
        // (hard to guarantee, but we can verify the accounting is consistent)
        REQUIRE(out.effective_B + out.skipped_outer == out.B_outer);
    }
}
