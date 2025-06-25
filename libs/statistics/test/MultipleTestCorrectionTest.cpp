#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>
#include <memory>
#include <tuple>
#include <iostream>
#include <cmath>    // For std::sqrt used in DecimalConstants.h -> DecimalSqrtConstants
#include <algorithm> // For std::sort, std::count_if
#include <numeric>   // For std::accumulate (potentially useful, though manual loop used here)
#include <boost/date_time/posix_time/posix_time.hpp> // Needed for ptime, time_duration

// --- Required Headers from your project ---
// Adjust paths as necessary
#include "TestUtils.h"                 // Provides createDecimal and DecimalType
#include "PalStrategy.h"
#include "MultipleTestingCorrection.h"
#include "DecimalConstants.h"
#include "PalAst.h"
#include "BacktesterStrategy.h"
#include "Portfolio.h"
#include "Security.h"
#include "TimeSeries.h"
#include "TimeFrame.h"
#include "TradingVolume.h"          // <<< Included as requested/dependency
#include "number.h"                 // Needed for num::fromString used by DecimalConstants.h
#include "BoostDateHelper.h"

// --- Use the project's namespace ---
using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;

// Note: DecimalType is now defined in TestUtils.h as dec::decimal
// Note: createDecimal function is provided by TestUtils.h

// --- Helper Functions adapted from PalStrategyTest.cpp ---
// (Helper functions createDescription, createLongOnOpen, etc. remain unchanged)
std::unique_ptr<PatternDescription>
createDescription (const std::string& fileName, unsigned int index, unsigned long indexDate,
                   const DecimalType& percLong, const DecimalType& percShort,
                   unsigned int numTrades, unsigned int consecutiveLosses)
{
    auto pL = std::make_shared<DecimalType>(percLong);
    auto pS = std::make_shared<DecimalType>(percShort);
    return std::make_unique<PatternDescription>(const_cast<char*>(fileName.c_str()), index, indexDate, pL, pS, numTrades, consecutiveLosses);
}

std::unique_ptr<LongMarketEntryOnOpen> createLongOnOpen() {
    return std::make_unique<LongMarketEntryOnOpen>();
}

std::unique_ptr<ShortMarketEntryOnOpen> createShortOnOpen() {
    return std::make_unique<ShortMarketEntryOnOpen>();
}

std::unique_ptr<LongSideProfitTargetInPercent> createLongProfitTarget(const DecimalType& targetPct) {
    return std::make_unique<LongSideProfitTargetInPercent>(std::make_shared<DecimalType>(targetPct));
}

std::unique_ptr<LongSideStopLossInPercent> createLongStopLoss(const DecimalType& stopPct) {
    return std::make_unique<LongSideStopLossInPercent>(std::make_shared<DecimalType>(stopPct));
}

std::unique_ptr<ShortSideProfitTargetInPercent> createShortProfitTarget(const DecimalType& targetPct) {
    return std::make_unique<ShortSideProfitTargetInPercent>(std::make_shared<DecimalType>(targetPct));
}

std::unique_ptr<ShortSideStopLossInPercent> createShortStopLoss(const DecimalType& stopPct) {
    return std::make_unique<ShortSideStopLossInPercent>(std::make_shared<DecimalType>(stopPct));
}


// --- Pattern Creation Helpers (returning shared_ptr) ---
// (Helper functions createShortPattern1, createLongPattern1 remain unchanged)
std::shared_ptr<PriceActionLabPattern> createShortPattern1() {
    auto desc = createDescription("C2_122AR.txt", 39, 20111017,
                                  createDecimal("90.00"), createDecimal("10.00"), 21, 2);

    // Construct individual PriceBarHigh references.
    auto high4 = std::make_unique<PriceBarHigh>(4);
    auto high5 = std::make_unique<PriceBarHigh>(5);
    auto high3 = std::make_unique<PriceBarHigh>(3);
    auto high0 = std::make_unique<PriceBarHigh>(0);
    auto high1 = std::make_unique<PriceBarHigh>(1);
    auto high2 = std::make_unique<PriceBarHigh>(2);

    // Create GreaterThanExpr nodes, passing raw pointers.
    auto shortgt1 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(high4.release(), high5.release()));
    auto high5_2 = std::make_unique<PriceBarHigh>(5);
    auto high3_2 = std::make_unique<PriceBarHigh>(3);
    auto shortgt2 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(high5_2.release(), high3_2.release()));
    auto high3_3 = std::make_unique<PriceBarHigh>(3);
    auto high0_2 = std::make_unique<PriceBarHigh>(0);
    auto shortgt3 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(high3_3.release(), high0_2.release()));
    auto high0_3 = std::make_unique<PriceBarHigh>(0);
    auto high1_2 = std::make_unique<PriceBarHigh>(1);
    auto shortgt4 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(high0_3.release(), high1_2.release()));
    auto high1_3 = std::make_unique<PriceBarHigh>(1);
    auto high2_2 = std::make_unique<PriceBarHigh>(2);
    auto shortgt5 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(high1_3.release(), high2_2.release()));

    // Combine with AndExpr nodes.
    auto shortand1 = std::unique_ptr<AndExpr>(new AndExpr(shortgt1.release(), shortgt2.release()));
    auto shortand2 = std::unique_ptr<AndExpr>(new AndExpr(shortgt3.release(), shortgt4.release()));
    auto shortand3 = std::unique_ptr<AndExpr>(new AndExpr(shortgt5.release(), shortand2.release()));
    auto shortPatternExpr = std::unique_ptr<AndExpr>(new AndExpr(shortand1.release(), shortand3.release()));

    auto entry = createShortOnOpen();
    auto target = createShortProfitTarget(createDecimal("1.34"));
    auto stop = createShortStopLoss(createDecimal("1.28"));

    return std::make_shared<PriceActionLabPattern>(desc.release(), shortPatternExpr.release(),
                                                    std::shared_ptr<MarketEntryExpression>(entry.release()),
                                                    std::shared_ptr<ProfitTargetInPercentExpression>(target.release()),
                                                    std::shared_ptr<StopLossInPercentExpression>(stop.release()));
}


std::shared_ptr<PriceActionLabPattern> createLongPattern1() {
    auto desc = createDescription("C2_122AR.txt", 39, 20131217,
                                  createDecimal("90.00"), createDecimal("10.00"), 21, 2);

    auto open5 = std::make_unique<PriceBarOpen>(5);
    auto close5 = std::make_unique<PriceBarClose>(5);
    auto gt1 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(open5.release(), close5.release()));
    auto close5_1 = std::make_unique<PriceBarClose>(5);
    auto close6_1 = std::make_unique<PriceBarClose>(6);
    auto gt2 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(close5_1.release(), close6_1.release()));
    auto and1 = std::unique_ptr<AndExpr>(new AndExpr(gt1.release(), gt2.release()));
    auto close6_2 = std::make_unique<PriceBarClose>(6);
    auto open6 = std::make_unique<PriceBarOpen>(6);
    auto gt3 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(close6_2.release(), open6.release()));
    auto open6_1 = std::make_unique<PriceBarOpen>(6);
    auto close8 = std::make_unique<PriceBarClose>(8);
    auto gt4 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(open6_1.release(), close8.release()));
    auto and2 = std::unique_ptr<AndExpr>(new AndExpr(gt3.release(), gt4.release()));
    auto close8_1 = std::make_unique<PriceBarClose>(8);
    auto open8 = std::make_unique<PriceBarOpen>(8);
    auto gt5 = std::unique_ptr<GreaterThanExpr>(new GreaterThanExpr(close8_1.release(), open8.release()));
    auto and3 = std::unique_ptr<AndExpr>(new AndExpr(and2.release(), gt5.release()));
    auto longPatternExpr = std::unique_ptr<AndExpr>(new AndExpr(and1.release(), and3.release()));

    auto entry = createLongOnOpen();
    auto target = createLongProfitTarget(createDecimal("2.56"));
    auto stop = createLongStopLoss(createDecimal("1.28"));

    return std::make_shared<PriceActionLabPattern>(desc.release(), longPatternExpr.release(),
                                                    std::shared_ptr<MarketEntryExpression>(entry.release()),
                                                    std::shared_ptr<ProfitTargetInPercentExpression>(target.release()),
                                                    std::shared_ptr<StopLossInPercentExpression>(stop.release()));
}

// --- Portfolio / Security Setup Helpers ---
// (Helper functions createDummySecurity, createDummyPortfolio, createDummyPalStrategy remain unchanged)
std::shared_ptr<Security<DecimalType>> createDummySecurity(const std::string& symbol = "AAPL") {
    auto dummyTimeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    date dt(2024, Jan, 1);

    OHLCTimeSeriesEntry<DecimalType> entry(dt,
                             createDecimal("100.0"), createDecimal("105.0"),
                             createDecimal("95.0"), createDecimal("101.0"),
                             createDecimal("1000"), TimeFrame::DAILY);
    dummyTimeSeries->addEntry(entry);

    return std::make_shared<EquitySecurity<DecimalType>>(symbol, "Apple Computer", dummyTimeSeries);
}

std::shared_ptr<Portfolio<DecimalType>> createDummyPortfolio(const std::string& name = "DummyPortfolio") {
    auto portfolio = std::make_shared<Portfolio<DecimalType>>(name);
    portfolio->addSecurity(createDummySecurity());
    return portfolio;
}

std::shared_ptr<PalStrategy<DecimalType>> createDummyPalStrategy(
    const std::string& name,
    std::shared_ptr<PriceActionLabPattern> pattern,
    std::shared_ptr<Portfolio<DecimalType>> portfolio)
{
    if (!pattern) {
        throw std::runtime_error("Attempted to create PalStrategy with null pattern in test setup");
    }
    if (!portfolio) {
         throw std::runtime_error("Attempted to create PalStrategy with null portfolio in test setup");
    }
    // Using PalLongStrategy as a concrete example, adjust if needed
    return std::make_shared<PalLongStrategy<DecimalType>>(name, pattern, portfolio);
}


// *** HELPER FUNCTION WITH FIX ***
// Calculates initial empirical p-value based on test statistic and null distribution
// In MultipleTestCorrectionTest.cpp

// *** REVISED HELPER FUNCTION ***
// This version uses 'if constexpr' to handle the different 'addStrategy' signatures
// for different correction policies.
template <typename CorrectionClass>
void addStrategyWithEmpiricalPValue(
    CorrectionClass& correction,
    const DecimalType& testStat,
    const std::vector<DecimalType>& nullDistribution,
    const std::shared_ptr<PalStrategy<DecimalType>>& strategy)
{
    if (nullDistribution.empty()) {
         throw std::runtime_error("Synthetic null distribution is empty in test setup");
    }

    size_t countGreaterEqual = std::count_if(nullDistribution.begin(), nullDistribution.end(),
                                             [&](const DecimalType& nullVal) {
                                                 return nullVal >= testStat;
                                             });
    
    // The raw p-value is calculated the same way for all tests.
    DecimalType empiricalPValue = DecimalType(static_cast<long>(countGreaterEqual)) / DecimalType(static_cast<long>(nullDistribution.size()));

    // Use `if constexpr` to call the correct overload based on the policy type
    if constexpr (std::is_same_v<CorrectionClass, mkc_timeseries::RomanoWolfStepdownCorrection<DecimalType>>)
    {
        // This is our new, corrected class. It expects the 3-element FullResultType tuple.
        // We pass the `testStat` as both the max_permuted_stat (index 1) and baseline_stat (index 2)
        // for the purpose of this unit test.
        correction.addStrategy({empiricalPValue, testStat, testStat}, strategy);
    }
    else
    {
        // This handles the older classes (HolmRomanoWolfCorrection, RomanoWolfStepdownCorrection2)
        // that expect a 2-element tuple of {p-value, test-stat}.
        correction.addStrategy({empiricalPValue, testStat}, strategy);
    }
}

// Helper to check monotonicity of adjusted p-values
template <typename CorrectionClass>
void checkAdjustedPValueMonotonicity(const CorrectionClass& correction) {
    DecimalType previousP = createDecimal("0.0");
    // Note: Accessing the container and sorting depends on the CorrectionClass implementation details.
    // This assumes the class provides sorted iterators *after* correction.
    // We might need to get the internal container, re-sort by adjusted p-value, then check.
    // Let's adapt based on HolmRomanoWolf/RomanoWolf structure which modifies the container in-place.
    // We need to access the container *after* correction and check the adjusted p-values (first element of tuple).

    // Get the internal container (assuming TestStatisticStrategyImplementation structure)
    // *** Requires adding 'const TestStatisticContainer& getInternalContainer() const' to the Correction Policies ***
    const auto& internalContainer = correction.getInternalContainer();

    // // Create a copy to sort by adjusted p-value without modifying the original order if it matters elsewhere
    auto sortedByAdjustedP = internalContainer;
    std::sort(sortedByAdjustedP.begin(), sortedByAdjustedP.end(), [](const auto& a, const auto& b){
         return std::get<0>(a) < std::get<0>(b); // Sort by adjusted p-value (index 0)
     });


    for (const auto& entry : sortedByAdjustedP) {
         DecimalType adjustedP = std::get(entry);
          REQUIRE(adjustedP >= previousP);
          previousP = adjustedP;
    }
}


std::shared_ptr<PriceActionLabPattern> sharedPattern() {
    // Cache the pattern creation to avoid redundant work
    static std::shared_ptr<PriceActionLabPattern> pattern = createLongPattern1();
    return pattern;
}

std::shared_ptr<Portfolio<DecimalType>> sharedPortfolio() {
    // Cache the portfolio creation
    static std::shared_ptr<Portfolio<DecimalType>> portfolio = createDummyPortfolio();
    return portfolio;
}

// Helper to inject the synthetic null - unchanged
template <typename CorrectionClass>
void injectSyntheticNull(CorrectionClass& correction, const std::vector<DecimalType>& syntheticNull) {
    correction.setSyntheticNullDistribution(syntheticNull);
}

// --- Test Cases ---
// (Test Cases remain unchanged from the previous version, using addStrategyWithEmpiricalPValue)

TEST_CASE("UnadjustedPValueStrategySelection", "[MultipleTestingCorrection]") {
    auto portfolio = sharedPortfolio();
    auto pattern = sharedPattern();

    auto strategy1 = createDummyPalStrategy("Strategy1_Unadj", pattern, portfolio);
    auto strategy2 = createDummyPalStrategy("Strategy2_Unadj", pattern, portfolio);
    auto strategy3 = createDummyPalStrategy("Strategy3_Unadj", pattern, portfolio);

    const DecimalType SIGNIFICANCE_THRESHOLD = DecimalConstants<DecimalType>::SignificantPValue;
    auto D = [](const char* s) { return createDecimal(s); };

    SECTION("Mixed significant and non-significant p-values") {
        UnadjustedPValueStrategySelection<DecimalType> correction;
        correction.addStrategy(D("0.01"), strategy1);
        correction.addStrategy(D("0.10"), strategy2);
        correction.addStrategy(D("0.04"), strategy3);

        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 2);
    }

    SECTION("All p-values non-significant") {
        UnadjustedPValueStrategySelection<DecimalType> correction;
        correction.addStrategy(SIGNIFICANCE_THRESHOLD + D("0.01"), strategy1);
        correction.addStrategy(SIGNIFICANCE_THRESHOLD + D("0.05"), strategy2);

        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 0);
    }

    SECTION("All p-values significant") {
        UnadjustedPValueStrategySelection<DecimalType> correction;
        correction.addStrategy(SIGNIFICANCE_THRESHOLD - D("0.01"), strategy1);
        correction.addStrategy(SIGNIFICANCE_THRESHOLD - D("0.02"), strategy2);

        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 2);
    }

    SECTION("No strategies") {
        UnadjustedPValueStrategySelection<DecimalType> correction;
        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 0);
    }
}

// Shared synthetic null distribution for empirical tests - unchanged
//std::vector<DecimalType> syntheticNull = {
//  createDecimal("0.1"),
//  createDecimal("0.5"),
//  createDecimal("1.5"),
//  createDecimal("2.5"),
//  createDecimal("3.5")
//};

std::vector<DecimalType> syntheticNull = {
  createDecimal("0.1"),
  createDecimal("0.2"),
  createDecimal("0.3"),
  createDecimal("0.4"),
  createDecimal("0.5")
};

TEST_CASE("HolmRomanoWolfCorrection - Synthetic Null", "[MultipleTestingCorrection]") {
    auto portfolio = sharedPortfolio();
    auto pattern = sharedPattern();
    auto D = [](const char* s) { return createDecimal(s); };

    SECTION("High, medium, low test stats") {
        HolmRomanoWolfCorrection<DecimalType> correction;
        addStrategyWithEmpiricalPValue(correction, D("10.0"), syntheticNull, createDummyPalStrategy("HRW_High", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, D("1.0"), syntheticNull, createDummyPalStrategy("HRW_Med", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, D("0.1"), syntheticNull, createDummyPalStrategy("HRW_Low", pattern, portfolio));

        injectSyntheticNull(correction, syntheticNull);
        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 2);
       // checkAdjustedPValueMonotonicity(correction);
    }

    SECTION("Identical weak stats (no survivors)") {
        HolmRomanoWolfCorrection<DecimalType> correction;
        auto weakStat = D("0.1");
        addStrategyWithEmpiricalPValue(correction, weakStat, syntheticNull, createDummyPalStrategy("HRW_Weak1", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, weakStat, syntheticNull, createDummyPalStrategy("HRW_Weak2", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, weakStat, syntheticNull, createDummyPalStrategy("HRW_Weak3", pattern, portfolio));

        injectSyntheticNull(correction, syntheticNull);
        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 0);
       // checkAdjustedPValueMonotonicity(correction);
    }

    SECTION("Stress test: 100 strategies") {
        HolmRomanoWolfCorrection<DecimalType> correction;
        auto strongStat = D("10.0");
        auto mediumStat = D("0.5");

        for(int i = 0; i < 10; ++i) {
             addStrategyWithEmpiricalPValue(correction, strongStat, syntheticNull, createDummyPalStrategy("HRW_Stress_Strong_" + std::to_string(i), pattern, portfolio));
        }
        for(int i = 0; i < 90; ++i) {
             addStrategyWithEmpiricalPValue(correction, mediumStat, syntheticNull, createDummyPalStrategy("HRW_Stress_Med_" + std::to_string(i), pattern, portfolio));
        }

        injectSyntheticNull(correction, syntheticNull);
        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 10);
       // checkAdjustedPValueMonotonicity(correction);
    }

    SECTION("No strategies") {
        HolmRomanoWolfCorrection<DecimalType> correction;
        injectSyntheticNull(correction, syntheticNull);
 	REQUIRE_THROWS_AS(correction.correctForMultipleTests(), std::runtime_error);
        //REQUIRE(correction.getNumSurvivingStrategies() == 0);
    }
}

TEST_CASE("RomanoWolfStepdownCorrection - Synthetic Null", "[MultipleTestingCorrection]") {
    auto portfolio = sharedPortfolio();
    auto pattern = sharedPattern();
    auto D = [](const char* s) { return createDecimal(s); };

    SECTION("High, medium, low test stats") {
        RomanoWolfStepdownCorrection<DecimalType> correction;
        addStrategyWithEmpiricalPValue(correction, D("10.0"), syntheticNull, createDummyPalStrategy("RW_High", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, D("1.0"), syntheticNull, createDummyPalStrategy("RW_Med", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, D("0.1"), syntheticNull, createDummyPalStrategy("RW_Low", pattern, portfolio));

        injectSyntheticNull(correction, syntheticNull);
        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 2);
        // checkAdjustedPValueMonotonicity(correction);
    }

    SECTION("Identical weak stats (no survivors)") {
        RomanoWolfStepdownCorrection<DecimalType> correction;
        auto weakStat = D("0.1");
        addStrategyWithEmpiricalPValue(correction, weakStat, syntheticNull, createDummyPalStrategy("RW_Weak1", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, weakStat, syntheticNull, createDummyPalStrategy("RW_Weak2", pattern, portfolio));
        addStrategyWithEmpiricalPValue(correction, weakStat, syntheticNull, createDummyPalStrategy("RW_Weak3", pattern, portfolio));

        injectSyntheticNull(correction, syntheticNull);
        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 0);
       // checkAdjustedPValueMonotonicity(correction);
    }

    SECTION("Stress test: 100 strategies") {
        RomanoWolfStepdownCorrection<DecimalType> correction;
        auto strongStat = D("10.0");
        auto mediumStat = D("0.5");

        for(int i = 0; i < 10; ++i) {
             addStrategyWithEmpiricalPValue(correction, strongStat, syntheticNull, createDummyPalStrategy("RW_Stress_Strong_" + std::to_string(i), pattern, portfolio));
        }
        for(int i = 0; i < 90; ++i) {
             addStrategyWithEmpiricalPValue(correction, mediumStat, syntheticNull, createDummyPalStrategy("RW_Stress_Med_" + std::to_string(i), pattern, portfolio));
        }

        injectSyntheticNull(correction, syntheticNull);
        correction.correctForMultipleTests();
        REQUIRE(correction.getNumSurvivingStrategies() == 10);
        // checkAdjustedPValueMonotonicity(correction);
    }

    SECTION("No strategies") {
        RomanoWolfStepdownCorrection<DecimalType> correction;
        injectSyntheticNull(correction, syntheticNull);
	REQUIRE_THROWS_AS(correction.correctForMultipleTests(), std::runtime_error);
        //REQUIRE(correction.getNumSurvivingStrategies() == 0);
    }
}
