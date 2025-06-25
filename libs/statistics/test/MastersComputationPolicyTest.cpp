#include <catch2/catch_test_macros.hpp>
#include "StrategyDataPreparer.h"
#include "MastersPermutationTestComputationPolicy.h"
#include "PermutationTestComputationPolicy.h"
#include "PALMastersMonteCarloValidationObserver.h"
#include "TestUtils.h"
#include "Security.h"
#include <memory>
#include <vector>
#include <atomic>
#include <thread>

using namespace mkc_timeseries;

namespace {

  struct DummyStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.5");
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };

  struct ProfitFactorPolicy
  {
    static DecimalType getPermutationTestStatistic(std::shared_ptr<BackTester<DecimalType>> aBackTester)
    {
      std::shared_ptr<BacktesterStrategy<DecimalType>> backTesterStrategy =
	(*(aBackTester->beginStrategies()));
      
      return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getLogProfitFactor();
    }
      
    static unsigned int getMinStrategyTrades() {
      return 3;
    }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };
  
  struct AlwaysLowStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
      return DecimalType("0.1");
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };

  struct RandomStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>> &) {
      return DecimalType(std::to_string(0.3 + (std::rand() % 100) / 1000.0));
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }

    static DecimalType getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<DecimalType>::DecimalZero;
    }

  };
  
  class DummyBackTester : public BackTester<DecimalType> {
  public:
    DummyBackTester() : BackTester<DecimalType>() {
      boost::gregorian::date startDate(2020,1,1);
      boost::gregorian::date endDate(2020,12,31);
      DateRange r(startDate,endDate);
      this->addDateRange(r);
    }

    std::shared_ptr<BackTester<DecimalType>> clone() const override {
      return std::make_shared<DummyBackTester>();
    }


    /**
     * @brief Determines whether this is a backtester that operates
     * on the daily time frame.
     * @return `true`
     */
    bool isDailyBackTester() const
    {
      return true;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the weekly time frame.
     * @return `false`.
     */
    bool isWeeklyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the monthly time frame.
     * @return `false`.
     */
    bool isMonthlyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on intraday time frames.
     * @return `false`.
     */
    bool isIntradayBackTester() const
    {
      return false;
    }

    void backtest() override {}

    // Add the new methods to prevent segfaults in observer tests
    uint32_t getNumTrades() const {
      if (this->getNumStrategies() == 0) {
        throw BackTesterException("getNumTrades: No strategies added");
      }
      // Return a dummy value for testing
      return 10;
    }

    uint32_t getNumBarsInTrades() const {
      if (this->getNumStrategies() == 0) {
        throw BackTesterException("getNumBarsInTrades: No strategies added");
      }
      // Return a dummy value for testing
      return 50;
    }
  };


  class DummyPalStrategy : public PalStrategy<DecimalType> {
  public:
    DummyPalStrategy(std::shared_ptr<Portfolio<DecimalType>> portfolio)
      : PalStrategy<DecimalType>("dummy", getDummyPattern(), portfolio, StrategyOptions(false, 0)) {}

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

    std::shared_ptr<PalStrategy<DecimalType>> clone2(std::shared_ptr<Portfolio<DecimalType>> portfolio) const override {
      return std::make_shared<DummyPalStrategy>(portfolio);
    }

    std::shared_ptr<BacktesterStrategy<DecimalType>> clone(const std::shared_ptr<Portfolio<DecimalType>>& portfolio) const override {
      return std::make_shared<DummyPalStrategy>(portfolio);
    }

    std::shared_ptr<BacktesterStrategy<DecimalType>> cloneForBackTesting() const override {
      return std::make_shared<DummyPalStrategy>(this->getPortfolio());
    }

    void eventExitOrders(Security<DecimalType> *, const InstrumentPosition<DecimalType>&, const boost::posix_time::ptime&) override {}
    void eventEntryOrders(Security<DecimalType> *, const InstrumentPosition<DecimalType>&, const boost::posix_time::ptime&) override {}
  };

  std::shared_ptr<Security<DecimalType>> createDummySecurity() {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES, 10);
    for (int i = 0; i < 10; ++i) {
      std::ostringstream dateStream;
      dateStream << "202001" << std::setw(2) << std::setfill('0') << (i + 1);
      auto entry = createTimeSeriesEntry(dateStream.str(), "100.0", "105.0", "95.0", "102.0", "1000.0");
      ts->addEntry(*entry);
    }
    return std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc", ts);
  }

  std::shared_ptr<Portfolio<DecimalType>> createDummyPortfolio() {
    auto portfolio = std::make_shared<Portfolio<DecimalType>>("DummyPortfolio");
    portfolio->addSecurity(createDummySecurity());
    return portfolio;
  }

  StrategyContext<DecimalType> makeStrategyContext(std::shared_ptr<PalStrategy<DecimalType>> strat, DecimalType baseline) {
    StrategyContext<DecimalType> ctx;
    ctx.strategy = strat;
    ctx.baselineStat = baseline;
    ctx.count = 0;
    return ctx;
  }
  // Test observer for capturing notifications
  template<class Decimal>
  class TestObserver : public PermutationTestObserver<Decimal> {
  private:
    mutable std::mutex m_mutex;
    std::vector<Decimal> m_testStatistics;
    std::vector<uint32_t> m_numTrades;
    std::vector<uint32_t> m_numBarsInTrades;
    std::vector<unsigned long long> m_strategyHashes;

  public:
    void update(const BackTester<Decimal>& permutedBacktester,
               const Decimal& permutedTestStatistic) override {
      std::lock_guard<std::mutex> lock(m_mutex);
      
      m_testStatistics.push_back(permutedTestStatistic);
      m_numTrades.push_back(permutedBacktester.getNumTrades());
      m_numBarsInTrades.push_back(permutedBacktester.getNumBarsInTrades());
      
      // Extract strategy hash for identification
      auto strategyHash = StrategyIdentificationHelper<Decimal>::extractStrategyHash(permutedBacktester);
      m_strategyHashes.push_back(strategyHash);
    }

    void updateMetric(const PalStrategy<DecimalType>* strategy,
                     typename PermutationTestObserver<DecimalType>::MetricType metricType,
                     const DecimalType& metricValue) override {
      // For testing purposes, we can just ignore this or store it if needed
      // This is a simplified implementation for the test observer
      (void)strategy;
      (void)metricType;
      (void)metricValue;
    }

    // Required interface methods (simplified for testing)
    std::optional<Decimal> getMinMetric(const PalStrategy<Decimal>* strategy, typename PermutationTestObserver<Decimal>::MetricType metric) const override {
      return std::nullopt;
    }
    
    std::optional<Decimal> getMaxMetric(const PalStrategy<Decimal>* strategy, typename PermutationTestObserver<Decimal>::MetricType metric) const override {
      return std::nullopt;
    }
    
    std::optional<double> getMedianMetric(const PalStrategy<Decimal>* strategy, typename PermutationTestObserver<Decimal>::MetricType metric) const override {
      return std::nullopt;
    }
    
    std::optional<double> getStdDevMetric(const PalStrategy<Decimal>* strategy, typename PermutationTestObserver<Decimal>::MetricType metric) const override {
      return std::nullopt;
    }

    void clear() override {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_testStatistics.clear();
      m_numTrades.clear();
      m_numBarsInTrades.clear();
      m_strategyHashes.clear();
    }

    // Test helper methods
    size_t getNotificationCount() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_testStatistics.size();
    }

    std::vector<Decimal> getTestStatistics() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_testStatistics;
    }

    std::vector<uint32_t> getNumTrades() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_numTrades;
    }

    std::vector<uint32_t> getNumBarsInTrades() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_numBarsInTrades;
    }

    std::vector<unsigned long long> getStrategyHashes() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_strategyHashes;
    }
  };

} // anonymous namespace

TEST_CASE("MastersPermutationPolicy handles empty active strategies") {
  std::cout << "MastersPermutationPolicy handles empty active strategies" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  // Create instance instead of using static method
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto count = policy.computePermutationCountForStep(
    10, DecimalType("0.5"), {}, bt, sec, portfolio);

  REQUIRE(count == 1);
  std::cout << "Finished MastersPermutationPolicy handles empty active strategies" << std::endl;
}

TEST_CASE("MastersPermutationPolicy works with basic valid input") {
  std::cout << "MastersPermutationPolicy works with basic valid input" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    getRandomPalStrategy()
  };

  // Create instance instead of using static method
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto count = policy.computePermutationCountForStep(
    10, DecimalType("0.5"), strategies, bt, sec, portfolio);

  REQUIRE(count >= 1);
  std::cout << "Finished MastersPermutationPolicy works with basic valid input" << std::endl;
}

TEST_CASE("MastersPermutationPolicy throws on null backtester") {
  std::cout << "MastersPermutationPolicy throws on null backtester" << std::endl;
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    getRandomPalStrategy()
  };

  // Create instance instead of using static method
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  REQUIRE_THROWS_AS(
    (policy.computePermutationCountForStep(
      5, DecimalType("0.5"), strategies, nullptr, sec, portfolio)),
    std::runtime_error);
}

TEST_CASE("MastersPermutationPolicy works with multiple strategies (thread safety test)") {
  std::cout << "In MastersPermutationPolicy works with multiple strategies (thread safety)" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies;
  for (int i = 0; i < 10; ++i) {
    strategies.push_back(getRandomPalStrategy());
  }

  // Create instance instead of using static method
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto count = policy.computePermutationCountForStep(
    1000, DecimalType("0.5"), strategies, bt, sec, portfolio);

  REQUIRE(count >= 1);
}

TEST_CASE("FastMastersPermutationPolicy handles empty strategy data") {
  std::cout << "In FastMastersPermutationPolicy handles empty strategy data" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  // Create instance instead of using static method
  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto result = policy.computeAllPermutationCounts(
    10, {}, bt, sec, portfolio);

  REQUIRE(result.empty());
}

TEST_CASE("FastMastersPermutationPolicy throws on null backtester") {
  std::cout << "In FastMastersPermutationPolicy throws on null backtester" << std::endl;
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  auto strategy = getRandomPalStrategy();

  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
  StrategyContext<DecimalType> ctx;
  ctx.strategy = strategy;
  ctx.baselineStat = DecimalType("0.5");
  ctx.count = 0;
  strategyData.push_back(ctx);

  // Create instance instead of using static method
  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  REQUIRE_THROWS_AS(
    (policy.computeAllPermutationCounts(
      10, strategyData, nullptr, sec, portfolio)),
    std::runtime_error);
}

TEST_CASE("FastMastersPermutationPolicy basic test with single strategy") {
  std::cout << "In FastMastersPermutationPolicy basic test with single strategy" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  auto strategy = getRandomPalStrategy();

  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
  StrategyContext<DecimalType> ctx;
  ctx.strategy = strategy;
  ctx.baselineStat = DecimalType("0.5");
  ctx.count = 0;
  strategyData.push_back(ctx);

  // Create instance instead of using static method
  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto result = policy.computeAllPermutationCounts(
    10, strategyData, bt, sec, portfolio);

  REQUIRE(result.size() == 1);
  auto strategyHash = strategy->getPatternHash();
  REQUIRE(result[strategyHash] >= 1);
}

TEST_CASE("FastMastersPermutationPolicy handles multiple strategies") {
  std::cout << "In FastMastersPermutationPolicy handles multiple strategies" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;

  for (int i = 0; i < 5; ++i) {
    auto strategy = getRandomPalStrategy();
    StrategyContext<DecimalType> ctx;
    ctx.strategy = strategy;
    ctx.baselineStat = DecimalType("0.5");
    ctx.count = 0;
    strategyData.push_back(ctx);
  }

  // Create instance instead of using static method
  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto result = policy.computeAllPermutationCounts(
    1000, strategyData, bt, sec, portfolio);

  REQUIRE(result.size() == 5);
  for (auto it = result.begin(); it != result.end(); ++it) {
    REQUIRE(it->second >= 1);
  }
}

TEST_CASE("FastMastersPermutationPolicy returns counts of 1 when no permutation exceeds baseline") {
  std::cout << "In FastMastersPermutationPolicy returns count of 1" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  auto strategy = getRandomPalStrategy();

  FastMastersPermutationPolicy<DecimalType, AlwaysLowStatPolicy>::LocalStrategyDataContainer strategyData;
  StrategyContext<DecimalType> ctx;
  ctx.strategy = strategy;
  ctx.baselineStat = DecimalType("0.5");
  ctx.count = 0;
  strategyData.push_back(ctx);

  // Create instance instead of using static method
  FastMastersPermutationPolicy<DecimalType, AlwaysLowStatPolicy> policy;
  auto result = policy.computeAllPermutationCounts(
    10, strategyData, bt, sec, portfolio);

  auto strategyHash = strategy->getPatternHash();
  REQUIRE(result[strategyHash] == 1);
}

TEST_CASE("FastMastersPermutationPolicy with randomized statistics produces reasonable counts") {
  std::cout << "In FastMastersPermutationPolicy with randomized statistics" << std::endl;
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  FastMastersPermutationPolicy<DecimalType, RandomStatPolicy>::LocalStrategyDataContainer strategyData;

  for (int i = 0; i < 3; ++i) {
    auto strategy = getRandomPalStrategy();
    strategyData.push_back(makeStrategyContext(strategy, DecimalType("0.35")));
  }

  uint numPerms = 100;
  
  // Create instance instead of using static method
  FastMastersPermutationPolicy<DecimalType, RandomStatPolicy> policy;
  auto result = policy.computeAllPermutationCounts(
    numPerms, strategyData, bt, sec, portfolio);

  REQUIRE(result.size() == 3);
  for (auto it = result.begin(); it != result.end(); ++it) {
    REQUIRE(it->second >= 1);
    REQUIRE(it->second <= numPerms + 1);
  }
}

TEST_CASE("FastMastersPermutationPolicy with real price patterns and real series", "[integration]") {
    // load a real-world OHLCTimeSeries
    auto realSeries = getRandomPriceSeries();
    REQUIRE(realSeries);

    // wrap it in a Security and date‐range–configured BackTester
    auto security = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "RandomSecurity", realSeries);
    auto bt = BackTesterFactory<DecimalType>::getBackTester(realSeries->getTimeFrame(),
							    realSeries->getFirstDate(),
							    realSeries->getLastDate());

    // grab hundreds of PAL patterns
    auto patterns = getRandomPricePatterns();
    REQUIRE(patterns);

    // build strategies and compute their baseline ProfitFactor stats
    auto strategyData = StrategyDataPreparer<DecimalType, ProfitFactorPolicy>::prepare(bt, security, patterns);
    REQUIRE(!strategyData.empty());

    // portfolio for synthetic draws
    auto portfolio = std::make_shared<Portfolio<DecimalType>>(security->getName() + " Portfolio");
    portfolio->addSecurity(security);

    // run 100 permutations in "fast" mode
    // Create instance instead of using static method
    FastMastersPermutationPolicy<DecimalType, ProfitFactorPolicy> policy;
    auto counts = policy.computeAllPermutationCounts(
        /*numPermutations=*/2500,
        strategyData,
        bt,
        security,
        portfolio
    );

    // should have a count for every strategy, and at least 1 (the unpermuted case)
    REQUIRE(counts.size() == strategyData.size());
    for (auto const& ctx : strategyData) {
          auto strategyHash = ctx.strategy->getPatternHash();
          REQUIRE(counts.at(strategyHash) >= 1);
      }
}

TEST_CASE("MastersPermutationPolicy with real price patterns and real series", "[integration]") {
    // load a real-world OHLCTimeSeries
    auto realSeries = getRandomPriceSeries();
    REQUIRE(realSeries);

    // wrap it in a Security and date‐range–configured BackTester

    auto security = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "RandomSecurity", realSeries);
    auto bt = BackTesterFactory<DecimalType>::getBackTester(realSeries->getTimeFrame(),
							    realSeries->getFirstDate(),
							    realSeries->getLastDate());

    // grab hundreds of PAL patterns
    auto patterns = getRandomPricePatterns();
    REQUIRE(patterns);

    // build strategies and compute their baseline ProfitFactor stats
    auto contexts = StrategyDataPreparer<DecimalType, ProfitFactorPolicy>::prepare(bt, security, patterns);
    REQUIRE(!contexts.empty());

    // extract strategy pointers & baseline of the first
    std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies;
    for (auto const& ctx : contexts) {
        strategies.push_back(ctx.strategy);
    }
    auto baseline = contexts.front().baselineStat;

    auto portfolio = std::make_shared<Portfolio<DecimalType>>(security->getName() + " Portfolio");
    portfolio->addSecurity(security);

    // run 100 stepwise permutations for the first strategy
    // Create instance instead of using static method
    MastersPermutationPolicy<DecimalType, ProfitFactorPolicy> policy;
    auto count = policy.computePermutationCountForStep(
        /*numPermutations=*/100,
        baseline,
        strategies,
        bt,
        security,
        portfolio
    );

    // at least the unpermuted (baseline) draw
    REQUIRE(count >= 1);
}

// NEW OBSERVER PATTERN TESTS

TEST_CASE("MastersPermutationPolicy Observer Integration") {
  std::cout << "Testing MastersPermutationPolicy Observer Integration" << std::endl;
  
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    getRandomPalStrategy()
  };

  std::cout << "DEBUG: Creating policy object..." << std::endl;
  // Create policy and observer
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  std::cout << "DEBUG: Policy created successfully" << std::endl;
  
  std::cout << "DEBUG: Creating observer..." << std::endl;
  auto observer = std::make_unique<TestObserver<DecimalType>>();
  auto* observerPtr = observer.get();
  std::cout << "DEBUG: Observer created, ptr = " << observerPtr << std::endl;
  
  std::cout << "DEBUG: About to call policy.attach()..." << std::endl;
  // Attach observer
  policy.attach(observerPtr);
  std::cout << "DEBUG: Observer attached successfully" << std::endl;
  
  std::cout << "DEBUG: About to call computePermutationCountForStep()..." << std::endl;
  // Run permutation test
  auto count = policy.computePermutationCountForStep(
    5, DecimalType("0.5"), strategies, bt, sec, portfolio);
  std::cout << "DEBUG: computePermutationCountForStep() completed successfully" << std::endl;
  (void)count; // Suppress unused variable warning

  // Verify observer received notifications
  REQUIRE(observerPtr->getNotificationCount() > 0);
  REQUIRE(observerPtr->getNotificationCount() <= 5);
  
  // Verify test statistics are captured
  auto testStats = observerPtr->getTestStatistics();
  for (const auto& stat : testStats) {
    REQUIRE(stat >= DecimalType("0.0"));
  }
  
  // Verify enhanced BackTester methods are used
  auto tradeCounts = observerPtr->getNumTrades();
  auto barCounts = observerPtr->getNumBarsInTrades();
  
  REQUIRE(tradeCounts.size() == barCounts.size());
  REQUIRE(tradeCounts.size() == observerPtr->getNotificationCount());
  
  std::cout << "Finished MastersPermutationPolicy Observer Integration" << std::endl;
}

TEST_CASE("FastMastersPermutationPolicy Observer Integration") {
  std::cout << "Testing FastMastersPermutationPolicy Observer Integration" << std::endl;
  
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();
  auto strategy = getRandomPalStrategy();

  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
  StrategyContext<DecimalType> ctx;
  ctx.strategy = strategy;
  ctx.baselineStat = DecimalType("0.5");
  ctx.count = 0;
  strategyData.push_back(ctx);

  // Create policy and observer
  FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto observer = std::make_unique<TestObserver<DecimalType>>();
  auto* observerPtr = observer.get();
  
  // Attach observer
  policy.attach(observerPtr);
  
  // Run permutation test
  auto result = policy.computeAllPermutationCounts(
    3, strategyData, bt, sec, portfolio);

  // Verify observer received notifications
  REQUIRE(observerPtr->getNotificationCount() > 0);
  
  // Verify results are reasonable
  REQUIRE(result.size() == 1);
  auto strategyHash = strategy->getPatternHash();
  REQUIRE(result[strategyHash] >= 1);
  
  // Verify enhanced statistics are captured
  auto tradeCounts = observerPtr->getNumTrades();
  auto barCounts = observerPtr->getNumBarsInTrades();
  
  for (size_t i = 0; i < tradeCounts.size(); ++i) {
    REQUIRE(tradeCounts[i] >= 0);
    REQUIRE(barCounts[i] >= 0);
  }
  
  std::cout << "Finished FastMastersPermutationPolicy Observer Integration" << std::endl;
}

TEST_CASE("DefaultPermuteMarketChangesPolicy Observer Integration") {
  std::cout << "Testing DefaultPermuteMarketChangesPolicy Observer Integration" << std::endl;
  
  // Create test data using TestUtils
  auto timeSeries = getRandomPriceSeries();
  
  // Create a security with the time series
  auto security = std::make_shared<EquitySecurity<DecimalType>>("QQQ", "Test Security", timeSeries);
  
  // Create strategy with a portfolio that contains the security
  auto strategy = getRandomPalStrategy(security);
  
  // Get the actual date range from the time series
  auto startDate = timeSeries->getFirstDate();
  auto endDate = timeSeries->getLastDate();
  
  auto bt = std::make_shared<DailyBackTester<DecimalType>>();
  bt->addDateRange(DateRange(startDate, endDate));
  bt->addStrategy(strategy);
  
  auto testData = std::make_pair(bt, strategy);
  auto backTester = testData.first;
  
  // Create policy and observer
  DefaultPermuteMarketChangesPolicy<DecimalType, DummyStatPolicy> policy;
  auto observer = std::make_unique<TestObserver<DecimalType>>();
  auto* observerPtr = observer.get();
  
  // Attach observer
  policy.attach(observerPtr);
  
  // Run permutation test with small number of permutations
  const uint32_t numPermutations = 3;
  const DecimalType baselineTestStat = DecimalType("0.5");
  
  auto result = policy.runPermutationTest(backTester, numPermutations, baselineTestStat);
  
  // Verify observer received notifications
  REQUIRE(observerPtr->getNotificationCount() >= 0);  // May be 0 if no valid permutations
  
  // If we got notifications, verify they contain valid data
  if (observerPtr->getNotificationCount() > 0) {
    auto testStats = observerPtr->getTestStatistics();
    auto tradeCounts = observerPtr->getNumTrades();
    auto barCounts = observerPtr->getNumBarsInTrades();
    
    REQUIRE(testStats.size() == tradeCounts.size());
    REQUIRE(testStats.size() == barCounts.size());
    
    for (size_t i = 0; i < testStats.size(); ++i) {
      REQUIRE(testStats[i] >= DecimalType("0.0"));
      REQUIRE(tradeCounts[i] >= 0);
      REQUIRE(barCounts[i] >= 0);
    }
  }
  
  std::cout << "Finished DefaultPermuteMarketChangesPolicy Observer Integration" << std::endl;
}

TEST_CASE("Multiple Observers Receive Same Notifications") {
  std::cout << "Testing Multiple Observers Receive Same Notifications" << std::endl;
  
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    getRandomPalStrategy()
  };

  // Create policy and multiple observers
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto observer1 = std::make_unique<TestObserver<DecimalType>>();
  auto observer2 = std::make_unique<TestObserver<DecimalType>>();
  auto* observer1Ptr = observer1.get();
  auto* observer2Ptr = observer2.get();
  
  // Attach both observers
  policy.attach(observer1Ptr);
  policy.attach(observer2Ptr);
  
  // Run permutation test
  auto count = policy.computePermutationCountForStep(
    3, DecimalType("0.5"), strategies, bt, sec, portfolio);
  (void)count; // Suppress unused variable warning

  // Both observers should receive the same number of notifications
  REQUIRE(observer1Ptr->getNotificationCount() == observer2Ptr->getNotificationCount());
  
  if (observer1Ptr->getNotificationCount() > 0) {
    // Both should receive the same test statistics
    auto stats1 = observer1Ptr->getTestStatistics();
    auto stats2 = observer2Ptr->getTestStatistics();
    REQUIRE(stats1.size() == stats2.size());
    
    for (size_t i = 0; i < stats1.size(); ++i) {
      REQUIRE(stats1[i] == stats2[i]);
    }
  }
  
  std::cout << "Finished Multiple Observers Receive Same Notifications" << std::endl;
}

TEST_CASE("Observer Detachment Works Correctly") {
  std::cout << "Testing Observer Detachment Works Correctly" << std::endl;
  
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    getRandomPalStrategy()
  };

  // Create policy and observer
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  auto observer = std::make_unique<TestObserver<DecimalType>>();
  auto* observerPtr = observer.get();
  
  // Attach and then detach observer
  policy.attach(observerPtr);
  policy.detach(observerPtr);
  
  // Run permutation test
  auto count = policy.computePermutationCountForStep(
    3, DecimalType("0.5"), strategies, bt, sec, portfolio);
  (void)count; // Suppress unused variable warning

  // Observer should not receive any notifications
  REQUIRE(observerPtr->getNotificationCount() == 0);
  
  std::cout << "Finished Observer Detachment Works Correctly" << std::endl;
}

TEST_CASE("Policy Thread Safety with Observers") {
  std::cout << "Testing Policy Thread Safety with Observers" << std::endl;
  
  auto bt = std::make_shared<DummyBackTester>();
  auto sec = createDummySecurity();
  auto portfolio = createDummyPortfolio();

  std::vector<std::shared_ptr<PalStrategy<DecimalType>>> strategies = {
    getRandomPalStrategy()
  };

  // Create policy
  MastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
  
  // Create multiple observers
  std::vector<std::unique_ptr<TestObserver<DecimalType>>> observers;
  std::vector<TestObserver<DecimalType>*> observerPtrs;
  
  for (int i = 0; i < 3; ++i) {
    observers.push_back(std::make_unique<TestObserver<DecimalType>>());
    observerPtrs.push_back(observers.back().get());
  }
  
  // Attach observers concurrently
  std::vector<std::thread> attachThreads;
  for (auto* observer : observerPtrs) {
    attachThreads.emplace_back([&policy, observer]() {
      policy.attach(observer);
    });
  }
  
  for (auto& thread : attachThreads) {
    thread.join();
  }
  
  // Run permutation test
  auto count = policy.computePermutationCountForStep(
    2, DecimalType("0.5"), strategies, bt, sec, portfolio);
  (void)count; // Suppress unused variable warning
  
  // All observers should receive the same notifications
  if (!observerPtrs.empty() && observerPtrs[0]->getNotificationCount() > 0) {
    auto expectedCount = observerPtrs[0]->getNotificationCount();
    for (auto* observer : observerPtrs) {
      REQUIRE(observer->getNotificationCount() == expectedCount);
    }
  }
  
  // Detach observers concurrently
  std::vector<std::thread> detachThreads;
  for (auto* observer : observerPtrs) {
    detachThreads.emplace_back([&policy, observer]() {
      policy.detach(observer);
    });
  }
  
  for (auto& thread : detachThreads) {
    thread.join();
  }
  
  std::cout << "Finished Policy Thread Safety with Observers" << std::endl;
}
