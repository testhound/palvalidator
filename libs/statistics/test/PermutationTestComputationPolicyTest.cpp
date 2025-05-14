#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <memory>
#include <vector>
#include <tuple>
#include <random>
#include <numeric>
#include "PermutationTestComputationPolicy.h"
#include "TestUtils.h"
#include "Security.h"
#include "BackTester.h"
#include "PalStrategy.h"
#include "DecimalConstants.h"

using namespace mkc_timeseries;
using DecimalType = DecimalType;  // from TestUtils.h

namespace {
  // Distribution test: uniform null policy (unit-level)
  struct UniformStatPolicy {
    static std::mt19937_64 rng;
    static std::uniform_real_distribution<double> dist;

    static unsigned getMinStrategyTrades() { return 0; }
    static DecimalType getPermutationTestStatistic(
        const std::shared_ptr<BackTester<DecimalType>>&)
    {
      return DecimalType{ std::to_string(dist(rng)) };
    }
  };
  std::mt19937_64 UniformStatPolicy::rng{12345};
  std::uniform_real_distribution<double> UniformStatPolicy::dist{0.0, 1.0};

  using UniformNullTester = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    UniformStatPolicy,
    PValueReturnPolicy<DecimalType>,
    PermutationTestingNullTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>
  >;

  // Integration test: separate null policy to avoid name collisions
  struct UniformIntegrationNullPolicy {
    static std::mt19937_64 rng;
    static std::uniform_real_distribution<double> dist;

    static unsigned getMinStrategyTrades() { return 0; }
    static DecimalType getPermutationTestStatistic(
        const std::shared_ptr<BackTester<DecimalType>>&)
    {
      return DecimalType{ std::to_string(dist(rng)) };
    }
  };
  std::mt19937_64 UniformIntegrationNullPolicy::rng{987654};
  std::uniform_real_distribution<double> UniformIntegrationNullPolicy::dist{0.0, 1.0};

  using UniformIntegrationTester = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    UniformIntegrationNullPolicy,
    PValueReturnPolicy<DecimalType>,
    PermutationTestingNullTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>
  >;

  // Policy that returns a fixed statistic of 0.5
  struct DummyStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.5");
    }
    static unsigned getMinStrategyTrades() { return 0; }
  };

  // Policy that always returns a low statistic of 0.1
  struct AlwaysLowStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.1");
    }
    static unsigned getMinStrategyTrades() { return 0; }
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
    DummyPalStrategy(std::shared_ptr<Portfolio<DecimalType>> p)
      : PalStrategy<DecimalType>("dummy", nullptr, p, StrategyOptions(false,0)) {}
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

  // Helpers for dummy portfolio
  std::shared_ptr<Security<DecimalType>> createDummySecurity() {
    auto ts = getRandomPriceSeries();
    return std::make_shared<EquitySecurity<DecimalType>>("SYM","Dummy",ts);
  }
  std::shared_ptr<Portfolio<DecimalType>> createDummyPortfolio() {
    auto p = std::make_shared<Portfolio<DecimalType>>("Port");
    p->addSecurity(createDummySecurity());
    return p;
  }
}

TEST_CASE("DefaultPermuteMarketChangesPolicy returns p=1 when statistic always ≥ baseline", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  DecimalType baseline("0.4"); uint32_t numPerms = 1;
  auto pValue = DefaultPermuteMarketChangesPolicy<DecimalType, DummyStatPolicy>::runPermutationTest(bt,numPerms,baseline);
  REQUIRE(pValue == DecimalType("1.0"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy returns small p-value when statistic always < baseline", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  DecimalType baseline("0.5"); uint32_t numPerms = 4;
  auto pValue = DefaultPermuteMarketChangesPolicy<DecimalType, AlwaysLowStatPolicy>::runPermutationTest(bt,numPerms,baseline);
  REQUIRE(pValue == DecimalType("0.2"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy with tuple return policy returns both p and summary", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  DecimalType baseline("0.4"); uint32_t numPerms = 1;
  using TuplePolicy = DefaultPermuteMarketChangesPolicy<DecimalType, DummyStatPolicy,PValueAndTestStatisticReturnPolicy<DecimalType>,PermutationTestingMaxTestStatisticPolicy<DecimalType>>;
  auto [pValue,summaryStat] = TuplePolicy::runPermutationTest(bt,numPerms,baseline);
  REQUIRE(pValue == DecimalType("1.0"));
  REQUIRE(summaryStat == DecimalType("0.5"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy with max-statistic collection yields correct max", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  DecimalType baseline("0.4"); uint32_t numPerms = 5;
  using MaxPolicy = DefaultPermuteMarketChangesPolicy<DecimalType, DummyStatPolicy,PValueAndTestStatisticReturnPolicy<DecimalType>,PermutationTestingMaxTestStatisticPolicy<DecimalType>>;
  auto [pValue,maxStat] = MaxPolicy::runPermutationTest(bt,numPerms,baseline);
  REQUIRE(pValue == DecimalType("1.0"));
  REQUIRE(maxStat == DecimalType("0.5"));
}

TEST_CASE("P-values under null uniform policy are approx uniform", "[distribution]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  const uint32_t Nperm = 1000; const int Nruns = 500;
  std::vector<double> pvals; pvals.reserve(Nruns);
  for(int i=0;i<Nruns;++i) {
    DecimalType baseline = UniformStatPolicy::getPermutationTestStatistic(bt);
    auto p = UniformNullTester::runPermutationTest(bt,Nperm,baseline);
    pvals.push_back(p.getAsDouble());
  }
  double mean = std::accumulate(pvals.begin(),pvals.end(),0.0)/Nruns;
  REQUIRE(mean == Catch::Approx(0.5).margin(0.05));
}

TEST_CASE("Integration: p-values under null approx uniform", "[integration]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  const uint32_t Nperm = 500; const int Nruns = 200;
  std::vector<double> pvals; pvals.reserve(Nruns);
  for(int i=0;i<Nruns;++i) {
    DecimalType baseline = UniformIntegrationNullPolicy::getPermutationTestStatistic(bt);
    auto p = UniformIntegrationTester::runPermutationTest(bt,Nperm,baseline);
    pvals.push_back(p.getAsDouble());
  }
  double expectedMean = double(Nperm + 2)/(2.0*(Nperm + 1));
  double mean = std::accumulate(pvals.begin(), pvals.end(), 0.0)/pvals.size();
  REQUIRE(mean == Catch::Approx(expectedMean).margin(0.05));
}
