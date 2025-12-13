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

#include "number.h"
#include "StatUtils.h"
#include "randutils.hpp"
#include "BiasCorrectedBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "PercentileTBootstrap.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

using palvalidator::analysis::PercentileTBootstrap;
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

// Simple sampler: arithmetic mean
struct MeanSampler
{
    template <typename Decimal>
    Decimal operator()(const std::vector<Decimal>& x) const
    {
        long double sum = 0.0L;
        for (auto& v : x) sum += static_cast<long double>(v);
        return static_cast<Decimal>(sum / static_cast<long double>(x.size()));
    }
};

// Minimal IID value resampler for tests (no blocks); matches (src, dst, m, rng) + getL()
struct IIDResamplerForTest
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

        double sum  = 0.0;
        double sum2 = 0.0;
        for (double v : thetas) {
            sum  += v;
            sum2 += v * v;
        }
        const double m   = static_cast<double>(thetas.size());
        const double var = std::max(0.0, (sum2 / m) - (sum / m) * (sum / m));
        const double se  = std::sqrt(var);

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
