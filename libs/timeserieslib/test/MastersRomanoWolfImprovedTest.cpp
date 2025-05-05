#include <catch2/catch_test_macros.hpp>
#include "MastersRomanoWolfImproved.h"
#include "StrategyDataPreparer.h"
#include "TestUtils.h"
#include "Security.h"
#include <memory>
#include <vector>
#include <cstdlib>
#include <algorithm>

using namespace mkc_timeseries;
typedef DecimalType D;

namespace {

// Dummy stat policy: always returns 0.5
template<typename Decimal>
struct DummyStatPolicy {
    static Decimal getPermutationTestStatistic(const std::shared_ptr<BackTester<Decimal>>&) {
        return Decimal("0.5");
    }
    static unsigned int getMinStrategyTrades() { return 0; }
};

// Always-low stat policy: always returns 0.1 (to trigger early failure)
template<typename Decimal>
struct AlwaysLowStatPolicy {
    static Decimal getPermutationTestStatistic(const std::shared_ptr<BackTester<Decimal>>&) {
        return Decimal("0.1");
    }
    static unsigned int getMinStrategyTrades() { return 0; }
};

// Random stat policy: returns a random value around 0.3
template<typename Decimal>
struct RandomStatPolicy {
    static Decimal getPermutationTestStatistic(const std::shared_ptr<BackTester<Decimal>>&) {
        return Decimal(std::to_string(0.3 + (std::rand() % 100) / 1000.0));
    }
    static unsigned int getMinStrategyTrades() { return 0; }
};

// Helpers: Copy DummyBackTesterEx and DummyPalStrategyEx from existing tests
class DummyBackTesterEx : public BackTester<D> {
public:
    DummyBackTesterEx() : BackTester<D>() {}
    std::shared_ptr<BackTester<D>> clone() const override { return std::make_shared<DummyBackTesterEx>(); }
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
    std::shared_ptr<BacktesterStrategy<D>> clone(const std::shared_ptr<Portfolio<D>>& pf) const override {
        return std::make_shared<DummyPalStrategyEx>(pf);
    }
    std::shared_ptr<BacktesterStrategy<D>> cloneForBackTesting() const override {
        return std::make_shared<DummyPalStrategyEx>(this->getPortfolio());
    }
    void eventExitOrders(Security<D> *, const InstrumentPosition<D>&, const boost::gregorian::date&) override {}
    void eventEntryOrders(Security<D> *, const InstrumentPosition<D>&, const boost::gregorian::date&) override {}
};

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
    auto pf = std::make_shared<Portfolio<D>>("DummyPortfolio");
    pf->addSecurity(createDummySecurity());
    return pf;
}

StrategyContext<D> makeStrategyContext(const std::shared_ptr<PalStrategy<D>>& strat, D baseline) {
    StrategyContext<D> ctx;
    ctx.strategy = strat;
    ctx.baselineStat = baseline;
    ctx.count = 0;
    return ctx;
}

} // anonymous namespace

// Tests for MastersRomanoWolfImproved

TEST_CASE("MastersRomanoWolfImproved handles empty data") {
    MastersRomanoWolfImproved<D, DummyStatPolicy<D>> algo;
    std::vector<StrategyContext<D>> data;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    auto pvals = algo.run(data, 100, bt, portfolio, D("0.05"));
    REQUIRE(pvals.empty());
}

TEST_CASE("MastersRomanoWolfImproved throws on null backtester") {
    MastersRomanoWolfImproved<D, DummyStatPolicy<D>> algo;
    auto portfolio = createDummyPortfolio();

    auto strat = std::make_shared<DummyPalStrategyEx>(portfolio);
    StrategyContext<D> ctx = makeStrategyContext(strat, D("0.5"));
    std::vector<StrategyContext<D>> data{ctx};
    REQUIRE_THROWS_AS(algo.run(data, 50, nullptr, portfolio, D("0.05")), std::runtime_error);
}

TEST_CASE("MastersRomanoWolfImproved basic single-strategy test") {
    MastersRomanoWolfImproved<D, DummyStatPolicy<D>> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto portfolio = createDummyPortfolio();
    auto strat = std::make_shared<DummyPalStrategyEx>(portfolio);
    StrategyContext<D> ctx = makeStrategyContext(strat, D("0.5"));
    std::vector<StrategyContext<D>> data{ctx};

    auto pvals = algo.run(data, 10, bt, portfolio, D("1.0"));
    REQUIRE(pvals.size() == 1);
    REQUIRE(pvals[strat] == D("1.0"));
}

TEST_CASE("MastersRomanoWolfImproved works with multiple strategies") {
    MastersRomanoWolfImproved<D, DummyStatPolicy<D>> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto pf = createDummyPortfolio();

    std::vector<StrategyContext<D>> data;
    std::vector<std::shared_ptr<PalStrategy<D>>> strats;
    for (int i = 0; i < 4; ++i) {
        auto s = std::make_shared<DummyPalStrategyEx>(pf);
        strats.push_back(s);
        data.push_back(makeStrategyContext(s, D("0.5")));
    }

    auto pvals = algo.run(data, 20, bt, pf, D("1.0"));
    REQUIRE(pvals.size() == strats.size());
    for (auto& s : strats) REQUIRE(pvals[s] == D("1.0"));
}

TEST_CASE("MastersRomanoWolfImproved low statistic raw p-value") {
    MastersRomanoWolfImproved<D, AlwaysLowStatPolicy<D>> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto pf = createDummyPortfolio();

    std::vector<StrategyContext<D>> data;
    for (int i = 0; i < 3; ++i)
        data.push_back(makeStrategyContext(std::make_shared<DummyPalStrategyEx>(pf), D("0.5")));

     unsigned long m = 5;
    D alpha("0.4");
    auto pvals = algo.run(data, m, bt, pf, alpha);
    REQUIRE(pvals.size() == data.size());
    // When statistic is always < alpha, raw p-value = 1/(m+1) == 1/6
    D expected = D("1") / D("6"); 
    for (auto& kv : pvals)
        REQUIRE(kv.second == expected);
}

TEST_CASE("MastersRomanoWolfImproved enforces step-down monotonicity") {
    MastersRomanoWolfImproved<D, RandomStatPolicy<D>> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto pf = createDummyPortfolio();

    std::srand(42);
    std::vector<StrategyContext<D>> data;
    for (int i = 0; i < 5; ++i) {
        auto s = std::make_shared<DummyPalStrategyEx>(pf);
        D stat(std::to_string(0.3 + (std::rand() % 100) / 1000.0));
        data.push_back(makeStrategyContext(s, stat));
    }
    std::sort(data.begin(), data.end(), [](auto const&a, auto const&b){ return a.baselineStat > b.baselineStat; });

    auto pvals = algo.run(data, 100, bt, pf, D("0.05"));
    D prev = D("0.0");
    for (auto& ctx : data) {
        auto v = pvals[ctx.strategy];
        REQUIRE(v >= prev);
        REQUIRE((v >= D("0.0") && v <= D("1.0")));
        prev = v;
    }
}

TEST_CASE("MastersRomanoWolfImproved throws on unsorted data") {
    MastersRomanoWolfImproved<D, DummyStatPolicy<D>> algo;
    auto bt = std::make_shared<DummyBackTesterEx>();
    auto pf = createDummyPortfolio();
    auto s1 = std::make_shared<DummyPalStrategyEx>(pf);
    auto s2 = std::make_shared<DummyPalStrategyEx>(pf);
    // ascending baselines
    StrategyContext<D> c1 = makeStrategyContext(s1, D("0.2"));
    StrategyContext<D> c2 = makeStrategyContext(s2, D("0.8"));
    std::vector<StrategyContext<D>> data{c1, c2};
    REQUIRE_THROWS_AS(algo.run(data, 10, bt, pf, D("0.05")), std::invalid_argument);
}

// Integration test with real data

template<typename Decimal>
struct ProfitFactorPolicy {
    static Decimal getPermutationTestStatistic(const std::shared_ptr<BackTester<Decimal>>& bt) {
        auto it = bt->beginStrategies();
        return (*it)->getStrategyBroker().getClosedPositionHistory().getLogProfitFactor();
    }
    static unsigned int getMinStrategyTrades() { return 3; }
};

TEST_CASE("MastersRomanoWolfImproved integration with real price patterns and series", "[integration]") {
    auto series = getRandomPriceSeries();
    REQUIRE(series);
    auto security = std::make_shared<EquitySecurity<D>>("QQQ", "RealSec", series);
    auto bt = BackTesterFactory<D>::getBackTester(series->getTimeFrame(), series->getFirstDate(), series->getLastDate());
    auto patterns = getRandomPricePatterns();
    
    REQUIRE(patterns);
    auto contexts = StrategyDataPreparer<D, ProfitFactorPolicy<D>>::prepare(bt, security, patterns);
    REQUIRE(!contexts.empty());
    
    std::sort(contexts.begin(), contexts.end(), [](auto const&a, auto const&b)
    { return a.baselineStat > b.baselineStat; });
    
    auto pf = std::make_shared<Portfolio<D>>(security->getName() + " PF"); pf->addSecurity(security);
    MastersRomanoWolfImproved<D, ProfitFactorPolicy<D>> algo;
    auto pvals = algo.run(contexts, 2500, bt, pf, D("0.05"));
    REQUIRE(pvals.size() == contexts.size());
    D prev = D("0.0");
    for (auto& ctx : contexts) {
        auto v = pvals[ctx.strategy];
        REQUIRE(v >= prev);
        REQUIRE((v >= D("0.0") && v <= D("1.0")));
        prev = v;
    }
}
