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
