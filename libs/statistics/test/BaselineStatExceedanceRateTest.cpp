// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "MastersPermutationTestComputationPolicy.h"
#include "PermutationStatisticsCollector.h"
#include "PermutationTestObserver.h"
#include "TestUtils.h"
#include "Security.h"

using namespace mkc_timeseries;

namespace {

    struct DummyStatPolicy {
        static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>>&) {
            return DecimalType("0.5");
        }

        static unsigned int getMinStrategyTrades() {
            return 0;
        }

        static DecimalType getMinTradeFailureTestStatistic() {
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

        bool isDailyBackTester() const { return true; }
        bool isWeeklyBackTester() const { return false; }
        bool isMonthlyBackTester() const { return false; }
        bool isIntradayBackTester() const { return false; }

        void backtest() override {}

        uint32_t getNumTrades() const {
            if (this->getNumStrategies() == 0) {
                throw BackTesterException("getNumTrades: No strategies added");
            }
            return 10;
        }

        uint32_t getNumBarsInTrades() const {
            if (this->getNumStrategies() == 0) {
                throw BackTesterException("getNumBarsInTrades: No strategies added");
            }
            return 50;
        }
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

    [[maybe_unused]] StrategyContext<DecimalType> makeStrategyContext(std::shared_ptr<PalStrategy<DecimalType>> strat, DecimalType baseline) {
        StrategyContext<DecimalType> ctx;
        ctx.strategy = strat;
        ctx.baselineStat = baseline;
        ctx.count = 0;
        return ctx;
    }

    // Test observer that captures BASELINE_STAT_EXCEEDANCE_RATE notifications
    class BaselineExceedanceTestObserver : public PermutationTestObserver<DecimalType> {
    private:
        mutable std::mutex m_mutex;
        std::map<const PalStrategy<DecimalType>*, DecimalType> m_baselineExceedanceRates;
        std::vector<DecimalType> m_testStatistics;

    public:
        void update(const BackTester<DecimalType>& permutedBacktester,
                   const DecimalType& permutedTestStatistic) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_testStatistics.push_back(permutedTestStatistic);
        }

        void updateMetric(const PalStrategy<DecimalType>* strategy,
                         MetricType metricType,
                         const DecimalType& metricValue) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (metricType == MetricType::BASELINE_STAT_EXCEEDANCE_RATE) {
                m_baselineExceedanceRates[strategy] = metricValue;
            }
        }

        // Required interface methods (simplified for testing)
        std::optional<DecimalType> getMinMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (metric == MetricType::BASELINE_STAT_EXCEEDANCE_RATE) {
                auto it = m_baselineExceedanceRates.find(strategy);
                if (it != m_baselineExceedanceRates.end()) {
                    return it->second;
                }
            }
            return std::nullopt;
        }
        
        std::optional<DecimalType> getMaxMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return getMinMetric(strategy, metric); // Same value for single metric
        }
        
        std::optional<double> getMedianMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            auto value = getMinMetric(strategy, metric);
            return value ? std::optional<double>(num::to_double(*value)) : std::nullopt;
        }
        
        std::optional<double> getStdDevMetric(const PalStrategy<DecimalType>* strategy, MetricType metric) const override {
            return std::nullopt; // No std dev for single value
        }
        
        void clear() override {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_baselineExceedanceRates.clear();
            m_testStatistics.clear();
        }

        // Test helper methods
        bool hasBaselineExceedanceRate(const PalStrategy<DecimalType>* strategy) const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_baselineExceedanceRates.find(strategy) != m_baselineExceedanceRates.end();
        }

        DecimalType getBaselineExceedanceRate(const PalStrategy<DecimalType>* strategy) const {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_baselineExceedanceRates.find(strategy);
            if (it != m_baselineExceedanceRates.end()) {
                return it->second;
            }
            return DecimalType("0.0");
        }

        size_t getTestStatisticsCount() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_testStatistics.size();
        }
    };

} // anonymous namespace

TEST_CASE("FastMastersPermutationPolicy calculates BASELINE_STAT_EXCEEDANCE_RATE", "[baseline-exceedance]") {
    SECTION("Single strategy with known baseline") {
        auto bt = std::make_shared<DummyBackTester>();
        auto sec = createDummySecurity();
        auto portfolio = createDummyPortfolio();
        auto strategy = getRandomPalStrategy();

        FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
        StrategyContext<DecimalType> ctx;
        ctx.strategy = strategy;
        ctx.baselineStat = DecimalType("0.5"); // Same as DummyStatPolicy returns
        ctx.count = 0;
        strategyData.push_back(ctx);

        // Create policy and observer
        FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
        auto observer = std::make_unique<BaselineExceedanceTestObserver>();
        auto* observerPtr = observer.get();
        
        // Attach observer
        policy.attach(observerPtr);
        
        // Run permutation test with small number of permutations
        const uint32_t numPermutations = 10;
        auto result = policy.computeAllPermutationCounts(
            numPermutations, strategyData, bt, sec, portfolio);

        // Verify observer received BASELINE_STAT_EXCEEDANCE_RATE notification
        REQUIRE(observerPtr->hasBaselineExceedanceRate(strategy.get()));
        
        // Get the calculated exceedance rate
        auto exceedanceRate = observerPtr->getBaselineExceedanceRate(strategy.get());
        
        // Since DummyStatPolicy always returns 0.5 and baseline is 0.5,
        // all permutations should exceed the baseline (including the unpermuted case)
        // Expected rate = (numPermutations + 1) / (numPermutations + 1) * 100 = 100%
        DecimalType expectedRate = DecimalType("100.0");
        REQUIRE(exceedanceRate == expectedRate);
        
        // Verify the result map also shows the expected count
        auto strategyHash = strategy->getPatternHash();
        REQUIRE(result[strategyHash] == numPermutations + 1);
    }
}

TEST_CASE("FastMastersPermutationPolicy calculates correct exceedance rate with multiple strategies", "[baseline-exceedance]") {
SECTION("Multiple strategies with different baselines") {
        auto bt = std::make_shared<DummyBackTester>();
        auto sec = createDummySecurity();
        auto portfolio = createDummyPortfolio();

        FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
        
        // Create strategies with different baseline stats
        auto strategy1 = getRandomPalStrategy(); // Weaker
        auto strategy2 = getRandomPalStrategy(); // Stronger
        
        StrategyContext<DecimalType> ctx1;
        ctx1.strategy = strategy1;
        ctx1.baselineStat = DecimalType("0.3"); // Below DummyStatPolicy's 0.5
        ctx1.count = 0;
        
        StrategyContext<DecimalType> ctx2;
        ctx2.strategy = strategy2;
        ctx2.baselineStat = DecimalType("0.7"); // Above DummyStatPolicy's 0.5
        ctx2.count = 0;
        
        // IMPORTANT: The policy expects data sorted descending (best-to-worst)
        // The test must mimic this.
        strategyData.push_back(ctx2); // Stronger first
        strategyData.push_back(ctx1); // Weaker second

        FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
        auto observer = std::make_unique<BaselineExceedanceTestObserver>();
        auto* observerPtr = observer.get();
        policy.attach(observerPtr);
        
        const uint32_t numPermutations = 5;
        auto result = policy.computeAllPermutationCounts(
            numPermutations, strategyData, bt, sec, portfolio);

        REQUIRE(observerPtr->hasBaselineExceedanceRate(strategy1.get()));
        REQUIRE(observerPtr->hasBaselineExceedanceRate(strategy2.get()));
        
        auto exceedanceRate1 = observerPtr->getBaselineExceedanceRate(strategy1.get());
        auto exceedanceRate2 = observerPtr->getBaselineExceedanceRate(strategy2.get());
        
        // --- CORRECTED EXPECTATIONS FOR THE NEW ALGORITHM ---

        // For Strategy 1 (weaker, baseline 0.3):
        // It's tested against max(permuted_stat(s1)) = 0.5.
        // The condition (0.5 >= 0.3) is TRUE for all permutations. Count should be 6.
        DecimalType expectedRate1 = DecimalType("100.0");
        REQUIRE(exceedanceRate1 == expectedRate1);
        
        // For Strategy 2 (stronger, baseline 0.7):
        // It's tested against max(permuted_stat(s1), permuted_stat(s2)) = 0.5.
        // The condition (0.5 >= 0.7) is FALSE for all permutations. Only the initial count of 1 remains.
        DecimalType expectedRate2 = DecimalType("100.0") / DecimalType(numPermutations + 1); // Approx 16.67%
        
        DecimalType tolerance = DecimalType("0.0001");
        DecimalType diff = exceedanceRate2 > expectedRate2 ?
                          exceedanceRate2 - expectedRate2 :
                          expectedRate2 - exceedanceRate2;
        REQUIRE(diff < tolerance);
        
        auto strategy1Hash = strategy1->getPatternHash();
        auto strategy2Hash = strategy2->getPatternHash();
        REQUIRE(result[strategy1Hash] == numPermutations + 1);
        REQUIRE(result[strategy2Hash] == 1);
    }
}

TEST_CASE("BASELINE_STAT_EXCEEDANCE_RATE calculation formula verification", "[baseline-exceedance]") {
    SECTION("Verify calculation formula: (count / (numPermutations + 1)) * 100") {
        auto bt = std::make_shared<DummyBackTester>();
        auto sec = createDummySecurity();
        auto portfolio = createDummyPortfolio();
        auto strategy = getRandomPalStrategy();

        FastMastersPermutationPolicy<DecimalType, DummyStatPolicy>::LocalStrategyDataContainer strategyData;
        StrategyContext<DecimalType> ctx;
        ctx.strategy = strategy;
        ctx.baselineStat = DecimalType("0.5"); // Same as DummyStatPolicy
        ctx.count = 0;
        strategyData.push_back(ctx);

        // Create policy and observer
        FastMastersPermutationPolicy<DecimalType, DummyStatPolicy> policy;
        auto observer = std::make_unique<BaselineExceedanceTestObserver>();
        auto* observerPtr = observer.get();
        
        // Attach observer
        policy.attach(observerPtr);
        
        // Test with different numbers of permutations
        std::vector<uint32_t> permutationCounts = {1, 5, 10, 20};
        
        for (uint32_t numPermutations : permutationCounts) {
            // Clear observer state
            observerPtr->clear();
            
            // Run permutation test
            auto result = policy.computeAllPermutationCounts(
                numPermutations, strategyData, bt, sec, portfolio);
            
            // Get the calculated exceedance rate
            auto exceedanceRate = observerPtr->getBaselineExceedanceRate(strategy.get());
            
            // Calculate expected rate using the formula
            auto strategyHash = strategy->getPatternHash();
            unsigned int exceedanceCount = result[strategyHash];
            DecimalType expectedRate = (DecimalType(exceedanceCount) / DecimalType(numPermutations + 1)) * DecimalType("100.0");
            
            REQUIRE(exceedanceRate == expectedRate);
            
            // Since baseline equals permutation stat, all should exceed
            REQUIRE(exceedanceCount == numPermutations + 1);
            REQUIRE(exceedanceRate == DecimalType("100.0"));
        }
    }
}

TEST_CASE("BASELINE_STAT_EXCEEDANCE_RATE observer integration", "[baseline-exceedance]") {
    SECTION("Observer receives both regular updates and metric updates") {
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
        auto observer = std::make_unique<BaselineExceedanceTestObserver>();
        auto* observerPtr = observer.get();
        
        // Attach observer
        policy.attach(observerPtr);
        
        // Run permutation test
        const uint32_t numPermutations = 3;
        auto result = policy.computeAllPermutationCounts(
            numPermutations, strategyData, bt, sec, portfolio);

        // Verify observer received regular update notifications during permutations
        REQUIRE(observerPtr->getTestStatisticsCount() > 0);
        REQUIRE(observerPtr->getTestStatisticsCount() <= numPermutations);
        
        // Verify observer received BASELINE_STAT_EXCEEDANCE_RATE metric notification
        REQUIRE(observerPtr->hasBaselineExceedanceRate(strategy.get()));
        
        // Verify the exceedance rate is reasonable
        auto exceedanceRate = observerPtr->getBaselineExceedanceRate(strategy.get());
        REQUIRE(exceedanceRate >= DecimalType("0.0"));
        REQUIRE(exceedanceRate <= DecimalType("100.0"));
    }
}
