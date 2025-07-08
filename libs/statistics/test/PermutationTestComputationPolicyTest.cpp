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
#include "ParallelExecutors.h"

using namespace mkc_timeseries;
using DecimalType = DecimalType;  // from TestUtils.h

namespace {
  // --------------------------------------------------------------------------
  // Unit-level: fake policies for determinism
  // --------------------------------------------------------------------------

  // 1) UniformStatPolicy: i.i.d. U(0,1) statistics
  struct UniformStatPolicy {
    static std::mt19937_64 rng;
    static std::uniform_real_distribution<double> dist;

    static unsigned getMinStrategyTrades() { return 0; }
    static DecimalType getPermutationTestStatistic(
      const std::shared_ptr<BackTester<DecimalType>>&)
    {
      return DecimalType{ std::to_string(dist(rng)) };
    }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };
  std::mt19937_64 UniformStatPolicy::rng{12345};
  std::uniform_real_distribution<double> UniformStatPolicy::dist{0.0,1.0};

  using UniformNullTester = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    UniformStatPolicy,
    PValueReturnPolicy<DecimalType>,
    PermutationTestingNullTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>
  >;

  // 2) DummyStatPolicy: always 0.5
  struct DummyStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&){
      return DecimalType("0.5");
    }
    static unsigned getMinStrategyTrades() { return 0; }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };

  // 3) AlwaysLowStatPolicy: always 0.1
  struct AlwaysLowStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&){
      return DecimalType("0.1");
    }
    static unsigned getMinStrategyTrades() { return 0; }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };

  // 4) NoTradesPolicy: never meets minTrades=1
  struct NoTradesPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&){
      return DecimalType("999");
    }
    static unsigned getMinStrategyTrades() { return 1; }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };

  // --------------------------------------------------------------------------
  // Integration-style: a distinct uniform-null policy
  // --------------------------------------------------------------------------

  struct UniformIntegrationNullPolicy
  {
    static unsigned getMinStrategyTrades() { return 0; }
  
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&){
      // Use thread_local to ensure each thread gets its own independent generator,
      // solving the statistical dependence issue while keeping the static interface.
      thread_local static std::mt19937_64 rng(std::random_device{}());
      thread_local static std::uniform_real_distribution<double> dist(0.0, 1.0);

      return DecimalType{ std::to_string(dist(rng)) };
    }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }
  };
  
  using UniformIntegrationTester = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    UniformIntegrationNullPolicy,
    PValueReturnPolicy<DecimalType>,
    PermutationTestingNullTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>
  >;

  // --------------------------------------------------------------------------
  // Minimal dummy backtester + strategy
  // --------------------------------------------------------------------------
  class DummyBackTester : public BackTester<DecimalType> {
  public:
    DummyBackTester() { 
      boost::gregorian::date s(2020,1,1), e(2020,12,31);
      this->addDateRange(DateRange(s,e));
    }
    std::shared_ptr<BackTester<DecimalType>> clone() const override {
      return std::make_shared<DummyBackTester>();
    }
    bool isDailyBackTester()   const override { return true; }
    bool isWeeklyBackTester()  const override { return false; }
    bool isMonthlyBackTester() const override { return false; }
    bool isIntradayBackTester()const override { return false; }
    void backtest() override {}

  protected:
  };

  class DummyPalStrategy : public PalStrategy<DecimalType> {
  public:
    DummyPalStrategy(std::shared_ptr<Portfolio<DecimalType>> p)
      : PalStrategy<DecimalType>("dummy", getDummyPattern(), p, StrategyOptions(false,0)) {}

    static std::shared_ptr<PriceActionLabPattern> getDummyPattern() {
      static std::shared_ptr<PriceActionLabPattern> dummyPattern;
      if (!dummyPattern) {
        // Get a real pattern from the test utility
        auto patterns = getRandomPricePatterns();
        if (patterns && patterns->getNumPatterns() > 0) {
          auto it = patterns->allPatternsBegin();
          dummyPattern = *it;
        }
      }
      return dummyPattern;
    }
    std::shared_ptr<PalStrategy<DecimalType>> clone2(std::shared_ptr<Portfolio<DecimalType>> p) const override {
      return std::make_shared<DummyPalStrategy>(p);
    }
    std::shared_ptr<BacktesterStrategy<DecimalType>> clone(const std::shared_ptr<Portfolio<DecimalType>>& p) const override {
      return std::make_shared<DummyPalStrategy>(p);
    }
    std::shared_ptr<BacktesterStrategy<DecimalType>> cloneForBackTesting() const override {
      return std::make_shared<DummyPalStrategy>(this->getPortfolio());
    }
    void eventExitOrders(Security<DecimalType>*, const InstrumentPosition<DecimalType>&, const boost::posix_time::ptime&) override {}
    void eventEntryOrders(Security<DecimalType>*, const InstrumentPosition<DecimalType>&, const boost::posix_time::ptime&) override {}
  };

  inline auto createDummySecurity() {
    auto ts = getRandomPriceSeries();
    return std::make_shared<EquitySecurity<DecimalType>>("SYM","Dummy",ts);
  }
  inline auto createDummyPortfolio() {
    auto p = std::make_shared<Portfolio<DecimalType>>("Port");
    p->addSecurity(createDummySecurity());
    return p;
  }
}

// ----------------------------------------------------------------------------
// Unit tests
// ----------------------------------------------------------------------------

TEST_CASE("p=1 when statistic always ≥ baseline", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  DefaultPermuteMarketChangesPolicy<DecimalType,DummyStatPolicy> policy;
  auto p = policy.runPermutationTest(bt,1,DecimalType("0.4"));
  REQUIRE(p == DecimalType("1.0"));
}

TEST_CASE("p=(0+1)/(N+1) when statistic always < baseline", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  DefaultPermuteMarketChangesPolicy<DecimalType,AlwaysLowStatPolicy> policy;
  auto p = policy.runPermutationTest(bt,4,DecimalType("0.5"));
  REQUIRE(p == DecimalType("0.2"));
}

TEST_CASE("tuple policy returns both p and summary", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  using T = DefaultPermuteMarketChangesPolicy<
              DecimalType, DummyStatPolicy,
              PValueAndTestStatisticReturnPolicy<DecimalType>,
              PermutationTestingMaxTestStatisticPolicy<DecimalType>
            >;
  T policy;
  auto [p,stat] = policy.runPermutationTest(bt,1,DecimalType("0.4"));
  REQUIRE(p    == DecimalType("1.0"));
  REQUIRE(stat == DecimalType("0.5"));
}

TEST_CASE("max-statistic policy yields correct max", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  using M = DefaultPermuteMarketChangesPolicy<
              DecimalType, DummyStatPolicy,
              PValueAndTestStatisticReturnPolicy<DecimalType>,
              PermutationTestingMaxTestStatisticPolicy<DecimalType>
            >;
  M policy;
  auto [p,stat] = policy.runPermutationTest(bt,5,DecimalType("0.4"));
  REQUIRE(p    == DecimalType("1.0"));
  REQUIRE(stat == DecimalType("0.5"));
}

TEST_CASE("p=1 when no permutations meet minTrades", "[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  using N = DefaultPermuteMarketChangesPolicy<
              DecimalType, NoTradesPolicy,
              PValueAndTestStatisticReturnPolicy<DecimalType>
            >;
  N policy;
  auto [p,stat] = policy.runPermutationTest(bt,10,DecimalType("0"));
  REQUIRE(p    == DecimalType("1.0"));
  REQUIRE(stat == DecimalConstants<DecimalType>::DecimalZero);
}

TEST_CASE("P-values under null uniform policy are approx uniform", "[distribution]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  constexpr uint32_t Nperm = 1000; constexpr int Nruns = 500;
  std::vector<double> pvals; pvals.reserve(Nruns);
  for(int i=0;i<Nruns;++i) {
    auto baseline = UniformStatPolicy::getPermutationTestStatistic(bt);
    UniformNullTester policy;
    auto p = policy.runPermutationTest(bt,Nperm,baseline);
    pvals.push_back(p.getAsDouble());
  }
  double mean = std::accumulate(pvals.begin(), pvals.end(), 0.0) / pvals.size();
  REQUIRE(mean == Catch::Approx(0.5).margin(0.05));
}

// ----------------------------------------------------------------------------
// Integration-style tests
// ----------------------------------------------------------------------------

// In PermutationTestComputationPolicyTest.cpp

TEST_CASE("Integration: p-values under null approx uniform", "[integration]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  constexpr uint32_t Nperm = 500; 
  constexpr int Nruns = 200;
  std::vector<double> pvals; 
  pvals.reserve(Nruns);

  // --- Start of Corrected Code ---
  for(int i = 0; i < Nruns; ++i) {
    // Generate the baseline by calling the static method directly.
    // The thread_local generator inside the method will handle independence.
    auto baseline = UniformIntegrationNullPolicy::getPermutationTestStatistic(bt);

    // The policy object is stateless, as it should be.
    UniformIntegrationTester policy;
    auto p = policy.runPermutationTest(bt, Nperm, baseline);
    pvals.push_back(p.getAsDouble());
  }
  // --- End of Corrected Code ---

  double expectedMean = double(Nperm + 2) / (2.0 * (Nperm + 1));
  double mean = std::accumulate(pvals.begin(), pvals.end(), 0.0) / pvals.size();

  // We calculate the theoretical standard deviation of the *sample mean* to set a robust margin.
  // For a continuous U(0,1) distribution (a good approximation here), variance is 1/12.
  // The standard error of the mean is sqrt(variance / num_samples).
  const double p_value_variance = 1.0 / 12.0;
  const double std_error_of_mean = std::sqrt(p_value_variance / Nruns);
  
  // We check if the observed mean is within 3 standard deviations of the expected mean.
  // This is a standard statistical check that is robust to random fluctuations.
  REQUIRE(mean == Catch::Approx(expectedMean).margin(3.0 * std_error_of_mean));
}

TEST_CASE("ThreadPoolExecutor vs StdAsyncExecutor same output", "[integration]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  constexpr uint32_t Nperm = 20;
  DecimalType baseline("0.5");

  using PoolTester = DefaultPermuteMarketChangesPolicy<
    DecimalType, DummyStatPolicy,
    PValueAndTestStatisticReturnPolicy<DecimalType>,
    PermutationTestingMaxTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>
  >;

  using AsyncTester = DefaultPermuteMarketChangesPolicy<
    DecimalType, DummyStatPolicy,
    PValueAndTestStatisticReturnPolicy<DecimalType>,
    PermutationTestingMaxTestStatisticPolicy<DecimalType>,
    concurrency::StdAsyncExecutor
  >;

  PoolTester poolPolicy;
  AsyncTester asyncPolicy;
  auto r1 = poolPolicy.runPermutationTest(bt, Nperm, baseline);
  auto r2 = asyncPolicy.runPermutationTest(bt, Nperm, baseline);

  REQUIRE(r1 == r2);
}

TEST_CASE("runPermutationTest throws if numPermutations==0","[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  DecimalType baseline("0.0");
  REQUIRE_THROWS_AS(
		    ([&]() {
		      DefaultPermuteMarketChangesPolicy<DecimalType,DummyStatPolicy> policy;
		      return policy.runPermutationTest(bt,0,baseline);
		    }()),
    std::invalid_argument);
}

TEST_CASE("numPermutations==1 yields p=1 or 0.5","[unit]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  // Policy that always gives stat==baseline
  struct EqPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&)
    { return DecimalType("0.5"); }

    static unsigned getMinStrategyTrades(){ return 0; }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };

  // baseline == statistic → p = (1+1)/(1+1) == 1
  DefaultPermuteMarketChangesPolicy<DecimalType,EqPolicy> policy1;
  auto p1 = policy1.runPermutationTest(bt,1,DecimalType("0.5"));
  REQUIRE(p1 == DecimalType("1.0"));
  // Policy that always gives stat < baseline → p = (0+1)/(1+1) == 0.5
  struct LtPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&)
    { return DecimalType("0.1"); }

    static unsigned getMinStrategyTrades(){ return 0; }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };
  DefaultPermuteMarketChangesPolicy<DecimalType,LtPolicy> policy2;
  auto p2 = policy2.runPermutationTest(bt,1,DecimalType("0.5"));
  REQUIRE(p2 == DecimalType("0.5"));
}
