// StrategyDataPreparerTest.cpp

#include <catch2/catch_test_macros.hpp>
#include "StrategyDataPreparer.h"
#include "Security.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

namespace {

  struct DummyStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.42");
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }
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

  class DummyMcptConfiguration : public McptConfiguration<DecimalType> {
  public:
    DummyMcptConfiguration(
      std::shared_ptr<BackTester<DecimalType>> bt,
      std::shared_ptr<Security<DecimalType>> sec,
      std::shared_ptr<PriceActionLabSystem> patterns,
      const DateRange& oosRange)
      : McptConfiguration(bt, bt, sec, patterns.get(), oosRange, oosRange, "dummy/path"),
	patternsHolder(patterns)
    {}

    // Keep shared_ptr of patterns alive since McptConfiguration stores raw ptr
    std::shared_ptr<PriceActionLabSystem> patternsHolder;
  };

  PALPatternPtr createDummyPattern(bool isLong = true) {
    auto desc = std::make_shared<PatternDescription>("dummy", 0, 20200101, new DecimalType("1.0"), new DecimalType("1.0"), 10, 0);
    auto expr = std::make_shared<GreaterThanExpr>(
      new PriceBarClose(0), new PriceBarOpen(0));
    auto entry = isLong ? static_cast<MarketEntryExpression*>(new LongMarketEntryOnOpen()) :
                          static_cast<MarketEntryExpression*>(new ShortMarketEntryOnOpen());
    auto target = new LongSideProfitTargetInPercent(new DecimalType("5.0"));
    auto stop = new LongSideStopLossInPercent(new DecimalType("2.0"));
    return std::make_shared<PriceActionLabPattern>(desc, expr, entry, target, stop);
  }

  //

  std::shared_ptr<Security<DecimalType>> createDummySecurity()
  {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES, 0);
  
    boost::gregorian::date d(2019, 12, 1);  // Way before Jan 2020
    int added = 0;
    
    while (added < 70)
      {
	if (isWeekday(d)) {
	  std::ostringstream dateStream;
	  dateStream << d.year()
		     << std::setw(2) << std::setfill('0') << d.month().as_number()
		     << std::setw(2) << std::setfill('0') << d.day().as_number();

	  auto entry = createTimeSeriesEntry(dateStream.str(), "100.0", "105.0", "95.0", "102.0", "1000.0");
	  ts->addEntry(*entry);
	  ++added;
	}
	d += boost::gregorian::days(1);
      }

    //    std::cout << "Dummy Time series" << *ts;
    return std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc", ts);
  }

  struct RunnerInitializer {
    RunnerInitializer() {
      static runner* r = new runner(4);  // 4 threads; instance_ptr() is set in constructor
      (void)r; // suppress unused warning
    }
  };

  static RunnerInitializer _runnerInit;

  /*
  std::shared_ptr<Security<DecimalType>> createDummySecurity() {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES, 10);
    for (int i = 0; i < 10; ++i) {
      auto entry = createTimeSeriesEntry("2020010" + std::to_string(i+1), "100", "105", "95", "102", "1000");
      ts->addEntry(*entry);
    }
    auto sec = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple", ts);
    return sec;
    }
  */

  

} // end anonymous namespace

TEST_CASE("StrategyDataPreparer::prepare returns strategies for valid configuration") {
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto patterns = std::make_shared<PriceActionLabSystem>();
  patterns->addPattern(createDummyPattern(true));
  patterns->addPattern(createDummyPattern(false));

  auto oosRange = DateRange(boost::gregorian::date(2020, 1, 15), boost::gregorian::date(2020, 2, 10));
  auto config = std::make_shared<DummyMcptConfiguration>(bt, sec, patterns, oosRange);
  config->patternsHolder = patterns; // Ensure shared_ptr stays alive

  auto results = StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(config);

  REQUIRE(results.size() == 2);
  for (const auto& ctx : results) {
    REQUIRE(ctx.strategy);
    REQUIRE(ctx.baselineStat == DecimalType("0.42"));
    REQUIRE(ctx.count == 1);
  }
}

TEST_CASE("StrategyDataPreparer::prepare throws on null configuration") {
  REQUIRE_THROWS_AS((StrategyDataPreparer<DecimalType, DummyStatPolicy>::prepare(nullptr)), std::runtime_error);
}
