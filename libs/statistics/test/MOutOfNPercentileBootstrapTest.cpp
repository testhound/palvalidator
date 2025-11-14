#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <random>

#include "number.h"
#include "StatUtils.h"
#include "TestUtils.h"
#include "StationaryMaskResamplers.h"
#include "MOutOfNPercentileBootstrap.h"
#include "BiasCorrectedBootstrap.h"
#include "RngUtils.h"

using palvalidator::analysis::quantile_type7_sorted;
using palvalidator::analysis::MOutOfNPercentileBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using mkc_timeseries::rng_utils::make_seed_seq;

// -------------------------------
// Mock BCa for Annualizer checks
// -------------------------------
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

// Helpers used in an annualizer check
static inline double round_to_decimal8(double x)
{
    return std::round(x * 1e8) / 1e8;
}
static inline double annualize_expect(double r_per_period, long double K)
{
    const long double g = std::log1pl(static_cast<long double>(r_per_period));
    const long double a = std::exp(K * g) - 1.0L;
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
        REQUIRE(num::to_double(quantile_type7_sorted(v, 0.0)) == Catch::Approx(10.0));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 0.5)) == Catch::Approx(30.0));
        REQUIRE(num::to_double(quantile_type7_sorted(v, 1.0)) == Catch::Approx(50.0));
    }

    SECTION("Linear interpolation between adjacent points")
    {
        // v = [0, 10, 20, 30], n=4
        // p=0.25: h=(3)*0.25+1=1.75 -> i=1, frac=0.75 → 7.5
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

    // B < 400 → invalid
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

    std::seed_seq seq = make_seed_seq(0x0000000100000002ull);
    std::mt19937_64 rng(seq);

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

    // Mean sampler (arithmetic mean)
    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t L = 3;
    StationaryMaskValueResampler<D> res(L);

    std::seed_seq seq = make_seed_seq(0x0000000b00000016ull);
    std::mt19937_64 rng(seq);

    // B >= 800 keeps runtime modest but stable
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
        REQUIRE(result.lower <= result.upper);
        const double mu = num::to_double(result.mean);
        const double lo = num::to_double(result.lower);
        const double hi = num::to_double(result.upper);
        REQUIRE(lo <= mu);
        REQUIRE(mu <= hi);
    }

    SECTION("m_sub override is respected")
    {
        std::seed_seq seq2 = make_seed_seq(0x5A5A5A5A5A5A5A5Aull);
        std::mt19937_64 rng2(seq2);
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

    // Data with a little curvature (squares)
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

    std::seed_seq seqA = make_seed_seq(0x00000065000000CAull);
    std::seed_seq seqB = make_seed_seq(0x00000065000000CBull);
    std::mt19937_64 rngA(seqA), rngB(seqB);

    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon90(1000, 0.90, 0.70, res);
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> moon95(1000, 0.95, 0.70, res);

    auto r90 = moon90.run(x, mean_sampler, rngA);
    auto r95 = moon95.run(x, mean_sampler, rngB);

    const double w90 = num::to_double(r90.upper) - num::to_double(r90.lower);
    const double w95 = num::to_double(r95.upper) - num::to_double(r95.lower);
    REQUIRE(w95 >= w90 - 1e-12);
}

TEST_CASE("MOutOfNPercentileBootstrap + GeoMeanStat: small-n (n=20) basics", "[Bootstrap][MOutOfN][GeoMean][SmallN]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;

    // Build a small-n per-period return series with mild +/- noise, all > -1
    const std::size_t n = 20;
    std::vector<D> r; r.reserve(n);
    const double base_vals[] = { 0.0020, -0.0010, 0.0005, 0.0030, -0.0008 };
    for (std::size_t i = 0; i < n; ++i)
        r.emplace_back(D(base_vals[i % 5]));

    GeoMeanStat<D> geo(/*clip_ruin=*/true,
                       /*winsor_small_n=*/true,
                       /*winsor_alpha=*/0.02,
                       /*ruin_eps=*/1e-8);

    auto sampler = [&](const std::vector<D>& a) -> D { return geo(a); };

    StationaryMaskValueResampler<D> res(/*L=*/3);

    std::seed_seq seq = make_seed_seq(0x000007E90000000Aull); // arbitrary, stable
    std::mt19937_64 rng(seq);

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

        REQUIRE(std::isfinite(num::to_double(out.lower)));
        REQUIRE(std::isfinite(num::to_double(out.mean)));
        REQUIRE(std::isfinite(num::to_double(out.upper)));
        REQUIRE(out.lower <= out.mean);
        REQUIRE(out.mean  <= out.upper);
    }

    SECTION("Changing confidence level widens interval")
    {
        std::seed_seq seqA = make_seed_seq(0x90ull);
        std::seed_seq seqB = make_seed_seq(0x95ull);
        std::mt19937_64 rngA(seqA), rngB(seqB);

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
        std::seed_seq seqA = make_seed_seq(0x80ull);
        std::seed_seq seqB = make_seed_seq(0x50ull);
        std::mt19937_64 rngA(seqA), rngB(seqB);

        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon80(/*B=*/1000, 0.95, 0.80, res);
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> moon50(/*B=*/1000, 0.95, 0.50, res);

        const auto o80 = moon80.run(r, sampler, rngA);
        const auto o50 = moon50.run(r, sampler, rngB);

        const double w80 = num::to_double(o80.upper) - num::to_double(o80.lower);
        const double w50 = num::to_double(o50.upper) - num::to_double(o50.lower);

        REQUIRE(w50 >= w80 - 1e-12);
    }

    SECTION("m_sub override respected")
    {
        std::seed_seq seqC = make_seed_seq(0xDEADBEEFCAFEBABEull);
        std::mt19937_64 rngC(seqC);
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
        const double v = 0.0004
                       + 0.0003 * std::sin(static_cast<double>(i) / 6.0)
                       - 0.0002 * ((i % 7) == 0 ? 1.0 : 0.0);
        r.emplace_back(D(v));
    }

    GeoMeanStat<D> geo(/*clip_ruin=*/true,
                       /*winsor_small_n=*/true,
                       /*winsor_alpha=*/0.02,
                       /*ruin_eps=*/1e-8);

    auto sampler = [&](const std::vector<D>& a) -> D { return geo(a); };

    StationaryMaskValueResampler<D> res(/*L=*/4);

    std::seed_seq seq = make_seed_seq(0x4D4D00000000000Bull);
    std::mt19937_64 rng(seq);

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
        const D lower = out.lower;
        const D mean  = out.mean;
        const D upper = out.upper;

        REQUIRE(lower <= mean);
        REQUIRE(mean  <= upper);
        REQUIRE(std::isfinite(num::to_double(lower)));
        REQUIRE(std::isfinite(num::to_double(mean)));
        REQUIRE(std::isfinite(num::to_double(upper)));

        const long double K = 252.0L;
        MOfNMockBCaForAnnualizer<D> mock(mean, lower, upper);
        mkc_timeseries::BCaAnnualizer<D> ann(mock, static_cast<double>(K));

        const double lo_ann = num::to_double(ann.getAnnualizedLowerBound());
        const double mu_ann = num::to_double(ann.getAnnualizedMean());
        const double hi_ann = num::to_double(ann.getAnnualizedUpperBound());

        REQUIRE(std::isfinite(lo_ann));
        REQUIRE(std::isfinite(mu_ann));
        REQUIRE(std::isfinite(hi_ann));
        REQUIRE(lo_ann <= mu_ann);
        REQUIRE(mu_ann <= hi_ann);
        REQUIRE(lo_ann > -1.0);

        const double lo_exp = round_to_decimal8(annualize_expect(num::to_double(lower), K));
        const double mu_exp = round_to_decimal8(annualize_expect(num::to_double(mean ), K));
        const double hi_exp = round_to_decimal8(annualize_expect(num::to_double(upper), K));

        REQUIRE(lo_ann == Catch::Approx(lo_exp).margin(1e-12));
        REQUIRE(mu_ann == Catch::Approx(mu_exp).margin(1e-12));
        REQUIRE(hi_ann == Catch::Approx(hi_exp).margin(1e-12));

        // Optional monotonicity sanity for small positive means
        MOfNMockBCaForAnnualizer<D> mock252(mean, lower, upper);
        MOfNMockBCaForAnnualizer<D> mock504(mean, lower, upper);
        mkc_timeseries::BCaAnnualizer<D> ann252(mock252, 252.0);
        mkc_timeseries::BCaAnnualizer<D> ann504(mock504, 504.0);
        REQUIRE(num::to_double(ann504.getAnnualizedMean())
             >= num::to_double(ann252.getAnnualizedMean()) - 1e-12);
    }
}

// -----------------------------------------------
// Executor policy: SingleThread vs ThreadPool
// -----------------------------------------------
TEST_CASE("MOutOfNPercentileBootstrap: Executor policy parity (CRN provider)", "[Bootstrap][MOutOfN][Executor]")
{
    using D = DecimalType;
    using ResT = StationaryMaskValueResampler<D>;

    // Simple deterministic series
    const std::size_t n = 64;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        x.emplace_back(D(0.001 * static_cast<double>(i % 9) - 0.002));

    // Statistic under test: arithmetic mean
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        long double s = 0.0L;
        for (const auto& v : a)
            s += static_cast<long double>(num::to_double(v));
        return D(static_cast<double>(s / static_cast<long double>(a.size())));
    };

    // Dependence-aware resampler
    ResT res(/*L=*/4);

    // Deterministic CRN-like provider: engine per replicate index b
    struct DummyCRN {
        std::mt19937_64 make_engine(std::size_t b) const {
            std::seed_seq ss{ static_cast<unsigned>(b & 0xffffffffu),
                              static_cast<unsigned>((b >> 32) & 0xffffffffu),
                              0xA5A5A5A5u, 0x5A5A5A5Au };
            return std::mt19937_64(ss);
        }
    } crn_provider;

    // Common bootstrap settings
    const std::size_t B   = 1200;
    const double      CL  = 0.95;
    const double      rho = 0.70;

    // Single-thread executor specialization
    using BootST = MOutOfNPercentileBootstrap<
        D, decltype(mean_sampler), ResT, std::mt19937_64, concurrency::SingleThreadExecutor
    >;

    // Thread-pool executor specialization
    using BootTP = MOutOfNPercentileBootstrap<
        D, decltype(mean_sampler), ResT, std::mt19937_64, concurrency::ThreadPoolExecutor<>
    >;

    BootST boot_st(B, CL, rho, res);
    BootTP boot_tp(B, CL, rho, res);

    boot_st.setChunkSizeHint(2048);
    boot_tp.setChunkSizeHint(2048);

    auto r_st = boot_st.run(x, mean_sampler, crn_provider);
    auto r_tp = boot_tp.run(x, mean_sampler, crn_provider);

    // Sanity: identical effective_B/skips and near-perfect parity
    REQUIRE(r_st.B           == r_tp.B);
    REQUIRE(r_st.effective_B == r_tp.effective_B);
    REQUIRE(r_st.skipped     == r_tp.skipped);
    REQUIRE(r_st.n           == r_tp.n);
    REQUIRE(r_st.m_sub       == r_tp.m_sub);
    REQUIRE(r_st.L           == r_tp.L);

    REQUIRE(num::to_double(r_st.mean)  == Catch::Approx(num::to_double(r_tp.mean)).margin(0.0));
    REQUIRE(num::to_double(r_st.lower) == Catch::Approx(num::to_double(r_tp.lower)).margin(0.0));
    REQUIRE(num::to_double(r_st.upper) == Catch::Approx(num::to_double(r_tp.upper)).margin(0.0));
}

// --------------------------------------------------------------
// Executor policy: sanity at small-n with GeoMean (n=24, B=1000)
// --------------------------------------------------------------
TEST_CASE("MOutOfNPercentileBootstrap: ThreadPoolExecutor works at small-n", "[Bootstrap][MOutOfN][Executor][SmallN]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;
    using ResT = StationaryMaskValueResampler<D>;

    const std::size_t n = 24;
    std::vector<D> r; r.reserve(n);
    const double base[] = { +0.0015, -0.0008, +0.0007, -0.0004, +0.0011, 0.0 };
    for (std::size_t i = 0; i < n; ++i)
        r.emplace_back(D(base[i % 6]));

    GeoMeanStat<D> geo(/*clip_ruin=*/true, /*winsor_small_n=*/true, /*winsor_alpha=*/0.02, /*ruin_eps=*/1e-9);
    auto sampler = [&](const std::vector<D>& a) -> D { return geo(a); };

    ResT res(/*L=*/3);

    struct DummyCRN {
        std::mt19937_64 make_engine(std::size_t b) const {
            std::seed_seq ss{ static_cast<unsigned>(b), 0xC0FFEEu, 0xFACEFEEDu };
            return std::mt19937_64(ss);
        }
    } crn;

    const std::size_t B  = 1000;
    const double      CL = 0.95;

    using BootST = MOutOfNPercentileBootstrap<
        D, decltype(sampler), ResT, std::mt19937_64, concurrency::SingleThreadExecutor
    >;
    using BootTP = MOutOfNPercentileBootstrap<
        D, decltype(sampler), ResT, std::mt19937_64, concurrency::ThreadPoolExecutor<>
    >;

    BootST st(B, CL, /*rho=*/0.70, res);
    BootTP tp(B, CL, /*rho=*/0.70, res);

    auto a = st.run(r, sampler, crn);
    auto b = tp.run(r, sampler, crn);

    // Ensure results are finite and ordered
    REQUIRE(std::isfinite(num::to_double(a.lower)));
    REQUIRE(std::isfinite(num::to_double(a.mean  )));
    REQUIRE(std::isfinite(num::to_double(a.upper )));
    REQUIRE(a.lower <= a.mean);
    REQUIRE(a.mean  <= a.upper);

    REQUIRE(std::isfinite(num::to_double(b.lower)));
    REQUIRE(std::isfinite(num::to_double(b.mean  )));
    REQUIRE(std::isfinite(num::to_double(b.upper )));
    REQUIRE(b.lower <= b.mean);
    REQUIRE(b.mean  <= b.upper);

    // Parity: identical under CRN provider
    REQUIRE(num::to_double(a.mean)  == Catch::Approx(num::to_double(b.mean)).margin(0.0));
    REQUIRE(num::to_double(a.lower) == Catch::Approx(num::to_double(b.lower)).margin(0.0));
    REQUIRE(num::to_double(a.upper) == Catch::Approx(num::to_double(b.upper)).margin(0.0));
}
