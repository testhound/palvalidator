#pragma once
#include <vector>
#include <algorithm>
#include <future>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "ParallelFor.h"
#include "ParallelExecutors.h"
#include "ClosedPositionHistory.h"
#include "randutils.hpp"
#include "RngUtils.h"

namespace mkc_timeseries {

// ----------------------------- Sampler Concept ------------------------------
// Any Sampler<T, Rng> must provide:
//   void sample(const std::vector<T>& in, std::vector<T>& out, Rng& rng, std::size_t m) const;
// where `out.size() == m` on return, created by the sampler.
// Sampling happens at the TRADE level (indices are trade-ordered).

// ------------------ Default: Stationary Trade-Block Sampler -----------------
template <typename Decimal>
class StationaryTradeBlockSampler {
public:
  explicit StationaryTradeBlockSampler(std::size_t expectedBlockLenTrades = 4)
  : m_blockLen(expectedBlockLenTrades ? expectedBlockLenTrades : 1) {}

  template <class Rng>
  void sample(const std::vector<Decimal>& in,
              std::vector<Decimal>& out,
              Rng& rng,
              std::size_t m) const
  {
    const std::size_t n = in.size();
    if (n == 0 || m == 0) { out.clear(); return; }

    out.resize(m);

    // Draw starts uniformly; continue each block with prob (1 - 1/L), stop with 1/L
    const double pStop = 1.0 / static_cast<double>(m_blockLen);

    // Use common RNG utilities for uniform random generation
    using rng_utils::get_random_index;
    using rng_utils::get_random_uniform_01;

    std::size_t i = 0;
    while (i < m) {
      std::size_t s = get_random_index(rng, n);   // block start
      out[i++] = in[s];

      // advance circularly until a geometric stop or we reach m
      std::size_t j = (s + 1) % n;
      while (i < m) {
        const bool stop = (get_random_uniform_01(rng) < pStop);
        if (stop) break;
        out[i++] = in[j];
        j = (j + 1) % n;
      }
    }
  }

private:
  std::size_t m_blockLen;
};

// -------------------- Meta Losing Streak Bootstrap (Percentile) -------------
template <
  typename Decimal,
  typename Sampler  = StationaryTradeBlockSampler<Decimal>,
  typename Executor = concurrency::SingleThreadExecutor,
  typename Rng      = randutils::mt19937_rng       // high-quality auto-seeded
>
class MetaLosingStreakBootstrapBound {
public:
  struct Options {
    std::size_t B = 5000;          // bootstrap replicates
    double      alpha = 0.05;      // upper (1 - alpha) bound
    double      sampleFraction = 1.0; // m-out-of-n: m = floor(sampleFraction * N), in (0,1]
    bool        treatZeroAsLoss = false; // if exactly zero-return counts as loss
  };

  // Constructor: you pass in Executor & RNG; Sampler can be passed or default-constructed.
  MetaLosingStreakBootstrapBound(Executor& executor, Rng& rng, Options opts = Options{})
  : m_exec(executor), m_rng(rng), m_opts(opts), m_sampler(Sampler{}) {}

  MetaLosingStreakBootstrapBound(Executor& executor, Rng& rng, Sampler sampler, Options opts = Options{})
  : m_exec(executor), m_rng(rng), m_opts(opts), m_sampler(std::move(sampler)) {}

  // Compute observed streak on actual trades
  template <template<class> class ClosedPositionHistoryT>
  int observedStreak(const ClosedPositionHistoryT<Decimal>& cph) const {
    auto pnl = extractTradeReturns(cph);
    return longestLosingStreak(pnl);
  }

  // Compute the (1 - alpha) bootstrap upper bound for Lmax
  template <template<class> class ClosedPositionHistoryT>
  int computeUpperBound(const ClosedPositionHistoryT<Decimal>& cph)
  {
    // 1) Extract trade-ordered P&L/returns
    const std::vector<Decimal> pnl = extractTradeReturns(cph);
    const std::size_t n = pnl.size();
    if (n == 0) return 0;

    // 2) Determine m for m-out-of-n
    const std::size_t m = clampM(n, m_opts.sampleFraction);

    // 3) Precompute per-replicate seeds to avoid sharing RNG across threads
    std::vector<uint64_t> seeds(m_opts.B);
    for (std::size_t i = 0; i < m_opts.B; ++i) {
      // Get a 64-bit value using common RNG utilities
      seeds[i] = rng_utils::get_random_value(m_rng);
    }

    // 4) Run bootstrap replicates in parallel; each task uses its own RNG seeded from seeds[b]
    std::vector<int> stats(m_opts.B);
    concurrency::parallel_for_chunked(
      static_cast<uint32_t>(m_opts.B), m_exec,
      [&](uint32_t b) {
        // Thread-local RNG seeded deterministically from seeds[b]
        Rng localRng; localRng.seed(seeds[b]);

        // Resample m trades
        std::vector<Decimal> boot;
        boot.reserve(m);
        m_sampler.sample(pnl, boot, localRng, m);

        // Statistic
        stats[b] = longestLosingStreak(boot);
      }
    ); // uses your chunked scheduler :contentReference[oaicite:2]{index=2}

    // 5) (1 - alpha) empirical bound
    const std::size_t k = static_cast<std::size_t>(
      std::floor((1.0 - m_opts.alpha) * static_cast<double>(m_opts.B - 1))
    );
    std::nth_element(stats.begin(), stats.begin() + k, stats.end());
    return stats[k];
  }

private:
  Executor& m_exec;
  Rng&      m_rng;
  Options   m_opts;
  Sampler   m_sampler;

  // ---- Statistic: longest consecutive losses ----
  int longestLosingStreak(const std::vector<Decimal>& pnl) const {
    int cur = 0, best = 0;
    for (const auto& x : pnl) {
      const bool isLoss = (x < Decimal(0)) || (m_opts.treatZeroAsLoss && x == Decimal(0));
      if (isLoss) { ++cur; if (cur > best) best = cur; }
      else cur = 0;
    }
    return best;
  }

  // ---- Helper: clamp m for m-out-of-n ----
  static std::size_t clampM(std::size_t n, double frac) {
    if (n == 0) return 0;
    if (frac <= 0.0) frac = 1.0;
    if (frac > 1.0)  frac = 1.0;
    std::size_t m = static_cast<std::size_t>(std::floor(frac * static_cast<double>(n)));
    return m == 0 ? std::min<std::size_t>(1, n) : m;
  }

  // ---- Extract trade-ordered returns from ClosedPositionHistory ----
  template <template<class> class ClosedPositionHistoryT>
  static std::vector<Decimal> extractTradeReturns(const ClosedPositionHistoryT<Decimal>& cph) {
    std::vector<Decimal> r;
    r.reserve(static_cast<std::size_t>(cph.getNumPositions()));

    // Assumes cph iterates trades in chronological (entry-time) order.
    for (auto it = cph.beginTradingPositions(); it != cph.endTradingPositions(); ++it) {
      const auto& pos = it->second; // shared_ptr<TradingPosition<Decimal>>
      r.push_back(pos->getPercentReturn()); // signed, long/short aware
    }
    return r;
  }
};

} // namespace mkc_timeseries
