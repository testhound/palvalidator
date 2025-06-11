#include <catch2/catch_test_macros.hpp>
#include "PALMastersMonteCarloValidation.h"
#include "MastersRomanoWolfImproved.h"
#include "Security.h"
#include "Portfolio.h"
#include "TestUtils.h"
#include "StrategyDataPreparer.h"

using namespace mkc_timeseries;
typedef DecimalType D;

namespace {

struct DummyStatPolicy {
  static D getPermutationTestStatistic(const std::shared_ptr<BackTester<D>>&) {
    return D("0.5");
  }
  static unsigned int getMinStrategyTrades() { return 0; }
  static D getMinTradeFailureTestStatistic() {
    return DecimalConstants<D>::DecimalZero;
  }
};

class DummyBackTesterEx : public BackTester<D> {
public:
  DummyBackTesterEx() : BackTester<D>() {}
  std::shared_ptr<BackTester<D>> clone() const override { return std::make_shared<DummyBackTesterEx>(); }
  void backtest() override {}
  bool isDailyBackTester() const { return true; }
  bool isWeeklyBackTester() const { return false; }
  bool isMonthlyBackTester() const { return false; }
  bool isIntradayBackTester() const { return false; }
};

class DummyAlgo : public IMastersSelectionBiasAlgorithm<D, DummyStatPolicy> {
public:
  std::map<std::shared_ptr<PalStrategy<D>>, D> run(const std::vector<StrategyContext<D>>& strategyData,
                                                    unsigned long,
                                                    const std::shared_ptr<BackTester<D>>&,
                                                    const std::shared_ptr<Portfolio<D>>&, const D&) override {
    std::map<std::shared_ptr<PalStrategy<D>>, D> result;
    for (const auto& ctx : strategyData) {
      result[ctx.strategy] = D("0.01");
    }
    return result;
  }
};

  struct EmptyMapAlgo : IMastersSelectionBiasAlgorithm<D, DummyStatPolicy>
  {
    std::map<std::shared_ptr<PalStrategy<D>>, D> run(
        const std::vector<StrategyContext<D>>& strategyData,
        unsigned long                          numPermutations,
        const std::shared_ptr<BackTester<D>>&  templateBackTester,
        const std::shared_ptr<Portfolio<D>>&   portfolio,
        const D&                               pValueSignificanceLevel) override
    {
      return {};  // empty map
    }
  };


  struct HighPAlgo : IMastersSelectionBiasAlgorithm<D, DummyStatPolicy>
  {
    std::map<std::shared_ptr<PalStrategy<D>>, D> run(
        const std::vector<StrategyContext<D>>& strategyData,
        unsigned long                          numPermutations,
        const std::shared_ptr<BackTester<D>>&  templateBackTester,
        const std::shared_ptr<Portfolio<D>>&   portfolio,
        const D&                               pValueSignificanceLevel) override
    {
      std::map<std::shared_ptr<PalStrategy<D>>, D> m;
      for (auto &ctx : strategyData)
	m[ctx.strategy] = D("0.10");
      return m;
    }
  };

  struct EqualPAlgo : IMastersSelectionBiasAlgorithm<D, DummyStatPolicy>
  {
    std::map<std::shared_ptr<PalStrategy<D>>, D> run(
        const std::vector<StrategyContext<D>>& strategyData,
        unsigned long                          numPermutations,
        const std::shared_ptr<BackTester<D>>&  templateBackTester,
        const std::shared_ptr<Portfolio<D>>&   portfolio,
        const D&                               pValueSignificanceLevel) override 
    {
      std::map<std::shared_ptr<PalStrategy<D>>, D> m;
      
      for (auto &ctx : strategyData) m[ctx.strategy] = D("0.05");  // default α
      return m;
  }
};


  struct PartialAlgo : IMastersSelectionBiasAlgorithm<D, DummyStatPolicy>
  {
    std::map<std::shared_ptr<PalStrategy<D>>, D> run(
        const std::vector<StrategyContext<D>>& strategyData,
        unsigned long                          numPermutations,
        const std::shared_ptr<BackTester<D>>&  templateBackTester,
        const std::shared_ptr<Portfolio<D>>&   portfolio,
        const D&                               pValueSignificanceLevel) override
    {
      std::map<std::shared_ptr<PalStrategy<D>>, D> m;
      
      // only return for the first strategy in the list
      if (!strategyData.empty())
 m[strategyData.front().strategy] = D("0.01");
      return m;
    }
  };
  std::shared_ptr<Security<D>> makeTestSecurity() {
  auto ts = std::make_shared<OHLCTimeSeries<D>>(TimeFrame::DAILY, TradingVolume::SHARES, 5);
  for (int i = 0; i < 5; ++i) {
    std::ostringstream date;
    date << "202001" << std::setw(2) << std::setfill('0') << (i + 1);
    ts->addEntry(*createTimeSeriesEntry(date.str(), "100", "105", "95", "102", "1000"));
  }
  return std::make_shared<EquitySecurity<D>>("AAPL", "Apple", ts);
}

PriceActionLabSystem* getSubsetOfPatterns(size_t maxPatterns = 3) {
  auto* fullSystem = getPricePatterns("QQQ_IR.txt");
  auto* subset = new PriceActionLabSystem();

  size_t count = 0;
  for (auto it = fullSystem->allPatternsBegin(); it != fullSystem->allPatternsEnd() && count < maxPatterns; ++it, ++count) {
    subset->addPattern(*it);
  }
  return subset;
}

} // namespace

TEST_CASE("PALMastersMonteCarloValidation handles null base security") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> validator(10, std::make_unique<DummyAlgo>());
  PriceActionLabSystem* dummyPatterns = getRandomPricePatterns();
  REQUIRE_THROWS_AS(validator.runPermutationTests(nullptr, dummyPatterns, DateRange(createDate("20200101"), createDate("20200105"))), PALMastersMonteCarloValidationException);
}

TEST_CASE("PALMastersMonteCarloValidation handles null pattern system") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> validator(10, std::make_unique<DummyAlgo>());
  auto security = makeTestSecurity();
  REQUIRE_THROWS_AS(validator.runPermutationTests(security, nullptr, DateRange(createDate("20200101"), createDate("20200105"))), PALMastersMonteCarloValidationException);
}

TEST_CASE("PALMastersMonteCarloValidation integration with DummyStatPolicy") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> validator(10, std::make_unique<DummyAlgo>());
  auto security = makeTestSecurity();
  auto patterns = getRandomPricePatterns();
  DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());
  validator.runPermutationTests(security, patterns, range);
  REQUIRE(validator.getNumSurvivingStrategies() > 0);
}

TEST_CASE("PALMastersMonteCarloValidation yields expected number of survivors") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> validator(10, std::make_unique<DummyAlgo>());
  auto security = makeTestSecurity();
  auto patterns = getRandomPricePatterns();
  DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());
  validator.runPermutationTests(security, patterns, range);
  auto survivors = validator.getNumSurvivingStrategies();
  REQUIRE(survivors > 0);
}

TEST_CASE("PALMastersMonteCarloValidation does not crash with empty pattern set") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> validator(10, std::make_unique<DummyAlgo>());
  auto security = makeTestSecurity();
  auto emptyPatterns = new PriceActionLabSystem();
  DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());
  validator.runPermutationTests(security, emptyPatterns, range);
  REQUIRE(validator.getNumSurvivingStrategies() == 0);
  delete emptyPatterns;
}

TEST_CASE("PALMastersMonteCarloValidation works with subset of patterns") {
  class FixedPValueAlgo : public IMastersSelectionBiasAlgorithm<D, DummyStatPolicy> {
  public:
    std::map<std::shared_ptr<PalStrategy<D>>, D> run(const std::vector<StrategyContext<D>>& strategyData,
                                                     unsigned long,
                                                     const std::shared_ptr<BackTester<D>>&,
                                                     const std::shared_ptr<Portfolio<D>>&, const D&) override {
      std::map<std::shared_ptr<PalStrategy<D>>, D> result;
      for (const auto& ctx : strategyData) {
        result[ctx.strategy] = D("0.02");
      }
      return result;
    }
  };

  PALMastersMonteCarloValidation<D, DummyStatPolicy> validator(5, std::make_unique<FixedPValueAlgo>());
  auto security = makeTestSecurity();
  auto patterns = getSubsetOfPatterns(3);
  DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());

  validator.runPermutationTests(security, patterns, range);
  REQUIRE(validator.getNumSurvivingStrategies() > 0);
  delete patterns;
}

TEST_CASE("PALMastersMonteCarloValidation ctor rejects zero permutations") {
  REQUIRE_THROWS_AS(
		    (PALMastersMonteCarloValidation<D, DummyStatPolicy>(0)),
    PALMastersMonteCarloValidationException
  );
}

TEST_CASE("Empty-map algorithm yields zero survivors") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> v(10, std::make_unique<EmptyMapAlgo>());
  auto sec      = makeTestSecurity();
  auto pats     = getRandomPricePatterns();
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};
  v.runPermutationTests(sec, pats, r);
  REQUIRE(v.getNumSurvivingStrategies() == 0);
}

TEST_CASE("High-pvalue algorithm rejects all strategies") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> v(10, std::make_unique<HighPAlgo>());
  auto sec  = makeTestSecurity();
  auto pats = getRandomPricePatterns();
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};
  v.runPermutationTests(sec, pats, r);
  REQUIRE(v.getNumSurvivingStrategies() == 0);
}

TEST_CASE("p-value == alpha is accepted") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> v(10, std::make_unique<EqualPAlgo>());
  auto sec  = makeTestSecurity();
  auto pats = getRandomPricePatterns();
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};
  v.runPermutationTests(sec, pats, r);
  REQUIRE(v.getNumSurvivingStrategies() > 0);
}

TEST_CASE("Missing p-values default to 1.0") {
  PALMastersMonteCarloValidation<D, DummyStatPolicy> v(10, std::make_unique<PartialAlgo>());
  auto sec  = makeTestSecurity();
  auto pats = getRandomPricePatterns();
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};
  v.runPermutationTests(sec, pats, r);
  // only the one with p=0.01 survives
  REQUIRE(v.getNumSurvivingStrategies() == 1);
}

TEST_CASE("No strategies found yields zero survivors")
{
  PALMastersMonteCarloValidation<D, DummyStatPolicy> v(10, std::make_unique<DummyAlgo>());
  auto sec          = makeTestSecurity();
  auto emptyPatterns = new PriceActionLabSystem();  // no patterns
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};

  v.runPermutationTests(sec, emptyPatterns, r);
  REQUIRE(v.getNumSurvivingStrategies() == 0);
  delete emptyPatterns;
}
