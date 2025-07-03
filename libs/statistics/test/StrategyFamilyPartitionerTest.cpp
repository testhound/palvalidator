#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <vector>
#include <numeric>

#include "number.h"
#include "PalAst.h"
#include "PalStrategy.h"
#include "Portfolio.h"
#include "PALMonteCarloTypes.h"
#include "StrategyFamilyPartitioner.h" // The class under test
#include "TestUtils.h" // For DecimalType definition

using namespace mkc_timeseries;

// Use the DecimalType from TestUtils.h for consistency
using Decimal = DecimalType;

// --- Test Helpers ---

// Helper function to create a pattern from a vector of expressions,
// adapted from PALPatternClassifierTest.cpp to reduce boilerplate.
PALPatternPtr createTestPattern(AstFactory& factory,
                                const std::vector<PatternExpressionPtr>& expressions,
                                bool is_long_pattern,
                                const std::string& profit_target_str,
                                const std::string& stop_loss_str) {

    if (expressions.empty()) {
        return nullptr;
    }

    PatternExpressionPtr final_expr = expressions[0];
    for (size_t i = 1; i < expressions.size(); ++i) {
        final_expr = std::make_shared<AndExpr>(final_expr, expressions[i]);
    }

    auto desc = std::make_shared<PatternDescription>("test.txt", 1, 20240101,
                                                     factory.getDecimalNumber(0), factory.getDecimalNumber(0),
                                                     0, 0);
    // Use the makePalStrategy factory to correctly create Long or Short patterns
    if (is_long_pattern) {
        auto entry = factory.getLongMarketEntryOnOpen();
        auto pt = factory.getLongProfitTarget(factory.getDecimalNumber(const_cast<char*>(profit_target_str.c_str())));
        auto sl = factory.getLongStopLoss(factory.getDecimalNumber(const_cast<char*>(stop_loss_str.c_str())));
        return std::make_shared<PriceActionLabPattern>(desc, final_expr, entry, pt, sl);
    } else {
        auto entry = factory.getShortMarketEntryOnOpen();
        auto pt = factory.getShortProfitTarget(factory.getDecimalNumber(const_cast<char*>(profit_target_str.c_str())));
        auto sl = factory.getShortStopLoss(factory.getDecimalNumber(const_cast<char*>(stop_loss_str.c_str())));
        return std::make_shared<PriceActionLabPattern>(desc, final_expr, entry, pt, sl);
    }
}

// Helper to create a complete StrategyContext for testing.
StrategyContext<Decimal> createTestStrategyContext(const std::string& name,
                                                     PALPatternPtr pattern,
                                                     std::shared_ptr<Portfolio<Decimal>> portfolio)
{
    StrategyContext<Decimal> context;
    // Use the makePalStrategy factory from PalStrategy.h
    context.strategy = makePalStrategy<Decimal>(name, pattern, portfolio);
    context.baselineStat = Decimal("1.0"); // Dummy value, not used by partitioner
    context.count = 1; // Dummy value, not used by partitioner
    return context;
}


// --- Unit Test Implementation ---

TEST_CASE("StrategyFamilyPartitioner operations", "[StrategyFamilyPartitioner]")
{
    AstFactory factory;
    auto portfolio = std::make_shared<Portfolio<Decimal>>("TestPortfolio");

    // --- 1. Define patterns with known classifications ---

    // Long Momentum (Breakout)
    auto longMomentumPattern = createTestPattern(factory,
        { std::make_shared<GreaterThanExpr>(factory.getPriceClose(0), factory.getPriceHigh(10)) },
        true, "3.0", "1.5");

    // Short Momentum (Pullback) - adapted from classifier test
    auto shortMomentumPattern = createTestPattern(factory,
        {
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(1), factory.getPriceHigh(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(0), factory.getPriceHigh(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(2), factory.getPriceLow(0)),
        },
        false, "2.0", "1.0");

    // Long Trend-Following (Continuation)
    auto longTrendPattern = createTestPattern(factory,
        {
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(0), factory.getPriceClose(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(1), factory.getPriceClose(2))
        },
        true, "4.0", "2.0");

    // Short Mean-Reversion (Trend Exhaustion)
    auto shortMeanRevPattern = createTestPattern(factory,
        {
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(2), factory.getPriceClose(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(1), factory.getPriceClose(0))
        },
        false, "0.8", "1.6");

    // Long Unclassified (Ambiguous)
    auto unclassifiedPattern = createTestPattern(factory,
        { std::make_shared<GreaterThanExpr>(factory.getPriceOpen(0), factory.getPriceLow(0)) },
        true, "1.1", "1.0");


    // --- 2. Create the container of strategies for testing ---
    StrategyDataContainer<Decimal> mixedContainer = {
        createTestStrategyContext("LongMomentum1", longMomentumPattern, portfolio),
        createTestStrategyContext("LongMomentum2", longMomentumPattern, portfolio),
        createTestStrategyContext("ShortMomentum1", shortMomentumPattern, portfolio),
        createTestStrategyContext("LongTrend1", longTrendPattern, portfolio),
        createTestStrategyContext("LongTrend2", longTrendPattern, portfolio),
        createTestStrategyContext("LongTrend3", longTrendPattern, portfolio),
        createTestStrategyContext("ShortMeanRev1", shortMeanRevPattern, portfolio),
        createTestStrategyContext("LongUnclassified1", unclassifiedPattern, portfolio)
    };


    // --- 3. Test Scenarios ---

    SECTION("Empty strategy list")
    {
        StrategyDataContainer<Decimal> emptyContainer;
        StrategyFamilyPartitioner<Decimal> partitioner(emptyContainer);

        REQUIRE(partitioner.getTotalStrategyCount() == 0);
        REQUIRE(partitioner.getNumberOfFamilies() == 0);
        REQUIRE(partitioner.getStatistics().empty());
        REQUIRE(partitioner.begin() == partitioner.end());
    }

    SECTION("Partitioning by Category only (default behavior)")
    {
        // Construct without the second argument, testing the default path
        StrategyFamilyPartitioner<Decimal> partitioner(mixedContainer);

        // --- A. Check top-level counts ---
        REQUIRE(partitioner.getTotalStrategyCount() == 8);
        REQUIRE(partitioner.getNumberOfFamilies() == 5); // LongMom, ShortMom, LongTrend, ShortMR, LongUnc

        // --- B. Check individual family counts using getFamilyCount ---
        // FIX: Initialize StrategyFamilyKey with all three members
        StrategyFamilyKey longMomKey        = {StrategyCategory::MOMENTUM, StrategySubType::NONE, true};
        StrategyFamilyKey shortMomKey       = {StrategyCategory::MOMENTUM, StrategySubType::NONE, false};
        StrategyFamilyKey longTrendKey      = {StrategyCategory::TREND_FOLLOWING, StrategySubType::NONE, true};
        StrategyFamilyKey shortMeanRevKey   = {StrategyCategory::MEAN_REVERSION, StrategySubType::NONE, false};
        StrategyFamilyKey longUnclassifiedKey = {StrategyCategory::UNCLASSIFIED, StrategySubType::NONE, true};
        StrategyFamilyKey nonExistentKey    = {StrategyCategory::TREND_FOLLOWING, StrategySubType::NONE, false};

        REQUIRE(partitioner.getFamilyCount(longMomKey) == 2);
        REQUIRE(partitioner.getFamilyCount(shortMomKey) == 1);
        REQUIRE(partitioner.getFamilyCount(longTrendKey) == 3);
        REQUIRE(partitioner.getFamilyCount(shortMeanRevKey) == 1);
        REQUIRE(partitioner.getFamilyCount(longUnclassifiedKey) == 1);
        REQUIRE(partitioner.getFamilyCount(nonExistentKey) == 0);

        // --- C. Check iterator functionality ---
        int familyCountFromIterator = 0;
        for ([[maybe_unused]] const auto& familyPair : partitioner) {
            familyCountFromIterator++;
        }
        REQUIRE(familyCountFromIterator == 5);

        // --- D. Check statistics calculation ---
        auto stats = partitioner.getStatistics();
        REQUIRE(stats.size() == 5);

        double totalPercentage = 0.0;
        for (const auto& familyStat : stats) {
            totalPercentage += familyStat.percentageOfTotal;
            if (familyStat.key.category == StrategyCategory::MOMENTUM && familyStat.key.isLong) {
                REQUIRE(familyStat.count == 2);
                REQUIRE_THAT(familyStat.percentageOfTotal, Catch::Matchers::WithinAbs(25.0, 0.01)); // 2/8
            } else if (familyStat.key.category == StrategyCategory::TREND_FOLLOWING && familyStat.key.isLong) {
                REQUIRE(familyStat.count == 3);
                REQUIRE_THAT(familyStat.percentageOfTotal, Catch::Matchers::WithinAbs(37.5, 0.01)); // 3/8
            }
        }
        REQUIRE_THAT(totalPercentage, Catch::Matchers::WithinAbs(100.0, 0.01));
    }

    SECTION("Partitioning by detailed Category and Sub-Type")
    {
        // Construct with partitionBySubType = true to test the new path
        StrategyFamilyPartitioner<Decimal> partitioner(mixedContainer, true);

        // The number of families should be the same in this specific test case,
        // but the keys will be more specific.
        REQUIRE(partitioner.getTotalStrategyCount() == 8);
        REQUIRE(partitioner.getNumberOfFamilies() == 5);

        // Define the more specific keys including the sub-type
        StrategyFamilyKey longMomBreakoutKey = {StrategyCategory::MOMENTUM, StrategySubType::BREAKOUT, true};
        StrategyFamilyKey shortMomPullbackKey = {StrategyCategory::MOMENTUM, StrategySubType::PULLBACK, false};
        StrategyFamilyKey longTrendContinuationKey = {StrategyCategory::TREND_FOLLOWING, StrategySubType::CONTINUATION, true};
        StrategyFamilyKey shortMRExhaustionKey = {StrategyCategory::MEAN_REVERSION, StrategySubType::TREND_EXHAUSTION, false};
        StrategyFamilyKey longUnclassifiedAmbiguousKey = {StrategyCategory::UNCLASSIFIED, StrategySubType::AMBIGUOUS, true};

        // Check counts for the new, granular families
        REQUIRE(partitioner.getFamilyCount(longMomBreakoutKey) == 2);
        REQUIRE(partitioner.getFamilyCount(shortMomPullbackKey) == 1);
        REQUIRE(partitioner.getFamilyCount(longTrendContinuationKey) == 3);
        REQUIRE(partitioner.getFamilyCount(shortMRExhaustionKey) == 1);
        REQUIRE(partitioner.getFamilyCount(longUnclassifiedAmbiguousKey) == 1);

        // Ensure that a key with the wrong sub-type finds nothing
        StrategyFamilyKey wrongSubtypeKey = {StrategyCategory::MOMENTUM, StrategySubType::CONTINUATION, true};
        REQUIRE(partitioner.getFamilyCount(wrongSubtypeKey) == 0);
    }
}
