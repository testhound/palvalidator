// PALMonteCarloValidationTest.cpp
#include <catch2/catch_test_macros.hpp>
#include "ParallelExecutors.h" 
#include "PALMonteCarloValidation.h"
#include "Security.h"
#include "Portfolio.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using D = DecimalType;

namespace {

  struct MixedMcpt
  {
    using ResultType = D;

    // must exactly match the ctor signature that PALMonteCarloValidation::runPermutationTests will use
    MixedMcpt(const std::shared_ptr<BackTester<D>>& /*bt*/,
	      unsigned long                         /*numPermutations*/)
    {}

    // no-arg runPermutationTest() so the static_assert and call site line up
    ResultType runPermutationTest() {
      // return whichever p-value you want here.
      // e.g. always 0.01, or use a static counter if you really need
      // first call -> 0.01, second -> 0.05, third -> 0.10, etc.
      // But for now, returning one of {D("0.01"), D("0.05"), D("0.10")} is fine:
      static int call = 0;
      D out;
      if      (call == 0) out = D("0.01");
      else if (call == 1) out = D("0.05");
      else                out = D("0.10");
      ++call;
      return out;
    }
  };
  
  struct OneMcpt {
    using ResultType = D;

    OneMcpt(const std::shared_ptr<BackTester<D>>& /*bt*/, unsigned long /*numPermutations*/) {} 
    ResultType runPermutationTest(...) { return D("0.05"); }
  };

  // Dummy MCPT type: always returns a small p-value (0.01)
  struct DummyMcpt {
    using ResultType = D;

    DummyMcpt(const std::shared_ptr<BackTester<D>>& /*bt*/, unsigned long /*numPermutations*/) {}
    ResultType runPermutationTest() { return D("0.01"); }
  };

  /// MCPT stub that always returns p == alpha exactly
  struct EqualMcpt {
    using ResultType = D;

    EqualMcpt(const std::shared_ptr<BackTester<D>>& /*bt*/, unsigned long /*numPermutations*/) {}
    ResultType runPermutationTest() {
      // must match the alpha you pass in tests
      return D("0.05");
    }
  };

  struct ExplodingMcpt {
    using ResultType = D;
    ExplodingMcpt(const std::shared_ptr<BackTester<D>>& /*bt*/, unsigned long /*numPermutations*/) {}
    ResultType runPermutationTest(...)
    {
      throw PALMonteCarloValidationException("MCPT failed");
    }
  };

  // Create a simple test security with 5 daily entries
  std::shared_ptr<Security<D>> makeTestSecurity() {
    auto ts = std::make_shared<OHLCTimeSeries<D>>(TimeFrame::DAILY, TradingVolume::SHARES, 5);
    for (int i = 0; i < 5; ++i) {
      std::ostringstream date;
      date << "202001" << std::setw(2) << std::setfill('0') << (i + 1);
      ts->addEntry(*createTimeSeriesEntry(date.str(), "100", "105", "95", "102", "1000"));
    }
    return std::make_shared<EquitySecurity<D>>("AAPL", "Apple", ts);
  }

  // Limit patterns to a subset of size maxPatterns
  std::shared_ptr<PriceActionLabSystem> getSubsetOfPatterns(size_t maxPatterns = 3) {
    auto fullSystem = getPricePatterns("QQQ_IR.txt");
    auto subset = std::make_shared<PriceActionLabSystem>();
    size_t count = 0;
    for (auto it = fullSystem->allPatternsBegin(); it != fullSystem->allPatternsEnd() && count < maxPatterns; ++it, ++count) {
      subset->addPattern(*it);
    }
    return subset;
  }


  struct ThrowingMcpt
  {
    using ResultType = D;
    // match the MCPT constructor signature
    ThrowingMcpt(const std::shared_ptr<BackTester<D>>& /*bt*/,
                 unsigned long /*numPermutations*/) 
    {}

    // must return ResultType and throw
    ResultType runPermutationTest() {
      throw PALMonteCarloValidationException("boom");
    }
  };

} // anonymous namespace

TEST_CASE("PALMonteCarloValidation handles null base security") {
    PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> validator(10);
    auto patterns = getRandomPricePatterns();
    REQUIRE_THROWS_AS(
        validator.runPermutationTests(nullptr, patterns,
            DateRange(createDate("20200101"), createDate("20200105"))),
        std::invalid_argument);  // Throws on null baseSecurity citeturn0file1
}

TEST_CASE("PALMonteCarloValidation handles null pattern system") {
    PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> validator(10);
    auto security = makeTestSecurity();
    REQUIRE_THROWS_AS(
		      (validator.runPermutationTests(security, nullptr,
						     DateRange(createDate("20200101"), createDate("20200105")))),
        std::invalid_argument);  // Throws on null patterns
}

TEST_CASE("PALMonteCarloValidation integration test") {
    PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> validator(10);
    auto security = makeTestSecurity();
    auto patterns = getRandomPricePatterns();
    DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());
    // Should not throw
    validator.runPermutationTests(security, patterns, range);
    REQUIRE(validator.getNumSurvivingStrategies() > 0);
}

TEST_CASE("PALMonteCarloValidation yields expected number of survivors") {
    PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> validator(10);
    auto security = makeTestSecurity();
    auto patterns = getRandomPricePatterns();
    DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());
    validator.runPermutationTests(security, patterns, range);
    auto survivors = validator.getNumSurvivingStrategies();
    REQUIRE(survivors > 0);
}

TEST_CASE("PALMonteCarloValidation does not crash with empty pattern set") {
    PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> validator(10);
    auto security = makeTestSecurity();
    auto emptyPatterns = std::make_shared<PriceActionLabSystem>();
    DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());
    validator.runPermutationTests(security, emptyPatterns, range);
    REQUIRE(validator.getNumSurvivingStrategies() == 0);
}

TEST_CASE("PALMonteCarloValidation works with subset of patterns") {
    PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> validator(5);
    auto security = makeTestSecurity();
    auto patterns = getSubsetOfPatterns(3);
    DateRange range(security->getTimeSeries()->getFirstDate(), security->getTimeSeries()->getLastDate());
    validator.runPermutationTests(security, patterns, range);
    REQUIRE(validator.getNumSurvivingStrategies() > 0);
}

TEST_CASE("PALMonteCarloValidation ctor rejects zero permutations")
{
  REQUIRE_THROWS_AS(
    (PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection>(0)),
    std::invalid_argument
  );
}

TEST_CASE("No survivors before running")
{
  PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> v(5);
  REQUIRE(v.getNumSurvivingStrategies() == 0);
  REQUIRE(v.beginSurvivingStrategies() == v.endSurvivingStrategies());
}

TEST_CASE("PALMonteCarloValidation with custom alpha rejects all at low threshold") {
  PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> v(1);
  auto sec = makeTestSecurity();
  auto pats = getSubsetOfPatterns(3);
  DateRange r{sec->getTimeSeries()->getFirstDate(), sec->getTimeSeries()->getLastDate()};
  // DummyMcpt returns p=0.01, so alpha=0.005 kills all
  v.runPermutationTests(sec, pats, r, D("0.005"));
  REQUIRE(v.getNumSurvivingStrategies() == 0);
}

TEST_CASE("PALMonteCarloValidation with custom alpha accepts all at high threshold") {
  PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> v(1);
  auto sec = makeTestSecurity();
  auto pats = getSubsetOfPatterns(3);
  DateRange r{sec->getTimeSeries()->getFirstDate(), sec->getTimeSeries()->getLastDate()};
  // alpha=0.02 accepts all p=0.01
  v.runPermutationTests(sec, pats, r, D("0.02"));
  REQUIRE(v.getNumSurvivingStrategies() == 3);
}

TEST_CASE("PALMonteCarloValidation survivors == num patterns for DummyMcpt") {
  PALMonteCarloValidation<D, DummyMcpt, UnadjustedPValueStrategySelection> v(3);
  auto sec = makeTestSecurity();
  auto pats = getSubsetOfPatterns(4);
  DateRange r{sec->getTimeSeries()->getFirstDate(), sec->getTimeSeries()->getLastDate()};
  v.runPermutationTests(sec, pats, r);
  REQUIRE(v.getNumSurvivingStrategies() == 4);
}

TEST_CASE("PALMonteCarloValidation surfaces MCPT exceptions") {
  PALMonteCarloValidation<D, ThrowingMcpt, UnadjustedPValueStrategySelection> v(2);
  auto sec = makeTestSecurity();
  auto pats = getSubsetOfPatterns(2);
  DateRange r{sec->getTimeSeries()->getFirstDate(), sec->getTimeSeries()->getLastDate()};
  REQUIRE_THROWS_AS(v.runPermutationTests(sec, pats, r), PALMonteCarloValidationException);
}

TEST_CASE("Rerun resets survivors (inclusive boundary)") {
  PALMonteCarloValidation<D, EqualMcpt, UnadjustedPValueStrategySelection> v(1);
  auto sec  = makeTestSecurity();
  auto pats = getSubsetOfPatterns(2);
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};
  v.runPermutationTests(sec, pats, r, D("0.05"));
  auto firstCount = v.getNumSurvivingStrategies();
  v.runPermutationTests(sec, pats, r, D("0.05"));
  REQUIRE(v.getNumSurvivingStrategies() == firstCount);
}

TEST_CASE("Default alpha is SignificantPValue and inclusive") {
  PALMonteCarloValidation<D, OneMcpt, UnadjustedPValueStrategySelection> v(1);
  auto sec  = makeTestSecurity();
  auto pats = getSubsetOfPatterns(1);
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};
  // omit alpha param => uses default 0.05 inclusive
  v.runPermutationTests(sec, pats, r);
  REQUIRE(v.getNumSurvivingStrategies() == 1);
}

TEST_CASE("p-value == alpha is accepted (inclusive boundary)") {
  PALMonteCarloValidation<D, EqualMcpt, UnadjustedPValueStrategySelection> v(1);
  auto sec  = makeTestSecurity();
  auto pats = getSubsetOfPatterns(2);
  DateRange r{sec->getTimeSeries()->getFirstDate(),
              sec->getTimeSeries()->getLastDate()};
  v.runPermutationTests(sec, pats, r, D("0.05"));
  // both patterns return 0.05 => both should survive
  REQUIRE(v.getNumSurvivingStrategies() == 2);
}

TEST_CASE("Mixed p-values: survivors are < or = alpha (sequential)") {
    // MixedMcpt is not thread safe so use SingleThreadExecutor
    using SeqValidator = PALMonteCarloValidation<
        D,
        MixedMcpt,
        UnadjustedPValueStrategySelection,
        concurrency::SingleThreadExecutor
    >;

    SeqValidator v(3);
    auto sec = makeTestSecurity();
    auto pats = getSubsetOfPatterns(3);
    DateRange r{sec->getTimeSeries()->getFirstDate(), sec->getTimeSeries()->getLastDate()};

    v.runPermutationTests(sec, pats, r);
    // now always runs in order: call==0 → 0.01, call==1 → 0.05, call==2 → 0.10
    REQUIRE(v.getNumSurvivingStrategies() == 2);

}


