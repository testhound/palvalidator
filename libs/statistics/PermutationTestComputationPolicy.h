// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
//
#ifndef __PERMUTATION_TEST_COMPUTATION_POLICY_H
#define __PERMUTATION_TEST_COMPUTATION_POLICY_H 1

#include <exception>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <iostream>
#include "number.h"
#include "DecimalConstants.h"
#include "BackTester.h"
#include "SyntheticTimeSeries.h"
#include "MonteCarloTestPolicy.h"
#include "SyntheticSecurityHelpers.h"
#include "PermutationTestResultPolicy.h"
#include "PermutationTestSubject.h"
#include "StrategyIdentificationHelper.h"
#include "RandomMersenne.h"
#include "SyntheticCache.h"
#include "ParallelExecutors.h"
#include "ParallelFor.h"

namespace mkc_timeseries
{
  /**
   * @class StandardPValueComputationPolicy
   * @brief Policy class for computing standard bias-corrected permutation test p-values.
   *
   * This policy implements the standard +1 correction method recommended in permutation
   * testing literature (e.g., Good 2005; North et al. 2002) to avoid zero p-values
   * and yield unbiased small-sample estimates.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template<typename Decimal>
  class StandardPValueComputationPolicy
  {
  public:
    /**
     * @brief Computes a bias-corrected Monte Carlo permutation test p-value.
     *
     * Applies the "+1" correction often recommended in the permutation-testing literature
     * (e.g. Good 2005; North et al. 2002) to avoid zero p-values and to yield an unbiased
     * small-sample estimate.  Given:
     *   - k = number of permutations whose test statistic ≥ the observed statistic
     *   - N = total number of permutations run
     *
     * this returns
     * \f[
     *    p \;=\; \frac{k + 1}{\,N + 1\,}
     * \f]
     *
     * which enforces a minimum p-value of \(1/(N+1)\) when \(k=0\).
     *
     * @param k
     *   Count of "extreme" permutations (i.e. ones at least as good as baseline).
     * @param N
     *   Total number of permutations executed.
     * @return
     *   A bias-corrected p-value in the interval \([1/(N+1),\,1]\).
     */
    static Decimal computePermutationPValue(std::uint32_t k,
                                            std::uint32_t N)
    {
      return Decimal(k + 1) / Decimal(N + 1);
    }
  };

  /**
   * @class WilsonPValueComputationPolicy
   * @brief Policy class for computing conservative permutation test p-values using Wilson score method.
   *
   * This policy uses the Wilson score upper confidence bound to provide a conservative
   * p-value estimate that accounts for Monte Carlo uncertainty in permutation tests.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template<typename Decimal>
  class WilsonPValueComputationPolicy
  {
  public:
    /**
     * @brief Conservative permutation p-value using the Wilson one-sided upper bound.
     *
     * Given a permutation test with @f$N@f$ random permutations and @f$k@f$ "extreme"
     * permutations (statistics at least as extreme as observed), we first compute the
     * **+1 corrected** estimator
     *
     * \f[
     *   \hat p \;=\; \frac{k+1}{N+1},
     * \f]
     *
     * then return the **Wilson one-sided upper bound** for @f$p@f$ at the desired
     * confidence (controlled by @p z). This inflates the reported p-value just enough
     * to account for Monte-Carlo uncertainty, so downstream rules like "promote if
     * @f$p \le \alpha@f$" are robust to finite @f$N@f$.
     *
     * @param k  Count of permutations with test statistic ≥ observed (extreme count).
     * @param N  Total number of (valid) permutations performed.
     * @return   Conservative p-value = Wilson upper bound for \f$\hat p = (k+1)/(N+1)\f$.
     *
     * @par Rationale
     * - The +1 correction ensures permutation p-values are never reported as zero and
     *   yields valid exact p-values under random permutations.
     * - Using the Wilson **upper** bound turns your p-value into a one-sided confidence
     *   bound for the *true* tail probability, directly addressing Monte-Carlo error at finite @f$N@f$.
     */
    static Decimal computePermutationPValue(std::uint32_t k,
                                            std::uint32_t N)
    {
      const double phat = static_cast<double>(k + 1) / static_cast<double>(N + 1);
      constexpr double kZ_OneSided95 = 1.6448536269514722;

      return Decimal(wilsonUpperBound(phat, N, kZ_OneSided95));
    }

  private:
    /**
     * @brief One-sided Wilson score *upper* confidence bound for a binomial proportion.
     *
     * Computes the Wilson score upper bound for the true success probability @f$p@f$
     * given an observed proportion @f$\hat p@f$ from @f$N@f$ Bernoulli trials:
     *
     * \f[
     * \mathrm{UB}_\text{Wilson}(\hat p; N, z) \;=\;
     * \frac{\hat p + \frac{z^2}{2N} + z\,\sqrt{\frac{\hat p(1-\hat p)}{N} + \frac{z^2}{4N^2}}}
     *      {1 + \frac{z^2}{N}}
     * \f]
     *
     * where @f$z@f$ is the normal quantile. For a **one-sided 95%** upper bound,
     * use @f$z \approx 1.64485@f$; for a **two-sided 95%** interval (each tail @f$\alpha/2=0.025@f$),
     * @f$z \approx 1.96@f$. The result is clipped to @f$[0,1]@f$ for numerical safety.
     *
     * @param phat  Observed proportion in @f$[0,1]@f$ (e.g., \f$\hat p = (k+1)/(N+1)\f$ for
     *              permutation tests with a +1 correction).
     * @param N     Number of Bernoulli trials (sample size).
     * @param z     Normal critical value (default 1.96; set 1.64485 for one-sided 95%).
     * @return      The Wilson one-sided *upper* confidence bound in @f$[0,1]@f$.
     *
     * @note Compared to the simple Wald bound \f$\hat p \pm z\sqrt{\hat p (1-\hat p)/N}\f$,
     *       the Wilson score bound has substantially better coverage—especially for small \f$N\f$
     *       or proportions near 0 or 1.
     *
     * @par References
     * - E. B. Wilson (1927), "Probable Inference, the Law of Succession, and Statistical Inference,"
     *   *JASA* 22(158), 209–212. (Original derivation of the score interval.)
     * - L. D. Brown, T. T. Cai, A. DasGupta (2001), "Interval Estimation for a Binomial Proportion,"
     *   *Statistical Science* 16(2), 101–133. Excellent review and comparison of binomial intervals.
     * - NIST/SEMATECH e-Handbook of Statistical Methods, "Confidence Intervals for a Proportion"
     *   (one-sided adaptation via replacing \f$z_{\alpha/2}\f$ with \f$z_{\alpha}\f$).
     * - Overview/derivation summary: "Binomial proportion confidence interval — Wilson score interval."
     */
    static double wilsonUpperBound(double phat, std::uint32_t N, double z = 1.645)
    {
      // --- Algorithmic notes (step-by-step) ---
      // 1) Precompute z^2 and denominator term 1 + z^2 / N.
      // 2) Compute the "center" term: phat + z^2/(2N).
      // 3) Compute the "radius" term:
      //      z * sqrt( phat*(1 - phat)/N + z^2/(4N^2) )
      //    which is the score-test standard error evaluated at the bound.
      // 4) Upper bound = (center + radius) / denom.
      // 5) Clip to [0, 1] to stabilize edge cases (very small/large phat, small N).

      const double z2 = z*z;
      const double denom  = 1.0 + z2 / N;
      const double center = phat + z2 / (2.0 * N);
      const double rad    = z * std::sqrt((phat * (1.0 - phat) + z2 / (4.0 * N)) / N);
      double ub = (center + rad) / denom;

      if (ub < 0.0)
        ub = 0.0;

      if (ub > 1.0)
        ub = 1.0;

      return ub;
    }
  };

  /**
   * @class DefaultPermuteMarketChangesPolicy
   * @brief Performs a hypothesis test via Monte-Carlo permutation testing of a trading strategy.
   *
   * This class implements a Monte-Carlo permutation testing methodology to evaluate
   * the statistical significance of a trading strategy's performance. It operates on a
   * trading strategy represented by the `BacktesterStrategy` class and utilizes a
   * backtesting engine provided by the `BackTester` class.
   *
   * The core principle involves permuting market changes to generate multiple synthetic
   * market scenarios. The strategy is then backtested on these scenarios to create a
   * distribution of performance metrics, which is used to assess the likelihood that
   * the original strategy's performance was due to chance.
   *
   * @tparam Decimal The numerical type used for calculations (e.g., double, mkc_timeseries::number).
   *
   * @tparam BackTestResultPolicy A policy class that defines how to extract the relevant
   * test statistic from a backtest result. It is expected to be
   * a class like `AllHighResLogPFPolicy`, which extracts a
   * high-resolution profit factor. This policy must define:
   * - `static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>>)`
   * - `static uint32_t getMinStrategyTrades()`
   *
   * @tparam _PermutationTestResultPolicy A policy class determining the return type of the
   * `runPermutationTest` method. It must define a nested
   * type `ReturnType` and a static method
   * `createReturnValue(Decimal pValue, Decimal summaryTestStat)`.
   * Defaults to `PValueReturnPolicy<Decimal>`.
   *
   * @tparam _PermutationTestStatisticsCollectionPolicy A policy class used to determine if and how
   * backtester statistics are collected across multiple permutations. These statistics can
   * be used by multiple-testing correction algorithms.
   *
   * It must implement:
   * - `void updateTestStatistic(Decimal)`
   * - `Decimal getTestStat()`
   * Defaults to `PermutationTestingNullTestStatisticPolicy<Decimal>`.
   * @tparam Executor A policy class that defines the execution model for permutations,
   * specifically whether concurrency is used. Defaults to `concurrency::StdAsyncExecutor`.
   * @tparam PValueComputationPolicy A policy class that defines how to compute the final p-value
   * from the permutation test results. Defaults to `StandardPValueComputationPolicy<Decimal>`.
   */
  template <class Decimal,
	    class BackTestResultPolicy,
	    typename _PermutationTestResultPolicy = PValueReturnPolicy<Decimal>,
	    typename _PermutationTestStatisticsCollectionPolicy = PermutationTestingNullTestStatisticPolicy<Decimal>,
	    typename Executor = concurrency::ThreadPoolExecutor<>,
	    typename PValueComputationPolicy = StandardPValueComputationPolicy<Decimal>,
	    SyntheticNullModel NullModel = SyntheticNullModel::N1_MaxDestruction
	    >
  class DefaultPermuteMarketChangesPolicy : public PermutationTestSubject<Decimal>
  {
    static_assert(has_return_type<_PermutationTestResultPolicy>::value,
    "_PermutationTestResultPolicy must define a nested ::ReturnType");
    static_assert(has_create_return_value_3param<_PermutationTestResultPolicy>::value,
    "_PermutationTestResultPolicy must have static createReturnValue(Decimal, Decimal, Decimal)");
    static_assert(has_update_test_statistic<_PermutationTestStatisticsCollectionPolicy>::value,
		  "_PermutationTestStatisticsCollectionPolicy must implement updateTestStatistic(Decimal)");
    static_assert(has_get_test_stat<_PermutationTestStatisticsCollectionPolicy>::value,
		  "_PermutationTestStatisticsCollectionPolicy must implement getTestStat()");
  public:
    using CacheType = SyntheticCache<Decimal,
				     LogNLookupPolicy<Decimal>,
				     NoRounding,
				     NullModel>;
    
    using ReturnType = typename _PermutationTestResultPolicy::ReturnType;

    DefaultPermuteMarketChangesPolicy()
    {}

    ~DefaultPermuteMarketChangesPolicy()
    {}

    /**
     * @brief Executes the Monte-Carlo permutation test for a given trading strategy.
     *
     * This method performs the permutation test by:
     *
     * 1. For each permutation:
     * a. Cloning the original strategy and backtester.
     * b. Creating a synthetic portfolio with permuted market data derived from the original security.
     * This step is repeated if the number of trades generated by the cloned strategy on the
     * synthetic data is less than a minimum threshold (defined by `BackTestResultPolicy::getMinStrategyTrades()`).
     * c. Running the backtest for the cloned strategy on the synthetic market data.
     * d. Computing a test statistic for this permutation using `BackTestResultPolicy::getPermutationTestStatistic()`.
     * e. Comparing the permutation's test statistic with the `baseLineTestStat`.
     * f. Updating a collection of test statistics via `_PermutationTestStatisticsCollectionPolicy` (thread-safe)
     *.
     * 2. These steps are executed in parallel for all permutations, as governed by the `Executor` policy.
     *
     * 3. Calculating the p-value as the proportion of permutations whose test statistic is greater than or
     * equal to the `baseLineTestStat`.
     *
     * 4. Obtaining a summary test statistic from the `_PermutationTestStatisticsCollectionPolicy`.
     *
     * 5. Returning the results as defined by the `_PermutationTestResultPolicy`.
     *
     * @param theBackTester A shared pointer to a `BackTester<Decimal>` object, which
     * encapsulates the backtesting engine and the trading strategy.
     * @param numPermutations The number of permutations (synthetic backtests) to run.
     * @param baseLineTestStat The test statistic obtained from running the strategy on the
     * original, unpermuted market data. This serves as the benchmark.
     * @return ReturnType The result of the permutation test, with its type determined by
     * the `_PermutationTestResultPolicy`. This typically includes the p-value
     * and may include a summary test statistic from the permutations.
     */
    ReturnType
    runPermutationTest(std::shared_ptr<BackTester<Decimal>> theBackTester,
		       uint32_t numPermutations,
		       const Decimal& baseLineTestStat)
    {
      if (numPermutations == 0)
	throw std::invalid_argument(
				    "DefaultPermuteMarketChangesPolicy::runPermutationTest: numPermutations must be > 0");

      // Grab the baseline strategy and its security/portfolio ONCE (before parallel work)
      auto aStrategy = *(theBackTester->beginStrategies());

      if (aStrategy->beginPortfolio() == aStrategy->endPortfolio()) {
	throw std::runtime_error(
				 "DefaultPermuteMarketChangesPolicy::runPermutationTest: Strategy portfolio is empty - "
				 "use getRandomPalStrategy(security) to create strategy with populated portfolio");
      }

      auto theSecurity       = aStrategy->beginPortfolio()->second; // shared_ptr<Security<Decimal>>
      auto originalPortfolio = aStrategy->getPortfolio();

      if (!theSecurity)
	throw std::runtime_error("DefaultPermuteMarketChangesPolicy::runPermutationTest: Security is null");
      if (!originalPortfolio)
	throw std::runtime_error("DefaultPermuteMarketChangesPolicy::runPermutationTest: Portfolio is null");

      // Atomics for valid/extreme counts (unchanged)
      std::atomic<uint32_t> validPerms{0}, extremeCount{0};

      // Optional summary collector (unchanged contract)
      _PermutationTestStatisticsCollectionPolicy testStatCollector;
      std::mutex                                 testStatMutex;

      // ---- Parallel work: TLS cache/portfolio/RNG + one-time BackTester clone per worker ----

      auto work = [=, &validPerms, &extremeCount, &testStatCollector, &testStatMutex](uint32_t /*permIndex*/) {
	// --- thread-local state (initialized once per worker thread) ---
	static thread_local RandomMersenne                        tls_rng;
	static thread_local std::unique_ptr<CacheType>               tls_cache;
	static thread_local std::shared_ptr<Portfolio<Decimal>>   tls_portfolio;
	static thread_local std::shared_ptr<BackTester<Decimal>>  tls_bt;

	if (!tls_cache) {
	  // Build cache from the *baseline* security; cache will swap series per permutation
	  tls_cache = std::make_unique<CacheType>(theSecurity);
	}
	if (!tls_portfolio) {
	  // Build one portfolio per worker (copy/clone baseline ONCE)
	  tls_portfolio = std::make_shared<Portfolio<Decimal>>(*originalPortfolio);
	}
	if (!tls_bt) {
	  // Clone the configured BackTester ONCE per worker (date ranges, engine setup, etc.)
	  tls_bt = theBackTester->clone();
	}

	// 1) Build synthetic series into the reusable per-thread Security
	auto& synSec = tls_cache->shuffleAndRebuild(tls_rng);  // swaps new series into the same Security instance

	// 2) Swap that Security into the per-thread portfolio (C++17 insert_or_assign inside)
	tls_portfolio->replaceSecurity(synSec);

	// 3) Create a fresh strategy for this permutation (fresh broker state),
	//    then reuse the already-cloned per-thread BackTester by swapping the strategy in.
	auto clonedStrat = aStrategy->clone_shallow(tls_portfolio);
	tls_bt->setSingleStrategy(clonedStrat);
	tls_bt->backtest();

	// 4) Compute permutation statistic (unchanged contract) from the *reused* tls_bt
	const Decimal testStat = BackTestResultPolicy::getPermutationTestStatistic(tls_bt);

	// 5) Notify observers (unchanged)
	this->notifyObservers(*tls_bt, testStat);

	// 6) Atomics: valid/extreme counts (unchanged)
	validPerms.fetch_add(1, std::memory_order_relaxed);
	if (testStat >= baseLineTestStat) {
	  extremeCount.fetch_add(1, std::memory_order_relaxed);
	}

	// 7) Update optional summary collector (unchanged)
	{
	  std::lock_guard<std::mutex> guard(testStatMutex);
	  testStatCollector.updateTestStatistic(testStat);
	}
      };

      // Execute in parallel using the provided Executor policy (chunked)
      Executor executor{};
      concurrency::parallel_for_chunked(numPermutations, executor, work);

      // ---- Final aggregation (unchanged) ----------------------------------------
      const uint32_t valid = validPerms.load(std::memory_order_relaxed);
      if (valid == 0) {
	// no informative draws → cannot reject null
	return _PermutationTestResultPolicy::createReturnValue(
							       Decimal(1), testStatCollector.getTestStat(), baseLineTestStat);
      }

      const uint32_t extreme = extremeCount.load(std::memory_order_relaxed);
      const Decimal  pValue  = PValueComputationPolicy::computePermutationPValue(extreme, valid);

      const Decimal summaryTestStat = testStatCollector.getTestStat();

      return _PermutationTestResultPolicy::createReturnValue(pValue, summaryTestStat, baseLineTestStat);
    }
  };
}
#endif
