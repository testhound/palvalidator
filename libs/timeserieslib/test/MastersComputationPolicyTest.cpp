#include <catch2/catch_test_macros.hpp>
#include "MastersPermutationTestComputationPolicy.h"
#include "TestUtils.h"
#include "Security.h"
#include <memory>
#include <vector>

using namespace mkc_timeseries;

namespace {

  struct DummyStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.5");
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }
  };

  struct AlwaysLowStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.1");
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }
  };

  struct RandomStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>> &) {
      return DecimalType(std::to_string(0.3 + (std::rand() % 100) / 1000.0));
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }
  };
  
  class DummyBackTester : public BackTester<DecimalType> {
  public:
    DummyBackTester() : BackTester<DecimalType>() {
      boost::gregorian::date startDate(2020,1,1);
      boost::gregorian::date endDate(2020,12,31);
      DateRange r(startDate,endDate);
      this->addDateRange(r);
    }

    std::shared_ptr<BackTester<DecimalType>> clone() const override {
      return std::make_shared<DummyBackTester>();
    }

    TimeSeriesDate previous_period(const TimeSeriesDate& d) const { return boost_previous_weekday(d); }
    TimeSeriesDate next_period(const TimeSeriesDate& d) const { return boost_next_weekday(d); }
    void backtest() override {}
  };

  class DummyPalStrategy : public PalStrategy<DecimalType> {
  public:
    DummyPalStrategy(std::shared_ptr<Portfolio<DecimalType>> portfolio)
      : PalStrategy<DecimalType>("dummy", nullptr, portfolio, StrategyOptions(false, 0)) {}

    std::shared_ptr<PalStrategy<DecimalType>> clone2(std::shared_ptr<Portfolio<DecimalType>> portfolio) const override {
      return std::make_shared<DummyPalStrategy>(portfolio);
    }

    std::shared_ptr<BacktesterStrategy<DecimalType>> clone(std::shared_ptr<Portfolio<DecimalType>> portfolio) const override {
      return std::make_shared<DummyPalStrategy>(portfolio);
    }

    std::shared_ptr<BacktesterStrategy<DecimalType>> cloneForBackTesting() const override {
      return std::make_shared<DummyPalStrategy>(this->getPortfolio());
    }

    void eventExitOrders(std::shared_ptr<Security<DecimalType>>, const InstrumentPosition<DecimalType>&, const boost::gregorian::date&) override {}
    void eventEntryOrders(std::shared_ptr<Security<DecimalType>>, const InstrumentPosition<DecimalType>&, const boost::gregorian::date&) override {}
  };

  std::shared_ptr<Security<DecimalType>> createDummySecurity() {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES, 10);
    for (int i = 0; i < 10; ++i) {
      std::ostringstream dateStream;
      dateStream << "202001" << std::setw(2) << std::setfill('0') << (i + 1);
      auto entry = createTimeSeriesEntry(dateStream.str(), "100.0", "105.0", "95.0", "102.0", "1000.0");
      ts->addEntry(*entry);
    }
    return std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc", ts);
  }

  std::shared_ptr<Portfolio<DecimalType>> createDummyPortfolio() {
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("DummyPortfolio");
    portfolio->addSecurity(createDummySecurity());
    return portfolio;
  }

  StrategyContext<DecimalType> makeStrategyContext(std::shared_ptr<PalStrategy<DecimalType>> strat, DecimalType baseline) {
    StrategyContext<DecimalType> ctx;
    ctx.strategy = strat;
    ctx.baselineStat = baseline;
    ctx.count = 0;
    return ctx;
  }
} // anonymous namespace

TEST_CASE("MastersPermutationPolicy handles empty active strategies") {
  std::cout << "MastersPermutationPolicy handles empty active strategies" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  auto count = MastersPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
    10, DecimalType("0.5"), {}, bt, sec, portfolio);

  REQUIRE(count == 1);
  std::cout << "Finished MastersPermutationPolicy handles empty active strategies" << std::endl;
}

TEST_CASE("MastersPermutationPolicy works with basic valid input") {
  std::cout << "MastersPermutationPolicy works with basic valid input" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    std::make_shared<DummyPalStrategy>(portfolio)
  };

  auto count = MastersPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
    10, DecimalType("0.5"), strategies, bt, sec, portfolio);

  REQUIRE(count >= 1);
  std::cout << "Finished MastersPermutationPolicy works with basic valid input" << std::endl;
}

TEST_CASE("MastersPermutationPolicy throws on null backtester") {
  std::cout << "MastersPermutationPolicy throws on null backtester" << std::endl;
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    std::make_shared<DummyPalStrategy>(portfolio)
  };

  REQUIRE_THROWS_AS(
    (MastersPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
      5, DecimalType("0.5"), strategies, nullptr, sec, portfolio)),
    std::runtime_error);
}

TEST_CASE("MastersPermutationPolicy works with multiple strategies (thread safety test)") {
  std::cout << "In MastersPermutationPolicy works with multiple strategies (thread safety)" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies;
  for (int i = 0; i < 10; ++i) {
    strategies.push_back(std::make_shared<DummyPalStrategy>(portfolio));
  }

  auto count = MastersPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
    1000, DecimalType("0.5"), strategies, bt, sec, portfolio);

  REQUIRE(count >= 1);
}

TEST_CASE("FastMastersPermutationPolicy handles empty strategy data") {
  std::cout << "In FastMastersPermutationPolicy handles empty strategy data" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  auto result = FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::computeAllPermutationCounts(
    10, {}, bt, sec, portfolio);

  REQUIRE(result.empty());
}

TEST_CASE("FastMastersPermutationPolicy throws on null backtester") {
  std::cout << "In FastMastersPermutationPolicy throws on null backtester" << std::endl;
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  auto strategy = std::make_shared<DummyPalStrategy>(portfolio);

  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
  StrategyContext<DecimalType> ctx;
  ctx.strategy = strategy;
  ctx.baselineStat = DecimalType("0.5");
  ctx.count = 0;
  strategyData.push_back(ctx);

  REQUIRE_THROWS_AS(
    (FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::computeAllPermutationCounts(
      10, strategyData, nullptr, sec, portfolio)),
    std::runtime_error);
}

TEST_CASE("FastMastersPermutationPolicy basic test with single strategy") {
  std::cout << "In FastMastersPermutationPolicy basic test with single strategy" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  auto strategy = std::make_shared<DummyPalStrategy>(portfolio);

  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
  StrategyContext<DecimalType> ctx;
  ctx.strategy = strategy;
  ctx.baselineStat = DecimalType("0.5");
  ctx.count = 0;
  strategyData.push_back(ctx);

  auto result = FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::computeAllPermutationCounts(
    10, strategyData, bt, sec, portfolio);

  REQUIRE(result.size() == 1);
  REQUIRE(result[strategy] >= 1);
}

TEST_CASE("FastMastersPermutationPolicy handles multiple strategies") {
  std::cout << "In FastMastersPermutationPolicy handles multiple strategies" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;

  for (int i = 0; i < 5; ++i) {
    auto strategy = std::make_shared<DummyPalStrategy>(portfolio);
    StrategyContext<DecimalType> ctx;
    ctx.strategy = strategy;
    ctx.baselineStat = DecimalType("0.5");
    ctx.count = 0;
    strategyData.push_back(ctx);
  }

  auto result = FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::computeAllPermutationCounts(
    1000, strategyData, bt, sec, portfolio);

  REQUIRE(result.size() == 5);
  for (auto it = result.begin(); it != result.end(); ++it) {
    REQUIRE(it->second >= 1);
  }
}

TEST_CASE("FastMastersPermutationPolicy returns counts of 1 when no permutation exceeds baseline") {
  std::cout << "In FastMastersPermutationPolicy returns count of 1" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  auto strategy = std::make_shared<DummyPalStrategy>(portfolio);

  FastMastersPermutationPolicy<DecimalType, AlwaysLowStatPolicy>::LocalStrategyDataContainer strategyData;
  StrategyContext<DecimalType> ctx;
  ctx.strategy = strategy;
  ctx.baselineStat = DecimalType("0.5");
  ctx.count = 0;
  strategyData.push_back(ctx);

  auto result = FastMastersPermutationPolicy<DecimalType, AlwaysLowStatPolicy>::computeAllPermutationCounts(
    10, strategyData, bt, sec, portfolio);

  REQUIRE(result[strategy] == 1);
}

TEST_CASE("FastMastersPermutationPolicy with randomized statistics produces reasonable counts") {
  std::cout << "In FastMastersPermutationPolicy with randomized statistics" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  FastMastersPermutationPolicy<DecimalType, RandomStatPolicy>::LocalStrategyDataContainer strategyData;

  for (int i = 0; i < 3; ++i) {
    auto strategy = std::make_shared<DummyPalStrategy>(portfolio);
    strategyData.push_back(makeStrategyContext(strategy, DecimalType("0.35")));
  }

  auto result = FastMastersPermutationPolicy<DecimalType, RandomStatPolicy>::computeAllPermutationCounts(
    100, strategyData, bt, sec, portfolio);

  REQUIRE(result.size() == 3);
  for (auto it = result.begin(); it != result.end(); ++it) {
    REQUIRE(it->second >= 1);
    REQUIRE(it->second <= 101);
  }
}
