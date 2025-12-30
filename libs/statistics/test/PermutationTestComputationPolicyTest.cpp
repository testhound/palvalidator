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

