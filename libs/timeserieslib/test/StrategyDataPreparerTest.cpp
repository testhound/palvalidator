// StrategyDataPreparerTest.cpp

#include <catch2/catch_test_macros.hpp>
#include <cpptrace/from_current.hpp>
#include "StrategyDataPreparer.h"
#include "Security.h"
#include "TestUtils.h"
#include "PalAst.h"

using namespace mkc_timeseries;

namespace {

  struct DummyStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&)
    {
      return DecimalType("0.42");
    }
    static unsigned int getMinStrategyTrades() { return 0; }
  };

  class DummyBackTester : public BackTester<DecimalType> {
  public:
    DummyBackTester() {
      auto d1 = boost::gregorian::date(2020, 1, 15);
      auto d2 = boost::gregorian::date(2020, 2, 10);
      this->addDateRange(DateRange(d1, d2));
    }
    std::shared_ptr<BackTester<DecimalType>> clone() const override {
      return std::make_shared<DummyBackTester>(*this);
    }
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const override {
      return boost_previous_weekday(d);
    }
    TimeSeriesDate next_period(const TimeSeriesDate& d) const override {
      return boost_next_weekday(d);
    }
    void backtest() override {}
  };

  PALPatternPtr createDummyPattern(bool isLong = true) {
    auto desc = std::make_shared<PatternDescription>("dummy", 0, 20200101,
        new DecimalType("1.0"), new DecimalType("1.0"), 10, 0);
    auto expr = std::make_shared<GreaterThanExpr>(
        new PriceBarClose(0), new PriceBarOpen(0));
    auto entry = isLong
        ? static_cast<MarketEntryExpression*>(new LongMarketEntryOnOpen())
        : static_cast<MarketEntryExpression*>(new ShortMarketEntryOnOpen());
    auto target = new LongSideProfitTargetInPercent(new DecimalType("5.0"));
    auto stop   = new LongSideStopLossInPercent(new DecimalType("2.0"));
    return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
  }

  std::shared_ptr<Security<DecimalType>> createDummySecurity()
  {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(
        TimeFrame::DAILY, TradingVolume::SHARES, 0);
    boost::gregorian::date d(2019, 12, 1);
    int added = 0;
    while (added < 70) {
      if (isWeekday(d)) {
        std::ostringstream dateStream;
        dateStream << d.year()
                   << std::setw(2) << std::setfill('0') << d.month().as_number()
                   << std::setw(2) << std::setfill('0') << d.day().as_number();
        auto entry = createTimeSeriesEntry(
            dateStream.str(), "100.0", "105.0", "95.0", "102.0", "1000.0");
        ts->addEntry(*entry);
        ++added;
      }
      d += boost::gregorian::days(1);
    }
    return std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc", ts);
  }

  struct RunnerInitializer {
    RunnerInitializer() {
      static runner* r = new runner(4);
      (void)r;
    }
  };
  static RunnerInitializer _runnerInit;

} // end anonymous namespace

TEST_CASE("StrategyDataPreparer::prepare returns strategies for valid inputs") {
  auto bt       = std::make_shared<DummyBackTester>();
  auto sec      = createDummySecurity();
  auto patterns = std::make_shared<PriceActionLabSystem>();
  patterns->addPattern(createDummyPattern(true));
  patterns->addPattern(createDummyPattern(false));

  CPPTRACE_TRY {
    auto results = StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
        bt, sec, patterns.get());
    REQUIRE(results.size() == 2);
    for (const auto& ctx : results) {
      REQUIRE(ctx.strategy);
      REQUIRE(ctx.baselineStat == DecimalType("0.42"));
      REQUIRE(ctx.count == 1);
    }
  }
  CPPTRACE_CATCH(const std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    cpptrace::from_current_exception().print();
  }
}

TEST_CASE("StrategyDataPreparer::prepare throws on null inputs") {
  auto bt       = std::make_shared<DummyBackTester>();
  auto sec      = createDummySecurity();
  auto patterns = std::make_shared<PriceActionLabSystem>();
  patterns->addPattern(createDummyPattern(true));

  REQUIRE_THROWS_AS(
      (StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
          nullptr, sec, patterns.get())),
      std::runtime_error);

  REQUIRE_THROWS_AS(
      (StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
          bt, nullptr, patterns.get())),
      std::runtime_error);

  REQUIRE_THROWS_AS(
      (StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
          bt, sec, nullptr)),
      std::runtime_error);
}

TEST_CASE("prepare returns empty container when no patterns") {
  auto bt       = std::make_shared<DummyBackTester>();
  auto sec      = createDummySecurity();
  PriceActionLabSystem emptyPatterns;

  auto results = StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
      bt, sec, &emptyPatterns);

  REQUIRE(results.empty());
}


TEST_CASE("prepare propagates exception from back-tester") {
  struct ExplodingBackTester : DummyBackTester {
    void backtest() override { throw std::runtime_error("boom"); }
    std::shared_ptr<BackTester<DecimalType>> clone() const override {
      return std::make_shared<ExplodingBackTester>(*this);
    }
  };

  auto bt       = std::make_shared<ExplodingBackTester>();
  auto sec      = createDummySecurity();
  auto patterns = std::make_shared<PriceActionLabSystem>();
  patterns->addPattern(createDummyPattern(true));

  // Because backtest() throws, prepare() should throw too

  REQUIRE_THROWS(
		 (StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
									      bt, sec, patterns.get())));
}

TEST_CASE("prepare assigns correct strategy names") {
  auto bt       = std::make_shared<DummyBackTester>();
  auto sec      = createDummySecurity();
  auto patterns = std::make_shared<PriceActionLabSystem>();
  patterns->addPattern(createDummyPattern(true));
  patterns->addPattern(createDummyPattern(false));
  patterns->addPattern(createDummyPattern(true));

  auto results = StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
      bt, sec, patterns.get());
  REQUIRE(results.size() == 3);

  std::vector<std::string> names;
  for (const auto& ctx : results)
    names.push_back(ctx.strategy->getStrategyName());

  REQUIRE(std::find(names.begin(), names.end(), "PAL Long 1")  != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "PAL Short 2") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "PAL Long 3")  != names.end());
}

// New tests using real price series and real patterns from TestUtils.h

TEST_CASE("StrategyDataPreparer::prepare with random price series") {
  auto bt     = std::make_shared<DummyBackTester>();
  auto series = getRandomPriceSeries();
  REQUIRE(series->getNumEntries() > 0);
  auto sec    = std::make_shared<EquitySecurity<DecimalType>>("RND", "Random Security", series);

  auto patterns = std::make_shared<PriceActionLabSystem>();
  patterns->addPattern(createDummyPattern(true));
  patterns->addPattern(createDummyPattern(false));

  CPPTRACE_TRY {
    auto results = StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
        bt, sec, patterns.get());
    REQUIRE(results.size() == 2);
  }
  CPPTRACE_CATCH(...) {
    FAIL("Backtest on random series should not throw");
  }
}


TEST_CASE("StrategyDataPreparer::prepare with random price patterns") {
  auto bt       = std::make_shared<DummyBackTester>();
  auto sec      = createDummySecurity();
  PriceActionLabSystem* patterns = getRandomPricePatterns();
  REQUIRE(patterns->getNumPatterns() > 0);

  CPPTRACE_TRY {
    auto results = StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(
        bt, sec, patterns);
    REQUIRE(results.size() == patterns->getNumPatterns());
    for (const auto& ctx : results) {
      REQUIRE(ctx.strategy);
      REQUIRE(ctx.baselineStat == DecimalType("0.42"));
      REQUIRE(ctx.count == 1);
    }
  }
  CPPTRACE_CATCH(...) {
    FAIL("Backtest on random patterns should not throw");
  }
  delete patterns;
}
