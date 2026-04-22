/**
 * @file StrategyAutoBootstrap.h
 * @brief Strategy-level orchestrator for automatic bootstrap CI selection.
 *
 * Defines BootstrapConfiguration (replication count, confidence level, time frame)
 * and BootstrapAlgorithmsConfiguration (per-method enable flags and parameters),
 * plus the StrategyAutoBootstrap driver that runs all enabled methods and
 * delegates winner selection to AutoBootstrapSelector.
 *
 * Copyright (C) MKC Associates, LLC — All Rights Reserved.
 */

#pragma once

#include <vector>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <cmath>
#include <mutex>
#include <map>
#include <string>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include "BootstrapTypes.h"
#include "TradingBootstrapFactory.h"
#include "AutoBootstrapSelector.h"
#include "MOutOfNPercentileBootstrap.h"
#include "PercentileTBootstrap.h"
#include "BasicBootstrap.h"
#include "NormalBootstrap.h"
#include "PercentileBootstrap.h"
#include "BiasCorrectedBootstrap.h"
#include "BacktesterStrategy.h"
#include "StatUtils.h"
#include "StationaryMaskResamplers.h"
#include "ParallelExecutors.h"
#include "TradeResampling.h"

namespace palvalidator
{
  namespace analysis
  {
    // Forward declarations
    using mkc_timeseries::StatisticSupport;
    using palvalidator::analysis::IntervalType;

    /**
     * @brief Immutable configuration of bootstrap parameters for a single strategy/statistic.
     */
    class BootstrapConfiguration
    {
    public:
      BootstrapConfiguration(std::size_t numBootStrapReplications,
                             std::size_t blockSize,
                             double confidenceLevel,
                             std::uint64_t stageTag,
                             std::uint64_t fold,
                             bool rescaleMOutOfN = true,
			     bool enableTradeLevelBootstrapping = false)
        : m_numBootStrapReplications(numBootStrapReplications),
          m_blockSize(blockSize),
          m_confidenceLevel(confidenceLevel),
          m_stageTag(stageTag),
          m_fold(fold),
          m_rescaleMOutOfN(rescaleMOutOfN),
	  m_enableTradeLevelBootstrapping(enableTradeLevelBootstrapping)
      {}

      std::size_t getNumBootStrapReplications() const
      {
        return m_numBootStrapReplications;
      }

      std::size_t getBlockSize() const
      {
        return m_blockSize;
      }

      double getConfidenceLevel() const
      {
        return m_confidenceLevel;
      }

      std::uint64_t getStageTag() const
      {
        return m_stageTag;
      }

      std::uint64_t getFold() const
      {
        return m_fold;
      }

      bool getRescaleMOutOfN() const
      {
        return m_rescaleMOutOfN;
      }

      bool isTradeLevelBootstrappingEnabled() const
      {
	return m_enableTradeLevelBootstrapping;
      }

      /// Outer B for Percentile-T bootstrap.
      std::size_t getPercentileTNumOuterReplications() const
      {
        return m_numBootStrapReplications;
      }

      /// Compute the number of inner Percentile-T replicates as B_outer / ratio.
      ///
      /// No upper cap is applied: the PercentileTBootstrap engine's adaptive early
      /// stopping (halts when SE* stabilises to +/-1.5%) governs actual compute cost,
      /// making a hard cap here redundant and — more importantly — incorrect: the cap
      /// previously caused B_inner to be independent of B_outer at all production
      /// replication counts (B_outer >= 20,000 with ratio=10 → B_inner=2000=cap).
      ///
      /// Only a lower floor is enforced so that degenerate ratios or very small
      /// B_outer values never produce fewer than percentile_t_constants::MIN_INNER
      /// inner draws (the engine's own hard requirement for SE* stability).
      ///
      /// Example B_inner values at ratio = 10:
      ///   B_outer =  5,000 → B_inner =   500
      ///   B_outer = 10,000 → B_inner = 1,000
      ///   B_outer = 25,000 → B_inner = 2,500
      ///   B_outer = 50,000 → B_inner = 5,000
      std::size_t getPercentileTNumInnerReplications(double ratio) const
      {
	// Minimum inner draws required for stable SE* estimation (engine hard gate).
	constexpr std::size_t kMinInnerReplications = percentile_t_constants::MIN_INNER;

	// Guard against nonsensical ratio — fall back to minimum workable inner size.
	if (!std::isfinite(ratio) || !(ratio > 0.0))
	  return kMinInnerReplications;

	const double inner_d =
	  static_cast<double>(m_numBootStrapReplications) / ratio;

	// Floor only — no cap. See doc comment above for rationale.
	return static_cast<std::size_t>(
	  std::max(inner_d, static_cast<double>(kMinInnerReplications)));
      }

    private:
      std::size_t   m_numBootStrapReplications;
      std::size_t   m_blockSize;
      double        m_confidenceLevel;
      std::uint64_t m_stageTag;
      std::uint64_t m_fold;
      bool          m_rescaleMOutOfN;
      bool m_enableTradeLevelBootstrapping;
    };

    /**
     * @brief Configuration of which bootstrap algorithms are enabled.
     *
     * All flags default to true, and there are no setters (immutable after construction).
     */
    class BootstrapAlgorithmsConfiguration
    {
    public:
      explicit BootstrapAlgorithmsConfiguration(bool enableNormal     = true,
                                                bool enableBasic      = true,
                                                bool enablePercentile = true,
                                                bool enableMOutOfN    = true,
                                                bool enablePercentileT = true,
                                                bool enableBCa        = true)
        : m_enableNormal(enableNormal),
          m_enableBasic(enableBasic),
          m_enablePercentile(enablePercentile),
          m_enableMOutOfN(enableMOutOfN),
          m_enablePercentileT(enablePercentileT),
          m_enableBCa(enableBCa)
      {}

      bool enableNormal() const
      {
        return m_enableNormal;
      }

      bool enableBasic() const
      {
        return m_enableBasic;
      }

      bool enablePercentile() const
      {
        return m_enablePercentile;
      }

      bool enableMOutOfN() const
      {
        return m_enableMOutOfN;
      }

      bool enablePercentileT() const
      {
        return m_enablePercentileT;
      }

      bool enableBCa() const
      {
        return m_enableBCa;
      }
      
    private:
      bool m_enableNormal;
      bool m_enableBasic;
      bool m_enablePercentile;
      bool m_enableMOutOfN;
      bool m_enablePercentileT;
      bool m_enableBCa;
    };

    /**
     * @brief Cross-strategy aggregator that records BCa tournament outcomes
     * and produces a summary report after all strategies have been bootstrapped.
     *
     * Designed to answer questions such as:
     *   - How many strategies ended up NOT choosing BCa? What percentage?
     *   - Of those, how many were "BCa competed and was outscored" vs. "BCa
     *     was penalized for Tier-2/3 instability" vs. "BCa was hard-disqualified"?
     *   - When BCa lost, did it typically lose to PercentileT or MOutOfN?
     *   - Does trade count correlate with BCa selection rate? (i.e. is the
     *     "BCa struggles at low n" theory supported by the data?)
     *   - Do different statistics (GeoMean vs PF) produce different BCa selection
     *     patterns on the same strategies?
     *
     * Usage:
     *   BCaSelectionAggregator<Decimal> agg;
     *   for (each strategy) {
     *     auto result = strategy.run(returns, os, "PF", &agg);   // pass &agg
     *   }
     *   agg.summarize(std::cout);                                 // at end
     *
     * Thread-safety: record() is mutex-guarded, so the same aggregator can be
     * passed to StrategyAutoBootstrap instances running concurrently across a
     * thread pool. Contention is negligible since record() is called once per
     * strategy run and completes in microseconds.
     *
     * The aggregator is template-parameterized on Decimal so it matches the
     * Candidate/Diagnostics types produced by StrategyAutoBootstrap<Decimal, …>.
     * All StrategyAutoBootstrap instantiations that share a Decimal type can
     * write to the same aggregator.
     */
    template <class Decimal>
    class BCaSelectionAggregator
    {
    public:
      using Result     = AutoCIResult<Decimal>;
      using MethodId   = typename Result::MethodId;
      using Candidate  = typename Result::Candidate;

      /// Why BCa wasn't the winner. The first four values mirror the branches
      /// of the existing per-strategy "BCa not selected — …" log block.
      /// kChosen means BCa won; the aggregator still records these so the
      /// summary can report a BCa-win rate.
      enum class Outcome
      {
        kChosen,                    // BCa won the tournament
        kOutscoredCleanly,          // BCa competed cleanly; lost on score
        kPenalizedAccelUnreliable,  // Tier-2 sensitivity failed; competed; lost
        kPenalizedNonMonotone,      // Tier-2 transform reversed; competed; lost
        kPenalizedBoth,             // Both soft penalties; competed; lost
        kDisqualifiedHard,          // Hard gate (z0/accel/skew/n/B_eff)
        kDisqualifiedLength,        // Interval too wide
        kDisqualifiedNonFinite,     // Non-finite z0/accel
        kDisqualifiedDomain,        // Domain constraint violated
        kNoBCaCandidate             // BCa didn't even produce a candidate
      };

      /// One row per strategy.run() call. Stored verbatim for post-hoc analysis.
      struct Record
      {
        std::string statistic_name;   // caller-supplied (e.g. "GeoMean", "PF")
        std::size_t n_trades = 0;
        MethodId    winning_method = MethodId::BCa;  // actually chosen method
        Outcome     outcome = Outcome::kChosen;

        // BCa candidate diagnostics (valid when a BCa candidate existed, i.e.
        // outcome != kNoBCaCandidate). Stored regardless of win/loss so the
        // summary can compare winning-BCa distributions to losing-BCa ones.
        bool        has_bca_candidate = false;
        double      bca_z0 = 0.0;
        double      bca_accel = 0.0;
        double      bca_skew_boot = 0.0;
        bool        bca_accel_reliable = true;
        bool        bca_transform_monotone = true;
        double      bca_stability_penalty = 0.0;
        double      bca_score = 0.0;

        // Winner's score (even when BCa won, this equals bca_score).
        double      winner_score = 0.0;
      };

      BCaSelectionAggregator() = default;

      /// Thread-safe. Extracts diagnostics from @p result and stores one Record.
      /// If @p statistic_name is empty, stores "unspecified".
      void record(const Result& result,
                  std::size_t n_trades,
                  std::string statistic_name = "")
      {
        Record rec;
        rec.statistic_name = statistic_name.empty() ? "unspecified"
                                                    : std::move(statistic_name);
        rec.n_trades = n_trades;

        const auto& diag = result.getDiagnostics();
        rec.winning_method = diag.getChosenMethod();
        rec.winner_score   = diag.getChosenScore();

        rec.has_bca_candidate = diag.hasBCaCandidate();

        if (rec.has_bca_candidate)
        {
          // Find the BCa candidate to harvest z0/accel/skew/reliability fields.
          for (const auto& cand : result.getCandidates())
          {
            if (cand.getMethod() != MethodId::BCa) continue;
            rec.bca_z0                  = cand.getZ0();
            rec.bca_accel               = cand.getAccel();
            rec.bca_skew_boot           = cand.getSkewBoot();
            rec.bca_accel_reliable      = cand.getAccelIsReliable();
            rec.bca_transform_monotone  = cand.getBcaTransformMonotone();
            rec.bca_stability_penalty   = cand.getStabilityPenalty();
            rec.bca_score               = cand.getScore();
            break;
          }
        }

        rec.outcome = classifyOutcome(diag, rec);

        {
          std::lock_guard<std::mutex> lk(m_mutex);
          m_records.push_back(std::move(rec));
        }
      }

      /// Number of strategies recorded across all statistics.
      std::size_t size() const
      {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_records.size();
      }

      /// Copy of all records. Primarily for testing / ad-hoc analysis.
      std::vector<Record> getRecords() const
      {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_records;
      }

      /// Human-readable label for an Outcome enum value.
      static const char* outcomeLabel(Outcome o)
      {
        switch (o)
        {
          case Outcome::kChosen:                    return "BCa chosen";
          case Outcome::kOutscoredCleanly:          return "outscored (clean BCa, no instability)";
          case Outcome::kPenalizedAccelUnreliable:  return "penalized: accel unreliable (Tier-2)";
          case Outcome::kPenalizedNonMonotone:      return "penalized: non-monotone transform";
          case Outcome::kPenalizedBoth:             return "penalized: accel unreliable + non-monotone";
          case Outcome::kDisqualifiedHard:          return "hard-disqualified (z0/accel/skew/n/B_eff gate)";
          case Outcome::kDisqualifiedLength:        return "hard-disqualified: interval too wide";
          case Outcome::kDisqualifiedNonFinite:     return "hard-disqualified: non-finite z0/accel";
          case Outcome::kDisqualifiedDomain:        return "hard-disqualified: domain violation";
          case Outcome::kNoBCaCandidate:            return "no BCa candidate produced";
        }
        return "unknown";
      }

      /// Emit the full summary report to @p os. Safe to call from any thread
      /// once all record() calls have completed.
      void summarize(std::ostream& os) const
      {
        std::lock_guard<std::mutex> lk(m_mutex);

        // RAII stream-state guard.
        // The ostream we receive may have arbitrary std::setfill / std::setw
        // / flags / precision set by earlier code in the caller. In particular,
        // if a prior formatter set std::setfill('0') (common for zero-padded
        // numeric output) and never restored it, every std::setw padded field
        // emitted below would be filled with '0' characters instead of spaces
        // — producing output like "GeoMean00000000" everywhere. Guard against
        // that by forcing a known clean state at entry and restoring the
        // caller's state at exit.
        struct StreamStateGuard
        {
          std::ostream& s;
          std::ios::fmtflags flags;
          std::streamsize    prec;
          std::streamsize    width;
          char               fill;
          explicit StreamStateGuard(std::ostream& o)
            : s(o), flags(o.flags()), prec(o.precision()),
              width(o.width()), fill(o.fill())
          {
            s.fill(' ');
            s.unsetf(std::ios::adjustfield | std::ios::floatfield |
                     std::ios::basefield);
            s << std::dec;
          }
          ~StreamStateGuard()
          {
            s.flags(flags);
            s.precision(prec);
            s.width(width);
            s.fill(fill);
          }
        } guard(os);

        os << "\n";
        os << "========================================================================\n";
        os << "                   BCa Selection Summary Report\n";
        os << "========================================================================\n";

        if (m_records.empty())
        {
          os << "No strategies recorded.\n";
          os << "========================================================================\n";
          return;
        }

        // Section 1: Overall outcome breakdown
        emitOverallBreakdown(os);

        // Section 2: Per-statistic breakdown (so GeoMean vs PF can be compared)
        emitPerStatisticBreakdown(os);

        // Section 3: Who won when BCa lost (BCa vs PercentileT vs MOutOfN)
        emitHeadToHeadBreakdown(os);

        // Section 4: Trade-count analysis — is "BCa loses at low n" supported?
        emitTradeCountAnalysis(os);

        // Section 5: BCa diagnostic distribution when BCa lost
        emitBcaLostDiagnostics(os);

        os << "========================================================================\n";
      }

    private:
      // Determine the Outcome category from diagnostics plus harvested BCa fields.
      // Mirrors the logic of the per-strategy "BCa not selected — …" log block in
      // run() (see lines 707–756 of this file) to keep reporting consistent.
      static Outcome classifyOutcome(
          const typename Result::SelectionDiagnostics& diag,
          const Record& rec)
      {
        if (diag.isBCaChosen())
          return Outcome::kChosen;

        if (!diag.hasBCaCandidate())
          return Outcome::kNoBCaCandidate;

        if (diag.wasBCaRejectedForNonFiniteParameters())
          return Outcome::kDisqualifiedNonFinite;
        if (diag.wasBCaRejectedForDomain())
          return Outcome::kDisqualifiedDomain;

        if (diag.wasBCaRejectedForInstability())
        {
          // Same categorisation as in run(): inspect the BCa candidate to
          // distinguish SOFT penalty cases (BCa competed, lost on score) from
          // HARD gate cases (BCa never entered the tournament).
          const bool accel_bad   = !rec.bca_accel_reliable;
          const bool nonmono_bad = !rec.bca_transform_monotone;

          if (accel_bad && nonmono_bad) return Outcome::kPenalizedBoth;
          if (accel_bad)                return Outcome::kPenalizedAccelUnreliable;
          if (nonmono_bad)              return Outcome::kPenalizedNonMonotone;
          return Outcome::kDisqualifiedHard;   // z0/accel/skew/n/B_eff hard gate
        }

        if (diag.wasBCaRejectedForLength())
          return Outcome::kDisqualifiedLength;

        // BCa competed, no instability flags, no length disqualification — just
        // outscored by another method.
        return Outcome::kOutscoredCleanly;
      }

      // --- Summary section helpers (no locking — caller holds the mutex) ---

      static std::string methodName(MethodId m)
      {
        return Result::methodIdToString(m);
      }

      // Formats "n (pct%)" with 1 decimal place on the percentage.
      static std::string fmtCountPct(std::size_t n, std::size_t denom)
      {
        std::ostringstream oss;
        oss << n;
        if (denom > 0)
        {
          const double pct = 100.0 * static_cast<double>(n) /
                             static_cast<double>(denom);
          oss << " (" << std::fixed << std::setprecision(1) << pct << "%)";
        }
        return oss.str();
      }

      void emitOverallBreakdown(std::ostream& os) const
      {
        const std::size_t N = m_records.size();
        os << "\n-- Overall (" << N << " strategy-statistic pairs recorded) --\n";

        std::map<Outcome, std::size_t> counts;
        for (const auto& r : m_records) ++counts[r.outcome];

        // Compute BCa chosen vs not for the headline ratio
        const std::size_t n_chosen = counts[Outcome::kChosen];
        const std::size_t n_not    = N - n_chosen;

        os << "  BCa chosen:     " << fmtCountPct(n_chosen, N) << "\n";
        os << "  BCa not chosen: " << fmtCountPct(n_not,    N) << "\n";

        // Per-outcome breakdown for the non-chosen cases
        if (n_not > 0)
        {
          os << "\n  Breakdown of cases where BCa was NOT chosen:\n";
          static constexpr Outcome kOrder[] = {
            Outcome::kOutscoredCleanly,
            Outcome::kPenalizedAccelUnreliable,
            Outcome::kPenalizedNonMonotone,
            Outcome::kPenalizedBoth,
            Outcome::kDisqualifiedHard,
            Outcome::kDisqualifiedLength,
            Outcome::kDisqualifiedNonFinite,
            Outcome::kDisqualifiedDomain,
            Outcome::kNoBCaCandidate,
          };
          for (Outcome o : kOrder)
          {
            const std::size_t c = counts[o];
            if (c == 0) continue;
            os << "    " << std::left << std::setw(48) << outcomeLabel(o)
               << fmtCountPct(c, n_not) << "\n";
          }
        }
      }

      void emitPerStatisticBreakdown(std::ostream& os) const
      {
        // Group by statistic_name and emit a mini-table per statistic
        std::map<std::string, std::vector<const Record*>> by_stat;
        for (const auto& r : m_records) by_stat[r.statistic_name].push_back(&r);

        if (by_stat.size() <= 1) return;  // Nothing interesting to show

        // Column widths tuned for typical labels:
        //   Statistic  (GeoMean, PF, logPF)       : 10
        //   N          (counts up to 9999)        :  7
        //   chosen / not-chosen (formatted pct)   : 16 each
        os << "\n-- Per-statistic breakdown --\n";
        os << "  " << std::left
           << std::setw(10) << "Statistic"
           << std::setw(7)  << "N"
           << std::setw(16) << "chosen"
           << std::setw(16) << "not chosen" << "\n";

        for (const auto& [stat, records] : by_stat)
        {
          const std::size_t N = records.size();
          std::size_t n_chosen = 0;
          for (const Record* r : records)
            if (r->outcome == Outcome::kChosen) ++n_chosen;
          const std::size_t n_not = N - n_chosen;

          os << "  " << std::left
             << std::setw(10) << stat
             << std::setw(7)  << N
             << std::setw(16) << fmtCountPct(n_chosen, N)
             << std::setw(16) << fmtCountPct(n_not, N) << "\n";
        }
      }

      void emitHeadToHeadBreakdown(std::ostream& os) const
      {
        // When BCa lost, who won?
        std::map<MethodId, std::size_t> winners_when_lost;
        std::size_t total_lost = 0;

        for (const auto& r : m_records)
        {
          if (r.outcome == Outcome::kChosen) continue;
          if (!r.has_bca_candidate) continue;   // Only count BCa-vs-X comparisons
          ++winners_when_lost[r.winning_method];
          ++total_lost;
        }

        if (total_lost == 0) return;

        os << "\n-- When BCa lost, who won? (" << total_lost
           << " head-to-head outcomes) --\n";
        for (const auto& [m, c] : winners_when_lost)
        {
          os << "  vs. " << std::left << std::setw(14) << methodName(m)
             << " " << fmtCountPct(c, total_lost) << "\n";
        }
      }

      void emitTradeCountAnalysis(std::ostream& os) const
      {
        // Bucket by trade-count band to see if BCa selection correlates with n.
        // Buckets: [<10], [10-14], [15-19], [20-29], [30-49], [50+]
        struct Bucket
        {
          const char*   label;
          std::size_t   lo;
          std::size_t   hi;   // inclusive
          std::size_t   n_total = 0;
          std::size_t   n_bca_chosen = 0;
          std::size_t   n_bca_lost_competed = 0;  // competed and lost on score
          std::size_t   n_bca_hard = 0;           // hard-disqualified
          std::vector<std::size_t> trade_counts;  // for median/mean per bucket
        };
        std::vector<Bucket> buckets = {
          {"< 10",     0,   9,   0, 0, 0, 0, {}},
          {"10 - 14",  10,  14,  0, 0, 0, 0, {}},
          {"15 - 19",  15,  19,  0, 0, 0, 0, {}},
          {"20 - 29",  20,  29,  0, 0, 0, 0, {}},
          {"30 - 49",  30,  49,  0, 0, 0, 0, {}},
          {"50+",      50,  static_cast<std::size_t>(-1), 0, 0, 0, 0, {}},
        };

        for (const auto& r : m_records)
        {
          for (auto& b : buckets)
          {
            if (r.n_trades < b.lo || r.n_trades > b.hi) continue;
            ++b.n_total;
            b.trade_counts.push_back(r.n_trades);
            if      (r.outcome == Outcome::kChosen)                     ++b.n_bca_chosen;
            else if (r.outcome == Outcome::kOutscoredCleanly
                     || r.outcome == Outcome::kPenalizedAccelUnreliable
                     || r.outcome == Outcome::kPenalizedNonMonotone
                     || r.outcome == Outcome::kPenalizedBoth)           ++b.n_bca_lost_competed;
            else if (r.outcome != Outcome::kNoBCaCandidate)             ++b.n_bca_hard;
            break;
          }
        }

        os << "\n-- BCa selection rate by trade count"
              " (tests 'BCa struggles at low n' theory) --\n";
        os << "  " << std::left
           << std::setw(10) << "n"
           << std::setw(8)  << "count"
           << std::setw(15) << "BCa chosen"
           << std::setw(18) << "lost on score"
           << std::setw(16) << "hard-rejected" << "\n";

        for (const auto& b : buckets)
        {
          if (b.n_total == 0) continue;
          os << "  " << std::left
             << std::setw(10) << b.label
             << std::setw(8)  << b.n_total
             << std::setw(15) << fmtCountPct(b.n_bca_chosen,         b.n_total)
             << std::setw(18) << fmtCountPct(b.n_bca_lost_competed,  b.n_total)
             << std::setw(16) << fmtCountPct(b.n_bca_hard,           b.n_total)
             << "\n";
        }

        // If the "BCa struggles at low n" theory holds, BCa-chosen% rises
        // monotonically (or at least non-decreasing overall) with n. Print a
        // one-line interpretation aid.
        os << "\n  Interpretation: if the low-n theory is supported,"
              " 'BCa chosen' % should\n"
              "  rise with trade count. Flat or inverse pattern would suggest"
              " n is not the\n"
              "  primary driver of BCa selection on this corpus.\n";
      }

      // Five-number summary (min, p25, median, p75, max) of a vector of doubles.
      struct FiveNum
      {
        double min, p25, median, p75, max, mean;
        std::size_t n;
      };
      static FiveNum fiveNumber(std::vector<double> xs)
      {
        FiveNum f{0,0,0,0,0,0,xs.size()};
        if (xs.empty()) return f;
        std::sort(xs.begin(), xs.end());
        auto pick = [&](double q) {
          if (xs.size() == 1) return xs[0];
          const double idx = q * (xs.size() - 1);
          const std::size_t lo = static_cast<std::size_t>(std::floor(idx));
          const std::size_t hi = std::min(lo + 1, xs.size() - 1);
          const double frac = idx - lo;
          return xs[lo] * (1.0 - frac) + xs[hi] * frac;
        };
        f.min    = xs.front();
        f.max    = xs.back();
        f.p25    = pick(0.25);
        f.median = pick(0.50);
        f.p75    = pick(0.75);
        f.mean   = std::accumulate(xs.begin(), xs.end(), 0.0) /
                   static_cast<double>(xs.size());
        return f;
      }

      void emitBcaLostDiagnostics(std::ostream& os) const
      {
        std::vector<double> z0, accel, skew, stab;
        std::size_t n_accel_unreliable = 0;
        std::size_t n_nonmonotone = 0;
        std::size_t total = 0;

        for (const auto& r : m_records)
        {
          if (r.outcome == Outcome::kChosen) continue;
          if (!r.has_bca_candidate) continue;
          ++total;
          z0.push_back(r.bca_z0);
          accel.push_back(r.bca_accel);
          skew.push_back(r.bca_skew_boot);
          stab.push_back(r.bca_stability_penalty);
          if (!r.bca_accel_reliable)     ++n_accel_unreliable;
          if (!r.bca_transform_monotone) ++n_nonmonotone;
        }

        if (total == 0) return;

        os << "\n-- BCa diagnostic distribution when BCa did NOT win ("
           << total << " cases) --\n";
        os << "  Flag rates:\n";
        os << "    " << std::left << std::setw(32)
           << "accel_is_reliable = false:"
           << fmtCountPct(n_accel_unreliable, total) << "\n";
        os << "    " << std::left << std::setw(32)
           << "transform_monotone = false:"
           << fmtCountPct(n_nonmonotone, total) << "\n";

        // Helper: format a single "label=value" unit padded to fixed width.
        // We build each unit with an ostringstream so label+value travel as
        // one token and the padding between tokens is uniform regardless of
        // how many digits the value itself took.
        auto formatStat = [](const char* tag, double v) {
          std::ostringstream o;
          o << std::fixed << std::setprecision(4) << tag << "=" << v;
          std::string s = o.str();
          // Pad to width 14 so columns line up even when values have differing
          // magnitudes (e.g. "-0.1546" vs "1.0160"). 14 = max label "min=" (4)
          // + max value width "-0.1234" (7) + small trailing gap (3).
          if (s.size() < 14) s.append(14 - s.size(), ' ');
          return s;
        };

        auto emit = [&](const char* label, const std::vector<double>& xs) {
          const auto f = fiveNumber(xs);
          os << "    " << std::left << std::setw(22) << label
             << formatStat("min", f.min)
             << formatStat("p25", f.p25)
             << formatStat("med", f.median)
             << formatStat("p75", f.p75)
             << formatStat("max", f.max)
             << "\n";
        };

        os << "  Five-number summaries:\n";
        emit("z0",                  z0);
        emit("accel",               accel);
        emit("|accel|",             [&]{ auto v=accel; for(auto& x:v) x=std::fabs(x); return v; }());
        emit("skew(boot)",          skew);
        emit("stability_penalty",   stab);
      }

      mutable std::mutex  m_mutex;
      std::vector<Record> m_records;
    };

    /**
     * @brief Orchestrates running multiple bootstrap engines for a given strategy/statistic.
     *
     * Responsibilities:
     * - Uses a TradingBootstrapFactory to build concrete bootstrap engines.
     * - Accepts a configured statistic functor (Sampler) to support stateful stats (like robust PF).
     * - Converts each engine's result into an AutoBootstrapSelector::Candidate.
     * - Calls AutoBootstrapSelector<Decimal>::select(...) and returns AutoCIResult<Decimal>.
     */
    template <class Decimal, class Sampler, class Resampler,
              class SampleType = Decimal>
    class StrategyAutoBootstrap
    {
    public:
      using Num        = Decimal;
      using Result     = AutoCIResult<Decimal>;
      using Selector   = AutoBootstrapSelector<Decimal>;
      using MethodId   = typename Result::MethodId;
      using Candidate  = typename Result::Candidate;
      using Factory    = ::TradingBootstrapFactory<>;
      using Executor   = concurrency::ThreadPoolExecutor<>;

      // BCa resampler uses the same resampler as other methods for consistency in bootstrap tournaments.
      // Previously hardcoded to StationaryBlockResampler but now uses generic template parameter.

      /**
       * @brief Constructor accepting a specific statistic instance.
       *
       * @param sampler_instance An instance of Sampler. This allows passing a configured
       * statistic (e.g., LogProfitFactorStat with a specific stop-loss) rather than
       * default-constructing one. Defaults to Sampler() if not provided.
       * @param sharedExec Optional persistent thread pool. When non-null the same pool
       * is injected into every engine constructed inside run(), eliminating the
       * per-engine spawn/join cost. When null (the default) the constructor checks
       * whether the factory already carries a shared executor (set e.g. by
       * PerformanceFilter) and inherits it automatically. Only when neither source
       * supplies a pool is a private pool created. The caller is responsible for
       * keeping any externally-supplied executor alive at least as long as this object.
       */
      StrategyAutoBootstrap(Factory& factory,
                            const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                            const BootstrapConfiguration& bootstrapConfiguration,
                            const BootstrapAlgorithmsConfiguration& algorithmsConfiguration,
                            Sampler sampler_instance = Sampler(),
			    IntervalType interval_type = IntervalType::TWO_SIDED,
                            std::shared_ptr<Executor> sharedExec = nullptr)
        : m_factory(factory),
          m_strategy(strategy),
          m_bootstrapConfiguration(bootstrapConfiguration),
          m_algorithmsConfiguration(algorithmsConfiguration),
          m_sampler_instance(sampler_instance),
	  m_interval_type(interval_type),
          m_exec(sharedExec             ? std::move(sharedExec)
                 : factory.getSharedExecutor() ? factory.getSharedExecutor()
                 : std::make_shared<Executor>())
      {}

      /**
       * @brief Run all configured bootstrap engines on @p returns and select the best CI.
       *
       * @param returns         Bar-level return series (SampleType = Decimal) or trade-level
       *                        series (SampleType = Trade<Decimal>). The element type must match
       *                        the SampleType template parameter of this class.
       * @param os              Optional logging stream. If non-null, engine failures are logged.
       * @param statistic_name  Optional label (e.g. "GeoMean", "PF", "logPF"). Only used when
       *                        @p aggregator is non-null, to tag the recorded outcome so the
       *                        summary report can break results down by statistic.
       * @param aggregator      Optional cross-strategy aggregator. When non-null, the tournament
       *                        outcome for this strategy is recorded (thread-safely) for later
       *                        summarisation via aggregator->summarize(). Pass nullptr to disable.
       *
       * @return AutoCIResult<Decimal> encapsulating the chosen method and all candidates.
       *
       * @throws std::invalid_argument if @p returns contains fewer than 2 elements.
       * @throws std::runtime_error if no engine produced a usable candidate.
       */
      Result run(const std::vector<SampleType>& returns,
                 std::ostream* os = nullptr,
                 const std::string& statistic_name = std::string{},
                 BCaSelectionAggregator<Decimal>* aggregator = nullptr)
      {
	// CONCERN-B: verify that the runtime flag in BootstrapConfiguration agrees
	// with the compile-time SampleType deduction.
	//
	// isTradeLevelBootstrappingEnabled() is stored for external inspection only;
	// actual dispatch is controlled by "if constexpr (is_same_v<SampleType,Decimal>)"
	// below. A mismatch means the caller passed the wrong flag value for the
	// instantiated SampleType — the flag is wrong, not the code path.
	//
	// static_assert cannot reference member variables, so we emit a runtime
	// warning to the log stream when a mismatch is detected. Compile-time
	// enforcement would require the flag to be a template parameter.
	if (os)
	  {
	    constexpr bool isBarLevel = std::is_same_v<SampleType, Decimal>;
	    const bool flagSaysTradeLevel =
	      m_bootstrapConfiguration.isTradeLevelBootstrappingEnabled();
	    if (isBarLevel == flagSaysTradeLevel)  // flag disagrees with SampleType
	      {
		(*os) << "   [AutoCI] WARNING: isTradeLevelBootstrappingEnabled()="
		      << (flagSaysTradeLevel ? "true" : "false")
		      << " conflicts with compile-time SampleType ("
		      << (isBarLevel ? "bar-level/Decimal" : "trade-level")
		      << "). The flag is informational only; actual dispatch follows SampleType.\n";
	      }
	  }

	std::vector<Candidate> candidates;
	candidates.reserve(6);

	if (returns.size() < 2)
	  {
	    throw std::invalid_argument(
					"StrategyAutoBootstrap::run: requires at least 2 returns.");
	  }

	// Inject the persistent executor into the factory before any make* calls.
	// This ensures all six engines constructed below share the same thread pool
	// rather than each spawning and joining their own. The factory stores the
	// shared_ptr by value, so if this StrategyAutoBootstrap instance is the sole
	// owner, the pool remains live for the entire run().
	m_factory.setSharedExecutor(m_exec);

	const std::size_t blockSize         = m_bootstrapConfiguration.getBlockSize();
	const double      cl                = m_bootstrapConfiguration.getConfidenceLevel();
	const std::size_t B_single          = m_bootstrapConfiguration.getNumBootStrapReplications();
	const std::uint64_t stageTag        = m_bootstrapConfiguration.getStageTag();
	const std::uint64_t fold            = m_bootstrapConfiguration.getFold();
	const std::size_t B_outer_percentileT =
	  m_bootstrapConfiguration.getPercentileTNumOuterReplications();

	// B_inner = B_outer / kPercentileTInnerRatio.
	// The PercentileTBootstrap engine's adaptive early stopping means actual
	// inner work is typically ~180 iterations per outer replicate regardless
	// of this budget; the ratio governs the maximum spend when stopping takes
	// longer (high-skew or borderline-stable SE*).
	// No upper cap is applied in getPercentileTNumInnerReplications — see its
	// doc comment for the full rationale.
	constexpr double kPercentileTInnerRatio = 10.0;
	const std::size_t B_inner_percentileT =
	  m_bootstrapConfiguration.getPercentileTNumInnerReplications(kPercentileTInnerRatio);

	// Shared resampler for percentile-like / Percentile-t engines.
	// Bar-level resamplers are constructed with a block size; IIDResampler
	// (used at trade level) is a zero-argument struct.
	Resampler resampler = makeResampler(blockSize);

	        typename Selector::ScoringWeights weights;

        const bool isRatioStatistic = Sampler::isRatioStatistic();

        if (isRatioStatistic)
          {
            weights = typename Selector::ScoringWeights(/*wCenterShift*/ 0.25,
							/*wSkew*/        0.5,
							/*wLength*/      0.75,
							/*wStability*/   1.5);
          }
        else
          {
            weights = typename Selector::ScoringWeights(
                           /*wCenterShift*/ 1.0,
                           /*wSkew*/        0.5,
                           /*wLength*/      0.25,
                           /*wStability*/   1.0);
          }

	if (os)
	  {
	    (*os) << "   [AutoCI] Weight profile: "
		  << (isRatioStatistic ? "ratio-statistic" : "returns-based")
		  << "  (wCenterShift=" << weights.getCenterShiftWeight()
		  << "  wSkew="         << weights.getSkewWeight()
		  << "  wLength="       << weights.getLengthWeight()
		  << "  wStability="    << weights.getStabilityWeight()
		  << ")\n";
	  }

	// 1) Normal bootstrap
	if (m_algorithmsConfiguration.enableNormal())
	  {
	    try
	      {
		auto [engine, crn] =
		  m_factory.template makeNormal<Decimal, Sampler, Resampler, Executor, SampleType>(
										       B_single,
										       cl,
										       resampler,
										       m_strategy,
										       stageTag,
										       static_cast<uint64_t>(blockSize),
										       fold,
										       m_interval_type);

		// USE CONFIGURED SAMPLER INSTANCE
		auto res = engine.run(returns, m_sampler_instance, crn); 
		candidates.push_back(
				     Selector::template summarizePercentileLike(
										MethodId::Normal, engine, res));
	      }
	    catch (const std::exception& e)
	      {
		if (os)
		  {
		    (*os) << "   [AutoCI] NormalBootstrap failed: "
			  << e.what() << "\n";
		  }
	      }
	  }

	// 2) Basic bootstrap
	if (m_algorithmsConfiguration.enableBasic())
	  {
	    try
	      {
		auto [engine, crn] =
		  m_factory.template makeBasic<Decimal, Sampler, Resampler, Executor, SampleType>(
										      B_single,
										      cl,
										      resampler,
										      m_strategy,
										      stageTag,
										      static_cast<uint64_t>(blockSize),
										      fold,
										      m_interval_type);

		// USE CONFIGURED SAMPLER INSTANCE
		auto res = engine.run(returns, m_sampler_instance, crn);
		candidates.push_back(
				     Selector::template summarizePercentileLike(
										MethodId::Basic, engine, res));
	      }
	    catch (const std::exception& e)
	      {
		if (os)
		  {
		    (*os) << "   [AutoCI] BasicBootstrap failed: "
			  << e.what() << "\n";
		  }
	      }
	  }

	// 3) Percentile bootstrap
	if (m_algorithmsConfiguration.enablePercentile())
	  {
	    try
	      {
		auto [engine, crn] =
		  m_factory.template makePercentile<Decimal, Sampler, Resampler, Executor, SampleType>(
											   B_single,
											   cl,
											   resampler,
											   m_strategy,
											   stageTag,
											   static_cast<uint64_t>(blockSize),
											   fold,
											   m_interval_type);

		// USE CONFIGURED SAMPLER INSTANCE
		auto res = engine.run(returns, m_sampler_instance, crn);
		candidates.push_back(
				     Selector::template summarizePercentileLike(
										MethodId::Percentile, engine, res));
	      }
	    catch (const std::exception& e)
	      {
		if (os)
		  {
		    (*os) << "   [AutoCI] PercentileBootstrap failed: "
			  << e.what() << "\n";
		  }
	      }
	  }

	// 4) M-out-of-N Percentile bootstrap
	if (m_algorithmsConfiguration.enableMOutOfN())
	  {
	    try
	      {
		// Extract rescaling flag from configuration
		const bool rescaleMOutOfN = m_bootstrapConfiguration.getRescaleMOutOfN();

		// if constexpr is required here rather than a runtime if.
		//
		// Both branches are instantiated by the compiler regardless of which
		// is executed at runtime.  The bar-level branch constructs an engine
		// with SampleType=Decimal and calls engine.run(returns,...) where
		// returns is vector<SampleType>.  When SampleType=Trade<Decimal> those
		// types don't match — a compile error — even though the branch is
		// never reached at runtime.  if constexpr discards the non-matching
		// branch entirely so only the active branch is instantiated.
		//
		// The isTradeLevelBootstrappingEnabled() flag is structurally redundant
		// with the compile-time SampleType check; it is preserved in
		// BootstrapConfiguration for documentation and external inspection but
		// cannot drive the MOutOfN dispatch here.
		if constexpr (std::is_same_v<SampleType, Decimal>)
		  {
		    // Bar-level path: use adaptive ratio policy (TailVolatilityAdaptivePolicy).
		    // makeAdaptiveMOutOfN intentionally does not expose SampleType — adaptive
		    // mode is bar-level only (enforced by static_assert in the class).
		    auto [engine, crn] =
		      m_factory.template makeAdaptiveMOutOfN<Decimal, Sampler, Resampler, Executor>(
								    B_single,
								    cl,
								    resampler,
								    m_strategy,
								    stageTag,
								    static_cast<uint64_t>(blockSize),
								    fold,
								    rescaleMOutOfN,
								    m_interval_type);

		    auto res = engine.run(returns, m_sampler_instance, crn);
		    candidates.push_back(
		      Selector::template summarizePercentileLike(MethodId::MOutOfN, engine, res));
		  }
		else
		  {
		    // Trade-level path: adaptive ratio computation requires ~8+ scalar
		    // observations for reliable Hill/skewness estimates and is blocked
		    // by a static_assert inside MOutOfNPercentileBootstrap. Use a fixed
		    // subsample ratio instead. 0.75 is a conservative default for the
		    // small trade populations typical in backtesting.
		    constexpr double TRADE_LEVEL_MOUTOFN_RATIO = 0.75;

		    const std::size_t n_trades = returns.size();

		    // BUG-2 FIX: Require at least 8 trades so the subsample is a
		    // genuine subsample.  At n=6 or n=7 the n^(2/3)/n floor reaches
		    // or exceeds 0.75, which combined with the min_m6_floor drives
		    // trade_ratio to 1.0 — the engine then clamps m_sub to n-1,
		    // yielding near-full resampling with no variance-reduction benefit.
		    // Skipping at n < 8 is consistent with the guard's intent: prevent
		    // degenerate subsamples that provide no subsampling benefit.
		    if (n_trades < 8)
		      {
			if (os)
			  (*os) << "   [AutoCI] MOutOfNPercentileBootstrap skipped: "
				   "fewer than 8 trades (n=" << n_trades << ")\n";
		      }
		    else
		      {
			const double n23_floor    = std::pow(static_cast<double>(n_trades), 2.0/3.0)
			                  / static_cast<double>(n_trades);
			const double min_m6_floor = 6.0 / static_cast<double>(n_trades);

			// Defensive floors ensure at least 6-obs subsamples at tiny N.
			// Cap at 1.0: a ratio > 1.0 means oversampling, not subsampling.
			// TRADE_LEVEL_MOUTOFN_RATIO (0.75) dominates at all N >= 9.
			const double trade_ratio = std::min(1.0,
			  std::max({TRADE_LEVEL_MOUTOFN_RATIO, n23_floor, min_m6_floor}));

			auto [engine, crn] =
			  m_factory.template makeMOutOfN<Decimal, Sampler, Resampler, Executor, SampleType>(
							      B_single,
							      cl,
							      trade_ratio,
							      resampler,
							      m_strategy,
							      stageTag,
							      static_cast<uint64_t>(blockSize),
							      fold,
							      rescaleMOutOfN,
							      m_interval_type);

			auto res = engine.run(returns, m_sampler_instance, crn, 0, os);
			candidates.push_back(
			  Selector::template summarizePercentileLike(MethodId::MOutOfN, engine, res));
		      }
		  }
	      }
	    catch (const std::exception& e)
	      {
		if (os)
		  {
		    (*os) << "   [AutoCI] MOutOfNPercentileBootstrap failed: "
			  << e.what() << "\n";
		  }
	      }
	  }

	// 5) Percentile-T bootstrap (double bootstrap)
	if (m_algorithmsConfiguration.enablePercentileT())
	  {
	    // Pre-run sample size gate.
	    //
	    // CandidateGateKeeper::isCommonCandidateValid() in AutoBootstrapScoring.h
	    // unconditionally rejects any PercentileT candidate where
	    //   n < AutoBootstrapConfiguration::kPercentileTMinSampleSize (= 20).
	    //
	    // Without this guard the full double bootstrap still runs:
	    //   B_outer (25 000) x MIN_INNER (100) = 2.5 M statistic evaluations
	    // per strategy, only for the result to be silently discarded.  When the
	    // median sample size is 13 and the minimum is 9, this is wasted work for
	    // the majority of strategies in the tournament.
	    //
	    // returns.size() is in SampleType units (bars at bar-level, trades at
	    // trade-level), matching exactly what PercentileTBootstrap::run_impl
	    // stores in Result::n and what the gate subsequently reads via getN().
	    if (returns.size() < AutoBootstrapConfiguration::kPercentileTMinSampleSize)
	      {
		if (os)
		  (*os) << "   [AutoCI] PercentileTBootstrap skipped: n="
			<< returns.size()
			<< " < kPercentileTMinSampleSize="
			<< AutoBootstrapConfiguration::kPercentileTMinSampleSize
			<< "\n";
	      }
	    else
	      {
	    try
	      {
		auto [engine, crn] =
		  m_factory.template makePercentileT<Decimal, Sampler, Resampler, Executor, SampleType>(
											    B_outer_percentileT,
											    B_inner_percentileT,
											    cl,
											    resampler,
											    m_strategy,
											    stageTag,
											    static_cast<uint64_t>(blockSize),
											    fold,
											    m_interval_type);

		// USE CONFIGURED SAMPLER INSTANCE
		auto res = engine.run(returns, m_sampler_instance, crn);
		candidates.push_back(
				     Selector::template summarizePercentileT(engine, res, os));
	      }
	    catch (const std::exception& e)
	      {
		if (os)
		  {
		    (*os) << "   [AutoCI] PercentileTBootstrap failed: "
			  << e.what() << "\n";
		  }
	      }
	      } // end else (n >= kPercentileTMinSampleSize)
	  }

	// 6) BCa (Bias-Corrected and Accelerated)
	if (m_algorithmsConfiguration.enableBCa())
	  {
	    try
	      {
		// Wrap the configured statistic instance in a typed std::function.
		// When SampleType = Decimal this is identical to the original bar-level
		// signature. When SampleType = Trade<Decimal> the lambda accepts a
		// vector of trades, and the factory's overload resolution selects the
		// trade-level makeBCa overload from the vector<Trade<Decimal>> first arg.
		Sampler capturedStat = m_sampler_instance;
		std::function<Decimal(const std::vector<SampleType>&)> statFn =
		  [capturedStat](const std::vector<SampleType>& r) { return capturedStat(r); };

		auto bcaEngine =
		  m_factory.template makeBCa<Decimal, Resampler, Executor>(
								    returns,
								    B_single,
								    cl,
								    statFn,
								    resampler,
								    m_strategy,
								    stageTag,
								    static_cast<uint64_t>(blockSize),
								    fold,
								    m_interval_type);

        // BCaBootstrap computes its statistics during construction; no run() needed.
        // Pass through optional logging stream so summarizeBCa may emit debug output
        // to the caller-provided stream when non-null.
        candidates.push_back(Selector::template summarizeBCa(bcaEngine, weights, os));
	      }
	    catch (const std::exception& e)
	      {
		if (os)
		  {
		    (*os) << "   [AutoCI] BCaBootstrap failed: "
			  << e.what() << "\n";
		  }
	      }
	  }

	if (candidates.empty())
	  {
	    throw std::runtime_error(
				     "StrategyAutoBootstrap::run: no bootstrap candidate succeeded.");
	  }

	const StatisticSupport support = m_sampler_instance.support();
	Result result = Selector::select(candidates, weights, support);

	if (os)
	  {
	    const auto& diagnostics = result.getDiagnostics();
	    const auto& chosen      = result.getChosenCandidate();

	    if (result.getChosenMethod() == MethodId::MOutOfN)
	      (*os) << "\n   [AutoCI] M-out-of-N selected\n";

	    // CONCERN-A FIX: emit BCa rejection explanation whenever BCa competed
	    // but did not win, regardless of which method was chosen. Previously
	    // this block only fired when MOutOfN won, giving no diagnostic output
	    // for the common case where Percentile-T outscored BCa.
	    if (diagnostics.hasBCaCandidate() && !diagnostics.isBCaChosen())
	      {
		(*os) << "   [AutoCI] BCa not selected — ";

		// Explain the primary reason, in severity order.
		if (diagnostics.wasBCaRejectedForNonFiniteParameters())
		  (*os) << "disqualified: non-finite z0 or acceleration\n";
		else if (diagnostics.wasBCaRejectedForDomain())
		  (*os) << "disqualified: interval violates domain constraints\n";
		else if (diagnostics.wasBCaRejectedForInstability())
		  {
		    // wasBCaRejectedForInstability() fires for two distinct
		    // categories under the Tier-3 rule:
		    //   SOFT penalties (BCa competed, was down-weighted, lost on
		    //   score): !getAccelIsReliable(), !getBcaTransformMonotone()
		    //   HARD gates (BCa never entered the tournament): z0/accel
		    //   above hard limits, skew above hard limit, n below
		    //   kBcaMinSampleSize, effective-B below gate.
		    // We inspect the BCa candidate to tell the user which category
		    // applied. "penalized" for soft; "disqualified" for hard.
		    bool accel_bad   = false;
		    bool nonmono_bad = false;
		    for (const auto& cand : result.getCandidates())
		      {
			if (cand.getMethod() != MethodId::BCa) continue;
			accel_bad   = !cand.getAccelIsReliable();
			nonmono_bad = !cand.getBcaTransformMonotone();
			break;
		      }

		    if (accel_bad && nonmono_bad)
		      (*os) << "penalized: unreliable acceleration"
			       " + non-monotone transform;"
			       " outscored by winner\n";
		    else if (accel_bad)
		      (*os) << "penalized: acceleration driven by a single"
			       " observation (Tier-2 sensitivity test failed);"
			       " outscored by winner\n";
		    else if (nonmono_bad)
		      (*os) << "penalized: BCa percentile-transform mapping"
			       " reversed direction; outscored by winner\n";
		    else
		      (*os) << "disqualified: parameter instability"
			       " (hard z0/accel/skew limit, sample-size floor,"
			       " or effective-B gate)\n";
		  }
		else if (diagnostics.wasBCaRejectedForLength())
		  (*os) << "disqualified: interval too wide\n";
		else
		  (*os) << "outscored by winner\n";

		// When wasBCaRejectedForInstability() fired, print the parameters
		// that contributed. Under the Tier-3 rule, "instability" groups
		// both hard-gate disqualifications (z0/accel hard limits, skew
		// hard limit, sample-size floor) AND soft-penalty signals (accel
		// unreliable, non-monotone transform) that down-weighted BCa but
		// let it compete. The per-field annotations below label each as
		// hard or soft so the caller can see which was the actual cause.
		//
		//   getAccelIsReliable()      — false when the jackknife
		//     acceleration is driven by a single observation under the
		//     Tier-1/2 rule (see BiasCorrectedBootstrap.h). Specifically,
		//     |â| is material (≥ kAccelMaterialThreshold) AND the
		//     leave-one-out sensitivity test fails (removing the
		//     top-|d³| observation changes â by more than
		//     kSensitivityThreshold relative). A soft penalty of
		//     kBcaAccelUnreliablePenalty is applied in the tournament;
		//     this detail line makes the event visible in the log.
		//
		//   getBcaTransformMonotone() — false if the BCa percentile-transform
		//     mapping produced α₁ > α₂ (inverted order).  The bounds are still
		//     valid after the silent swap in calculateBCaBounds(), but the BCa
		//     correction reversed direction.  A soft penalty of
		//     kBcaTransformNonMonotonePenalty is applied in the tournament;
		//     this detail line makes the event visible in the log.
		//
		// BUG-1 FIX: threshold annotations now reference the actual gate
		// constants from AutoBootstrapConfiguration rather than magic
		// literals. The previous annotations used 0.4 (not a defined gate)
		// for z0 and unlabelled 0.1 for accel; both are now labelled with
		// their correct gate type (hard vs soft).
		if (diagnostics.wasBCaRejectedForInstability())
		  {
		    for (const auto& cand : result.getCandidates())
		      {
			if (cand.getMethod() != MethodId::BCa) continue;

			(*os) << "   [AutoCI]   BCa instability detail:\n";

			(*os) << "   [AutoCI]     z0 (bias):        " << cand.getZ0();
			if (!std::isfinite(cand.getZ0()))
			  (*os) << "  <- non-finite (hard gate)";
			else if (std::abs(cand.getZ0()) > AutoBootstrapConfiguration::kBcaZ0HardLimit)
			  (*os) << "  <- |z0| > " << AutoBootstrapConfiguration::kBcaZ0HardLimit
				<< " (hard rejection gate)";
			else if (std::abs(cand.getZ0()) > AutoBootstrapConfiguration::kBcaZ0SoftThreshold)
			  (*os) << "  <- |z0| > " << AutoBootstrapConfiguration::kBcaZ0SoftThreshold
				<< " (soft penalty threshold)";
			(*os) << "\n";

			(*os) << "   [AutoCI]     a  (accel):       " << cand.getAccel();
			if (!std::isfinite(cand.getAccel()))
			  (*os) << "  <- non-finite (hard gate)";
			else if (std::abs(cand.getAccel()) > AutoBootstrapConfiguration::kBcaAHardLimit)
			  (*os) << "  <- |a| > " << AutoBootstrapConfiguration::kBcaAHardLimit
				<< " (hard rejection gate)";
			else if (std::abs(cand.getAccel()) > AutoBootstrapConfiguration::kBcaASoftThreshold)
			  (*os) << "  <- |a| > " << AutoBootstrapConfiguration::kBcaASoftThreshold
				<< " (soft penalty threshold)";
			(*os) << "\n";

			(*os) << "   [AutoCI]     accel reliable:   "
			      << (cand.getAccelIsReliable()
				    ? "yes"
				    : "NO — acceleration driven by one observation"
				      " (Tier-2 sensitivity test failed)")
			      << "\n";
			// When accel is unreliable, note that a soft penalty was
			// already applied in the tournament so the caller knows the
			// stability_penalty below already incorporates
			// kBcaAccelUnreliablePenalty.
			if (!cand.getAccelIsReliable())
			  (*os) << "   [AutoCI]       (stability_penalty includes "
				<< AutoBootstrapConfiguration::kBcaAccelUnreliablePenalty
				<< " accel-unreliable soft penalty)\n";

			(*os) << "   [AutoCI]     transform monotone: "
			      << (cand.getBcaTransformMonotone()
				    ? "yes"
				    : "NO — BCa percentile mapping reversed direction (bounds swapped)")
			      << "\n";
			// When non-monotone, note that a soft penalty was already applied in
			// the tournament so the caller knows the stability_penalty below
			// already incorporates kBcaTransformNonMonotonePenalty = 0.5.
			if (!cand.getBcaTransformMonotone())
			  (*os) << "   [AutoCI]       (stability_penalty includes "
				<< AutoBootstrapConfiguration::kBcaTransformNonMonotonePenalty
				<< " non-monotone soft penalty)\n";

			(*os) << "   [AutoCI]     skew(boot):       " << cand.getSkewBoot();
			if (std::isfinite(cand.getSkewBoot()) &&
			    std::abs(cand.getSkewBoot()) > AutoBootstrapConfiguration::kBcaSkewHardLimit)
			  (*os) << "  <- |skew| > " << AutoBootstrapConfiguration::kBcaSkewHardLimit
				<< " (hard rejection gate)";
			(*os) << "\n";

			(*os) << "   [AutoCI]     stability penalty: "
			      << cand.getStabilityPenalty() << "\n";
		      }
		  }
	      }

	    (*os) << "   [AutoCI] Selected method="
		  << Result::methodIdToString(diagnostics.getChosenMethod())
		  << "  mean="  << chosen.getMean()
		  << "  LB="    << chosen.getLower()
		  << "  UB="    << chosen.getUpper()
		  << "  n="     << chosen.getN()
		  << "  B_eff=" << chosen.getEffectiveB()
		  << "  z0="    << chosen.getZ0()
		  << "  a="     << chosen.getAccel()
		  << "\n";

	    (*os) << "   [AutoCI] Diagnostics: "
		  << "score="                    << diagnostics.getChosenScore()
		  << "  stability_penalty="      << diagnostics.getChosenStabilityPenalty()
		  << "  length_penalty="         << diagnostics.getChosenLengthPenalty()
		  << "  hasBCa="                 << (diagnostics.hasBCaCandidate() ? "true" : "false")
		  << "  bcaChosen="              << (diagnostics.isBCaChosen() ? "true" : "false")
		  << "  bcaRejectedInstability=" << (diagnostics.wasBCaRejectedForInstability() ? "true" : "false")
		  << "  bcaRejectedLength="      << (diagnostics.wasBCaRejectedForLength() ? "true" : "false")
		  << "  numCandidates="          << diagnostics.getNumCandidates()
		  << "\n";
	  }

	// Cross-strategy aggregation (optional). Recording is thread-safe and
	// lightweight; the per-strategy log block above is unaffected.
	if (aggregator)
	  {
	    const auto& chosen = result.getChosenCandidate();
	    aggregator->record(result,
	                       static_cast<std::size_t>(chosen.getN()),
	                       statistic_name);
	  }

	return result;
      }
      
    private:
      // -----------------------------------------------------------------------
      // Resampler construction helper.
      //
      // Bar-level resamplers (StationaryBlockResampler, StationaryMask*) are
      // constructed with a block size. IIDResampler<Trade<Decimal>> is a plain
      // struct with no constructor arguments. if constexpr selects the right
      // form at compile time so both paths produce well-formed code.
      // -----------------------------------------------------------------------
      Resampler makeResampler(std::size_t blockSize) const
      {
        if constexpr (std::is_same_v<SampleType, Decimal>)
          return Resampler(blockSize);
        else
          return Resampler{};
      }

      Factory& m_factory;
      const mkc_timeseries::BacktesterStrategy<Decimal>& m_strategy;
      BootstrapConfiguration             m_bootstrapConfiguration;
      BootstrapAlgorithmsConfiguration   m_algorithmsConfiguration;
      Sampler                            m_sampler_instance;
      IntervalType                       m_interval_type;
      /// Persistent thread pool shared across all six engines in every run() call.
      /// Always non-null: either caller-supplied or self-created at construction.
      std::shared_ptr<Executor>          m_exec;
    };
  } // namespace analysis
} // namespace palvalidator
