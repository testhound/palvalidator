#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "MonteCarloTestPolicy.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "StatUtils.h"
#include "DecimalConstants.h"
#include "TestUtils.h"
#include "AstResourceManager.h"

using namespace mkc_timeseries;

std::shared_ptr<PriceActionLabPattern>
createTestLongPattern(const std::string& profitTargetStr, const std::string& stopLossStr)
{
    mkc_palast::AstResourceManager resourceManager;
    auto percentLong = resourceManager.getDecimalNumber("100.0");
    auto percentShort = resourceManager.getDecimalNumber("0.0");
    auto description = std::make_shared<PatternDescription>("TestPattern.txt", 1, 20230101,
                                                           percentLong, percentShort, 20, 1);
    auto c0 = resourceManager.getPriceClose(0);
    auto o0 = resourceManager.getPriceOpen(0);
    auto patternExpr = std::make_shared<GreaterThanExpr>(c0, o0);
    auto entry = resourceManager.getLongMarketEntryOnOpen();
    auto ptValue = resourceManager.getDecimalNumber(profitTargetStr);
    auto slValue = resourceManager.getDecimalNumber(stopLossStr);
    auto profitTarget = resourceManager.getLongProfitTarget(ptValue);
    auto stopLoss = resourceManager.getLongStopLoss(slValue);
    return resourceManager.createPattern(description, patternExpr, entry,
                                         std::dynamic_pointer_cast<ProfitTargetInPercentExpression>(profitTarget),
                                         std::dynamic_pointer_cast<StopLossInPercentExpression>(stopLoss));
}

template <class Decimal>
class MockPolicyBackTester : public BackTester<Decimal> {
private:
    uint32_t mNumTrades = 0;
    std::vector<Decimal> mHighResReturns;

public:
    MockPolicyBackTester() : BackTester<Decimal>() {}

    void setNumTrades(uint32_t trades) { mNumTrades = trades; }
    void setHighResReturns(const std::vector<Decimal>& returns) { mHighResReturns = returns; }

    uint32_t getNumTrades() const override { return mNumTrades; }
    std::vector<Decimal> getAllHighResReturns(typename BackTester<Decimal>::StrategyPtr strat) const override { return mHighResReturns; }

    std::shared_ptr<BackTester<Decimal>> clone() const override {
        auto mock = std::make_shared<MockPolicyBackTester<Decimal>>();
        mock->setNumTrades(this->mNumTrades);
        mock->setHighResReturns(this->mHighResReturns);
        return mock;
    }

    bool isDailyBackTester() const override { return true; }
    bool isWeeklyBackTester() const override { return false; }
    bool isMonthlyBackTester() const override { return false; }
    bool isIntradayBackTester() const override { return false; }
};

TEST_CASE("GatedProfitabilityScaledPalPolicy tests", "[MonteCarloTestPolicy]") {
    using DT = DecimalType;

    auto portfolio = std::make_shared<Portfolio<DT>>("TestPortfolio");
    auto palPattern = createTestLongPattern("2.0", "1.0");
    auto palStrategy = std::make_shared<PalLongStrategy<DT>>("TestPalStrategy", palPattern, portfolio);
    auto backtester = std::make_shared<MockPolicyBackTester<DT>>();
    backtester->addStrategy(palStrategy);

    const auto minTrades = GatedProfitabilityScaledPalPolicy<DT>::getMinStrategyTrades();
    const auto minBars = GatedProfitabilityScaledPalPolicy<DT>::getMinBarSeriesSize();
    const DT failureStat = GatedProfitabilityScaledPalPolicy<DT>::getMinTradeFailureTestStatistic();

    SECTION("Fails if number of trades is below minimum threshold") {
        backtester->setNumTrades(minTrades - 1);
        backtester->setHighResReturns({DT(1.0)});
        DT statistic = GatedProfitabilityScaledPalPolicy<DT>::getPermutationTestStatistic(backtester);
        REQUIRE(statistic == failureStat);
    }

    SECTION("Fails if high-resolution return series is too small") {
        std::vector<DT> returns(minBars - 1, DT(0.1));
        backtester->setNumTrades(minTrades);
        backtester->setHighResReturns(returns);
        DT statistic = GatedProfitabilityScaledPalPolicy<DT>::getPermutationTestStatistic(backtester);
        REQUIRE(statistic == failureStat);
    }

    SECTION("Fails if bootstrapped Profit Factor is below the gate") {
        std::vector<DT> returns;
        returns.insert(returns.end(), 10, DT("0.15"));
        returns.insert(returns.end(), 10, DT("-0.1"));
        backtester->setNumTrades(minTrades);
        backtester->setHighResReturns(returns);

	DT statistic = GatedProfitabilityScaledPalPolicy<DT>::getDeterministicTestStatistic(backtester);
        REQUIRE(statistic == failureStat);
    }

    SECTION("Successful calculation with strong performance") {
        std::vector<DT> returns(20, DT("0.1"));
        backtester->setNumTrades(20);
        backtester->setHighResReturns(returns);

        auto [expectedPF, expectedProfitability] = StatUtils<DT>::computeProfitability(returns);
        DT targetPF = GatedProfitabilityScaledPalPolicy<DT>::getTargetProfitFactor();
        DT payoffRatio = palPattern->getProfitTargetAsDecimal() / palPattern->getStopLossAsDecimal();
        DT expectedPAL = (targetPF / (targetPF + payoffRatio)) * DT(100);
        DT profitabilityRatio = std::min(expectedProfitability / expectedPAL, DT(1.0));
        DT pfRatio = std::min(expectedPF / targetPF, DT("1.5"));
        DT expectedFinalScore = profitabilityRatio * pfRatio; // Should be 1.5

        DT statistic = GatedProfitabilityScaledPalPolicy<DT>::getDeterministicTestStatistic(backtester);
        REQUIRE_THAT(num::to_double(statistic), Catch::Matchers::WithinAbs(num::to_double(expectedFinalScore), 0.0001));
    }

    SECTION("Successful calculation with mixed performance") {
        std::vector<DT> returns;
        returns.insert(returns.end(), 10, DT("0.5"));
        returns.insert(returns.end(), 10, DT("-0.2"));
        backtester->setNumTrades(20);
        backtester->setHighResReturns(returns);

        auto [expectedPF, expectedProfitability] = StatUtils<DT>::computeProfitability(returns);
        DT targetPF = GatedProfitabilityScaledPalPolicy<DT>::getTargetProfitFactor();
        DT payoffRatio = palPattern->getProfitTargetAsDecimal() / palPattern->getStopLossAsDecimal();
        DT expectedPAL = (targetPF / (targetPF + payoffRatio)) * DT(100);
        DT profitabilityRatio = std::min(expectedProfitability / expectedPAL, DT(1.0));
        DT pfRatio = std::min(expectedPF / targetPF, DT("1.5"));
        DT expectedFinalScore = profitabilityRatio * pfRatio; // Should be 1.25

        DT statistic = GatedProfitabilityScaledPalPolicy<DT>::getDeterministicTestStatistic(backtester);
        REQUIRE_THAT(num::to_double(statistic), Catch::Matchers::WithinAbs(num::to_double(expectedFinalScore), 0.0001));
    }
}
