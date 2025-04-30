#include <catch2/catch_test_macros.hpp>
#include "StrategyDataPreparer.h"
#include "MastersRomanoWolf.h"
#include "MastersPermutationTestComputationPolicy.h"
#include "TestUtils.h"
#include "Security.h"
#include <memory>
#include <vector>
#include <cstdlib>

using namespace mkc_timeseries;

typedef DecimalType D;

namespace {

// Dummy stat policy: always returns 0.5
struct DummyStatPolicy {
    static D getPermutationTestStatistic(const std::shared_ptr<BackTester<D>>&) {
        return D("0.5");
    }
    static unsigned int getMinStrategyTrades() { return 0; }
};

  
  struct RandomStatPolicy {
    static DecimalType getPermutationTestStatistic(const std::shared_ptr<BackTester<DecimalType>> &) {
      return DecimalType(std::to_string(0.3 + (std::rand() % 100) / 1000.0));
    }

    static unsigned int getMinStrategyTrades() {
      return 0;
    }
  };

// Always-low stat policy: always returns 0.1 (to trigger failure)
struct AlwaysLowStatPolicy {
    static D getPermutationTestStatistic(const std::shared_ptr<BackTester<D>>&) {
        return D("0.1");
    }
    static unsigned int getMinStrategyTrades() { return 0; }
};

// Copy of DummyBackTester and DummyPalStrategy from existing tests
class DummyBackTesterEx : public BackTester<D> {
public:
    DummyBackTesterEx() : BackTester<D>() {}
    std::shared_ptr<BackTester<D>> clone() const override {
        return std::make_shared<DummyBackTesterEx>();
    }
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const override { return boost_previous_weekday(d); }
    TimeSeriesDate next_period(const TimeSeriesDate& d) const override { return boost_next_weekday(d); }
    void backtest() override {}
};

class DummyPalStrategyEx : public PalStrategy<D> {
public:
    DummyPalStrategyEx(std::shared_ptr<Portfolio<D>> pf)
      : PalStrategy<D>("dummy", nullptr, pf, StrategyOptions(false, 0)) {}
    std::shared_ptr<PalStrategy<D>> clone2(std::shared_ptr<Portfolio<D>> pf) const override {
        return std::make_shared<DummyPalStrategyEx>(pf);
    }
    std::shared_ptr<BacktesterStrategy<D>> clone(std::shared_ptr<Portfolio<D>> pf) const override {
        return std::make_shared<DummyPalStrategyEx>(pf);
    }
    std::shared_ptr<BacktesterStrategy<D>> cloneForBackTesting() const override {
        return std::make_shared<DummyPalStrategyEx>(this->getPortfolio());
    }
    void eventExitOrders(std::shared_ptr<Security<D>>, const InstrumentPosition<D>&, const boost::gregorian::date&) override {}
    void eventEntryOrders(std::shared_ptr<Security<D>>, const InstrumentPosition<D>&, const boost::gregorian::date&) override {}
};

  // Helpers to create dummy security, portfolio, and strategy context
std::shared_ptr<Security<D>> createDummySecurity() {
    auto ts = std::make_shared<OHLCTimeSeries<D>>(TimeFrame::DAILY, TradingVolume::SHARES, 10);
    for (int i = 0; i < 10; ++i) {
        std::ostringstream dateStream;
        dateStream << "202001" << std::setw(2) << std::setfill('0') << (i + 1);
        auto entry = createTimeSeriesEntry(dateStream.str(), "100.0", "105.0", "95.0", "102.0", "1000.0");
        ts->addEntry(*entry);
    }
    return std::make_shared<EquitySecurity<D>>("AAPL", "Apple Inc", ts);
}

std::shared_ptr<Portfolio<D>> createDummyPortfolio() {
    auto portfolio = std::make_shared<Portfolio<D>>("DummyPortfolio");
    portfolio->addSecurity(createDummySecurity());
    return portfolio;
}

StrategyContext<D> makeStrategyContext(const std::shared_ptr<PalStrategy<D>>& strat, D baseline) {
    StrategyContext<D> ctx;
    ctx.strategy = strat;
    ctx.baselineStat = baseline;
    ctx.count = 0;
    return ctx;
}

} // anonymous namespace

TEST_CASE("MastersRomanoWolf run handles empty strategy data") {
    MastersRomanoWolf<D, DummyStatPolicy> algo;
    std::vector<StrategyContext<D>> data;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    auto pvals = algo.run(data, 10, bt, portfolio, D("0.05"));
    REQUIRE(pvals.empty());
}

TEST_CASE("MastersRomanoWolf run throws on null backtester") {
    MastersRomanoWolf<D, DummyStatPolicy> algo;
    auto portfolio = createDummyPortfolio();
    auto strategy = std::make_shared<DummyPalStrategyEx>(portfolio);
    StrategyContext<D> ctx = makeStrategyContext(strategy, D("0.5"));
    std::vector<StrategyContext<D>> data{ctx};
    REQUIRE_THROWS_AS(algo.run(data, 5, nullptr, portfolio, D("0.05")), std::runtime_error);
}

TEST_CASE("MastersRomanoWolf run basic test with single strategy") {
    MastersRomanoWolf<D, DummyStatPolicy> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    auto strategy = std::make_shared<DummyPalStrategyEx>(portfolio);
    StrategyContext<D> ctx = makeStrategyContext(strategy, D("0.5"));
    std::vector<StrategyContext<D>> data{ctx};

    // Use high significance to allow removal without failure
    auto pvals = algo.run(data, 1, bt, portfolio, D("1.0"));
    REQUIRE(pvals.size() == 1);
    REQUIRE(pvals[strategy] == D("1.0"));
}

TEST_CASE("MastersRomanoWolf run works with multiple strategies") {
    MastersRomanoWolf<D, DummyStatPolicy> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    std::vector<StrategyContext<D>> data;
    std::vector<std::shared_ptr<PalStrategy<D>>> strategies;
    for (int i = 0; i < 3; ++i) {
        auto strat = std::make_shared<DummyPalStrategyEx>(portfolio);
        strategies.push_back(strat);
        data.push_back(makeStrategyContext(strat, D("0.5")));
    }

    auto pvals = algo.run(data, 1, bt, portfolio, D("1.0"));
    REQUIRE(pvals.size() == strategies.size());
    for (auto& strat : strategies) {
        REQUIRE(pvals[strat] == D("1.0"));
    }
}

TEST_CASE("MastersRomanoWolf run failure early sets same p-value for all remaining strategies") {
    MastersRomanoWolf<D, AlwaysLowStatPolicy> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    std::vector<StrategyContext<D>> data;
    std::vector<std::shared_ptr<PalStrategy<D>>> strategies;
    for (int i = 0; i < 3; ++i) {
        auto strat = std::make_shared<DummyPalStrategyEx>(portfolio);
        strategies.push_back(strat);
        data.push_back(makeStrategyContext(strat, D("0.5")));
    }

    // Use low significance to trigger failure on first step
    auto pvals = algo.run(data, 1, bt, portfolio, D("0.4"));
    REQUIRE(pvals.size() == strategies.size());
    // p = 1/(1+1) = 0.5
    for (auto& strat : strategies) {
        REQUIRE(pvals[strat] == D("0.5"));
    }
}

TEST_CASE("MastersRomanoWolf handles randomized statistics") {
    // set up dummy backtester & portfolio
    auto portfolio = createDummyPortfolio();
    auto bt        = std::make_shared<DummyBackTesterEx>();

    // seed RNG so this test is repeatable
    std::srand(1234);

    // build three strategies with random baselines around 0.5
    std::vector<StrategyContext<D>> data;
    for (int i = 0; i < 3; ++i) {
        auto strat = std::make_shared<DummyPalStrategyEx>(portfolio);
        // random baseline in [0.3,0.399]
        D baseline(std::to_string(0.3 + (std::rand() % 100) / 1000.0));
        data.push_back(makeStrategyContext(strat, baseline));
    }

    // Ensure data is sorted by baselineStat in descending order to satisfy precondition
    std::sort(data.begin(), data.end(),
              [](auto const &a, auto const &b) { return a.baselineStat > b.baselineStat; });

    MastersRomanoWolf<D, RandomStatPolicy> algo;
    auto pvals = algo.run(data, /*numPermutations=*/500, bt, portfolio, /*sigLevel=*/D("0.05"));

    // 1) All p-values in [0,1]
    for (auto &kv : pvals) {
        REQUIRE(kv.second >= D("0.0"));
        REQUIRE(kv.second <= D("1.0"));
    }

    // 2) Enforce step‐down: as baselineStat increases, adjusted p‐value should not decrease
    D prev = D("0.0");
    for (auto &ctx : data) {
        auto v = pvals[ctx.strategy];
        REQUIRE(v >= prev);
        prev = v;
    }
}

// New tests for unsorted input validation
TEST_CASE("MastersRomanoWolf run throws on unsorted strategy data") {
    MastersRomanoWolf<D, DummyStatPolicy> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    // Create two strategies with baselines 0.5 and 1.0
    auto s1 = std::make_shared<DummyPalStrategyEx>(portfolio);
    auto s2 = std::make_shared<DummyPalStrategyEx>(portfolio);
    StrategyContext<D> ctx1 = makeStrategyContext(s1, D("0.5"));
    StrategyContext<D> ctx2 = makeStrategyContext(s2, D("1.0"));
    // Unsorted: ascending order (0.5 < 1.0)
    std::vector<StrategyContext<D>> data{ctx1, ctx2};
    REQUIRE_THROWS_AS(algo.run(data, 1, bt, portfolio, D("0.05")), std::invalid_argument);
}

TEST_CASE("MastersRomanoWolf run throws on partially unsorted strategy data") {
    MastersRomanoWolf<D, DummyStatPolicy> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    // Create three strategies with baselines 1.0, 0.5, 0.8
    auto sa = std::make_shared<DummyPalStrategyEx>(portfolio);
    auto sb = std::make_shared<DummyPalStrategyEx>(portfolio);
    auto sc = std::make_shared<DummyPalStrategyEx>(portfolio);
    StrategyContext<D> ca = makeStrategyContext(sa, D("1.0"));
    StrategyContext<D> cb = makeStrategyContext(sb, D("0.5"));
    StrategyContext<D> cc = makeStrategyContext(sc, D("0.8"));
    // Partially unsorted: 0.8 follows 0.5
    std::vector<StrategyContext<D>> data{ca, cb, cc};
    REQUIRE_THROWS_AS(algo.run(data, 2, bt, portfolio, D("0.05")), std::invalid_argument);
}

// Integration test using real-world data

template<typename Decimal>
struct ProfitFactorPolicy {
    static Decimal getPermutationTestStatistic(const std::shared_ptr<BackTester<Decimal>>& aBackTester) {
        auto it = aBackTester->beginStrategies();
        auto backTesterStrategy = *it;
        return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getLogProfitFactor();
    }
    static unsigned int getMinStrategyTrades() { return 3; }
};

TEST_CASE("MastersRomanoWolf integration test with real price patterns and real time series", "[integration]") {
    // load a real-world OHLCTimeSeries
    auto realSeries = getRandomPriceSeries();
    REQUIRE(realSeries);

    // wrap it in a Security and date-range–configured BackTester
    auto security = std::make_shared<EquitySecurity<D>>("QQQ", "Invesco Nasdaq 100 ETF", realSeries);
    auto bt = BackTesterFactory<D>::getBackTester(realSeries->getTimeFrame(),
                                                  realSeries->getFirstDate(),
                                                  realSeries->getLastDate());

    // grab PAL patterns
    auto patterns = getRandomPricePatterns();
    REQUIRE(patterns);

    // build contexts with ProfitFactorPolicy
    auto contexts = StrategyDataPreparer<D, ProfitFactorPolicy<D>>::prepare(bt, security, patterns);
    REQUIRE(!contexts.empty());

    // ensure sorted descending by observed statistic
    std::sort(contexts.begin(), contexts.end(),
              [](auto const &a, auto const &b) { return a.baselineStat > b.baselineStat; });

    // portfolio for testing
    auto portfolio = std::make_shared<Portfolio<D>>(security->getName() + " Portfolio");
    portfolio->addSecurity(security);

    MastersRomanoWolf<D, ProfitFactorPolicy<D>> algo;
    unsigned long numPerms = 500;
    D alpha("0.05");
    auto pvals = algo.run(contexts, numPerms, bt, portfolio, alpha);

    // should have a p-value for every context
    REQUIRE(pvals.size() == contexts.size());
    // p-values in [0,1] and non-decreasing
    D prev = D("0.0");
    for (auto& ctx : contexts) {
        auto v = pvals[ctx.strategy];
        REQUIRE(v >= D("0.0"));
        REQUIRE(v <= D("1.0"));
        REQUIRE(v >= prev);
        prev = v;
    }
}
