// ============================================================================
// UNIT TESTS: Bootstrap Robustness Analyzer Classes
// ============================================================================
// Comprehensive Catch2 tests for:
// - BootstrapConfig
// - StrategyBootstrapResult
// - RobustnessAnalysisResult
// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "BootstrapRobustnessAnalyzer.h"
#include "PalStrategy.h"
#include "number.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace palvalidator::analysis;
using namespace mkc_timeseries;

using Num = num::DefaultNumber;
using DecimalType = decimal7;  // Type alias for decimal type used in patterns
using D = Num;  // Type alias used in template parameters

// ============================================================================
// TEST FIXTURES AND HELPERS
// ============================================================================

namespace {


static std::shared_ptr<LongMarketEntryOnOpen>
createLongOnOpen()
{
  return std::make_shared<LongMarketEntryOnOpen>();
}

static std::shared_ptr<LongSideProfitTargetInPercent>
createLongProfitTarget(const std::string& targetPct)
{
  return std::make_shared<LongSideProfitTargetInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

static std::shared_ptr<LongSideStopLossInPercent>
createLongStopLoss(const std::string& targetPct)
{
  return std::make_shared<LongSideStopLossInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}


static std::shared_ptr<PriceActionLabPattern>
createLongPattern1()
{
  // Create description using shared_ptr
  auto percentLong = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc = std::make_shared<PatternDescription>("C2_122AR.txt", 39, 20131217,
                                                   percentLong, percentShort, 21, 2);

  auto open5 = std::make_shared<PriceBarOpen>(5);
  auto close5 = std::make_shared<PriceBarClose>(5);
  auto gt1 = std::make_shared<GreaterThanExpr>(open5, close5);

  auto close6 = std::make_shared<PriceBarClose>(6);
  auto gt2 = std::make_shared<GreaterThanExpr>(close5, close6);

  // OPEN OF 5 BARS AGO > CLOSE OF 5 BARS AGO
  // AND CLOSE OF 5 BARS AGO > CLOSE OF 6 BARS AGO
  auto and1 = std::make_shared<AndExpr>(gt1, gt2);

  auto open6 = std::make_shared<PriceBarOpen>(6);
  auto gt3 = std::make_shared<GreaterThanExpr>(close6, open6);

  auto close8 = std::make_shared<PriceBarClose>(8);
  auto gt4 = std::make_shared<GreaterThanExpr>(open6, close8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  auto and2 = std::make_shared<AndExpr>(gt3, gt4);

  auto open8 = std::make_shared<PriceBarOpen>(8);
  auto gt5 = std::make_shared<GreaterThanExpr>(close8, open8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  // CLOSE OF 8 BARS AGO > OPEN OF 8 BARS AGO

  auto and3 = std::make_shared<AndExpr>(and2, gt5);
  auto longPattern1 = std::make_shared<AndExpr>(and1, and3);
  auto entry = createLongOnOpen();
  auto target = createLongProfitTarget("2.56");
  auto stop = createLongStopLoss("1.28");

  // 2.56 profit target in points = 93.81
  return std::make_shared<PriceActionLabPattern>(desc, longPattern1,
                                                 entry,
                                                 target,
                                                 stop);
}

// Helper: Create a mock strategy for testing
std::shared_ptr<PalStrategy<Num>> createMockStrategy(const std::string& name) {
  auto pattern1 = createLongPattern1();
    
  auto portfolio1 = std::make_shared<mkc_timeseries::Portfolio<D>>("P1");
    
  mkc_timeseries::StrategyOptions options(false, 0, 0);
    
  auto strategy1 = mkc_timeseries::makePalStrategy<D>(name.c_str(), pattern1, portfolio1, options);

  return strategy1;
}

} // anonymous namespace

// ============================================================================
// BOOTSTRAPCONFIG TESTS
// ============================================================================

TEST_CASE("BootstrapConfig: Basic construction and getters", "[BootstrapConfig]")
{
    SECTION("Construct with all parameters")
    {
        BootstrapConfig config(10, 0.95, false, true);
        
        REQUIRE(config.getNumSeeds() == 10);
        REQUIRE(config.getMinPassRate() == Catch::Approx(0.95));
        REQUIRE(config.getRequirePerfect() == false);
        REQUIRE(config.getReportDetailedResults() == true);
    }
    
    SECTION("Construct with default optional parameters")
    {
        BootstrapConfig config(5, 0.80);
        
        REQUIRE(config.getNumSeeds() == 5);
        REQUIRE(config.getMinPassRate() == Catch::Approx(0.80));
        REQUIRE(config.getRequirePerfect() == false);  // default
        REQUIRE(config.getReportDetailedResults() == false);  // default
    }
}

TEST_CASE("BootstrapConfig: Edge cases and validation", "[BootstrapConfig]")
{
    SECTION("Minimum number of seeds (1)")
    {
        BootstrapConfig config(1, 0.95);
        REQUIRE(config.getNumSeeds() == 1);
    }
    
    SECTION("Large number of seeds")
    {
        BootstrapConfig config(1000, 0.95);
        REQUIRE(config.getNumSeeds() == 1000);
    }
    
    SECTION("Pass rate at boundary values")
    {
        BootstrapConfig config1(10, 0.0);
        REQUIRE(config1.getMinPassRate() == Catch::Approx(0.0));
        
        BootstrapConfig config2(10, 1.0);
        REQUIRE(config2.getMinPassRate() == Catch::Approx(1.0));
    }
    
    SECTION("RequirePerfect flag overrides minPassRate semantically")
    {
        // When requirePerfect is true, semantically we expect 100% pass rate
        // The config stores both, but the analyzer should use 1.0 when requirePerfect is true
        BootstrapConfig config(10, 0.80, true);
        
        REQUIRE(config.getRequirePerfect() == true);
        REQUIRE(config.getMinPassRate() == Catch::Approx(0.80));  // Still stored
        // Analyzer should use 1.0 when requirePerfect is true
    }
}

TEST_CASE("BootstrapConfig: Immutability", "[BootstrapConfig]")
{
    BootstrapConfig config(10, 0.95, false, true);
    
    // Get values
    auto numSeeds = config.getNumSeeds();
    auto minPassRate = config.getMinPassRate();
    auto requirePerfect = config.getRequirePerfect();
    auto reportDetailed = config.getReportDetailedResults();
    
    // Call getters again - should return same values (object is immutable)
    REQUIRE(config.getNumSeeds() == numSeeds);
    REQUIRE(config.getMinPassRate() == Catch::Approx(minPassRate));
    REQUIRE(config.getRequirePerfect() == requirePerfect);
    REQUIRE(config.getReportDetailedResults() == reportDetailed);
}

TEST_CASE("BootstrapConfig: Const correctness", "[BootstrapConfig]")
{
    const BootstrapConfig config(10, 0.95, false, true);
    
    // All getters should work on const object
    REQUIRE(config.getNumSeeds() == 10);
    REQUIRE(config.getMinPassRate() == Catch::Approx(0.95));
    REQUIRE(config.getRequirePerfect() == false);
    REQUIRE(config.getReportDetailedResults() == true);
}

TEST_CASE("BootstrapConfig: Copy construction", "[BootstrapConfig]")
{
    BootstrapConfig original(15, 0.90, true, false);
    BootstrapConfig copy = original;
    
    REQUIRE(copy.getNumSeeds() == original.getNumSeeds());
    REQUIRE(copy.getMinPassRate() == Catch::Approx(original.getMinPassRate()));
    REQUIRE(copy.getRequirePerfect() == original.getRequirePerfect());
    REQUIRE(copy.getReportDetailedResults() == original.getReportDetailedResults());
}

TEST_CASE("BootstrapConfig: Typical use cases", "[BootstrapConfig]")
{
    SECTION("Conservative configuration (require perfect)")
    {
        BootstrapConfig config(10, 1.0, true, false);
        
        REQUIRE(config.getNumSeeds() == 10);
        REQUIRE(config.getMinPassRate() == Catch::Approx(1.0));
        REQUIRE(config.getRequirePerfect() == true);
    }
    
    SECTION("Standard configuration (95% threshold)")
    {
        BootstrapConfig config(10, 0.95, false, false);
        
        REQUIRE(config.getNumSeeds() == 10);
        REQUIRE(config.getMinPassRate() == Catch::Approx(0.95));
        REQUIRE(config.getRequirePerfect() == false);
    }
    
    SECTION("Exploratory configuration (lower threshold, detailed output)")
    {
        BootstrapConfig config(20, 0.80, false, true);
        
        REQUIRE(config.getNumSeeds() == 20);
        REQUIRE(config.getMinPassRate() == Catch::Approx(0.80));
        REQUIRE(config.getReportDetailedResults() == true);
    }
}

// ============================================================================
// STRATEGYBOOTSTRAPRESULT TESTS
// ============================================================================

TEST_CASE("StrategyBootstrapResult: Basic construction and getters", "[StrategyBootstrapResult]")
{
    auto strategy = createMockStrategy("TestStrategy");
    std::vector<uint64_t> seeds = {111, 222, 333, 444, 555};
    std::vector<bool> passed = {true, true, false, true, false};
    int passCount = 3;
    
    StrategyBootstrapResult result(strategy, seeds, passed, passCount);
    
    REQUIRE(result.getStrategy() == strategy);
    REQUIRE(result.getTestedSeeds() == seeds);
    REQUIRE(result.getPassedForEachSeed() == passed);
    REQUIRE(result.getPassCount() == 3);
    REQUIRE(result.getTotalTested() == 5);
    REQUIRE(result.getPassRate() == Catch::Approx(0.6));  // 3/5
    REQUIRE(result.isAccepted() == false);  // Default until set
}

TEST_CASE("StrategyBootstrapResult: Computed values", "[StrategyBootstrapResult]")
{
    SECTION("Perfect pass rate (100%)")
    {
        auto strategy = createMockStrategy("Perfect");
        std::vector<uint64_t> seeds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::vector<bool> passed(10, true);  // All true
        
        StrategyBootstrapResult result(strategy, seeds, passed, 10);
        
        REQUIRE(result.getPassCount() == 10);
        REQUIRE(result.getTotalTested() == 10);
        REQUIRE(result.getPassRate() == Catch::Approx(1.0));
    }
    
    SECTION("Zero pass rate (0%)")
    {
        auto strategy = createMockStrategy("Failed");
        std::vector<uint64_t> seeds = {1, 2, 3, 4, 5};
        std::vector<bool> passed(5, false);  // All false
        
        StrategyBootstrapResult result(strategy, seeds, passed, 0);
        
        REQUIRE(result.getPassCount() == 0);
        REQUIRE(result.getTotalTested() == 5);
        REQUIRE(result.getPassRate() == Catch::Approx(0.0));
    }
    
    SECTION("Marginal pass rate (50%)")
    {
        auto strategy = createMockStrategy("Marginal");
        std::vector<uint64_t> seeds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::vector<bool> passed = {true, false, true, false, true, false, true, false, true, false};
        
        StrategyBootstrapResult result(strategy, seeds, passed, 5);
        
        REQUIRE(result.getPassCount() == 5);
        REQUIRE(result.getTotalTested() == 10);
        REQUIRE(result.getPassRate() == Catch::Approx(0.5));
    }
    
    SECTION("High pass rate (90%)")
    {
        auto strategy = createMockStrategy("High");
        std::vector<uint64_t> seeds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::vector<bool> passed = {true, true, true, true, true, true, true, true, true, false};
        
        StrategyBootstrapResult result(strategy, seeds, passed, 9);
        
        REQUIRE(result.getPassCount() == 9);
        REQUIRE(result.getTotalTested() == 10);
        REQUIRE(result.getPassRate() == Catch::Approx(0.9));
    }
}

TEST_CASE("StrategyBootstrapResult: Acceptance flag", "[StrategyBootstrapResult]")
{
    auto strategy = createMockStrategy("Test");
    std::vector<uint64_t> seeds = {1, 2, 3, 4, 5};
    std::vector<bool> passed = {true, true, true, false, false};
    
    StrategyBootstrapResult result(strategy, seeds, passed, 3);
    
    SECTION("Default acceptance is false")
    {
        REQUIRE(result.isAccepted() == false);
    }
    
    SECTION("Can set acceptance to true")
    {
        result.setAccepted(true);
        REQUIRE(result.isAccepted() == true);
    }
    
    SECTION("Can set acceptance to false explicitly")
    {
        result.setAccepted(true);
        REQUIRE(result.isAccepted() == true);
        
        result.setAccepted(false);
        REQUIRE(result.isAccepted() == false);
    }
}

TEST_CASE("StrategyBootstrapResult: Edge cases", "[StrategyBootstrapResult]")
{
    SECTION("Single seed test")
    {
        auto strategy = createMockStrategy("Single");
        std::vector<uint64_t> seeds = {12345};
        std::vector<bool> passed = {true};
        
        StrategyBootstrapResult result(strategy, seeds, passed, 1);
        
        REQUIRE(result.getTotalTested() == 1);
        REQUIRE(result.getPassCount() == 1);
        REQUIRE(result.getPassRate() == Catch::Approx(1.0));
    }
    
    SECTION("Empty vectors (edge case - shouldn't happen in practice)")
    {
        auto strategy = createMockStrategy("Empty");
        std::vector<uint64_t> seeds;
        std::vector<bool> passed;
        
        StrategyBootstrapResult result(strategy, seeds, passed, 0);
        
        REQUIRE(result.getTotalTested() == 0);
        REQUIRE(result.getPassCount() == 0);
        REQUIRE(result.getPassRate() == Catch::Approx(0.0));
    }
    
    SECTION("Large number of seeds")
    {
        auto strategy = createMockStrategy("Large");
        std::vector<uint64_t> seeds(100);
        std::vector<bool> passed(100);
        
        // Fill with pattern: 95 passes, 5 fails
        for (int i = 0; i < 95; i++) passed[i] = true;
        for (int i = 95; i < 100; i++) passed[i] = false;
        
        StrategyBootstrapResult result(strategy, seeds, passed, 95);
        
        REQUIRE(result.getTotalTested() == 100);
        REQUIRE(result.getPassCount() == 95);
        REQUIRE(result.getPassRate() == Catch::Approx(0.95));
    }
}

TEST_CASE("StrategyBootstrapResult: Const correctness", "[StrategyBootstrapResult]")
{
    auto strategy = createMockStrategy("Const");
    std::vector<uint64_t> seeds = {1, 2, 3};
    std::vector<bool> passed = {true, true, false};
    
    const StrategyBootstrapResult result(strategy, seeds, passed, 2);
    
    // All const getters should work
    REQUIRE(result.getPassCount() == 2);
    REQUIRE(result.getTotalTested() == 3);
    REQUIRE(result.getPassRate() == Catch::Approx(0.666666).epsilon(0.001));
    REQUIRE(result.getTestedSeeds().size() == 3);
    REQUIRE(result.getPassedForEachSeed().size() == 3);
}

TEST_CASE("StrategyBootstrapResult: Vector references are stable", "[StrategyBootstrapResult]")
{
    auto strategy = createMockStrategy("Stable");
    std::vector<uint64_t> seeds = {111, 222, 333};
    std::vector<bool> passed = {true, false, true};
    
    StrategyBootstrapResult result(strategy, seeds, passed, 2);
    
    // Get references
    const auto& seedsRef1 = result.getTestedSeeds();
    const auto& passedRef1 = result.getPassedForEachSeed();
    
    // Get references again
    const auto& seedsRef2 = result.getTestedSeeds();
    const auto& passedRef2 = result.getPassedForEachSeed();
    
    // Should be the same object (same address)
    REQUIRE(&seedsRef1 == &seedsRef2);
    REQUIRE(&passedRef1 == &passedRef2);
    
    // Verify contents
    REQUIRE(seedsRef1.size() == 3);
    REQUIRE(passedRef1.size() == 3);
    REQUIRE(seedsRef1[0] == 111);
    REQUIRE(passedRef1[0] == true);
}

// ============================================================================
// ROBUSTNESSANALYSISRESULT TESTS
// ============================================================================

TEST_CASE("RobustnessAnalysisResult: Basic construction and getters", "[RobustnessAnalysisResult]")
{
    // Create mock strategy results
    auto strategy1 = createMockStrategy("S1");
    auto strategy2 = createMockStrategy("S2");
    auto strategy3 = createMockStrategy("S3");
    
    std::vector<uint64_t> seeds = {1, 2, 3, 4, 5};
    
    StrategyBootstrapResult sr1(strategy1, seeds, {true, true, true, true, true}, 5);
    sr1.setAccepted(true);
    
    StrategyBootstrapResult sr2(strategy2, seeds, {true, true, true, false, false}, 3);
    sr2.setAccepted(false);
    
    StrategyBootstrapResult sr3(strategy3, seeds, {true, true, false, false, false}, 2);
    sr3.setAccepted(false);
    
    std::vector<StrategyBootstrapResult> strategyResults = {sr1, sr2, sr3};
    std::vector<std::shared_ptr<PalStrategy<Num>>> acceptedStrategies = {strategy1};
    
    RobustnessAnalysisResult result(strategyResults, acceptedStrategies);
    
    REQUIRE(result.getTotalStrategies() == 3);
    REQUIRE(result.getAcceptedCount() == 1);
    REQUIRE(result.getRejectedCount() == 2);
    REQUIRE(result.getAcceptedStrategies().size() == 1);
    REQUIRE(result.getStrategyResults().size() == 3);
}

TEST_CASE("RobustnessAnalysisResult: Distribution statistics", "[RobustnessAnalysisResult]")
{
    // Use 20 seeds to get accurate 95% pass rate (19/20 = 95%)
    std::vector<uint64_t> seeds(20);
    for (int i = 0; i < 20; i++) seeds[i] = i + 1;
    
    // Create strategies with different pass rates
    auto perfect = createMockStrategy("Perfect");
    StrategyBootstrapResult srPerfect(perfect, seeds, 
        std::vector<bool>(20, true), 20);  // 100%
    srPerfect.setAccepted(true);
    
    auto high = createMockStrategy("High");
    std::vector<bool> highPassed(20, true);
    highPassed[19] = false;  // 19/20 = 95%
    StrategyBootstrapResult srHigh(high, seeds, highPassed, 19);  // 95%
    srHigh.setAccepted(true);
    
    auto moderate = createMockStrategy("Moderate");
    std::vector<bool> moderatePassed(20, true);
    for (int i = 16; i < 20; i++) moderatePassed[i] = false;  // 16/20 = 80%
    StrategyBootstrapResult srModerate(moderate, seeds, moderatePassed, 16);  // 80%
    srModerate.setAccepted(false);
    
    auto low = createMockStrategy("Low");
    std::vector<bool> lowPassed(20, false);
    for (int i = 0; i < 10; i++) lowPassed[i] = true;  // 10/20 = 50%
    StrategyBootstrapResult srLow(low, seeds, lowPassed, 10);  // 50%
    srLow.setAccepted(false);
    
    auto veryLow = createMockStrategy("VeryLow");
    std::vector<bool> veryLowPassed(20, false);
    for (int i = 0; i < 2; i++) veryLowPassed[i] = true;  // 2/20 = 10%
    StrategyBootstrapResult srVeryLow(veryLow, seeds, veryLowPassed, 2);  // 10%
    srVeryLow.setAccepted(false);
    
    std::vector<StrategyBootstrapResult> strategyResults = {
        srPerfect, srHigh, srModerate, srLow, srVeryLow
    };
    std::vector<std::shared_ptr<PalStrategy<Num>>> acceptedStrategies = {perfect, high};
    
    RobustnessAnalysisResult result(strategyResults, acceptedStrategies);
    
    SECTION("Total counts")
    {
        REQUIRE(result.getTotalStrategies() == 5);
        REQUIRE(result.getAcceptedCount() == 2);
        REQUIRE(result.getRejectedCount() == 3);
    }
    
    SECTION("Distribution statistics")
    {
        REQUIRE(result.getPerfectPassRateCount() == 1);    // 100%
        REQUIRE(result.getHighPassRateCount() == 1);       // 95% (95-99% range)
        REQUIRE(result.getModeratePassRateCount() == 1);   // 80%
        REQUIRE(result.getLowPassRateCount() == 1);        // 50%
        REQUIRE(result.getVeryLowPassRateCount() == 1);    // 10%
    }
}

TEST_CASE("RobustnessAnalysisResult: Edge cases in distribution", "[RobustnessAnalysisResult]")
{
    std::vector<uint64_t> seeds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    
    SECTION("All strategies perfect")
    {
        std::vector<StrategyBootstrapResult> results;
        std::vector<std::shared_ptr<PalStrategy<Num>>> accepted;
        
        for (int i = 0; i < 5; i++) {
            auto strat = createMockStrategy("Perfect" + std::to_string(i));
            StrategyBootstrapResult sr(strat, seeds, std::vector<bool>(20, true), 20);
            sr.setAccepted(true);
            results.push_back(sr);
            accepted.push_back(strat);
        }
        
        RobustnessAnalysisResult result(results, accepted);
        
        REQUIRE(result.getPerfectPassRateCount() == 5);
        REQUIRE(result.getHighPassRateCount() == 0);
        REQUIRE(result.getModeratePassRateCount() == 0);
        REQUIRE(result.getLowPassRateCount() == 0);
        REQUIRE(result.getVeryLowPassRateCount() == 0);
    }
    
    SECTION("All strategies fail")
    {
        std::vector<StrategyBootstrapResult> results;
        std::vector<std::shared_ptr<PalStrategy<Num>>> accepted;  // Empty
        
        for (int i = 0; i < 5; i++) {
            auto strat = createMockStrategy("Failed" + std::to_string(i));
            StrategyBootstrapResult sr(strat, seeds, std::vector<bool>(20, false), 0);
            sr.setAccepted(false);
            results.push_back(sr);
        }
        
        RobustnessAnalysisResult result(results, accepted);
        
        REQUIRE(result.getAcceptedCount() == 0);
        REQUIRE(result.getRejectedCount() == 5);
        REQUIRE(result.getVeryLowPassRateCount() == 5);  // All at 0%
    }
    
    SECTION("Boundary pass rates (exactly 95%, 80%, 50%)")
    {
        // 95% exactly (19/20)
        auto s95 = createMockStrategy("S95");
        std::vector<bool> p95(20, true);
        p95[19] = false;
        StrategyBootstrapResult sr95(s95, seeds, p95, 19);
        sr95.setAccepted(true);
        
        // 80% exactly (16/20)
        auto s80 = createMockStrategy("S80");
        std::vector<bool> p80(20, true);
        for (int i = 16; i < 20; i++) p80[i] = false;
        StrategyBootstrapResult sr80(s80, seeds, p80, 16);
        sr80.setAccepted(false);
        
        // 50% exactly (10/20)
        auto s50 = createMockStrategy("S50");
        std::vector<bool> p50(20, false);
        for (int i = 0; i < 10; i++) p50[i] = true;
        StrategyBootstrapResult sr50(s50, seeds, p50, 10);
        sr50.setAccepted(false);
        
        std::vector<StrategyBootstrapResult> results = {sr95, sr80, sr50};
        std::vector<std::shared_ptr<PalStrategy<Num>>> accepted = {s95};
        
        RobustnessAnalysisResult result(results, accepted);
        
        // 95% should be in "High" category (95-99% range)
        REQUIRE(result.getHighPassRateCount() == 1);
        // 80% should be in "Moderate" category (80-94% range)
        REQUIRE(result.getModeratePassRateCount() == 1);
        // 50% should be in "Low" category (50-79% range)
        REQUIRE(result.getLowPassRateCount() == 1);
    }
}

TEST_CASE("RobustnessAnalysisResult: Empty results", "[RobustnessAnalysisResult]")
{
    std::vector<StrategyBootstrapResult> emptyResults;
    std::vector<std::shared_ptr<PalStrategy<Num>>> emptyAccepted;
    
    RobustnessAnalysisResult result(emptyResults, emptyAccepted);
    
    REQUIRE(result.getTotalStrategies() == 0);
    REQUIRE(result.getAcceptedCount() == 0);
    REQUIRE(result.getRejectedCount() == 0);
    REQUIRE(result.getPerfectPassRateCount() == 0);
    REQUIRE(result.getHighPassRateCount() == 0);
    REQUIRE(result.getModeratePassRateCount() == 0);
    REQUIRE(result.getLowPassRateCount() == 0);
    REQUIRE(result.getVeryLowPassRateCount() == 0);
}

TEST_CASE("RobustnessAnalysisResult: Const correctness", "[RobustnessAnalysisResult]")
{
    auto strategy = createMockStrategy("Test");
    std::vector<uint64_t> seeds = {1, 2, 3};
    StrategyBootstrapResult sr(strategy, seeds, {true, true, false}, 2);
    sr.setAccepted(true);
    
    std::vector<StrategyBootstrapResult> results = {sr};
    std::vector<std::shared_ptr<PalStrategy<Num>>> accepted = {strategy};
    
    const RobustnessAnalysisResult result(results, accepted);
    
    // All getters should work on const object
    REQUIRE(result.getTotalStrategies() == 1);
    REQUIRE(result.getAcceptedCount() == 1);
    REQUIRE(result.getRejectedCount() == 0);
    REQUIRE(result.getStrategyResults().size() == 1);
    REQUIRE(result.getAcceptedStrategies().size() == 1);
}

TEST_CASE("RobustnessAnalysisResult: Large dataset", "[RobustnessAnalysisResult]")
{
    // Simulate a realistic scenario with 100 strategies
    // Use 20 seeds to get accurate 95% pass rate (19/20 = 95%)
    std::vector<uint64_t> seeds(20);
    for (int i = 0; i < 20; i++) seeds[i] = i + 1;
    
    std::vector<StrategyBootstrapResult> results;
    std::vector<std::shared_ptr<PalStrategy<Num>>> accepted;
    
    // Distribution: 40 perfect, 30 high, 15 moderate, 10 low, 5 very low
    for (int i = 0; i < 40; i++) {
        auto s = createMockStrategy("Perfect" + std::to_string(i));
        StrategyBootstrapResult sr(s, seeds, std::vector<bool>(20, true), 20);  // 100%
        sr.setAccepted(true);
        results.push_back(sr);
        accepted.push_back(s);
    }
    
    for (int i = 0; i < 30; i++) {
        auto s = createMockStrategy("High" + std::to_string(i));
        std::vector<bool> p(20, true);
        p[19] = false;  // 19/20 = 95%
        StrategyBootstrapResult sr(s, seeds, p, 19);
        sr.setAccepted(true);
        results.push_back(sr);
        accepted.push_back(s);
    }
    
    for (int i = 0; i < 15; i++) {
        auto s = createMockStrategy("Moderate" + std::to_string(i));
        std::vector<bool> p(20, true);
        for (int j = 16; j < 20; j++) p[j] = false;  // 16/20 = 80%
        StrategyBootstrapResult sr(s, seeds, p, 16);
        sr.setAccepted(false);
        results.push_back(sr);
    }
    
    for (int i = 0; i < 10; i++) {
        auto s = createMockStrategy("Low" + std::to_string(i));
        std::vector<bool> p(20, false);
        for (int j = 0; j < 12; j++) p[j] = true;  // 12/20 = 60%
        StrategyBootstrapResult sr(s, seeds, p, 12);
        sr.setAccepted(false);
        results.push_back(sr);
    }
    
    for (int i = 0; i < 5; i++) {
        auto s = createMockStrategy("VeryLow" + std::to_string(i));
        std::vector<bool> p(20, false);
        for (int j = 0; j < 2; j++) p[j] = true;  // 2/20 = 10%
        StrategyBootstrapResult sr(s, seeds, p, 2);
        sr.setAccepted(false);
        results.push_back(sr);
    }
    
    RobustnessAnalysisResult result(results, accepted);
    
    REQUIRE(result.getTotalStrategies() == 100);
    REQUIRE(result.getAcceptedCount() == 70);  // 40 perfect + 30 high
    REQUIRE(result.getRejectedCount() == 30);  // 15 moderate + 10 low + 5 very low
    REQUIRE(result.getPerfectPassRateCount() == 40);
    REQUIRE(result.getHighPassRateCount() == 30);
    REQUIRE(result.getModeratePassRateCount() == 15);
    REQUIRE(result.getLowPassRateCount() == 10);
    REQUIRE(result.getVeryLowPassRateCount() == 5);
}

// ============================================================================
// INTEGRATION TESTS (Multiple Classes Together)
// ============================================================================

TEST_CASE("Integration: BootstrapConfig with StrategyBootstrapResult interpretation", "[Integration]")
{
    // Test that pass rates align with config thresholds
    BootstrapConfig config(20, 0.95);
    
    auto strategy = createMockStrategy("Test");
    std::vector<uint64_t> seeds(20);
    for (int i = 0; i < 20; i++) seeds[i] = i + 1;
    
    SECTION("Strategy with 95% pass rate should meet threshold")
    {
        std::vector<bool> passed(20, true);
        passed[19] = false;  // 19/20 = 95%
        
        StrategyBootstrapResult sr(strategy, seeds, passed, 19);
        
        // Should meet threshold of 0.95
        REQUIRE(sr.getPassRate() >= config.getMinPassRate());
        
        sr.setAccepted(true);
        REQUIRE(sr.isAccepted() == true);
    }
    
    SECTION("Strategy with 80% pass rate should not meet threshold")
    {
        std::vector<bool> passed(20, true);
        for (int i = 16; i < 20; i++) passed[i] = false;  // 16/20 = 80%
        
        StrategyBootstrapResult sr(strategy, seeds, passed, 16);
        
        // Should NOT meet threshold of 0.95
        REQUIRE(sr.getPassRate() < config.getMinPassRate());
        
        sr.setAccepted(false);
        REQUIRE(sr.isAccepted() == false);
    }
}

TEST_CASE("Integration: Complete workflow simulation", "[Integration]")
{
    // Simulate a complete analysis workflow
    
    // Step 1: Create configuration
    BootstrapConfig config(20, 0.95, false, false);
    REQUIRE(config.getNumSeeds() == 20);
    
    // Step 2: Create strategy results
    std::vector<uint64_t> seeds(20);
    for (int i = 0; i < 20; i++) seeds[i] = i + 1;
    
    auto robust = createMockStrategy("RobustStrategy");
    StrategyBootstrapResult srRobust(robust, seeds, std::vector<bool>(20, true), 20);  // 100%
    srRobust.setAccepted(true);
    
    auto highQuality = createMockStrategy("HighQualityStrategy");
    std::vector<bool> highPassed(20, true);
    highPassed[19] = false;  // 19/20 = 95%
    StrategyBootstrapResult srHigh(highQuality, seeds, highPassed, 19);
    srHigh.setAccepted(true);  // Meets 95% threshold
    
    auto weak = createMockStrategy("WeakStrategy");
    std::vector<bool> weakPassed(20, false);
    for (int i = 0; i < 6; i++) weakPassed[i] = true;  // 6/20 = 30%
    StrategyBootstrapResult srWeak(weak, seeds, weakPassed, 6);
    srWeak.setAccepted(false);
    
    // Step 3: Create analysis result
    std::vector<StrategyBootstrapResult> allResults = {srRobust, srHigh, srWeak};
    std::vector<std::shared_ptr<PalStrategy<Num>>> acceptedOnly = {robust, highQuality};
    
    RobustnessAnalysisResult analysis(allResults, acceptedOnly);
    
    // Step 4: Verify results
    REQUIRE(analysis.getTotalStrategies() == 3);
    REQUIRE(analysis.getAcceptedCount() == 2);
    REQUIRE(analysis.getRejectedCount() == 1);
    REQUIRE(analysis.getPerfectPassRateCount() == 1);  // 100% strategy
    REQUIRE(analysis.getHighPassRateCount() == 1);     // 95% strategy is in high range
    REQUIRE(analysis.getVeryLowPassRateCount() == 1);  // 30% is very low
    
    // Verify accepted strategies list
    const auto& acceptedStrategies = analysis.getAcceptedStrategies();
    REQUIRE(acceptedStrategies.size() == 2);
    // REQUIRE(acceptedStrategies[0] == robust);  // Uncomment if not using nullptr
}
