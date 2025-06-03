// MonteCarloPermuteMarketChangesTest.cpp

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <utility>
#include <boost/date_time.hpp>

#include "MonteCarloPermutationTest.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "Security.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;  // bring in BackTester, PalStrategy, EquitySecurity, etc.

namespace {

//------------------------------------------------------------------------------
// Minimal DummyBackTester & DummyPalStrategy (see PermutationTestComputationPolicyTest.cpp)
//------------------------------------------------------------------------------
class DummyBackTester : public BackTester<DecimalType> {
public:
    DummyBackTester() {
        boost::gregorian::date s(2020,1,1), e(2020,12,31);
        this->addDateRange(DateRange(s,e));
    }
    std::shared_ptr<BackTester<DecimalType>> clone() const override {
        return std::make_shared<DummyBackTester>();
    }
    bool isDailyBackTester()    const override { return true; }
    bool isWeeklyBackTester()   const override { return false; }
    bool isMonthlyBackTester()  const override { return false; }
    bool isIntradayBackTester() const override { return false; }
    void backtest() override {}
protected:
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const override { return d; }
    TimeSeriesDate next_period    (const TimeSeriesDate& d) const override { return d; }
};

class DummyPalStrategy : public PalStrategy<DecimalType> {
public:
    explicit DummyPalStrategy(std::shared_ptr<Portfolio<DecimalType>> p)
      : PalStrategy<DecimalType>("dummy", getDummyPattern(), p, StrategyOptions(false,0)) {}

    static std::shared_ptr<PriceActionLabPattern> getDummyPattern() {
      static std::shared_ptr<PriceActionLabPattern> dummyPattern;
      if (!dummyPattern) {
        // Get a real pattern from the test utility
        PriceActionLabSystem* patterns = getRandomPricePatterns();
        if (patterns && patterns->getNumPatterns() > 0) {
          auto it = patterns->allPatternsBegin();
          dummyPattern = *it;
        }
      }
      return dummyPattern;
    }
    std::shared_ptr<PalStrategy<DecimalType>>
    clone2(std::shared_ptr<Portfolio<DecimalType>> p) const override {
        return std::make_shared<DummyPalStrategy>(p);
    }
    std::shared_ptr<BacktesterStrategy<DecimalType>>
    clone(const std::shared_ptr<Portfolio<DecimalType>>& p) const override {
        return std::make_shared<DummyPalStrategy>(p);
    }
    std::shared_ptr<BacktesterStrategy<DecimalType>>
    cloneForBackTesting() const override {
        return std::make_shared<DummyPalStrategy>(this->getPortfolio());
    }
    void eventExitOrders(Security<DecimalType>*, const InstrumentPosition<DecimalType>&, const boost::posix_time::ptime&) override {}
    void eventEntryOrders(Security<DecimalType>*, const InstrumentPosition<DecimalType>&, const boost::posix_time::ptime&) override {}
};

inline auto createDummySecurity() {
    auto ts = getRandomPriceSeries();
    return std::make_shared<EquitySecurity<DecimalType>>("SYM", "Dummy", ts);
}

inline auto createDummyPortfolio() {
    auto p = std::make_shared<Portfolio<DecimalType>>("Port");
    p->addSecurity(createDummySecurity());
    return p;
}

//------------------------------------------------------------------------------
// Stub policies for determinism
//------------------------------------------------------------------------------
template <class D>
struct StubBackTestResultPolicy {
    static unsigned getMinStrategyTrades() { return 0; }
    static D getPermutationTestStatistic(const std::shared_ptr<BackTester<D>>&) {
        return DecimalConstants<D>::DecimalOne;
    }

  static D getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<D>::DecimalZero;
    }

};

template <class D>
struct StubComputationPolicy : public PermutationTestSubject<D> {
    using ReturnType = std::pair<D, uint32_t>;
    static ReturnType runPermutationTest(
        const std::shared_ptr<BackTester<D>>&,
        uint32_t numPermutations,
        const D& baselineTestStat)
    {
        return std::make_pair(baselineTestStat, numPermutations);
    }
};

} // namespace

//------------------------------------------------------------------------------
// Constructor‐validation
//------------------------------------------------------------------------------
TEST_CASE("MonteCarloPermuteMarketChanges constructor enforces valid inputs",
          "[unit][MonteCarloPermuteMarketChanges]") 
{
    using TestMC = MonteCarloPermuteMarketChanges<
        DecimalType,
        StubBackTestResultPolicy,
        StubComputationPolicy<DecimalType>
    >;

    // (1) no strategies → exception
    auto btEmpty = std::make_shared<DummyBackTester>();
    REQUIRE_THROWS_AS(TestMC(btEmpty, 10), MonteCarloPermutationException);

    // prepare a BT with exactly one strategy
    auto bt1 = std::make_shared<DummyBackTester>();
    bt1->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

    // (2) numPermutations == 0 → exception
    REQUIRE_THROWS_AS(TestMC(bt1, 0), MonteCarloPermutationException);

    // (3) numPermutations < 10 → exception
    REQUIRE_THROWS_AS(TestMC(bt1, 9), MonteCarloPermutationException);

    // (4) more than one strategy → exception
    auto bt2 = std::make_shared<DummyBackTester>();
    bt2->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
    bt2->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
    REQUIRE_THROWS_AS(TestMC(bt2, 10), MonteCarloPermutationException);

    // (5) exactly one strategy & ≥10 permutations → ok
    REQUIRE_NOTHROW(TestMC(bt1, 10));
}

//------------------------------------------------------------------------------
// runPermutationTest → passes through stub values
//------------------------------------------------------------------------------
TEST_CASE("runPermutationTest returns expected values from stub policies",
          "[unit][MonteCarloPermuteMarketChanges]") 
{
    using TestMC = MonteCarloPermuteMarketChanges<
        DecimalType,
        StubBackTestResultPolicy,
        StubComputationPolicy<DecimalType>
    >;

    auto bt = std::make_shared<DummyBackTester>();
    bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

    constexpr uint32_t perms = 42;
    TestMC mc(bt, perms);

    auto result = mc.runPermutationTest();
    REQUIRE(result.first  == DecimalConstants<DecimalType>::DecimalOne);
    REQUIRE(result.second == perms);
}
