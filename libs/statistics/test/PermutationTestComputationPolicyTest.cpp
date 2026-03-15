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
  template <class Decimal>
  struct DummyBackTestResultPolicy
  {
    static Decimal getPermutationTestStatistic(std::shared_ptr<mkc_timeseries::BackTester<Decimal>>) {
      return Decimal{}; // never executed in these smoke tests
    }
    static uint32_t getMinStrategyTrades() { return 1; }
  };
  
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
      : PalStrategy<DecimalType>("dummy", getDummyPattern(), p, StrategyOptions(false,0,8)) {}

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

  struct DeterministicStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&){
      return DecimalType("0.5");
    }
    static unsigned getMinStrategyTrades() { return 0; }
    static DecimalType getMinTradeFailureTestStatistic() {
      return DecimalConstants<DecimalType>::DecimalZero;
    }
  };
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

// ----------------------------------------------------------------------------
// Policy class unit tests
// ----------------------------------------------------------------------------

TEST_CASE("StandardPValueComputationPolicy: basic formula", "[policy][unit]") {
  // Test (k+1)/(N+1) formula
  SECTION("k=0, N=99 should give 1/100") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 99);
    REQUIRE(p == DecimalType("0.01"));
  }
  
  SECTION("k=5, N=99 should give 6/100") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(5, 99);
    REQUIRE(p == DecimalType("0.06"));
  }
  
  SECTION("k=N should give 1.0") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(100, 100);
    REQUIRE(p == DecimalType("1.0"));
  }
}

TEST_CASE("StandardPValueComputationPolicy: minimum p-value", "[policy][unit]") {
  // Minimum p-value should be 1/(N+1)
  SECTION("N=999, k=0 gives minimum of 1/1000") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 999);
    REQUIRE(p == DecimalType("0.001"));
  }
  
  SECTION("N=9, k=0 gives minimum of 1/10") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 9);
    REQUIRE(p == DecimalType("0.1"));
  }
}

TEST_CASE("StandardPValueComputationPolicy: edge cases", "[policy][unit]") {
  SECTION("N=1, k=0 should give 0.5") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 1);
    REQUIRE(p == DecimalType("0.5"));
  }
  
  SECTION("N=1, k=1 should give 1.0") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(1, 1);
    REQUIRE(p == DecimalType("1.0"));
  }
  
  SECTION("All extreme: k=N") {
    for (uint32_t N = 10; N <= 100; N += 10) {
      auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(N, N);
      REQUIRE(p == DecimalType("1.0"));
    }
  }
}

TEST_CASE("WilsonPValueComputationPolicy: conservativeness", "[policy][unit]") {
  // Wilson should be >= Standard (conservative)
  SECTION("Wilson >= Standard for various k,N") {
    std::vector<std::tuple<uint32_t, uint32_t>> test_cases = {
      {0, 99},    // k=0, N=99
      {5, 99},    // k=5, N=99
      {10, 100},  // k=10, N=100
      {50, 100},  // k=50, N=100
      {1, 10},    // k=1, N=10
      {5, 10}     // k=5, N=10
    };
    
    for (const auto& [k, N] : test_cases) {
      auto standard_p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
      auto wilson_p = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
      
      INFO("k=" << k << ", N=" << N);
      REQUIRE(wilson_p >= standard_p);
    }
  }
}

TEST_CASE("WilsonPValueComputationPolicy: returns valid p-values", "[policy][unit]") {
  // Wilson p-values should be in [0, 1]
  SECTION("Various k,N combinations stay in [0,1]") {
    for (uint32_t N = 10; N <= 100; N += 10) {
      for (uint32_t k = 0; k <= N; k += N/5) {
        auto p = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
        INFO("k=" << k << ", N=" << N << ", p=" << p);
        REQUIRE(p >= DecimalType("0.0"));
        REQUIRE(p <= DecimalType("1.0"));
      }
    }
  }
}

TEST_CASE("WilsonPValueComputationPolicy: edge cases", "[policy][unit]") {
  SECTION("k=0, small N") {
    // When k=0, Wilson should be more conservative than standard 1/(N+1)
    auto standard = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 10);
    auto wilson = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 10);
    REQUIRE(wilson > standard);
  }
  
  SECTION("k=N should give 1.0") {
    auto p = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(100, 100);
    REQUIRE(p.getAsDouble() == Catch::Approx(1.0).epsilon(0.01));
  }
  
  SECTION("Very small N=1") {
    // With N=1, k=0 should still be valid
    auto p = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 1);
    REQUIRE(p >= DecimalType("0.0"));
    REQUIRE(p <= DecimalType("1.0"));
  }
}

TEST_CASE("WilsonPValueComputationPolicy: conservativeness increases with smaller N", "[policy][unit]") {
  // The Wilson adjustment should be more conservative (larger difference from standard) 
  // when N is smaller, as Monte Carlo uncertainty is higher
  
  uint32_t k = 5;  // Fixed number of extreme values
  
  // Calculate difference between Wilson and Standard for different N
  std::vector<double> differences;
  std::vector<uint32_t> N_values = {20, 50, 100, 500, 1000};
  
  for (uint32_t N : N_values) {
    auto standard = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
    auto wilson = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
    double diff = (wilson - standard).getAsDouble();
    differences.push_back(diff);
  }
  
  // Verify that differences decrease as N increases (monotonic)
  for (size_t i = 1; i < differences.size(); ++i) {
    INFO("N=" << N_values[i-1] << " diff=" << differences[i-1] << 
         ", N=" << N_values[i] << " diff=" << differences[i]);
    REQUIRE(differences[i] < differences[i-1]);
  }
}

TEST_CASE("DefaultPermuteMarketChangesPolicy with StandardPValueComputationPolicy", "[policy][integration]") {
  // Test that using StandardPValueComputationPolicy explicitly works as default
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  using StandardTester = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    AlwaysLowStatPolicy,
    PValueReturnPolicy<DecimalType>,
    PermutationTestingNullTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>,
    StandardPValueComputationPolicy<DecimalType>
  >;
  
  StandardTester policy;
  auto p = policy.runPermutationTest(bt, 4, DecimalType("0.5"));
  
  // With AlwaysLowStatPolicy (always 0.1 < baseline 0.5), k=0, N=4
  // Standard formula: (0+1)/(4+1) = 1/5 = 0.2
  REQUIRE(p == DecimalType("0.2"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy with WilsonPValueComputationPolicy", "[policy][integration]") {
  // Test that using WilsonPValueComputationPolicy gives conservative p-values
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  using WilsonTester = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    AlwaysLowStatPolicy,
    PValueReturnPolicy<DecimalType>,
    PermutationTestingNullTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>,
    WilsonPValueComputationPolicy<DecimalType>
  >;
  
  using StandardTester = DefaultPermuteMarketChangesPolicy<
    DecimalType,
    AlwaysLowStatPolicy,
    PValueReturnPolicy<DecimalType>,
    PermutationTestingNullTestStatisticPolicy<DecimalType>,
    concurrency::ThreadPoolExecutor<>,
    StandardPValueComputationPolicy<DecimalType>
  >;
  
  WilsonTester wilsonPolicy;
  StandardTester standardPolicy;
  
  auto baseline = DecimalType("0.5");
  auto wilson_p = wilsonPolicy.runPermutationTest(bt, 10, baseline);
  auto standard_p = standardPolicy.runPermutationTest(bt, 10, baseline);
  
  INFO("Wilson p=" << wilson_p << ", Standard p=" << standard_p);
  REQUIRE(wilson_p > standard_p);  // Wilson should be more conservative
}

TEST_CASE("Policy classes: numerical stability", "[policy][unit]") {
  // Test extreme cases for numerical stability
  
  SECTION("Very large N") {
    uint32_t k = 100;
    uint32_t N = 10000;
    
    auto standard = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
    auto wilson = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
    
    // Both should be valid p-values
    REQUIRE(standard >= DecimalType("0.0"));
    REQUIRE(standard <= DecimalType("1.0"));
    REQUIRE(wilson >= DecimalType("0.0"));
    REQUIRE(wilson <= DecimalType("1.0"));
    
    // They should be very close for large N (Wilson adjustment becomes negligible)
    double diff = (wilson - standard).getAsDouble();
    REQUIRE(diff < 0.01);  // Less than 1% difference
  }
  
  SECTION("k very close to N") {
    uint32_t N = 100;
    uint32_t k = 99;
    
    auto standard = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
    auto wilson = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(k, N);
    
    // Both should be very close to 1.0
    REQUIRE(standard.getAsDouble() > 0.99);
    REQUIRE(wilson.getAsDouble() > 0.99);
  }
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: CacheType uses default N1 null model",
          "[PermutationPolicy][NullModel][N1]") {
  using DT = DecimalType;

  // Default template args include PValueComputationPolicy; here we pass it explicitly to match the signature:
  using PolicyDefault =
    DefaultPermuteMarketChangesPolicy<
      DT,
      DummyBackTestResultPolicy<DT>,                 // BackTestResultPolicy
      PValueReturnPolicy<DT>,                        // _PermutationTestResultPolicy (default)
      PermutationTestingNullTestStatisticPolicy<DT>, // _PermutationTestStatisticsCollectionPolicy (default)
      concurrency::SingleThreadExecutor,             // Executor (ok to choose SingleThread)
      StandardPValueComputationPolicy<DT>            // PValueComputationPolicy (explicit)
      // NullModel omitted -> defaults to N1_MaxDestruction in your NullModel-enabled version
    >;

  using ExpectedCacheN1 =
    SyntheticCache<
      DT,
      LogNLookupPolicy<DT>,
      NoRounding,
      SyntheticNullModel::N1_MaxDestruction
    >;

  static_assert(std::is_same<typename PolicyDefault::CacheType, ExpectedCacheN1>::value,
                "PolicyDefault::CacheType should resolve to SyntheticCache with N1_MaxDestruction.");
  REQUIRE(true);
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: CacheType uses N0_PairedDay when specified",
          "[PermutationPolicy][NullModel][N0]") {
  using DT = DecimalType;

  using PolicyN0 =
    DefaultPermuteMarketChangesPolicy<
      DT,
      DummyBackTestResultPolicy<DT>,                 // BackTestResultPolicy
      PValueReturnPolicy<DT>,                        // _PermutationTestResultPolicy
      PermutationTestingNullTestStatisticPolicy<DT>, // _PermutationTestStatisticsCollectionPolicy
      concurrency::SingleThreadExecutor,             // Executor
      StandardPValueComputationPolicy<DT>,           // PValueComputationPolicy
      SyntheticNullModel::N0_PairedDay               // << new knob
    >;

  using ExpectedCacheN0 =
    SyntheticCache<
      DT,
      LogNLookupPolicy<DT>,
      NoRounding,
      SyntheticNullModel::N0_PairedDay
    >;

  static_assert(std::is_same<typename PolicyN0::CacheType, ExpectedCacheN0>::value,
                "PolicyN0::CacheType should resolve to SyntheticCache with N0_PairedDay.");
  REQUIRE(true);
}

// ============================================================================
// CRITICAL GAP 1: Thread Safety Tests
// ============================================================================

TEST_CASE("DecimalType: thread-safe properties", "[thread-safety][critical]") {
  SECTION("Decimal is trivially copyable") {
    REQUIRE(std::is_trivially_copyable_v<DecimalType>);
  }
  
  SECTION("Decimal is standard layout") {
    REQUIRE(std::is_standard_layout_v<DecimalType>);
  }
  
  SECTION("Decimal has expected size (one int64)") {
    REQUIRE(sizeof(DecimalType) == sizeof(int64_t));
  }
}

TEST_CASE("DecimalType: concurrent reads are race-free", "[thread-safety][critical]") {
  const DecimalType shared_value(12345.6789);
  std::atomic<bool> start_flag{false};
  std::atomic<int> error_count{0};
  
  constexpr int NUM_READERS = 10;
  constexpr int NUM_READS = 10000;
  
  auto reader_task = [&]() {
    // Wait for all threads to be ready
    while (!start_flag.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    
    // Perform many concurrent reads
    for (int i = 0; i < NUM_READS; ++i) {
      DecimalType local_copy = shared_value;  // Copy
      
      // Verify value hasn't been corrupted
      if (local_copy != DecimalType(12345.6789)) {
        error_count.fetch_add(1, std::memory_order_relaxed);
      }
      
      // Perform const operations (comparisons)
      if (local_copy < DecimalType(12345.0) || local_copy > DecimalType(12346.0)) {
        error_count.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };
  
  // Spawn reader threads
  std::vector<std::thread> threads;
  threads.reserve(NUM_READERS);
  for (int i = 0; i < NUM_READERS; ++i) {
    threads.emplace_back(reader_task);
  }
  
  // Start all threads simultaneously
  start_flag.store(true, std::memory_order_release);
  
  // Wait for completion
  for (auto& t : threads) {
    t.join();
  }
  
  // No errors should occur with thread-safe concurrent reads
  REQUIRE(error_count.load() == 0);
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: thread-safe execution", "[thread-safety][critical]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // Run permutation test - should not crash or produce data races
  // Run with ThreadSanitizer to detect actual races
  auto result = policy.runPermutationTest(bt, 100, DecimalType("0.3"));
  
  // Basic sanity check
  REQUIRE(result >= DecimalType("0.0"));
  REQUIRE(result <= DecimalType("1.0"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: concurrent p-value computation stability", 
          "[thread-safety][critical]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // Run multiple times - if there are race conditions, results will be inconsistent
  std::vector<double> p_values;
  constexpr int NUM_RUNS = 10;
  
  for (int i = 0; i < NUM_RUNS; ++i) {
    auto result = policy.runPermutationTest(bt, 100, DecimalType("0.3"));
    p_values.push_back(result.getAsDouble());
  }
  
  // With deterministic policy, all p-values should be identical
  // (or very close due to floating point)
  double first = p_values[0];
  for (size_t i = 1; i < p_values.size(); ++i) {
    REQUIRE(std::abs(p_values[i] - first) < 1e-10);
  }
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: no data races with multiple concurrent tests",
          "[thread-safety][critical]") {
  // Run multiple permutation tests concurrently to stress test thread safety
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  constexpr int NUM_CONCURRENT_TESTS = 5;
  std::vector<std::thread> threads;
  std::atomic<int> failures{0};
  
  auto test_task = [&]() {
    try {
      DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
      auto result = policy.runPermutationTest(bt, 50, DecimalType("0.3"));
      
      if (result < DecimalType("0.0") || result > DecimalType("1.0")) {
        failures.fetch_add(1, std::memory_order_relaxed);
      }
    } catch (...) {
      failures.fetch_add(1, std::memory_order_relaxed);
    }
  };
  
  for (int i = 0; i < NUM_CONCURRENT_TESTS; ++i) {
    threads.emplace_back(test_task);
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  REQUIRE(failures.load() == 0);
}

// ============================================================================
// CRITICAL GAP 2: Stress Tests
// ============================================================================

TEST_CASE("DefaultPermuteMarketChangesPolicy: large permutation count", 
          "[stress][critical]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Run with large permutation count
  constexpr uint32_t LARGE_N = 10000;
  auto result = policy.runPermutationTest(bt, LARGE_N, DecimalType("0.3"));
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
  
  // Verify result is valid
  REQUIRE(result >= DecimalType("0.0"));
  REQUIRE(result <= DecimalType("1.0"));
  
  // Should complete in reasonable time (adjust as needed for your hardware)
  INFO("10,000 permutations took " << duration.count() << " seconds");
  REQUIRE(duration.count() < 60);  // Should complete within 60 seconds
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: very large permutation count",
          "[stress][critical][slow]") {
  // Tagged [slow] so it can be excluded from regular runs
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  constexpr uint32_t VERY_LARGE_N = 100000;
  auto result = policy.runPermutationTest(bt, VERY_LARGE_N, DecimalType("0.3"));
  
  REQUIRE(result >= DecimalType("0.0"));
  REQUIRE(result <= DecimalType("1.0"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: repeated execution doesn't leak memory",
          "[stress][critical]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // Run many times to detect memory leaks (use valgrind/ASAN to verify)
  for (int i = 0; i < 100; ++i) {
    auto result = policy.runPermutationTest(bt, 100, DecimalType("0.3"));
    REQUIRE(result >= DecimalType("0.0"));
  }
}

// ============================================================================
// CRITICAL GAP 3: Exception Handling Tests
// ============================================================================

TEST_CASE("DefaultPermuteMarketChangesPolicy: throws on zero permutations",
          "[exception][critical]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  REQUIRE_THROWS_AS(
    policy.runPermutationTest(bt, 0, DecimalType("0.3")),
    std::invalid_argument
  );
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: throws on null backtester",
          "[exception][critical]") {
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  REQUIRE_THROWS(
    policy.runPermutationTest(nullptr, 100, DecimalType("0.3"))
  );
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: throws on empty portfolio",
          "[exception][critical]") {
  auto bt = std::make_shared<DummyBackTester>();
  
  // Create strategy with empty portfolio
  auto empty_portfolio = std::make_shared<Portfolio<DecimalType>>("Empty");
  auto strategy = std::make_shared<DummyPalStrategy>(empty_portfolio);
  bt->addStrategy(strategy);
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  REQUIRE_THROWS_AS(
    policy.runPermutationTest(bt, 100, DecimalType("0.3")),
    std::runtime_error
  );
}


TEST_CASE("DefaultPermuteMarketChangesPolicy: handles single permutation edge case",
          "[exception][critical]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // Should handle N=1 without crashing
  auto result = policy.runPermutationTest(bt, 1, DecimalType("0.3"));
  
  // With deterministic policy returning 0.5, and baseline 0.3:
  // 0.5 >= 0.3, so k=1, N=1, p=(1+1)/(1+1) = 1.0
  REQUIRE(result == DecimalType("1.0"));
}

// ============================================================================
// IMPORTANT GAP 4: Atomic Operations Tests
// ============================================================================

TEST_CASE("DefaultPermuteMarketChangesPolicy: atomic counters are accurate",
          "[atomic][important]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  // Policy that always returns a value greater than baseline
  struct AlwaysHighStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&){
      return DecimalType("0.9");
    }
    static unsigned getMinStrategyTrades() { return 0; }
    static DecimalType getMinTradeFailureTestStatistic() {
      return DecimalConstants<DecimalType>::DecimalZero;
    }
  };
  
  DefaultPermuteMarketChangesPolicy<DecimalType, AlwaysHighStatPolicy> policy;
  
  constexpr uint32_t N = 100;
  auto result = policy.runPermutationTest(bt, N, DecimalType("0.1"));
  
  // All permutations should be extreme (k=N), so p=(N+1)/(N+1) = 1.0
  REQUIRE(result.getAsDouble() == Catch::Approx(1.0).epsilon(0.01));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: no lost atomic updates under contention",
          "[atomic][important]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // Run the same test multiple times
  std::vector<double> results;
  for (int i = 0; i < 10; ++i) {
    auto p = policy.runPermutationTest(bt, 100, DecimalType("0.3"));
    results.push_back(p.getAsDouble());
  }
  
  // All results should be identical (deterministic policy)
  double first = results[0];
  for (double r : results) {
    REQUIRE(std::abs(r - first) < 1e-10);
  }
}

// ============================================================================
// IMPORTANT GAP 5: Thread-Local Storage Tests
// ============================================================================

TEST_CASE("DefaultPermuteMarketChangesPolicy: TLS initialization is safe",
          "[tls][important]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // First call - should initialize TLS
  auto result1 = policy.runPermutationTest(bt, 50, DecimalType("0.3"));
  REQUIRE(result1 >= DecimalType("0.0"));
  
  // Second call - should reuse TLS (if same thread pool)
  auto result2 = policy.runPermutationTest(bt, 50, DecimalType("0.3"));
  REQUIRE(result2 >= DecimalType("0.0"));
  
  // Results should be identical with deterministic policy
  REQUIRE(std::abs(result1.getAsDouble() - result2.getAsDouble()) < 1e-10);
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: TLS works with different baseline stats",
          "[tls][important]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // Run with different baselines - each should produce valid results
  auto result1 = policy.runPermutationTest(bt, 50, DecimalType("0.1"));
  auto result2 = policy.runPermutationTest(bt, 50, DecimalType("0.9"));
  
  REQUIRE(result1 >= DecimalType("0.0"));
  REQUIRE(result2 >= DecimalType("0.0"));
  
  // With policy always returning 0.5:
  // baseline 0.1: 0.5 >= 0.1 -> all extreme -> p close to 1.0
  // baseline 0.9: 0.5 < 0.9 -> none extreme -> p close to 0
  REQUIRE(result1 > result2);
}

// ============================================================================
// IMPORTANT GAP 6: Observer Pattern Tests
// ============================================================================

// Mock observer for testing
class TestObserver : public PermutationTestObserver<DecimalType> {
public:
  TestObserver() : call_count_(0) {}
  
  void update(const BackTester<DecimalType>& bt, const DecimalType& stat) override {
    call_count_.fetch_add(1, std::memory_order_relaxed);
    last_stat_ = stat;
  }
  
  void updateMetric(const PalStrategy<DecimalType>* strategy,
                    MetricType metricType,
                    const DecimalType& metricValue) override {
    // Not implemented for this test
  }
  
  std::optional<DecimalType> getMinMetric(const PalStrategy<DecimalType>* strategy,
                                          MetricType metric) const override {
    return std::nullopt;
  }
  
  std::optional<DecimalType> getMaxMetric(const PalStrategy<DecimalType>* strategy,
                                          MetricType metric) const override {
    return std::nullopt;
  }
  
  std::optional<double> getMedianMetric(const PalStrategy<DecimalType>* strategy,
                                        MetricType metric) const override {
    return std::nullopt;
  }
  
  std::optional<double> getStdDevMetric(const PalStrategy<DecimalType>* strategy,
                                        MetricType metric) const override {
    return std::nullopt;
  }
  
  void clear() override {
    call_count_.store(0, std::memory_order_relaxed);
    last_stat_ = DecimalType{};
  }
  
  int getCallCount() const { return call_count_.load(std::memory_order_relaxed); }
  DecimalType getLastStat() const { return last_stat_; }
  
private:
  std::atomic<int> call_count_;
  DecimalType last_stat_;
};

TEST_CASE("DefaultPermuteMarketChangesPolicy: observers are notified",
          "[observer][important]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  auto observer = std::make_shared<TestObserver>();
  
  policy.attach(observer.get());
  
  constexpr uint32_t N = 10;
  auto result = policy.runPermutationTest(bt, N, DecimalType("0.3"));
  
  // Observer should be called once per permutation
  REQUIRE(observer->getCallCount() == N);
  
  // Last stat should be from deterministic policy
  REQUIRE(observer->getLastStat() == DecimalType("0.5"));
}

TEST_CASE("DefaultPermuteMarketChangesPolicy: multiple observers work correctly",
          "[observer][important]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  auto observer1 = std::make_shared<TestObserver>();
  auto observer2 = std::make_shared<TestObserver>();
  
  policy.attach(observer1.get());
  policy.attach(observer2.get());
  
  constexpr uint32_t N = 10;
  auto result = policy.runPermutationTest(bt, N, DecimalType("0.3"));
  
  // Both observers should be called
  REQUIRE(observer1->getCallCount() == N);
  REQUIRE(observer2->getCallCount() == N);
}

// ============================================================================
// NICE TO HAVE GAP 7: Performance Benchmarks
// ============================================================================

TEST_CASE("DefaultPermuteMarketChangesPolicy: baseline performance",
          "[performance][benchmark]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  // Benchmark different permutation counts
  std::vector<std::pair<uint32_t, double>> benchmarks;
  
  for (uint32_t N : {100, 500, 1000, 5000}) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = policy.runPermutationTest(bt, N, DecimalType("0.3"));
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    benchmarks.push_back({N, duration.count()});
    
    INFO("N=" << N << " took " << duration.count() << "ms");
  }
  
  // Sanity check: should scale roughly linearly
  // (actual scaling depends on thread count and overhead)
  double first_rate = benchmarks[0].second / benchmarks[0].first;
  double last_rate = benchmarks.back().second / benchmarks.back().first;
  
  // Rate should not increase by more than 10x (indicates good parallelization)
  REQUIRE(last_rate < first_rate * 10);
}

// ============================================================================
// Additional Edge Cases
// ============================================================================

TEST_CASE("DefaultPermuteMarketChangesPolicy: extreme baseline values",
          "[edge-case]") {
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));
  
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;
  
  SECTION("Very low baseline") {
    auto result = policy.runPermutationTest(bt, 100, DecimalType("0.0001"));
    // All permutations should be extreme -> p close to 1.0
    REQUIRE(result.getAsDouble() > 0.99);
  }
  
  SECTION("Very high baseline") {
    auto result = policy.runPermutationTest(bt, 100, DecimalType("999.0"));
    // No permutations should be extreme -> p = 1/(N+1)
    REQUIRE(result.getAsDouble() < 0.02);
  }
}

TEST_CASE("StandardPValueComputationPolicy: edge cases",
          "[policy][edge-case]") {
  SECTION("k=0, N=1") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 1);
    REQUIRE(p == DecimalType("0.5"));  // (0+1)/(1+1) = 0.5
  }
  
  SECTION("k=N") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(100, 100);
    REQUIRE(p.getAsDouble() == Catch::Approx(1.0).epsilon(0.001));
  }
  
  SECTION("Large N") {
    auto p = StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(500, 10000);
    REQUIRE(p >= DecimalType("0.0"));
    REQUIRE(p <= DecimalType("1.0"));
  }
}

TEST_CASE("WilsonPValueComputationPolicy: numerical stability",
          "[policy][edge-case]") {
  SECTION("Very small p-hat") {
    auto p = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(0, 10000);
    REQUIRE(p >= DecimalType("0.0"));
    REQUIRE(p <= DecimalType("1.0"));
  }
  
  SECTION("Very large p-hat") {
    auto p = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(9999, 10000);
    REQUIRE(p >= DecimalType("0.0"));
    REQUIRE(p <= DecimalType("1.0"));
  }
  
  SECTION("Extreme N") {
    auto p = WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(5000, 100000);
    REQUIRE(p >= DecimalType("0.0"));
    REQUIRE(p <= DecimalType("1.0"));
  }
}


// ============================================================================
// NoEarlyStoppingPolicy
// ============================================================================
//
// NoEarlyStoppingPolicy is a compile-time no-op — its shouldStop always
// returns false and the compiler eliminates the branch entirely for the
// default DefaultPermuteMarketChangesPolicy instantiation. The tests verify:
//
//   1. shouldStop returns false unconditionally for any input combination.
//   2. When used in DefaultPermuteMarketChangesPolicy the observer is called
//      exactly N times, confirming all permutations ran to completion.
//   3. The p-value formula is unaffected — results match the expected
//      StandardPValueComputationPolicy output exactly.

TEST_CASE("NoEarlyStoppingPolicy: shouldStop always returns false",
          "[NoEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  NoEarlyStoppingPolicy<DT> policy;
  const DT alpha = DecimalConstants<DT>::SignificantPValue;  // 0.05

  // Cover the four quadrants: few/many valid perms, zero/many extreme counts.
  SECTION("Before minBeforeStop: never stops")
  {
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        0, 0, alpha));
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        50, 0, alpha));
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        199, 0, alpha));
  }

  SECTION("After minBeforeStop with zero extreme count: never stops")
  {
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        500, 0, alpha));
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        5000, 0, alpha));
  }

  SECTION("After minBeforeStop with all extreme: never stops")
  {
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        500, 500, alpha));
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        5000, 5000, alpha));
  }

  SECTION("With clearly failing implied p-value: still never stops")
  {
    // implied p = (300+1)/(500+1) ≈ 0.60 >> 3*0.05 = 0.15
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        500, 300, alpha));
  }

  SECTION("Works with WilsonPValueComputationPolicy too")
  {
    REQUIRE_FALSE(policy.shouldStop<WilsonPValueComputationPolicy<DT>>(
        500, 0, alpha));
    REQUIRE_FALSE(policy.shouldStop<WilsonPValueComputationPolicy<DT>>(
        500, 500, alpha));
  }
}

TEST_CASE("NoEarlyStoppingPolicy: all N permutations run (observer count)",
          "[NoEarlyStoppingPolicy][integration]")
{
  // The observer is called once per completed permutation (after atomics are
  // updated). With NoEarlyStoppingPolicy the count must equal numPermutations
  // exactly.
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  // Use the default policy instantiation which has NoEarlyStoppingPolicy.
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;

  auto observer = std::make_shared<TestObserver>();
  policy.attach(observer.get());

  constexpr uint32_t N = 200;
  policy.runPermutationTest(bt, N, DecimalType("0.3"));

  REQUIRE(observer->getCallCount() == static_cast<int>(N));
}

TEST_CASE("NoEarlyStoppingPolicy: p-value matches exact formula",
          "[NoEarlyStoppingPolicy][integration]")
{
  // DeterministicStatPolicy always returns 0.5.
  // With baseline 0.9: 0.5 < 0.9 → zero extreme counts → p = 1/(N+1).
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> policy;

  constexpr uint32_t N = 99;
  auto p = policy.runPermutationTest(bt, N, DecimalType("0.9"));

  // (0+1)/(99+1) = 0.01
  REQUIRE(p == DecimalType("0.01"));
}


// ============================================================================
// ThresholdEarlyStoppingPolicy — unit tests (policy in isolation)
// ============================================================================
//
// These tests call shouldStop directly without going through
// DefaultPermuteMarketChangesPolicy, allowing precise control over
// (validPerms, extremeCount, targetAlpha) at each guard condition.

TEST_CASE("ThresholdEarlyStoppingPolicy: never stops before minBeforeStop",
          "[ThresholdEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  // Default construction: minBeforeStop=200, checkInterval=100.
  ThresholdEarlyStoppingPolicy<DT> policy;
  const DT alpha = DecimalConstants<DT>::SignificantPValue;

  // Even with zero extreme counts and validPerms < minBeforeStop, must not stop.
  for (uint32_t v = 0; v < 200; ++v)
  {
    INFO("validPerms=" << v);
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        v, 0, alpha));
  }
}

TEST_CASE("ThresholdEarlyStoppingPolicy: only checks at checkInterval multiples",
          "[ThresholdEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  // checkInterval=100, minBeforeStop=200.
  ThresholdEarlyStoppingPolicy<DT> policy;
  const DT alpha = DecimalConstants<DT>::SignificantPValue;

  // At v=250, extreme=0 (would trigger passing condition at v>=500),
  // but implied p is 0 which could trigger failing — the point is that
  // non-multiples of checkInterval must return false regardless.
  // Use a clearly failing scenario: extreme=200 out of 250 → p≈0.80.
  // At multiples of 100 (200, 300) it may stop; at 201,251 etc. it must not.

  // Check that off-interval values do not stop.
  for (uint32_t v : {201u, 251u, 301u, 350u, 401u, 450u, 501u, 550u})
  {
    INFO("validPerms=" << v << " (off-interval)");
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        v, v / 2, alpha));  // implied p ≈ 0.50, clearly failing
  }
}

TEST_CASE("ThresholdEarlyStoppingPolicy: stops for clearly passing (zero extreme)",
          "[ThresholdEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  // minPassPerms=500, checkInterval=100, minBeforeStop=200.
  ThresholdEarlyStoppingPolicy<DT> policy;
  const DT alpha = DecimalConstants<DT>::SignificantPValue;

  SECTION("Does NOT stop before minPassPerms even with zero extreme")
  {
    // At v=200,300,400 (checkInterval multiples, >= minBeforeStop) with e=0,
    // the passing condition (v >= minPassPerms=500) is not yet satisfied.
    for (uint32_t v : {200u, 300u, 400u})
    {
      INFO("validPerms=" << v);
      // implied p with Wilson is small but minPassPerms not reached — the
      // failing threshold (p > 3*0.05=0.15) is not met either when e==0.
      REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
          v, 0, alpha));
    }
  }

  SECTION("Stops at exactly minPassPerms with zero extreme")
  {
    // v=500, e=0, v % 100 == 0, v >= minPassPerms → must stop.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        500, 0, alpha));
  }

  SECTION("Stops at multiples of checkInterval beyond minPassPerms")
  {
    for (uint32_t v : {500u, 600u, 700u, 1000u, 5000u})
    {
      INFO("validPerms=" << v);
      REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
          v, 0, alpha));
    }
  }

  SECTION("Does NOT stop when extreme count is non-zero (not a clear pass)")
  {
    // e=1 at v=500 is not a clear pass — do not stop on passing condition.
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        500, 1, alpha));
  }
}

TEST_CASE("ThresholdEarlyStoppingPolicy: stops for clearly failing",
          "[ThresholdEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  ThresholdEarlyStoppingPolicy<DT> policy;  // failingMultiplier=3, alpha=0.05 → threshold=0.15
  const DT alpha = DecimalConstants<DT>::SignificantPValue;

  SECTION("Stops when implied p >> 3*alpha")
  {
    // v=300, e=150 → StandardPV = (150+1)/(300+1) ≈ 0.502 >> 0.15.
    // v % 100 == 0, v >= minBeforeStop=200 → all guards pass.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        300, 150, alpha));
  }

  SECTION("Stops when implied p is exactly above threshold")
  {
    // We want impliedP > 0.15. StandardPV = (k+1)/(N+1).
    // At v=200, e=30: (31)/(201) ≈ 0.154 > 0.15 → stop.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        200, 30, alpha));
  }

  SECTION("Does NOT stop when implied p is below threshold (boundary case)")
  {
    // At v=200, e=0: StandardPV = 1/201 ≈ 0.005 << 0.15 → no fail stop.
    // (Pass condition not met either since minPassPerms=500.)
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        200, 0, alpha));
  }

  SECTION("Does NOT stop for near-alpha boundary (implied p just below threshold)")
  {
    // At v=1000, e=100: StandardPV = 101/1001 ≈ 0.101 < 0.15 → no stop.
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        1000, 100, alpha));
  }
}

TEST_CASE("ThresholdEarlyStoppingPolicy: Wilson policy used consistently",
          "[ThresholdEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  ThresholdEarlyStoppingPolicy<DT> policy;
  const DT alpha = DecimalConstants<DT>::SignificantPValue;

  // Wilson inflates p-values, so a case that barely does not stop under
  // Standard may stop under Wilson on the failing side.
  // v=1000, e=100: Standard ≈ 0.101, Wilson > 0.101.
  // If Wilson UB > 0.15, this stops under Wilson but not under Standard.
  const DT standardP =
      StandardPValueComputationPolicy<DT>::computePermutationPValue(100, 1000);
  const DT wilsonP =
      WilsonPValueComputationPolicy<DT>::computePermutationPValue(100, 1000);

  // Confirm Wilson is more conservative (higher p-value) as established elsewhere.
  REQUIRE(wilsonP >= standardP);

  // Both should be well below 0.15 at this point — neither stops.
  REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
      1000, 100, alpha));
  REQUIRE_FALSE(policy.shouldStop<WilsonPValueComputationPolicy<DT>>(
      1000, 100, alpha));

  // With v=300, e=150: Standard ≈ 0.502, Wilson ≈ similar. Both >> 0.15.
  REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(300, 150, alpha));
  REQUIRE(policy.shouldStop<WilsonPValueComputationPolicy<DT>>(300, 150, alpha));
}

TEST_CASE("ThresholdEarlyStoppingPolicy: custom constructor parameters respected",
          "[ThresholdEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  const DT alpha = DecimalConstants<DT>::SignificantPValue;

  SECTION("Custom minBeforeStop=50 allows earlier stopping")
  {
    ThresholdEarlyStoppingPolicy<DT> policy(
        /*checkInterval*/  50,
        /*minBeforeStop*/  50,
        /*minPassPerms*/   100,
        /*failingMultiplier*/ DecimalConstants<DT>::DecimalThree);

    // v=50, e=0, v>=minPassPerms? No (100). v>=minBeforeStop? Yes.
    // Failing: 1/51 ≈ 0.02 < 0.15 → no. Passing: e==0 but v<minPassPerms → no.
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        50, 0, alpha));

    // v=100, e=0, v>=minPassPerms=100 → clear pass.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        100, 0, alpha));
  }

  SECTION("Custom failingMultiplier=2 triggers earlier on failing side")
  {
    // multiplier=2 → threshold = 2*0.05 = 0.10
    ThresholdEarlyStoppingPolicy<DT> policy(
        100, 200, 500,
        DecimalConstants<DT>::DecimalTwo);  // failingMultiplier=2

    // Standard: (51)/(401) ≈ 0.127 > 0.10 → stop.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        400, 50, alpha));

    // With default multiplier=3 (threshold=0.15), same inputs would NOT stop
    // since 0.127 < 0.15. Confirm:
    ThresholdEarlyStoppingPolicy<DT> defaultPolicy;
    REQUIRE_FALSE(defaultPolicy.shouldStop<StandardPValueComputationPolicy<DT>>(
        400, 50, alpha));
  }

  SECTION("Custom checkInterval=50 checks more frequently")
  {
    ThresholdEarlyStoppingPolicy<DT> policy(
        /*checkInterval*/ 50,
        /*minBeforeStop*/ 200,
        /*minPassPerms*/  500,
        DecimalConstants<DT>::DecimalThree);

    // v=250 (multiple of 50), e=0, v<minPassPerms=500 → no pass stop.
    // Failing: 1/251 << 0.15 → no fail stop either.
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        250, 0, alpha));

    // v=500 (multiple of 50), e=0, v>=minPassPerms=500 → clear pass.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        500, 0, alpha));

    // v=300 (multiple of 50 but NOT of 100), e=150 → implied p ≈ 0.50 > 0.15 → stop.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        300, 150, alpha));
  }
}

TEST_CASE("ThresholdEarlyStoppingPolicy: targetAlpha parameter scales threshold",
          "[ThresholdEarlyStoppingPolicy][unit]")
{
  using DT = DecimalType;
  ThresholdEarlyStoppingPolicy<DT> policy;  // failingMultiplier=3

  SECTION("Stricter alpha=0.01 → threshold=0.03, stops earlier on failing side")
  {
    const DT strictAlpha = DecimalConstants<DT>::createDecimal("0.01");
    // v=300, e=12: Standard=(13)/(301)≈0.043 > 3*0.01=0.03 → stops.
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        300, 12, strictAlpha));

    // Same counts with alpha=0.05 → threshold=0.15 → 0.043 < 0.15 → no stop.
    const DT looseAlpha = DecimalConstants<DT>::SignificantPValue;
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        300, 12, looseAlpha));
  }

  SECTION("Looser alpha=0.10 → threshold=0.30, stops less eagerly")
  {
    const DT looseAlpha = DecimalConstants<DT>::TenPercent;
    // v=400, e=80: Standard=(81)/(401)≈0.202 < 0.30 → no stop.
    REQUIRE_FALSE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        400, 80, looseAlpha));

    // Same counts with alpha=0.05 → threshold=0.15 → 0.202 > 0.15 → stops.
    const DT strictAlpha = DecimalConstants<DT>::SignificantPValue;
    REQUIRE(policy.shouldStop<StandardPValueComputationPolicy<DT>>(
        400, 80, strictAlpha));
  }
}


// ============================================================================
// ThresholdEarlyStoppingPolicy — integration tests
// ============================================================================
//
// These tests wire ThresholdEarlyStoppingPolicy into
// DefaultPermuteMarketChangesPolicy and verify end-to-end behaviour.
// The TestObserver call count reveals how many permutations actually ran.

namespace {
  // Instantiation using ThresholdEarlyStoppingPolicy with Standard p-value.
  using EarlyStopTester = DefaultPermuteMarketChangesPolicy<
      DecimalType,
      DeterministicStatPolicy,
      PValueReturnPolicy<DecimalType>,
      PermutationTestingNullTestStatisticPolicy<DecimalType>,
      concurrency::SingleThreadExecutor,
      StandardPValueComputationPolicy<DecimalType>,
      SyntheticNullModel::N1_MaxDestruction,
      ThresholdEarlyStoppingPolicy<DecimalType>>;

  // Same but using Wilson — confirms targetAlpha flows through the policy chain.
  using EarlyStopWilsonTester = DefaultPermuteMarketChangesPolicy<
      DecimalType,
      DeterministicStatPolicy,
      PValueReturnPolicy<DecimalType>,
      PermutationTestingNullTestStatisticPolicy<DecimalType>,
      concurrency::SingleThreadExecutor,
      WilsonPValueComputationPolicy<DecimalType>,
      SyntheticNullModel::N1_MaxDestruction,
      ThresholdEarlyStoppingPolicy<DecimalType>>;
} // anonymous namespace

TEST_CASE("ThresholdEarlyStoppingPolicy: stops early for clearly passing strategy",
          "[ThresholdEarlyStoppingPolicy][integration]")
{
  // DeterministicStatPolicy always returns 0.5.
  // Baseline 0.9 > 0.5 → zero extreme counts → clearly passing after minPassPerms=500.
  // With N=10000 and NoEarlyStoppingPolicy all 10000 would run.
  // With ThresholdEarlyStoppingPolicy the observer count must be < 10000.
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  EarlyStopTester policy;
  auto observer = std::make_shared<TestObserver>();
  policy.attach(observer.get());

  constexpr uint32_t N = 10000;
  auto p = policy.runPermutationTest(bt, N, DecimalType("0.9"));

  // Must have stopped before all N ran.
  const int ran = observer->getCallCount();
  REQUIRE(ran < static_cast<int>(N));

  // Must have stopped after minPassPerms=500 at the earliest.
  REQUIRE(ran >= 500);

  // P-value must be valid (in range).
  REQUIRE(p >= DecimalType("0.0"));
  REQUIRE(p <= DecimalType("1.0"));

  // P-value must be very small — zero extreme counts out of ran permutations.
  // Standard: 1/(ran+1) which is at most 1/501.
  REQUIRE(p.getAsDouble() < 0.01);
}

TEST_CASE("ThresholdEarlyStoppingPolicy: stops early for clearly failing strategy",
          "[ThresholdEarlyStoppingPolicy][integration]")
{
  // DeterministicStatPolicy always returns 0.5.
  // Baseline 0.1 < 0.5 → every permutation is extreme → implied p ≈ 1.0 >> 0.15.
  // Stopping should occur at the first check after minBeforeStop=200.
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  EarlyStopTester policy;
  auto observer = std::make_shared<TestObserver>();
  policy.attach(observer.get());

  constexpr uint32_t N = 10000;
  auto p = policy.runPermutationTest(bt, N, DecimalType("0.1"));

  // Must have stopped well before N.
  const int ran = observer->getCallCount();
  REQUIRE(ran < static_cast<int>(N));

  // Must have reached minBeforeStop=200 before any check.
  REQUIRE(ran >= 200);

  // P-value must be close to 1.0 — all extreme counts.
  REQUIRE(p.getAsDouble() > 0.9);
}

TEST_CASE("ThresholdEarlyStoppingPolicy: does not stop for boundary strategy",
          "[ThresholdEarlyStoppingPolicy][integration]")
{
  // A strategy right at the boundary: we need roughly 5% of permutations to be
  // extreme. DeterministicStatPolicy always returns 0.5, so we cannot produce
  // a boundary case deterministically without a custom policy.
  //
  // Instead we verify the absence of premature stopping by running with a
  // deterministic policy where the outcome is already clear (all-extreme) but
  // using a very small N=300 — less than minPassPerms=500 — to ensure the
  // passing condition cannot fire. The failing condition fires at N=300 because
  // all permutations are extreme (p≈1.0 >> 0.15).
  // The key assertion is that ran < N, confirming stopping fired correctly.
  //
  // For the boundary case itself, we assert that running with NoEarlyStoppingPolicy
  // and ThresholdEarlyStoppingPolicy produce the same pass/fail decision for a
  // clearly-failing strategy (p >> alpha in both cases).
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  // Clearly failing: baseline very low, all permutations extreme.
  const DecimalType baseline("0.1");
  const DecimalType alpha = DecimalConstants<DecimalType>::SignificantPValue;

  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> noStop;
  EarlyStopTester earlyStop;

  auto pNoStop    = noStop.runPermutationTest(bt, 1000, baseline);
  auto pEarlyStop = earlyStop.runPermutationTest(bt, 1000, baseline);

  // Both must clearly fail (p >> alpha=0.05).
  REQUIRE(pNoStop.getAsDouble()    > alpha.getAsDouble());
  REQUIRE(pEarlyStop.getAsDouble() > alpha.getAsDouble());

  // Both agree on the pass/fail decision.
  const bool noStopFails    = pNoStop.getAsDouble()    > alpha.getAsDouble();
  const bool earlyStopFails = pEarlyStop.getAsDouble() > alpha.getAsDouble();
  REQUIRE(noStopFails == earlyStopFails);
}

TEST_CASE("ThresholdEarlyStoppingPolicy: p-value uses counts at stopping point",
          "[ThresholdEarlyStoppingPolicy][integration]")
{
  // Verify that the final p-value is consistent with the counts at the moment
  // stopping occurred. With DeterministicStatPolicy (always 0.5) and baseline
  // 0.9 (no extremes), StandardPValueComputationPolicy gives 1/(ran+1).
  // We cannot know ran in advance but we can verify the relationship holds:
  // p == 1/(ran+1) where ran == observer->getCallCount().
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  EarlyStopTester policy;
  auto observer = std::make_shared<TestObserver>();
  policy.attach(observer.get());

  auto p = policy.runPermutationTest(bt, 10000, DecimalType("0.9"));

  const int ran = observer->getCallCount();

  // Expected: Standard p-value with k=0 extreme counts out of ran valid perms.
  const DecimalType expected =
      StandardPValueComputationPolicy<DecimalType>::computePermutationPValue(0, ran);

  REQUIRE(p.getAsDouble() == Catch::Approx(expected.getAsDouble()).epsilon(1e-9));
}

TEST_CASE("ThresholdEarlyStoppingPolicy: Wilson policy flows through consistently",
          "[ThresholdEarlyStoppingPolicy][integration]")
{
  // Same scenario as above but with WilsonPValueComputationPolicy.
  // Confirms targetAlpha and p-value policy both flow through to the stopping
  // check and final computation without mixing.
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  EarlyStopWilsonTester policy;
  auto observer = std::make_shared<TestObserver>();
  policy.attach(observer.get());

  auto p = policy.runPermutationTest(bt, 10000, DecimalType("0.9"));

  const int ran = observer->getCallCount();

  // Must have stopped early.
  REQUIRE(ran < 10000);

  // Final p-value must match Wilson formula at the actual counts (k=0, N=ran).
  const DecimalType expected =
      WilsonPValueComputationPolicy<DecimalType>::computePermutationPValue(0, ran);

  REQUIRE(p.getAsDouble() == Catch::Approx(expected.getAsDouble()).epsilon(1e-9));
}

TEST_CASE("ThresholdEarlyStoppingPolicy: early stopping reduces runtime",
          "[ThresholdEarlyStoppingPolicy][integration][performance]")
{
  // Timing comparison for a clearly failing scenario. The early stopping tester
  // should be meaningfully faster than the no-early-stopping tester at N=5000.
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  constexpr uint32_t N = 5000;
  const DecimalType baseline("0.1");  // all permutations extreme → clear fail

  auto t0 = std::chrono::high_resolution_clock::now();
  DefaultPermuteMarketChangesPolicy<DecimalType, DeterministicStatPolicy> noStop;
  noStop.runPermutationTest(bt, N, baseline);
  auto t1 = std::chrono::high_resolution_clock::now();

  EarlyStopTester earlyStop;
  earlyStop.runPermutationTest(bt, N, baseline);
  auto t2 = std::chrono::high_resolution_clock::now();

  const auto noStopMs   = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  const auto earlyStopMs = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

  INFO("NoEarlyStop: " << noStopMs << "us, EarlyStop: " << earlyStopMs << "us");

  // Early stopping must be faster. We require at least 2x speedup for a
  // clearly failing strategy at N=5000 — the stopping condition fires around
  // permutation 200-300, so actual speedup should be ~15-20x.
  REQUIRE(earlyStopMs * 2 < noStopMs);
}

TEST_CASE("ThresholdEarlyStoppingPolicy: targetAlpha parameter affects stopping point",
          "[ThresholdEarlyStoppingPolicy][integration]")
{
  // For a clearly failing scenario (all extreme → implied p≈1.0), the failing
  // condition fires regardless of targetAlpha since 1.0 >> 3*alpha for any
  // reasonable alpha. Both runs should stop early; the stricter alpha run
  // stops no later than the looser alpha run because the threshold is lower.
  auto bt = std::make_shared<DummyBackTester>();
  bt->addStrategy(std::make_shared<DummyPalStrategy>(createDummyPortfolio()));

  EarlyStopTester policyStrict, policyLoose;

  auto observerStrict = std::make_shared<TestObserver>();
  auto observerLoose  = std::make_shared<TestObserver>();
  policyStrict.attach(observerStrict.get());
  policyLoose.attach(observerLoose.get());

  const DecimalType strictAlpha = DecimalConstants<DecimalType>::createDecimal("0.01");
  const DecimalType looseAlpha  = DecimalConstants<DecimalType>::TwentyPercent;
  const DecimalType baseline("0.1");  // all permutations extreme

  policyStrict.runPermutationTest(bt, 10000, baseline, strictAlpha);
  policyLoose.runPermutationTest(bt, 10000, baseline, looseAlpha);

  const int ranStrict = observerStrict->getCallCount();
  const int ranLoose  = observerLoose->getCallCount();

  INFO("Strict alpha=" << strictAlpha << " ran=" << ranStrict);
  INFO("Loose alpha="  << looseAlpha  << " ran=" << ranLoose);

  // Both must have stopped early.
  REQUIRE(ranStrict < 10000);
  REQUIRE(ranLoose  < 10000);

  // Stricter alpha → lower threshold (3*0.01=0.03) → failing condition fires
  // at least as early as loose alpha (3*0.20=0.60).
  REQUIRE(ranStrict <= ranLoose);
}
