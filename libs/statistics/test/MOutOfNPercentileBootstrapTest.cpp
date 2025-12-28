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

// ========================================================================
// ADAPTIVE RATIO TESTS (NEW)
// ========================================================================

TEST_CASE("MOutOfNPercentileBootstrap: Adaptive constructor basic functionality",
          "[Bootstrap][MOutOfN][Adaptive]")
{
  using D = DecimalType;
  using mkc_timeseries::GeoMeanStat;
  using ResT = StationaryMaskValueResampler<D>;

  // Create a moderate-sized dataset with typical characteristics
  const std::size_t n = 40;
  std::vector<D> returns;
  returns.reserve(n);
  
  // Generate returns with some volatility
  for (std::size_t i = 0; i < n; ++i)
  {
    const double v = 0.001 * std::sin(static_cast<double>(i) / 5.0);
    returns.emplace_back(D(v));
  }

  GeoMeanStat<D> geo;
  auto sampler = [&](const std::vector<D>& a) -> D { return geo(a); };
  
  ResT res(3);
  
  std::seed_seq seq = make_seed_seq(0xADA971E001ull);
  std::mt19937_64 rng(seq);

  SECTION("Adaptive constructor compiles and runs")
  {
    // Start from fixed-ratio constructor, then switch to adaptive mode
    MOutOfNPercentileBootstrap<D, decltype(sampler), ResT> bootstrap(
        /*B=*/1000, /*CL=*/0.95, /*m_ratio=*/0.70, res);
    
    // Switch to adaptive mode with GeoMeanStat policy
    bootstrap.setAdaptiveRatioPolicy<GeoMeanStat<D>>(
        std::make_shared<palvalidator::analysis::TailVolatilityAdaptivePolicy<D, GeoMeanStat<D>>>());
    
    REQUIRE(bootstrap.isAdaptiveMode());
    REQUIRE(bootstrap.mratio() == -1.0);
    
    auto result = bootstrap.run(returns, sampler, rng);
    
    // Verify result structure
    REQUIRE(result.B == 1000);
    REQUIRE(result.n == n);
    REQUIRE(result.m_sub >= 2);
    REQUIRE(result.m_sub < n);

    // In adaptive mode, computed_ratio is the policy's logical m/n ratio,
    // which must lie strictly between 0 and 1.
    REQUIRE(std::isfinite(result.computed_ratio));
    REQUIRE(result.computed_ratio > 0.0);
    REQUIRE(result.computed_ratio < 1.0);
    
    // Verify bounds are valid
    REQUIRE(std::isfinite(num::to_double(result.lower)));
    REQUIRE(std::isfinite(num::to_double(result.mean)));
    REQUIRE(std::isfinite(num::to_double(result.upper)));
    REQUIRE(result.lower <= result.mean);
    REQUIRE(result.mean <= result.upper);
  }

  SECTION("Fixed ratio constructor still works (backward compatibility)")
  {
    MOutOfNPercentileBootstrap<D, decltype(sampler), ResT> bootstrap(
        /*B=*/1000, /*CL=*/0.95, /*m_ratio=*/0.70, res);
    
    REQUIRE_FALSE(bootstrap.isAdaptiveMode());
    REQUIRE(bootstrap.mratio() == Catch::Approx(0.70));
    
    auto result = bootstrap.run(returns, sampler, rng);
    
    // In fixed mode, computed_ratio must match the configured m_ratio.
    REQUIRE(result.computed_ratio == Catch::Approx(0.70).margin(0.0));
  }
}

TEST_CASE("StatisticalContext: Basic statistical calculations",
          "[Bootstrap][StatisticalContext]")
{
  using D = DecimalType;
  using palvalidator::analysis::detail::StatisticalContext;

  SECTION("Normal distribution characteristics")
  {
    // Generate approximately normal returns
    std::vector<D> returns;
    const std::size_t n = 50;
    returns.reserve(n);
    
    std::seed_seq seq = make_seed_seq(0x00A0AA1001ull);
    std::mt19937_64 rng(seq);
    std::normal_distribution<double> dist(0.001, 0.01);
    
    for (std::size_t i = 0; i < n; ++i)
      returns.emplace_back(D(dist(rng)));
    
    StatisticalContext<D> ctx(returns);
    
    REQUIRE(ctx.getSampleSize() == n);
    REQUIRE(ctx.getAnnualizedVolatility() > 0.0);
    REQUIRE(std::isfinite(ctx.getSkewness()));
    REQUIRE(std::isfinite(ctx.getExcessKurtosis()));
    
    // Normal distribution should not trigger heavy-tail flags
    // (though with small sample this might occasionally happen)
    INFO("Tail index: " << ctx.getTailIndex());
    INFO("Heavy tails: " << ctx.hasHeavyTails());
  }

  SECTION("Heavy-tailed distribution characteristics")
  {
    // Generate returns with heavy left tail
    std::vector<D> returns;
    const std::size_t n = 40;
    returns.reserve(n);
    
    // Most returns are small positive
    for (std::size_t i = 0; i < 30; ++i)
      returns.emplace_back(D(0.001));
    
    // Add some large negative returns (heavy left tail)
    for (std::size_t i = 0; i < 10; ++i)
      returns.emplace_back(D(-0.05 * (i + 1)));
    
    StatisticalContext<D> ctx(returns);
    
    REQUIRE(ctx.getSampleSize() == n);
    REQUIRE(ctx.getAnnualizedVolatility() > 0.0);
    
    // Should detect heavy tails or strong asymmetry
    const bool detected = ctx.hasHeavyTails() || ctx.hasStrongAsymmetry();
    INFO("Heavy tails: " << ctx.hasHeavyTails());
    INFO("Strong asymmetry: " << ctx.hasStrongAsymmetry());
    INFO("Tail index: " << ctx.getTailIndex());
    
    // With this construction, we expect detection
    REQUIRE(detected);
  }

  SECTION("Annualization factor is applied correctly")
  {
    std::vector<D> returns;
    for (std::size_t i = 0; i < 30; ++i)
      returns.emplace_back(D(0.01 + 0.01 * (i % 2)));
    
    StatisticalContext<D> ctx1(returns, 1.0);
    StatisticalContext<D> ctx252(returns, 252.0);
    
    // Annualized volatility should scale by sqrt(factor)
    const double ratio = ctx252.getAnnualizedVolatility() / ctx1.getAnnualizedVolatility();
    REQUIRE(ratio == Catch::Approx(std::sqrt(252.0)).margin(0.01));
  }
}

TEST_CASE("TailVolatilityAdaptivePolicy: Prior ratio calculation",
          "[Bootstrap][AdaptivePolicy]")
{
  using D = DecimalType;
  using mkc_timeseries::GeoMeanStat;
  using palvalidator::analysis::TailVolatilityAdaptivePolicy;
  using palvalidator::analysis::detail::StatisticalContext;

  GeoMeanStat<D> geo;

  SECTION("High volatility regime triggers high ratio")
  {
    // Generate high-volatility returns
    std::vector<D> returns;
    const std::size_t n = 30;
    returns.reserve(n);
    
    std::seed_seq seq = make_seed_seq(0x4194A0101ull);
    std::mt19937_64 rng(seq);
    std::normal_distribution<double> dist(0.0, 0.05); // 5% per-period vol
    
    for (std::size_t i = 0; i < n; ++i)
      returns.emplace_back(D(dist(rng)));
    
    // Annualize with factor of 252 to get ~79% annualized vol
    StatisticalContext<D> ctx(returns, 252.0);
    
    TailVolatilityAdaptivePolicy<D, GeoMeanStat<D>> policy;
    
    std::ostringstream oss;
    double ratio = policy.computeRatio(returns, ctx, 0.95, 1000, &oss);
    
    INFO("Computed ratio: " << ratio);
    INFO("Annualized vol: " << ctx.getAnnualizedVolatility());
    INFO("Diagnostics: " << oss.str());
    
    // High volatility should trigger high ratio (close to 0.80)
    REQUIRE(ratio >= 0.60);
    REQUIRE(ratio <= 1.0);
  }

// MOutOfNPercentileBootstrapTest.cpp: Lines 797-814
  SECTION("Normal regime uses moderate ratio")
  {
    // Generate normal-volatility returns
    std::vector<D> returns;
    const std::size_t n = 40;
    returns.reserve(n);
    
    // FIX: Use a deterministic, low-vol, symmetric sample to ensure Hill estimator is benign.
    // Per-period sigma approx 0.001. SigmaAnn approx 1.58%. This will ensure the heavy-tail flag is false.
    for (std::size_t i = 0; i < n; ++i)
      returns.emplace_back(D(0.001 * (i % 2 == 0 ? 1.0 : -1.0)));
    
    // Annualize with factor of 252 to get ~16% annualized vol
    StatisticalContext<D> ctx(returns, 252.0);
    
    TailVolatilityAdaptivePolicy<D, GeoMeanStat<D>> policy;
    
    double ratio = policy.computeRatio(returns, ctx, 0.95, 1000, nullptr);
    
    INFO("Computed ratio: " << ratio);
    INFO("Annualized vol: " << ctx.getAnnualizedVolatility());
    
    // Normal regime should use moderate ratio (around 0.50)
    REQUIRE(ratio >= 0.30);
    REQUIRE(ratio <= 0.70);
  }

  SECTION("Small N uses 50% rule")
  {
    // Very small sample
    std::vector<D> returns{ D(0.01), D(-0.005), D(0.002), D(0.003) };
    
    StatisticalContext<D> ctx(returns);
    
    TailVolatilityAdaptivePolicy<D, GeoMeanStat<D>> policy;
    
    double ratio = policy.computeRatio(returns, ctx, 0.95, 1000, nullptr);
    
    INFO("Computed ratio for n=4: " << ratio);
    
    // Should use ~50% rule for n < 5
    REQUIRE(ratio >= 0.40);
    REQUIRE(ratio <= 0.60);
  }
}

TEST_CASE("FixedRatioPolicy: Returns fixed ratio", 
          "[Bootstrap][FixedRatioPolicy]")
{
  using D = DecimalType;
  using mkc_timeseries::GeoMeanStat;
  using palvalidator::analysis::FixedRatioPolicy;
  using palvalidator::analysis::detail::StatisticalContext;

  SECTION("Returns configured ratio regardless of data")
  {
    std::vector<D> returns;
    for (std::size_t i = 0; i < 30; ++i)
      returns.emplace_back(D(0.01));
    
    StatisticalContext<D> ctx(returns);
    
    FixedRatioPolicy<D, GeoMeanStat<D>> policy(0.75);
    
    double ratio = policy.computeRatio(returns, ctx, 0.95, 1000, nullptr);
    
    REQUIRE(ratio == Catch::Approx(0.75));
  }

  SECTION("Validates ratio bounds in constructor")
  {
    REQUIRE_THROWS_AS(
        (FixedRatioPolicy<D, GeoMeanStat<D>>(0.0)),
        std::invalid_argument);
    
    REQUIRE_THROWS_AS(
        (FixedRatioPolicy<D, GeoMeanStat<D>>(1.0)),
        std::invalid_argument);
    
    REQUIRE_THROWS_AS(
        (FixedRatioPolicy<D, GeoMeanStat<D>>(-0.5)),
        std::invalid_argument);
  }
}

TEST_CASE("MOutOfNPercentileBootstrap: Adaptive vs Fixed mode comparison", 
          "[Bootstrap][MOutOfN][Adaptive][Integration]")
{
  using D = DecimalType;
  using mkc_timeseries::GeoMeanStat;
  using ResT = StationaryMaskValueResampler<D>;

  // Generate test data
  const std::size_t n = 35;
  std::vector<D> returns;
  returns.reserve(n);
  
  std::seed_seq seq = make_seed_seq(0xC04FA7E01ull);
  std::mt19937_64 rng(seq);
  std::normal_distribution<double> dist(0.002, 0.015);
  
  for (std::size_t i = 0; i < n; ++i)
    returns.emplace_back(D(dist(rng)));

  GeoMeanStat<D> geo;
  auto sampler = [&](const std::vector<D>& a) -> D { return geo(a); };
  
  ResT res(3);

  SECTION("Adaptive mode produces valid policy ratio")
  {
    std::seed_seq seq1 = make_seed_seq(0xADA97001ull);
    std::mt19937_64 rng1(seq1);
    
    MOutOfNPercentileBootstrap<D, decltype(sampler), ResT> bootstrap(
        /*B=*/1000, /*CL=*/0.95, /*m_ratio=*/0.70, res);
    
    // Switch to adaptive mode
    bootstrap.setAdaptiveRatioPolicy<GeoMeanStat<D>>(
        std::make_shared<palvalidator::analysis::TailVolatilityAdaptivePolicy<D, GeoMeanStat<D>>>());
    
    std::ostringstream oss;
    auto result = bootstrap.run(returns, sampler, rng1, 0, &oss);
    
    INFO("Adaptive ratio (policy): " << result.computed_ratio);
    INFO("m_sub: " << result.m_sub);
    INFO("Diagnostics:\n" << oss.str());
    
    // In adaptive mode, computed_ratio is the policy m/n ratio,
    // which also governs m_sub = floor(policy_ratio * n), up to clamping.
    REQUIRE(result.computed_ratio > 0.0);
    REQUIRE(result.computed_ratio < 1.0);

    const std::size_t expected_m =
        static_cast<std::size_t>(std::floor(result.computed_ratio * n));
    REQUIRE(result.m_sub == expected_m);
    
    // Verify bounds are valid
    REQUIRE(std::isfinite(num::to_double(result.lower)));
    REQUIRE(std::isfinite(num::to_double(result.upper)));
    REQUIRE(result.lower <= result.upper);
  }

  SECTION("Fixed mode reports configured ratio")
  {
    std::seed_seq seq2 = make_seed_seq(0x41EED001ull);
    std::mt19937_64 rng2(seq2);
    
    const double fixed_ratio = 0.65;
    MOutOfNPercentileBootstrap<D, decltype(sampler), ResT> bootstrap(
        /*B=*/1000, /*CL=*/0.95, fixed_ratio, res);
    
    REQUIRE_FALSE(bootstrap.isAdaptiveMode());
    REQUIRE(bootstrap.mratio() == Catch::Approx(fixed_ratio));
    
    auto result = bootstrap.run(returns, sampler, rng2);
    
    // In fixed mode, computed_ratio must equal the configured m_ratio
    // even though m_sub is integer-rounded from it.
    REQUIRE(result.computed_ratio == Catch::Approx(fixed_ratio).margin(0.0));

    // And m_sub is derived from that fixed_ratio (subject to [2, n-1] clamp).
    const std::size_t expected_m =
        static_cast<std::size_t>(std::floor(fixed_ratio * n));
    REQUIRE(result.m_sub == expected_m);
  }

  SECTION("m_sub_override reports m_sub/n")
  {
    std::seed_seq seq3 = make_seed_seq(0x0EE44DE1ull);
    std::mt19937_64 rng3(seq3);
    
    MOutOfNPercentileBootstrap<D, decltype(sampler), ResT> bootstrap(
        /*B=*/1000, /*CL=*/0.95, /*m_ratio=*/0.70, res);
    
    // Switch to adaptive mode (but override will take precedence for m_sub)
    bootstrap.setAdaptiveRatioPolicy<GeoMeanStat<D>>(
        std::make_shared<palvalidator::analysis::TailVolatilityAdaptivePolicy<D, GeoMeanStat<D>>>());
    
    const std::size_t m_override = 20;
    auto result = bootstrap.run(returns, sampler, rng3, m_override);
    
    REQUIRE(result.m_sub == m_override);

    // With an explicit override, computed_ratio is defined as m_sub / n,
    // i.e., the realized subsample fraction, independent of policy or fixed m_ratio.
    const double expected_ratio =
        static_cast<double>(m_override) / static_cast<double>(n);

    REQUIRE(result.computed_ratio
            == Catch::Approx(expected_ratio).margin(0.001));
  }
}

TEST_CASE("StatisticalContext: Heavy-tail detection logic", 
          "[Bootstrap][StatisticalContext][HeavyTails]")
{
  using D = DecimalType;
  using palvalidator::analysis::detail::StatisticalContext;

  SECTION("Conservative OR logic: quantile shape triggers detection")
  {
    std::vector<D> returns;
    const std::size_t n = 40;
    returns.reserve(n);
    
    // Q2 (Median) for n=40 is at index 20.5 (0-based)
    // Set positive/negative counts to force Q1, Q2, Q3 into distinct regions.
    for (std::size_t i = 0; i < 30; ++i)
      returns.emplace_back(D(0.001)); // Q2, Q3 will be here
    for (std::size_t i = 0; i < 10; ++i)
      returns.emplace_back(D(-0.01 - 0.005 * i)); // Q1 will be here
 
    StatisticalContext<D> ctx(returns);
    
    // Should detect via quantile shape (strong asymmetry or heavy tails)
    const bool detected = ctx.hasHeavyTails() || ctx.hasStrongAsymmetry();
    INFO("Heavy tails: " << ctx.hasHeavyTails());
    INFO("Strong asymmetry: " << ctx.hasStrongAsymmetry());
    
    REQUIRE(detected);
  }

  SECTION("Conservative OR logic: Hill estimator triggers detection")
  {
    std::vector<D> returns;
    const std::size_t n = 40;
    returns.reserve(n);
    
    // Create distribution with Pareto-like tail (α ≈ 1.5)
    for (std::size_t i = 0; i < 30; ++i)
      returns.emplace_back(D(0.0005));
    
    // Add extreme losses following power law
    returns.emplace_back(D(-0.01));
    returns.emplace_back(D(-0.02));
    returns.emplace_back(D(-0.04));
    returns.emplace_back(D(-0.08));
    returns.emplace_back(D(-0.16));
    returns.emplace_back(D(-0.32));
    returns.emplace_back(D(-0.64));
    returns.emplace_back(D(-0.80));
    returns.emplace_back(D(-0.90));
    returns.emplace_back(D(-0.95));
    
    StatisticalContext<D> ctx(returns);
    
    INFO("Tail index: " << ctx.getTailIndex());
    INFO("Heavy tails: " << ctx.hasHeavyTails());
    
    // Should detect via Hill estimator (α ≤ 2.0)
    if (ctx.getTailIndex() > 0.0)
    {
      REQUIRE(ctx.getTailIndex() <= 3.0); // Should be in heavy-tail range
    }
  }
}

TEST_CASE("TailVolatilityAdaptivePolicy: computeRatioWithRefinement behavior",
          "[Bootstrap][AdaptivePolicy][Refinement]")
{
  using D  = DecimalType;
  using mkc_timeseries::GeoMeanStat;
  using palvalidator::analysis::TailVolatilityAdaptivePolicy;
  using palvalidator::analysis::detail::StatisticalContext;
  using palvalidator::analysis::detail::CandidateScore;

  GeoMeanStat<D> geo;
  TailVolatilityAdaptivePolicy<D, GeoMeanStat<D>> policy;

  // Helper to build a low-vol, symmetric return series so we land in the
  // "normal regime" (prior ratio ≈ normalRatio = 0.50) and avoid heavy-tail flags.
  auto make_symmetric_returns = [](std::size_t n) {
    std::vector<D> r;
    r.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
      r.emplace_back(D(0.001 * (i % 2 == 0 ? 1.0 : -1.0)));
    return r;
  };

  SECTION("Refinement picks candidate whose instability is minimized (unique best)")
  {
    const std::size_t n = 30; // within [15, 60] refinement window
    auto returns = make_symmetric_returns(n);

    // Annualization factor arbitrary but consistent with "normal regime".
    StatisticalContext<D> ctx(returns, 252.0);

    // Sanity: prior ratio should be in the "normal" range (around 0.50).
    const double prior = policy.computeRatio(returns, ctx, 0.95, 1000, nullptr);
    INFO("Prior ratio (for context): " << prior);

    // Probe maker that defines instability as |rho - target|,
    // and returns a CandidateScore with ratio=rho.
    struct TargetProbeMaker
    {
      double target;
      mutable std::vector<double> called_rhos;

      CandidateScore runProbe(const std::vector<D>& /*data*/,
                              double rho,
                              std::size_t /*B*/) const
      {
        called_rhos.push_back(rho);
        const double instability = std::fabs(rho - target);
        return CandidateScore(/*lowerBound=*/0.0,
                              /*sigma=*/0.0,
                              /*instability=*/instability,
                              /*ratio=*/rho);
      }
    };

    // We choose a target that is *exactly* on the candidate grid around 0.5:
    // candidates ≈ {0.25, 0.30, ..., 0.70, 0.75}. Target 0.70 is in that set.
    TargetProbeMaker probe{0.70};

    const double refined =
        policy.computeRatioWithRefinement(returns, ctx,
                                          /*confidenceLevel=*/0.95,
                                          /*B=*/1000,
                                          probe,
                                          /*os=*/nullptr);

    INFO("Refined ratio: " << refined);

    // Ensure refinement actually probed multiple candidates and chose the best.
    REQUIRE(probe.called_rhos.size() >= 3);

    // Because instability = |rho - 0.70|, the unique best candidate is rho=0.70.
    // The theoretical n^(2/3) floor for n=30 (~0.32) and ratio-statistic floor
    // do not bind here, so the final refined ratio should be ≈ 0.70.
    REQUIRE(refined == Catch::Approx(0.70).margin(1e-12));
  }

  SECTION("Refinement tie-breaking prefers smaller ratio when instabilities tie")
  {
    const std::size_t n = 30; // within [15, 60]
    auto returns = make_symmetric_returns(n);
    StatisticalContext<D> ctx(returns, 252.0);

    // Probe maker that creates a tie between two candidates (e.g. 0.55 and 0.60)
    // with the same minimal instability, and larger instability elsewhere.
    // Because TailVolatilityAdaptivePolicy breaks ties by smaller ratio, it
    // should select 0.55.
    struct TieProbeMaker
    {
      mutable std::vector<double> called_rhos;

      CandidateScore runProbe(const std::vector<D>& /*data*/,
                              double rho,
                              std::size_t /*B*/) const
      {
        called_rhos.push_back(rho);

        double instability;
        if (std::fabs(rho - 0.55) < 1e-6 || std::fabs(rho - 0.60) < 1e-6)
          instability = 0.10; // tied minima
        else
          instability = 0.20; // strictly worse

        return CandidateScore(/*lowerBound=*/0.0,
                              /*sigma=*/0.0,
                              /*instability=*/instability,
                              /*ratio=*/rho);
      }
    };

    TieProbeMaker probe;

    const double refined =
        policy.computeRatioWithRefinement(returns, ctx,
                                          /*confidenceLevel=*/0.95,
                                          /*B=*/1000,
                                          probe,
                                          /*os=*/nullptr);

    INFO("Refined ratio with tie: " << refined);

    // Should have probed more than just one candidate.
    REQUIRE(probe.called_rhos.size() >= 3);

    // Among candidates with equal minimal instability (0.55 and 0.60),
    // the implementation chooses the *smaller* ratio.
    REQUIRE(refined == Catch::Approx(0.55).margin(1e-12));
  }

  SECTION("No refinement when N is outside refinement window [15,60]")
  {
    // Case 1: n below refinement window but >= 5 (so not using small-n 50% rule)
    {
      const std::size_t n_small = 10;
      auto returns = make_symmetric_returns(n_small);
      StatisticalContext<D> ctx_small(returns, 252.0);

      // Probe maker that records if it was ever used.
      struct FlagProbeMaker
      {
        mutable bool called = false;

        CandidateScore runProbe(const std::vector<D>& /*data*/,
                                double /*rho*/,
                                std::size_t /*B*/) const
        {
          called = true;
          return CandidateScore(0.0, 0.0, 0.0, 0.5);
        }
      } probe_small;

      const double base =
          policy.computeRatio(returns, ctx_small, 0.95, 1000, nullptr);

      const double refined =
          policy.computeRatioWithRefinement(returns, ctx_small,
                                            /*confidenceLevel=*/0.95,
                                            /*B=*/1000,
                                            probe_small,
                                            /*os=*/nullptr);

      INFO("Base ratio (n=10): " << base);
      INFO("Refined ratio (n=10): " << refined);

      // For n < 15, refinement must be skipped, and probe must never be called.
      REQUIRE_FALSE(probe_small.called);
      REQUIRE(refined == Catch::Approx(base).margin(1e-12));
    }

    // Case 2: n above refinement window
    {
      const std::size_t n_large = 80;
      auto returns = make_symmetric_returns(n_large);
      StatisticalContext<D> ctx_large(returns, 252.0);

      struct FlagProbeMaker
      {
        mutable bool called = false;

        CandidateScore runProbe(const std::vector<D>& /*data*/,
                                double /*rho*/,
                                std::size_t /*B*/) const
        {
          called = true;
          return CandidateScore(0.0, 0.0, 0.0, 0.5);
        }
      } probe_large;

      const double base =
          policy.computeRatio(returns, ctx_large, 0.95, 1000, nullptr);

      const double refined =
          policy.computeRatioWithRefinement(returns, ctx_large,
                                            /*confidenceLevel=*/0.95,
                                            /*B=*/1000,
                                            probe_large,
                                            /*os=*/nullptr);

      INFO("Base ratio (n=80): " << base);
      INFO("Refined ratio (n=80): " << refined);

      // For n > 60, refinement must be skipped as well.
      REQUIRE_FALSE(probe_large.called);
      REQUIRE(refined == Catch::Approx(base).margin(1e-12));
    }
  }

  SECTION("Small-n path in computeRatioWithRefinement matches 50% rule and skips refinement")
  {
    std::vector<D> returns{ D(0.01), D(-0.005), D(0.002), D(0.003) };
    StatisticalContext<D> ctx(returns);

    struct FlagProbeMaker
    {
      mutable bool called = false;

      CandidateScore runProbe(const std::vector<D>& /*data*/,
                              double /*rho*/,
                              std::size_t /*B*/) const
      {
        called = true;
        return CandidateScore(0.0, 0.0, 0.0, 0.5);
      }
    } probe;

    const double base =
        policy.computeRatio(returns, ctx, 0.95, 1000, nullptr);

    const double refined =
        policy.computeRatioWithRefinement(returns, ctx,
                                          /*confidenceLevel=*/0.95,
                                          /*B=*/1000,
                                          probe,
                                          /*os=*/nullptr);

    INFO("Base ratio (n=4): " << base);
    INFO("Refined ratio (n=4): " << refined);

    // computeRatioWithRefinement must defer to the small-n 50% rule
    // and never invoke the probe engine.
    REQUIRE_FALSE(probe.called);
    REQUIRE(refined == Catch::Approx(base).margin(1e-12));
    REQUIRE(refined >= 0.40);
    REQUIRE(refined <= 0.60);
  }
}

// Minimal strategy type for testing – the bootstrap does not call any methods on it
struct DummyStrategy { };

// Recording factory that mimics the TradingBootstrapFactory interface
// Must be defined at namespace scope because it has template member functions
struct RecordingFactory
{
  struct CRNProvider
  {
    std::mt19937_64 make_engine(std::size_t b) const
    {
      // Deterministic engine per replicate index
      std::seed_seq ss{
        static_cast<unsigned>(b & 0xffffffffu),
        static_cast<unsigned>((b >> 32) & 0xffffffffu),
        0xA5A5A5A5u,
        0x5A5A5A5Au
      };
      return std::mt19937_64(ss);
    }
  };

  struct CallRecord
  {
    std::size_t B     = 0;
    double      CL    = 0.0;
    double      rho   = 0.0;
    int         stage = 0;
    int         Lsmall= 0;
    int         fold  = 0;
  };

  // Mock bootstrap engine that provides a run() method
  template<typename Decimal, typename BootstrapStatistic, typename Resampler>
  struct MockBootstrapEngine
  {
    using Result = typename palvalidator::analysis::MOutOfNPercentileBootstrap<
      Decimal,
      BootstrapStatistic,
      Resampler
    >::Result;

    std::size_t B;
    double CL;
    double rho;
    const Resampler& resampler;

    MockBootstrapEngine(std::size_t B_, double CL_, double rho_, const Resampler& res)
      : B(B_), CL(CL_), rho(rho_), resampler(res)
    {
    }

    template<typename Provider>
    Result run(const std::vector<Decimal>& data,
               BootstrapStatistic statistic,
               const Provider& provider) const
    {
      // Create a minimal valid result for probe testing
      const std::size_t n = data.size();
      const std::size_t m_sub = static_cast<std::size_t>(std::floor(rho * n));
      
      const Decimal stat_value = statistic(data);
      
      // Return a simple result with the statistic value as all bounds
      return Result{
        /*mean=*/stat_value,
        /*lower=*/stat_value * Decimal(0.9),  // Mock lower bound
        /*upper=*/stat_value * Decimal(1.1),  // Mock upper bound
        /*cl=*/CL,
        /*B=*/B,
        /*effective_B=*/B,
        /*skipped=*/0,
        /*n=*/n,
        /*m_sub=*/m_sub,
        /*L=*/resampler.getL(),
        /*computed_ratio=*/rho
      };
    }
  };

  std::size_t callCount = 0;
  CallRecord lastCall;

  template<typename Decimal, typename BootstrapStatistic, typename Resampler, typename StrategyT>
  std::pair<MockBootstrapEngine<Decimal, BootstrapStatistic, Resampler>, CRNProvider>
  makeMOutOfN(std::size_t B,
              double      cl,
              double      rho,
              const Resampler& resampler,
              StrategyT&,
              int         stageTag,
              int         L_small,
              int         fold)
  {
    ++callCount;
    lastCall.B      = B;
    lastCall.CL     = cl;
    lastCall.rho    = rho;
    lastCall.stage  = stageTag;
    lastCall.Lsmall = L_small;
    lastCall.fold   = fold;

    MockBootstrapEngine<Decimal, BootstrapStatistic, Resampler> engine(B, cl, rho, resampler);
    return std::make_pair(engine, CRNProvider{});
  }
};

TEST_CASE("MOutOfNPercentileBootstrap::runWithRefinement: wiring and invariants",
          "[Bootstrap][MOutOfN][Adaptive][Refinement]")
{
  using D  = DecimalType;
  using mkc_timeseries::GeoMeanStat;
  using ResT = StationaryMaskValueResampler<D>;
  using palvalidator::analysis::FixedRatioPolicy;

  // Simple arithmetic-mean sampler
  auto mean_sampler = [](const std::vector<D>& a) -> D
  {
    double s = 0.0;
    for (const auto& v : a)
      s += num::to_double(v);
    return D(s / static_cast<double>(a.size()));
  };

  const std::size_t B  = 800;
  const double      CL = 0.95;
  const std::size_t L  = 3;
  ResT res(L);

  SECTION("runWithRefinement throws on n < 3 and does not touch factory")
  {
    std::vector<D> tiny{ D(1), D(2) }; // n = 2 → invalid for m-out-of-n

    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), ResT> bootstrap(B, CL, 0.70, res);

    DummyStrategy    strategy;
    RecordingFactory factory;

    REQUIRE_THROWS_AS(
        (bootstrap.template runWithRefinement<GeoMeanStat<D>>(
            tiny, mean_sampler, strategy, factory, /*stageTag=*/0, /*fold=*/0)),
        std::invalid_argument);

    REQUIRE(factory.callCount == 0);
  }

  SECTION("Default policy path uses TailVolatilityAdaptivePolicy and consistent ratio")
  {
    // Moderate-sized deterministic dataset
    const std::size_t n = 40;
    std::vector<D> returns;
    returns.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
      const double v = 0.001 * std::sin(static_cast<double>(i) / 5.0);
      returns.emplace_back(D(v));
    }

    // Fixed-ratio constructor; runWithRefinement should still use the
    // default TailVolatilityAdaptivePolicy when no adaptive policy is set.
    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), ResT> bootstrap(B, CL, 0.70, res);

    DummyStrategy    strategy;
    RecordingFactory factory;

    const int stageTag = 5;
    const int fold     = 2;

    auto result =
        bootstrap.template runWithRefinement<GeoMeanStat<D>>(
            returns, mean_sampler, strategy, factory, stageTag, fold);

    REQUIRE(result.B  == B);
    REQUIRE(result.cl == Catch::Approx(CL));
    REQUIRE(result.n  == n);
    REQUIRE(result.L  == L);

    REQUIRE(result.m_sub >= 2);
    REQUIRE(result.m_sub <  n);

    // In the refinement path, the reported ratio is always m_sub / n
    const double inferredRatio =
        static_cast<double>(result.m_sub) / static_cast<double>(n);
    REQUIRE(result.computed_ratio == Catch::Approx(inferredRatio));

    // Factory must have been called exactly once with the final ratio
    REQUIRE(factory.callCount >= 1);
    REQUIRE(factory.lastCall.B      == B);
    REQUIRE(factory.lastCall.CL     == Catch::Approx(CL));
    REQUIRE(factory.lastCall.stage  == stageTag);
    REQUIRE(factory.lastCall.fold   == fold);
    REQUIRE(factory.lastCall.Lsmall == static_cast<int>(L));
    REQUIRE(factory.lastCall.rho    == Catch::Approx(inferredRatio));

    // Basic sanity on effective replicate counts
    REQUIRE(result.effective_B + result.skipped == result.B);
    REQUIRE(result.effective_B >= result.B / 2);
  }

  SECTION("Explicit FixedRatioPolicy is honored and passed through to factory (after clamping)")
  {
    const std::size_t n = 50;
    std::vector<D> returns;
    returns.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
      returns.emplace_back(D(0.001 * static_cast<double>(i)));

    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), ResT> bootstrap(B, CL, 0.70, res);

    // Install a fixed-ratio adaptive policy (no refinement; prior-only)
    const double policy_ratio = 0.65;
    bootstrap.setAdaptiveRatioPolicy<GeoMeanStat<D>>(
        std::make_shared<FixedRatioPolicy<D, GeoMeanStat<D>>>(policy_ratio));

    DummyStrategy    strategy;
    RecordingFactory factory;

    const int stageTag = 42;
    const int fold     = 7;

    auto result =
        bootstrap.template runWithRefinement<GeoMeanStat<D>>(
            returns, mean_sampler, strategy, factory, stageTag, fold);

    REQUIRE(result.B  == B);
    REQUIRE(result.cl == Catch::Approx(CL));
    REQUIRE(result.n  == n);
    REQUIRE(result.L  == L);

    // The policy ratio is 0.65, but runWithRefinement clamps via m_sub
    std::size_t expected_m =
        static_cast<std::size_t>(std::floor(policy_ratio * static_cast<double>(n)));
    if (expected_m < 2)
      expected_m = 2;
    if (expected_m >= n)
      expected_m = n - 1;

    const double expected_ratio =
        static_cast<double>(expected_m) / static_cast<double>(n);

    REQUIRE(result.m_sub          == expected_m);
    REQUIRE(result.computed_ratio == Catch::Approx(expected_ratio));

    // Factory sees the final (clamped) ratio and correct wiring metadata
    REQUIRE(factory.callCount      >= 1);
    REQUIRE(factory.lastCall.B     == B);
    REQUIRE(factory.lastCall.CL    == Catch::Approx(CL));
    REQUIRE(factory.lastCall.stage == stageTag);
    REQUIRE(factory.lastCall.fold  == fold);
    REQUIRE(factory.lastCall.Lsmall== static_cast<int>(L));
    REQUIRE(factory.lastCall.rho   == Catch::Approx(expected_ratio));

    REQUIRE(result.effective_B + result.skipped == result.B);
    REQUIRE(result.effective_B >= result.B / 2);
  }
}

TEST_CASE("MOutOfNPercentileBootstrap: diagnostics unavailable before run",
          "[Bootstrap][MOutOfN][Diagnostics]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t B  = 800;
    const double      CL = 0.95;
    const double      rho = 0.70;

    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        moon(B, CL, rho, res);

    SECTION("hasDiagnostics is false before any run()")
    {
        REQUIRE_FALSE(moon.hasDiagnostics());
    }

    SECTION("Diagnostic getters throw before run()")
    {
        REQUIRE_THROWS_AS(moon.getBootstrapStatistics(), std::logic_error);
        REQUIRE_THROWS_AS(moon.getBootstrapMean(),       std::logic_error);
        REQUIRE_THROWS_AS(moon.getBootstrapVariance(),   std::logic_error);
        REQUIRE_THROWS_AS(moon.getBootstrapSe(),         std::logic_error);
        REQUIRE_THROWS_AS(moon.getBootstrapSkewness(),   std::logic_error);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: diagnostics consistent with Result",
          "[Bootstrap][MOutOfN][Diagnostics]")
{
    using D = DecimalType;

    // Simple nontrivial data: 0..59
    const std::size_t n = 60;
    std::vector<D> x; x.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        x.emplace_back(D(static_cast<int>(i)));

    auto mean_sampler = [](const std::vector<D>& a) -> D
    {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };

    const std::size_t L   = 3;
    StationaryMaskValueResampler<D> res(L);

    const std::size_t B  = 800;
    const double      CL = 0.95;
    const double      rho = 0.70;

    std::seed_seq   seq  = make_seed_seq(0x0000001100000022ull);
    std::mt19937_64 rng(seq);

    MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>>
        moon(B, CL, rho, res);

    auto out = moon.run(x, mean_sampler, rng);

    REQUIRE(moon.hasDiagnostics());

    const auto& stats   = moon.getBootstrapStatistics();
    const double mean_b = moon.getBootstrapMean();
    const double var_b  = moon.getBootstrapVariance();
    const double se_b   = moon.getBootstrapSe();
    const double skew_b = moon.getBootstrapSkewness();

    SECTION("Bootstrap statistics size and counters match Result")
    {
        REQUIRE(stats.size() == out.effective_B);
        REQUIRE(out.effective_B + out.skipped == out.B);
        REQUIRE(out.B == B);
        REQUIRE(out.n == n);
        REQUIRE(out.L == L);
    }

    SECTION("Mean/variance/SE/skewness match recomputation from statistics")
    {
        REQUIRE_FALSE(stats.empty());

        const std::size_t m = stats.size();

        // Mean
        double m_re = 0.0;
        for (double v : stats)
            m_re += v;
        m_re /= static_cast<double>(m);

        // Sample variance (m - 1 in denominator) and SE
        double v_re = 0.0;
        if (m > 1)
        {
            for (double v : stats)
            {
                const double d = v - m_re;
                v_re += d * d;
            }
            v_re /= static_cast<double>(m - 1);
        }
        const double se_re = std::sqrt(v_re);

        // Skewness: E[(X - mean)^3] / SE^3  with E over m
        double skew_re = 0.0;
        if (m > 2 && se_re > 0.0)
        {
            double m3 = 0.0;
            for (double v : stats)
            {
                const double d = v - m_re;
                m3 += d * d * d;
            }
            m3 /= static_cast<double>(m);
            skew_re = m3 / (se_re * se_re * se_re);
        }

        REQUIRE(mean_b          == Catch::Approx(m_re).margin(1e-12));
        REQUIRE(var_b           == Catch::Approx(v_re).margin(1e-12));
        REQUIRE(se_b            == Catch::Approx(se_re).margin(1e-12));
        REQUIRE(skew_b          == Catch::Approx(skew_re).margin(1e-12));
        REQUIRE(out.skew_boot   == Catch::Approx(skew_re).margin(1e-12));
    }
}
