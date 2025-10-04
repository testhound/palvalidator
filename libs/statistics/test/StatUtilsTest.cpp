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

#include "StatUtils.h"
#include "TestUtils.h" // For DecimalType typedef
#include "DecimalConstants.h"
#include "number.h" // For num::to_double

using namespace mkc_timeseries;

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
        GeoMeanStat<DecimalType> stat; // clip=false
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
