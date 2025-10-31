// BCaBootstrapAnnualizationIntegrationTest.cpp
//
// Integration test: real BCaBootStrap -> BCaAnnualizer
// Verifies that annualization preserves ordering and matches analytic transform.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <cmath>
#include <limits>

#include "number.h"
#include "StatUtils.h"
#include "randutils.hpp"
#include "BiasCorrectedBootstrap.h"

using DecimalType = num::DefaultNumber;

static inline double round_to_decimal8(double x)
{
    return std::round(x * 1e8) / 1e8;
}

static inline double annualize_expect(double r_per_period, long double K)
{
    // Compute (1+r)^K - 1 robustly in long double
    const long double g = std::log1pl(static_cast<long double>(r_per_period));
    const long double a = std::exp(K * g) - 1.0L;
    return static_cast<double>(a);
}

TEST_CASE("BCaBootStrap → BCaAnnualizer: ordering preserved and matches analytic", "[BCa][Annualizer][Integration]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;
    using mkc_timeseries::BCaBootStrap;
    using mkc_timeseries::StationaryBlockResampler;
    using mkc_timeseries::BCaAnnualizer;

    // Synthetic per-period returns (> -1), small positives with occasional mild negatives.
    const std::size_t n = 60;
    std::vector<D> r; r.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        // Around ~0.05% average with gentle oscillation and rare small negatives
        const double v = 0.0005
                       + 0.0003 * std::sin(static_cast<double>(i) / 7.0)
                       - 0.0004 * ((i % 11) == 0 ? 1.0 : 0.0);
        r.emplace_back(D(v));
    }

    // Statistic: GeoMeanStat (log-aware), with conservative guards
    GeoMeanStat<D> geo(/*clip_ruin=*/true,
                       /*winsor_small_n=*/true,
                       /*winsor_alpha=*/0.02,
                       /*ruin_eps=*/1e-8);

    auto sampler = [&](const std::vector<D>& a) -> D
    {
        return geo(a);
    };

    // Resampler: stationary blocks with mean block length L
    const std::size_t L = 4;
    StationaryBlockResampler<D> res(L);

    // Bootstrap config (keep modest for unit test stability)
    const std::size_t B = 1000;
    const double CL = 0.95;

    // Real BCa bootstrap object
    BCaBootStrap<D, StationaryBlockResampler<D>> bca(
        r, B, CL, sampler, res
    );

    // Let the annualizer pull results (it can trigger calculation internally)
    const long double K = 252.0L; // e.g., daily → ~252 periods/year
    BCaAnnualizer<D> ann(bca, static_cast<double>(K));

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

        // For returns strictly > -1, (1+r)^K - 1 > -1
        REQUIRE(lo_ann > -1.0);
    }

    SECTION("Annualizer matches analytic transform (rounded to decimal<8>)")
    {
        // Fetch the per-period CI the annualizer used (via the BCa object)
        const double lo = num::to_double(bca.getLowerBound());
        const double mu = num::to_double(bca.getMean());
        const double hi = num::to_double(bca.getUpperBound());

        // Analytic targets
        const double lo_exp = round_to_decimal8(annualize_expect(lo, K));
        const double mu_exp = round_to_decimal8(annualize_expect(mu, K));
        const double hi_exp = round_to_decimal8(annualize_expect(hi, K));

        REQUIRE(lo_ann == Catch::Approx(lo_exp).margin(1e-12));
        REQUIRE(mu_ann == Catch::Approx(mu_exp).margin(1e-12));
        REQUIRE(hi_ann == Catch::Approx(hi_exp).margin(1e-12));
    }

    SECTION("Larger K weakly increases annualized mean for small positive per-period return")
    {
        BCaAnnualizer<D> ann252(bca, 252.0);
        BCaAnnualizer<D> ann504(bca, 504.0);

        const double m252 = num::to_double(ann252.getAnnualizedMean());
        const double m504 = num::to_double(ann504.getAnnualizedMean());

        REQUIRE(m504 >= m252 - 1e-12);
    }
}
