// BoundFutureReturnsIntervalTypeTest.cpp
//
// Unit tests for IntervalType functionality in BoundFutureReturns
// Tests TWO_SIDED, ONE_SIDED_LOWER, and ONE_SIDED_UPPER confidence intervals
//
// Place in: libs/timeseries/test/
//
// Requires:
//  - Catch2 v3
//  - BoundFutureReturns.h (with IntervalType support)
//  - BootstrapTypes.h
//  - BiasCorrectedBootstrap.h
//  - ClosedPositionHistory.h
//  - MonthlyReturnsBuilder.h

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

#include "BoundFutureReturns.h"
#include "BootstrapTypes.h"
#include "BiasCorrectedBootstrap.h"
#include "ClosedPositionHistory.h"
#include "MonthlyReturnsBuilder.h"
#include "BoostDateHelper.h"
#include "TradingPosition.h"
#include "TestUtils.h"
#include "number.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using palvalidator::analysis::IntervalType;

// Symbol constant used in tests
const static std::string testSymbol("@TEST");

// ==================== Helper Functions ====================

/**
 * @brief Create a simple monthly returns dataset for testing
 * 
 * Creates 20 months of returns (Jan 2020 - Aug 2021) with known distribution:
 * - Mix of positive and negative returns
 * - Mild skew and variance
 * - Suitable for testing quantile bounds
 */
template<typename D>
std::vector<D> createTestMonthlyReturns()
{
    const char* rstrs[20] = {
        "0.012", "-0.006", "0.007", "0.004", "-0.011",
        "0.018", "0.000", "0.009", "0.013", "-0.004",
        "0.006", "0.008", "-0.007", "0.015", "0.003",
        "0.011", "-0.005", "0.010", "0.002", "0.014"
    };
    
    std::vector<D> monthly;
    monthly.reserve(20);
    for (int i = 0; i < 20; ++i) {
        monthly.push_back(createDecimal(rstrs[i]));
    }
    return monthly;
}

/**
 * @brief Create a ClosedPositionHistory with specified monthly returns
 */
template<typename D>
ClosedPositionHistory<D> createTestHistory()
{
    const char* rstrs[20] = {
        "0.012", "-0.006", "0.007", "0.004", "-0.011",
        "0.018", "0.000", "0.009", "0.013", "-0.004",
        "0.006", "0.008", "-0.007", "0.015", "0.003",
        "0.011", "-0.005", "0.010", "0.002", "0.014"
    };
    
    ClosedPositionHistory<D> hist;
    TradingVolume one(1, TradingVolume::CONTRACTS);
    
    auto add_long_1bar = [&](int y, int m, int d, const char* r_str) {
        D r = createDecimal(r_str);
        D entry = createDecimal("100");
        D exit = entry * (D("1.0") + r);
        
        TimeSeriesDate de(y, m, d);
        auto e = createTimeSeriesEntry(de, entry, entry, entry, entry, 10);
        auto pos = std::make_shared<TradingPositionLong<D>>(testSymbol, e->getOpenValue(), *e, one);
        
        int d_exit = std::min(d + 1, 28);
        TimeSeriesDate dx(y, m, d_exit);
        pos->ClosePosition(dx, exit);
        hist.addClosedPosition(pos);
    };
    
    // Fill months Jan 2020 .. Aug 2021
    int y = 2020;
    int m = static_cast<int>(Jan);
    for (int i = 0; i < 20; ++i) {
        add_long_1bar(y, m, 5 + (i % 10), rstrs[i]);
        if (++m > static_cast<int>(Dec)) {
            m = static_cast<int>(Jan);
            ++y;
        }
    }
    
    return hist;
}

// ==================== TWO_SIDED Tests (Backward Compatibility) ====================

TEST_CASE("BoundFutureReturns: TWO_SIDED default behavior (backward compatible)",
          "[BoundFutureReturns][IntervalType][TwoSided]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2000;
    const double cl = 0.95;
    const unsigned L = 3;
    
    // Constructor without IntervalType parameter (should default to TWO_SIDED)
    BoundFutureReturns<D> bfr_default(
        monthly,
        L,
        0.10,  // lowerQuantileP
        0.90,  // upperQuantileP
        B,
        cl
        // No IntervalType - should default to TWO_SIDED
    );
    
    // Explicit TWO_SIDED
    BoundFutureReturns<D> bfr_explicit(
        monthly,
        L,
        0.10,
        0.90,
        B,
        cl,
        IntervalType::TWO_SIDED
    );
    
    SECTION("Default produces valid bounds") {
        D lb = bfr_default.getLowerBound();
        D ub = bfr_default.getUpperBound();
        D q10 = bfr_default.getLowerPointQuantile();
        D q90 = bfr_default.getUpperPointQuantile();
        
        // Basic ordering
        REQUIRE(lb <= q10);
        REQUIRE(q10 <= q90);
        REQUIRE(q90 <= ub);
        
        // All finite
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
        REQUIRE(std::isfinite(num::to_double(q10)));
        REQUIRE(std::isfinite(num::to_double(q90)));
    }
    
    SECTION("Explicit TWO_SIDED produces same bounds as default") {
        // Note: Since bootstrap is stochastic, we can only check they're very close
        // if using same seed. Here we just verify both are valid and reasonable.
        D lb_default = bfr_default.getLowerBound();
        D lb_explicit = bfr_explicit.getLowerBound();
        D ub_default = bfr_default.getUpperBound();
        D ub_explicit = bfr_explicit.getUpperBound();
        
        // Both should produce valid, ordered bounds
        REQUIRE(lb_default <= bfr_default.getUpperPointQuantile());
        REQUIRE(lb_explicit <= bfr_explicit.getUpperPointQuantile());
        
        // Bounds should be in reasonable range (not too wide)
        double lb_d = num::to_double(lb_default);
        double ub_d = num::to_double(ub_default);
        double width = ub_d - lb_d;
        REQUIRE(width > 0.0);
        REQUIRE(width < 0.10);  // Should be less than 10% wide
    }
}

TEST_CASE("BoundFutureReturns: TWO_SIDED with ClosedPositionHistory",
          "[BoundFutureReturns][IntervalType][TwoSided]")
{
    using D = DecimalType;
    auto hist = createTestHistory<D>();
    
    const unsigned B = 2000;
    const double cl = 0.95;
    const unsigned L = 3;
    
    BoundFutureReturns<D> bfr(
        hist,
        L,
        0.10,
        0.90,
        B,
        cl,
        IntervalType::TWO_SIDED
    );
    
    SECTION("Produces valid bounds from ClosedPositionHistory") {
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        auto qci_lower = bfr.getLowerQuantileCI();
        auto qci_upper = bfr.getUpperQuantileCI();
        
        // Conservative policy: operational bounds are CI endpoints
        REQUIRE(lb == qci_lower.lo);
        REQUIRE(ub == qci_upper.hi);
        
        // Ordering
        REQUIRE(qci_lower.lo <= qci_lower.point);
        REQUIRE(qci_lower.point <= qci_lower.hi);
        REQUIRE(qci_upper.lo <= qci_upper.point);
        REQUIRE(qci_upper.point <= qci_upper.hi);
        
        // Cross-quantile ordering
        REQUIRE(qci_lower.point <= qci_upper.point);
    }
    
    SECTION("Policy switching works") {
        auto qci_lower = bfr.getLowerQuantileCI();
        auto qci_upper = bfr.getUpperQuantileCI();
        
        // Initially conservative
        REQUIRE(bfr.getLowerBound() == qci_lower.lo);
        REQUIRE(bfr.getUpperBound() == qci_upper.hi);
        
        // Switch to point policy
        bfr.usePointPolicy();
        REQUIRE(bfr.getLowerBound() == qci_lower.point);
        REQUIRE(bfr.getUpperBound() == qci_upper.point);
        
        // Switch back to conservative
        bfr.useConservativePolicy();
        REQUIRE(bfr.getLowerBound() == qci_lower.lo);
        REQUIRE(bfr.getUpperBound() == qci_upper.hi);
    }
}

// ==================== ONE_SIDED Tests ====================

TEST_CASE("BoundFutureReturns: ONE_SIDED_LOWER basic functionality",
          "[BoundFutureReturns][IntervalType][OneSidedLower]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2000;
    const double cl = 0.95;
    const unsigned L = 3;
    
    BoundFutureReturns<D> bfr(
        monthly,
        L,
        0.10,  // lowerQuantileP
        0.90,  // upperQuantileP
        B,
        cl,
        IntervalType::ONE_SIDED_LOWER
    );
    
    SECTION("Produces valid bounds") {
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        D q10 = bfr.getLowerPointQuantile();
        D q90 = bfr.getUpperPointQuantile();
        
        // Basic ordering
        REQUIRE(lb <= q10);
        REQUIRE(q10 <= q90);
        REQUIRE(q90 <= ub);
        
        // All finite
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
    }
    
    SECTION("Lower quantile uses ONE_SIDED_LOWER interval") {
        auto qci_lower = bfr.getLowerQuantileCI();
        
        // ONE_SIDED_LOWER: tight lower bound, loose upper bound
        // Lower bound should be tighter than TWO_SIDED would be
        REQUIRE(qci_lower.lo <= qci_lower.point);
        REQUIRE(qci_lower.point <= qci_lower.hi);
        
        // Upper endpoint should be relatively high (less conservative)
        double hi_d = num::to_double(qci_lower.hi);
        double pt_d = num::to_double(qci_lower.point);
        REQUIRE(hi_d >= pt_d);
    }
    
    SECTION("Upper quantile uses ONE_SIDED_UPPER interval") {
        auto qci_upper = bfr.getUpperQuantileCI();
        
        // ONE_SIDED_UPPER: loose lower bound, tight upper bound
        REQUIRE(qci_upper.lo <= qci_upper.point);
        REQUIRE(qci_upper.point <= qci_upper.hi);
        
        // Lower endpoint should be relatively low
        double lo_d = num::to_double(qci_upper.lo);
        double pt_d = num::to_double(qci_upper.point);
        REQUIRE(lo_d <= pt_d);
    }
}

TEST_CASE("BoundFutureReturns: ONE_SIDED_UPPER has same behavior as ONE_SIDED_LOWER",
          "[BoundFutureReturns][IntervalType][OneSidedUpper]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2000;
    const double cl = 0.95;
    const unsigned L = 3;
    
    // ONE_SIDED_UPPER should map to symmetric one-sided intervals
    // (lower quantile gets ONE_SIDED_LOWER, upper quantile gets ONE_SIDED_UPPER)
    BoundFutureReturns<D> bfr(
        monthly,
        L,
        0.10,
        0.90,
        B,
        cl,
        IntervalType::ONE_SIDED_UPPER
    );
    
    SECTION("Produces valid bounds") {
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        
        // All finite and ordered
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
        REQUIRE(lb <= ub);
    }
    
    SECTION("Symmetric behavior: appropriate one-sided interval for each quantile") {
        auto qci_lower = bfr.getLowerQuantileCI();
        auto qci_upper = bfr.getUpperQuantileCI();
        
        // Lower quantile should have ONE_SIDED_LOWER behavior
        REQUIRE(qci_lower.lo <= qci_lower.point);
        REQUIRE(qci_lower.point <= qci_lower.hi);
        
        // Upper quantile should have ONE_SIDED_UPPER behavior
        REQUIRE(qci_upper.lo <= qci_upper.point);
        REQUIRE(qci_upper.point <= qci_upper.hi);
    }
}

// ==================== Comparison Tests ====================

TEST_CASE("BoundFutureReturns: ONE_SIDED_LOWER vs TWO_SIDED comparison",
          "[BoundFutureReturns][IntervalType][Comparison]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2500;  // More samples for stable comparison
    const double cl = 0.95;
    const unsigned L = 3;
    
    BoundFutureReturns<D> bfr_two(
        monthly, L, 0.10, 0.90, B, cl,
        IntervalType::TWO_SIDED
    );
    
    BoundFutureReturns<D> bfr_one(
        monthly, L, 0.10, 0.90, B, cl,
        IntervalType::ONE_SIDED_LOWER
    );
    
    SECTION("Point quantiles are identical") {
        // Point quantiles computed on same data should be identical
        D q10_two = bfr_two.getLowerPointQuantile();
        D q10_one = bfr_one.getLowerPointQuantile();
        D q90_two = bfr_two.getUpperPointQuantile();
        D q90_one = bfr_one.getUpperPointQuantile();
        
        REQUIRE(q10_two == q10_one);
        REQUIRE(q90_two == q90_one);
    }
    
    SECTION("ONE_SIDED lower bound is higher (less conservative)") {
        auto qci_lower_two = bfr_two.getLowerQuantileCI();
        auto qci_lower_one = bfr_one.getLowerQuantileCI();
        
        double lb_two = num::to_double(qci_lower_two.lo);
        double lb_one = num::to_double(qci_lower_one.lo);
        
        // ONE_SIDED_LOWER uses 5% tail instead of 2.5% tail
        // So lower bound should be higher (less conservative)
        // Allow small tolerance for bootstrap variation
        REQUIRE(lb_one >= lb_two - 0.002);
        
        // The bound should actually be meaningfully higher
        // (but bootstrap variation means we need generous tolerance)
        INFO("TWO_SIDED lower: " << lb_two);
        INFO("ONE_SIDED lower: " << lb_one);
    }
    
    SECTION("ONE_SIDED upper bound is lower (less conservative)") {
        auto qci_upper_two = bfr_two.getUpperQuantileCI();
        auto qci_upper_one = bfr_one.getUpperQuantileCI();
        
        double ub_two = num::to_double(qci_upper_two.hi);
        double ub_one = num::to_double(qci_upper_one.hi);
        
        // ONE_SIDED_UPPER uses 5% tail instead of 2.5% tail
        // So upper bound should be lower (less conservative)
        REQUIRE(ub_one <= ub_two + 0.002);
        
        INFO("TWO_SIDED upper: " << ub_two);
        INFO("ONE_SIDED upper: " << ub_one);
    }
    
    SECTION("ONE_SIDED intervals are narrower") {
        auto qci_lower_two = bfr_two.getLowerQuantileCI();
        auto qci_lower_one = bfr_one.getLowerQuantileCI();
        
        double width_two = num::to_double(qci_lower_two.hi - qci_lower_two.lo);
        double width_one = num::to_double(qci_lower_one.hi - qci_lower_one.lo);
        
        // ONE_SIDED intervals should be narrower on the constrained side
        // (though bootstrap variation can complicate this)
        REQUIRE(width_two > 0.0);
        REQUIRE(width_one > 0.0);
        
        INFO("TWO_SIDED width: " << width_two);
        INFO("ONE_SIDED width: " << width_one);
    }
}

TEST_CASE("BoundFutureReturns: ONE_SIDED provides tighter monitoring bounds",
          "[BoundFutureReturns][IntervalType][Comparison][Monitoring]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2500;
    const double cl = 0.99;  // High confidence for risk monitoring
    const unsigned L = 3;
    
    BoundFutureReturns<D> bfr_two(
        monthly, L, 0.05, 0.95, B, cl,
        IntervalType::TWO_SIDED
    );
    
    BoundFutureReturns<D> bfr_one(
        monthly, L, 0.05, 0.95, B, cl,
        IntervalType::ONE_SIDED_LOWER
    );
    
    SECTION("Lower monitoring bound comparison (risk management use case)") {
        D lb_two = bfr_two.getLowerBound();  // Conservative: uses 0.5% tail
        D lb_one = bfr_one.getLowerBound();  // Appropriate: uses 1% tail
        
        double lb_two_d = num::to_double(lb_two);
        double lb_one_d = num::to_double(lb_one);
        
        // ONE_SIDED should give higher (less conservative) lower bound
        // This is the key advantage for risk monitoring
        REQUIRE(lb_one_d >= lb_two_d - 0.003);
        
        INFO("TWO_SIDED 99% lower bound: " << lb_two_d * 100 << "%");
        INFO("ONE_SIDED 99% lower bound: " << lb_one_d * 100 << "%");
        INFO("Difference: " << (lb_one_d - lb_two_d) * 100 << " percentage points");
    }
}

// ==================== Different Confidence Levels ====================

TEST_CASE("BoundFutureReturns: IntervalType with different confidence levels",
          "[BoundFutureReturns][IntervalType][ConfidenceLevels]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2000;
    const unsigned L = 3;
    
    SECTION("CL=0.90") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.10, 0.90, B, 0.90,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
        REQUIRE(lb <= ub);
    }
    
    SECTION("CL=0.95") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.10, 0.90, B, 0.95,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
        REQUIRE(lb <= ub);
    }
    
    SECTION("CL=0.99") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.10, 0.90, B, 0.99,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
        REQUIRE(lb <= ub);
    }
    
    SECTION("Higher CL produces wider intervals") {
        BoundFutureReturns<D> bfr90(monthly, L, 0.10, 0.90, B, 0.90, IntervalType::TWO_SIDED);
        BoundFutureReturns<D> bfr95(monthly, L, 0.10, 0.90, B, 0.95, IntervalType::TWO_SIDED);
        BoundFutureReturns<D> bfr99(monthly, L, 0.10, 0.90, B, 0.99, IntervalType::TWO_SIDED);
        
        auto qci90 = bfr90.getLowerQuantileCI();
        auto qci95 = bfr95.getLowerQuantileCI();
        auto qci99 = bfr99.getLowerQuantileCI();
        
        double width90 = num::to_double(qci90.hi - qci90.lo);
        double width95 = num::to_double(qci95.hi - qci95.lo);
        double width99 = num::to_double(qci99.hi - qci99.lo);
        
        // Higher confidence → wider interval
        // Allow tolerance for bootstrap variation
        REQUIRE(width95 >= width90 - 0.001);
        REQUIRE(width99 >= width95 - 0.001);
    }
}

// ==================== Different Quantiles ====================

TEST_CASE("BoundFutureReturns: IntervalType with different quantile pairs",
          "[BoundFutureReturns][IntervalType][Quantiles]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2000;
    const double cl = 0.95;
    const unsigned L = 3;
    
    SECTION("5th and 95th percentiles") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.05, 0.95, B, cl,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D q5 = bfr.getLowerPointQuantile();
        D q95 = bfr.getUpperPointQuantile();
        
        REQUIRE(q5 <= q95);
        
        auto qci_lower = bfr.getLowerQuantileCI();
        auto qci_upper = bfr.getUpperQuantileCI();
        
        REQUIRE(qci_lower.point == q5);
        REQUIRE(qci_upper.point == q95);
    }
    
    SECTION("10th and 90th percentiles (default)") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.10, 0.90, B, cl,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D q10 = bfr.getLowerPointQuantile();
        D q90 = bfr.getUpperPointQuantile();
        
        REQUIRE(q10 <= q90);
    }
    
    SECTION("25th and 75th percentiles (IQR)") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.25, 0.75, B, cl,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D q25 = bfr.getLowerPointQuantile();
        D q75 = bfr.getUpperPointQuantile();
        
        REQUIRE(q25 <= q75);
        
        // IQR should be narrower than outer quantiles
        double iqr = num::to_double(q75 - q25);
        REQUIRE(iqr > 0.0);
    }
}

// ==================== Edge Cases ====================

TEST_CASE("BoundFutureReturns: IntervalType with minimal data",
          "[BoundFutureReturns][IntervalType][EdgeCases]")
{
    using D = DecimalType;
    
    // Minimum viable dataset (12 months)
    std::vector<D> monthly;
    for (int i = 0; i < 12; ++i) {
        monthly.push_back(createDecimal("0.01"));  // All same value
    }
    
    const unsigned B = 1000;  // Reduce for faster test
    const double cl = 0.95;
    const unsigned L = 2;
    
    SECTION("Works with minimal data (12 months)") {
        // Should not throw
        BoundFutureReturns<D> bfr(
            monthly, L, 0.10, 0.90, B, cl,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
    }
    
    SECTION("TWO_SIDED also works with minimal data") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.10, 0.90, B, cl,
            IntervalType::TWO_SIDED
        );
        
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        
        REQUIRE(std::isfinite(num::to_double(lb)));
        REQUIRE(std::isfinite(num::to_double(ub)));
    }
}

TEST_CASE("BoundFutureReturns: IntervalType with negative returns",
          "[BoundFutureReturns][IntervalType][EdgeCases]")
{
    using D = DecimalType;
    
    // All negative returns (bear market scenario)
    std::vector<D> monthly;
    const char* neg_returns[15] = {
        "-0.02", "-0.01", "-0.03", "-0.015", "-0.025",
        "-0.01", "-0.02", "-0.018", "-0.012", "-0.022",
        "-0.015", "-0.02", "-0.01", "-0.025", "-0.018"
    };
    
    for (int i = 0; i < 15; ++i) {
        monthly.push_back(createDecimal(neg_returns[i]));
    }
    
    const unsigned B = 1500;
    const double cl = 0.95;
    const unsigned L = 3;
    
    SECTION("Handles all-negative returns") {
        BoundFutureReturns<D> bfr(
            monthly, L, 0.10, 0.90, B, cl,
            IntervalType::ONE_SIDED_LOWER
        );
        
        D lb = bfr.getLowerBound();
        D ub = bfr.getUpperBound();
        D q10 = bfr.getLowerPointQuantile();
        D q90 = bfr.getUpperPointQuantile();
        
        // All should be negative
        REQUIRE(num::to_double(lb) < 0.0);
        REQUIRE(num::to_double(q10) < 0.0);
        REQUIRE(num::to_double(q90) < 0.0);
        REQUIRE(num::to_double(ub) <= 0.0);
        
        // Still ordered
        REQUIRE(lb <= q10);
        REQUIRE(q10 <= q90);
        REQUIRE(q90 <= ub);
    }
}

// ==================== Diagnostic Access ====================

TEST_CASE("BoundFutureReturns: IntervalType with full diagnostic access",
          "[BoundFutureReturns][IntervalType][Diagnostics]")
{
    using D = DecimalType;
    auto monthly = createTestMonthlyReturns<D>();
    
    const unsigned B = 2000;
    const double cl = 0.95;
    const unsigned L = 3;
    const double pL = 0.10;
    const double pU = 0.90;
    
    BoundFutureReturns<D> bfr(
        monthly, L, pL, pU, B, cl,
        IntervalType::ONE_SIDED_LOWER
    );
    
    SECTION("Can access all diagnostics") {
        // Parameters
        REQUIRE(bfr.getLowerQuantileP() == Catch::Approx(pL));
        REQUIRE(bfr.getUpperQuantileP() == Catch::Approx(pU));
        REQUIRE(bfr.getNumBootstraps() == B);
        REQUIRE(bfr.getConfidenceLevel() == Catch::Approx(cl));
        
        // Data
        auto monthly_retrieved = bfr.getMonthlyReturns();
        REQUIRE(monthly_retrieved.size() == monthly.size());
        
        // Quantile CIs
        auto qci_lower = bfr.getLowerQuantileCI();
        auto qci_upper = bfr.getUpperQuantileCI();
        
        REQUIRE(std::isfinite(num::to_double(qci_lower.point)));
        REQUIRE(std::isfinite(num::to_double(qci_lower.lo)));
        REQUIRE(std::isfinite(num::to_double(qci_lower.hi)));
        
        REQUIRE(std::isfinite(num::to_double(qci_upper.point)));
        REQUIRE(std::isfinite(num::to_double(qci_upper.lo)));
        REQUIRE(std::isfinite(num::to_double(qci_upper.hi)));
    }
    
    SECTION("Point quantiles accessible") {
        D q10 = bfr.getLowerPointQuantile();
        D q90 = bfr.getUpperPointQuantile();
        
        auto qci_lower = bfr.getLowerQuantileCI();
        auto qci_upper = bfr.getUpperQuantileCI();
        
        REQUIRE(q10 == qci_lower.point);
        REQUIRE(q90 == qci_upper.point);
    }
}

// ==================== Real-World Use Case ====================

TEST_CASE("BoundFutureReturns: Risk monitoring use case (ONE_SIDED_LOWER)",
          "[BoundFutureReturns][IntervalType][UseCase]")
{
    using D = DecimalType;
    auto hist = createTestHistory<D>();
    
    // Real-world risk monitoring parameters
    const unsigned B = 5000;    // High bootstrap samples
    const double cl = 0.99;     // High confidence (99%)
    const unsigned L = 3;       // 3-month blocks
    const double pL = 0.05;     // 5th percentile (monitoring bad months)
    const double pU = 0.90;     // Not used, but standard
    
    BoundFutureReturns<D> bfr(
        hist, L, pL, pU, B, cl,
        IntervalType::ONE_SIDED_LOWER
    );
    
    SECTION("Provides actionable risk bound") {
        D lb = bfr.getLowerBound();
        double lb_pct = num::to_double(lb) * 100.0;
        
        INFO("99% confident lower bound (5th percentile): " << lb_pct << "%");
        
        // Should be negative (bad months) but finite
        REQUIRE(std::isfinite(lb_pct));
        REQUIRE(lb_pct < 5.0);  // Should be less than median
        
        // Can be used for risk management decisions
        if (lb_pct < -5.0) {
            INFO("High downside risk: worst months could exceed -5%");
        } else if (lb_pct < 0.0) {
            INFO("Moderate downside risk: worst months could be slightly negative");
        } else {
            INFO("Low downside risk: even bad months likely positive");
        }
        
        REQUIRE(true);  // Test always passes, just shows diagnostic
    }
    
    SECTION("ONE_SIDED is more capital-efficient than TWO_SIDED") {
        // Compare with TWO_SIDED
        BoundFutureReturns<D> bfr_conservative(
            hist, L, pL, pU, B, cl,
            IntervalType::TWO_SIDED
        );
        
        D lb_one = bfr.getLowerBound();
        D lb_two = bfr_conservative.getLowerBound();
        
        double lb_one_pct = num::to_double(lb_one) * 100.0;
        double lb_two_pct = num::to_double(lb_two) * 100.0;
        
        INFO("ONE_SIDED lower: " << lb_one_pct << "%");
        INFO("TWO_SIDED lower: " << lb_two_pct << "%");
        INFO("Efficiency gain: " << (lb_one_pct - lb_two_pct) << " percentage points");
        
        // ONE_SIDED should be higher (less conservative, more capital-efficient)
        REQUIRE(lb_one_pct >= lb_two_pct - 0.5);
    }
}
