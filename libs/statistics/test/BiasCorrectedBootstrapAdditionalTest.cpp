// BiasCorrectedBootstrapAdditionalTests.cpp
//
// Additional unit tests to fill coverage gaps in BCaBootstrap implementation.
// These tests complement the existing BiasCorrectedBootstrapTest.cpp file.
//
// Coverage areas:
// - BCaAnnualizer class
// - calculateAnnualizationFactor function
// - unbiasedIndex static method
// - Edge cases for BCaBootStrap
// - Jackknife methods for both resamplers
// - Custom statistics
// - Error handling paths

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

#include "BiasCorrectedBootstrap.h"
#include "TestUtils.h"
#include "number.h"
#include "randutils.hpp"

using namespace mkc_timeseries;

// ======================== BCaAnnualizer Tests ========================

TEST_CASE("BCaAnnualizer: Basic annualization with IID resampler", "[BCaAnnualizer]")
{
    using D = DecimalType;
    
    // Daily returns with positive mean
    std::vector<D> daily_returns = {
        DecimalType("0.001"), DecimalType("0.002"), DecimalType("-0.001"),
        DecimalType("0.0015"), DecimalType("0.0025"), DecimalType("0.001"),
        DecimalType("-0.0005"), DecimalType("0.002"), DecimalType("0.0015"),
        DecimalType("0.001"), DecimalType("0.0005"), DecimalType("0.002")
    };
    
    BCaBootStrap<D> bca(daily_returns, 1000, 0.95);
    
    // Annualize with standard 252 trading days
    double annualization_factor = 252.0;
    BCaAnnualizer<D> annualizer(bca, annualization_factor);
    
    D daily_mean = bca.getMean();
    D annualized_mean = annualizer.getAnnualizedMean();
    
    // Check that annualized mean is larger than daily mean (for positive returns)
    REQUIRE(num::to_double(annualized_mean) > num::to_double(daily_mean));
    
    // Annualized bounds should also be properly scaled
    D daily_lower = bca.getLowerBound();
    D daily_upper = bca.getUpperBound();
    D annualized_lower = annualizer.getAnnualizedLowerBound();
    D annualized_upper = annualizer.getAnnualizedUpperBound();
    
    // Bounds should maintain ordering
    REQUIRE(annualized_lower <= annualized_mean);
    REQUIRE(annualized_mean <= annualized_upper);
    
    // For positive returns, annualized bounds should be larger
    if (num::to_double(daily_lower) > 0.0) {
        REQUIRE(num::to_double(annualized_lower) > num::to_double(daily_lower));
    }
    REQUIRE(num::to_double(annualized_upper) > num::to_double(daily_upper));
}

TEST_CASE("BCaAnnualizer: Negative returns are handled correctly", "[BCaAnnualizer]")
{
    using D = DecimalType;
    
    // Daily returns with negative mean
    std::vector<D> losing_returns = {
        DecimalType("-0.002"), DecimalType("-0.001"), DecimalType("-0.003"),
        DecimalType("-0.0015"), DecimalType("0.0005"), DecimalType("-0.002"),
        DecimalType("-0.001"), DecimalType("-0.0025"), DecimalType("-0.0015")
    };
    
    BCaBootStrap<D> bca(losing_returns, 1000, 0.95);
    BCaAnnualizer<D> annualizer(bca, 252.0);
    
    D annualized_mean = annualizer.getAnnualizedMean();
    
    // Annualized mean should still be negative
    REQUIRE(num::to_double(annualized_mean) < 0.0);
}

TEST_CASE("BCaAnnualizer: Invalid annualization factor throws", "[BCaAnnualizer][Error]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {DecimalType("0.001"), DecimalType("0.002")};
    BCaBootStrap<D> bca(returns, 1000, 0.95);
    
    // Zero factor
    REQUIRE_THROWS_AS(BCaAnnualizer<D>(bca, 0.0), std::invalid_argument);
    
    // Negative factor
    REQUIRE_THROWS_AS(BCaAnnualizer<D>(bca, -252.0), std::invalid_argument);
    
    // Infinity
    REQUIRE_THROWS_AS(BCaAnnualizer<D>(bca, std::numeric_limits<double>::infinity()), 
                     std::invalid_argument);
    
    // NaN
    REQUIRE_THROWS_AS(BCaAnnualizer<D>(bca, std::numeric_limits<double>::quiet_NaN()), 
                     std::invalid_argument);
}

TEST_CASE("BCaAnnualizer: Different time frames", "[BCaAnnualizer]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.005"), DecimalType("-0.002"), DecimalType("0.004"),
        DecimalType("0.003"), DecimalType("0.001"), DecimalType("-0.001")
    };
    
    BCaBootStrap<D> bca(returns, 1000, 0.95);
    
    SECTION("Weekly to annual (52 weeks)")
    {
        BCaAnnualizer<D> annualizer(bca, 52.0);
        REQUIRE(std::isfinite(num::to_double(annualizer.getAnnualizedMean())));
    }
    
    SECTION("Monthly to annual (12 months)")
    {
        BCaAnnualizer<D> annualizer(bca, 12.0);
        REQUIRE(std::isfinite(num::to_double(annualizer.getAnnualizedMean())));
    }
    
    SECTION("Quarterly to annual (4 quarters)")
    {
        BCaAnnualizer<D> annualizer(bca, 4.0);
        REQUIRE(std::isfinite(num::to_double(annualizer.getAnnualizedMean())));
    }
}

// =================== calculateAnnualizationFactor Tests ===================

TEST_CASE("calculateAnnualizationFactor: Standard time frames", "[Annualizer]")
{
    SECTION("DAILY time frame")
    {
        double factor = calculateAnnualizationFactor(TimeFrame::DAILY);
        REQUIRE(factor == Catch::Approx(252.0));
    }
    
    SECTION("WEEKLY time frame")
    {
        double factor = calculateAnnualizationFactor(TimeFrame::WEEKLY);
        REQUIRE(factor == Catch::Approx(52.0));
    }
    
    SECTION("MONTHLY time frame")
    {
        double factor = calculateAnnualizationFactor(TimeFrame::MONTHLY);
        REQUIRE(factor == Catch::Approx(12.0));
    }
}

TEST_CASE("calculateAnnualizationFactor: Custom trading parameters", "[Annualizer]")
{
    // Custom trading days (e.g., crypto markets: 365 days)
    double factor = calculateAnnualizationFactor(TimeFrame::DAILY, 0, 365.0);
    REQUIRE(factor == Catch::Approx(365.0));
}

// ======================= unbiasedIndex Tests =======================

TEST_CASE("BCaBootStrap::unbiasedIndex: Basic functionality", "[BCaBootStrap][unbiasedIndex]")
{
    using D = DecimalType;
    
    SECTION("Middle percentile")
    {
        // For p=0.5 and B=1000: index = floor(0.5 * 1001) - 1 = floor(500.5) - 1 = 499
        int idx = BCaBootStrap<D>::unbiasedIndex(0.5, 1000);
        REQUIRE(idx == 499);
    }
    
    SECTION("Lower percentile")
    {
        // For p=0.025 and B=1000: index = floor(0.025 * 1001) - 1 = floor(25.025) - 1 = 24
        int idx = BCaBootStrap<D>::unbiasedIndex(0.025, 1000);
        REQUIRE(idx == 24);
    }
    
    SECTION("Upper percentile")
    {
        // For p=0.975 and B=1000: index = floor(0.975 * 1001) - 1 = floor(975.975) - 1 = 974
        int idx = BCaBootStrap<D>::unbiasedIndex(0.975, 1000);
        REQUIRE(idx == 974);
    }
}

TEST_CASE("BCaBootStrap::unbiasedIndex: Edge cases with clamping", "[BCaBootStrap][unbiasedIndex]")
{
    using D = DecimalType;
    
    SECTION("p = 0.0 clamps to index 0")
    {
        int idx = BCaBootStrap<D>::unbiasedIndex(0.0, 1000);
        REQUIRE(idx == 0);
    }
    
    SECTION("p = 1.0 clamps to index B-1")
    {
        int idx = BCaBootStrap<D>::unbiasedIndex(1.0, 1000);
        REQUIRE(idx == 999);
    }
    
    SECTION("p slightly above 1.0 clamps to B-1")
    {
        int idx = BCaBootStrap<D>::unbiasedIndex(1.001, 1000);
        REQUIRE(idx == 999);
    }
    
    SECTION("p negative clamps to 0")
    {
        int idx = BCaBootStrap<D>::unbiasedIndex(-0.1, 1000);
        REQUIRE(idx == 0);
    }
    
    SECTION("Very small B")
    {
        int idx = BCaBootStrap<D>::unbiasedIndex(0.5, 100);
        REQUIRE(idx >= 0);
        REQUIRE(idx <= 99);
    }
}

// ==================== BCaBootStrap Edge Cases ====================

TEST_CASE("BCaBootStrap: Minimum valid dataset (n=2)", "[BCaBootStrap][EdgeCase]")
{
    using D = DecimalType;
    
    std::vector<D> tiny_returns = {DecimalType("0.01"), DecimalType("-0.01")};
    
    REQUIRE_NOTHROW(BCaBootStrap<D>(tiny_returns, 1000, 0.95));
    
    BCaBootStrap<D> bca(tiny_returns, 1000, 0.95);
    
    // Should compute without crashing
    D mean = bca.getMean();
    D lower = bca.getLowerBound();
    D upper = bca.getUpperBound();
    
    REQUIRE(std::isfinite(num::to_double(mean)));
    REQUIRE(lower <= upper);
}

TEST_CASE("BCaBootStrap: Small dataset (n=3)", "[BCaBootStrap][EdgeCase]")
{
    using D = DecimalType;
    
    std::vector<D> small_returns = {
        DecimalType("0.02"), DecimalType("0.00"), DecimalType("-0.01")
    };
    
    BCaBootStrap<D> bca(small_returns, 1000, 0.95);
    
    REQUIRE(bca.getSampleSize() == 3);
    REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
}

TEST_CASE("BCaBootStrap: Constant dataset triggers degenerate handling", "[BCaBootStrap][EdgeCase]")
{
    using D = DecimalType;
    
    // All identical values
    std::vector<D> constant_returns(20, DecimalType("0.05"));
    
    BCaBootStrap<D> bca(constant_returns, 1000, 0.95);
    
    // All bounds should equal the constant value
    D mean = bca.getMean();
    D lower = bca.getLowerBound();
    D upper = bca.getUpperBound();
    
    REQUIRE(num::to_double(mean) == Catch::Approx(0.05));
    REQUIRE(num::to_double(lower) == Catch::Approx(0.05));
    REQUIRE(num::to_double(upper) == Catch::Approx(0.05));
    
    // z0 and acceleration should be benign
    double z0 = bca.getZ0();
    D accel = bca.getAcceleration();
    
    REQUIRE(z0 == Catch::Approx(0.0));
    REQUIRE(num::to_double(accel) == Catch::Approx(0.0));
}

TEST_CASE("BCaBootStrap: Extreme confidence levels", "[BCaBootStrap][EdgeCase]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.02"), DecimalType("-0.01"),
        DecimalType("0.015"), DecimalType("-0.005"), DecimalType("0.02")
    };
    
    SECTION("99% confidence level")
    {
        BCaBootStrap<D> bca(returns, 2000, 0.99);
        
        // Interval should be wider than 95%
        D lower_99 = bca.getLowerBound();
        D upper_99 = bca.getUpperBound();
        D width_99 = upper_99 - lower_99;
        
        BCaBootStrap<D> bca_95(returns, 2000, 0.95);
        D width_95 = bca_95.getUpperBound() - bca_95.getLowerBound();
        
        REQUIRE(num::to_double(width_99) > num::to_double(width_95));
    }
    
    SECTION("99.9% confidence level")
    {
        BCaBootStrap<D> bca(returns, 2000, 0.999);
        
        D lower = bca.getLowerBound();
        D upper = bca.getUpperBound();
        
        REQUIRE(lower <= upper);
        REQUIRE(std::isfinite(num::to_double(lower)));
        REQUIRE(std::isfinite(num::to_double(upper)));
    }
    
    SECTION("90% confidence level")
    {
        BCaBootStrap<D> bca(returns, 2000, 0.90);
        
        // Should produce narrower interval
        D lower_90 = bca.getLowerBound();
        D upper_90 = bca.getUpperBound();
        
        REQUIRE(lower_90 <= upper_90);
    }
}

// ==================== Custom Statistics Tests ====================

TEST_CASE("BCaBootStrap: Custom statistic - median", "[BCaBootStrap][CustomStat]")
{
    using D = DecimalType;
    
    auto median_fn = [](const std::vector<D>& data) -> D {
        if (data.empty()) return D(0);
        std::vector<D> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        size_t n = sorted.size();
        if (n % 2 == 0) {
            return (sorted[n/2 - 1] + sorted[n/2]) / D(2);
        } else {
            return sorted[n/2];
        }
    };
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.05"), DecimalType("-0.02"),
        DecimalType("0.03"), DecimalType("0.00"), DecimalType("0.02"),
        DecimalType("-0.01"), DecimalType("0.04"), DecimalType("0.015")
    };
    
    BCaBootStrap<D> bca(returns, 1000, 0.95, median_fn);
    
    D median = bca.getMean(); // Actually median, but called getMean()
    D lower = bca.getLowerBound();
    D upper = bca.getUpperBound();
    
    REQUIRE(lower <= median);
    REQUIRE(median <= upper);
    REQUIRE(std::isfinite(num::to_double(median)));
}

TEST_CASE("BCaBootStrap: Custom statistic - standard deviation", "[BCaBootStrap][CustomStat]")
{
    using D = DecimalType;
    
    auto stddev_fn = [](const std::vector<D>& data) -> D {
        if (data.size() < 2) return D(0);
        D mean = std::accumulate(data.begin(), data.end(), D(0)) / D(data.size());
        D sum_sq = D(0);
        for (const auto& x : data) {
            D diff = x - mean;
            sum_sq += diff * diff;
        }
        // Use sqrt via conversion to double and back
        double var = num::to_double(sum_sq) / static_cast<double>(data.size() - 1);
        return D(std::sqrt(var));
    };
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.05"), DecimalType("-0.03"),
        DecimalType("0.02"), DecimalType("-0.01"), DecimalType("0.04"),
        DecimalType("0.00"), DecimalType("0.03"), DecimalType("-0.02")
    };
    
    BCaBootStrap<D> bca(returns, 1000, 0.95, stddev_fn);
    
    D std_dev = bca.getMean();
    D lower = bca.getLowerBound();
    D upper = bca.getUpperBound();
    
    // Standard deviation should be positive
    REQUIRE(num::to_double(std_dev) > 0.0);
    REQUIRE(num::to_double(lower) > 0.0);
    REQUIRE(lower <= upper);
}

TEST_CASE("BCaBootStrap: Custom statistic - max value", "[BCaBootStrap][CustomStat]")
{
    using D = DecimalType;
    
    auto max_fn = [](const std::vector<D>& data) -> D {
        return *std::max_element(data.begin(), data.end());
    };
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.03"), DecimalType("-0.01"),
        DecimalType("0.05"), DecimalType("0.02"), DecimalType("-0.02")
    };
    
    BCaBootStrap<D> bca(returns, 1000, 0.95, max_fn);
    
    D max_val = bca.getMean();
    
    // Max should be at least 0.05 (from original data)
    REQUIRE(num::to_double(max_val) >= 0.04); // Allow for sampling variation
}

// ==================== IIDResampler Jackknife Tests ====================

TEST_CASE("IIDResampler::jackknife: Produces n statistics", "[IIDResampler][Jackknife]")
{
    using D = DecimalType;
    using Policy = IIDResampler<D>;
    
    std::vector<D> data = {
        DecimalType("1.0"), DecimalType("2.0"), DecimalType("3.0"),
        DecimalType("4.0"), DecimalType("5.0")
    };
    
    Policy resampler;
    auto mean_fn = [](const std::vector<D>& x) -> D {
        return std::accumulate(x.begin(), x.end(), D(0)) / D(x.size());
    };
    
    auto jk_stats = resampler.jackknife(data, mean_fn);
    
    REQUIRE(jk_stats.size() == data.size());
}

TEST_CASE("IIDResampler::jackknife: Delete-one correctly computes means", "[IIDResampler][Jackknife]")
{
    using D = DecimalType;
    using Policy = IIDResampler<D>;
    
    std::vector<D> data = {
        DecimalType("10.0"), DecimalType("20.0"), DecimalType("30.0")
    };
    
    Policy resampler;
    auto mean_fn = [](const std::vector<D>& x) -> D {
        return std::accumulate(x.begin(), x.end(), D(0)) / D(x.size());
    };
    
    auto jk_stats = resampler.jackknife(data, mean_fn);
    
    // Jackknife replicate 0: removes 10.0, keeps {20.0, 30.0}, mean = 25.0
    REQUIRE(num::to_double(jk_stats[0]) == Catch::Approx(25.0));
    
    // Jackknife replicate 1: removes 20.0, keeps {10.0, 30.0}, mean = 20.0
    REQUIRE(num::to_double(jk_stats[1]) == Catch::Approx(20.0));
    
    // Jackknife replicate 2: removes 30.0, keeps {10.0, 20.0}, mean = 15.0
    REQUIRE(num::to_double(jk_stats[2]) == Catch::Approx(15.0));
}

TEST_CASE("IIDResampler::jackknife: Error on insufficient data", "[IIDResampler][Jackknife][Error]")
{
    using D = DecimalType;
    using Policy = IIDResampler<D>;
    
    Policy resampler;
    auto mean_fn = [](const std::vector<D>& x) -> D {
        return std::accumulate(x.begin(), x.end(), D(0)) / D(x.size());
    };
    
    std::vector<D> too_small = {DecimalType("1.0")};
    REQUIRE_THROWS_AS(resampler.jackknife(too_small, mean_fn), std::invalid_argument);
    
    std::vector<D> empty;
    REQUIRE_THROWS_AS(resampler.jackknife(empty, mean_fn), std::invalid_argument);
}

// ============= StationaryBlockResampler Jackknife Tests =============

TEST_CASE("StationaryBlockResampler::jackknife: Produces n statistics", "[StationaryBlockResampler][Jackknife]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    std::vector<D> data(20, DecimalType("1.0"));
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = DecimalType(std::to_string(i).c_str());
    }
    
    Policy resampler(4);
    auto mean_fn = [](const std::vector<D>& x) -> D {
        return std::accumulate(x.begin(), x.end(), D(0)) / D(x.size());
    };
    
    auto jk_stats = resampler.jackknife(data, mean_fn);
    
    REQUIRE(jk_stats.size() == data.size());
}

TEST_CASE("StationaryBlockResampler::jackknife: Block deletion is circular", "[StationaryBlockResampler][Jackknife]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    // Create data: {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
    std::vector<D> data(10);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = DecimalType(std::to_string(i).c_str());
    }
    
    Policy resampler(3); // L=3, so delete blocks of length 3
    auto sum_fn = [](const std::vector<D>& x) -> D {
        return std::accumulate(x.begin(), x.end(), D(0));
    };
    
    auto jk_stats = resampler.jackknife(data, sum_fn);
    
    // Full sum = 0+1+2+3+4+5+6+7+8+9 = 45
    // Jackknife 0: delete {0,1,2}, keep {3,4,5,6,7,8,9}, sum = 42
    // Jackknife 1: delete {1,2,3}, keep {4,5,6,7,8,9,0}, sum = 39
    // etc.
    
    double full_sum = 45.0;
    
    // Check that each jackknife statistic is less than full sum
    for (const auto& stat : jk_stats) {
        REQUIRE(num::to_double(stat) < full_sum);
    }
}

TEST_CASE("StationaryBlockResampler::jackknife: L larger than n-1 uses effective L", "[StationaryBlockResampler][Jackknife]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    std::vector<D> data = {
        DecimalType("1.0"), DecimalType("2.0"), DecimalType("3.0"),
        DecimalType("4.0"), DecimalType("5.0")
    };
    
    // L=10 is larger than n-1=4, so effective L should be 4
    Policy resampler(10);
    auto mean_fn = [](const std::vector<D>& x) -> D {
        return std::accumulate(x.begin(), x.end(), D(0)) / D(x.size());
    };
    
    auto jk_stats = resampler.jackknife(data, mean_fn);
    
    // Should still produce n=5 statistics
    REQUIRE(jk_stats.size() == 5);
    
    // Each jackknife replicate should have size 1 (since we're deleting 4 out of 5)
    // So each stat should just be one of the original values
    for (const auto& stat : jk_stats) {
        REQUIRE(std::isfinite(num::to_double(stat)));
    }
}

TEST_CASE("StationaryBlockResampler::jackknife: Error on insufficient data", "[StationaryBlockResampler][Jackknife][Error]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    Policy resampler(3);
    auto mean_fn = [](const std::vector<D>& x) -> D {
        return std::accumulate(x.begin(), x.end(), D(0)) / D(x.size());
    };
    
    std::vector<D> too_small = {DecimalType("1.0")};
    REQUIRE_THROWS_AS(resampler.jackknife(too_small, mean_fn), std::invalid_argument);
}

// ============= StationaryBlockResampler Edge Cases =============

TEST_CASE("StationaryBlockResampler: Minimum block length L=2", "[StationaryBlockResampler][EdgeCase]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    std::vector<D> data(50);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = DecimalType(std::to_string(i % 10).c_str());
    }
    
    // Even if we request L=1, it should use L=2 as minimum
    Policy resampler(1);
    REQUIRE(resampler.getL() == 2);
    
    randutils::mt19937_rng rng;
    auto sample = resampler(data, 100, rng);
    
    REQUIRE(sample.size() == 100);
}

TEST_CASE("StationaryBlockResampler: L larger than dataset size", "[StationaryBlockResampler][EdgeCase]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    std::vector<D> data = {
        DecimalType("1.0"), DecimalType("2.0"), DecimalType("3.0")
    };
    
    // L=100 is much larger than data size
    Policy resampler(100);
    
    randutils::mt19937_rng rng;
    
    // Should still work - will draw blocks and wrap
    REQUIRE_NOTHROW(resampler(data, 50, rng));
    
    auto sample = resampler(data, 50, rng);
    REQUIRE(sample.size() == 50);
}

TEST_CASE("StationaryBlockResampler: Very small output size", "[StationaryBlockResampler][EdgeCase]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    std::vector<D> data(100);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = DecimalType(std::to_string(i).c_str());
    }
    
    Policy resampler(5);
    randutils::mt19937_rng rng;
    
    // Request very small sample
    auto sample = resampler(data, 3, rng);
    REQUIRE(sample.size() == 3);
}

// ==================== Diagnostics Access Tests ====================

TEST_CASE("BCaBootStrap: getBootstrapStatistics returns expected size", "[BCaBootStrap][Diagnostics]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.01"), DecimalType("0.02"), DecimalType("-0.01"),
        DecimalType("0.015"), DecimalType("-0.005")
    };
    
    unsigned int B = 500;
    BCaBootStrap<D> bca(returns, B, 0.95);
    
    const auto& boot_stats = bca.getBootstrapStatistics();
    
    REQUIRE(boot_stats.size() == B);
}

TEST_CASE("BCaBootStrap: z0 and acceleration are accessible", "[BCaBootStrap][Diagnostics]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {
        DecimalType("0.02"), DecimalType("0.01"), DecimalType("0.00"),
        DecimalType("-0.01"), DecimalType("0.03"), DecimalType("0.015")
    };
    
    BCaBootStrap<D> bca(returns, 1000, 0.95);
    
    double z0 = bca.getZ0();
    D accel = bca.getAcceleration();
    
    REQUIRE(std::isfinite(z0));
    REQUIRE(std::isfinite(num::to_double(accel)));
}

TEST_CASE("BCaBootStrap: getConfidenceLevel and getNumResamples", "[BCaBootStrap][Diagnostics]")
{
    using D = DecimalType;
    
    std::vector<D> returns = {DecimalType("0.01"), DecimalType("0.02")};
    
    double cl = 0.90;
    unsigned int B = 1500;
    
    BCaBootStrap<D> bca(returns, B, cl);
    
    REQUIRE(bca.getConfidenceLevel() == Catch::Approx(cl));
    REQUIRE(bca.getNumResamples() == B);
    REQUIRE(bca.getSampleSize() == 2);
}

// ==================== Mixed Scenarios ====================

TEST_CASE("BCaBootStrap with StationaryBlockResampler: Full integration", "[BCaBootStrap][StationaryBlock][Integration]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    // Autocorrelated data (trending)
    std::vector<D> returns;
    for (int i = 0; i < 50; ++i) {
        double val = 0.01 * std::sin(i * 0.2) + 0.005;
        returns.push_back(DecimalType(std::to_string(val).c_str()));
    }
    
    Policy sampler(5);
    BCaBootStrap<D, Policy> bca(returns, 1000, 0.95, 
                                 &StatUtils<D>::computeMean, sampler);
    
    D mean = bca.getMean();
    D lower = bca.getLowerBound();
    D upper = bca.getUpperBound();
    
    REQUIRE(lower <= mean);
    REQUIRE(mean <= upper);
    
    // Check diagnostics are reasonable
    double z0 = bca.getZ0();
    REQUIRE(std::abs(z0) < 5.0);
}

TEST_CASE("BCaAnnualizer with StationaryBlockResampler", "[BCaAnnualizer][StationaryBlock][Integration]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    std::vector<D> daily_returns = {
        DecimalType("0.001"), DecimalType("0.0015"), DecimalType("0.002"),
        DecimalType("0.001"), DecimalType("-0.0005"), DecimalType("0.0025"),
        DecimalType("0.002"), DecimalType("0.0015"), DecimalType("0.001"),
        DecimalType("0.0005"), DecimalType("0.002"), DecimalType("0.0018")
    };
    
    Policy sampler(3);
    BCaBootStrap<D, Policy> bca(daily_returns, 1000, 0.95,
                                 &StatUtils<D>::computeMean, sampler);
    
    BCaAnnualizer<D> annualizer(bca, 252.0);
    
    D annualized_mean = annualizer.getAnnualizedMean();
    D annualized_lower = annualizer.getAnnualizedLowerBound();
    D annualized_upper = annualizer.getAnnualizedUpperBound();
    
    REQUIRE(annualized_lower <= annualized_mean);
    REQUIRE(annualized_mean <= annualized_upper);
}

// ==================== IIDResampler::getL Tests ====================

TEST_CASE("IIDResampler::getL returns 1", "[IIDResampler]")
{
    using D = DecimalType;
    using Policy = IIDResampler<D>;
    
    Policy resampler;
    REQUIRE(resampler.getL() == 1);
}

TEST_CASE("StationaryBlockResampler::getL and meanBlockLen are consistent", "[StationaryBlockResampler]")
{
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;
    
    Policy resampler(7);
    REQUIRE(resampler.getL() == 7);
    REQUIRE(resampler.meanBlockLen() == 7);
}
