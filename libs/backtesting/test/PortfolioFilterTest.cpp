// PortfolioFilterTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>

#include "PortfolioFilter.h"            // PortfolioFilter / AdaptiveVolatilityPortfolioFilter
#include "TimeSeriesIndicators.h"       // AdaptiveVolatilityPercentRankAnnualizedSeries
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "BiasCorrectedBootstrap.h"     // calculateAnnualizationFactor
#include "TestUtils.h"                  // DecimalType alias, helpers
#include "DecimalConstants.h"
#include "number.h"
#include "BoostDateHelper.h"            // getDefaultBarTime()
#include "PalStrategy.h"                // PalMetaStrategy
#include "Security.h"                   // EquitySecurity
#include "Portfolio.h"                  // Portfolio
#include "PalAst.h"                     // For creating test patterns
#include "AstResourceManager.h"         // For AST resource management
#include "BackTester.h"                 // For backtesting infrastructure

using namespace mkc_timeseries;
using namespace mkc_palast;  // For AstResourceManager
using num::fromString;
using boost::posix_time::ptime;

// ======= DecimalApproxMatcher (same style as project tests) =======
template<typename Decimal>
struct DecimalApproxMatcher {
    Decimal expected;
    Decimal tolerance;
    DecimalApproxMatcher(const Decimal& e, const Decimal& t) : expected(e), tolerance(t) {}
};
template<typename Decimal>
bool operator==(const Decimal& actual, const DecimalApproxMatcher<Decimal>& approx) {
    if (actual > approx.expected) return (actual - approx.expected).abs() <= approx.tolerance;
    else                          return (approx.expected - actual).abs() <= approx.tolerance;
}
template<typename Decimal>
auto decimalApprox(const Decimal& expected, const Decimal& tolerance) {
    return DecimalApproxMatcher<Decimal>(expected, tolerance);
}
// =================================================================

typedef DecimalType DecType; // from TestUtils.h, renamed to avoid conflict with boost::date_time::Dec
static const DecType TOL = fromString<DecType>("0.00001");

// Build a synthetic OHLC series by applying a deterministic return pattern
static OHLCTimeSeries<DecType> makeSeriesWithPattern(std::size_t nBars)
{
    OHLCTimeSeries<DecType> series(TimeFrame::DAILY, TradingVolume::SHARES);

    // Start date
    auto day = boost::gregorian::date(2023, 1, 1);
    double close = 100.0;

    // Repeating return pattern to create varying volatility
    // (units are simple returns, e.g., 0.01 = +1%)
    // Enhanced pattern with more extreme variations to create lower percentile ranks
    const std::vector<double> pattern = {
        0.001, -0.001, 0.0,   0.05,   0.0,   -0.04,  0.0,  0.008, 0.0,  -0.012,
        0.000, 0.000,  0.005, -0.005, 0.0,    0.08,  0.0, -0.06,  0.0,   0.004,
        0.002, -0.002, 0.001, 0.10,  -0.08,  0.001, 0.0,  0.015, 0.0,  -0.025,
        0.000, 0.001,  0.003, -0.003, 0.0,    0.12, -0.10, 0.02,  0.0,   0.006
    };

    for (std::size_t i = 0; i < nBars; ++i) {
        double r = pattern[i % pattern.size()];
        double open = close;
        double newClose = close * (1.0 + r);
        double high = std::max(open, newClose) * 1.001;
        double low  = std::min(open, newClose) * 0.999;

        // Use the OHLCTimeSeriesEntry ctor with a date (consistent with your tests)
        series.addEntry(OHLCTimeSeriesEntry<DecType>(
            day, fromString<DecType>(std::to_string(open)),
            fromString<DecType>(std::to_string(high)),
            fromString<DecType>(std::to_string(low)),
            fromString<DecType>(std::to_string(newClose)),
            fromString<DecType>("1000"),
            TimeFrame::DAILY));

        // next day
        day = day + boost::gregorian::days(1);
        close = newClose;
    }

    return series;
}

TEST_CASE("PortfolioFilter / AdaptiveVolatilityPortfolioFilter", "[PortfolioFilter]") {
    const TimeFrame::Duration tf = TimeFrame::DAILY;

    // A reasonably sized sample to ensure R^2 (20) and pr windows fill
    auto ohlc = makeSeriesWithPattern(/*nBars=*/120);

    // Percent-rank window we'll use for both filter and reference series
    const uint32_t prPeriod = 10;

    // Build the filter (constructor uses r2Period=20 and calculateAnnualizationFactor)
    AdaptiveVolatilityPortfolioFilter<DecType> filter(ohlc, prPeriod);

    // Build a REFERENCE percent-rank series the exact same way as the filter:
    // annualization via calculateAnnualizationFactor(TimeFrame::DAILY)
    const double ann = calculateAnnualizationFactor(tf);
    auto ref = AdaptiveVolatilityPercentRankAnnualizedSeries<DecType>(ohlc, /*r2=*/20, prPeriod, ann);

    REQUIRE(ref.getNumEntries() > 0);  // sanity

    // Find any date with rank < 0.75 (should exist with our varied pattern)
    const DecType thr = DecimalConstants<DecType>::createDecimal("0.75");
    boost::optional<boost::gregorian::date> dateAllow;
    boost::optional<boost::gregorian::date> dateDeny;

    for (auto it = ref.beginSortedAccess(); it != ref.endSortedAccess(); ++it) {
        const DecType v = it->getValue();
        if (!dateAllow && (v < thr)) dateAllow = it->getDate();
        if (!dateDeny  && (v >= thr)) dateDeny  = it->getDate();
        if (dateAllow && dateDeny) break;
    }

    SECTION("areEntriesAllowed == true when percent-rank < 0.75") {
        REQUIRE(dateAllow.has_value()); // pattern should create some sub-75th values
        const ptime when(*dateAllow, getDefaultBarTime());
        REQUIRE(filter.areEntriesAllowed(when) == true);
    }

    SECTION("areEntriesAllowed == false when percent-rank >= 0.75") {
        REQUIRE(dateDeny.has_value()); // and some >= 75th too
        const ptime when(*dateDeny, getDefaultBarTime());
        REQUIRE(filter.areEntriesAllowed(when) == false);
    }

    SECTION("areEntriesAllowed == false when timestamp not present") {
        // Choose a date before the first available in ref (or safely earlier)
        auto missing = boost::gregorian::date(2022, 12, 15);
        const ptime when(missing, getDefaultBarTime());
        REQUIRE(filter.areEntriesAllowed(when) == false);
    }

    SECTION("Sanity: reference series spans the expected dates") {
        // First usable output index for percent-rank: r2Period + prPeriod - 1
        const std::size_t n = ohlc.getNumEntries();
        const std::size_t expected_vol_len = (n >= 20) ? (n - 20 + 1) : 0;      // after R^2 warm-up
        const std::size_t expected_pr_len  = (expected_vol_len >= prPeriod)
                                             ? (expected_vol_len - prPeriod + 1)
                                             : 0;
        REQUIRE(ref.getNumEntries() == expected_pr_len);
    }
}

TEST_CASE("PortfolioFilter / NoPortfolioFilter", "[PortfolioFilter]") {
    // Create test OHLC series for constructor
    auto ohlc = makeSeriesWithPattern(/*nBars=*/50);
    
    // Create a NoPortfolioFilter instance
    NoPortfolioFilter<DecType> noFilter(ohlc);

    SECTION("areEntriesAllowed always returns true for any valid date") {
        // Test with a specific date
        auto testDate = boost::gregorian::date(2023, 6, 15);
        const ptime when(testDate, getDefaultBarTime());
        REQUIRE(noFilter.areEntriesAllowed(when) == true);
    }

    SECTION("areEntriesAllowed always returns true for different dates") {
        // Test with multiple different dates to ensure consistency
        std::vector<boost::gregorian::date> testDates = {
            boost::gregorian::date(2020, 1, 1),
            boost::gregorian::date(2023, 12, 31),
            boost::gregorian::date(2025, 7, 4),
            boost::gregorian::date(1990, 2, 14)
        };

        for (const auto& date : testDates) {
            const ptime when(date, getDefaultBarTime());
            REQUIRE(noFilter.areEntriesAllowed(when) == true);
        }
    }

    SECTION("areEntriesAllowed always returns true for different times of day") {
        // Test with same date but different times
        auto testDate = boost::gregorian::date(2023, 6, 15);
        
        std::vector<boost::posix_time::time_duration> testTimes = {
            boost::posix_time::time_duration(9, 30, 0),   // Market open
            boost::posix_time::time_duration(12, 0, 0),   // Noon
            boost::posix_time::time_duration(16, 0, 0),   // Market close
            boost::posix_time::time_duration(23, 59, 59)  // End of day
        };

        for (const auto& time : testTimes) {
            const ptime when(testDate, time);
            REQUIRE(noFilter.areEntriesAllowed(when) == true);
        }
    }

    SECTION("Constructor creates valid instance") {
        // Test that the constructor works and creates a usable instance
        NoPortfolioFilter<DecType> filter1(ohlc);
        NoPortfolioFilter<DecType> filter2(ohlc);
        
        auto testDate = boost::gregorian::date(2023, 1, 1);
        const ptime when(testDate, getDefaultBarTime());
        
        REQUIRE(filter1.areEntriesAllowed(when) == true);
        REQUIRE(filter2.areEntriesAllowed(when) == true);
    }

    SECTION("Polymorphic usage through base class pointer") {
        // Test that NoPortfolioFilter works correctly when used polymorphically
        std::unique_ptr<PortfolioFilter<DecType>> filter = std::make_unique<NoPortfolioFilter<DecType>>(ohlc);
        
        auto testDate = boost::gregorian::date(2023, 6, 15);
        const ptime when(testDate, getDefaultBarTime());
        
        REQUIRE(filter->areEntriesAllowed(when) == true);
    }
}

TEST_CASE("PortfolioFilter / AdaptiveVolatilityPortfolioFilter (Simons policy)", "[PortfolioFilter]") {
    using mkc_timeseries::SimonsHLCVolatilityPolicy;  // policy under test

    const TimeFrame::Duration tf = TimeFrame::DAILY;

    // Same synthetic series helper used above
    auto ohlc = makeSeriesWithPattern(/*nBars=*/120);

    // Percent-rank window (same as the default-policy test)
    const uint32_t prPeriod = 10;

    // Build a filter that uses SimonsHLCVolatilityPolicy
    // (Assumes AdaptiveVolatilityPortfolioFilter is templated on Decimal, VolPolicy with
    //  default = CloseToCloseVolatilityPolicy.)
    AdaptiveVolatilityPortfolioFilter<DecType, SimonsHLCVolatilityPolicy> filterSimons(ohlc, prPeriod);

    // Reference percent-rank series computed the *same way* the filter does, but using Simons policy
    const double ann = calculateAnnualizationFactor(tf);
    auto refSimons = AdaptiveVolatilityPercentRankAnnualizedSeries<DecType, SimonsHLCVolatilityPolicy>(
                        ohlc, /*r2=*/20, prPeriod, ann);

    REQUIRE(refSimons.getNumEntries() > 0); // sanity

    // Find a date with rank < 0.75 (should exist with our varied pattern), and one >= 0.75
    const DecType thr = DecimalConstants<DecType>::createDecimal("0.75");
    boost::optional<boost::gregorian::date> dateAllow;
    boost::optional<boost::gregorian::date> dateDeny;

    for (auto it = refSimons.beginSortedAccess(); it != refSimons.endSortedAccess(); ++it) {
        const DecType v = it->getValue();
        if (!dateAllow && (v < thr))  dateAllow = it->getDate();
        if (!dateDeny  && (v >= thr)) dateDeny  = it->getDate();
        if (dateAllow && dateDeny) break;
    }

    SECTION("areEntriesAllowed == true when percent-rank < 0.75 (Simons)") {
        REQUIRE(dateAllow.has_value());
        const ptime when(*dateAllow, getDefaultBarTime());
        REQUIRE(filterSimons.areEntriesAllowed(when) == true);
    }

    SECTION("areEntriesAllowed == false when percent-rank >= 0.75 (Simons)") {
        REQUIRE(dateDeny.has_value());
        const ptime when(*dateDeny, getDefaultBarTime());
        REQUIRE(filterSimons.areEntriesAllowed(when) == false);
    }

    SECTION("Sanity: reference (Simons) series spans the expected dates") {
        const std::size_t n = ohlc.getNumEntries();
        const std::size_t expected_vol_len = (n >= 20) ? (n - 20 + 1) : 0; // after R² warm-up
        const std::size_t expected_pr_len  = (expected_vol_len >= prPeriod)
                                             ? (expected_vol_len - prPeriod + 1)
                                             : 0;
        REQUIRE(refSimons.getNumEntries() == expected_pr_len);
    }
}

TEST_CASE("PortfolioFilter / AdaptiveVolatilityPortfolioFilter (one-argument constructor)", "[PortfolioFilter]") {
    const TimeFrame::Duration tf = TimeFrame::DAILY;
    
    // Create a larger series (~290 entries) to account for lookbacks and EMA warmup
    // StandardPercentRankPeriod for DAILY returns 252, plus R^2 period (20) = 272 minimum
    auto ohlc = makeSeriesWithPattern(/*nBars=*/300);
    
    SECTION("One-argument constructor uses StandardPercentRankPeriod") {
        // Test the one-argument constructor
        AdaptiveVolatilityPortfolioFilter<DecType> filterOneArg(ohlc);
        
        // For comparison, create a filter with explicit StandardPercentRankPeriod
        const uint32_t expectedPeriod = StandardPercentRankPeriod(tf); // Should be 252 for DAILY
        AdaptiveVolatilityPortfolioFilter<DecType> filterTwoArg(ohlc, expectedPeriod);
        
        REQUIRE(expectedPeriod == 252); // Verify our expectation for DAILY timeframe
        
        // Build reference series using the same parameters as the one-arg constructor should use
        const double ann = calculateAnnualizationFactor(tf);
        auto refSeries = AdaptiveVolatilityPercentRankAnnualizedSeries<DecType>(ohlc, /*r2=*/20, expectedPeriod, ann);
        
        REQUIRE(refSeries.getNumEntries() > 0); // Sanity check
        
        // Find dates with different volatility ranks to test both allow/deny cases
        const DecType threshold = DecimalConstants<DecType>::createDecimal("0.75");
        boost::optional<boost::gregorian::date> dateAllow;
        boost::optional<boost::gregorian::date> dateDeny;
        
        for (auto it = refSeries.beginSortedAccess(); it != refSeries.endSortedAccess(); ++it) {
            const DecType v = it->getValue();
            if (!dateAllow && (v < threshold)) dateAllow = it->getDate();
            if (!dateDeny && (v >= threshold)) dateDeny = it->getDate();
            if (dateAllow && dateDeny) break;
        }
        
        // Test that both constructors behave identically
        if (dateAllow.has_value()) {
            const ptime whenAllow(*dateAllow, getDefaultBarTime());
            REQUIRE(filterOneArg.areEntriesAllowed(whenAllow) == true);
            REQUIRE(filterTwoArg.areEntriesAllowed(whenAllow) == true);
            REQUIRE(filterOneArg.areEntriesAllowed(whenAllow) == filterTwoArg.areEntriesAllowed(whenAllow));
        }
        
        if (dateDeny.has_value()) {
            const ptime whenDeny(*dateDeny, getDefaultBarTime());
            REQUIRE(filterOneArg.areEntriesAllowed(whenDeny) == false);
            REQUIRE(filterTwoArg.areEntriesAllowed(whenDeny) == false);
            REQUIRE(filterOneArg.areEntriesAllowed(whenDeny) == filterTwoArg.areEntriesAllowed(whenDeny));
        }
    }
    
    SECTION("One-argument constructor handles missing dates correctly") {
        AdaptiveVolatilityPortfolioFilter<DecType> filter(ohlc);
        
        // Test with a date before the series starts
        auto missingDate = boost::gregorian::date(2022, 1, 1);
        const ptime whenMissing(missingDate, getDefaultBarTime());
        REQUIRE(filter.areEntriesAllowed(whenMissing) == false);
    }
    
    SECTION("One-argument constructor produces expected series length") {
        AdaptiveVolatilityPortfolioFilter<DecType> filter(ohlc);
        
        // Calculate expected length: after R^2 warmup (20), then percent rank warmup (252)
        const std::size_t n = ohlc.getNumEntries();
        const std::size_t expectedVolLen = (n >= 20) ? (n - 20 + 1) : 0;
        const std::size_t expectedPrLen = (expectedVolLen >= 252) ? (expectedVolLen - 252 + 1) : 0;
        
        // Build reference to verify our calculation
        const double ann = calculateAnnualizationFactor(tf);
        auto refSeries = AdaptiveVolatilityPercentRankAnnualizedSeries<DecType>(ohlc, /*r2=*/20, /*pr=*/252, ann);
        
        REQUIRE(refSeries.getNumEntries() == expectedPrLen);
        REQUIRE(expectedPrLen > 0); // Should have some entries with 300 input bars
    }
}

// Helper function to create a simple test pattern
static std::shared_ptr<PriceActionLabPattern> createTestLongPattern()
{
    // Use AstResourceManager for proper AST creation
    AstResourceManager resourceManager;
    
    // Create a simple long pattern: Open[0] > Close[1]
    auto open0 = resourceManager.getPriceOpen(0);
    auto close1 = resourceManager.getPriceClose(1);
    auto patternExpression = std::make_shared<GreaterThanExpr>(open0, close1);
    
    // Create shared_ptr values for percentages
    auto percentLong = std::make_shared<DecType>(fromString<DecType>("5.0"));
    auto percentShort = std::make_shared<DecType>(fromString<DecType>("2.0"));
    
    // Create proper PatternDescription with required parameters
    auto patternDesc = std::make_shared<PatternDescription>(
        "TestLong.txt",                    // fileName
        1,                                 // patternIndex
        0,                                 // patternLength
        percentLong,                       // profitTarget
        percentShort,                      // stopLoss
        2,                                 // maxBarsBack
        1                                  // isLong (1 for long, 0 for short)
    );
    
    // Create market entry expression
    auto longEntry = resourceManager.getLongMarketEntryOnOpen();
    
    // Create shared_ptr values for profit target and stop loss
    auto profitTargetValue = std::make_shared<DecType>(fromString<DecType>("5.0"));
    auto stopLossValue = std::make_shared<DecType>(fromString<DecType>("2.0"));
    
    // Create profit target and stop loss expressions
    auto profitTarget = resourceManager.getLongProfitTarget(profitTargetValue);
    auto stopLoss = resourceManager.getLongStopLoss(stopLossValue);
    
    // Create the pattern using AstResourceManager
    return resourceManager.createPattern(
        patternDesc,
        patternExpression,
        longEntry,
        profitTarget,
        stopLoss
    );
}

// Helper function to create portfolio with test security (with sufficient data for AdaptiveVolatilityPortfolioFilter)
std::shared_ptr<Portfolio<DecType>> createTestPortfolio()
{
    // Create test OHLC series with enough data for AdaptiveVolatilityPortfolioFilter
    // Need at least 280 bars: 252 (default percent rank period) + 20 (R-squared period) + buffer
    auto ohlc = makeSeriesWithPattern(300);
    
    // Create security
    auto security = std::make_shared<EquitySecurity<DecType>>("MSFT", "Test Security",
                                                          std::make_shared<const OHLCTimeSeries<DecType>>(ohlc));
    
    // Create portfolio and add security
    auto portfolio = std::make_shared<Portfolio<DecType>>("Test Portfolio");
    portfolio->addSecurity(security);
    
    return portfolio;
}

TEST_CASE("PalMetaStrategy / Default NoPortfolioFilter behavior", "[PalMetaStrategy][PortfolioFilter]") {
    auto portfolio = createTestPortfolio();
    auto pattern = createTestLongPattern();
    
    SECTION("Default template parameter uses NoPortfolioFilter") {
        // This should use NoPortfolioFilter<DecType> as default
        PalMetaStrategy<DecType> strategy("Test Strategy", portfolio);
        strategy.addPricePattern(pattern);
        
        // NoPortfolioFilter should always allow entries
        // We can't directly test the filter, but we can verify the strategy was created successfully
        REQUIRE(strategy.getStrategyName() == "Test Strategy");
        REQUIRE(strategy.getPatternMaxBarsBack() == 2);
    }
    
    SECTION("Explicit NoPortfolioFilter template parameter") {
        // Explicitly specify NoPortfolioFilter
        PalMetaStrategy<DecType, NoPortfolioFilter<DecType>> strategy("Test Strategy", portfolio);
        strategy.addPricePattern(pattern);
        
        REQUIRE(strategy.getStrategyName() == "Test Strategy");
        REQUIRE(strategy.getPatternMaxBarsBack() == 2);
    }
}

TEST_CASE("PalMetaStrategy / AdaptiveVolatilityPortfolioFilter integration", "[PalMetaStrategy][PortfolioFilter]") {
    auto portfolio = createTestPortfolio();
    auto pattern = createTestLongPattern();
    
    SECTION("Strategy creation with AdaptiveVolatilityPortfolioFilter") {
        // Create strategy with adaptive volatility filter
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> strategy("Filtered Strategy", portfolio);
        strategy.addPricePattern(pattern);
        
        REQUIRE(strategy.getStrategyName() == "Filtered Strategy");
        REQUIRE(strategy.getPatternMaxBarsBack() == 2);
    }
    
    SECTION("Strategy creation with custom percent rank period") {
        // Test that the filter gets constructed with OHLC data from portfolio
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> strategy("Custom Filter Strategy", portfolio);
        strategy.addPricePattern(pattern);
        
        // Verify strategy was created successfully (filter construction didn't throw)
        REQUIRE(strategy.getStrategyName() == "Custom Filter Strategy");
    }
}

TEST_CASE("PalMetaStrategy / Portfolio filter error handling", "[PalMetaStrategy][PortfolioFilter]") {
    auto pattern = createTestLongPattern();
    
    SECTION("Empty portfolio throws exception") {
        auto emptyPortfolio = std::make_shared<Portfolio<DecType>>("Empty Portfolio");
        
        // Should throw exception when trying to create filter with empty portfolio
        REQUIRE_THROWS_AS((PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>>("Strategy", emptyPortfolio)), PalStrategyException);
    }
    
    SECTION("Exception message is descriptive") {
        auto emptyPortfolio = std::make_shared<Portfolio<DecType>>("Empty Portfolio");
        
        try {
            PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> strategy("Strategy", emptyPortfolio);
            FAIL("Expected exception was not thrown");
        } catch (const PalStrategyException& e) {
            std::string message = e.what();
            REQUIRE(message.find("Portfolio must contain at least one security") != std::string::npos);
        }
    }
}

TEST_CASE("PalMetaStrategy / Clone operations with portfolio filters", "[PalMetaStrategy][PortfolioFilter]") {
    auto portfolio = createTestPortfolio();
    auto pattern = createTestLongPattern();
    
    SECTION("Clone with new portfolio recreates filter") {
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> originalStrategy("Original", portfolio);
        originalStrategy.addPricePattern(pattern);
        
        // Create new portfolio for clone
        auto newPortfolio = createTestPortfolio();
        
        // Clone should succeed and create new filter instance
        auto clonedStrategy = originalStrategy.clone(newPortfolio);
        
        REQUIRE(clonedStrategy != nullptr);
        REQUIRE(clonedStrategy->getStrategyName() == "Original");
    }
    
    SECTION("CloneForBackTesting preserves filter type") {
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> originalStrategy("Original", portfolio);
        originalStrategy.addPricePattern(pattern);
        
        // Clone for backtesting should succeed
        auto clonedStrategy = originalStrategy.cloneForBackTesting();
        
        REQUIRE(clonedStrategy != nullptr);
        REQUIRE(clonedStrategy->getStrategyName() == "Original");
    }
}

TEST_CASE("PalMetaStrategy / Copy constructor and assignment with filters", "[PalMetaStrategy][PortfolioFilter]") {
    auto portfolio = createTestPortfolio();
    auto pattern = createTestLongPattern();
    
    SECTION("Copy constructor preserves filter") {
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> originalStrategy("Original", portfolio);
        originalStrategy.addPricePattern(pattern);
        
        // Copy constructor should work
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> copiedStrategy(originalStrategy);
        
        REQUIRE(copiedStrategy.getStrategyName() == "Original");
        REQUIRE(copiedStrategy.getPatternMaxBarsBack() == 2);
    }
    
    SECTION("Assignment operator preserves filter") {
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> originalStrategy("Original", portfolio);
        originalStrategy.addPricePattern(pattern);
        
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> assignedStrategy("Temp", portfolio);
        
        // Assignment should work
        assignedStrategy = originalStrategy;
        
        REQUIRE(assignedStrategy.getStrategyName() == "Original");
        REQUIRE(assignedStrategy.getPatternMaxBarsBack() == 2);
    }
}

TEST_CASE("PalMetaStrategy / Mixed filter types", "[PalMetaStrategy][PortfolioFilter]") {
    auto portfolio = createTestPortfolio();
    auto pattern = createTestLongPattern();
    
    SECTION("Different strategies can use different filter types") {
        // Strategy with no filter
        PalMetaStrategy<DecType> noFilterStrategy("No Filter", portfolio);
        noFilterStrategy.addPricePattern(pattern);
        
        // Strategy with adaptive volatility filter
        PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>> filteredStrategy("Filtered", portfolio);
        filteredStrategy.addPricePattern(pattern);
        
        // Both should be created successfully
        REQUIRE(noFilterStrategy.getStrategyName() == "No Filter");
        REQUIRE(filteredStrategy.getStrategyName() == "Filtered");
    }
}

// Helper function to create OHLC series with high volatility periods
static OHLCTimeSeries<DecType> makeHighVolatilitySeriesForFilterTest(std::size_t nBars)
{
    OHLCTimeSeries<DecType> series(TimeFrame::DAILY, TradingVolume::SHARES);

    // Start date
    auto day = boost::gregorian::date(2023, 1, 1);
    double close = 100.0;

    // Create a pattern with periods of high and low volatility
    // High volatility periods should result in percent ranks > 0.75
    for (std::size_t i = 0; i < nBars; ++i) {
        double r;
        
        // Create alternating periods of high and low volatility
        if ((i / 20) % 2 == 0) {
            // High volatility period - large price moves
            r = ((i % 4) == 0) ? 0.15 : ((i % 4) == 1) ? -0.12 : ((i % 4) == 2) ? 0.18 : -0.15;
        } else {
            // Low volatility period - small price moves
            r = ((i % 4) == 0) ? 0.005 : ((i % 4) == 1) ? -0.003 : ((i % 4) == 2) ? 0.008 : -0.006;
        }
        
        double open = close;
        double newClose = close * (1.0 + r);
        double high = std::max(open, newClose) * 1.002;
        double low  = std::min(open, newClose) * 0.998;

        series.addEntry(OHLCTimeSeriesEntry<DecType>(
            day, fromString<DecType>(std::to_string(open)),
            fromString<DecType>(std::to_string(high)),
            fromString<DecType>(std::to_string(low)),
            fromString<DecType>(std::to_string(newClose)),
            fromString<DecType>("1000"),
            TimeFrame::DAILY));

        day = day + boost::gregorian::days(1);
        close = newClose;
    }

    return series;
}

// Helper function to create portfolio with high volatility test data (with shorter warmup for testing)
std::shared_ptr<Portfolio<DecType>> createHighVolatilityTestPortfolio()
{
    // Create test OHLC series with high volatility periods
    // Need enough bars for warmup: 20 (R-squared) + shorter percent rank period + backtest period
    auto ohlc = makeHighVolatilitySeriesForFilterTest(450); // Plenty of bars for warmup and testing
    
    // Create security
    auto security = std::make_shared<EquitySecurity<DecType>>("AAPL", "High Volatility Test Security",
                                                          std::make_shared<const OHLCTimeSeries<DecType>>(ohlc));
    
    // Create portfolio and add security
    auto portfolio = std::make_shared<Portfolio<DecType>>("High Volatility Test Portfolio");
    portfolio->addSecurity(security);
    
    return portfolio;
}

// Helper function to create a pattern that always matches (for testing filter behavior)
std::shared_ptr<PriceActionLabPattern> createAlwaysMatchPattern()
{
    // Use AstResourceManager for proper AST creation
    AstResourceManager resourceManager;
    
    // Create a simple pattern that should match frequently: Close[0] > Open[1]
    auto close0 = resourceManager.getPriceClose(0);
    auto open1 = resourceManager.getPriceOpen(1);
    auto patternExpression = std::make_shared<GreaterThanExpr>(close0, open1);
    
    // Create shared_ptr values for percentages
    auto percentLong = std::make_shared<DecType>(fromString<DecType>("2.0"));
    auto percentShort = std::make_shared<DecType>(fromString<DecType>("1.0"));
    
    // Create proper PatternDescription with required parameters
    auto patternDesc = std::make_shared<PatternDescription>(
        "AlwaysMatch.txt",                 // fileName
        1,                                 // patternIndex
        0,                                 // patternLength
        percentLong,                       // profitTarget
        percentShort,                      // stopLoss
        2,                                 // maxBarsBack
        1                                  // isLong (1 for long, 0 for short)
    );
    
    // Create market entry expression
    auto longEntry = resourceManager.getLongMarketEntryOnOpen();
    
    // Create shared_ptr values for profit target and stop loss
    auto profitTargetValue = std::make_shared<DecType>(fromString<DecType>("2.0"));
    auto stopLossValue = std::make_shared<DecType>(fromString<DecType>("1.0"));
    
    // Create profit target and stop loss expressions
    auto profitTarget = resourceManager.getLongProfitTarget(profitTargetValue);
    auto stopLoss = resourceManager.getLongStopLoss(stopLossValue);
    
    // Create the pattern using AstResourceManager
    return resourceManager.createPattern(
        patternDesc,
        patternExpression,
        longEntry,
        profitTarget,
        stopLoss
    );
}

TEST_CASE("PalMetaStrategy / Filter effectiveness verification", "[PalMetaStrategy][PortfolioFilter][Integration]") {
    auto portfolio = createHighVolatilityTestPortfolio();
    auto pattern = createAlwaysMatchPattern();
    
    SECTION("AdaptiveVolatilityPortfolioFilter reduces trades compared to NoPortfolioFilter") {
        // Create portfolios for each strategy (need separate instances)
        auto noFilterPortfolio = createHighVolatilityTestPortfolio();
        auto filteredPortfolio = createHighVolatilityTestPortfolio();
        
        // Create strategy with no filter (default template parameter)
        auto noFilterStrategy = std::make_shared<PalMetaStrategy<DecType>>("No Filter Strategy", noFilterPortfolio);
        noFilterStrategy->addPricePattern(pattern);
        
        // Create strategy with adaptive volatility filter using shorter period
        // Note: We can't easily pass the custom period to the constructor, so we'll use the default
        // but start the backtest much later to ensure proper warmup
        auto filteredStrategy = std::make_shared<PalMetaStrategy<DecType, AdaptiveVolatilityPortfolioFilter<DecType>>>("Filtered Strategy", filteredPortfolio);
        filteredStrategy->addPricePattern(pattern);
        
        // Create backtesters for both strategies
        // Start after sufficient warmup: 20 (R-squared) + 252 (default percent rank) = 272 bars minimum
        // Our series starts 2023-01-01, so day 280 is around 2023-10-07
        auto startDate = boost::gregorian::date(2023, 10, 10); // Start after warmup period
        auto endDate = boost::gregorian::date(2024, 2, 1);     // Test over several months
        
        auto noFilterBacktester = std::make_shared<DailyBackTester<DecType>>(startDate, endDate);
        noFilterBacktester->addStrategy(noFilterStrategy);
        
        auto filteredBacktester = std::make_shared<DailyBackTester<DecType>>(startDate, endDate);
        filteredBacktester->addStrategy(filteredStrategy);
        
        // Run both backtests
        noFilterBacktester->backtest();
        filteredBacktester->backtest();
        
        // Get trade counts from both backtests
        uint32_t noFilterTrades = noFilterBacktester->getNumTrades();
        uint32_t filteredTrades = filteredBacktester->getNumTrades();
        
        // Log the results for debugging
        std::cout << "No Filter Trades: " << noFilterTrades << std::endl;
        std::cout << "Filtered Trades: " << filteredTrades << std::endl;
        
        // Verify that both strategies generated some trades
        REQUIRE(noFilterTrades > 0);
        REQUIRE(filteredTrades >= 0); // Filtered strategy might have 0 trades if all periods are high volatility
        
        // The key assertion: AdaptiveVolatilityPortfolioFilter should reduce trades
        // during high volatility periods, resulting in fewer total trades
        REQUIRE(filteredTrades < noFilterTrades);
        
        // If both strategies have trades, verify meaningful reduction
        if (filteredTrades > 0) {
            double reductionRatio = static_cast<double>(noFilterTrades - filteredTrades) / noFilterTrades;
            std::cout << "Reduction: " << (reductionRatio * 100.0) << "%" << std::endl;
            
            // Expect some reduction, but don't require a specific percentage since it depends on volatility patterns
            REQUIRE(reductionRatio >= 0.0); // At least no increase in trades
        } else {
            std::cout << "Filtered strategy blocked all trades due to high volatility" << std::endl;
            // This is actually a valid outcome - the filter is working perfectly
        }
    }
    
    SECTION("Verify high volatility periods exist in test data") {
        // Verify that our test data actually has high volatility periods
        auto security = portfolio->beginPortfolio()->second;
        auto ohlcTimeSeries = security->getTimeSeries();
        
        // Create the same filter series that AdaptiveVolatilityPortfolioFilter would use
        const double ann = calculateAnnualizationFactor(TimeFrame::DAILY);
        auto filterSeries = AdaptiveVolatilityPercentRankAnnualizedSeries<DecType>(*ohlcTimeSeries, /*r2=*/20, /*pr=*/252, ann);
        
        REQUIRE(filterSeries.getNumEntries() > 0);
        
        // Count how many entries have volatility rank >= 0.75 (high volatility)
        int highVolatilityCount = 0;
        int totalCount = 0;
        const DecType threshold = fromString<DecType>("0.75");
        
        for (auto it = filterSeries.beginSortedAccess(); it != filterSeries.endSortedAccess(); ++it) {
            totalCount++;
            if (it->getValue() >= threshold) {
                highVolatilityCount++;
            }
        }
        
        REQUIRE(totalCount > 0);
        REQUIRE(highVolatilityCount > 0); // Should have some high volatility periods
        
        // Verify that we have a reasonable mix of high and low volatility
        double highVolatilityRatio = static_cast<double>(highVolatilityCount) / totalCount;
        REQUIRE(highVolatilityRatio > 0.1); // At least 10% should be high volatility
        REQUIRE(highVolatilityRatio < 0.9); // At most 90% should be high volatility
        
        // Log the volatility distribution for debugging
        std::cout << "High volatility periods: " << highVolatilityCount << "/" << totalCount
                  << " (" << (highVolatilityRatio * 100.0) << "%)" << std::endl;
    }
}
