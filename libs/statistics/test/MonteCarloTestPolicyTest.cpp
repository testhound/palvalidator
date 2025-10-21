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
