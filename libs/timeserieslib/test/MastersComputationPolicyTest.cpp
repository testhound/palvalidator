#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "MastersPermutationTestComputationPolicy.h"
#include "TestUtils.h"
#include "Security.h"
#include <memory>
#include <vector>

using namespace mkc_timeseries;

namespace {

struct DummyStatPolicy {
  static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&)
  {
    return DecimalType("0.5");
  }
};

class DummyBackTester : public BackTester<DecimalType> {
public:
  DummyBackTester() : BackTester<DecimalType>()
  {
    boost::gregorian::date startDate(2020,1,1);
    boost::gregorian::date endDate(2020,12,31);
    
    DateRange r(startDate,endDate);
    this->addDateRange(r);
  }

  std::shared_ptr<BackTester<DecimalType>> clone() const override {
    return std::make_shared<DummyBackTester>();
  }

  TimeSeriesDate previous_period(const TimeSeriesDate& d) const
  {
    return boost_previous_weekday(d);
  }

  TimeSeriesDate next_period(const TimeSeriesDate& d) const
  {
    return boost_next_weekday(d);
  }

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

  // Add 10 synthetic entries with YYYYMMDD format
  for (int i = 0; i < 10; ++i) {
    std::ostringstream dateStream;
    dateStream << "202001" << std::setw(2) << std::setfill('0') << (i + 1);

    auto entry = createTimeSeriesEntry(
      dateStream.str(), "100.0", "105.0", "95.0", "102.0", "1000.0");

    ts->addEntry(*entry);
  }

  return std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc", ts);
}

std::shared_ptr<Portfolio<DecimalType>> createDummyPortfolio() {
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("DummyPortfolio");
  portfolio->addSecurity(createDummySecurity());
  return portfolio;
}

} // anonymous

TEST_CASE("MasterPermutationPolicy handles empty active strategies") {
  std::shared_ptr<DummyBackTester> bt = std::make_shared<DummyBackTester>();
  std::shared_ptr<Security<DecimalType>> sec = createDummySecurity();
  std::shared_ptr<Portfolio<DecimalType>> portfolio = createDummyPortfolio();

  auto count = MasterPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
    10, DecimalType("0.5"), {}, bt, sec, portfolio);

  REQUIRE(count == 1);
}

TEST_CASE("MasterPermutationPolicy works with basic valid input") {
  std::shared_ptr<DummyBackTester> bt = std::make_shared<DummyBackTester>();
  std::shared_ptr<Security<DecimalType>> sec = createDummySecurity();
  std::shared_ptr<Portfolio<DecimalType>> portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
  std::make_shared<DummyPalStrategy>(portfolio)
  };

  auto count = MasterPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
  10, DecimalType("0.5"), strategies, bt, sec, portfolio);

  REQUIRE(count >= 1);
}

TEST_CASE("MasterPermutationPolicy throws on null backtester") {
  std::shared_ptr<Security<DecimalType>> sec = createDummySecurity();
  std::shared_ptr<Portfolio<DecimalType>> portfolio = createDummyPortfolio();
  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    std::make_shared<DummyPalStrategy>(portfolio)
  };

  REQUIRE_THROWS_AS(
    (MasterPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
											   5, DecimalType("0.5"), strategies, nullptr, sec, portfolio)), // <--- Extra ( here
       std::runtime_error);
}

TEST_CASE("MasterPermutationPolicy works with multiple strategies (thread safety test)") {
  std::shared_ptr<DummyBackTester> bt = std::make_shared<DummyBackTester>();
  std::shared_ptr<Security<DecimalType>> sec = createDummySecurity();
  std::shared_ptr<Portfolio<DecimalType>> portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies;
  for (int i = 0; i < 10; ++i) {
    strategies.push_back(std::make_shared<DummyPalStrategy>(portfolio));
  }

  auto count = MasterPermutationPolicy<DecimalType, DummyStatPolicy>::computePermutationCountForStep(
    1000, DecimalType("0.5"), strategies, bt, sec, portfolio);

  REQUIRE(count >= 1);
}
