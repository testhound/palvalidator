#pragma once
#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <set>
#include "IMastersSelectionBiasAlgorithm.h"
#include "MastersPermutationTestComputationPolicy.h"
#include "PermutationTestSubject.h"

namespace mkc_timeseries
{
  /**
   * @class MastersRomanoWolfImproved
   * @brief Fast stepwise permutation testing algorithm with strong FWE control.
   *
   * This "improved" version performs all m permutations in one bulk pass,
   * computing exceedance counts for each strategy simultaneously (from worst to best),
   * then applies a step-down inclusion loop (from best to worst) over the precomputed counts.
   * This is mathematically equivalent to the naive stepwise algorithm but avoids repeating
   * the expensive shuffle/backtest m times per strategy, reducing the complexity to
   * O(N + m × total_backtests).
   *
   * Based on the algorithm in Timothy Masters book"
   * "Permutation and Randomization Tests for Trading System Development: Algorithms in C++"
   *
   * Which itself is based on "Efficient Computation of Adjusted p-Values for Resampling-Based
   * Stepdown Multiple Testing" (Romano & Wolf, 2016)
   *
   * This class uses
   * class FastMastersPermutationPolicy to compute counts in a single Monte Carlo sweep.
   *
   * Template Parameters:
   *   @tparam Decimal            Numeric type for test statistics (e.g., double).
   *   @tparam BaselineStatPolicy Policy providing:
   *                              - getMinStrategyTrades()
   *                              - getPermutationTestStatistic(bt)
   */

  template<class Decimal, class BaselineStatPolicy>
    class MastersRomanoWolfImproved final
        : public IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>,
          public PermutationTestSubject<Decimal>
    {
        using Base       = IMastersSelectionBiasAlgorithm<Decimal, BaselineStatPolicy>;
        using StrategyPtr= typename Base::StrategyPtr;
        using StrategyVec= typename Base::StrategyVec;

    public:
      /**
         * @brief Run the fast stepwise FWE permutation test.
         *
         * Implements the two-phase improved algorithm:
         *
	 * 
	 * Precondition: `strategyData` **must** be sorted in **descending** order by
	 *   `baselineStat` (highest first) before calling.
	 *
         * Phase 1: Bulk permutation counts (worst-to-best)
         *   - Call FastMastersPermutationPolicy::computeAllPermutationCounts to
         *     generate a map of each strategy to its exceedance count:
         *     count_i = 1 + # of permutations where max_{all active strategies}
         *               (statistic) >= original_statistic_i
         *   - This single Monte Carlo loop shuffles once per permutation,
         *     runs backtests for all strategies, and accumulates counts.
         *
         * Phase 2: Step‑down inclusion (best-to-worst)
         *   - Iterate through strategies in descending order of observed performance.
         *   - Compute p_i = count_i / (m + 1), then adjust: p_adj_i = max(p_i, lastAdj).
         *   - If p_adj_i <= alpha, accept (remove tightening bound), else assign
         *     p_adj_i to all remaining and exit.
         *
         * @param strategyData     Pre-sorted vector of StrategyContext (strategy + observed statistic).
	 *                          **Precondition:** sorted descending by `baselineStat`
         * @param numPermutations  Number of permutations (m > 0).
         * @param tmplBT           Prototype backtester to clone per backtest.
         * @param portfolio        Portfolio containing the target security (first element).
         * @param sigLevel         Desired familywise error rate alpha.
         * @return Map from strategy ptr to its adjusted p-value.
         */
      std::map<unsigned long long, Decimal> run(const StrategyVec&                strategyData,
     unsigned long                     numPermutations,
     const std::shared_ptr<BackTester<Decimal>>& templateBacktester,
     const std::shared_ptr<Portfolio<Decimal>>&  portfolio,
     const Decimal&                   sigLevel) override
      {
 if ( !templateBacktester )
   {
     throw std::runtime_error("MastersRomanoWolfImproved::run - backtester is null");
   }

 using FMPP = FastMastersPermutationPolicy<Decimal, BaselineStatPolicy>;

 // Check the precondition and throw if violated
 if (!std::is_sorted(strategyData.begin(), strategyData.end(),
       [](auto const& a, auto const& b) {
         return a.baselineStat > b.baselineStat;
       }))
   {
     const std::string error_message =
       "MastersRomanoWolfImproved::run requires strategyData to be "
       "pre-sorted in descending order by baselineStat.";
  
     throw std::invalid_argument(error_message);
   }

 // Extract the target security from the portfolio:
 auto secIt = portfolio->beginPortfolio();
 if (secIt == portfolio->endPortfolio()) {
   throw std::runtime_error(
       "MastersRomanoWolfImproved::run - portfolio contains no securities");
 }
 auto secPtr = secIt->second;
     
 // Phase 1: compute exceedance counts for every strategy in one Monte Carlo sweep
 //   counts[strategy] = 1 + # permutations where strategy's observed statistic
 //                      is beaten by the max-of-all in that permutation.
 // Bulk compute exceedance counts for every strategy once.

 // Create instance of FastMastersPermutationPolicy for observer support
 FMPP fastPermutationPolicy;
 
 // Chain attached observers to the policy instance (pass-through Subject design)
 std::shared_lock<std::shared_mutex> observerLock(this->m_observersMutex);
 for (auto* observer : this->m_observers) {
     if (observer) {
         fastPermutationPolicy.attach(observer);
     }
 }
 observerLock.unlock();
 
 auto baseStatExceedanceCounts = fastPermutationPolicy.computeAllPermutationCounts(numPermutations,
         strategyData,
         templateBacktester,
         secPtr,
         portfolio);

 // SANITY CHECK
 sanityCheckCounts(baseStatExceedanceCounts, strategyData);
 sanityCheckCounts(strategyData,
     baseStatExceedanceCounts,
     std::string("Check after computeAllPermutationCounts"));
 /*
 Decimal denom = Decimal(numPermutations + 1);

 for (const auto& ctx : strategyData)
   {
     StrategyPtr strategy = ctx.strategy;

     auto it = baseStatExceedanceCounts.find(strategy);
     if (it == baseStatExceedanceCounts.end())
       throw std::logic_error("Missing exceedance count for strategy in statistics export.");

     unsigned int count = it->second;
     Decimal rate = Decimal(count) / denom;

     // Notify all attached observers
     for (auto* observer : this->m_observers)
       {
  auto* collector = dynamic_cast<PermutationStatisticsCollector<Decimal>*>(observer);
  if (collector)
    {
      collector->recordExceedanceRate(strategy.get(), rate * Decimal(100)); // store as percentage
    }
       }
   }
 */

 // Setup for formatted output
 std::cout << "\n--- Step-Down P-Value Adjustment Log ---\n";
 std::cout << std::left << std::setw(28) << "Strategy Name"
    << std::setw(15) << "Exceed Count"
    << std::setw(15) << "Raw P-Value"
    << std::setw(20) << "Adjusted P-Value" << "\n";
 std::cout << std::string(80, '-') << std::endl;

 std::map<unsigned long long, Decimal> pvals;
 Decimal lastAdjustedPValue = Decimal(0);

 // Phase 2: step-down inclusion loop (best-to-worst)
 for (auto& context : strategyData)
   {
     unsigned int exceededCount = numPermutations + 1;

     auto strategyHash = context.strategy->getPatternHash();
     auto it = baseStatExceedanceCounts.find(strategyHash);
     if (it != baseStatExceedanceCounts.end())
       exceededCount = it->second;

     /**
      * Enforce monotonicity on the adjusted p-values in the step-down permutation test.
      *
      * In a step-down procedure, we rank strategies by their observed statistic (e.g. Profit-Factor)
      * from highest (best) to lowest (worst), then compute a "raw" p-value for each:
      *
      *     // count of permutations whose test statistic ≥ observed, divided by total draws
      *     Decimal p = Decimal(c) / Decimal(numPermutations + 1);
      *
      * To prevent a weaker (lower-ranked) strategy from ever appearing more significant
      * than a stronger (higher-ranked) one, we enforce that the sequence of adjusted
      * p-values never decreases as we move down the list:
      *
      *     // take the larger of this strategy's raw p and the previous (best) adjusted p
      *     Decimal adj = std::max(p, lastAdjustedPValue);
      *
      * This ensures:
      *  1. **Non-decreasing p-values**: Once you hit, say, 0.04 at the top, every following
      *     strategy's adjusted p will be ≥ 0.04.
      *  2. **Logical consistency**: You cannot claim a weaker system is more significant
      *     than a stronger one.
      *  3. **Step-down stopping rule**: As soon as an adjusted p exceeds your α threshold,
      *     you can stop: no weaker strategy further down can sneak in below the threshold.
      *
      * In plain English:
      *
      * "We first decide how likely it is that pure chance could give us each strategy's
      * observed result.  Then, to keep our decisions consistent from best to worst, we
 -	     * never let a later strategy's p-value drop below the one before it."
 +	     * never let a later strategy's p-value drop below the one before it."
      */
     Decimal pValue   = Decimal(exceededCount) / Decimal(numPermutations + 1);
     Decimal adjustedPValue = std::max(pValue, lastAdjustedPValue);
     pvals[strategyHash] = adjustedPValue;

     std::cout << std::left << std::setw(28) << context.strategy->getStrategyName()
              << std::setw(15) << exceededCount
              << std::fixed << std::setprecision(7) << std::setw(15) << pValue
              << std::setw(20) << adjustedPValue << "\n";
     
     // *****************************
     if (adjustedPValue <= sigLevel)
       lastAdjustedPValue = adjustedPValue;      // tighten bound
     else
       {
  //
  // Propagate the failing p-value to all remaining strategies
  for (auto& later_ctx : strategyData)
    {
      if (pvals.find(later_ctx.strategy->getPatternHash()) == pvals.end())
        {
   pvals[later_ctx.strategy->getPatternHash()] = adjustedPValue;
   
   // Also log these subsequent strategies so the report is complete
   std::cout << std::left << std::setw(28) << later_ctx.strategy->getStrategyName()
      << std::setw(15) << "---" // No count for these
      << std::fixed << std::setprecision(7) << std::setw(15) << "---" // No raw p-value
      << std::setw(20) << adjustedPValue << " (Inherited)" << "\n";
        }
    }
  //
  // failure ⇒ all remaining inherit same p‑value
  /*
  for (auto& later : strategyData)
    {
      auto laterHash = later.strategy->getPatternHash();
      if (!pvals.count(laterHash))
        pvals[laterHash] = adjustedPValue;
        } */
  break;
       }
   }

 std::map<unsigned long long, Decimal> baselineStats;
 for (const auto& ctx : strategyData)
   {
     auto strategyID = ctx.strategy->getPatternHash();
     baselineStats[strategyID] = ctx.baselineStat;
   }

 // Final sanity check before returning
 finalSanityAudit(strategyData,
    baselineStats,
    baseStatExceedanceCounts,
    pvals);

 return pvals;
      }
    private:
      // Helper to verify that 'counts' has an entry for every unique strategy hash
    void sanityCheckCounts(const std::map<unsigned long long, unsigned int>& counts,
			   const StrategyVec& strategyData) const
      {
	// Collect unique strategy hashes from strategyData
	std::set<unsigned long long> expectedHashes;
	for (auto const& ctx : strategyData) {
	  auto strategyHash = ctx.strategy->getPatternHash();
	  expectedHashes.insert(strategyHash);
	}

	// Check that counts map has exactly the expected unique hashes
	if (counts.size() != expectedHashes.size())
	  throw std::logic_error("Permutation count map has wrong number of unique entries");
	
	// Check that every expected hash is present in counts
	for (auto const& expectedHash : expectedHashes) {
	  if (counts.find(expectedHash) == counts.end())
            throw std::logic_error("Missing permutation count for a strategy hash");
	}
	
	// Check that counts doesn't contain unexpected hashes
	for (auto const& kv : counts)
	  {
	    if (expectedHashes.find(kv.first) == expectedHashes.end())
	      throw std::logic_error("counts map contains an unexpected strategy hash key");
	  }
      }

      void sanityCheckCounts(const StrategyVec& sorted_strategy_data,
			     const std::map<unsigned long long, unsigned int>& final_counts,
			     const std::string& contextTag)
      {
	for (const auto& ctx : sorted_strategy_data)
	  {
	    const auto& strategy = ctx.strategy;
	    auto strategyID = strategy->getPatternHash();
	    
	    if (final_counts.find(strategyID) == final_counts.end())
	      {
		std::ostringstream msg;
		msg << "[sanityCheckCounts][" << contextTag << "] Missing entry for strategy ID: " << strategyID << "\n";
		msg << "  Baseline stat: " << ctx.baselineStat << "\n";
		msg << "  Trade count: " << ctx.count << "\n";
		msg << "  Strategy pointer: " << static_cast<const void*>(strategy.get()) << "\n";
		msg << "  This may indicate inconsistent hashing or use of cloned strategies.\n";
		throw std::runtime_error(msg.str());
	      }
	  }
      }

      void finalSanityAudit(const StrategyVec& sorted_strategy_data,
			    const std::map<unsigned long long, Decimal>& baseline_stats,
			    const std::map<unsigned long long, unsigned int>& final_counts,
			    const std::map<unsigned long long, Decimal>& adjusted_p_values)
      {
	for (const auto& ctx : sorted_strategy_data)
	  {
	    auto strategyID = ctx.strategy->getPatternHash();

	    if (baseline_stats.find(strategyID) == baseline_stats.end())
	      {
		std::ostringstream msg;
		msg << "[finalSanityAudit] Missing baseline stat for strategy ID: " << strategyID;
		throw std::runtime_error(msg.str());
	      }

	    if (final_counts.find(strategyID) == final_counts.end())
	      {
		std::ostringstream msg;
		msg << "[finalSanityAudit] Missing exceedance count for strategy ID: " << strategyID;
		throw std::runtime_error(msg.str());
	      }

	    if (adjusted_p_values.find(strategyID) == adjusted_p_values.end())
	      {
		std::ostringstream msg;
		msg << "[finalSanityAudit] Missing adjusted p-value for strategy ID: " << strategyID;
		throw std::runtime_error(msg.str());
	      }
	  }

	// Check that adjusted p-values are non-decreasing
	Decimal lastP = Decimal(0);
	for (const auto& ctx : sorted_strategy_data)
	  {
	    auto strategyID = ctx.strategy->getPatternHash();
	    Decimal currentP = adjusted_p_values.at(strategyID);

	    if (currentP < lastP)
	      {
		std::ostringstream msg;
		msg << "[finalSanityAudit] Adjusted p-values must be monotonically non-decreasing.\n";
		msg << "  Violation: strategy ID " << strategyID << ", adjusted p: " << currentP << ", previous: " << lastP;
		throw std::runtime_error(msg.str());
	      }

	    lastP = currentP;
	  }
      }
    };
} // namespace mkc_timeseries
