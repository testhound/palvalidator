#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <map>
#include <memory>
#include <vector>

#include "MetaLosingStreakBootstrapBound.h"
#include "ParallelExecutors.h"
#include "number.h"
#include "randutils.hpp"

using num::DefaultNumber;

// ---- Minimal stub to mimic ClosedPositionHistory<Decimal> for unit testing ----
template <class Decimal>
struct FakeTradingPosition {
    explicit FakeTradingPosition(Decimal r) : r_(r) {}
    Decimal getPercentReturn() const { return r_; }
    Decimal r_;
};

template <class Decimal>
class FakeClosedPositionHistory {
public:
    using Map = std::map<int, std::shared_ptr<FakeTradingPosition<Decimal>>>;

    // API expected by MetaLosingStreakBootstrapBound:
    std::size_t getNumPositions() const { return positions_.size(); }
    typename Map::const_iterator beginTradingPositions() const { return positions_.begin(); }
    typename Map::const_iterator endTradingPositions() const { return positions_.end(); }

    // Test helper: add a trade with controlled P&L; trades are ordered by insertion key.
    void addTrade(int key, Decimal percentReturn) {
        positions_.emplace(key, std::make_shared<FakeTradingPosition<Decimal>>(percentReturn));
    }

private:
    Map positions_;
};

TEST_CASE("MetaLosingStreakBootstrapBound: observedStreak correctness on simple sequences", "[LosingStreak][Observed]")
{
    using D = DefaultNumber;
    using Bounder = mkc_timeseries::MetaLosingStreakBootstrapBound<D>;

    concurrency::SingleThreadExecutor exec;
    randutils::mt19937_rng rng; // seeding doesn't matter for observed

    Bounder::Options opts; // defaults fine
    Bounder bounder(exec, rng, opts);

    // Case 1: no losses
    FakeClosedPositionHistory<D> cph1;
    for (int i = 0; i < 10; ++i) cph1.addTrade(i, D(0.01)); // all positive
    REQUIRE(bounder.observedStreak(cph1) == 0);

    // Case 2: alternating W/L → longest losing streak = 1
    FakeClosedPositionHistory<D> cph2;
    for (int i = 0; i < 12; ++i) {
        const bool loss = (i % 2 == 1);
        cph2.addTrade(i, loss ? D(-0.01) : D(0.01));
    }
    REQUIRE(bounder.observedStreak(cph2) == 1);

    // Case 3: one run of 4 losses
    FakeClosedPositionHistory<D> cph3;
    // W W L L L L W
    std::vector<double> seq = {+1,+1,-1,-1,-1,-1,+1};
    for (int i = 0; i < static_cast<int>(seq.size()); ++i)
        cph3.addTrade(i, D(seq[i] * 0.01));
    REQUIRE(bounder.observedStreak(cph3) == 4);
}

TEST_CASE("MetaLosingStreakBootstrapBound: bootstrap bound extremes (all wins / all losses)", "[LosingStreak][Bootstrap]")
{
    using D = DefaultNumber;
    using Bounder = mkc_timeseries::MetaLosingStreakBootstrapBound<D>;

    // Use single-threaded executor for determinism
    concurrency::SingleThreadExecutor exec;

    // Deterministic seed → reproducible seeds vector
    randutils::seed_seq_fe128 s{1u,2u,3u,4u};
    randutils::mt19937_rng rng(s);

    Bounder::Options opts;
    opts.B = 2000;
    opts.alpha = 0.05;
    opts.sampleFraction = 1.0;
    opts.treatZeroAsLoss = false;

    Bounder bounder(exec, rng, opts);

    // All wins → longest losing streak should be 0 in every bootstrap replicate
    FakeClosedPositionHistory<D> allWins;
    for (int i = 0; i < 25; ++i) allWins.addTrade(i, D(0.01));
    REQUIRE(bounder.computeUpperBound(allWins) == 0);

    // All losses → every bootstrap replicate is all losses; Lmax == N exactly
    FakeClosedPositionHistory<D> allLosses;
    for (int i = 0; i < 30; ++i) allLosses.addTrade(i, D(-0.01));
    REQUIRE(bounder.computeUpperBound(allLosses) == 30);
}

TEST_CASE("MetaLosingStreakBootstrapBound: m-out-of-n reduces (or maintains) the upper bound", "[LosingStreak][Bootstrap]")
{
    using D = DefaultNumber;
    using Bounder = mkc_timeseries::MetaLosingStreakBootstrapBound<D>;

    // Executor setups
    concurrency::SingleThreadExecutor exec;

    // Use identical base seed for both runs so the only difference is sampleFraction
    randutils::seed_seq_fe128 sA{2025u, 11u, 01u, 1u};
    randutils::mt19937_rng rngA(sA);

    // Prepare a mixed-loss series with modest clustering
    FakeClosedPositionHistory<D> cph;
    // W W L L W L L W W W L L L W ...
    const std::vector<int> pattern = {+1,+1,-1,-1,+1,-1,-1,+1,+1,+1,-1,-1,-1,+1,+1,-1,+1,-1};
    for (int i = 0; i < static_cast<int>(pattern.size()); ++i)
        cph.addTrade(i, D(0.01 * pattern[i]));

    // Baseline: sampleFraction = 1.0
    Bounder::Options oFull; oFull.B = 3000; oFull.alpha = 0.05; oFull.sampleFraction = 1.0;
    Bounder boundFull(exec, rngA, oFull);
    const int ub_full = boundFull.computeUpperBound(cph);

    // Re-seed for a fair comparison (new independent stream)
    randutils::seed_seq_fe128 sB{2025u, 11u, 01u, 2u};
    randutils::mt19937_rng rngB(sB);

    // m-out-of-n: smaller m ⇒ bound should not exceed the full-sample bound too often; usually ≤
    Bounder::Options oFrac = oFull; oFrac.sampleFraction = 0.7;
    Bounder boundFrac(exec, rngB, oFrac);
    const int ub_frac = boundFrac.computeUpperBound(cph);

    REQUIRE(ub_frac <= ub_full);
}

TEST_CASE("MetaLosingStreakBootstrapBound: determinism across SingleThread vs ThreadPool (given same seeds vector)", "[LosingStreak][Bootstrap][Parallel]")
{
    using D = DefaultNumber;
    using Sampler = mkc_timeseries::StationaryTradeBlockSampler<D>;
    using Rng = randutils::mt19937_rng;
    
    // Explicitly specify template parameters for different executors
    using BounderST = mkc_timeseries::MetaLosingStreakBootstrapBound<D, Sampler, concurrency::SingleThreadExecutor, Rng>;
    using BounderTP = mkc_timeseries::MetaLosingStreakBootstrapBound<D, Sampler, concurrency::ThreadPoolExecutor<>, Rng>;

    // Build a moderately sized trade list with mixed outcomes
    FakeClosedPositionHistory<D> cph;
    for (int i = 0; i < 100; ++i) {
        // ~40% losses in modest clusters
        const bool loss = ( (i % 7 == 2) || (i % 7 == 3) || (i % 7 == 6) );
        cph.addTrade(i, loss ? D(-0.01) : D(0.01));
    }

    // Options for single-thread
    BounderST::Options optsST;
    optsST.B = 4000;
    optsST.alpha = 0.05;
    optsST.sampleFraction = 1.0;

    // Options for thread pool
    BounderTP::Options optsTP;
    optsTP.B = 4000;
    optsTP.alpha = 0.05;
    optsTP.sampleFraction = 1.0;

    // Identical base seed for both runs → same per-replicate seeds
    randutils::seed_seq_fe128 s{77u,88u,99u,11u};
    randutils::mt19937_rng rng1(s);
    randutils::mt19937_rng rng2(s);

    // Single-thread executor
    concurrency::SingleThreadExecutor exec1;
    BounderST boundST(exec1, rng1, optsST);
    const int ub_st = boundST.computeUpperBound(cph);

    // Thread pool executor
    concurrency::ThreadPoolExecutor<> execTP;
    BounderTP boundTP(execTP, rng2, optsTP);
    const int ub_tp = boundTP.computeUpperBound(cph);

    // Because the seeds vector is pre-generated from the same RNG stream,
    // results should match even with different executors.
    REQUIRE(ub_tp == ub_st);
}
