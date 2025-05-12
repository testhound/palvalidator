#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include <tuple>
#include "PermutationTestComputationPolicy.h"
#include "TestUtils.h"
#include "Security.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "DecimalConstants.h"

using namespace mkc_timeseries;
using DecimalType = DecimalType;  // from TestUtils.h

namespace {

  // Policy that returns a fixed statistic of 0.5 and requires no minimum trades
  struct DummyStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.5");
    }
    static unsigned int getMinStrategyTrades() { return 0; }
  };

  // Policy that always returns a low statistic of 0.1
  struct AlwaysLowStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.1");
    }
    static unsigned int getMinStrategyTrades() { return 0; }
  };

  // A minimal BackTester that does nothing
  class DummyBackTester : public BackTester<DecimalType> {
  public:
    DummyBackTester() : BackTester<DecimalType>() {
      boost::gregorian::date start(2020,1,1), end(2020,12,31);
      this->addDateRange(DateRange(start,end));
    }
    std::shared_ptr<BackTester<DecimalType>> clone() const override {
      return std::make_shared<DummyBackTester>();
    }
    bool isDailyBackTester() const override { return true; }
    bool isWeeklyBackTester() const override { return false; }
    bool isMonthlyBackTester() const override { return false; }
    bool isIntradayBackTester() const override { return false; }
    void backtest() override {}

  protected:
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const override { return d; }
    TimeSeriesDate next_period(const TimeSeriesDate& d)   const override { return d; }
  };

  // A no-op strategy
  class DummyPalStrategy : public PalStrategy<DecimalType> {
  public:
    DummyPalStrategy(std::shared_ptr<Portfolio<DecimalType>> portfolio)
      : PalStrategy<DecimalType>("dummy", nullptr, portfolio, StrategyOptions(false,0)) {}

    std::shared_ptr<PalStrategy<DecimalType>> clone2(std::shared_ptr<Portfolio<DecimalType>> p) const override {
      return std::make_shared<DummyPalStrategy>(p);
    }
    std::shared_ptr<BacktesterStrategy<DecimalType>> clone(const std::shared_ptr<Portfolio<DecimalType>>& p) const override {
      return std::make_shared<DummyPalStrategy>(p);
    }
    std::shared_ptr<BacktesterStrategy<DecimalType>> cloneForBackTesting() const override {
      return std::make_shared<DummyPalStrategy>(this->getPortfolio());
    }
    void eventExitOrders(Security<DecimalType>*, const InstrumentPosition<DecimalType>&, const boost::gregorian::date&) override {}
    void eventEntryOrders(Security<DecimalType>*, const InstrumentPosition<DecimalType>&, const boost::gregorian::date&) override {}
  };

  // Create a security backed by a random series
  std::shared_ptr<Security<DecimalType>> createDummySecurity() {
    auto ts = getRandomPriceSeries();
    return std::make_shared<EquitySecurity<DecimalType>>("SYM", "Dummy", ts);
  }

  // Portfolio containing one dummy security
  std::shared_ptr<Portfolio<DecimalType>> createDummyPortfolio() {
    auto p = std::make_shared<Portfolio<DecimalType>>("Port");
    p->addSecurity(createDummySecurity());
    return p;
  }
}

TEST_CASE("DefaultPermuteMarketChangesPolicy returns p=1 when statistic always ≥ baseline", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  auto portfolio = createDummyPortfolio();
  auto strat = std::make_shared<DummyPalStrategy>(portfolio);
  bt->addStrategy(strat);

  DecimalType baseline("0.4");
  uint32_t numPerms = 1;

  // Default policy returns only p-value (PValueReturnPolicy)
  auto pValue = DefaultPermuteMarketChangesPolicy<DecimalType, DummyStatPolicy>::runPermutationTest(
    bt, numPerms, baseline
  );

  REQUIRE(pValue == DecimalType("1.0"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy returns small p-value when statistic always < baseline", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  auto portfolio = createDummyPortfolio();
  auto strat = std::make_shared<DummyPalStrategy>(portfolio);
  bt->addStrategy(strat);

  DecimalType baseline("0.5");
  uint32_t numPerms = 4;

  auto pValue = DefaultPermuteMarketChangesPolicy<DecimalType, AlwaysLowStatPolicy>::runPermutationTest(
    bt, numPerms, baseline
  );

  // Expect (0+1)/(4+1) = 0.2
  REQUIRE(pValue == DecimalType("0.2"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy with tuple return policy returns both p and summary", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  auto portfolio = createDummyPortfolio();
  auto strat = std::make_shared<DummyPalStrategy>(portfolio);
  bt->addStrategy(strat);

  DecimalType baseline("0.4");
  uint32_t numPerms = 1;

  using TuplePolicy = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    DummyStatPolicy,
    PValueAndTestStatisticReturnPolicy<DecimalType>,
    PermutationTestingMaxTestStatisticPolicy<DecimalType>
  >;

  auto result = TuplePolicy::runPermutationTest(bt, numPerms, baseline);
  auto [pValue, summaryStat] = result;

  REQUIRE(pValue == DecimalType("1.0"));
  REQUIRE(summaryStat == DecimalType("0.5"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy with max-statistic collection yields correct max", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  auto portfolio = createDummyPortfolio();
  auto strat = std::make_shared<DummyPalStrategy>(portfolio);
  bt->addStrategy(strat);

  DecimalType baseline("0.4");
  uint32_t numPerms = 5;

  using MaxPolicy = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    DummyStatPolicy,
    PValueAndTestStatisticReturnPolicy<DecimalType>,
    PermutationTestingMaxTestStatisticPolicy<DecimalType>
  >;

  auto result = MaxPolicy::runPermutationTest(bt, numPerms, baseline);
  auto [pValue, maxStat] = result;

  REQUIRE(pValue == DecimalType("1.0"));
  REQUIRE(maxStat == DecimalType("0.5"));
}

