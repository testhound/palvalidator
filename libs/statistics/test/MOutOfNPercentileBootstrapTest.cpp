#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

#include "number.h"
#include "StatUtils.h" 
#include "TestUtils.h"
#include "randutils.hpp"
#include "StationaryMaskResamplers.h"
#include "MOutOfNPercentileBootstrap.h"
#include "BiasCorrectedBootstrap.h"

using palvalidator::analysis::quantile_type7_sorted;
using palvalidator::analysis::MOutOfNPercentileBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;

template <class D>
class MOfNMockBCaForAnnualizer
  : public mkc_timeseries::BCaBootStrap<D, mkc_timeseries::StationaryBlockResampler<D>>
{
    using Base = mkc_timeseries::BCaBootStrap<D, mkc_timeseries::StationaryBlockResampler<D>>;
public:
    MOfNMockBCaForAnnualizer(const D& mean, const D& lower, const D& upper)
      : Base(/*returns=*/std::vector<D>{D(0), D(0)}, /*B=*/100, /*CL=*/0.95,
             /*statistic=*/[](const std::vector<D>& v){ return v[0]; },
             /*sampler=*/mkc_timeseries::StationaryBlockResampler<D>(3))
    {
        this->setMean(mean);
        this->setLowerBound(lower);
        this->setUpperBound(upper);
        this->m_is_calculated = true; // prevent real BCa path
    }
protected:
    void calculateBCaBounds() override { this->m_is_calculated = true; } // no-op
};

// Helper to match decimal<8> lattice
static inline double round_to_decimal8(double x)
{
    return std::round(x * 1e8) / 1e8;
}

// Analytic annualization: (1 + r)^K - 1 using long double
static inline double annualize_expect(double r_per_period, long double K)
{
    const long double g = std::log1pl(static_cast<long double>(r_per_period));
    const long double a = std::exp(K * g) - 1.0L; // std::exp is overloaded for long double
    return static_cast<double>(a);
}

// -----------------------------
// quantile_type7_sorted tests
// -----------------------------

TEST_CASE("quantile_type7_sorted: basic properties and edges", "[Quantile][Type7]")
{
    using D = DecimalType;

    SECTION("Throws on empty input")
    {
        std::vector<D> empty;
        REQUIRE_THROWS_AS(quantile_type7_sorted(empty, 0.5), std::invalid_argument);
    }

    SECTION("p <= 0 returns front; p >= 1 returns back")
    {
        std::vector<D> v{ D(1), D(3), D(5), D(7) };
        REQUIRE(num::to_double(quantile_type7_sorted(v, -0.1)) == Catch::Approx(1.0));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 0.0))  == Catch::Approx(1.0));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 1.0))  == Catch::Approx(7.0));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 1.1))  == Catch::Approx(7.0));
    }

    SECTION("Matches integer order stats at exact plotting positions")
    {
        // For type-7: h = (n-1)p + 1; when h is integer, we return x[h]
        std::vector<D> v{ D(10), D(20), D(30), D(40), D(50) }; // n=5, 0-based indices 0..4
        // p that makes h integer:
        // h=1 -> p=0.0 ; h=3 -> p=(3-1)/(5-1)=0.5 ; h=5 -> p=1.0
        REQUIRE(num::to_double(quantile_type7_sorted(v, 0.0)) == Catch::Approx(10.0));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 0.5)) == Catch::Approx(30.0));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 1.0)) == Catch::Approx(50.0));
    }

    SECTION("Linear interpolation between adjacent points")
    {
        // v = [0, 10, 20, 30], n=4
        // For p=0.25: h = (3)*0.25 + 1 = 1.75 -> i=floor(1.75)=1, frac=0.75
        // Q = v[0] + 0.75*(v[1]-v[0]) = 0 + 0.75*(10-0) = 7.5
        std::vector<D> v{ D(0), D(10), D(20), D(30) };
        REQUIRE(num::to_double(quantile_type7_sorted(v, 0.25)) == Catch::Approx(7.5));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 0.75)) == Catch::Approx(22.5));
    }

    SECTION("Monotonic in p")
    {
        std::vector<D> v{ D(1), D(2), D(3), D(4), D(5), D(6) };
        const double q1 = num::to_double(quantile_type7_sorted(v, 0.2));
        const double q2 = num::to_double(quantile_type7_sorted(v, 0.8));
        REQUIRE(q1 <= q2);
    }
}

// -----------------------------------------
// MOutOfNPercentileBootstrap basic behavior
// -----------------------------------------

TEST_CASE("MOutOfNPercentileBootstrap: constructor validation", "[Bootstrap][MOutOfN]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    // B < 400
    REQUIRE_THROWS_AS((MOutOfNPercentileBootstrap<D,
                        std::function<D(const std::vector<D>&)>,
                        StationaryMaskValueResampler<D>>(399, 0.95, 0.7, res)),
                      std::invalid_argument);

    // CL out of range
    REQUIRE_THROWS_AS((MOutOfNPercentileBootstrap<D,
                        std::function<D(const std::vector<D>&)>,
                        StationaryMaskValueResampler<D>>(800, 0.5, 0.7, res)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((MOutOfNPercentileBootstrap<D,
                        std::function<D(const std::vector<D>&)>,
                        StationaryMaskValueResampler<D>>(800, 1.0, 0.7, res)),
                      std::invalid_argument);

    // m_ratio out of (0,1)
    REQUIRE_THROWS_AS((MOutOfNPercentileBootstrap<D,
                        std::function<D(const std::vector<D>&)>,
                        StationaryMaskValueResampler<D>>(800, 0.95, 0.0, res)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS((MOutOfNPercentileBootstrap<D,
                        std::function<D(const std::vector<D>&)>,
                        StationaryMaskValueResampler<D>>(800, 0.95, 1.0, res)),
                      std::invalid_argument);
}

TEST_CASE("MOutOfNPercentileBootstrap: run() input validation", "[Bootstrap][MOutOfN]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    // n < 3
    std::vector<D> tiny{ D(1), D(2) };

    randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
    randutils::mt19937_rng rng(seed);

    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(800, 0.95, 0.7, res);
    REQUIRE_THROWS_AS(moon.run(tiny, mean_sampler, rng), std::invalid_argument);
}

TEST_CASE("MOutOfNPercentileBootstrap: basic statistics and diagnostics", "[Bootstrap][MOutOfN]")
{
    using D = DecimalType;

    // Build a simple increasing series to make sanity checks easy
    const std::size_t n = 60;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i))); // 0..59

    // Mean sampler (arithmetic mean for simplicity)
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t L = 3;
    StationaryMaskValueResampler<D> res(L);

    // Default RNG template is randutils::mt19937_rng; pass explicit rng anyway for clarity
    randutils::seed_seq_fe128 seed{11u,22u,33u,44u};
    randutils::mt19937_rng rng(seed);

    // Use B >= 800 to keep runtime modest but stable
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon(800, 0.95, 0.70, res);

    auto result = moon.run(x, mean_sampler, rng);

    SECTION("Result fields are populated consistently")
    {
        REQUIRE(result.B == 800);
        REQUIRE(result.cl == Catch::Approx(0.95));
        REQUIRE(result.n == n);
        REQUIRE(result.m_sub >= 2);
        REQUIRE(result.m_sub < n);
        REQUIRE(result.L == L);
        REQUIRE(result.effective_B >= result.B / 2); // non-degenerate majority
        REQUIRE(result.skipped + result.effective_B == result.B);
    }

    SECTION("Mean equals the statistic on original sample")
    {
        // The true mean of 0..59 is 29.5
        REQUIRE(num::to_double(result.mean) == Catch::Approx(29.5).margin(1e-12));
    }

    SECTION("Percentile bracket sanity")
    {
        // Lower <= Upper and mean should be between (not guaranteed tight, but typical)
        REQUIRE(result.lower <= result.upper);
        const double mu = num::to_double(result.mean);
        const double lo = num::to_double(result.lower);
        const double hi = num::to_double(result.upper);
        REQUIRE(lo <= mu);
        REQUIRE(mu <= hi);
    }

    SECTION("m_sub override is respected")
    {
        // Re-run with explicit m_sub override
        randutils::mt19937_rng rng2(seed);
        const std::size_t m_override = 25;
        auto result2 = moon.run(x, mean_sampler, rng2, m_override);
        REQUIRE(result2.m_sub == m_override);
        REQUIRE(result2.n == n);
        REQUIRE(result2.L == L);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: quantile behavior reflects confidence level", "[Bootstrap][MOutOfN][Quantile]")
{
    using D = DecimalType;

    // Data with a little curvature (squares) to avoid too symmetric stats
    const std::size_t n = 80;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) x.emplace_back(D(static_cast<int>(i * i)));

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        long double s = 0.0L;
        for (const auto& v : a) s += static_cast<long double>(num::to_double(v));
        return D(static_cast<double>(s / static_cast<long double>(a.size())));
    };

    StationaryMaskValueResampler<D> res(4);

    randutils::seed_seq_fe128 seed{101u,202u,303u,404u};
    randutils::mt19937_rng rngA(seed), rngB(seed);

    // Two different CLs; same B and seed so the only difference is the quantile
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon90(1000, 0.90, 0.70, res);
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon95(1000, 0.95, 0.70, res);

    auto r90 = moon90.run(x, mean_sampler, rngA);
    auto r95 = moon95.run(x, mean_sampler, rngB);

    // 95% CI should be (weakly) wider than 90% CI
    const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
    const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);
    REQUIRE(w95 >= w90 - 1e-12);
}

TEST_CASE("MOutOfNPercentileBootstrap + GeoMeanStat: small-n (n=20) basics", "[Bootstrap][MOutOfN][GeoMean][SmallN]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;

    // Build a small-n per-period return series with mild +/- noise, all > -1
    // Pattern repeats to avoid pathological tails; magnitudes are small (<= ~0.6%)
    const std::size_t n = 20;
    std::vector<D> r; r.reserve(n);
    const double base_vals[] = { 0.0020, -0.0010, 0.0005, 0.0030, -0.0008 };
    for (std::size_t i = 0; i < n; ++i)
    {
        r.emplace_back(D(base_vals[i % 5]));
    }

    // GeoMeanStat with conservative guards (clip ruin, winsorize at small n)
    GeoMeanStat<D> geo(/*clip_ruin=*/true,
                       /*winsor_small_n=*/true,
                       /*winsor_alpha=*/0.02,
                       /*ruin_eps=*/1e-8);

    auto sampler = [&](const std::vector<D>& a) -> D
    {
        return geo(a);
    };

    // Resampler with L=3 (appropriate for small-n)
    StationaryMaskValueResampler<D> res(/*L=*/3);

    randutils::seed_seq_fe128 seed{2025u, 10u, 30u, 1u};
    randutils::mt19937_rng rng(seed);

    // B kept modest for test runtime; still >= 800 per your validation pattern
    MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon(/*B=*/1000,
                                                                                           /*CL=*/0.95,
                                                                                           /*m_ratio=*/0.70,
                                                                                           res);

    auto out = moon.run(r, sampler, rng);

    SECTION("Result invariants and finite outputs")
    {
        REQUIRE(out.B == 1000);
        REQUIRE(out.n == n);
        REQUIRE(out.m_sub >= 2);
        REQUIRE(out.m_sub < n);
        REQUIRE(out.effective_B + out.skipped == out.B);

        // Bounds should be finite and ordered
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));

        REQUIRE(out.lower <= out.mean);
        REQUIRE(out.mean  <= out.upper);
    }

    SECTION("Changing confidence level widens interval")
    {
        randutils::mt19937_rng rngA(seed), rngB(seed);

        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon90(/*B=*/1000, 0.90, 0.70, res);
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon95(/*B=*/1000, 0.95, 0.70, res);

        const auto r90 = moon90.run(r, sampler, rngA);
        const auto r95 = moon95.run(r, sampler, rngB);

        const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
        const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);
        REQUIRE(w95 >= w90 - 1e-12);
    }

    SECTION("Smaller m (stronger sub-sampling) tends to widen CI")
    {
        randutils::mt19937_rng rngA(seed), rngB(seed);

        // Same B/CL/L, different m_ratio; smaller m typically increases variance
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon80(/*B=*/1000, 0.95, 0.80, res);
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon50(/*B=*/1000, 0.95, 0.50, res);

        const auto o80 = moon80.run(r, sampler, rngA);
        const auto o50 = moon50.run(r, sampler, rngB);

        const double w80 = num::to_double(o80.upper) - num::to_double(o80.lower);
        const double w50 = num::to_double(o50.upper) - num::to_double(o50.lower);

        // Not strictly guaranteed for every path, but should hold overwhelmingly in practice.
        REQUIRE(w50 >= w80 - 1e-12);
    }

    SECTION("m_sub override respected")
    {
        randutils::mt19937_rng rngC(seed);
        const std::size_t m_override = 13; // 2 <= m < n
        auto out2 = moon.run(r, sampler, rngC, m_override);
        REQUIRE(out2.m_sub == m_override);
        REQUIRE(out2.n == n);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap + GeoMeanStat: moderate-n (n=60) sanity", "[Bootstrap][MOutOfN][GeoMean]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;

    // Mildly varying returns (within +/-0.5%) and strictly > -1
    const std::size_t n = 60;
    std::vector<D> r; r.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        // Oscillate around ~0.0004 with small negatives
        const double v = 0.0004 + 0.0003 * std::sin(static_cast<double>(i) / 6.0) - 0.0002 * ((i % 7) == 0 ? 1.0 : 0.0);
        r.emplace_back(D(v));
    }

    GeoMeanStat<D> geo(/*clip_ruin=*/true,
                       /*winsor_small_n=*/true,
                       /*winsor_alpha=*/0.02,
                       /*ruin_eps=*/1e-8);

    auto sampler = [&](const std::vector<D>& a) -> D
    {
        return geo(a);
    };

    StationaryMaskValueResampler<D> res(/*L=*/4);

    randutils::seed_seq_fe128 seed{77u, 88u, 99u, 11u};
    randutils::mt19937_rng rng(seed);

    MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon(/*B=*/1200,
                                                                                           /*CL=*/0.95,
                                                                                           /*m_ratio=*/0.70,
                                                                                           res);

    auto out = moon.run(r, sampler, rng);

    SECTION("Outputs are finite and ordered")
    {
        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));
        REQUIRE(out.lower <= out.mean);
        REQUIRE(out.mean  <= out.upper);
    }

    SECTION("Effective replicates are reasonable")
    {
        REQUIRE(out.effective_B >= out.B / 2);
        REQUIRE(out.skipped + out.effective_B == out.B);
    }

    SECTION("Annualization via BCaAnnualizer preserves ordering and matches analytic (GeoMean)")
      {
	using D = DecimalType;

	// Per-period CI from MOutOfNPercentileBootstrap (GeoMean sampler on returns)
	const D lower = out.lower;
	const D mean  = out.mean;
	const D upper = out.upper;

	// Sanity on per-period outputs
	REQUIRE(lower <= mean);
	REQUIRE(mean  <= upper);
	REQUIRE(std::isfinite(num::to_double(lower)));
	REQUIRE(std::isfinite(num::to_double(mean)));
	REQUIRE(std::isfinite(num::to_double(upper)));

	// Annualize (e.g., daily bars K=252)
	const long double K = 252.0L;
	MOfNMockBCaForAnnualizer<D> mock(mean, lower, upper);
	mkc_timeseries::BCaAnnualizer<D> ann(mock, static_cast<double>(K));

	const double lo_ann = num::to_double(ann.getAnnualizedLowerBound());
	const double mu_ann = num::to_double(ann.getAnnualizedMean());
	const double hi_ann = num::to_double(ann.getAnnualizedUpperBound());

	// Ordering & > -1
	REQUIRE(std::isfinite(lo_ann));
	REQUIRE(std::isfinite(mu_ann));
	REQUIRE(std::isfinite(hi_ann));
	REQUIRE(lo_ann <= mu_ann);
	REQUIRE(mu_ann <= hi_ann);
	REQUIRE(lo_ann > -1.0);

	// Analytic match (rounded to decimal<8>)
	const auto round_to_decimal8 = [](double x){ return std::round(x * 1e8) / 1e8; };
	const auto annualize_expect = [](double r, long double K)
	{
	  const long double g = std::log1pl(static_cast<long double>(r));
	  const long double a = std::exp(K * g) - 1.0L;
	  return static_cast<double>(a);
	};

	const double lo_exp = round_to_decimal8(annualize_expect(num::to_double(lower), K));
	const double mu_exp = round_to_decimal8(annualize_expect(num::to_double(mean ), K));
	const double hi_exp = round_to_decimal8(annualize_expect(num::to_double(upper), K));

	REQUIRE(lo_ann == Catch::Approx(lo_exp).margin(1e-12));
	REQUIRE(mu_ann == Catch::Approx(mu_exp).margin(1e-12));
	REQUIRE(hi_ann == Catch::Approx(hi_exp).margin(1e-12));

	// Optional: larger K weakly increases annualized mean for small positive returns
	MOfNMockBCaForAnnualizer<D> mock252(mean, lower, upper);
	MOfNMockBCaForAnnualizer<D> mock504(mean, lower, upper);
	mkc_timeseries::BCaAnnualizer<D> ann252(mock252, 252.0);
	mkc_timeseries::BCaAnnualizer<D> ann504(mock504, 504.0);
	REQUIRE(num::to_double(ann504.getAnnualizedMean())
		>= num::to_double(ann252.getAnnualizedMean()) - 1e-12);
      }

}
