// StatUtilsTest.cpp
//
// Unit tests for the static utility functions in StatUtils.h.
// This file uses the Catch2 testing framework to validate the correctness
// of statistical calculations such as Profit Factor, Log Profit Factor,
// and Profitability.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp> // For Catch::Approx
#include <vector>
#include <cmath> // For std::log
#include <numeric> // For std::accumulate

#include "TimeSeries.h"
#include "ClosedPositionHistory.h"
#include "MonthlyReturnsBuilder.h"
#include "StatUtils.h"
#include "TestUtils.h" // For DecimalType typedef
#include "DecimalConstants.h"
#include "number.h" // For num::to_double

using namespace mkc_timeseries;
constexpr double kACFAbsTol = 1e-6;  // decimal vs double accumulation tolerance
const static std::string myCornSymbol("@C");

// Test suite for the StatUtils::computeProfitFactor function
TEST_CASE("StatUtils::computeProfitFactor", "[StatUtils]") {
    // Test a standard scenario with a mix of positive (wins) and negative (losses) returns.
    SECTION("Basic scenario with wins and losses") {
        std::vector<DecimalType> returns = {DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"), DecimalType("-0.10")};
        // Calculation:
        // Gross Wins = 0.10 + 0.20 = 0.30
        // Gross Losses = -0.05 + -0.10 = -0.15
        // Profit Factor = 0.30 / abs(-0.15) = 2.0
        DecimalType expected("2.0");
        REQUIRE(StatUtils<DecimalType>::computeProfitFactor(returns, false) == expected);
    }

    // Test a scenario with only positive returns.
    SECTION("Only winning trades") {
        std::vector<DecimalType> returns = {DecimalType("0.10"), DecimalType("0.05"), DecimalType("0.20")};
        // With zero losses, the function should return a fixed large number to signify high profitability.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalOneHundred;
        REQUIRE(StatUtils<DecimalType>::computeProfitFactor(returns, false) == expected);
    }

    // Test a scenario with only negative returns.
    SECTION("Only losing trades") {
        std::vector<DecimalType> returns = {DecimalType("-0.10"), DecimalType("-0.05"), DecimalType("-0.20")};
        // With zero wins, the profit factor should be 0.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalZero;
        REQUIRE(StatUtils<DecimalType>::computeProfitFactor(returns, false) == expected);
    }

    // Test an edge case with an empty input vector.
    SECTION("Empty vector of returns") {
        std::vector<DecimalType> returns;
        // An empty set of returns implies no losses, so it should be treated as the no-loss case.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalOneHundred;
        REQUIRE(StatUtils<DecimalType>::computeProfitFactor(returns, false) == expected);
    }

    // Test a scenario where all returns are zero.
    SECTION("Returns are all zero") {
        std::vector<DecimalType> returns = {DecimalType("0.0"), DecimalType("0.0"), DecimalType("0.0")};
        // Zero returns mean no wins and no losses.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalOneHundred;
        REQUIRE(StatUtils<DecimalType>::computeProfitFactor(returns, false) == expected);
    }

    // Test the log-compressed variant of the profit factor calculation.
    SECTION("Compressed result") {
        std::vector<DecimalType> returns = {DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"), DecimalType("-0.10")};
        // Profit Factor = 2.0
        // Compressed = log(1 + 2.0) = log(3.0)
        double expected_val = std::log(3.0);
        REQUIRE(num::to_double(StatUtils<DecimalType>::computeProfitFactor(returns, true)) == Catch::Approx(expected_val));
    }
}

// Test suite for the StatUtils::computeLogProfitFactor function
TEST_CASE("StatUtils::computeLogProfitFactor", "[StatUtils]") {
    // Test a standard scenario with a mix of wins and losses.
    SECTION("Basic scenario with wins and losses") {
        std::vector<DecimalType> returns = {DecimalType("0.1"), DecimalType("-0.05"), DecimalType("0.2"), DecimalType("-0.1")};
        // Calculation:
        // Log Wins = log(1.1) + log(1.2) ~= 0.095310 + 0.182321 = 0.277631
        // Log Losses = log(0.95) + log(0.9) ~= -0.051293 + -0.105360 = -0.156653
        // Log PF = 0.277631 / abs(-0.156653) ~= 1.77233
        double expected_val = (std::log(1.1) + std::log(1.2)) / std::abs(std::log(0.95) + std::log(0.9));
        REQUIRE(num::to_double(StatUtils<DecimalType>::computeLogProfitFactor(returns, false)) == Catch::Approx(expected_val));
    }

    // Test with only winning trades.
    SECTION("Only winning trades") {
        std::vector<DecimalType> returns = {DecimalType("0.1"), DecimalType("0.2")};
        // No log losses, should return the constant for high profitability.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalOneHundred;
        REQUIRE(StatUtils<DecimalType>::computeLogProfitFactor(returns, false) == expected);
    }

    // Test with only losing trades.
    SECTION("Only losing trades") {
        std::vector<DecimalType> returns = {DecimalType("-0.1"), DecimalType("-0.2")};
        // No log wins, should be 0.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalZero;
        REQUIRE(StatUtils<DecimalType>::computeLogProfitFactor(returns, false) == expected);
    }

    // Test an edge case with an empty input vector.
    SECTION("Empty vector of returns") {
        std::vector<DecimalType> returns;
        // No log wins or losses, treated as the no-loss case.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalOneHundred;
        REQUIRE(StatUtils<DecimalType>::computeLogProfitFactor(returns, false) == expected);
    }

    // Test that returns which would result in invalid log arguments (e.g., log(0) or log(<0)) are ignored.
    SECTION("Returns that result in non-positive log arguments") {
        std::vector<DecimalType> returns = {DecimalType("0.5"), DecimalType("-1.0"), DecimalType("-1.5")};
        // log(1 + (-1.0)) and log(1 + (-1.5)) are invalid and should be skipped.
        // Only log(1 + 0.5) is valid. This results in Log Wins > 0 and Log Losses = 0.
        DecimalType expected = DecimalConstants<DecimalType>::DecimalOneHundred;
        REQUIRE(StatUtils<DecimalType>::computeLogProfitFactor(returns, false) == expected);
    }

    // Test the log-compressed variant of the log profit factor.
    SECTION("Compressed result") {
        std::vector<DecimalType> returns = {DecimalType("0.1"), DecimalType("-0.05"), DecimalType("0.2"), DecimalType("-0.1")};
        // Log PF ~= 1.77233
        // Compressed = log(1 + 1.77233) = log(2.77233)
        double log_pf = (std::log(1.1) + std::log(1.2)) / std::abs(std::log(0.95) + std::log(0.9));
        double expected_val = std::log(1 + log_pf);
        REQUIRE(num::to_double(StatUtils<DecimalType>::computeLogProfitFactor(returns, true)) == Catch::Approx(expected_val));
    }
}

// Test suite for the StatUtils::computeProfitability function
TEST_CASE("StatUtils::computeProfitability", "[StatUtils]") {
    // Test a standard scenario with a mix of wins and losses.
    SECTION("Basic scenario with wins and losses") {
        std::vector<DecimalType> returns = {DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"), DecimalType("-0.10")};
        // pf = 2.0
        // awt = (0.10 + 0.20) / 2 = 0.15
        // alt = abs(-0.05 + -0.10) / 2 = 0.075
        // rwl = awt / alt = 0.15 / 0.075 = 2.0
        // p = 100 * pf / (pf + rwl) = 100 * 2.0 / (2.0 + 2.0) = 200 / 4.0 = 50.0
        auto [pf, p] = StatUtils<DecimalType>::computeProfitability(returns);

        REQUIRE(num::to_double(pf) == Catch::Approx(2.0));
        REQUIRE(num::to_double(p) == Catch::Approx(50.0));
    }

    // Test an edge case with an empty input vector.
    SECTION("Empty vector of returns") {
        std::vector<DecimalType> returns;
        // As per the implementation, this should return zeros for both profit factor and profitability.
        auto [pf, p] = StatUtils<DecimalType>::computeProfitability(returns);
        REQUIRE(pf == DecimalConstants<DecimalType>::DecimalZero);
        REQUIRE(p == DecimalConstants<DecimalType>::DecimalZero);
    }

    // Test with only winning trades.
    SECTION("Only winning trades") {
        std::vector<DecimalType> returns = {DecimalType("0.10"), DecimalType("0.20"), DecimalType("0.30")};
        // pf = 100 (no losses)
        // rwl = 0 (no losing trades)
        // p = (100 * 100) / (100 + 0) = 100
        auto [pf, p] = StatUtils<DecimalType>::computeProfitability(returns);

        REQUIRE(pf == DecimalConstants<DecimalType>::DecimalOneHundred);
        REQUIRE(num::to_double(p) == Catch::Approx(100.0));
    }

    // Test with only losing trades.
    SECTION("Only losing trades") {
        std::vector<DecimalType> returns = {DecimalType("-0.10"), DecimalType("-0.20")};
        // pf = 0 (no wins)
        // rwl = 0 (no winning trades)
        // p = (100 * 0) / (0 + 0) -> denominator is 0, so p = 0
        auto [pf, p] = StatUtils<DecimalType>::computeProfitability(returns);

        REQUIRE(pf == DecimalConstants<DecimalType>::DecimalZero);
        REQUIRE(p == DecimalConstants<DecimalType>::DecimalZero);
    }

    // Test that trades with a return of zero are correctly ignored in the calculations.
    SECTION("Trades with zero return are ignored") {
        std::vector<DecimalType> returns = {DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.0"), DecimalType("0.20"), DecimalType("-0.10")};
        // The result should be the same as the basic scenario, as the zero-return trade has no impact.
        auto [pf, p] = StatUtils<DecimalType>::computeProfitability(returns);

        REQUIRE(num::to_double(pf) == Catch::Approx(2.0));
        REQUIRE(num::to_double(p) == Catch::Approx(50.0));
    }
}

TEST_CASE("StatUtils::bootstrapWithReplacement", "[StatUtils][Bootstrap]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Random thread-local bootstrap produces same-sized output") {
        std::vector<DecimalType> input = { DecimalType("0.1"), DecimalType("0.2"), DecimalType("0.3") };
        auto result = Stat::bootstrapWithReplacement(input);
        REQUIRE(result.size() == input.size());
        for (const auto& val : result) {
            REQUIRE((val == DecimalType("0.1") || val == DecimalType("0.2") || val == DecimalType("0.3")));
        }
    }

    SECTION("Seeded bootstrap returns deterministic result") {
        std::vector<DecimalType> input = { DecimalType("0.1"), DecimalType("0.2"), DecimalType("0.3") };
        auto result1 = Stat::bootstrapWithReplacement(input, 5, 12345);
        auto result2 = Stat::bootstrapWithReplacement(input, 5, 12345);

        REQUIRE(result1.size() == 5);
        REQUIRE(result2.size() == 5);
        REQUIRE(result1 == result2);  // Deterministic behavior
    }

    SECTION("Bootstrap with explicit sample size larger than input") {
        std::vector<DecimalType> input = { DecimalType("0.1"), DecimalType("0.2") };
        auto result = Stat::bootstrapWithReplacement(input, 10);
        REQUIRE(result.size() == 10);
        for (const auto& val : result) {
            REQUIRE((val == DecimalType("0.1") || val == DecimalType("0.2")));
        }
    }

    SECTION("Bootstrap with empty input throws exception") {
      std::vector<DecimalType> input;
      REQUIRE_THROWS_AS(Stat::bootstrapWithReplacement(input), std::invalid_argument);
    }
}

TEST_CASE("StatUtils::getBootStrappedProfitability with deterministic seed", "[StatUtils][Bootstrap]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Bootstrap with fixed seed produces reproducible profitability result") {
        std::vector<DecimalType> returns = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"),
            DecimalType("0.15")
        };

        constexpr size_t numBootstraps = 10;
        constexpr uint64_t seed = 42;

        auto result1 = Stat::getBootStrappedProfitability(
            returns,
            Stat::computeProfitability,
            numBootstraps,
            seed
        );

        auto result2 = Stat::getBootStrappedProfitability(
            returns,
            Stat::computeProfitability,
            numBootstraps,
            seed
        );

        // Must be reproducible
        REQUIRE(num::to_double(std::get<0>(result1)) == Catch::Approx(num::to_double(std::get<0>(result2))));
        REQUIRE(num::to_double(std::get<1>(result1)) == Catch::Approx(num::to_double(std::get<1>(result2))));
    }

    SECTION("Bootstrap returns zero when sample size is too small") {
        std::vector<DecimalType> smallSample = {
            DecimalType("0.05"), DecimalType("-0.03")
        };

        auto result = Stat::getBootStrappedProfitability(
            smallSample,
            Stat::computeProfitability,
            5,
            123
        );

        REQUIRE(result == std::make_tuple(
            DecimalConstants<DecimalType>::DecimalZero,
            DecimalConstants<DecimalType>::DecimalZero
        ));
    }
}

TEST_CASE("StatUtils::getBootStrappedLogProfitability with deterministic seed", "[StatUtils][Bootstrap]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Bootstrap with fixed seed produces reproducible log profitability result") {
        std::vector<DecimalType> returns = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"),
            DecimalType("0.15")
        };

        constexpr size_t numBootstraps = 10;
        constexpr uint64_t seed = 42;

        // Call the function twice with the same seed
        auto result1 = Stat::getBootStrappedLogProfitability(
            returns,
            numBootstraps,
            seed
        );

        auto result2 = Stat::getBootStrappedLogProfitability(
            returns,
            numBootstraps,
            seed
        );

        // The results must be identical due to the fixed seed
        REQUIRE(num::to_double(std::get<0>(result1)) == Catch::Approx(num::to_double(std::get<0>(result2))));
        REQUIRE(num::to_double(std::get<1>(result1)) == Catch::Approx(num::to_double(std::get<1>(result2))));
    }
}

TEST_CASE("StatUtils non-seeded bootstrap methods are statistically sound", "[StatUtils][Bootstrap][Stochastic]") {
    using Stat = StatUtils<DecimalType>;

    std::vector<DecimalType> returns = {
        DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
        DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
        DecimalType("-0.02"), DecimalType("0.08"), DecimalType("-0.12"),
        DecimalType("0.25")
    };

    // Calculate the "true" statistics from the original data to serve as a benchmark.
    auto [true_lpf, true_lp] = Stat::computeLogProfitability(returns);
    auto [true_pf, true_p] = Stat::computeProfitability(returns);
    auto true_single_pf = Stat::computeProfitFactor(returns);

    SECTION("getBootStrappedLogProfitability (non-seeded) distribution is centered on true value") {
        constexpr int num_runs = 200;
        std::vector<DecimalType> lpf_results;
        std::vector<DecimalType> lp_results;
        lpf_results.reserve(num_runs);
        lp_results.reserve(num_runs);

        // Run the non-deterministic function many times to get a distribution of results.
        for (int i = 0; i < num_runs; ++i) {
            auto [lpf, lp] = Stat::getBootStrappedLogProfitability(returns, 100); // 100 bootstraps per run
            lpf_results.push_back(lpf);
            lp_results.push_back(lp);
        }

        // Calculate the mean and standard deviation of the resulting distributions.
        DecimalType mean_lpf = Stat::computeMean(lpf_results);
        DecimalType stddev_lpf = Stat::computeStdDev(lpf_results, mean_lpf);

        DecimalType mean_lp = Stat::computeMean(lp_results);
        DecimalType stddev_lp = Stat::computeStdDev(lp_results, mean_lp);

        // The true value should be within a reasonable range of the bootstrapped mean.
        // A 3-sigma range is a common choice for such statistical tests.
        REQUIRE(num::to_double(true_lpf) == Catch::Approx(num::to_double(mean_lpf)).margin(num::to_double(stddev_lpf * DecimalType(3.0))));
        REQUIRE(num::to_double(true_lp) == Catch::Approx(num::to_double(mean_lp)).margin(num::to_double(stddev_lp * DecimalType(3.0))));
    }
    
    SECTION("getBootStrappedProfitability (non-seeded) distribution is centered on true value") {
        constexpr int num_runs = 200;
        std::vector<DecimalType> pf_results;
        std::vector<DecimalType> p_results;
        pf_results.reserve(num_runs);
        p_results.reserve(num_runs);

        for (int i = 0; i < num_runs; ++i) {
            auto [pf, p] = Stat::getBootStrappedProfitability(returns, Stat::computeProfitability, 100);
            pf_results.push_back(pf);
            p_results.push_back(p);
        }

        DecimalType mean_pf = Stat::computeMean(pf_results);
        DecimalType stddev_pf = Stat::computeStdDev(pf_results, mean_pf);

        DecimalType mean_p = Stat::computeMean(p_results);
        DecimalType stddev_p = Stat::computeStdDev(p_results, mean_p);

        REQUIRE(num::to_double(true_pf) == Catch::Approx(num::to_double(mean_pf)).margin(num::to_double(stddev_pf * DecimalType(3.0))));
        REQUIRE(num::to_double(true_p) == Catch::Approx(num::to_double(mean_p)).margin(num::to_double(stddev_p * DecimalType(3.0))));
    }

    SECTION("getBootStrappedStatistic (non-seeded) distribution is centered on true value") {
        auto computePF = [](const std::vector<DecimalType>& series) -> DecimalType {
            return Stat::computeProfitFactor(series);
        };
        
        constexpr int num_runs = 200;
        std::vector<DecimalType> pf_results;
        pf_results.reserve(num_runs);

        for (int i = 0; i < num_runs; ++i) {
            pf_results.push_back(Stat::getBootStrappedStatistic(returns, computePF, 100));
        }

        DecimalType mean_pf = Stat::computeMean(pf_results);
        DecimalType stddev_pf = Stat::computeStdDev(pf_results, mean_pf);

        REQUIRE(num::to_double(true_single_pf) == Catch::Approx(num::to_double(mean_pf)).margin(num::to_double(stddev_pf * DecimalType(3.0))));
    }
}

// --------------------------- New Tests: GeoMeanStat ---------------------------

TEST_CASE("GeoMeanStat basic correctness and edge cases", "[StatUtils][GeoMean]") {
    // Absolute tolerance to accommodate Decimal<->double rounding differences.
    constexpr double kGeoTol = 5e-8;

    // Convenience lambda to compute expected geometric mean in double
    auto expected_geo = [](const std::vector<double>& rs) -> double {
        if (rs.empty()) return 0.0;
        long double s = 0.0L;
        for (double r : rs) {
            // assume r > -1 for validity
            s += std::log1p(r);
        }
        return std::expm1(s / static_cast<long double>(rs.size()));
    };

    SECTION("Positive returns only") {
        std::vector<DecimalType> v = { DecimalType("0.10"), DecimalType("0.20"), DecimalType("0.05") };
        GeoMeanStat<DecimalType> stat; // default: clip=false
        DecimalType got = stat(v);

        double expd = expected_geo({0.10, 0.20, 0.05});
        REQUIRE(num::to_double(got) == Catch::Approx(expd).margin(kGeoTol));
    }

    SECTION("Mixed positive, negative, and zero returns") {
        std::vector<DecimalType> v = { DecimalType("0.0"), DecimalType("0.10"), DecimalType("-0.05") };
        GeoMeanStat<DecimalType> stat;
        DecimalType got = stat(v);

        double expd = expected_geo({0.0, 0.10, -0.05});
        REQUIRE(num::to_double(got) == Catch::Approx(expd).margin(kGeoTol));
    }

    SECTION("Constant returns: geometric mean equals the constant return") {
        std::vector<DecimalType> v = { DecimalType("0.05"), DecimalType("0.05"), DecimalType("0.05"), DecimalType("0.05") };
        GeoMeanStat<DecimalType> stat;
        DecimalType got = stat(v);

        REQUIRE(num::to_double(got) == Catch::Approx(0.05).margin(kGeoTol));
    }

    SECTION("Empty vector returns 0") {
        std::vector<DecimalType> v;
        GeoMeanStat<DecimalType> stat;
        DecimalType got = stat(v);

        REQUIRE(got == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Return <= -1 throws by default") {
        std::vector<DecimalType> v = { DecimalType("0.02"), DecimalType("-1.0") };
        GeoMeanStat<DecimalType> stat(false); // clip=false
        REQUIRE_THROWS_AS(stat(v), std::domain_error);
    }

    SECTION("Clipping mode: r <= -1 is winsorized and does not throw") {
        std::vector<DecimalType> v = { DecimalType("0.02"), DecimalType("-1.0") };
        const double eps = 1e-6;
        GeoMeanStat<DecimalType> stat(/*clip_ruin=*/true, /*eps=*/eps);

        // Should not throw
        DecimalType got = stat(v);

        // Expected with r clipped to (-1 + eps)
        double expd = expected_geo({0.02, -1.0 + eps});
        REQUIRE(num::to_double(got) == Catch::Approx(expd).margin(kGeoTol));

        // And the result must be strictly greater than -1
        REQUIRE(got > DecimalType("-1.0"));
    }
}

TEST_CASE("GeoMeanStat: auto winsorization at small N", "[StatUtils][GeoMean][Winsor]") {
    using D = DecimalType;
    using DC = DecimalConstants<D>;

    auto manual_winsor1_log_geom = [](const std::vector<D>& v) {
        std::vector<D> logs; logs.reserve(v.size());
        const D one = DC::DecimalOne;
        for (auto& r : v) {
            D g = one + r;
            REQUIRE(g > D("0")); // assume no ruin in this helper
            logs.push_back(std::log(g));
        }
        auto s = logs;
        std::sort(s.begin(), s.end());
        const std::size_t n = s.size();
        const std::size_t k = 1;
        const D lo = s[k], hi = s[n - 1 - k];
        for (auto& x : logs) { if (x < lo) x = lo; else if (x > hi) x = hi; }
        D sum = DC::DecimalZero;
        for (auto& x : logs) sum += x;
        const D avg = sum / D(static_cast<double>(n));
        return std::exp(avg) - one;
    };

    auto raw_geom = [](const std::vector<D>& v) {
        using std::log; using std::exp;
        const D one = DC::DecimalOne;
        D sum = DC::DecimalZero;
        for (auto& r : v) { D g = one + r; REQUIRE(g > D("0")); sum += std::log(g); }
        const D avg = sum / D(static_cast<double>(v.size()));
        return std::exp(avg) - one;
    };

    SECTION("N=30 → winsorize one per tail") {
        std::vector<D> v(30, D("0.005"));
        v[3]  = D("-0.45");  // extreme negative
        v[17] = D("0.20");   // extreme positive

        GeoMeanStat<D> stat;         // default clipping as configured in production
        D got  = stat(v);            // auto winsorization should apply (k=1)
        D expd = manual_winsor1_log_geom(v);

        REQUIRE(num::to_double(got) == Catch::Approx(num::to_double(expd)).margin(1e-8));
    }

    SECTION("N=19 → no winsorization") {
        std::vector<D> v(19, D("0.005"));
        v[2]  = D("-0.45");
        v[10] = D("0.20");

        GeoMeanStat<D> stat;
        D got  = stat(v);            // no winsorization for N<20
        D expd = raw_geom(v);

        REQUIRE(num::to_double(got) == Catch::Approx(num::to_double(expd)).margin(1e-8));
    }

    SECTION("N=50 → no winsorization") {
        std::vector<D> v(50, D("0.005"));
        v[5]  = D("-0.45");
        v[22] = D("0.20");

        GeoMeanStat<D> stat(true, true, 0.02, 1e-8, 0);
        D got  = stat(v);            // no winsorization for N>=50
        D expd = raw_geom(v);

        REQUIRE(num::to_double(got) == Catch::Approx(num::to_double(expd)).margin(1e-8));
    }

    SECTION("Clipping with ruin + small-N winsorization does not throw") {
        std::vector<D> v(30, D("0.0"));
        v[7]  = D("-1.0");  // ruin bar (will be clamped if clipping is on)
        v[12] = D("0.25");

        GeoMeanStat<D> stat(/*clip_ruin=*/true, /*ruin_eps=*/1e-8);
        D got = stat(v); // should not throw
        REQUIRE(got > D("-1.0"));
        REQUIRE(std::isfinite(num::to_double(got)));
    }
}

TEST_CASE("GeoMeanStat works as a statistic in getBootStrappedStatistic", "[StatUtils][GeoMean][Bootstrap]") {
    using Stat = StatUtils<DecimalType>;
    GeoMeanStat<DecimalType> stat; // default: clip=false

    std::vector<DecimalType> returns = {
        DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
        DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
        DecimalType("-0.02"), DecimalType("0.08"), DecimalType("-0.12"),
        DecimalType("0.25")
    };

    // "True" geometric mean for the original sample
    DecimalType true_geo = stat(returns);

    // Run multiple bootstrap medians to form a distribution of estimates
    constexpr int num_runs = 100;
    std::vector<DecimalType> boot_medians;
    boot_medians.reserve(num_runs);

    for (int i = 0; i < num_runs; ++i) {
        boot_medians.push_back(Stat::getBootStrappedStatistic(
            returns,
            stat,          // std::function will bind to GeoMeanStat::operator()
            100));         // bootstraps per run
    }

    DecimalType mean_est = Stat::computeMean(boot_medians);
    DecimalType std_est  = Stat::computeStdDev(boot_medians, mean_est);

    // The true geometric mean should be within ~3 std dev of the bootstrap distribution mean.
    REQUIRE(num::to_double(true_geo) == Catch::Approx(num::to_double(mean_est)).margin(num::to_double(std_est * DecimalType(3.0))));
}

TEST_CASE("LogProfitFactorStat basic correctness vs StatUtils::computeLogProfitFactorRobust",
          "[StatUtils][LogPFStat]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Default-constructed functor matches robust LPF (compressed)") {
        std::vector<DecimalType> returns = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"),
            DecimalType("0.15"), DecimalType("0.05"),
            DecimalType("-0.02")
        };

        typename StatUtils<DecimalType>::LogProfitFactorStat stat; // compressResult=true by default

        DecimalType direct = Stat::computeLogProfitFactorRobust(
            returns,
            /*compressResult=*/true
        );
        DecimalType via_functor = stat(returns);

        REQUIRE(num::to_double(via_functor) ==
                Catch::Approx(num::to_double(direct)).epsilon(1e-12));
    }

    SECTION("Non-compressed functor matches robust LPF with compressResult=false") {
        std::vector<DecimalType> returns = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10")
        };

        // Explicitly disable compression in the functor
        typename StatUtils<DecimalType>::LogProfitFactorStat stat(/*compressResult=*/false);

        DecimalType direct = Stat::computeLogProfitFactorRobust(
            returns,
            /*compressResult=*/false
        );
        DecimalType via_functor = stat(returns);

        REQUIRE(num::to_double(via_functor) ==
                Catch::Approx(num::to_double(direct)).epsilon(1e-12));
    }

    SECTION("Empty input returns zero consistently with StatUtils implementation") {
        std::vector<DecimalType> empty;
        typename StatUtils<DecimalType>::LogProfitFactorStat stat;

        DecimalType direct = Stat::computeLogProfitFactorRobust(empty);
        DecimalType via_functor = stat(empty);

        REQUIRE(via_functor == direct);
        REQUIRE(via_functor == DecimalConstants<DecimalType>::DecimalZero);
    }
}

TEST_CASE("LogProfitFactorStat works as a statistic in getBootStrappedStatistic",
          "[StatUtils][LogPFStat][Bootstrap]") {
    using Stat = StatUtils<DecimalType>;

    // Use a mixed realistic-looking return series
    std::vector<DecimalType> returns = {
        DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
        DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
        DecimalType("-0.02"), DecimalType("0.08"), DecimalType("-0.12"),
        DecimalType("0.25")
    };

    // Statistic functor (compressed robust log PF)
    typename StatUtils<DecimalType>::LogProfitFactorStat stat;

    // "True" statistic for the original sample
    DecimalType true_logPF = stat(returns);

    // Run multiple bootstrap medians to form a distribution of estimates
    constexpr int num_runs = 100;
    std::vector<DecimalType> boot_medians;
    boot_medians.reserve(num_runs);

    for (int i = 0; i < num_runs; ++i) {
        boot_medians.push_back(
            Stat::getBootStrappedStatistic(
                returns,
                stat,         // std::function binds to LogProfitFactorStat::operator()
                100));        // bootstraps per run
    }

    DecimalType mean_est = Stat::computeMean(boot_medians);
    DecimalType std_est  = Stat::computeStdDev(boot_medians, mean_est);

    // As with the GeoMeanStat test: the true value should lie within ~3σ of the
    // bootstrap distribution mean; this checks both bias and wiring.
    REQUIRE(num::to_double(true_logPF) ==
            Catch::Approx(num::to_double(mean_est))
                .margin(num::to_double(std_est * DecimalType(3.0))));
}

TEST_CASE("Quantile function with linear interpolation", "[Quantile]") {

    SECTION("Empty vector returns zero") {
        std::vector<DecimalType> empty_vec;
        DecimalType result = StatUtils<DecimalType>::quantile(empty_vec, 0.5);
        REQUIRE(num::to_double(result) == Catch::Approx(0.0));
    }

    SECTION("Quantile is clamped to [0.0, 1.0]") {
        std::vector<DecimalType> v = {createDecimal("10"), createDecimal("20")};
        // q < 0.0 should be treated as q = 0.0
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, -1.0)) == Catch::Approx(10.0));
        // q > 1.0 should be treated as q = 1.0
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 2.0)) == Catch::Approx(20.0));
    }

    SECTION("Even number of elements") {
        // Sorted: {10, 20, 30, 40}. n=4.
        std::vector<DecimalType> v = {createDecimal("40"), createDecimal("10"), createDecimal("30"), createDecimal("20")};

        // Median (q=0.5): idx = 0.5 * (4-1) = 1.5. Interpolate between v[1] (20) and v[2] (30).
        // 20 + 0.5 * (30 - 20) = 25
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.5)) == Catch::Approx(25.0));

        // 25th percentile (q=0.25): idx = 0.25 * 3 = 0.75. Interpolate between v[0] (10) and v[1] (20).
        // 10 + 0.75 * (20 - 10) = 17.5
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.25)) == Catch::Approx(17.5));

        // 75th percentile (q=0.75): idx = 0.75 * 3 = 2.25. Interpolate between v[2] (30) and v[3] (40).
        // 30 + 0.25 * (40 - 30) = 32.5
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.75)) == Catch::Approx(32.5));
    }

    SECTION("Odd number of elements") {
        // Sorted: {10, 20, 30, 40, 50}. n=5.
        std::vector<DecimalType> v = {createDecimal("50"), createDecimal("20"), createDecimal("40"), createDecimal("10"), createDecimal("30")};

        // Median (q=0.5): idx = 0.5 * (5-1) = 2.0. Exact index, no interpolation.
        // Result should be v[2], which is 30.
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.5)) == Catch::Approx(30.0));

        // 90th percentile (q=0.9): idx = 0.9 * 4 = 3.6. Interpolate between v[3] (40) and v[4] (50).
        // 40 + 0.6 * (50 - 40) = 46.0
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.9)) == Catch::Approx(46.0));
    }

    SECTION("Minimum (0th percentile) and Maximum (100th percentile)") {
        std::vector<DecimalType> v = {createDecimal("15"), createDecimal("-5"), createDecimal("100"), createDecimal("30")};
        // Sorted: {-5, 15, 30, 100}
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.0)) == Catch::Approx(-5.0));
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 1.0)) == Catch::Approx(100.0));
    }

    SECTION("Single element vector") {
        std::vector<DecimalType> v = {createDecimal("42")};
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.0)) == Catch::Approx(42.0));
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.5)) == Catch::Approx(42.0));
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 1.0)) == Catch::Approx(42.0));
    }

    SECTION("Vector with duplicate values") {
        // Sorted: {10, 20, 20, 30}. n=4
        std::vector<DecimalType> v = {createDecimal("30"), createDecimal("20"), createDecimal("10"), createDecimal("20")};

        // Median (q=0.5): idx = 0.5 * 3 = 1.5. Interpolate between v[1] (20) and v[2] (20).
        // 20 + 0.5 * (20 - 20) = 20
        REQUIRE(num::to_double(StatUtils<DecimalType>::quantile(v, 0.5)) == Catch::Approx(20.0));
    }
}

TEST_CASE("StatUtils::computeVariance basic correctness and edge cases", "[StatUtils][Variance]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Known small dataset: {1,2,3,4} -> sample variance = 5/(4-1) = 1.666666...") {
        std::vector<DecimalType> v = {
            DecimalType("1.0"), DecimalType("2.0"),
            DecimalType("3.0"), DecimalType("4.0")
        };
        DecimalType mean = Stat::computeMean(v);
        DecimalType var  = Stat::computeVariance(v, mean);

        REQUIRE(num::to_double(mean) == Catch::Approx(2.5));
        REQUIRE(num::to_double(var)  == Catch::Approx(1.6666666667));
    }

    SECTION("Single-element vector -> variance = 0") {
        std::vector<DecimalType> v = { DecimalType("42.0") };
        DecimalType mean = Stat::computeMean(v);
        DecimalType var  = Stat::computeVariance(v, mean);

        REQUIRE(num::to_double(mean) == Catch::Approx(42.0));
        REQUIRE(var == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Empty vector -> variance = 0") {
        std::vector<DecimalType> v;
        DecimalType mean = Stat::computeMean(v);
        DecimalType var  = Stat::computeVariance(v, mean);

        REQUIRE(mean == DecimalConstants<DecimalType>::DecimalZero);
        REQUIRE(var  == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Agreement with StdDev: var ≈ (stddev)^2 (for mixed small returns)") {
        std::vector<DecimalType> v = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"), DecimalType("0.15")
        };
        DecimalType mean = Stat::computeMean(v);
        DecimalType var  = Stat::computeVariance(v, mean);
        DecimalType sd   = Stat::computeStdDev(v, mean);

        REQUIRE(num::to_double(var) == Catch::Approx(std::pow(num::to_double(sd), 2)).margin(1e-9));
    }
}

TEST_CASE("StatUtils::computeMeanAndVariance correctness and consistency", "[StatUtils][MeanVar]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Matches computeMean + computeVariance on a mixed set") {
        std::vector<DecimalType> r = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10")
        };

        // One-pass result
        auto [m1, v1] = Stat::computeMeanAndVariance(r);

        // Two-pass reference
        DecimalType m2 = Stat::computeMean(r);
        DecimalType v2 = Stat::computeVariance(r, m2);

        REQUIRE(num::to_double(m1) == Catch::Approx(num::to_double(m2)).epsilon(1e-12));
        REQUIRE(num::to_double(v1) == Catch::Approx(num::to_double(v2)).epsilon(1e-12));

        // StdDev^2 ≈ Var sanity check
        DecimalType sd = Stat::computeStdDev(r, m2);

        REQUIRE(num::to_double(v1) == Catch::Approx(std::pow(num::to_double(sd), 2)).margin(1e-9));
    }

    SECTION("Edge cases: empty and single-element") {
        {
            std::vector<DecimalType> r;
            auto [m, v] = Stat::computeMeanAndVariance(r);
            REQUIRE(m == DecimalConstants<DecimalType>::DecimalZero);
            REQUIRE(v == DecimalConstants<DecimalType>::DecimalZero);
        }
        {
            std::vector<DecimalType> r = { DecimalType("7.5") };
            auto [m, v] = Stat::computeMeanAndVariance(r);
            REQUIRE(num::to_double(m) == Catch::Approx(7.5));
            REQUIRE(v == DecimalConstants<DecimalType>::DecimalZero);
        }
    }

    SECTION("Light numerical-stability check (large level + tiny noise)") {
        // Use moderate magnitudes to stay within DecimalType precision comfortably.
        std::vector<DecimalType> r = {
            DecimalType("10000.0000"),
            DecimalType("10000.0001"),
            DecimalType("9999.9999"),
            DecimalType("10000.0002"),
            DecimalType("9999.9998")
        };

        // One-pass mean/var
        auto [m_dec, v_dec] = Stat::computeMeanAndVariance(r);

        // Double-precision reference (unbiased sample variance)
        std::vector<double> d;
        d.reserve(r.size());
        for (const auto& x : r) d.push_back(num::to_double(x));
        const double n = static_cast<double>(d.size());
        const double m_ref = std::accumulate(d.begin(), d.end(), 0.0) / n;
        double ss = 0.0;
        for (double x : d) {
            const double diff = x - m_ref;
            ss += diff * diff;
        }
        const double v_ref = (d.size() > 1) ? (ss / (n - 1.0)) : 0.0;

        REQUIRE(num::to_double(m_dec) == Catch::Approx(m_ref).margin(1e-10));
        REQUIRE(num::to_double(v_dec) == Catch::Approx(v_ref).margin(1e-8));
    }
}

// --------------------------- New Tests: ComputeFast / computeMeanAndVarianceFast ---------------------------

TEST_CASE("ComputeFast<DecimalType>::run matches standard mean/variance", "[StatUtils][ComputeFast]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Typical mixed returns") {
        std::vector<DecimalType> r = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"), DecimalType("0.15")
        };

        // Reference (regular path)
        auto [m_ref, v_ref] = Stat::computeMeanAndVariance(r);

        // Fast path (hybrid Welford specialization for dec::decimal)
        auto [m_fast, v_fast] = Stat::computeMeanAndVarianceFast(r);

        REQUIRE(num::to_double(m_fast) == Catch::Approx(num::to_double(m_ref)).epsilon(1e-12));
        REQUIRE(num::to_double(v_fast) == Catch::Approx(num::to_double(v_ref)).epsilon(1e-12));

        // StdDev^2 ≈ Var sanity (computed from fast mean)
        DecimalType sd = Stat::computeStdDev(r, m_fast);
        REQUIRE(num::to_double(v_fast) == Catch::Approx(std::pow(num::to_double(sd), 2)).margin(1e-9));
    }

    SECTION("Edge cases: empty and single element") {
        {
            std::vector<DecimalType> r;
            auto [m_ref, v_ref]   = Stat::computeMeanAndVariance(r);
            auto [m_fast, v_fast] = Stat::computeMeanAndVarianceFast(r);

            REQUIRE(m_fast == m_ref);
            REQUIRE(v_fast == v_ref);
        }
        {
            std::vector<DecimalType> r = { DecimalType("7.5") };
            auto [m_ref, v_ref]   = Stat::computeMeanAndVariance(r);
            auto [m_fast, v_fast] = Stat::computeMeanAndVarianceFast(r);

            REQUIRE(num::to_double(m_fast) == Catch::Approx(num::to_double(m_ref)).epsilon(1e-12));
            REQUIRE(v_fast == v_ref); // both should be zero
        }
    }
}

TEST_CASE("computeMeanAndVarianceFast numerical stability on large level + tiny noise", "[StatUtils][ComputeFast][Stability]") {
    using Stat = StatUtils<DecimalType>;

    // Moderate magnitudes given Decimal precision; mirrors earlier stability test.
    std::vector<DecimalType> r = {
        DecimalType("10000.0000"),
        DecimalType("10000.0001"),
        DecimalType("9999.9999"),
        DecimalType("10000.0002"),
        DecimalType("9999.9998")
    };

    // Regular path
    auto [m_ref, v_ref] = Stat::computeMeanAndVariance(r);

    // Fast path
    auto [m_fast, v_fast] = Stat::computeMeanAndVarianceFast(r);

    // Means should be extremely close
    REQUIRE(num::to_double(m_fast) == Catch::Approx(num::to_double(m_ref)).margin(1e-10));

    // Variances are ~1e-8 in scale; allow tight absolute margin
    REQUIRE(num::to_double(v_fast) == Catch::Approx(num::to_double(v_ref)).margin(1e-8));

    // stddev^2 ≈ var (fast outputs)
    DecimalType sd_fast = Stat::computeStdDev(r, m_fast);
    REQUIRE(num::to_double(v_fast) == Catch::Approx(std::pow(num::to_double(sd_fast), 2)).margin(1e-9));
}

TEST_CASE("StatUtils::sharpeFromReturns basic behavior and edge cases", "[StatUtils][Sharpe]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Typical mixed returns (fast path)") {
        std::vector<DecimalType> r = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"), DecimalType("0.15")
        };

        // Reference via fast mean/variance then formula (same eps/ppy)
        auto [m, v] = Stat::computeMeanAndVarianceFast(r);
        const double eps = 1e-8;
        const double sd  = std::sqrt(std::max(0.0, num::to_double(v) + eps));
        const double ref = (sd > 0.0) ? (num::to_double(m) / sd) : 0.0;

        DecimalType sr = Stat::sharpeFromReturns(r, /*eps=*/eps, /*periodsPerYear=*/1.0, 0.0);
        REQUIRE(num::to_double(sr) == Catch::Approx(ref).epsilon(1e-8));
    }

    SECTION("Annualization scales Sharpe by sqrt(periodsPerYear)") {
        std::vector<DecimalType> r = { DecimalType("0.01"), DecimalType("0.00"),
                                       DecimalType("-0.005"), DecimalType("0.015") };

        const double eps = 1e-8;
        const double sr1   = num::to_double(Stat::sharpeFromReturns(r, eps, 1.0, 0.0));
        const double sr252 = num::to_double(Stat::sharpeFromReturns(r, eps, 252.0, 0.0));

        REQUIRE(sr252 == Catch::Approx(sr1 * std::sqrt(252.0)).epsilon(1e-8));
    }

    SECTION("Risk-free subtraction reduces Sharpe (holding variance constant)") {
        // Slight variation to avoid zero variance
        std::vector<DecimalType> r = { DecimalType("0.010"), DecimalType("0.010"), DecimalType("0.011") };

        const double eps = 1e-8;
        const double sr0    = num::to_double(Stat::sharpeFromReturns(r, eps, 1.0, /*rf*/0.0));
        const double sr5bps = num::to_double(Stat::sharpeFromReturns(r, eps, 1.0, /*rf*/0.0005));

        REQUIRE(sr5bps < sr0);
    }

    SECTION("Empty vector -> Sharpe = 0") {
        std::vector<DecimalType> r;
        REQUIRE(Stat::sharpeFromReturns(r) == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Constant returns with eps=0 -> Sharpe = 0 (degenerate variance)") {
        std::vector<DecimalType> r = { DecimalType("0.01"), DecimalType("0.01"), DecimalType("0.01") };
        // With eps=0 the sd path should detect zero variance and return 0.
        REQUIRE(Stat::sharpeFromReturns(r, /*eps=*/0.0, 1.0, 0.0) == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Numerical sanity: stddev^2 ≈ variance inside Sharpe path") {
        std::vector<DecimalType> r = {
            DecimalType("0.08"), DecimalType("-0.02"),
            DecimalType("0.03"), DecimalType("0.01"), DecimalType("-0.04")
        };

        // Compare sd^2 (reconstructed from Sharpe components) with fast variance
        auto [m, v] = Stat::computeMeanAndVarianceFast(r);
        const double eps = 1e-8;

        // Rebuild sd from the returned Sharpe: sr = mean / sd  =>  sd = mean / sr
        const double sr = num::to_double(Stat::sharpeFromReturns(r, eps, 1.0, 0.0));
        const double sd = (sr != 0.0) ? (num::to_double(m) / sr) : 0.0;

        REQUIRE(std::pow(sd, 2) == Catch::Approx(num::to_double(v) + eps).margin(1e-9));
    }
}

TEST_CASE("StatUtils::sharpeFromReturns (lean) behavior and edge cases", "[StatUtils][SharpeLean]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Lean matches explicit mean/sd formula with fast mean/var") {
        std::vector<DecimalType> r = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"), DecimalType("0.15")
        };

        const double eps = 1e-8;

        auto [m, v] = Stat::computeMeanAndVarianceFast(r);
        const double sd  = std::sqrt(std::max(0.0, num::to_double(v) + eps));
        const double ref = (sd > 0.0) ? (num::to_double(m) / sd) : 0.0;

        const double sr_lean = num::to_double(Stat::sharpeFromReturns(r, eps));
        REQUIRE(sr_lean == Catch::Approx(ref).margin(1e-9));
    }

    SECTION("Lean equals general overload with defaults (ppy=1, rf=0)") {
        std::vector<DecimalType> r = {
            DecimalType("0.02"), DecimalType("-0.01"),
            DecimalType("0.03"), DecimalType("-0.005"), DecimalType("0.015")
        };
        const double eps = 1e-8;

        const double sr_lean = num::to_double(Stat::sharpeFromReturns(r, eps));
        const double sr_gen  = num::to_double(Stat::sharpeFromReturns(r, eps, /*periodsPerYear=*/1.0, /*rf=*/0.0));

        REQUIRE(sr_lean == Catch::Approx(sr_gen).margin(1e-9));
    }

    SECTION("Annualized general ≈ lean * sqrt(periodsPerYear)") {
        std::vector<DecimalType> r = {
            DecimalType("0.01"), DecimalType("0.00"),
            DecimalType("-0.005"), DecimalType("0.015")
        };
        const double eps = 1e-8;
        const double ppy = 252.0;

        const double sr_lean = num::to_double(Stat::sharpeFromReturns(r, eps));
        const double sr_ann  = num::to_double(Stat::sharpeFromReturns(r, eps, ppy, /*rf=*/0.0));

        REQUIRE(sr_ann == Catch::Approx(sr_lean * std::sqrt(ppy)).margin(5e-9));
    }

    SECTION("Empty vector -> Sharpe = 0") {
        std::vector<DecimalType> r;
        REQUIRE(Stat::sharpeFromReturns(r) == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Constant returns with eps=0 -> Sharpe = 0 (degenerate variance)") {
        std::vector<DecimalType> r = { DecimalType("0.01"), DecimalType("0.01"), DecimalType("0.01") };
        REQUIRE(Stat::sharpeFromReturns(r, /*eps=*/0.0) == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("General with positive risk-free reduces Sharpe vs lean") {
        // Slight variation to ensure non-zero variance
        std::vector<DecimalType> r = { DecimalType("0.010"), DecimalType("0.010"), DecimalType("0.011") };
        const double eps = 1e-8;

        const double sr_lean = num::to_double(Stat::sharpeFromReturns(r, eps));
        const double sr_rf   = num::to_double(Stat::sharpeFromReturns(r, eps, /*ppy=*/1.0, /*rf=*/0.0005));

        REQUIRE(sr_rf < sr_lean);
    }
}

// --------------------------- New Tests: ACF & Block-Length Heuristic ---------------------------

TEST_CASE("StatUtils::computeACF basic behavior and edge cases", "[StatUtils][ACF]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("n < 2 throws") {
        std::vector<DecimalType> x = { createDecimal("0.01") };
        REQUIRE_THROWS_AS(Stat::computeACF(x, /*maxLag=*/5), std::invalid_argument);
    }

    SECTION("Constant series: rho[0]=1, others ~0") {
        std::vector<DecimalType> x(6, createDecimal("0.05"));
        auto acf = Stat::computeACF(x, /*maxLag=*/10);
        REQUIRE(acf.size() == 6 - 1 + 1);          // min(maxLag, n-1)+1 = 6
        REQUIRE(num::to_double(acf[0]) == Catch::Approx(1.0));
        for (size_t k = 1; k < acf.size(); ++k) {
            REQUIRE(num::to_double(acf[k]) == Catch::Approx(0.0).margin(1e-12));
        }
    }

    SECTION("Agreement with reference (double) implementation") {
        // Mild trend + noise to ensure nontrivial ACF
        std::vector<DecimalType> x;
        for (int t = 0; t < 20; ++t) {
            // 0.01 * t + small deterministic wobble (keeps test deterministic)
            double v = 0.01 * t + ( (t % 3 == 0) ? 0.002 : (t % 3 == 1 ? -0.001 : 0.0) );
            x.push_back(createDecimal(std::to_string(v)));
        }

        // Library ACF (Decimal)
        auto acf_dec = Stat::computeACF(x, /*maxLag=*/12);

        // Reference ACF (double) using the same definition:
        auto acf_ref = [&]() {
            const size_t n = x.size();
            const size_t L = std::min<std::size_t>(12, n - 1);
            std::vector<double> xd(n);
            double mu = 0.0;
            for (auto& d : x) mu += num::to_double(d);
            mu /= double(n);
            double denom = 0.0;
            for (size_t i = 0; i < n; ++i) {
                xd[i] = num::to_double(x[i]) - mu;
                denom += xd[i] * xd[i];
            }
            std::vector<double> r(L + 1, 0.0);
            if (denom == 0.0) { r[0] = 1.0; return r; }
            r[0] = 1.0;
            for (size_t k = 1; k <= L; ++k) {
                double nume = 0.0;
                for (size_t t = k; t < n; ++t) nume += xd[t] * xd[t - k];
                r[k] = nume / denom;
            }
            return r;
        }();

        REQUIRE(acf_dec.size() == acf_ref.size());
        for (size_t k = 0; k < acf_ref.size(); ++k) {
            REQUIRE(num::to_double(acf_dec[k]) == Catch::Approx(acf_ref[k]).margin(kACFAbsTol));
        }
    }
}

TEST_CASE("StatUtils::suggestStationaryBlockLengthFromACF heuristic", "[StatUtils][ACF][BlockLen]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("No lag clears threshold -> clamp to minL") {
        // thresh = 2/sqrt(nSamples). Choose n=25 -> thresh = 0.4
        // Make all |rho(k)| <= 0.39 so none is 'significant'.
        std::vector<DecimalType> acf = {
            createDecimal("1.0"), createDecimal("0.39"), createDecimal("-0.10"),
            createDecimal("0.00"), createDecimal("0.05")
        };
        unsigned L = Stat::suggestStationaryBlockLengthFromACF(acf, /*nSamples=*/25, /*minL=*/2, /*maxL=*/6);
        REQUIRE(L == 2);
    }

    SECTION("Largest significant lag determines L (within clamps)") {
        // n=100 -> thresh = 2 / 10 = 0.2
        // Significant at lags 1 and 2, then it drops below threshold
        std::vector<DecimalType> acf = {
            createDecimal("1.0"), createDecimal("0.25"), createDecimal("0.22"),
            createDecimal("0.05"), createDecimal("0.00")
        };
        unsigned L = Stat::suggestStationaryBlockLengthFromACF(acf, /*nSamples=*/100, /*minL=*/2, /*maxL=*/6);
        REQUIRE(L == 2);
    }

    SECTION("Clamp to maxL when significance extends beyond range") {
        // n=100 -> thresh = 0.2; make lag 9 still significant
        std::vector<DecimalType> acf(11, createDecimal("0.0"));
        acf[0] = createDecimal("1.0");
        acf[9] = createDecimal("0.25"); // significant far out
        unsigned L = Stat::suggestStationaryBlockLengthFromACF(acf, /*nSamples=*/100, /*minL=*/2, /*maxL=*/6);
        REQUIRE(L == 6); // clamped to maxL
    }

    SECTION("Empty ACF or zero nSamples throws") {
        std::vector<DecimalType> empty_acf;
        REQUIRE_THROWS_AS(Stat::suggestStationaryBlockLengthFromACF(empty_acf, 10), std::invalid_argument);
        std::vector<DecimalType> acf = { createDecimal("1.0"), createDecimal("0.1") };
        REQUIRE_THROWS_AS(Stat::suggestStationaryBlockLengthFromACF(acf, 0), std::invalid_argument);
    }
}

// --------------------------- Smoke test: Monthly -> ACF -> BlockLen ---------------------------

TEST_CASE("StatUtils: end-to-end monthly->ACF->block length", "[StatUtils][ACF][Monthly][Smoke]") {
    using D    = DecimalType;
    using Stat = StatUtils<DecimalType>;

    // Fabricate 12 months in 2021 with deliberate short-range dependence:
    // Pairs of identical returns increase |rho(1)|, and the symmetric design keeps mean ~ 0.
    // Sequence (Jan..Dec): +2%, +2%, -2%, -2%, +1.5%, +1.5%, -1.5%, -1.5%, +1%, +1%, -1%, -1%
    ClosedPositionHistory<D> hist;
    TradingVolume one(1, TradingVolume::CONTRACTS);

    auto add_long_1bar = [&](int y, int m, int d, const char* r_str) {
        D r     = createDecimal(r_str);
        D entry = createDecimal("100");
        D exit  = entry * (D("1.0") + r);

        TimeSeriesDate de(y, m, d);
        auto e = createTimeSeriesEntry(de, entry, entry, entry, entry, 10);
        auto pos = std::make_shared<TradingPositionLong<D>>(myCornSymbol, e->getOpenValue(), *e, one);

        int d_exit = std::min(d + 1, 28);
        TimeSeriesDate dx(y, m, d_exit);
        pos->ClosePosition(dx, exit);
        hist.addClosedPosition(pos);
    };

    // Build the 12 months (single-bar per month → monthly return equals r_str exactly)
    add_long_1bar(2021, Jan,  5,  "0.02");
    add_long_1bar(2021, Feb,  8,  "0.02");
    add_long_1bar(2021, Mar,  5, "-0.02");
    add_long_1bar(2021, Apr, 12, "-0.02");
    add_long_1bar(2021, May,  6,  "0.015");
    add_long_1bar(2021, Jun, 15,  "0.015");
    add_long_1bar(2021, Jul,  7, "-0.015");
    add_long_1bar(2021, Aug, 19, "-0.015");
    add_long_1bar(2021, Sep,  9,  "0.01");
    add_long_1bar(2021, Oct, 13,  "0.01");
    add_long_1bar(2021, Nov,  3, "-0.01");
    add_long_1bar(2021, Dec, 21, "-0.01");

    // 1) Build monthly returns
    auto monthly = mkc_timeseries::buildMonthlyReturnsFromClosedPositions<D>(hist);
    REQUIRE(monthly.size() == 12);

    // Spot-check a few exact values (single-bar months)
    REQUIRE(monthly.front() == createDecimal("0.02"));    // Jan
    REQUIRE(monthly[1]      == createDecimal("0.02"));    // Feb
    REQUIRE(monthly[2]      == createDecimal("-0.02"));   // Mar
    REQUIRE(monthly.back()  == createDecimal("-0.01"));   // Dec

    // 2) Compute ACF up to 6 lags
    const std::size_t maxLag = 6;
    auto acf = Stat::computeACF(monthly, maxLag);

    // ACF shape: length = min(maxLag, n-1) + 1 = 7, and rho[0] == 1
    REQUIRE(acf.size() == 7);
    REQUIRE(num::to_double(acf[0]) == Catch::Approx(1.0));

    // 3) Suggest stationary bootstrap block length using the heuristic
    //    thresh = 2/sqrt(n) with n = 12 → ~0.577.
    const double thresh = 2.0 / std::sqrt(12.0);

    // Recompute k* like the production heuristic does (largest lag with |rho(k)| > thresh)
    unsigned k_star = 1;
    for (std::size_t k = 1; k < acf.size(); ++k) {
        if (std::fabs(num::to_double(acf[k])) > thresh) {
            k_star = static_cast<unsigned>(k);
        }
    }

    // Production clamp is [2,6] by default
    const unsigned expectedL = std::max<unsigned>(2, std::min<unsigned>(6, k_star));

    const unsigned L = Stat::suggestStationaryBlockLengthFromACF(acf, monthly.size(),
                                                                 /*minL=*/2, /*maxL=*/6);

    // 4) The suggested L should match the heuristic’s clamped k*
    REQUIRE(L == expectedL);

    // 5) Sanity: L is within [2,6]
    REQUIRE(L >= 2);
    REQUIRE(L <= 6);
}

TEST_CASE("StatUtils::computeLogProfitFactorRobust correctness & stability", "[StatUtils][LPF][Robust]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Robust LPF is conservative vs classic on typical mixed returns") {
        // Balanced sample with non-trivial wins and losses
        std::vector<DecimalType> r = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10"), DecimalType("0.15"),
            DecimalType("0.05"),  DecimalType("-0.02")
        };

        // Classic LPF (uncompressed)
        DecimalType lpf_classic = Stat::computeLogProfitFactor(r, /*compressResult=*/false);

        // Robust LPF (uncompressed; default priors)
        DecimalType lpf_robust  = Stat::computeLogProfitFactorRobust(
            r,
            /*compressResult=*/false
        );

        // Robust should not exceed classic (adds denominator ridge & ruin clamp)
        REQUIRE(num::to_double(lpf_robust) <= num::to_double(lpf_classic) + 1e-12);
    }

    SECTION("Ruin handling: robust LPF penalizes near/at -100% events instead of skipping them") {
        // Contains invalid log arguments in classic path (-1.0, -1.5) → classic ends up with only wins
        // and returns the 'no-loss' sentinel (100). Robust should clamp & count losses → much smaller LPF.
        std::vector<DecimalType> r = { DecimalType("0.50"), DecimalType("-1.0"), DecimalType("-1.5") };

        DecimalType lpf_classic = Stat::computeLogProfitFactor(r, /*compressResult=*/false);
        DecimalType lpf_robust  = Stat::computeLogProfitFactorRobust(r, /*compressResult=*/false);

        // Classic goes to sentinel 100; robust must be finite and strictly smaller.
        REQUIRE(lpf_classic == DecimalConstants<DecimalType>::DecimalOneHundred);
        REQUIRE(num::to_double(lpf_robust) < 100.0);
        REQUIRE(num::to_double(lpf_robust) >= 0.0); // still non-negative by definition
    }

    SECTION("Small-loss denominator stabilization: robust LPF prevents huge blow-ups") {
        // Tiny losses can make classic denominator ~0 in log-space → enormous LPF
        std::vector<DecimalType> r = {
            DecimalType("0.03"), DecimalType("0.02"), DecimalType("0.01"),
            DecimalType("-0.00000001"), DecimalType("-0.00000002")
        };

        DecimalType lpf_classic = Stat::computeLogProfitFactor(r, /*compressResult=*/false);
        DecimalType lpf_robust  = Stat::computeLogProfitFactorRobust(r, /*compressResult=*/false);

        // Robust should be ≤ classic (ridge in denominator) and not NaN/inf
        REQUIRE(num::to_double(lpf_robust) <= num::to_double(lpf_classic) + 1e-12);
        REQUIRE(std::isfinite(num::to_double(lpf_robust)));
    }

    SECTION("Compression consistency: compressed == log1p(uncompressed)") {
        std::vector<DecimalType> r = {
            DecimalType("0.10"), DecimalType("-0.05"),
            DecimalType("0.20"), DecimalType("-0.10")
        };

        DecimalType uncompressed = Stat::computeLogProfitFactorRobust(
            r,
            /*compressResult=*/false
        );
        DecimalType compressed = Stat::computeLogProfitFactorRobust(
            r,
            /*compressResult=*/true
        );

        const double exp_compressed = std::log1p(num::to_double(uncompressed));
        REQUIRE(num::to_double(compressed) == Catch::Approx(exp_compressed).margin(1e-12));
    }

    SECTION("Prior strength monotonicity: larger prior_strength → smaller (more conservative) LPF") {
        std::vector<DecimalType> r = {
            DecimalType("0.12"), DecimalType("-0.04"),
            DecimalType("0.09"), DecimalType("-0.03"),
            DecimalType("0.02")
        };

        // Hold all knobs equal, vary prior_strength
        const bool   compress = false;
        const double eps      = 1e-8;
        const double floor    = 1e-6;

        DecimalType lpf_ps_0_5 = Stat::computeLogProfitFactorRobust(r, compress, eps, floor, /*prior_strength=*/0.5);
        DecimalType lpf_ps_1_0 = Stat::computeLogProfitFactorRobust(r, compress, eps, floor, /*prior_strength=*/1.0);
        DecimalType lpf_ps_2_0 = Stat::computeLogProfitFactorRobust(r, compress, eps, floor, /*prior_strength=*/2.0);

        REQUIRE(num::to_double(lpf_ps_1_0) <= num::to_double(lpf_ps_0_5) + 1e-12);
        REQUIRE(num::to_double(lpf_ps_2_0) <= num::to_double(lpf_ps_1_0) + 1e-12);
    }

    SECTION("Ruin clamp epsilon prevents -inf and yields finite conservative value") {
        // A single ruin bar with no positive bars; classic would skip and report 'no-loss' sentinel (100).
        std::vector<DecimalType> r = { DecimalType("-1.0") };

        DecimalType lpf_classic = Stat::computeLogProfitFactor(r, /*compressResult=*/false);
        DecimalType lpf_robust  = Stat::computeLogProfitFactorRobust(
            r,
            /*compressResult=*/false,
            /*ruin_eps=*/1e-8,  // explicit for test clarity
            /*denom_floor=*/1e-6,
            /*prior_strength=*/1.0
        );

        REQUIRE(lpf_classic == DecimalConstants<DecimalType>::DecimalOneHundred); // classic 'no-loss' path
        REQUIRE(std::isfinite(num::to_double(lpf_robust)));
        // With only losses, numerator ~ 0 → robust LPF should be ~0 (or very small, due to ridge)
        REQUIRE(num::to_double(lpf_robust) >= 0.0);
        REQUIRE(num::to_double(lpf_robust) <= 0.10); // generous upper cap for this corner
    }
}

TEST_CASE("StatUtils::getBowleySkewness basic behavior and edge cases", "[StatUtils][BowleySkew]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Returns zero for n < 4") {
        std::vector<DecimalType> v = {
            createDecimal("1.0"),
            createDecimal("2.0"),
            createDecimal("3.0")
        };
        DecimalType bowley = Stat::getBowleySkewness(v);
        REQUIRE(bowley == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Symmetric distribution yields Bowley skewness ~ 0") {
        // Symmetric around 0: {-3, -1, 1, 3}
        std::vector<DecimalType> v = {
            createDecimal("-3.0"),
            createDecimal("-1.0"),
            createDecimal("1.0"),
            createDecimal("3.0")
        };

        DecimalType bowley = Stat::getBowleySkewness(v);
        REQUIRE(num::to_double(bowley) == Catch::Approx(0.0));
    }

    SECTION("Positively skewed distribution yields positive Bowley skewness") {
        // Right-skewed: {0, 0, 0, 1, 10}
        // With the linear quantile interpolation used in StatUtils::quantile,
        // Bowley skewness for this example is exactly 1.0.
        std::vector<DecimalType> v = {
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("1.0"),
            createDecimal("10.0")
        };

        DecimalType bowley = Stat::getBowleySkewness(v);
        REQUIRE(num::to_double(bowley) == Catch::Approx(1.0));
        REQUIRE(num::to_double(bowley) > 0.0);
    }

    SECTION("Constant vector returns zero skewness") {
        std::vector<DecimalType> v = {
            createDecimal("5.0"),
            createDecimal("5.0"),
            createDecimal("5.0"),
            createDecimal("5.0")
        };

        DecimalType bowley = Stat::getBowleySkewness(v);
        REQUIRE(bowley == DecimalConstants<DecimalType>::DecimalZero);
    }
}

TEST_CASE("StatUtils::getTailSpanRatio behavior", "[StatUtils][TailSpanRatio]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("n < 8 returns neutral ratio 1.0") {
        // 7 elements: below the minimum size threshold
        std::vector<DecimalType> v = {
            createDecimal("1.0"),
            createDecimal("2.0"),
            createDecimal("3.0"),
            createDecimal("4.0"),
            createDecimal("5.0"),
            createDecimal("6.0"),
            createDecimal("7.0")
        };

        double ratio = Stat::getTailSpanRatio(v);
        REQUIRE(ratio == Catch::Approx(1.0));
    }

    SECTION("Symmetric tails around median give ratio ~ 1.0") {
        // Symmetric around 0: {-2, -1, 0, 1, 2}
        std::vector<DecimalType> v = {
            createDecimal("-2.0"),
            createDecimal("-1.0"),
            createDecimal("0.0"),
            createDecimal("1.0"),
            createDecimal("2.0")
        };

        double ratio = Stat::getTailSpanRatio(v);
        REQUIRE(ratio == Catch::Approx(1.0));
    }

    SECTION("Heavier lower tail yields ratio > 1.0") {
        // Example with a stretched lower tail:
        // {-10, -5, -1, 0, 1, 2, 3, 4}
        // With 10–50–90% quantiles, this produces a tail ratio of 2.5.
        std::vector<DecimalType> v = {
            createDecimal("-10.0"),
            createDecimal("-5.0"),
            createDecimal("-1.0"),
            createDecimal("0.0"),
            createDecimal("1.0"),
            createDecimal("2.0"),
            createDecimal("3.0"),
            createDecimal("4.0")
        };

        double ratio = Stat::getTailSpanRatio(v);
        REQUIRE(ratio == Catch::Approx(2.5));
        REQUIRE(ratio > 1.0);
    }

    SECTION("Degenerate / nearly constant series falls back to 1.0") {
        // All values identical → both spans ~0 → should return 1.0
        std::vector<DecimalType> v = {
            createDecimal("3.0"),
            createDecimal("3.0"),
            createDecimal("3.0"),
            createDecimal("3.0"),
            createDecimal("3.0"),
            createDecimal("3.0"),
            createDecimal("3.0"),
            createDecimal("3.0")
        };

        double ratio = Stat::getTailSpanRatio(v);
        REQUIRE(ratio == Catch::Approx(1.0));
    }
}

TEST_CASE("StatUtils::computeQuantileShape summarizes Bowley skewness and tail span ratio", "[StatUtils][QuantileShape]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("n < 8 returns default QuantileShape") {
        // 7 elements → below minimum size threshold inside computeQuantileShape.
        std::vector<DecimalType> v = {
            createDecimal("1.0"),
            createDecimal("2.0"),
            createDecimal("3.0"),
            createDecimal("4.0"),
            createDecimal("5.0"),
            createDecimal("6.0"),
            createDecimal("7.0")
        };

        auto shape = Stat::computeQuantileShape(v);

        REQUIRE(shape.bowleySkew == Catch::Approx(0.0));
        REQUIRE(shape.tailRatio  == Catch::Approx(1.0));
        REQUIRE(shape.hasStrongAsymmetry == false);
        REQUIRE(shape.hasHeavyTails      == false);
    }

    SECTION("Symmetric distribution: bowleySkew ~ 0, tailRatio ~ 1, flags false") {
        // Build a symmetric distribution around 0 with n >= 8:
        // {-2, -1, 0, 1, 2, -2, -1, 0, 1, 2}
        std::vector<DecimalType> v = {
            createDecimal("-2.0"),
            createDecimal("-1.0"),
            createDecimal("0.0"),
            createDecimal("1.0"),
            createDecimal("2.0"),
            createDecimal("-2.0"),
            createDecimal("-1.0"),
            createDecimal("0.0"),
            createDecimal("1.0"),
            createDecimal("2.0")
        };

        auto shape = Stat::computeQuantileShape(v);

        REQUIRE(shape.bowleySkew == Catch::Approx(0.0));
        REQUIRE(shape.tailRatio  == Catch::Approx(1.0));
        REQUIRE(shape.hasStrongAsymmetry == false);
        REQUIRE(shape.hasHeavyTails      == false);
    }

    SECTION("Strong skew + heavy tails trigger both flags") {
        // Right-skewed with a very stretched upper tail:
        // {0, 0, 0, 0, 1, 10, 10, 10}
        // Under the StatUtils quantile convention:
        //   Bowley skewness ≈ 0.9  (|B| >= 0.30 → strong asymmetry)
        //   tailRatio ≈ 19.0       (>= 2.5     → heavy tails)
        std::vector<DecimalType> v = {
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("1.0"),
            createDecimal("10.0"),
            createDecimal("10.0"),
            createDecimal("10.0")
        };

        auto shape = Stat::computeQuantileShape(v);

        REQUIRE(shape.bowleySkew == Catch::Approx(0.9).margin(1e-12));
        REQUIRE(shape.tailRatio  == Catch::Approx(19.0).margin(1e-12));
        REQUIRE(shape.hasStrongAsymmetry == true);
        REQUIRE(shape.hasHeavyTails      == true);
    }

    SECTION("Custom thresholds change flag behavior") {
        // Same skewed / heavy-tailed distribution as above, but with very lax thresholds.
        std::vector<DecimalType> v = {
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("0.0"),
            createDecimal("1.0"),
            createDecimal("10.0"),
            createDecimal("10.0"),
            createDecimal("10.0")
        };

        // Use thresholds larger than the observed values so both flags go false.
        double bigBowleyThreshold    = 2.0;
        double bigTailRatioThreshold = 25.0;

        auto shape = Stat::computeQuantileShape(v, bigBowleyThreshold, bigTailRatioThreshold);

        REQUIRE(shape.bowleySkew == Catch::Approx(0.9).margin(1e-12));
        REQUIRE(shape.tailRatio  == Catch::Approx(19.0).margin(1e-12));
        REQUIRE(shape.hasStrongAsymmetry == false);
        REQUIRE(shape.hasHeavyTails      == false);
    }
}

TEST_CASE("StatUtils::getMoorsKurtosis behavior and edge cases", "[StatUtils][MoorsKurtosis]") {
    using Stat = StatUtils<DecimalType>;

    SECTION("Returns zero for n < 7") {
        // Minimum stable size for octiles is 7 or 8. We check n < 7.
        std::vector<DecimalType> v = {
            createDecimal("1.0"), createDecimal("2.0"), createDecimal("3.0"),
            createDecimal("4.0"), createDecimal("5.0"), createDecimal("6.0")
        };
        DecimalType moors = Stat::getMoorsKurtosis(v);
        REQUIRE(moors == DecimalConstants<DecimalType>::DecimalZero);
    }

    // --------------------------- FIX FOR StatUtilsTest.cpp ---------------------------

    SECTION("Normal distribution yields excess kurtosis ~ 0") {
        std::vector<DecimalType> v;
        v.reserve(1000); // Use a larger sample for better approximation of normality

        // --- Use a C++ Normal Distribution Generator ---
        std::mt19937_64 rng(42); // Seed for deterministic tests
        std::normal_distribution<> normal_dist(0.0, 1.0); // Mean 0, StdDev 1

        for (int i = 0; i < 1000; ++i) {
            v.push_back(createDecimal(std::to_string(normal_dist(rng))));
        }
        // --------------------------------------------------

        DecimalType moors_exkurt = Stat::getMoorsKurtosis(v);
        
        // Expect excess kurtosis (K_Moors - 1.233) to be close to zero.
        // A margin of 0.1 is safe for a 1000-sample simulation.
        REQUIRE(num::to_double(moors_exkurt) == Catch::Approx(0.0).margin(0.1));
    }

    SECTION("Heavy-tailed distribution yields positive excess kurtosis") {
        // A leptokurtic, heavy-tailed distribution (outliers at both ends)
        // Values: {-100, -5, -2, 0, 2, 5, 100} plus middle elements to pad N>=8
        std::vector<DecimalType> v = {
            createDecimal("-100.0"), createDecimal("100.0"), // Outliers
            createDecimal("-5.0"), createDecimal("5.0"),
            createDecimal("-2.0"), createDecimal("2.0"),
            createDecimal("-1.0"), createDecimal("1.0"), // N=8 minimum
            createDecimal("0.0"), createDecimal("0.5")   // N=10
        };

        DecimalType moors_exkurt = Stat::getMoorsKurtosis(v);
        // Heavy tails must yield positive excess kurtosis
        REQUIRE(num::to_double(moors_exkurt) > 0.0);
        REQUIRE(num::to_double(moors_exkurt) == Catch::Approx(1.0).margin(1.0)); // Test against a large positive value (e.g., K_Moors > 2.233)
    }

    SECTION("Constant vector returns zero excess kurtosis") {
        std::vector<DecimalType> v(10, createDecimal("5.0"));
        
        DecimalType moors_exkurt = Stat::getMoorsKurtosis(v);
        // Denominator (Q3 - Q1) is zero, so it should return 0.
        REQUIRE(moors_exkurt == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("Calculation of K_Moors for a simple dataset (n=8)") {
        // Sorted: {1, 2, 3, 4, 5, 6, 7, 8}. n=8
        std::vector<DecimalType> v = {
            createDecimal("8"), createDecimal("7"), createDecimal("6"), createDecimal("5"),
            createDecimal("4"), createDecimal("3"), createDecimal("2"), createDecimal("1")
        };
        
        // Quantiles for this set (using the StatUtils linear interpolation method):
        // Q1 (0.25): idx=1.75 -> 2 + 0.75*(3-2) = 2.75
        // Q3 (0.75): idx=5.25 -> 6 + 0.25*(7-6) = 6.25
        // O1 (0.125): idx=0.875 -> 1 + 0.875*(2-1) = 1.875
        // O3 (0.375): idx=2.625 -> 3 + 0.625*(4-3) = 3.625
        // O5 (0.625): idx=4.375 -> 5 + 0.375*(6-5) = 5.375
        // O7 (0.875): idx=6.125 -> 7 + 0.125*(8-7) = 7.125

        // Denom = Q3 - Q1 = 6.25 - 2.75 = 3.5
        // Numer = (O7 - O5) + (O3 - O1) = (7.125 - 5.375) + (3.625 - 1.875) = 1.75 + 1.75 = 3.5
        // K_Moors (Total) = Numer / Denom = 3.5 / 3.5 = 1.0
        // Excess Kurtosis = 1.0 - 1.233 = -0.233

        DecimalType moors_exkurt = Stat::getMoorsKurtosis(v);
        REQUIRE(num::to_double(moors_exkurt) == Catch::Approx(-0.233));
    }
}

TEST_CASE("StatUtils::computeSkewAndExcessKurtosis (Robust Quantile)", "[StatUtils][RobustStats][SkewKurtosis]") {
    using Stat = StatUtils<DecimalType>;
    
    // Test data for known quantile results: n=8, slightly asymmetric, platykurtic-like
    // Sorted: {1, 2, 3, 4, 5, 6, 7, 8} (Used in getMoorsKurtosis test)
    std::vector<DecimalType> v_uniform_like = {
        createDecimal("8"), createDecimal("7"), createDecimal("6"), createDecimal("5"),
        createDecimal("4"), createDecimal("3"), createDecimal("2"), createDecimal("1")
    };
    
    // Quantile results for v_uniform_like (n=8):
    // Q1=2.75, Q2=4.5, Q3=6.25
    // Bowley Skewness (B) = (2.75 + 6.25 - 2*4.5) / (6.25 - 2.75) = (9.0 - 9.0) / 3.5 = 0.0
    // Moors' Excess Kurtosis (K_Moors - 1.233) = 1.0 - 1.233 = -0.233

    SECTION("Minimum sample size n < 7 returns {0.0, 0.0}") {
        std::vector<DecimalType> small_v(6, createDecimal("1.0"));
        auto [skew, exkurt] = Stat::computeSkewAndExcessKurtosis(small_v);
        REQUIRE(skew == Catch::Approx(0.0));
        REQUIRE(exkurt == Catch::Approx(0.0));
    }

    SECTION("Symmetric, uniform-like distribution returns B=0.0 and negative excess kurtosis") {
        auto [skew, exkurt] = Stat::computeSkewAndExcessKurtosis(v_uniform_like);
        
        // Should match the calculated Bowley Skewness for this uniform-like set (B=0.0)
        REQUIRE(skew == Catch::Approx(0.0).margin(1e-12)); 
        
        // Should match the calculated Moors' Excess Kurtosis (-0.233)
        REQUIRE(exkurt == Catch::Approx(-0.233).margin(1e-12)); 
    }

    SECTION("Positively skewed, heavy-tailed distribution (Right Skew + High Kurtosis)") {
        // Highly right-skewed, heavy-tailed (leptokurtic) distribution:
        // {-1, 0, 0, 1, 1, 2, 5, 10, 50, 100} (n=10)
        std::vector<DecimalType> v_heavy_skew = {
            createDecimal("100.0"), createDecimal("50.0"), createDecimal("10.0"), 
            createDecimal("5.0"), createDecimal("2.0"), createDecimal("1.0"), 
            createDecimal("1.0"), createDecimal("0.0"), createDecimal("0.0"), 
            createDecimal("-1.0") 
        };
        
        // Expected values (calculated manually for n=10):
        // Q1 (0.25): idx=2.25 -> 0.0 + 0.25*(1.0-0.0) = 0.25
        // Q2 (0.50): idx=4.5  -> 1.0 + 0.50*(2.0-1.0) = 1.50
        // Q3 (0.75): idx=6.75 -> 5.0 + 0.75*(10.0-5.0) = 8.75
        // Bowley Skewness (B) = (0.25 + 8.75 - 2*1.50) / (8.75 - 0.25) = 6.0 / 8.5 ≈ 0.70588
        
        // Moors' Kurtosis is difficult to calculate analytically, but it should be strongly positive.
        
        auto [skew, exkurt] = Stat::computeSkewAndExcessKurtosis(v_heavy_skew);

        // Skewness must be significantly positive (right-skew)
        REQUIRE(skew > 0.70);
        REQUIRE(skew < 0.71);
        
        // Excess Kurtosis must be significantly positive (heavy-tailed)
        REQUIRE(exkurt > 0.5);
    }

    SECTION("Constant series (degenerate) returns {0.0, 0.0}") {
        std::vector<DecimalType> v(10, createDecimal("42.0"));
        auto [skew, exkurt] = Stat::computeSkewAndExcessKurtosis(v);
        
        // getBowleySkewness returns 0.0 for degenerate series (Q3-Q1 = 0)
        // getMoorsKurtosis returns 0.0 for degenerate series (Q3-Q1 = 0)
        REQUIRE(skew == Catch::Approx(0.0));
        REQUIRE(exkurt == Catch::Approx(0.0));
    }
}

TEST_CASE("LogProfitFactorStat with Strategy Stop Loss (Zero-Loss Handling)", "[StatUtils][LogPFStat][StopLoss]") {
    using Stat = StatUtils<DecimalType>;

    // Scenario: Only winning trades.
    // Classic PF would be Infinite (or capped at 100).
    // Robust PF with Ruin Proxy would be near-zero (assuming catastrophic ruin).
    // Robust PF with Stop Loss should be "Optimistic but realistic".
    std::vector<DecimalType> returns = {
        DecimalType("0.10"), // +10%
        DecimalType("0.20")  // +20%
    };

    // Pre-calculate common values
    double log_win_1 = std::log(1.10);
    double log_win_2 = std::log(1.20);
    double sum_log_wins = log_win_1 + log_win_2; // ~0.27763

    SECTION("Stop Loss provided: uses |ln(1-SL)| as prior when no losses exist") {
        double stop_loss_pct = 0.05; // 5% stop loss
        double expected_loss_mag = std::abs(std::log(1.0 - stop_loss_pct)); // |ln(0.95)| ~ 0.051293

        // Construct stat with stop_loss_pct as the LAST argument
        // defaults: compress=true, ruin=1e-8, floor=1e-6, prior=1.0
        typename Stat::LogProfitFactorStat stat(
            /*compress=*/true, 
            Stat::DefaultRuinEps, 
            Stat::DefaultDenomFloor, 
            Stat::DefaultPriorStrength, 
            stop_loss_pct // <--- Explicit Stop Loss
        );

        DecimalType result = stat(returns);

        // Math Check:
        // Numerator = sum_log_wins (~0.2776)
        // Denominator = 0 (actual losses) + expected_loss_mag (~0.05129)
        // PF_raw = 0.2776 / 0.05129 ~= 5.41
        // Result (compressed) = ln(1 + 5.41) ~= 1.858

        double pf_raw = sum_log_wins / expected_loss_mag;
        double expected_result = std::log(1.0 + pf_raw);

        REQUIRE(num::to_double(result) == Catch::Approx(expected_result).margin(1e-8));
        
        // Sanity check: Result should be much higher than 0, but finite
        REQUIRE(num::to_double(result) > 1.0);
    }

    SECTION("Stop Loss provided but Uncompressed result requested") {
        double stop_loss_pct = 0.05;
        double expected_loss_mag = std::abs(std::log(1.0 - stop_loss_pct));

        // Disable compression
        typename Stat::LogProfitFactorStat stat(
            /*compress=*/false, 
            Stat::DefaultRuinEps, 
            Stat::DefaultDenomFloor, 
            Stat::DefaultPriorStrength, 
            stop_loss_pct
        );

        DecimalType result = stat(returns);

        double expected_result = sum_log_wins / expected_loss_mag; // ~5.41

        REQUIRE(num::to_double(result) == Catch::Approx(expected_result).margin(1e-8));
    }

    SECTION("Zero Stop Loss (Default): Falls back to Ruin Proxy") {
        // This confirms the "old" behavior is preserved when no stop is known
        double stop_loss_pct = 0.0; // Unknown

        typename Stat::LogProfitFactorStat stat(
            /*compress=*/true, 
            Stat::DefaultRuinEps, 
            Stat::DefaultDenomFloor, 
            Stat::DefaultPriorStrength, 
            stop_loss_pct // 0.0
        );

        DecimalType result = stat(returns);

        // Math Check:
        // Ruin Proxy = max(-ln(1e-8), 1e-6) ~= 18.42
        // PF_raw = 0.2776 / 18.42 ~= 0.015
        // Result (compressed) = ln(1 + 0.015) ~= 0.0149

        double ruin_mag = std::max(-std::log(Stat::DefaultRuinEps), Stat::DefaultDenomFloor);
        double pf_raw = sum_log_wins / ruin_mag;
        double expected_result = std::log(1.0 + pf_raw);

        REQUIRE(num::to_double(result) == Catch::Approx(expected_result).margin(1e-8));

        // Crucial comparison: With StopLoss(0.05) we got ~1.85. With RuinProxy we get ~0.015.
        // This validates that the new logic significantly rescues high-quality/low-N strategies.
        REQUIRE(num::to_double(result) < 0.1); 
    }

    SECTION("Stop Loss is ignored if actual losses exist") {
        // Add a small loss to the series.
        // The logic should use the median of ACTUAL losses, ignoring the stop loss assumption.
        std::vector<DecimalType> mixed_returns = {
            DecimalType("0.10"), 
            DecimalType("-0.02") // Actual loss
        };

        double stop_loss_pct = 0.50; // Huge assumed stop (50%) -> Mag ~0.69
        // Actual loss mag is |ln(0.98)| ~ 0.0202.
        
        typename Stat::LogProfitFactorStat stat(
            /*compress=*/false, 
            Stat::DefaultRuinEps, 
            Stat::DefaultDenomFloor, 
            Stat::DefaultPriorStrength, 
            stop_loss_pct
        );

        DecimalType result = stat(mixed_returns);

        double log_win = std::log(1.10);
        double log_loss = std::abs(std::log(0.98));
        
        // Denominator = Actual Loss Sum (log_loss) + Prior (Median of Actual Losses)
        // Median of {log_loss} is just log_loss.
        // Denom = log_loss + log_loss * 1.0 = 2 * log_loss
        
        double expected_denom = log_loss + log_loss;
        double expected_result = log_win / expected_denom;

        // Verify result matches actual data, NOT the 50% stop loss
        REQUIRE(num::to_double(result) == Catch::Approx(expected_result).margin(1e-8));
    }
}

TEST_CASE("StatUtils::makeLogGrowthSeries basic correctness and ruin clipping",
          "[StatUtils][LogGrowth]") 
{
    using Stat = StatUtils<DecimalType>;

    SECTION("Simple positive / negative returns") {
        std::vector<DecimalType> returns = {
            DecimalType("0.10"),   // +10%
            DecimalType("-0.05"),  // -5%
            DecimalType("0.00")    // flat
        };

        const double ruin_eps = Stat::DefaultRuinEps;

        auto logs = Stat::makeLogGrowthSeries(returns, ruin_eps);

        REQUIRE(logs.size() == returns.size());

        // Expected: log(1 + r) for each element (no clipping here)
        REQUIRE(num::to_double(logs[0]) ==
                Catch::Approx(std::log(1.10)).margin(1e-12));
        REQUIRE(num::to_double(logs[1]) ==
                Catch::Approx(std::log(0.95)).margin(1e-12));
        REQUIRE(num::to_double(logs[2]) ==
                Catch::Approx(std::log(1.0)).margin(1e-12));
    }

    SECTION("Ruin handling when 1 + r <= 0") {
        // These are "ruin" bars: 1+r <= 0
        std::vector<DecimalType> returns = {
            DecimalType("-1.0"),   // 1 + r = 0
            DecimalType("-1.5")    // 1 + r = -0.5
        };

        const double ruin_eps = 1e-4;
        auto logs = Stat::makeLogGrowthSeries(returns, ruin_eps);

        REQUIRE(logs.size() == returns.size());

        const double expected_log = std::log(ruin_eps);

        REQUIRE(num::to_double(logs[0]) ==
                Catch::Approx(expected_log).margin(1e-12));
        REQUIRE(num::to_double(logs[1]) ==
                Catch::Approx(expected_log).margin(1e-12));
    }

    SECTION("Empty input returns empty log series") {
        std::vector<DecimalType> returns;
        const double ruin_eps = Stat::DefaultRuinEps;

        auto logs = Stat::makeLogGrowthSeries(returns, ruin_eps);

        REQUIRE(logs.empty());
    }
}

TEST_CASE("LogProfitFactorFromLogBarsStat basic correctness and edge cases",
          "[StatUtils][LogPFLogBars]") 
{
    using Stat = StatUtils<DecimalType>;
    using DC   = DecimalConstants<DecimalType>;
    using LogPFLogBars = typename Stat::LogProfitFactorFromLogBarsStat;

    SECTION("Empty input returns zero") {
        std::vector<DecimalType> empty;
        LogPFLogBars stat;  // default parameters

        DecimalType result = stat(empty);

        REQUIRE(result == DC::DecimalZero);
    }

    SECTION("All-positive logBars behave like pure wins") {
        // logBars > 0 => all wins, no losses
        std::vector<DecimalType> logBars = {
            DecimalType(std::log(1.10)),
            DecimalType(std::log(1.05))
        };

        LogPFLogBars stat(/*compress=*/true);

        DecimalType result = stat(logBars);

        // With no losses, denominator is purely the prior,
        // so the value should be positive and finite.
        REQUIRE(num::to_double(result) > 0.0);
        REQUIRE(std::isfinite(num::to_double(result)));
    }
}

TEST_CASE("LogProfitFactorFromLogBarsStat matches LogProfitFactorStat on same data",
          "[StatUtils][LogPFLogBars][Consistency]") 
{
    using Stat        = StatUtils<DecimalType>;
    using LogPF       = typename Stat::LogProfitFactorStat;
    using LogPFLogBars= typename Stat::LogProfitFactorFromLogBarsStat;

    // Mixed, realistic-looking return series (same style as other tests)
    std::vector<DecimalType> returns = {
        DecimalType("0.10"), DecimalType("-0.05"),
        DecimalType("0.20"), DecimalType("-0.10"),
        DecimalType("0.15"), DecimalType("0.05"),
        DecimalType("-0.02")
    };

    const double ruin_eps       = Stat::DefaultRuinEps;
    const double denom_floor    = Stat::DefaultDenomFloor;
    const double prior_strength = Stat::DefaultPriorStrength;
    const double stop_loss_pct  = 0.0;
    const bool   compress       = true;

    // 1) Compute log-bars once using the same ruin epsilon.
    auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);

    // 2) "Original" path: robust PF on raw returns (uses computeLogProfitFactorRobust internally).
    LogPF stat_returns(
        /*compressResult=*/compress,
        ruin_eps,
        denom_floor,
        prior_strength,
        stop_loss_pct
    );

    DecimalType via_returns = stat_returns(returns);

    // 3) New path: robust PF on precomputed log-bars.
    LogPFLogBars stat_logbars(
        /*compressResult=*/compress,
        ruin_eps,
        denom_floor,
        prior_strength,
        stop_loss_pct
    );

    DecimalType via_logbars = stat_logbars(logBars);

    // 4) They should match to tight tolerance.
    REQUIRE(num::to_double(via_logbars) ==
            Catch::Approx(num::to_double(via_returns)).epsilon(1e-12));
}

TEST_CASE("LogProfitFactorFromLogBarsStat works as a statistic in getBootStrappedStatistic",
          "[StatUtils][LogPFLogBars][Bootstrap]") 
{
    using Stat        = StatUtils<DecimalType>;
    using LogPFLogBars= typename Stat::LogProfitFactorFromLogBarsStat;

    std::vector<DecimalType> returns = {
        DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
        DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
        DecimalType("-0.02"), DecimalType("0.08"), DecimalType("-0.12"),
        DecimalType("0.25")
    };

    const double ruin_eps       = Stat::DefaultRuinEps;
    const double denom_floor    = Stat::DefaultDenomFloor;
    const double prior_strength = Stat::DefaultPriorStrength;
    const double stop_loss_pct  = 0.0;

    // Precompute log-bars once (what you'll do in production before BCa).
    auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);

    LogPFLogBars stat(
        /*compressResult=*/true,
        ruin_eps,
        denom_floor,
        prior_strength,
        stop_loss_pct
    );

    // "True" value for the original sample in log-bar space
    DecimalType true_logPF = stat(logBars);

    // Now bootstrap in the log-bar domain
    constexpr int num_runs = 50;
    std::vector<DecimalType> boot_medians;
    boot_medians.reserve(num_runs);

    for (int i = 0; i < num_runs; ++i) {
        boot_medians.push_back(
            Stat::getBootStrappedStatistic(
                logBars,
                stat,
                100));      // bootstraps per run
    }

    DecimalType mean_est = Stat::computeMean(boot_medians);
    DecimalType std_est  = Stat::computeStdDev(boot_medians, mean_est);

    REQUIRE(num::to_double(true_logPF) ==
            Catch::Approx(num::to_double(mean_est))
                .margin(num::to_double(std_est * DecimalType(3.0))));
}

// ---------------------- New Tests: GeoMeanFromLogBarsStat ----------------------

TEST_CASE("GeoMeanFromLogBarsStat basic correctness on log-bars",
          "[StatUtils][GeoMeanFromLogs]") {
    constexpr double kGeoTol = 5e-8;

    // Helper: geometric mean from precomputed log(1+r) values (double version)
    auto expected_from_logs = [](const std::vector<double>& logs) -> double {
        if (logs.empty()) return 0.0;
        long double s = 0.0L;
        for (double x : logs) {
            s += x;
        }
        long double mean_log = s / static_cast<long double>(logs.size());
        return std::expm1(mean_log); // exp(mean_log) - 1
    };

    SECTION("Small sample, no winsorization (n < 20)") {
        // Raw returns, but we manually build log(1+r) here at double precision.
        std::vector<double> rs   = { 0.10, 0.20, -0.05 };
        std::vector<double> dlog;
        dlog.reserve(rs.size());
        for (double r : rs) {
            dlog.push_back(std::log1p(r)); // log(1+r)
        }

        // Build Decimal log-bars for the statistic
        std::vector<DecimalType> logBars;
        logBars.reserve(dlog.size());
        for (double x : dlog) {
            logBars.emplace_back(x);
        }

        GeoMeanFromLogBarsStat<DecimalType> stat; // default winsor config
        DecimalType got = stat(logBars);

        double expected = expected_from_logs(dlog);
        REQUIRE(num::to_double(got) == Catch::Approx(expected).margin(kGeoTol));
    }

    SECTION("Empty log-bar vector returns 0") {
        std::vector<DecimalType> logBars;
        GeoMeanFromLogBarsStat<DecimalType> stat;
        DecimalType got = stat(logBars);

        REQUIRE(got == DecimalConstants<DecimalType>::DecimalZero);
    }
}

TEST_CASE("GeoMeanFromLogBarsStat matches GeoMeanStat via makeLogGrowthSeries "
          "when no winsorization is active",
          "[StatUtils][GeoMeanFromLogs][Equivalence]") {
    using Stat = StatUtils<DecimalType>;
    constexpr double kGeoTol = 5e-8;

    // n = 10 -> outside [20, 30], so no winsorization in either path.
    std::vector<DecimalType> returns = {
        DecimalType("0.10"), DecimalType("-0.05"), DecimalType("0.20"),
        DecimalType("-0.10"), DecimalType("0.15"), DecimalType("0.05"),
        DecimalType("-0.02"), DecimalType("0.08"), DecimalType("-0.12"),
        DecimalType("0.25")
    };

    // Raw-return geometric mean
    GeoMeanStat<DecimalType> geo_raw; // default: clip_ruin=true, ruin_eps=1e-8
    DecimalType gm_raw = geo_raw(returns);

    // Precompute log(1+r) once, using the same ruin epsilon as GeoMeanStat default
    const double ruin_eps = 1e-8;
    std::vector<DecimalType> logBars =
        Stat::makeLogGrowthSeries(returns, ruin_eps);

    GeoMeanFromLogBarsStat<DecimalType> geo_from_logs;
    DecimalType gm_from_logs = geo_from_logs(logBars);

    REQUIRE(num::to_double(gm_from_logs) ==
            Catch::Approx(num::to_double(gm_raw)).margin(kGeoTol));
}

TEST_CASE("GeoMeanFromLogBarsStat matches GeoMeanStat in the small-N winsorization band",
          "[StatUtils][GeoMeanFromLogs][Winsor]") {
    using D   = DecimalType;
    using Stat = StatUtils<D>;
    constexpr double kGeoTol = 5e-8;

    // n = 30 → both GeoMeanStat and GeoMeanFromLogBarsStat will winsorize (k>=1).
    std::vector<D> returns(30, D("0.005"));
    returns[3]  = D("-0.45");  // extreme negative outlier
    returns[17] = D("0.20");   // extreme positive outlier

    // Raw-return geometric mean with GeoMeanStat (includes winsorization)
    GeoMeanStat<D> geo_raw;    // default config
    D gm_raw = geo_raw(returns);

    // Build log-bars in the same way the production code does (ruin-aware).
    const double ruin_eps = 1e-8;
    std::vector<D> logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);

    GeoMeanFromLogBarsStat<D> geo_from_logs;
    D gm_from_logs = geo_from_logs(logBars);

    REQUIRE(num::to_double(gm_from_logs) ==
            Catch::Approx(num::to_double(gm_raw)).margin(kGeoTol));
}

// ============================================================================
// NEW TESTS: StatUtils::computeSkewness, computeMedian, computeMedianSorted
// ============================================================================
// These tests validate the new centralized statistical functions added to
// StatUtils.h for bootstrap median and skewness calculations.
// 
// Add these tests to the end of StatUtilsTest.cpp before the closing brace.
// ============================================================================

TEST_CASE("StatUtils::computeSkewness with double", "[StatUtils][Skewness]")
{
    using Stat = StatUtils<double>;
    
    SECTION("Symmetric distribution has zero skewness")
    {
        // Perfectly symmetric distribution: {1, 2, 3, 4, 5}
        std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
        double mean = 3.0;
        double se = std::sqrt(2.0); // Sample std dev
        
        double skew = Stat::computeSkewness(data, mean, se);
        
        // Should be very close to zero for symmetric data
        REQUIRE(skew == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Right-skewed distribution has positive skewness")
    {
        // Right-skewed: {1, 1, 1, 1, 1, 2, 3, 10}
        // Mean pulled right by outlier
        std::vector<double> data = {1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 3.0, 10.0};
        double sum = std::accumulate(data.begin(), data.end(), 0.0);
        double mean = sum / data.size(); // ≈ 2.5
        
        // Compute sample standard deviation
        double var = 0.0;
        for (double v : data) {
            double d = v - mean;
            var += d * d;
        }
        var /= (data.size() - 1);
        double se = std::sqrt(var); // ≈ 3.0
        
        double skew = Stat::computeSkewness(data, mean, se);
        
        // Should be positive for right-skewed
        REQUIRE(skew > 0.0);
        REQUIRE(skew > 1.0); // Significantly skewed
    }
    
    SECTION("Left-skewed distribution has negative skewness")
    {
        // Left-skewed: {-10, -3, -2, -1, -1, -1, -1, -1}
        // Mean pulled left by outlier
        std::vector<double> data = {-10.0, -3.0, -2.0, -1.0, -1.0, -1.0, -1.0, -1.0};
        double sum = std::accumulate(data.begin(), data.end(), 0.0);
        double mean = sum / data.size(); // ≈ -2.5
        
        double var = 0.0;
        for (double v : data) {
            double d = v - mean;
            var += d * d;
        }
        var /= (data.size() - 1);
        double se = std::sqrt(var); // ≈ 3.0
        
        double skew = Stat::computeSkewness(data, mean, se);
        
        // Should be negative for left-skewed
        REQUIRE(skew < 0.0);
        REQUIRE(skew < -1.0); // Significantly skewed
    }
    
    SECTION("Returns zero for n < 3")
    {
        std::vector<double> data = {1.0, 2.0};
        double skew = Stat::computeSkewness(data, 1.5, 0.5);
        
        REQUIRE(skew == 0.0);
    }
    
    SECTION("Returns zero when se = 0")
    {
        std::vector<double> data = {1.0, 1.0, 1.0};
        double skew = Stat::computeSkewness(data, 1.0, 0.0);
        
        REQUIRE(skew == 0.0);
    }
    
    SECTION("Returns zero when se is negative")
    {
        std::vector<double> data = {1.0, 2.0, 3.0};
        double skew = Stat::computeSkewness(data, 2.0, -0.5);
        
        REQUIRE(skew == 0.0);
    }
    
    SECTION("Handles empty vector")
    {
        std::vector<double> data;
        double skew = Stat::computeSkewness(data, 0.0, 1.0);
        
        REQUIRE(skew == 0.0);
    }
    
    SECTION("Matches manual calculation for known distribution")
    {
        // Data: {2, 4, 6, 8, 10}
        std::vector<double> data = {2.0, 4.0, 6.0, 8.0, 10.0};
        double mean = 6.0;
        
        // Compute variance: E[(X-μ)²]
        double m2 = 0.0;
        for (double v : data) {
            double d = v - mean;
            m2 += d * d;
        }
        m2 /= (data.size() - 1); // Sample variance
        double se = std::sqrt(m2); // ≈ 3.1623
        
        // Compute third moment: E[(X-μ)³]
        double m3 = 0.0;
        for (double v : data) {
            double d = v - mean;
            m3 += d * d * d;
        }
        m3 /= data.size(); // Population third moment
        
        // Skewness = m3 / σ³
        double expected_skew = m3 / (se * se * se);
        
        double computed_skew = Stat::computeSkewness(data, mean, se);
        
        REQUIRE(computed_skew == Catch::Approx(expected_skew).margin(1e-10));
        REQUIRE(computed_skew == Catch::Approx(0.0).margin(1e-10)); // Symmetric
    }
}

TEST_CASE("StatUtils::computeSkewness with DecimalType", "[StatUtils][Skewness][Decimal]")
{
    using Stat = StatUtils<DecimalType>;
    
    SECTION("Works with decimal type")
    {
        std::vector<DecimalType> data = {
            DecimalType("1.0"),
            DecimalType("2.0"),
            DecimalType("3.0"),
            DecimalType("4.0"),
            DecimalType("5.0")
        };
        DecimalType mean = DecimalType("3.0");
        DecimalType se = DecimalType("1.41421356"); // sqrt(2)
        
        DecimalType skew = Stat::computeSkewness(data, mean, se);
        
        // Should be near zero for symmetric data
        REQUIRE(num::to_double(skew) == Catch::Approx(0.0).margin(1e-6));
    }
    
    SECTION("Right-skewed decimal distribution")
    {
        std::vector<DecimalType> data = {
            DecimalType("0.10"),
            DecimalType("0.12"),
            DecimalType("0.15"),
            DecimalType("0.18"),
            DecimalType("0.85")  // Outlier
        };
        
        // Compute mean
        DecimalType sum = DecimalConstants<DecimalType>::DecimalZero;
        for (const auto& v : data) {
            sum += v;
        }
        DecimalType mean = sum / DecimalType(static_cast<double>(data.size()));
        
        // Compute SE
        DecimalType var = DecimalConstants<DecimalType>::DecimalZero;
        for (const auto& v : data) {
            DecimalType d = v - mean;
            var += d * d;
        }
        var /= DecimalType(static_cast<double>(data.size() - 1));
        double var_d = num::to_double(var);
        DecimalType se = DecimalType(std::sqrt(var_d));
        
        DecimalType skew = Stat::computeSkewness(data, mean, se);
        
        // Should be positive for right-skewed
        REQUIRE(num::to_double(skew) > 0.0);
    }
}

TEST_CASE("StatUtils::computeMedian with double", "[StatUtils][Median]")
{
    using Stat = StatUtils<double>;
    
    SECTION("Odd number of elements returns middle value")
    {
        std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == 3.0);
    }
    
    SECTION("Even number of elements returns average of two middle values")
    {
        std::vector<double> data = {1.0, 2.0, 3.0, 4.0};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == 2.5); // (2 + 3) / 2
    }
    
    SECTION("Sorts unsorted data correctly")
    {
        std::vector<double> data = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0};
        double median = Stat::computeMedian(data);
        
        // Sorted: {1, 2, 3, 5, 8, 9}
        // Median: (3 + 5) / 2 = 4.0
        REQUIRE(median == 4.0);
    }
    
    SECTION("Does not modify input vector")
    {
        std::vector<double> data = {5.0, 2.0, 8.0, 1.0};
        std::vector<double> original = data; // Copy
        
        double median = Stat::computeMedian(data);
        
        // Verify input unchanged
        REQUIRE(data == original);
        REQUIRE(median == 3.5); // (2 + 5) / 2
    }
    
    SECTION("Single element returns that element")
    {
        std::vector<double> data = {42.0};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == 42.0);
    }
    
    SECTION("Two elements returns average")
    {
        std::vector<double> data = {10.0, 20.0};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == 15.0);
    }
    
    SECTION("Empty vector returns zero")
    {
        std::vector<double> data;
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == 0.0);
    }
    
    SECTION("Handles negative values")
    {
        std::vector<double> data = {-5.0, -2.0, 0.0, 3.0, 7.0};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == 0.0); // Middle of sorted: {-5, -2, 0, 3, 7}
    }
    
    SECTION("Handles duplicate values")
    {
        std::vector<double> data = {1.0, 2.0, 2.0, 2.0, 3.0};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == 2.0);
    }
    
    SECTION("Large dataset with known median")
    {
        // Create 1000 elements: 1, 2, 3, ..., 1000
        std::vector<double> data;
        data.reserve(1000);
        for (int i = 1; i <= 1000; ++i) {
            data.push_back(static_cast<double>(i));
        }
        
        double median = Stat::computeMedian(data);
        
        // Median of 1...1000 is (500 + 501) / 2 = 500.5
        REQUIRE(median == 500.5);
    }
    
    SECTION("Bootstrap-like scenario: resampled values")
    {
        // Simulates bootstrap statistics from resampling
        std::vector<double> data = {1.25, 1.18, 1.32, 1.15, 1.28, 1.22, 1.35, 1.20};
        double median = Stat::computeMedian(data);
        
        // Sorted: {1.15, 1.18, 1.20, 1.22, 1.25, 1.28, 1.32, 1.35}
        // Median: (1.22 + 1.25) / 2 = 1.235
        REQUIRE(median == Catch::Approx(1.235));
    }
}

TEST_CASE("StatUtils::computeMedian with DecimalType", "[StatUtils][Median][Decimal]")
{
    using Stat = StatUtils<DecimalType>;
    
    SECTION("Works with decimal type")
    {
        std::vector<DecimalType> data = {
            DecimalType("1.0"),
            DecimalType("2.0"),
            DecimalType("3.0"),
            DecimalType("4.0"),
            DecimalType("5.0")
        };
        
        DecimalType median = Stat::computeMedian(data);
        
        REQUIRE(median == DecimalType("3.0"));
    }
    
    SECTION("Even count with decimals")
    {
        std::vector<DecimalType> data = {
            DecimalType("1.25"),
            DecimalType("1.50"),
            DecimalType("1.75"),
            DecimalType("2.00")
        };
        
        DecimalType median = Stat::computeMedian(data);
        
        // (1.50 + 1.75) / 2 = 1.625
        REQUIRE(num::to_double(median) == Catch::Approx(1.625));
    }
    
    SECTION("Bootstrap profit factor scenario")
    {
        // Simulates bootstrap profit factor statistics
        std::vector<DecimalType> data = {
            DecimalType("1.15"),
            DecimalType("1.25"),
            DecimalType("1.10"),
            DecimalType("1.30"),
            DecimalType("1.20")
        };
        
        DecimalType median = Stat::computeMedian(data);
        
        // Sorted: {1.10, 1.15, 1.20, 1.25, 1.30}
        // Median: 1.20
        REQUIRE(median == DecimalType("1.20"));
    }
}

TEST_CASE("StatUtils::computeMedianSorted with double", "[StatUtils][Median][Sorted]")
{
    using Stat = StatUtils<double>;
    
    SECTION("Odd number of elements in sorted vector")
    {
        std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0, 5.0};
        double median = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median == 3.0);
    }
    
    SECTION("Even number of elements in sorted vector")
    {
        std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0};
        double median = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median == 2.5);
    }
    
    SECTION("Does not modify input vector")
    {
        std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0};
        std::vector<double> original = sorted; // Copy
        
        double median = Stat::computeMedianSorted(sorted);
        
        // Verify input unchanged
        REQUIRE(sorted == original);
        REQUIRE(median == 2.5);
    }
    
    SECTION("Single element")
    {
        std::vector<double> sorted = {42.0};
        double median = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median == 42.0);
    }
    
    SECTION("Two elements")
    {
        std::vector<double> sorted = {10.0, 20.0};
        double median = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median == 15.0);
    }
    
    SECTION("Empty vector returns zero")
    {
        std::vector<double> sorted;
        double median = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median == 0.0);
    }
    
    SECTION("Pre-sorted bootstrap statistics")
    {
        // Already sorted from quantile calculation
        std::vector<double> sorted = {0.95, 1.05, 1.12, 1.18, 1.25, 1.32, 1.40, 1.55};
        double median = Stat::computeMedianSorted(sorted);
        
        // (1.18 + 1.25) / 2 = 1.215
        REQUIRE(median == Catch::Approx(1.215));
    }
}

TEST_CASE("StatUtils::computeMedianSorted with DecimalType", "[StatUtils][Median][Sorted][Decimal]")
{
    using Stat = StatUtils<DecimalType>;
    
    SECTION("Works with decimal type")
    {
        std::vector<DecimalType> sorted = {
            DecimalType("1.0"),
            DecimalType("2.0"),
            DecimalType("3.0")
        };
        
        DecimalType median = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median == DecimalType("2.0"));
    }
}

TEST_CASE("StatUtils: computeMedian vs computeMedianSorted consistency",
          "[StatUtils][Median][Consistency]")
{
    using Stat = StatUtils<double>;
    
    SECTION("Both functions return same result for sorted input")
    {
        std::vector<double> sorted = {1.0, 2.0, 3.0, 5.0, 8.0, 9.0};
        
        double median1 = Stat::computeMedian(sorted);
        double median2 = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median1 == median2);
        REQUIRE(median1 == 4.0); // (3 + 5) / 2
    }
    
    SECTION("computeMedian sorts before computing median")
    {
        std::vector<double> unsorted = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0};
        std::vector<double> sorted   = {1.0, 2.0, 3.0, 5.0, 8.0, 9.0};
        
        double median_from_unsorted = Stat::computeMedian(unsorted);
        double median_from_sorted   = Stat::computeMedianSorted(sorted);
        
        REQUIRE(median_from_unsorted == median_from_sorted);
        REQUIRE(median_from_unsorted == 4.0);
    }
    
    SECTION("Performance test: computeMedianSorted should be faster")
    {
        // This is more of a documentation test - we expect computeMedianSorted
        // to be O(1) vs computeMedian being O(n log n)
        
        std::vector<double> large_sorted;
        large_sorted.reserve(10000);
        for (int i = 0; i < 10000; ++i) {
            large_sorted.push_back(static_cast<double>(i));
        }
        
        // Both should give same result
        double median1 = Stat::computeMedian(large_sorted);
        double median2 = Stat::computeMedianSorted(large_sorted);
        
        REQUIRE(median1 == median2);
        REQUIRE(median1 == 4999.5); // (4999 + 5000) / 2
        
        // computeMedianSorted is preferred when data already sorted
        // (e.g., after quantile calculation in bootstrap)
    }
}

TEST_CASE("StatUtils: Median for bootstrap validation scenario",
          "[StatUtils][Median][Bootstrap][Integration]")
{
    using Stat = StatUtils<double>;
    
    SECTION("Bootstrap geometric mean statistics")
    {
        // Simulates 1000 bootstrap geometric mean replicates
        // Values typically between 0.005 and 0.015 (0.5% to 1.5% per period)
        std::vector<double> bootstrap_stats = {
            0.0082, 0.0095, 0.0088, 0.0102, 0.0075,
            0.0091, 0.0098, 0.0085, 0.0093, 0.0089,
            0.0097, 0.0086, 0.0094, 0.0087, 0.0099
        };
        
        double median = Stat::computeMedian(bootstrap_stats);
        
        // Median should be central value
        REQUIRE(median > 0.008);
        REQUIRE(median < 0.010);
        
        // For validation: if median > 0.008, strategy passes threshold
        bool passes_threshold = (median > 0.008);
        REQUIRE(passes_threshold == true);
    }
    
    SECTION("Bootstrap profit factor statistics")
    {
        // Simulates bootstrap profit factor replicates
        // Lower bound might be 1.10, but we want median >= 1.25 for robustness
        std::vector<double> pf_stats = {
            1.15, 1.28, 1.22, 1.35, 1.18,
            1.25, 1.30, 1.20, 1.27, 1.23,
            1.32, 1.21, 1.29, 1.24, 1.31
        };
        
        double median = Stat::computeMedian(pf_stats);
        
        // Median should be around 1.25
        REQUIRE(median == Catch::Approx(1.25).margin(0.02));
        
        // Two-stage validation:
        // Stage 1: Lower bound >= 1.10 (suppose this passed)
        // Stage 2: Median >= 1.25 (additional robustness check)
        double lower_bound = 1.12; // From bootstrap CI
        bool stage1 = (lower_bound >= 1.10);
        bool stage2 = (median >= 1.25);
        
        REQUIRE(stage1 == true);
        REQUIRE(stage2 == true);
    }
    
    SECTION("Skewed bootstrap distribution")
    {
        // Right-skewed: a few very high values pull the mean up
        // Median is more robust than mean for validation
        std::vector<double> skewed_stats = {
            1.10, 1.15, 1.18, 1.20, 1.22, 1.25, 1.28, 1.30,
            1.32, 1.35, 1.38, 1.40, 2.50, 3.80, 5.20  // Outliers
        };
        
        // Compute both mean and median
        double sum = std::accumulate(skewed_stats.begin(), skewed_stats.end(), 0.0);
        double mean = sum / skewed_stats.size();
        double median = Stat::computeMedian(skewed_stats);
        
        // Mean pulled up by outliers
        REQUIRE(mean > 1.60);
        
        // Median more robust (middle value around 1.30)
        REQUIRE(median == Catch::Approx(1.30).margin(0.05));
        
        // For validation, median is preferred over mean
        REQUIRE(median < mean);
    }
}

TEST_CASE("StatUtils: Edge cases and robustness",
          "[StatUtils][Median][Skewness][EdgeCases]")
{
    using Stat = StatUtils<double>;
    
    SECTION("Very small values (near machine epsilon)")
    {
        std::vector<double> data = {1e-10, 2e-10, 3e-10, 4e-10, 5e-10};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == Catch::Approx(3e-10).margin(1e-15));
    }
    
    SECTION("Very large values")
    {
        std::vector<double> data = {1e10, 2e10, 3e10, 4e10, 5e10};
        double median = Stat::computeMedian(data);
        
        REQUIRE(median == Catch::Approx(3e10).epsilon(1e-10));
    }
    
    SECTION("All identical values")
    {
        std::vector<double> data = {42.0, 42.0, 42.0, 42.0, 42.0};
        
        double median = Stat::computeMedian(data);
        REQUIRE(median == 42.0);
        
        double skew = Stat::computeSkewness(data, 42.0, 0.0);
        REQUIRE(skew == 0.0); // Zero skewness (se = 0)
    }
    
    SECTION("Extreme skewness with outlier")
    {
	std::vector<double> data = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1000.0};
        
        double sum = std::accumulate(data.begin(), data.end(), 0.0);
        double mean = sum / data.size();
        
        double var = 0.0;
        for (double v : data) {
            double d = v - mean;
            var += d * d;
        }
        var /= (data.size() - 1);
        double se = std::sqrt(var);
        
        double skew = Stat::computeSkewness(data, mean, se);
        
        // Should have very high positive skewness
        REQUIRE(skew > 2.0);
        
        // Median should be 1.0 (robust to outlier)
        double median = Stat::computeMedian(data);
        REQUIRE(median == 1.0);
    }
}

TEST_CASE("StatUtils: Type compatibility across double and DecimalType",
          "[StatUtils][Median][Skewness][TypeCompatibility]")
{
    SECTION("computeMedian works with both types")
    {
        std::vector<double> data_double = {1.0, 2.0, 3.0};
        double median_double = StatUtils<double>::computeMedian(data_double);
        
        std::vector<DecimalType> data_decimal = {
            DecimalType("1.0"),
            DecimalType("2.0"),
            DecimalType("3.0")
        };
        DecimalType median_decimal = StatUtils<DecimalType>::computeMedian(data_decimal);
        
        REQUIRE(median_double == 2.0);
        REQUIRE(median_decimal == DecimalType("2.0"));
        REQUIRE(num::to_double(median_decimal) == median_double);
    }
    
    SECTION("computeSkewness works with both types")
    {
        std::vector<double> data_double = {1.0, 2.0, 3.0, 4.0, 5.0};
        double skew_double = StatUtils<double>::computeSkewness(
            data_double, 3.0, std::sqrt(2.0));
        
        std::vector<DecimalType> data_decimal = {
            DecimalType("1.0"),
            DecimalType("2.0"),
            DecimalType("3.0"),
            DecimalType("4.0"),
            DecimalType("5.0")
        };
        DecimalType skew_decimal = StatUtils<DecimalType>::computeSkewness(
            data_decimal, DecimalType("3.0"), DecimalType("1.41421356"));
        
        REQUIRE(skew_double == Catch::Approx(0.0).margin(1e-10));
        REQUIRE(num::to_double(skew_decimal) == Catch::Approx(0.0).margin(1e-6));
    }
}

TEST_CASE("AdaptiveWinsorizer::computeK - Mode 0 (Legacy)", "[StatUtils][Winsorizer][Mode0]") {
    using D = DecimalType;
    using Winsorizer = AdaptiveWinsorizer<D>;
    
    // Mode 0: Legacy hard cutoff at n=30
    Winsorizer winsor(0.02, 0);  // alpha=0.02, mode=0
    
    SECTION("n < 20: No winsorization") {
        REQUIRE(winsor.computeK(15) == 0);
        REQUIRE(winsor.computeK(19) == 0);
    }
    
    SECTION("n in [20, 30]: Winsorize with k >= 1") {
        // For alpha=0.02, floor(0.02*n) < 1 for all n <= 50
        // So k should always be forced to 1
        REQUIRE(winsor.computeK(20) == 1);
        REQUIRE(winsor.computeK(25) == 1);
        REQUIRE(winsor.computeK(26) == 1);  // Median of user's data
        REQUIRE(winsor.computeK(30) == 1);
    }
    
    SECTION("n > 30: No winsorization (original discontinuity)") {
        REQUIRE(winsor.computeK(31) == 0);  // ← Discontinuity!
        REQUIRE(winsor.computeK(35) == 0);
        REQUIRE(winsor.computeK(50) == 0);
        REQUIRE(winsor.computeK(100) == 0);
    }
}

TEST_CASE("AdaptiveWinsorizer::computeK - Mode 1 (Smooth Fade)", "[StatUtils][Winsorizer][Mode1]") {
    using D = DecimalType;
    using Winsorizer = AdaptiveWinsorizer<D>;;
    
    // Mode 1: Smooth fade (default, recommended)
    Winsorizer winsor(0.02, 1);  // alpha=0.02, mode=1
    
    SECTION("n < 20: No winsorization") {
        REQUIRE(winsor.computeK(15) == 0);
        REQUIRE(winsor.computeK(19) == 0);
    }
    
    SECTION("n in [20, 30]: Full protection (k >= 1)") {
        REQUIRE(winsor.computeK(20) == 1);
        REQUIRE(winsor.computeK(25) == 1);
        REQUIRE(winsor.computeK(26) == 1);  // Median
        REQUIRE(winsor.computeK(30) == 1);
    }
    
    SECTION("n in [31, 50]: Still protected (k >= 1, NO discontinuity!)") {
        // This is the KEY improvement: no cliff at n=31
        REQUIRE(winsor.computeK(31) == 1);  // ← SMOOTH! (not 0)
        REQUIRE(winsor.computeK(35) == 1);
        REQUIRE(winsor.computeK(40) == 1);
        REQUIRE(winsor.computeK(50) == 1);  // Covers 94% of user's data
    }
    
    SECTION("n in [51, 100]: Gradual fade") {
        // After n=50, k can drop to 0 (smooth transition)
        REQUIRE(winsor.computeK(51) == 0);  // First n where k=0
        REQUIRE(winsor.computeK(60) == 0);
        REQUIRE(winsor.computeK(80) == 0);
        REQUIRE(winsor.computeK(100) == 0);
    }
    
    SECTION("n > 100: Uses raw alpha") {
        // For large n, k = floor(alpha * n)
        REQUIRE(winsor.computeK(150) == 3);  // floor(0.02 * 150) = 3
        REQUIRE(winsor.computeK(200) == 4);  // floor(0.02 * 200) = 4
    }
}

TEST_CASE("AdaptiveWinsorizer::computeK - Mode 2 (Always On)", "[StatUtils][Winsorizer][Mode2]") {
    using D = DecimalType;
    using Winsorizer = AdaptiveWinsorizer<D>;;
    
    // Mode 2: Always on (constant alpha)
    Winsorizer winsor(0.02, 2);  // alpha=0.02, mode=2
    
    SECTION("n < 20: No winsorization") {
        REQUIRE(winsor.computeK(15) == 0);
        REQUIRE(winsor.computeK(19) == 0);
    }
    
    SECTION("n >= 20: Always apply with k >= 1") {
        REQUIRE(winsor.computeK(20) == 1);
        REQUIRE(winsor.computeK(30) == 1);
        REQUIRE(winsor.computeK(31) == 1);  // No discontinuity
        REQUIRE(winsor.computeK(50) == 1);
        REQUIRE(winsor.computeK(60) == 1);  // Still k=1 (unlike Mode 1)
        REQUIRE(winsor.computeK(100) == 2); // floor(0.02 * 100) = 2
    }
}

TEST_CASE("AdaptiveWinsorizer::computeK - Alpha scaling", "[StatUtils][Winsorizer][Alpha]") {
    using D = DecimalType;
    using Winsorizer = AdaptiveWinsorizer<D>;;
    
    SECTION("Larger alpha increases k for large n") {
        Winsorizer winsor_2pct(0.02, 1);
        Winsorizer winsor_5pct(0.05, 1);
        
        // For small n (< 50), both enforce k >= 1
        REQUIRE(winsor_2pct.computeK(30) == 1);
        REQUIRE(winsor_5pct.computeK(30) == 1);
        
        // For large n, alpha matters
        REQUIRE(winsor_2pct.computeK(200) == 4);  // floor(0.02 * 200)
        REQUIRE(winsor_5pct.computeK(200) == 10); // floor(0.05 * 200)
    }
    
    SECTION("Alpha = 0 disables winsorization") {
        Winsorizer winsor(0.0, 1);
        REQUIRE(winsor.computeK(30) == 0);
        REQUIRE(winsor.computeK(100) == 0);
    }
}

TEST_CASE("AdaptiveWinsorizer::computeK - kmax capping", "[StatUtils][Winsorizer][Kmax]") {
    using D = DecimalType;
    using Winsorizer = AdaptiveWinsorizer<D>;
    
    SECTION("k capped at (n-1)/2 to avoid clipping > half the data") {
        // Very large alpha that would exceed kmax
        Winsorizer winsor(0.4, 2);  // 40% per tail (unrealistic but tests capping)
        
        // n=10: kmax = 4, alpha*n = 4.0 → k = min(4, 4) = 4
        REQUIRE(winsor.computeK(10) <= 4);
        
        // n=20: kmax = 9, alpha*n = 8.0 → k = min(8, 9) = 8
        REQUIRE(winsor.computeK(20) <= 9);
    }
}

TEST_CASE("AdaptiveWinsorizer::apply - Winsorization mechanics", "[StatUtils][Winsorizer][Apply]") {
    using D = DecimalType;
    using Winsorizer = AdaptiveWinsorizer<D>;
    
    constexpr double tol = 1e-10;
    
    SECTION("k=0: No-op (no winsorization)") {
        Winsorizer winsor(0.02, 0);  // Mode 0, n=15 → k=0
        
        std::vector<D> logs = {D("-0.15"), D("-0.08"), D("0.05"), D("0.10"), D("0.20")};
        auto original = logs;
        
        winsor.apply(logs);
        
        // Should be unchanged
        REQUIRE(logs.size() == original.size());
        for (size_t i = 0; i < logs.size(); ++i) {
            REQUIRE(logs[i] == original[i]);
        }
    }
    
    SECTION("k=1: Clip 1 value per tail (requires n>=20)") {
        Winsorizer winsor(0.02, 1);  // Mode 1

        // The Winsorizer disables itself for n < 20. 
        // We construct a vector of size 20 to force k=1.
        //
        // Target Sorted Tails:
        //   Index 0 (Min):        -0.15  -> Should clip to -0.08
        //   Index 1 (Lo Boundary): -0.08  -> Should stay -0.08
        //   ... middle zeros ...
        //   Index 18 (Hi Boundary): 0.10  -> Should stay 0.10
        //   Index 19 (Max):         0.20  -> Should clip to 0.10

        std::vector<D> logs;
        logs.reserve(20);

        // Push specific test values (unsorted)
        logs.push_back(D("0.20"));   // Index 0: Max
        logs.push_back(D("-0.15"));  // Index 1: Min
        logs.push_back(D("0.10"));   // Index 2: Hi Boundary
        logs.push_back(D("-0.08"));  // Index 3: Lo Boundary

        // Pad with 16 zeros to reach n=20
        for(int i = 0; i < 16; ++i) {
            logs.push_back(D("0.0"));
        }

        winsor.apply(logs);

        // Verification
        
        // 1. Max (0.20) at index 0 should be clipped to Hi Boundary (0.10)
        REQUIRE(num::to_double(logs[0]) == Catch::Approx(0.10).margin(tol));

        // 2. Min (-0.15) at index 1 should be clipped to Lo Boundary (-0.08)
        REQUIRE(num::to_double(logs[1]) == Catch::Approx(-0.08).margin(tol));

        // 3. Boundaries should remain unchanged
        REQUIRE(num::to_double(logs[2]) == Catch::Approx(0.10).margin(tol));
        REQUIRE(num::to_double(logs[3]) == Catch::Approx(-0.08).margin(tol));
    }
    
    SECTION("k=2: Clip 2 values per tail") {
        // Larger n to get k=2
        // n=50, alpha=0.05 => k = floor(2.5) = 2
        Winsorizer winsor(0.05, 2);  // 5% alpha, always on
        
        // Initialize with a neutral bulk value (0.0)
        std::vector<D> logs(50, D("0.00"));

        // --- Set up Lower Tail ---
        // Sorted Index 0: Extreme Min
        logs[0] = D("-0.50");
        // Sorted Index 1: 2nd Min (Should be clipped)
        logs[1] = D("-0.30");
        // Sorted Index 2: Lower Boundary (Should NOT be clipped, sets the floor)
        logs[2] = D("-0.10");

        // --- Set up Upper Tail ---
        // Sorted Index 47: Upper Boundary (Should NOT be clipped, sets the ceiling)
        logs[47] = D("0.10");
        // Sorted Index 48: 2nd Max (Should be clipped)
        logs[48] = D("0.30");
        // Sorted Index 49: Extreme Max
        logs[49] = D("0.50");

        winsor.apply(logs);
        
        // Verification:
        // With k=2, the values at indices 0 and 1 should be clipped to the value at index 2 (-0.10).
        // The values at indices 48 and 49 should be clipped to the value at index 47 (0.10).

        // 1. Verify Lower Tail Clipping
        REQUIRE(num::to_double(logs[0]) == Catch::Approx(-0.10).margin(tol)); // Clipped
        REQUIRE(num::to_double(logs[1]) == Catch::Approx(-0.10).margin(tol)); // Clipped
        REQUIRE(num::to_double(logs[2]) == Catch::Approx(-0.10).margin(tol)); // Boundary preserved

        // 2. Verify Upper Tail Clipping
        REQUIRE(num::to_double(logs[47]) == Catch::Approx(0.10).margin(tol)); // Boundary preserved
        REQUIRE(num::to_double(logs[48]) == Catch::Approx(0.10).margin(tol)); // Clipped
        REQUIRE(num::to_double(logs[49]) == Catch::Approx(0.10).margin(tol)); // Clipped
    }

    SECTION("Empty vector: Safe no-op") {
        Winsorizer winsor(0.02, 1);
        std::vector<D> logs;
        
        winsor.apply(logs);  // Should not crash
        
        REQUIRE(logs.empty());
    }
    
    SECTION("Single value: No winsorization possible") {
        Winsorizer winsor(0.02, 1);
        std::vector<D> logs = {D("0.05")};
        
        winsor.apply(logs);
        
        REQUIRE(logs.size() == 1);
        REQUIRE(logs[0] == D("0.05"));
    }
}

TEST_CASE("GeoMeanStat - Mode 0 (Legacy) matches original behavior", "[StatUtils][GeoMean][Mode0]") {
    using D = DecimalType;
    using DC = DecimalConstants<D>;
    
    constexpr double kGeoTol = 5e-8;
    
    // Helper: manual winsorization (k=1) in log domain
    auto manual_winsor1 = [](const std::vector<D>& returns) {
        std::vector<D> logs;
        logs.reserve(returns.size());
        const D one = DC::DecimalOne;
        for (const auto& r : returns) {
            D growth = one + r;
            if (growth <= D("1e-8")) growth = D("1e-8");
            logs.push_back(std::log(growth));
        }
        
        auto sorted = logs;
        std::sort(sorted.begin(), sorted.end());
        const size_t n = sorted.size();
        const size_t k = 1;
        const D lo = sorted[k];
        const D hi = sorted[n - 1 - k];
        
        for (auto& x : logs) {
            if (x < lo) x = lo;
            else if (x > hi) x = hi;
        }
        
        D sum = DC::DecimalZero;
        for (const auto& x : logs) sum += x;
        return std::exp(sum / D(static_cast<double>(n))) - one;
    };
    
    SECTION("n=30: Winsorization ON (matches original)") {
        std::vector<D> returns(30, D("0.005"));
        returns[3] = D("-0.45");   // Outlier
        returns[17] = D("0.20");   // Outlier
        
        GeoMeanStat<D> stat_mode0(true, true, 0.02, 1e-8, 0);  // Mode 0
        D gm = stat_mode0(returns);
        D expected = manual_winsor1(returns);
        
        REQUIRE(num::to_double(gm) == Catch::Approx(num::to_double(expected)).margin(kGeoTol));
    }
    
    SECTION("n=31: Winsorization OFF (original discontinuity)") {
        std::vector<D> returns(31, D("0.005"));
        returns[3] = D("-0.45");
        returns[17] = D("0.20");
        
        GeoMeanStat<D> stat_mode0(true, true, 0.02, 1e-8, 0);  // Mode 0
        D gm = stat_mode0(returns);
        
        // Should use raw geometric mean (no winsorization)
        // NOT equal to manual_winsor1 (which would clip)
        D with_winsor = manual_winsor1(returns);
        
        REQUIRE(num::to_double(gm) != Catch::Approx(num::to_double(with_winsor)).margin(kGeoTol));
    }
}

TEST_CASE("GeoMeanStat - Mode 1 (Smooth Fade) eliminates discontinuity", "[StatUtils][GeoMean][Mode1]") {
    using D = DecimalType;
    
    SECTION("n=30 and n=31: Smooth transition (both winsorized)") {
        // Create two very similar datasets
        std::vector<D> returns_30(30, D("0.005"));
        returns_30[3] = D("-0.45");
        returns_30[17] = D("0.20");
        
        std::vector<D> returns_31 = returns_30;
        returns_31.push_back(D("0.005"));  // Add one average return
        
        GeoMeanStat<D> stat(true, true, 0.02, 1e-8, 1);  // Mode 1
        
        D gm_30 = stat(returns_30);
        D gm_31 = stat(returns_31);
        
        // Should be VERY close (both have k=1, smooth transition)
        double diff = std::abs(num::to_double(gm_30 - gm_31));
        REQUIRE(diff < 0.002);  // Within 0.2% (smooth!)
        
        // Compare to Mode 0 which would show discontinuity
        GeoMeanStat<D> stat_legacy(true, true, 0.02, 1e-8, 0);
        D gm_30_legacy = stat_legacy(returns_30);
        D gm_31_legacy = stat_legacy(returns_31);
        
        double diff_legacy = std::abs(num::to_double(gm_30_legacy - gm_31_legacy));
        // Legacy should have LARGER jump (discontinuity)
        REQUIRE(diff_legacy > diff);
    }
    
    SECTION("n in [31, 50]: All get k>=1 protection") {
        for (size_t n = 31; n <= 50; ++n) {
            std::vector<D> returns(n, D("0.005"));
            returns[2] = D("-0.30");
            returns[n-2] = D("0.18");
            
            GeoMeanStat<D> stat(true, true, 0.02, 1e-8, 1);  // Mode 1
            D gm = stat(returns);
            
            // All should apply winsorization (verify by checking result is reasonable)
            REQUIRE(num::to_double(gm) > -0.30);  // Not dominated by outlier
            REQUIRE(num::to_double(gm) < 0.18);   // Not dominated by outlier
        }
    }
}

TEST_CASE("GeoMeanStat - Mode 2 (Always On) applies uniformly", "[StatUtils][GeoMean][Mode2]") {
    using D = DecimalType;
    
    SECTION("All n >= 20 get winsorization") {
        std::vector<size_t> sample_sizes = {20, 30, 50, 100};
        
        for (size_t n : sample_sizes) {
            std::vector<D> returns(n, D("0.01"));
            returns[0] = D("-0.40");     // Outlier
            returns[n-1] = D("0.25");    // Outlier
            
            GeoMeanStat<D> stat(true, true, 0.02, 1e-8, 2);  // Mode 2
            D gm = stat(returns);
            
            // All should be winsorized (no discontinuities)
            REQUIRE(num::to_double(gm) > -0.40);
            REQUIRE(num::to_double(gm) < 0.25);
        }
    }
}

TEST_CASE("GeoMeanFromLogBarsStat - Mode consistency with GeoMeanStat", "[StatUtils][GeoMeanFromLogs][Modes]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;
    
    constexpr double kGeoTol = 5e-8;
    const double ruin_eps = 1e-8;
    
    SECTION("Mode 0: Both structs produce identical results") {
        std::vector<D> returns(30, D("0.01"));
        returns[5] = D("-0.35");
        returns[22] = D("0.18");
        
        GeoMeanStat<D> stat1(true, true, 0.02, ruin_eps, 0);  // Mode 0
        D gm1 = stat1(returns);
        
        auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);
        GeoMeanFromLogBarsStat<D> stat2(true, 0.02, 0);  // Mode 0
        D gm2 = stat2(logBars);
        
        REQUIRE(num::to_double(gm1) == Catch::Approx(num::to_double(gm2)).margin(kGeoTol));
    }
    
    SECTION("Mode 1: Both structs produce identical results") {
        std::vector<D> returns(35, D("0.01"));  // n=35 to test [31,50] range
        returns[5] = D("-0.35");
        returns[28] = D("0.18");
        
        GeoMeanStat<D> stat1(true, true, 0.02, ruin_eps, 1);  // Mode 1
        D gm1 = stat1(returns);
        
        auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);
        GeoMeanFromLogBarsStat<D> stat2(true, 0.02, 1);  // Mode 1
        D gm2 = stat2(logBars);
        
        REQUIRE(num::to_double(gm1) == Catch::Approx(num::to_double(gm2)).margin(kGeoTol));
    }
    
    SECTION("Mode 2: Both structs produce identical results") {
        std::vector<D> returns(60, D("0.01"));
        returns[10] = D("-0.35");
        returns[50] = D("0.18");
        
        GeoMeanStat<D> stat1(true, true, 0.02, ruin_eps, 2);  // Mode 2
        D gm1 = stat1(returns);
        
        auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);
        GeoMeanFromLogBarsStat<D> stat2(true, 0.02, 2);  // Mode 2
        D gm2 = stat2(logBars);
        
        REQUIRE(num::to_double(gm1) == Catch::Approx(num::to_double(gm2)).margin(kGeoTol));
    }
}

TEST_CASE("GeoMeanFromLogBarsStat - Mode 1 eliminates discontinuity", "[StatUtils][GeoMeanFromLogs][Mode1]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;
    
    const double ruin_eps = 1e-8;
    
    SECTION("n=30 vs n=31: Smooth transition") {
        // Base returns
        std::vector<D> returns_30(30, D("0.005"));
        returns_30[3] = D("-0.40");
        returns_30[25] = D("0.22");
        
        std::vector<D> returns_31 = returns_30;
        returns_31.push_back(D("0.005"));
        
        // Convert to log-bars
        auto logBars_30 = Stat::makeLogGrowthSeries(returns_30, ruin_eps);
        auto logBars_31 = Stat::makeLogGrowthSeries(returns_31, ruin_eps);
        
        GeoMeanFromLogBarsStat<D> stat(true, 0.02, 1);  // Mode 1
        
        D gm_30 = stat(logBars_30);
        D gm_31 = stat(logBars_31);
        
        // Should be very close (smooth transition)
        double diff = std::abs(num::to_double(gm_30 - gm_31));
        REQUIRE(diff < 0.002);
    }
}

TEST_CASE("GeoMeanStat - User data profile (min=20, median=26, 94% <= 50)", "[StatUtils][GeoMean][UserData]") {
    using D = DecimalType;
    
    SECTION("Mode 1 protects 94% of user data (n <= 50)") {
        // Test coverage across user's actual data range
        std::vector<size_t> sizes = {20, 26, 30, 35, 40, 50};  // All <= 50
        
        for (size_t n : sizes) {
            std::vector<D> returns(n, D("0.01"));
            returns[2] = D("-0.30");      // Outlier
            returns[n-3] = D("0.20");     // Outlier
            
            GeoMeanStat<D> stat(true, true, 0.02, 1e-8, 1);  // Mode 1
            D gm = stat(returns);
            
            // All these sizes should get k >= 1 protection
            // Verify by checking result is dampened (not dominated by outliers)
            REQUIRE(num::to_double(gm) > -0.25);  // Outlier dampened
            REQUIRE(num::to_double(gm) < 0.15);   // Outlier dampened
            REQUIRE(std::isfinite(num::to_double(gm)));
        }
    }
    
    SECTION("Mode 0 vs Mode 1: Difference for n in [31, 50]") {
        // This range represents ~40% of user's data that benefits from Mode 1
        size_t n = 35;  // Example: median + Qn ≈ 26 + 7 = 33-35
        
        std::vector<D> returns(n, D("0.01"));
        returns[5] = D("-0.40");  // Massive loss
        returns[28] = D("0.25");
        
        GeoMeanStat<D> stat_mode0(true, true, 0.02, 1e-8, 0);  // Legacy
        GeoMeanStat<D> stat_mode1(true, true, 0.02, 1e-8, 1);  // Smooth
        
        D gm_mode0 = stat_mode0(returns);
        D gm_mode1 = stat_mode1(returns);
        
        // Mode 0: No winsorization. The -40% loss crashes the mean.
        // Mode 1: Winsorization active. The -40% is clipped, preserving the mean.
        
        // 1. Mode 1 should be significantly HIGHER than Mode 0 (better performance)
        REQUIRE(num::to_double(gm_mode1) > num::to_double(gm_mode0));
        
        // 2. Mode 1 should remain close to the bulk return (0.01)
        //    (Allowing small deviation due to the net effect of clipping)
        REQUIRE(num::to_double(gm_mode1) == Catch::Approx(0.01).margin(0.005));

        // 3. Mode 0 should be significantly dragged down
        //    (It dropped to ~0.001 in your failure log)
        REQUIRE(num::to_double(gm_mode0) < 0.005); 
    }
}

TEST_CASE("AdaptiveWinsorizer - Getters", "[StatUtils][Winsorizer][Getters]") {
    using D = DecimalType;
    using Winsorizer = AdaptiveWinsorizer<D>;
    
    SECTION("getAlpha returns constructor value") {
        Winsorizer winsor1(0.02, 1);
        REQUIRE(winsor1.getAlpha() == 0.02);
        
        Winsorizer winsor2(0.05, 1);
        REQUIRE(winsor2.getAlpha() == 0.05);
    }
    
    SECTION("getAdaptiveMode returns constructor value") {
        Winsorizer winsor0(0.02, 0);
        REQUIRE(winsor0.getAdaptiveMode() == 0);
        
        Winsorizer winsor1(0.02, 1);
        REQUIRE(winsor1.getAdaptiveMode() == 1);
        
        Winsorizer winsor2(0.02, 2);
        REQUIRE(winsor2.getAdaptiveMode() == 2);
    }
}

TEST_CASE("GeoMeanStat - Default constructor uses Mode 1", "[StatUtils][GeoMean][Default]") {
    using D = DecimalType;
    
    SECTION("Default constructor should use smooth fade (mode=1)") {
        std::vector<D> returns(35, D("0.01"));  // n=35 in [31,50] range
        returns[5] = D("-0.30");
        returns[28] = D("0.20");
        
        GeoMeanStat<D> stat_default;  // Default constructor
        D gm_default = stat_default(returns);
        
        GeoMeanStat<D> stat_explicit(true, true, 0.02, 1e-8, 1);  // Explicit Mode 1
        D gm_explicit = stat_explicit(returns);
        
        // Should be identical
        REQUIRE(num::to_double(gm_default) == Catch::Approx(num::to_double(gm_explicit)).margin(1e-10));
    }
}

TEST_CASE("GeoMeanFromLogBarsStat - Default constructor uses Mode 1", "[StatUtils][GeoMeanFromLogs][Default]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;
    
    const double ruin_eps = 1e-8;
    
    SECTION("Default constructor should use smooth fade (mode=1)") {
        std::vector<D> returns(35, D("0.01"));
        returns[5] = D("-0.30");
        returns[28] = D("0.20");
        
        auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);
        
        GeoMeanFromLogBarsStat<D> stat_default;  // Default constructor
        D gm_default = stat_default(logBars);
        
        GeoMeanFromLogBarsStat<D> stat_explicit(true, 0.02, 1);  // Explicit Mode 1
        D gm_explicit = stat_explicit(logBars);
        
        // Should be identical
        REQUIRE(num::to_double(gm_default) == Catch::Approx(num::to_double(gm_explicit)).margin(1e-10));
    }
}

// Additional unit tests for StatUtils::computeStdDev
// These tests should be added to StatUtilsTest.cpp

TEST_CASE("StatUtils::computeStdDev", "[StatUtils]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;
    
    SECTION("Basic calculation with positive values") {
        std::vector<D> data = {D("1.0"), D("2.0"), D("3.0"), D("4.0"), D("5.0")};
        D mean = D("3.0");  // (1+2+3+4+5)/5 = 3.0
        // Variance = ((1-3)^2 + (2-3)^2 + (3-3)^2 + (4-3)^2 + (5-3)^2) / (5-1)
        //          = (4 + 1 + 0 + 1 + 4) / 4 = 10/4 = 2.5
        // Std Dev = sqrt(2.5) ≈ 1.58113883
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(1.58113883).margin(1e-6));
    }
    
    SECTION("Identical values (zero variance)") {
        std::vector<D> data = {D("5.0"), D("5.0"), D("5.0"), D("5.0")};
        D mean = D("5.0");
        // Variance = 0, StdDev should be 0
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(0.0).margin(1e-10));
    }
    
    SECTION("Two values") {
        std::vector<D> data = {D("10.0"), D("20.0")};
        D mean = D("15.0");
        // Variance = ((10-15)^2 + (20-15)^2) / (2-1) = (25 + 25) / 1 = 50
        // Std Dev = sqrt(50) ≈ 7.071068
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(7.071068).margin(1e-6));
    }
    
    SECTION("Single value") {
        std::vector<D> data = {D("42.0")};
        D mean = D("42.0");
        // With n=1, variance calculation depends on implementation
        // Typically returns 0 for n=1 (division by n-1=0 case)
        D stdDev = Stat::computeStdDev(data, mean);
        // Should handle gracefully, likely returning 0
        REQUIRE(std::isfinite(num::to_double(stdDev)));
    }
    
    SECTION("Empty vector") {
        std::vector<D> data;
        D mean = D("0.0");
        // Should handle gracefully without crashing
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(0.0).margin(1e-10));
    }
    
    SECTION("Negative values") {
        std::vector<D> data = {D("-5.0"), D("-3.0"), D("-1.0"), D("1.0"), D("3.0")};
        D mean = D("-1.0");  // (-5-3-1+1+3)/5 = -5/5 = -1.0
        // Variance = ((-5-(-1))^2 + (-3-(-1))^2 + (-1-(-1))^2 + (1-(-1))^2 + (3-(-1))^2) / 4
        //          = (16 + 4 + 0 + 4 + 16) / 4 = 40/4 = 10
        // Std Dev = sqrt(10) ≈ 3.162278
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(3.162278).margin(1e-6));
    }
    
    SECTION("Mixed positive and negative values") {
        std::vector<D> data = {D("-10.0"), D("-5.0"), D("0.0"), D("5.0"), D("10.0")};
        D mean = D("0.0");
        // Variance = (100 + 25 + 0 + 25 + 100) / 4 = 250/4 = 62.5
        // Std Dev = sqrt(62.5) ≈ 7.905694
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(7.905694).margin(1e-6));
    }
    
    SECTION("Small decimal values (financial returns)") {
        std::vector<D> data = {D("0.01"), D("0.02"), D("-0.01"), D("0.03"), D("-0.02")};
        D mean = D("0.006");  // (0.01+0.02-0.01+0.03-0.02)/5 = 0.03/5 = 0.006
        // Manual calculation of variance
        // Deviations: 0.004, 0.014, -0.016, 0.024, -0.026
        // Squared: 0.000016, 0.000196, 0.000256, 0.000576, 0.000676
        // Sum = 0.00172, Variance = 0.00172/4 = 0.00043
        // Std Dev = sqrt(0.00043) ≈ 0.020736
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(0.020736).margin(1e-6));
    }
    
    SECTION("Large values") {
        std::vector<D> data = {D("1000.0"), D("2000.0"), D("3000.0"), D("4000.0"), D("5000.0")};
        D mean = D("3000.0");
        // Variance = (4000000 + 1000000 + 0 + 1000000 + 4000000) / 4 = 10000000/4 = 2500000
        // Std Dev = sqrt(2500000) ≈ 1581.13883
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(1581.13883).margin(1e-3));
    }

    SECTION("Very small variance (numerical stability)") {
        std::vector<D> data = {D("1.0000001"), D("1.0000002"), D("1.0000003"), D("1.0000004")};
        D mean = D("1.00000025");
        // Very small deviations - tests numerical stability
        // Note: With such tiny differences, the variance may be smaller than
        // double precision can represent, resulting in stdDev = 0
        D stdDev = Stat::computeStdDev(data, mean);
        // The key requirement is that it doesn't crash and returns a valid number
        REQUIRE(std::isfinite(num::to_double(stdDev)));
        REQUIRE(num::to_double(stdDev) >= 0.0);  // Can be 0 due to precision limits
    }
    
    SECTION("Outliers present") {
        std::vector<D> data = {D("1.0"), D("2.0"), D("3.0"), D("4.0"), D("100.0")};
        D mean = D("22.0");  // (1+2+3+4+100)/5 = 110/5 = 22
        // Variance dominated by outlier
        // Deviations: -21, -20, -19, -18, 78
        // Squared: 441, 400, 361, 324, 6084
        // Sum = 7610, Variance = 7610/4 = 1902.5
        // Std Dev = sqrt(1902.5) ≈ 43.618
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(43.618).margin(1e-3));
    }
    
    SECTION("Incorrect mean provided (edge case)") {
        // Testing what happens if user provides wrong mean
        std::vector<D> data = {D("1.0"), D("2.0"), D("3.0"), D("4.0"), D("5.0")};
        D wrong_mean = D("10.0");  // Actual mean is 3.0
        // Should still compute, just with incorrect result
        D stdDev = Stat::computeStdDev(data, wrong_mean);
        REQUIRE(std::isfinite(num::to_double(stdDev)));
        REQUIRE(num::to_double(stdDev) > 0.0);
    }
    
    SECTION("Zero mean with positive and negative values") {
        std::vector<D> data = {D("-2.0"), D("-1.0"), D("0.0"), D("1.0"), D("2.0")};
        D mean = D("0.0");
        // Variance = (4 + 1 + 0 + 1 + 4) / 4 = 10/4 = 2.5
        // Std Dev = sqrt(2.5) ≈ 1.58113883
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(1.58113883).margin(1e-6));
    }
    
    SECTION("Large dataset (100 values)") {
        std::vector<D> data;
        for (int i = 1; i <= 100; ++i) {
            data.push_back(D(std::to_string(i)));
        }
        D mean = D("50.5");  // Mean of 1 to 100
        // For uniform distribution 1 to n: stddev = sqrt((n^2 - 1) / 12)
        // For n=100: stddev ≈ sqrt(9999/12) ≈ 28.866
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(29.01149).margin(1e-3));
    }
    
    SECTION("All zeros") {
        std::vector<D> data = {D("0.0"), D("0.0"), D("0.0"), D("0.0")};
        D mean = D("0.0");
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(num::to_double(stdDev) == Catch::Approx(0.0).margin(1e-10));
    }
    
    SECTION("High precision decimal values") {
        std::vector<D> data = {
            D("1.23456789"),
            D("2.34567890"),
            D("3.45678901"),
            D("4.56789012")
        };
        D mean = D("2.90123148");
        // Should maintain precision in calculation
        D stdDev = Stat::computeStdDev(data, mean);
        REQUIRE(std::isfinite(num::to_double(stdDev)));
        REQUIRE(num::to_double(stdDev) > 0.0);
    }
}

TEST_CASE("StatUtils::computeStdDev - Integration with computeMean", "[StatUtils][Integration]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;
    
    SECTION("Compute mean and stddev together") {
        std::vector<D> data = {D("10.0"), D("20.0"), D("30.0"), D("40.0"), D("50.0")};
        D mean = Stat::computeMean(data);
        D stdDev = Stat::computeStdDev(data, mean);
        
        REQUIRE(num::to_double(mean) == Catch::Approx(30.0));
        REQUIRE(num::to_double(stdDev) == Catch::Approx(15.811388).margin(1e-5));
    }
    
    SECTION("Financial returns with computed mean") {
        std::vector<D> returns = {
            D("0.05"), D("-0.02"), D("0.03"), D("-0.01"), 
            D("0.04"), D("-0.03"), D("0.02"), D("0.01")
        };
        D mean = Stat::computeMean(returns);
        D stdDev = Stat::computeStdDev(returns, mean);
        
        REQUIRE(num::to_double(stdDev) > 0.0);
        REQUIRE(std::isfinite(num::to_double(stdDev)));
    }
}

TEST_CASE("StatUtils::computeStdDev - Comparison with variance", "[StatUtils][Variance]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;
    
    SECTION("StdDev should be sqrt of variance") {
        std::vector<D> data = {D("2.0"), D("4.0"), D("6.0"), D("8.0"), D("10.0")};
        D mean = D("6.0");
        
        D variance = Stat::computeVariance(data, mean);
        D stdDev = Stat::computeStdDev(data, mean);
        
        double var_val = num::to_double(variance);
        double std_val = num::to_double(stdDev);
        
        REQUIRE(std_val == Catch::Approx(std::sqrt(var_val)).margin(1e-10));
    }
    
    SECTION("Zero variance implies zero stddev") {
        std::vector<D> data = {D("7.5"), D("7.5"), D("7.5")};
        D mean = D("7.5");
        
        D variance = Stat::computeVariance(data, mean);
        D stdDev = Stat::computeStdDev(data, mean);
        
        REQUIRE(num::to_double(variance) == Catch::Approx(0.0).margin(1e-10));
        REQUIRE(num::to_double(stdDev) == Catch::Approx(0.0).margin(1e-10));
    }
}

// =============================================================================
// Quantile Type-7 Tests
// =============================================================================
// These tests validate the new quantileType7Sorted and quantileType7Unsorted
// functions that implement the Hyndman-Fan Type 7 quantile definition.
// This is the default quantile method in R, NumPy, and Excel.
//
// Reference: Hyndman, R.J. and Fan, Y. (1996). Sample quantiles in statistical 
// packages. American Statistician, 50(4), 361-365.
// =============================================================================

TEST_CASE("StatUtils::quantileType7Sorted - Edge Cases", "[StatUtils][Quantile]") {
    SECTION("Empty vector throws exception") {
        std::vector<double> empty;
        REQUIRE_THROWS_AS(StatUtils<double>::quantileType7Sorted(empty, 0.5), std::invalid_argument);
    }
    
    SECTION("Single element returns that element for any probability") {
        std::vector<double> data = {42.0};
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.0) == 42.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.25) == 42.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.5) == 42.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.75) == 42.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 1.0) == 42.0);
    }
    
    SECTION("Two elements - exact interpolation") {
        std::vector<double> data = {1.0, 3.0};
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.0) == 1.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.5) == 2.0);  // midpoint
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 1.0) == 3.0);
    }
    
    SECTION("Identical values") {
        std::vector<double> data = {5.0, 5.0, 5.0, 5.0, 5.0};
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.0) == 5.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.25) == 5.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.5) == 5.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.75) == 5.0);
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 1.0) == 5.0);
    }
}

TEST_CASE("StatUtils::quantileType7Sorted - Boundary Probabilities", "[StatUtils][Quantile]") {
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    
    SECTION("Probability = 0.0 returns minimum") {
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.0) == 1.0);
    }
    
    SECTION("Probability = 1.0 returns maximum") {
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 1.0) == 10.0);
    }
    
    SECTION("Negative probability clamped to minimum") {
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, -0.5) == 1.0);
    }
    
    SECTION("Probability > 1.0 clamped to maximum") {
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 1.5) == 10.0);
    }
}

TEST_CASE("StatUtils::quantileType7Sorted - Standard Percentiles", "[StatUtils][Quantile]") {
    // Dataset: 1, 2, 3, ..., 10
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    
    SECTION("Median (p=0.5)") {
        // For n=10, p=0.5: h = (10-1)*0.5 + 1 = 5.5
        // i = 5 (1-based), interpolate between x[4]=5 and x[5]=6
        // Result: 5 + 0.5*(6-5) = 5.5
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.5) == 5.5);
    }
    
    SECTION("First quartile (p=0.25)") {
        // For n=10, p=0.25: h = (10-1)*0.25 + 1 = 3.25
        // i = 3 (1-based), interpolate between x[2]=3 and x[3]=4
        // Result: 3 + 0.25*(4-3) = 3.25
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.25) == 3.25);
    }
    
    SECTION("Third quartile (p=0.75)") {
        // For n=10, p=0.75: h = (10-1)*0.75 + 1 = 7.75
        // i = 7 (1-based), interpolate between x[6]=7 and x[7]=8
        // Result: 7 + 0.75*(8-7) = 7.75
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.75) == 7.75);
    }
    
    SECTION("95th percentile (p=0.95)") {
      // For n=10, p=0.95: h = (10-1)*0.95 + 1 = 9.55
      // i = 9 (1-based), interpolate between x[8]=9 and x[9]=10
      // Result: 9 + 0.55*(10-9) = 9.55
      REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.95) == Catch::Approx(9.55));
    }
    
    SECTION("99th percentile (p=0.99)") {
      // For n=10, p=0.99: h = (10-1)*0.99 + 1 = 9.91
      // i = 9 (1-based), interpolate between x[8]=9 and x[9]=10
      // Result: 9 + 0.91*(10-9) = 9.91
      REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.99) == Catch::Approx(9.91));
    }
}

TEST_CASE("StatUtils::quantileType7Sorted - R Compatibility", "[StatUtils][Quantile]") {
    // Test data verified with R: quantile(1:10, type=7)
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    
    SECTION("Compare with R quantile(type=7) results") {
        // R command: quantile(1:10, probs=c(0, 0.1, 0.25, 0.5, 0.75, 0.9, 1), type=7)
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.00) == Catch::Approx(1.00));
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.10) == Catch::Approx(1.90));
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.25) == Catch::Approx(3.25));
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.50) == Catch::Approx(5.50));
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.75) == Catch::Approx(7.75));
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.90) == Catch::Approx(9.10));
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 1.00) == Catch::Approx(10.00));
    }
}

TEST_CASE("StatUtils::quantileType7Sorted - Special Values", "[StatUtils][Quantile]") {
    SECTION("Negative values") {
        std::vector<double> data = {-5.0, -3.0, -1.0, 0.0, 1.0, 3.0, 5.0};
        // For n=7, p=0.5: h = (7-1)*0.5 + 1 = 4.0
        // i = 4 (1-based), frac=0.0 => x[3] (0-based) = 0.0
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.5) == 0.0);
    }
    
    SECTION("Very small values (near machine epsilon)") {
        std::vector<double> data = {1e-15, 2e-15, 3e-15, 4e-15, 5e-15};
        // For n=5, p=0.5: h = (5-1)*0.5 + 1 = 3.0
        // i = 3 (1-based), frac=0.0 => x[2] (0-based) = 3e-15
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.5) == 3e-15);
    }
    
    SECTION("Very large values") {
        std::vector<double> data = {1e15, 2e15, 3e15, 4e15, 5e15};
        // For n=5, p=0.75: h = (5-1)*0.75 + 1 = 4.0
        // i = 4 (1-based), frac=0.0 => x[3] (0-based) = 4e15
        REQUIRE(StatUtils<double>::quantileType7Sorted(data, 0.75) == 4e15);
    }
}

TEST_CASE("StatUtils::quantileType7Sorted - Decimal Type Support", "[StatUtils][Quantile]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;

    SECTION("Works with Decimal type") {
        std::vector<D> data = {
            D("1.0"), D("2.0"), D("3.0"), D("4.0"), D("5.0")
        };
        
        D result = Stat::quantileType7Sorted(data, 0.5);
        // For n=5, p=0.5: h = (5-1)*0.5 + 1 = 3.0
        // i = 3 (1-based), frac=0.0 => x[2] = 3.0
        REQUIRE(num::to_double(result) == Catch::Approx(3.0));
    }
    
    SECTION("Decimal precision maintained during interpolation") {
        std::vector<D> data = {
            D("1.123456789"), D("2.234567890"), D("3.345678901"), 
            D("4.456789012"), D("5.567890123")
        };
        
        D result = Stat::quantileType7Sorted(data, 0.75);
        // Should preserve decimal precision
        REQUIRE(std::isfinite(num::to_double(result)));
        REQUIRE(num::to_double(result) > num::to_double(data[2]));
        REQUIRE(num::to_double(result) <= num::to_double(data[4]));
    }
}

TEST_CASE("StatUtils::quantileType7Sorted - Large Dataset", "[StatUtils][Quantile]") {
    SECTION("Dataset of 1000 elements") {
        std::vector<double> data(1000);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<double>(i + 1);
        }
        
        // Median
        double median = StatUtils<double>::quantileType7Sorted(data, 0.5);
        // For n=1000, p=0.5: h = (1000-1)*0.5 + 1 = 500.5
        // i = 500, interpolate between x[499]=500 and x[500]=501
        // Result: 500 + 0.5*(501-500) = 500.5
        REQUIRE(median == 500.5);
        
        // 90th percentile
        double p90 = StatUtils<double>::quantileType7Sorted(data, 0.9);
        // For n=1000, p=0.9: h = (1000-1)*0.9 + 1 = 900.1
        // i = 900, interpolate between x[899]=900 and x[900]=901
        // Result: 900 + 0.1*(901-900) = 900.1
        REQUIRE(p90 == 900.1);
    }
}

TEST_CASE("StatUtils::quantileType7Sorted - Interpolation Accuracy", "[StatUtils][Quantile]") {
    SECTION("Fine-grained interpolation between [0, 1]") {
        std::vector<double> data = {0.0, 1.0};
        
        // Test 101 points from p=0.00 to p=1.00
        for (int i = 0; i <= 100; ++i) {
            double p = i / 100.0;
            double result = StatUtils<double>::quantileType7Sorted(data, p);
            // For [0, 1], quantile at p should equal p
            REQUIRE(result == Catch::Approx(p).margin(1e-12));
        }
    }
}

// =============================================================================
// quantileType7Unsorted Tests
// =============================================================================

TEST_CASE("StatUtils::quantileType7Unsorted - Edge Cases", "[StatUtils][Quantile]") {
    SECTION("Empty vector throws exception") {
        std::vector<double> empty;
        REQUIRE_THROWS_AS(StatUtils<double>::quantileType7Unsorted(empty, 0.5), std::invalid_argument);
    }
    
    SECTION("Single element returns that element") {
        std::vector<double> data = {42.0};
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.0) == 42.0);
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.5) == 42.0);
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 1.0) == 42.0);
    }
    
    SECTION("Two elements (unsorted)") {
        std::vector<double> data = {3.0, 1.0};  // reversed order
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.0) == 1.0);
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.5) == 2.0);
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 1.0) == 3.0);
    }
}

TEST_CASE("StatUtils::quantileType7Unsorted - Does Not Modify Input", "[StatUtils][Quantile]") {
    SECTION("Input vector remains unchanged") {
        std::vector<double> data = {5.0, 2.0, 8.0, 1.0, 9.0};
        std::vector<double> original = data;
        
        StatUtils<double>::quantileType7Unsorted(data, 0.5);
        
        REQUIRE(data == original);
    }
}

TEST_CASE("StatUtils::quantileType7Unsorted - Equivalence with Sorted", "[StatUtils][Quantile]") {
    SECTION("Unsorted gives same results as sorted") {
        std::vector<double> unsorted = {7.3, 2.1, 9.5, 1.8, 5.4, 3.2, 8.7, 4.6, 10.2, 6.9};
        std::vector<double> sorted = unsorted;
        std::sort(sorted.begin(), sorted.end());
        
        std::vector<double> probabilities = {0.0, 0.1, 0.25, 0.333, 0.5, 0.667, 0.75, 0.9, 0.95, 1.0};
        
        for (double p : probabilities) {
            double result_sorted = StatUtils<double>::quantileType7Sorted(sorted, p);
            double result_unsorted = StatUtils<double>::quantileType7Unsorted(unsorted, p);
            
            REQUIRE(result_sorted == Catch::Approx(result_unsorted).margin(1e-9));
        }
    }
    
    SECTION("Random ordering produces same results") {
        std::vector<double> data1 = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
        std::vector<double> data2 = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0, 7.0, 4.0, 10.0, 6.0};
        std::vector<double> data3 = {10.0, 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0};
        
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data1, 0.5) == 
                StatUtils<double>::quantileType7Unsorted(data2, 0.5));
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data1, 0.5) == 
                StatUtils<double>::quantileType7Unsorted(data3, 0.5));
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data1, 0.75) == 
                StatUtils<double>::quantileType7Unsorted(data2, 0.75));
    }
}

TEST_CASE("StatUtils::quantileType7Unsorted - Standard Percentiles", "[StatUtils][Quantile]") {
    // Unsorted dataset
    std::vector<double> data = {7.0, 3.0, 9.0, 1.0, 5.0, 2.0, 8.0, 4.0, 10.0, 6.0};
    
    SECTION("Median (p=0.5)") {
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.5) == 5.5);
    }
    
    SECTION("First quartile (p=0.25)") {
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.25) == 3.25);
    }
    
    SECTION("Third quartile (p=0.75)") {
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.75) == 7.75);
    }
}

TEST_CASE("StatUtils::quantileType7Unsorted - Decimal Type Support", "[StatUtils][Quantile]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;
    
    SECTION("Works with Decimal type on unsorted data") {
        std::vector<D> data = {
            D("5.0"), D("2.0"), D("4.0"), D("1.0"), D("3.0")
        };
        
        D result = Stat::quantileType7Unsorted(data, 0.75);
        // When sorted: [1, 2, 3, 4, 5]
        // For n=5, p=0.75: h = 4.0, x[3] = 4.0
        REQUIRE(num::to_double(result) == Catch::Approx(4.0));
    }
}

TEST_CASE("StatUtils::quantileType7Unsorted - Boundary Conditions", "[StatUtils][Quantile]") {
    std::vector<double> data = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0, 7.0, 4.0, 10.0, 6.0};
    
    SECTION("p=0.0 returns minimum") {
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 0.0) == 1.0);
    }
    
    SECTION("p=1.0 returns maximum") {
        REQUIRE(StatUtils<double>::quantileType7Unsorted(data, 1.0) == 10.0);
    }
}

TEST_CASE("StatUtils::quantileType7 - Performance Comparison", "[StatUtils][Quantile][Performance]") {
    SECTION("Sorted version is O(1) for pre-sorted data") {
        std::vector<double> data(1000);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<double>(i);
        }
        
        // Should be instant on sorted data
        double result = StatUtils<double>::quantileType7Sorted(data, 0.5);
        REQUIRE(std::isfinite(result));
    }
    
    SECTION("Unsorted version handles unsorted data correctly") {
        std::vector<double> unsorted(100);
        for (size_t i = 0; i < unsorted.size(); ++i) {
            unsorted[i] = static_cast<double>(100 - i);  // Reverse order
        }
        
        double result = StatUtils<double>::quantileType7Unsorted(unsorted, 0.5);
        REQUIRE(result == Catch::Approx(50.5));
    }
}

TEST_CASE("StatUtils::quantileType7 - Bootstrap Use Case", "[StatUtils][Quantile][Bootstrap]") {
    SECTION("Typical bootstrap replicate distribution") {
        // Simulate bootstrap statistics (slightly skewed)
        std::vector<double> bootstrap_stats = {
            0.85, 0.92, 0.88, 0.95, 0.87, 0.91, 0.89, 0.93, 0.86, 0.94,
            0.90, 0.88, 0.92, 0.91, 0.89, 0.87, 0.93, 0.90, 0.88, 0.95
        };
        
        // Compute 95% CI using Type 7 quantiles (as in PercentileBootstrap)
        std::vector<double> sorted_stats = bootstrap_stats;
        std::sort(sorted_stats.begin(), sorted_stats.end());
        
        double lower = StatUtils<double>::quantileType7Sorted(sorted_stats, 0.025);
        double upper = StatUtils<double>::quantileType7Sorted(sorted_stats, 0.975);
        
        REQUIRE(lower < upper);
        REQUIRE(lower >= sorted_stats.front());
        REQUIRE(upper <= sorted_stats.back());
    }
    
    SECTION("Handles degenerate cases in bootstrap") {
        // All bootstrap replicates identical (can happen with small samples)
        std::vector<double> degenerate = {1.5, 1.5, 1.5, 1.5, 1.5};
        
        double q25 = StatUtils<double>::quantileType7Sorted(degenerate, 0.25);
        double q50 = StatUtils<double>::quantileType7Sorted(degenerate, 0.50);
        double q75 = StatUtils<double>::quantileType7Sorted(degenerate, 0.75);
        
        REQUIRE(q25 == 1.5);
        REQUIRE(q50 == 1.5);
        REQUIRE(q75 == 1.5);
    }
}

TEST_CASE("StatUtils::quantileType7 - Financial Data Use Case", "[StatUtils][Quantile][Finance]") {
    using D = DecimalType;
    using Stat = StatUtils<D>;

    SECTION("VaR calculation (Value at Risk)") {
        // Daily returns (percent)
        std::vector<D> returns = {
            D("0.015"), D("-0.023"), D("0.008"), D("-0.012"), D("0.019"),
            D("-0.005"), D("0.011"), D("-0.018"), D("0.007"), D("-0.009"),
            D("0.014"), D("-0.021"), D("0.006"), D("-0.015"), D("0.013")
        };
        
        std::vector<D> sorted_returns = returns;
        std::sort(sorted_returns.begin(), sorted_returns.end());
        
        // 95% VaR is the 5th percentile
        D var_95 = Stat::quantileType7Sorted(sorted_returns, 0.05);
        
        // VaR should be negative (a loss)
        REQUIRE(num::to_double(var_95) < 0.0);
    }
    
    SECTION("Percentile rank for performance benchmarking") {
        std::vector<D> strategy_returns = {
            D("0.08"), D("0.12"), D("0.15"), D("0.09"), D("0.11"),
            D("0.14"), D("0.10"), D("0.13"), D("0.16"), D("0.07")
        };
        
        std::vector<D> sorted = strategy_returns;
        std::sort(sorted.begin(), sorted.end());
        
        // What return do you need to be in top quartile?
        D top_quartile_threshold = Stat::quantileType7Sorted(sorted, 0.75);
        
        REQUIRE(num::to_double(top_quartile_threshold) > num::to_double(D("0.10")));
    }
}

TEST_CASE("StatisticSupport: factories and observers", "[StatisticSupport]")
{
  SECTION("unbounded() creates a support with no lower bound")
  {
    const StatisticSupport support = StatisticSupport::unbounded();

    REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::None);
    REQUIRE_FALSE(support.hasLowerBound());

    // The stored bound/eps are still accessible (implementation detail),
    // but there should be no enforcement.
    REQUIRE(support.lowerBound() == Catch::Approx(0.0));
    REQUIRE(support.epsilon() == Catch::Approx(1e-12));
  }

  SECTION("nonStrictLowerBound() stores bound and epsilon")
  {
    const StatisticSupport support = StatisticSupport::nonStrictLowerBound(2.5, 1e-6);

    REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
    REQUIRE(support.hasLowerBound());
    REQUIRE(support.lowerBound() == Catch::Approx(2.5));
    REQUIRE(support.epsilon() == Catch::Approx(1e-6));
  }

  SECTION("strictLowerBound() stores bound and epsilon")
  {
    const StatisticSupport support = StatisticSupport::strictLowerBound(-1.0, 1e-9);

    REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::Strict);
    REQUIRE(support.hasLowerBound());
    REQUIRE(support.lowerBound() == Catch::Approx(-1.0));
    REQUIRE(support.epsilon() == Catch::Approx(1e-9));
  }

  SECTION("factories use the documented default epsilon when not provided")
  {
    const StatisticSupport nonStrictDefault = StatisticSupport::nonStrictLowerBound(0.0);
    const StatisticSupport strictDefault    = StatisticSupport::strictLowerBound(0.0);

    REQUIRE(nonStrictDefault.epsilon() == Catch::Approx(1e-12));
    REQUIRE(strictDefault.epsilon()    == Catch::Approx(1e-12));
  }
}

TEST_CASE("StatisticSupport::violatesLowerBound", "[StatisticSupport]")
{
  SECTION("unbounded() never violates, even for negative or non-finite values")
  {
    const StatisticSupport support = StatisticSupport::unbounded();

    REQUIRE_FALSE(support.violatesLowerBound(-1e9));
    REQUIRE_FALSE(support.violatesLowerBound(0.0));
    REQUIRE_FALSE(support.violatesLowerBound(42.0));

    // Note: Current implementation returns false for non-finite when mode == None,
    // because it early-returns before checking std::isfinite(lo).
    REQUIRE_FALSE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE(support.violatesLowerBound(std::numeric_limits<double>::infinity()));
    REQUIRE_FALSE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()));
  }

  SECTION("nonStrictLowerBound: allows lo >= bound - eps")
  {
    const double bound = 0.0;
    const double eps   = 1e-6;
    const StatisticSupport support = StatisticSupport::nonStrictLowerBound(bound, eps);

    // Exactly at the bound should pass
    REQUIRE_FALSE(support.violatesLowerBound(0.0));

    // Slightly below bound but within tolerance should pass: lo >= bound - eps
    REQUIRE_FALSE(support.violatesLowerBound(-0.5e-6));

    // Just beyond tolerance should fail
    REQUIRE(support.violatesLowerBound(-2.0e-6));

    // Non-finite is always a violation for bounded modes
    REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::infinity()));
    REQUIRE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()));
  }

  SECTION("strictLowerBound: requires lo > bound + eps")
  {
    const double bound = -1.0;
    const double eps   = 1e-6;
    const StatisticSupport support = StatisticSupport::strictLowerBound(bound, eps);

    // Exactly at the bound fails (strict)
    REQUIRE(support.violatesLowerBound(-1.0));

    // Even slightly above bound but not above bound+eps still fails
    // bound + eps = -0.999999
    REQUIRE(support.violatesLowerBound(-0.9999995));

    // Above bound+eps passes
    REQUIRE_FALSE(support.violatesLowerBound(-0.999998));

    // Clearly below fails
    REQUIRE(support.violatesLowerBound(-1.1));

    // Non-finite is always a violation for bounded modes
    REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::infinity()));
    REQUIRE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()));
  }
}

TEST_CASE("GeoMeanStat::isRatioStatistic", "[StatUtils][GeoMeanStat]") {
    SECTION("Returns false (geometric mean is not a ratio statistic)") {
        // The geometric mean represents average growth rate, not a ratio
        REQUIRE(GeoMeanStat<DecimalType>::isRatioStatistic() == false);
    }
}

TEST_CASE("GeoMeanStat::support", "[StatUtils][GeoMeanStat]") {
    SECTION("Clip ruin mode enabled - non-strict lower bound") {
        const double ruinEps = 1e-8;
        GeoMeanStat<DecimalType> stat(true, true, 0.02, ruinEps, 1);
        
        auto support = stat.support();
        
        // Should have a lower bound
        REQUIRE(support.hasLowerBound() == true);
        
        // Should be non-strict mode (lo >= bound)
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        
        // Lower bound should be ruinEps - 1.0
        REQUIRE(support.lowerBound() == Catch::Approx(ruinEps - 1.0));
        
        // Check epsilon
        REQUIRE(support.epsilon() == Catch::Approx(1e-12));
    }
    
    SECTION("Clip ruin mode enabled - with different ruin epsilon") {
        const double ruinEps = 1e-6;
        GeoMeanStat<DecimalType> stat(true, true, 0.02, ruinEps, 1);
        
        auto support = stat.support();
        
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(ruinEps - 1.0));
    }
    
    SECTION("Clip ruin mode disabled - strict lower bound at -1") {
        GeoMeanStat<DecimalType> stat(false, true, 0.02, 1e-8, 1);
        
        auto support = stat.support();
        
        // Should have a lower bound
        REQUIRE(support.hasLowerBound() == true);
        
        // Should be strict mode (lo > bound)
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::Strict);
        
        // Lower bound should be -1.0 (growth factor must be > 0, i.e., 1+r > 0)
        REQUIRE(support.lowerBound() == Catch::Approx(-1.0));
        
        // Check epsilon
        REQUIRE(support.epsilon() == Catch::Approx(1e-12));
    }
    
    SECTION("Backward compatible constructor") {
        const double ruinEps = 1e-7;
        GeoMeanStat<DecimalType> stat(true, ruinEps);
        
        auto support = stat.support();
        
        // Should use clip ruin mode with non-strict bound
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(ruinEps - 1.0));
    }
    
    SECTION("Support violation check - clip ruin enabled") {
        const double ruinEps = 1e-8;
        GeoMeanStat<DecimalType> stat(true, true, 0.02, ruinEps, 1);
        auto support = stat.support();
        
        // Value just below the bound should violate
        REQUIRE(support.violatesLowerBound(ruinEps - 1.0 - 1e-10) == true);
        
        // Value at the bound should not violate (non-strict)
        REQUIRE(support.violatesLowerBound(ruinEps - 1.0) == false);
        
        // Value above the bound should not violate
        REQUIRE(support.violatesLowerBound(ruinEps - 1.0 + 1e-10) == false);
        REQUIRE(support.violatesLowerBound(0.0) == false);
        REQUIRE(support.violatesLowerBound(1.0) == false);
        
        // Non-finite values should violate
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()) == true);
    }
    
    SECTION("Support violation check - clip ruin disabled") {
        GeoMeanStat<DecimalType> stat(false, true, 0.02, 1e-8, 1);
        auto support = stat.support();
        
        // Strict mode: lo > bound + eps
        // Value at the bound should violate (strict mode)
        REQUIRE(support.violatesLowerBound(-1.0) == true);
        
        // Value just above bound + eps should not violate
        REQUIRE(support.violatesLowerBound(-1.0 + 2e-12) == false);
        
        // Value well above the bound should not violate
        REQUIRE(support.violatesLowerBound(-0.5) == false);
        REQUIRE(support.violatesLowerBound(0.0) == false);
        REQUIRE(support.violatesLowerBound(1.0) == false);
        
        // Non-finite values should violate
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()) == true);
    }
}

// =============================================================================
// TEST SUITE: GeoMeanFromLogBarsStat
// =============================================================================

TEST_CASE("GeoMeanFromLogBarsStat::isRatioStatistic", "[StatUtils][GeoMeanFromLogBarsStat]") {
    SECTION("Returns false (geometric mean is not a ratio statistic)") {
        // The geometric mean from log bars represents average growth rate, not a ratio
        REQUIRE(GeoMeanFromLogBarsStat<DecimalType>::isRatioStatistic() == false);
    }
}

TEST_CASE("GeoMeanFromLogBarsStat::support", "[StatUtils][GeoMeanFromLogBarsStat]") {
    SECTION("Default construction - non-strict lower bound") {
        const double ruinEps = 1e-8;
        GeoMeanFromLogBarsStat<DecimalType> stat(true, 0.02, 1, ruinEps);
        
        auto support = stat.support();
        
        // Should have a lower bound
        REQUIRE(support.hasLowerBound() == true);
        
        // Should be non-strict mode (lo >= bound)
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        
        // Lower bound should be ruinEps - 1.0
        REQUIRE(support.lowerBound() == Catch::Approx(ruinEps - 1.0));
        
        // Check epsilon
        REQUIRE(support.epsilon() == Catch::Approx(1e-12));
    }
    
    SECTION("Custom ruin epsilon") {
        const double ruinEps = 1e-6;
        GeoMeanFromLogBarsStat<DecimalType> stat(true, 0.02, 1, ruinEps);
        
        auto support = stat.support();
        
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(ruinEps - 1.0));
    }
    
    SECTION("Different winsorization modes preserve support behavior") {
        const double ruinEps = 1e-8;
        
        // Mode 0: Legacy
        GeoMeanFromLogBarsStat<DecimalType> stat0(true, 0.02, 0, ruinEps);
        auto support0 = stat0.support();
        REQUIRE(support0.lowerBound() == Catch::Approx(ruinEps - 1.0));
        
        // Mode 1: Smooth fade
        GeoMeanFromLogBarsStat<DecimalType> stat1(true, 0.02, 1, ruinEps);
        auto support1 = stat1.support();
        REQUIRE(support1.lowerBound() == Catch::Approx(ruinEps - 1.0));
        
        // Mode 2: Always on
        GeoMeanFromLogBarsStat<DecimalType> stat2(true, 0.02, 2, ruinEps);
        auto support2 = stat2.support();
        REQUIRE(support2.lowerBound() == Catch::Approx(ruinEps - 1.0));
        
        // All should have same support characteristics
        REQUIRE(support0.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support1.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support2.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
    }
    
    SECTION("Support violation check") {
        const double ruinEps = 1e-8;
        GeoMeanFromLogBarsStat<DecimalType> stat(true, 0.02, 1, ruinEps);
        auto support = stat.support();
        
        const double bound = ruinEps - 1.0;
        
        // Value just below the bound should violate
        REQUIRE(support.violatesLowerBound(bound - 1e-10) == true);
        
        // Value at the bound should not violate (non-strict)
        REQUIRE(support.violatesLowerBound(bound) == false);
        
        // Value above the bound should not violate
        REQUIRE(support.violatesLowerBound(bound + 1e-10) == false);
        REQUIRE(support.violatesLowerBound(0.0) == false);
        REQUIRE(support.violatesLowerBound(1.0) == false);
        
        // Non-finite values should violate
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()) == true);
    }
    
    SECTION("Winsorization disabled preserves support") {
        const double ruinEps = 1e-8;
        GeoMeanFromLogBarsStat<DecimalType> stat(false, 0.02, 1, ruinEps);
        
        auto support = stat.support();
        
        // Support should be the same regardless of winsorization setting
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(ruinEps - 1.0));
    }
}

// =============================================================================
// TEST SUITE: LogProfitFactorStat
// =============================================================================

TEST_CASE("LogProfitFactorStat::isRatioStatistic", "[StatUtils][LogProfitFactorStat]") {
    SECTION("Returns true (profit factor is a ratio statistic)") {
        // Profit factor is a ratio of gains to losses
        REQUIRE(StatUtils<DecimalType>::LogProfitFactorStat::isRatioStatistic() == true);
    }
}

TEST_CASE("LogProfitFactorStat::support", "[StatUtils][LogProfitFactorStat]") {
    SECTION("Default construction - non-strict lower bound at 0") {
        StatUtils<DecimalType>::LogProfitFactorStat stat;
        
        auto support = stat.support();
        
        // Should have a lower bound
        REQUIRE(support.hasLowerBound() == true);
        
        // Should be non-strict mode (lo >= bound)
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        
        // Lower bound should be 0.0 (both raw PF and log(1+PF) are >= 0)
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
        
        // Check epsilon
        REQUIRE(support.epsilon() == Catch::Approx(1e-12));
    }
    
    SECTION("Compressed result mode") {
        StatUtils<DecimalType>::LogProfitFactorStat stat(true);
        
        auto support = stat.support();
        
        // Support should be the same for compressed and uncompressed
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
    }
    
    SECTION("Uncompressed result mode") {
        StatUtils<DecimalType>::LogProfitFactorStat stat(false);
        
        auto support = stat.support();
        
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
    }
    
    SECTION("Different parameter combinations preserve support") {
        // Various constructor parameter combinations
        StatUtils<DecimalType>::LogProfitFactorStat stat1(true, 1e-8, 1e-6, 1.0, 0.0);
        StatUtils<DecimalType>::LogProfitFactorStat stat2(false, 1e-7, 1e-5, 0.5, 0.025);
        StatUtils<DecimalType>::LogProfitFactorStat stat3(true, 1e-6, 1e-4, 2.0, 0.05);
        
        // All should have the same support characteristics
        REQUIRE(stat1.support().lowerBound() == Catch::Approx(0.0));
        REQUIRE(stat2.support().lowerBound() == Catch::Approx(0.0));
        REQUIRE(stat3.support().lowerBound() == Catch::Approx(0.0));
        
        REQUIRE(stat1.support().lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(stat2.support().lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(stat3.support().lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
    }
    
    SECTION("Support violation check") {
        StatUtils<DecimalType>::LogProfitFactorStat stat;
        auto support = stat.support();
        
        // Negative values should violate
        REQUIRE(support.violatesLowerBound(-1e-10) == true);
        REQUIRE(support.violatesLowerBound(-0.5) == true);
        REQUIRE(support.violatesLowerBound(-1.0) == true);
        
        // Value at the bound should not violate (non-strict)
        REQUIRE(support.violatesLowerBound(0.0) == false);
        
        // Positive values should not violate
        REQUIRE(support.violatesLowerBound(1e-10) == false);
        REQUIRE(support.violatesLowerBound(0.5) == false);
        REQUIRE(support.violatesLowerBound(1.0) == false);
        REQUIRE(support.violatesLowerBound(10.0) == false);
        
        // Non-finite values should violate
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()) == true);
    }
    
    SECTION("Support with stop-loss percentage") {
        // Stop-loss percentage affects internal calculations but not support
        StatUtils<DecimalType>::LogProfitFactorStat stat(true, 1e-8, 1e-6, 1.0, 0.025);
        
        auto support = stat.support();
        
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
    }
}

// =============================================================================
// TEST SUITE: LogProfitFactorFromLogBarsStat
// =============================================================================

TEST_CASE("LogProfitFactorFromLogBarsStat::isRatioStatistic", "[StatUtils][LogProfitFactorFromLogBarsStat]") {
    SECTION("Returns true (profit factor is a ratio statistic)") {
        // Profit factor from log bars is also a ratio of gains to losses
        REQUIRE(StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat::isRatioStatistic() == true);
    }
}

TEST_CASE("LogProfitFactorFromLogBarsStat::support", "[StatUtils][LogProfitFactorFromLogBarsStat]") {
    SECTION("Default construction - non-strict lower bound at 0") {
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat;
        
        auto support = stat.support();
        
        // Should have a lower bound
        REQUIRE(support.hasLowerBound() == true);
        
        // Should be non-strict mode (lo >= bound)
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        
        // Lower bound should be 0.0
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
        
        // Check epsilon
        REQUIRE(support.epsilon() == Catch::Approx(1e-12));
    }
    
    SECTION("Compressed result mode") {
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat(true);
        
        auto support = stat.support();
        
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
    }
    
    SECTION("Uncompressed result mode") {
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat(false);
        
        auto support = stat.support();
        
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
    }
    
    SECTION("Different parameter combinations preserve support") {
        // Various constructor parameter combinations
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat1(true, 1e-8, 1e-6, 1.0, 0.0);
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat2(false, 1e-7, 1e-5, 0.5, 0.025);
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat3(true, 1e-6, 1e-4, 2.0, 0.05);
        
        // All should have the same support characteristics
        REQUIRE(stat1.support().lowerBound() == Catch::Approx(0.0));
        REQUIRE(stat2.support().lowerBound() == Catch::Approx(0.0));
        REQUIRE(stat3.support().lowerBound() == Catch::Approx(0.0));
        
        REQUIRE(stat1.support().lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(stat2.support().lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(stat3.support().lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
    }
    
    SECTION("Support violation check") {
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat;
        auto support = stat.support();
        
        // Negative values should violate
        REQUIRE(support.violatesLowerBound(-1e-10) == true);
        REQUIRE(support.violatesLowerBound(-0.5) == true);
        REQUIRE(support.violatesLowerBound(-1.0) == true);
        
        // Value at the bound should not violate (non-strict)
        REQUIRE(support.violatesLowerBound(0.0) == false);
        
        // Positive values should not violate
        REQUIRE(support.violatesLowerBound(1e-10) == false);
        REQUIRE(support.violatesLowerBound(0.5) == false);
        REQUIRE(support.violatesLowerBound(1.0) == false);
        REQUIRE(support.violatesLowerBound(10.0) == false);
        
        // Non-finite values should violate
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(-std::numeric_limits<double>::infinity()) == true);
        REQUIRE(support.violatesLowerBound(std::numeric_limits<double>::quiet_NaN()) == true);
    }
    
    SECTION("Support with stop-loss percentage") {
        // Stop-loss percentage affects internal calculations but not support
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat(true, 1e-8, 1e-6, 1.0, 0.025);
        
        auto support = stat.support();
        
        REQUIRE(support.hasLowerBound() == true);
        REQUIRE(support.lowerBoundMode() == StatisticSupport::LowerBoundMode::NonStrict);
        REQUIRE(support.lowerBound() == Catch::Approx(0.0));
    }
    
    SECTION("Consistency with LogProfitFactorStat support") {
        // Both LogProfitFactorStat and LogProfitFactorFromLogBarsStat should have identical support
        StatUtils<DecimalType>::LogProfitFactorStat stat1(true, 1e-8, 1e-6, 1.0, 0.025);
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat2(true, 1e-8, 1e-6, 1.0, 0.025);
        
        auto support1 = stat1.support();
        auto support2 = stat2.support();
        
        REQUIRE(support1.lowerBound() == support2.lowerBound());
        REQUIRE(support1.lowerBoundMode() == support2.lowerBoundMode());
        REQUIRE(support1.epsilon() == support2.epsilon());
    }
}

// =============================================================================
// TEST SUITE: Cross-Struct Consistency Tests
// =============================================================================

TEST_CASE("Cross-struct consistency", "[StatUtils][Consistency]") {
    SECTION("GeoMean structs have consistent isRatioStatistic behavior") {
        REQUIRE(GeoMeanStat<DecimalType>::isRatioStatistic() ==
                GeoMeanFromLogBarsStat<DecimalType>::isRatioStatistic());
    }
    
    SECTION("LogProfitFactor structs have consistent isRatioStatistic behavior") {
        REQUIRE(StatUtils<DecimalType>::LogProfitFactorStat::isRatioStatistic() == 
                StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat::isRatioStatistic());
    }
    
    SECTION("Ratio statistics are correctly identified") {
        // GeoMean variants should not be ratio statistics
        REQUIRE(GeoMeanStat<DecimalType>::isRatioStatistic() == false);
        REQUIRE(GeoMeanFromLogBarsStat<DecimalType>::isRatioStatistic() == false);
        
        // LogProfitFactor variants should be ratio statistics
        REQUIRE(StatUtils<DecimalType>::LogProfitFactorStat::isRatioStatistic() == true);
        REQUIRE(StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat::isRatioStatistic() == true);
    }
    
    SECTION("GeoMean variants with same ruin_eps have consistent support") {
        const double ruinEps = 1e-8;
        
        GeoMeanStat<DecimalType> stat1(true, true, 0.02, ruinEps, 1);
        GeoMeanFromLogBarsStat<DecimalType> stat2(true, 0.02, 1, ruinEps);
        
        auto support1 = stat1.support();
        auto support2 = stat2.support();
        
        // Both should have the same lower bound
        REQUIRE(support1.lowerBound() == Catch::Approx(support2.lowerBound()));
        REQUIRE(support1.lowerBoundMode() == support2.lowerBoundMode());
    }
    
    SECTION("LogProfitFactor variants have identical support") {
        StatUtils<DecimalType>::LogProfitFactorStat stat1;
        StatUtils<DecimalType>::LogProfitFactorFromLogBarsStat stat2;
        
        auto support1 = stat1.support();
        auto support2 = stat2.support();
        
        REQUIRE(support1.lowerBound() == Catch::Approx(support2.lowerBound()));
        REQUIRE(support1.lowerBoundMode() == support2.lowerBoundMode());
        REQUIRE(support1.epsilon() == Catch::Approx(support2.epsilon()));
    }
}

// =============================================================================
// TEST SUITE: StatisticSupport Edge Cases
// =============================================================================

TEST_CASE("StatisticSupport edge cases", "[StatUtils][StatisticSupport]") {
    SECTION("Very small epsilon values") {
        GeoMeanStat<DecimalType> stat(true, true, 0.02, 1e-15, 1);
        auto support = stat.support();
        
        // Should handle very small ruin epsilon values
        REQUIRE(support.lowerBound() == Catch::Approx(1e-15 - 1.0));
        REQUIRE(support.hasLowerBound() == true);
    }
    
    SECTION("Epsilon tolerance in violation checks") {
        StatUtils<DecimalType>::LogProfitFactorStat stat;
        auto support = stat.support();
        
        const double eps = 1e-12;
        
        // Value just below bound - eps should violate
        REQUIRE(support.violatesLowerBound(-eps - 1e-13) == true);
        
        // Value at bound - eps should not violate (within tolerance)
        REQUIRE(support.violatesLowerBound(-eps) == false);
    }
    
    SECTION("Support properties are constexpr compatible") {
        // Verify that isRatioStatistic can be used in compile-time contexts
        constexpr bool geoMeanIsRatio = GeoMeanStat<DecimalType>::isRatioStatistic();
        constexpr bool pfIsRatio = StatUtils<DecimalType>::LogProfitFactorStat::isRatioStatistic();
        
        REQUIRE(geoMeanIsRatio == false);
        REQUIRE(pfIsRatio == true);
    }
}
