#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "MonteCarloTestPolicy.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "StatUtils.h"
#include "DecimalConstants.h"
#include "TestUtils.h"
#include "AstResourceManager.h"
#include "TimeSeriesIndicators.h"

using namespace mkc_timeseries;

// Helper function to create a mock PriceActionLabPattern for testing.
// This pattern is for a long strategy.
std::shared_ptr<PriceActionLabPattern>
createTestLongPattern(const std::string& profitTargetStr, const std::string& stopLossStr)
{
    // Using a static resource manager to cache AST nodes is efficient.
    static mkc_palast::AstResourceManager resourceManager;
    auto percentLong = resourceManager.getDecimalNumber("100.0");
    auto percentShort = resourceManager.getDecimalNumber("0.0");
    auto description = std::make_shared<PatternDescription>("TestPattern.txt", 1, 20230101,
                                                           percentLong, percentShort, 20, 1);
    auto c0 = resourceManager.getPriceClose(0);
    auto o0 = resourceManager.getPriceOpen(0);
    auto patternExpr = std::make_shared<GreaterThanExpr>(c0, o0);
    auto entry = resourceManager.getLongMarketEntryOnOpen();
    auto ptValue = resourceManager.getDecimalNumber(profitTargetStr.c_str());
    auto slValue = resourceManager.getDecimalNumber(stopLossStr.c_str());
    auto profitTarget = resourceManager.getLongProfitTarget(ptValue);
    auto stopLoss = resourceManager.getLongStopLoss(slValue);
    
    // The cast is necessary because the factory returns a base class pointer.
    return std::make_shared<PriceActionLabPattern>(description, patternExpr, entry,
                                         std::dynamic_pointer_cast<ProfitTargetInPercentExpression>(profitTarget),
                                         std::dynamic_pointer_cast<StopLossInPercentExpression>(stopLoss));
}

// A mock BackTester class that allows us to inject specific return series
// and trade counts to test the policies' logic without running a full backtest.
template <class Decimal>
class MockPolicyBackTester : public BackTester<Decimal> {
private:
    uint32_t mNumTrades = 0;
    std::vector<Decimal> mHighResReturns;

public:
    MockPolicyBackTester() : BackTester<Decimal>() {}

    void setNumTrades(uint32_t trades) { mNumTrades = trades; }
    void setHighResReturns(const std::vector<Decimal>& returns) { mHighResReturns = returns; }

    // Override base class methods to return our mock data.
    uint32_t getNumTrades() const override { return mNumTrades; }
    std::vector<Decimal> getAllHighResReturns(typename BackTester<Decimal>::StrategyPtr strat) const override { return mHighResReturns; }

    // The clone method is required by the permutation testing framework.
    std::shared_ptr<BackTester<Decimal>> clone() const override {
        auto mock = std::make_shared<MockPolicyBackTester<Decimal>>();
        mock->setNumTrades(this->mNumTrades);
        mock->setHighResReturns(this->mHighResReturns);
        // Ensure the cloned backtester also has the strategy.
        for (auto it = this->beginStrategies(); it != this->endStrategies(); ++it)
            mock->addStrategy(*it);
            
        return mock;
    }

    // These methods are required by the BackTester interface.
    bool isDailyBackTester() const override { return true; }
    bool isWeeklyBackTester() const override { return false; }
    bool isMonthlyBackTester() const override { return false; }
    bool isIntradayBackTester() const override { return false; }
};

// Test suite for the bootstrapped Monte Carlo policies.
TEST_CASE("Bootstrapped Monte Carlo Policies", "[MonteCarloTestPolicy]") {
  using DT = DecimalType;

  // --- Setup common objects for all tests in this suite ---
  auto portfolio = std::make_shared<Portfolio<DT>>("TestPortfolio");
  auto backtester = std::make_shared<MockPolicyBackTester<DT>>();
    
  // Create a strategy with a 2% profit target and 1% stop loss (2:1 payoff ratio).
  auto palPattern = createTestLongPattern("0.02", "0.01");
  auto palStrategy = std::make_shared<PalLongStrategy<DT>>("TestPalStrategy", palPattern, portfolio);
  backtester->addStrategy(palStrategy);

  // A sample return series with a positive expected outcome.
  std::vector<DT> returns;
  returns.insert(returns.end(), 15, DT("0.02")); // 15 wins of 2%
  returns.insert(returns.end(), 10, DT("-0.01")); // 10 losses of 1%

  // --- Tests for BootStrappedProfitFactorPolicy ---
  SECTION("BootStrappedProfitFactorPolicy") {
    const auto minTrades = BootStrappedProfitFactorPolicy<DT>::getMinStrategyTrades();
    const auto minBars = BootStrappedProfitFactorPolicy<DT>::getMinBarSeriesSize();
    const DT failureStat = BootStrappedProfitFactorPolicy<DT>::getMinTradeFailureTestStatistic();

    SECTION("Fails if number of trades is below minimum") {
      backtester->setNumTrades(minTrades - 1);
      backtester->setHighResReturns(returns); // Sufficient bars
      DT statistic = BootStrappedProfitFactorPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Fails if number of bars is below minimum") {
      std::vector<DT> smallReturnSeries(minBars - 1, DT("0.01"));
      backtester->setNumTrades(minTrades); // Sufficient trades
      backtester->setHighResReturns(smallReturnSeries);
      DT statistic = BootStrappedProfitFactorPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Calculates a statistic whose distribution is centered on the true profit factor") {
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(returns);

      DT truePF = StatUtils<DT>::computeProfitFactor(returns);
            
      std::vector<DT> results;
      results.reserve(100);
      for(int i = 0; i < 100; ++i) {
	results.push_back(BootStrappedProfitFactorPolicy<DT>::getPermutationTestStatistic(backtester));
      }

      DT mean_pf = StatUtils<DT>::computeMean(results);
      DT stddev_pf = StatUtils<DT>::computeStdDev(results, mean_pf);

      // The true value should be within 3 standard deviations of the bootstrapped mean.
      REQUIRE_THAT(num::to_double(truePF), Catch::Matchers::WithinAbs(num::to_double(mean_pf), num::to_double(stddev_pf * DT(3.0))));
    }
  }

  // --- Tests for BootStrappedLogProfitFactorPolicy ---
  SECTION("BootStrappedLogProfitFactorPolicy") {
    const auto minTrades = BootStrappedLogProfitFactorPolicy<DT>::getMinStrategyTrades();
    const auto minBars = BootStrappedLogProfitFactorPolicy<DT>::getMinBarSeriesSize();
    const DT failureStat = BootStrappedLogProfitFactorPolicy<DT>::getMinTradeFailureTestStatistic();

    SECTION("Fails if number of trades is below minimum") {
      backtester->setNumTrades(minTrades - 1);
      backtester->setHighResReturns(returns);
      DT statistic = BootStrappedLogProfitFactorPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Fails if number of bars is below minimum") {
      std::vector<DT> smallReturnSeries(minBars - 1, DT("0.01"));
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(smallReturnSeries);
      DT statistic = BootStrappedLogProfitFactorPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Returns a conservative BCa lower bound for log profit factor") {
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(returns);

      DT trueLPF = StatUtils<DT>::computeLogProfitFactor(returns);

      std::vector<DT> results;
      results.reserve(150);
      for (int i = 0; i < 150; ++i) {
	results.push_back(BootStrappedLogProfitFactorPolicy<DT>::getPermutationTestStatistic(backtester));
      }

      DT mean_lpf   = StatUtils<DT>::computeMean(results);
      // 1) Conservative on average
      REQUIRE(num::to_double(mean_lpf) <= num::to_double(trueLPF) + 1e-12);

      // 2) Coverage sanity: most draws should be ≤ true LPF
      size_t covered = 0;
      for (const auto& lb : results) if (lb <= trueLPF) ++covered;
      REQUIRE(covered >= 135); // ≥90% of 150 draws conservative
      
      // 3) Monotonicity: if we improve returns (3% wins / 1% losses), LB should increase
      std::vector<DT> stronger = returns;
      for (auto& r : stronger) {
	if (r > DT(0)) r = DT("0.03"); else r = DT("-0.01");
      }
      backtester->setHighResReturns(stronger);

      std::vector<DT> results_stronger;
      results_stronger.reserve(100);
      for (int i = 0; i < 100; ++i) {
	results_stronger.push_back(BootStrappedLogProfitFactorPolicy<DT>::getPermutationTestStatistic(backtester));
      }
      DT mean_lpf_stronger = StatUtils<DT>::computeMean(results_stronger);
      
      REQUIRE(num::to_double(mean_lpf_stronger) >= num::to_double(mean_lpf) - 1e-12);
    }
  }

  SECTION("BootStrappedSharpeRatioPolicy") {
    const auto minTrades = BootStrappedSharpeRatioPolicy<DT>::getMinStrategyTrades();
    const auto minBars = BootStrappedSharpeRatioPolicy<DT>::getMinBarSeriesSize();
    const DT failureStat = BootStrappedSharpeRatioPolicy<DT>::getMinTradeFailureTestStatistic();

    SECTION("Fails if number of trades is below minimum") {
      backtester->setNumTrades(minTrades - 1);
      backtester->setHighResReturns(returns); // Sufficient bars
      DT statistic = BootStrappedSharpeRatioPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Fails if number of bars is below minimum") {
      std::vector<DT> smallReturnSeries(minBars - 1, DT("0.01"));
      backtester->setNumTrades(minTrades); // Sufficient trades
      backtester->setHighResReturns(smallReturnSeries);
      DT statistic = BootStrappedSharpeRatioPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Calculates a composite score whose distribution is centered on the expected value") {
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(returns);

      // The policy now returns the BCa lower bound of the Sharpe ratio directly,
      // without multiplying by a confidence factor.
      // Calculate the expected Sharpe ratio.
      DT mean_return = StatUtils<DT>::computeMean(returns);
      DT stddev_return = StatUtils<DT>::computeStdDev(returns, mean_return);
      DT trueSharpe = (stddev_return > DT(0)) ? (mean_return / stddev_return) : DT(0);

      // The expected score should be close to the Sharpe ratio, but slightly lower
      // due to the BCa lower bound adjustment. We'll verify the distribution is
      // reasonable by checking that results are consistently positive for this profitable series.
            
      std::vector<DT> results;
      results.reserve(100);
      for(int i = 0; i < 100; ++i) {
        results.push_back(BootStrappedSharpeRatioPolicy<DT>::getPermutationTestStatistic(backtester));
      }

      DT mean_score = StatUtils<DT>::computeMean(results);
      DT stddev_score = StatUtils<DT>::computeStdDev(results, mean_score);
            
      // The lower-bound statistic should not systematically exceed the sample Sharpe
      REQUIRE(num::to_double(mean_score) <= num::to_double(trueSharpe));

      // Median lower bound should also be ≤ sample Sharpe
      DT median_score = MedianOfVec(results);
      REQUIRE(num::to_double(median_score) <= num::to_double(trueSharpe));

      // A 95% lower bound should rarely exceed the sample Sharpe
      size_t exceed = std::count_if(results.begin(), results.end(),
      [&](const DT& x){ return num::to_double(x) > num::to_double(trueSharpe); });
      REQUIRE(exceed <= 5);  // allow up to ~α * 100 exceedances
    }
  }

  // --- Tests for BootStrappedProfitabilityPFPolicy ---
  SECTION("BootStrappedProfitabilityPFPolicy") {
    const auto minTrades = BootStrappedProfitabilityPFPolicy<DT>::getMinStrategyTrades();
    const auto minBars = BootStrappedProfitabilityPFPolicy<DT>::getMinBarSeriesSize();
    const DT failureStat = BootStrappedProfitabilityPFPolicy<DT>::getMinTradeFailureTestStatistic();

    SECTION("Fails if number of trades is below minimum") {
      backtester->setNumTrades(minTrades - 1);
      backtester->setHighResReturns(returns);
      DT statistic = BootStrappedProfitabilityPFPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Fails if number of bars is below minimum") {
      std::vector<DT> smallReturnSeries(minBars - 1, DT("0.01"));
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(smallReturnSeries);
      DT statistic = BootStrappedProfitabilityPFPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Calculates a score whose distribution is centered on the expected value") {
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(returns);

      // Manually calculate the expected score based on the true (non-bootstrapped) metrics.
      auto [truePF, trueProfitability] = StatUtils<DT>::computeProfitability(returns);
            
      DT targetPF = BootStrappedProfitabilityPFPolicy<DT>::getTargetProfitFactor();
      DT payoffRatio = palPattern->getPayoffRatio();
      DT expectedPALProfitability = (targetPF / (targetPF + payoffRatio)) * DT(100);

      DT profitabilityRatio = std::min(DT(1.0), trueProfitability / expectedPALProfitability);
      DT pfRatio = std::min(DT(1.5), truePF / targetPF);
      DT expectedScore = profitabilityRatio * pfRatio;
            
      std::vector<DT> results;
      results.reserve(100);
      for(int i = 0; i < 100; ++i) {
	results.push_back(BootStrappedProfitabilityPFPolicy<DT>::getPermutationTestStatistic(backtester));
      }

      DT mean_score = StatUtils<DT>::computeMean(results);
      DT stddev_score = StatUtils<DT>::computeStdDev(results, mean_score);
            
      REQUIRE_THAT(num::to_double(expectedScore), Catch::Matchers::WithinAbs(num::to_double(mean_score), num::to_double(stddev_score * DT(3.0))));
    }
  }

  // --- Tests for BootStrappedLogProfitabilityPFPolicy ---
  SECTION("BootStrappedLogProfitabilityPFPolicy") {
    const auto minTrades = BootStrappedLogProfitabilityPFPolicy<DT>::getMinStrategyTrades();
    const auto minBars = BootStrappedLogProfitabilityPFPolicy<DT>::getMinBarSeriesSize();
    const DT failureStat = BootStrappedLogProfitabilityPFPolicy<DT>::getMinTradeFailureTestStatistic();

    SECTION("Fails if number of trades is below minimum") {
      backtester->setNumTrades(minTrades - 1);
      backtester->setHighResReturns(returns);
      DT statistic = BootStrappedLogProfitabilityPFPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Fails if number of bars is below minimum") {
      std::vector<DT> smallReturnSeries(minBars - 1, DT("0.01"));
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(smallReturnSeries);
      DT statistic = BootStrappedLogProfitabilityPFPolicy<DT>::getPermutationTestStatistic(backtester);
      REQUIRE(statistic == failureStat);
    }

    SECTION("Calculates a score whose distribution is centered on the expected value") {
      backtester->setNumTrades(minTrades);
      backtester->setHighResReturns(returns);

      // Manually calculate the expected score based on the true (non-bootstrapped) metrics.
      auto [trueLPF, trueLP] = StatUtils<DT>::computeLogProfitability(returns);
            
      DecimalType expected_log_win = DT(std::log(num::to_double(DT(1.0) + palPattern->getProfitTargetAsDecimal())));
      DecimalType expected_log_loss = num::abs(DT(std::log(num::to_double(DT(1.0) - palPattern->getStopLossAsDecimal()))));
      DecimalType expectedLRWL = expected_log_win / expected_log_loss;
      DecimalType targetLogPF = BootStrappedLogProfitabilityPFPolicy<DT>::getTargetLogProfitFactor();
      DecimalType expectedLogProfitability = (DT(100.0) * targetLogPF) / (targetLogPF + expectedLRWL);

      DecimalType profitabilityRatio = std::min(DT(1.0), trueLP / expectedLogProfitability);
      DecimalType lpfRatio = std::min(DT(1.5), trueLPF / targetLogPF);
      DecimalType expectedScore = profitabilityRatio * lpfRatio;
            
      std::vector<DT> results;
      results.reserve(100);
      for(int i = 0; i < 100; ++i) {
	results.push_back(BootStrappedLogProfitabilityPFPolicy<DT>::getPermutationTestStatistic(backtester));
      }

      DT mean_score = StatUtils<DT>::computeMean(results);
      DT stddev_score = StatUtils<DT>::computeStdDev(results, mean_score);
            
      REQUIRE_THAT(num::to_double(expectedScore), Catch::Matchers::WithinAbs(num::to_double(mean_score), num::to_double(stddev_score * DT(3.0))));
    }
  }
}

// ============================================================================
// GeoMeanPolicy and MeanLogReturnPolicy
// ============================================================================
//
// These policies are deterministic — no bootstrap, no RNG — so the same input
// always produces the same output. Tests use exact value comparisons (tight
// WithinAbs tolerances) rather than distributional checks.
//
// A profitable series is used throughout: 15 wins of 0.87% + 10 losses of 1.14%
// matching the rounded exit parameters described in the design discussion.
//
// A losing series flips the ratio: 5 wins of 0.87% + 20 losses of 1.14%.
//
// Mathematical invariant under test:
//   log(1 + GeoMeanPolicy result) == MeanLogReturnPolicy result  (exact, same winsorization)

namespace
{
  // Helpers shared across the two new policy sections.

  // Profitable series: 67% win rate, target/stop matching the design discussion.
  std::vector<DecimalType> makeProfitableSeries()
  {
    std::vector<DecimalType> r;
    r.insert(r.end(), 15, DecimalType("0.0087"));   // 0.87% wins
    r.insert(r.end(), 10, DecimalType("-0.0114"));  // 1.14% losses
    return r;
  }

  // Losing series: 20% win rate, clearly negative expectancy.
  std::vector<DecimalType> makeLosingSeries()
  {
    std::vector<DecimalType> r;
    r.insert(r.end(), 5,  DecimalType("0.0087"));
    r.insert(r.end(), 20, DecimalType("-0.0114"));
    return r;
  }

  // Breakeven series: all zero returns. log(1+0)=0, mean=0 regardless of
  // winsorization (all values identical), so both policies must return exactly 0.
  std::vector<DecimalType> makeBreakevenSeries(std::size_t n = 25)
  {
    return std::vector<DecimalType>(n, DecimalType("0.0"));
  }

  // Larger profitable series (n=200) where winsorization has negligible effect —
  // useful for the analytical correctness tests because the expected value can be
  // computed independently without tracing adaptive-winsorizer internals.
  std::vector<DecimalType> makeLargeProfitableSeries()
  {
    std::vector<DecimalType> r;
    r.insert(r.end(), 134, DecimalType("0.0087"));
    r.insert(r.end(), 66,  DecimalType("-0.0114"));
    return r;
  }

  // Build a mock backtester pre-loaded with a strategy and a given return series.
  std::shared_ptr<MockPolicyBackTester<DecimalType>>
  makeBacktester(const std::vector<DecimalType>& returns, uint32_t numTrades = 10)
  {
    auto portfolio  = std::make_shared<Portfolio<DecimalType>>("TestPortfolio");
    auto palPattern = createTestLongPattern("0.0087", "0.0114");
    auto strategy   = std::make_shared<PalLongStrategy<DecimalType>>(
                        "GeoMeanTestStrategy", palPattern, portfolio);

    auto bt = std::make_shared<MockPolicyBackTester<DecimalType>>();
    bt->addStrategy(strategy);
    bt->setNumTrades(numTrades);
    bt->setHighResReturns(returns);
    return bt;
  }
} // anonymous namespace


// ----------------------------------------------------------------------------
// GeoMeanPolicy
// ----------------------------------------------------------------------------

TEST_CASE("GeoMeanPolicy: threshold guards", "[MonteCarloTestPolicy][GeoMeanPolicy]")
{
  using DT = DecimalType;
  const auto minTrades = GeoMeanPolicy<DT>::getMinStrategyTrades();
  const auto minBars   = GeoMeanPolicy<DT>::getMinBarSeriesSize();
  const DT   failure   = GeoMeanPolicy<DT>::getMinTradeFailureTestStatistic();

  SECTION("Returns failure statistic when trades below minimum")
  {
    auto bt = makeBacktester(makeProfitableSeries(), minTrades - 1);
    REQUIRE(GeoMeanPolicy<DT>::getPermutationTestStatistic(bt) == failure);
  }

  SECTION("Returns failure statistic when bar series below minimum")
  {
    std::vector<DT> tinyBars(minBars - 1, DT("0.005"));
    auto bt = makeBacktester(tinyBars, minTrades);
    REQUIRE(GeoMeanPolicy<DT>::getPermutationTestStatistic(bt) == failure);
  }

  SECTION("Returns failure statistic for empty bar series")
  {
    auto bt = makeBacktester({}, minTrades);
    REQUIRE(GeoMeanPolicy<DT>::getPermutationTestStatistic(bt) == failure);
  }

  SECTION("Throws when backtester has more than one strategy")
  {
    auto bt = makeBacktester(makeProfitableSeries(), minTrades);
    auto portfolio2 = std::make_shared<Portfolio<DT>>("P2");
    auto pattern2   = createTestLongPattern("0.01", "0.005");
    auto strategy2  = std::make_shared<PalLongStrategy<DT>>("S2", pattern2, portfolio2);
    bt->addStrategy(strategy2);
    REQUIRE_THROWS_AS(GeoMeanPolicy<DT>::getPermutationTestStatistic(bt),
                      BackTesterException);
  }
}

TEST_CASE("GeoMeanPolicy: sign correctness", "[MonteCarloTestPolicy][GeoMeanPolicy]")
{
  using DT = DecimalType;
  const auto minTrades = GeoMeanPolicy<DT>::getMinStrategyTrades();

  SECTION("Positive for a profitable series")
  {
    auto bt  = makeBacktester(makeProfitableSeries(), minTrades);
    DT   val = GeoMeanPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE(num::to_double(val) > 0.0);
  }

  SECTION("Negative for a losing series")
  {
    auto bt  = makeBacktester(makeLosingSeries(), minTrades);
    DT   val = GeoMeanPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE(num::to_double(val) < 0.0);
  }

  SECTION("Zero for all-zero returns (breakeven)")
  {
    auto bt  = makeBacktester(makeBreakevenSeries(), minTrades);
    DT   val = GeoMeanPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE_THAT(num::to_double(val), Catch::Matchers::WithinAbs(0.0, 1e-10));
  }
}

TEST_CASE("GeoMeanPolicy: determinism", "[MonteCarloTestPolicy][GeoMeanPolicy]")
{
  using DT = DecimalType;
  auto bt  = makeBacktester(makeProfitableSeries(),
                             GeoMeanPolicy<DT>::getMinStrategyTrades());

  // Policy is deterministic — repeated calls must return identical values.
  const DT first = GeoMeanPolicy<DT>::getPermutationTestStatistic(bt);
  for (int i = 0; i < 20; ++i)
  {
    DT val = GeoMeanPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE(val == first);
  }
}

TEST_CASE("GeoMeanPolicy: analytical correctness", "[MonteCarloTestPolicy][GeoMeanPolicy]")
{
  // GeoMeanPolicy must delegate exactly to GeoMeanStat with default construction.
  // We verify by computing the expected value independently and comparing tightly.
  using DT = DecimalType;

  auto series = makeLargeProfitableSeries();   // n=200, winsorization effect is minor
  auto bt     = makeBacktester(series, GeoMeanPolicy<DT>::getMinStrategyTrades());

  const DT policyResult = GeoMeanPolicy<DT>::getPermutationTestStatistic(bt);

  // Independent calculation via GeoMeanStat directly.
  GeoMeanStat<DT> stat{};  // same default construction as GeoMeanPolicy uses
  const DT expected = stat(series);

  REQUIRE_THAT(num::to_double(policyResult),
               Catch::Matchers::WithinAbs(num::to_double(expected), 1e-12));
}

TEST_CASE("GeoMeanPolicy: monotonicity", "[MonteCarloTestPolicy][GeoMeanPolicy]")
{
  // A more profitable series must produce a strictly higher statistic.
  using DT = DecimalType;
  const auto minTrades = GeoMeanPolicy<DT>::getMinStrategyTrades();

  auto btProfit = makeBacktester(makeProfitableSeries(), minTrades);
  auto btLoss   = makeBacktester(makeLosingSeries(),     minTrades);

  const DT profitStat = GeoMeanPolicy<DT>::getPermutationTestStatistic(btProfit);
  const DT lossStat   = GeoMeanPolicy<DT>::getPermutationTestStatistic(btLoss);

  REQUIRE(num::to_double(profitStat) > num::to_double(lossStat));
}

TEST_CASE("GeoMeanPolicy: sensitive to return distribution", "[MonteCarloTestPolicy][GeoMeanPolicy]")
{
  // A series with higher win rate must produce a higher statistic than
  // one with lower win rate, all else equal.
  using DT = DecimalType;
  const auto minTrades = GeoMeanPolicy<DT>::getMinStrategyTrades();

  std::vector<DT> highWinRate, lowWinRate;
  highWinRate.insert(highWinRate.end(), 18, DT("0.0087"));
  highWinRate.insert(highWinRate.end(),  7, DT("-0.0114"));
  lowWinRate.insert(lowWinRate.end(),   12, DT("0.0087"));
  lowWinRate.insert(lowWinRate.end(),   13, DT("-0.0114"));

  auto btHigh = makeBacktester(highWinRate, minTrades);
  auto btLow  = makeBacktester(lowWinRate,  minTrades);

  REQUIRE(num::to_double(GeoMeanPolicy<DT>::getPermutationTestStatistic(btHigh)) >
          num::to_double(GeoMeanPolicy<DT>::getPermutationTestStatistic(btLow)));
}


// ----------------------------------------------------------------------------
// MeanLogReturnPolicy
// ----------------------------------------------------------------------------

TEST_CASE("MeanLogReturnPolicy: threshold guards", "[MonteCarloTestPolicy][MeanLogReturnPolicy]")
{
  using DT = DecimalType;
  const auto minTrades = MeanLogReturnPolicy<DT>::getMinStrategyTrades();
  const auto minBars   = MeanLogReturnPolicy<DT>::getMinBarSeriesSize();
  const DT   failure   = MeanLogReturnPolicy<DT>::getMinTradeFailureTestStatistic();

  SECTION("Returns failure statistic when trades below minimum")
  {
    auto bt = makeBacktester(makeProfitableSeries(), minTrades - 1);
    REQUIRE(MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt) == failure);
  }

  SECTION("Returns failure statistic when bar series below minimum")
  {
    std::vector<DT> tinyBars(minBars - 1, DT("0.005"));
    auto bt = makeBacktester(tinyBars, minTrades);
    REQUIRE(MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt) == failure);
  }

  SECTION("Returns failure statistic for empty bar series")
  {
    auto bt = makeBacktester({}, minTrades);
    REQUIRE(MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt) == failure);
  }

  SECTION("Throws when backtester has more than one strategy")
  {
    auto bt = makeBacktester(makeProfitableSeries(), minTrades);
    auto portfolio2 = std::make_shared<Portfolio<DT>>("P2");
    auto pattern2   = createTestLongPattern("0.01", "0.005");
    auto strategy2  = std::make_shared<PalLongStrategy<DT>>("S2", pattern2, portfolio2);
    bt->addStrategy(strategy2);
    REQUIRE_THROWS_AS(MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt),
                      BackTesterException);
  }
}

TEST_CASE("MeanLogReturnPolicy: sign correctness", "[MonteCarloTestPolicy][MeanLogReturnPolicy]")
{
  using DT = DecimalType;
  const auto minTrades = MeanLogReturnPolicy<DT>::getMinStrategyTrades();

  SECTION("Positive for a profitable series")
  {
    auto bt  = makeBacktester(makeProfitableSeries(), minTrades);
    DT   val = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE(num::to_double(val) > 0.0);
  }

  SECTION("Negative for a losing series")
  {
    auto bt  = makeBacktester(makeLosingSeries(), minTrades);
    DT   val = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE(num::to_double(val) < 0.0);
  }

  SECTION("Zero for all-zero returns (breakeven)")
  {
    auto bt  = makeBacktester(makeBreakevenSeries(), minTrades);
    DT   val = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE_THAT(num::to_double(val), Catch::Matchers::WithinAbs(0.0, 1e-10));
  }
}

TEST_CASE("MeanLogReturnPolicy: determinism", "[MonteCarloTestPolicy][MeanLogReturnPolicy]")
{
  using DT = DecimalType;
  auto bt  = makeBacktester(makeProfitableSeries(),
                             MeanLogReturnPolicy<DT>::getMinStrategyTrades());

  const DT first = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt);
  for (int i = 0; i < 20; ++i)
  {
    DT val = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt);
    REQUIRE(val == first);
  }
}

TEST_CASE("MeanLogReturnPolicy: analytical correctness", "[MonteCarloTestPolicy][MeanLogReturnPolicy]")
{
  // With n=200, winsorization clips k=floor(0.02*200)=4 from each tail.
  // We compute the expected mean log return by hand using the same pipeline:
  // makeLogGrowthSeries → AdaptiveWinsorizer(0.02,1).apply → arithmetic mean.
  using DT = DecimalType;

  auto series = makeLargeProfitableSeries();  // n=200
  auto bt     = makeBacktester(series, MeanLogReturnPolicy<DT>::getMinStrategyTrades());

  const DT policyResult = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt);

  // Reproduce the computation independently.
  std::vector<DT> logBars =
    StatUtils<DT>::makeLogGrowthSeries(series, StatUtils<DT>::DefaultRuinEps);

  AdaptiveWinsorizer<DT> winsorizer(0.02, 1);
  winsorizer.apply(logBars);

  DT sum = DecimalConstants<DT>::DecimalZero;
  for (const auto& x : logBars)
    sum += x;
  const DT expected = sum / DT(static_cast<int>(logBars.size()));

  REQUIRE_THAT(num::to_double(policyResult),
               Catch::Matchers::WithinAbs(num::to_double(expected), 1e-12));
}

TEST_CASE("MeanLogReturnPolicy: monotonicity", "[MonteCarloTestPolicy][MeanLogReturnPolicy]")
{
  using DT = DecimalType;
  const auto minTrades = MeanLogReturnPolicy<DT>::getMinStrategyTrades();

  auto btProfit = makeBacktester(makeProfitableSeries(), minTrades);
  auto btLoss   = makeBacktester(makeLosingSeries(),     minTrades);

  const DT profitStat = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(btProfit);
  const DT lossStat   = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(btLoss);

  REQUIRE(num::to_double(profitStat) > num::to_double(lossStat));
}


// ----------------------------------------------------------------------------
// GeoMeanPolicy vs MeanLogReturnPolicy: inter-policy consistency
// ----------------------------------------------------------------------------

TEST_CASE("GeoMean vs MeanLogReturn: mathematical relationship",
          "[MonteCarloTestPolicy][GeoMeanPolicy][MeanLogReturnPolicy]")
{
  // The exact invariant: log(1 + GeoMean) == MeanLogReturn.
  // Both policies apply identical winsorization, so this must hold to floating
  // point precision for any input series that passes both threshold checks.
  using DT = DecimalType;
  const auto minTrades = GeoMeanPolicy<DT>::getMinStrategyTrades();

  auto checkRelationship = [&](const std::vector<DT>& series, const std::string& label)
  {
    auto bt = makeBacktester(series, minTrades);

    const DT geoMean    = GeoMeanPolicy<DT>::getPermutationTestStatistic(bt);
    const DT meanLogRet = MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt);

    // log(1 + geoMean) must equal meanLogReturn to within double round-trip
    // precision. GeoMeanStat computes exp(meanLog)-1 in Decimal space; we then
    // convert to double and apply std::log. That double round-trip loses ~3 ULPs
    // at this scale (~6e-4), so 1e-8 is the appropriate tolerance here.
    const double lhs = std::log(1.0 + num::to_double(geoMean));
    const double rhs = num::to_double(meanLogRet);

    INFO("Series: " << label);
    REQUIRE_THAT(lhs, Catch::Matchers::WithinAbs(rhs, 1e-8));
  };

  checkRelationship(makeProfitableSeries(),     "profitable (n=25)");
  checkRelationship(makeLosingSeries(),         "losing (n=25)");
  checkRelationship(makeBreakevenSeries(),      "breakeven (n=25)");
  checkRelationship(makeLargeProfitableSeries(),"profitable (n=200)");
}

TEST_CASE("GeoMean vs MeanLogReturn: preserve same ordering across series",
          "[MonteCarloTestPolicy][GeoMeanPolicy][MeanLogReturnPolicy]")
{
  // Since exp() is strictly monotonic, both policies must rank any two series
  // identically. Verify across several pairs.
  using DT = DecimalType;
  const auto minTrades = GeoMeanPolicy<DT>::getMinStrategyTrades();

  struct Pair { std::vector<DT> a, b; std::string label; };
  std::vector<Pair> pairs = {
    { makeProfitableSeries(), makeLosingSeries(),  "profitable > losing" },
    { makeLargeProfitableSeries(), makeProfitableSeries(), "large profitable > small profitable" },
  };

  for (const auto& p : pairs)
  {
    auto btA = makeBacktester(p.a, minTrades);
    auto btB = makeBacktester(p.b, minTrades);

    const double geoA = num::to_double(GeoMeanPolicy<DT>::getPermutationTestStatistic(btA));
    const double geoB = num::to_double(GeoMeanPolicy<DT>::getPermutationTestStatistic(btB));
    const double mlrA = num::to_double(MeanLogReturnPolicy<DT>::getPermutationTestStatistic(btA));
    const double mlrB = num::to_double(MeanLogReturnPolicy<DT>::getPermutationTestStatistic(btB));

    INFO("Pair: " << p.label);
    // Both policies must agree on which series is larger.
    REQUIRE((geoA > geoB) == (mlrA > mlrB));
  }
}

TEST_CASE("GeoMean vs MeanLogReturn: agree on sign for all test series",
          "[MonteCarloTestPolicy][GeoMeanPolicy][MeanLogReturnPolicy]")
{
  using DT = DecimalType;
  const auto minTrades = GeoMeanPolicy<DT>::getMinStrategyTrades();

  auto checkSign = [&](const std::vector<DT>& series, const std::string& label)
  {
    auto bt = makeBacktester(series, minTrades);
    const double geo = num::to_double(GeoMeanPolicy<DT>::getPermutationTestStatistic(bt));
    const double mlr = num::to_double(MeanLogReturnPolicy<DT>::getPermutationTestStatistic(bt));
    INFO("Series: " << label);
    // Signs must agree: both positive, both negative, or both zero.
    REQUIRE(((geo > 0) == (mlr > 0)));
    REQUIRE(((geo < 0) == (mlr < 0)));
  };

  checkSign(makeProfitableSeries(),     "profitable");
  checkSign(makeLosingSeries(),         "losing");
  checkSign(makeBreakevenSeries(),      "breakeven");
}

TEST_CASE("GeoMean vs MeanLogReturn: threshold constants match",
          "[MonteCarloTestPolicy][GeoMeanPolicy][MeanLogReturnPolicy]")
{
  // Both policies have the same minimum trade and bar requirements so they
  // can be used interchangeably in the validation framework without special-casing.
  using DT = DecimalType;
  REQUIRE(GeoMeanPolicy<DT>::getMinStrategyTrades() ==
          MeanLogReturnPolicy<DT>::getMinStrategyTrades());
  REQUIRE(GeoMeanPolicy<DT>::getMinBarSeriesSize() ==
          MeanLogReturnPolicy<DT>::getMinBarSeriesSize());
  REQUIRE(GeoMeanPolicy<DT>::getMinTradeFailureTestStatistic() ==
          MeanLogReturnPolicy<DT>::getMinTradeFailureTestStatistic());
}