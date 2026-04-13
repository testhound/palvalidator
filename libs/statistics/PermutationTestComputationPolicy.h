// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
//

/**
 * @file PermutationTestComputationPolicy.h
 * @brief P-value computation policies, early stopping policies, and the main
 *        Monte-Carlo permutation test driver for trading strategies.
 *
 * This header provides the policy classes that together implement a configurable
 * permutation testing framework:
 *   - StandardPValueComputationPolicy / WilsonPValueComputationPolicy for
 *     computing the final p-value from extreme and total counts.
 *   - NoEarlyStoppingPolicy / ThresholdEarlyStoppingPolicy for controlling
 *     whether the permutation loop may terminate before all permutations run.
 *   - DefaultPermuteMarketChangesPolicy, the main driver that orchestrates
 *     synthetic series generation, backtesting, and statistical aggregation.
 */

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
     *   - k = number of permutations whose test statistic >= the observed statistic
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
     * @param k  Count of permutations with test statistic >= observed (extreme count).
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
     * @param phat  Observed proportion in @f$[0,1]@f$.
     * @param N     Number of Bernoulli trials (sample size).
     * @param z     Normal critical value (set 1.64485 for one-sided 95%).
     * @return      The Wilson one-sided *upper* confidence bound in @f$[0,1]@f$.
     */
    static double wilsonUpperBound(double phat, std::uint32_t N, double z = 1.645)
    {
      const double z2     = z * z;
      const double denom  = 1.0 + z2 / N;
      const double center = phat + z2 / (2.0 * N);
      const double rad    = z * std::sqrt((phat * (1.0 - phat) + z2 / (4.0 * N)) / N);
      double ub = (center + rad) / denom;

      if (ub < 0.0) ub = 0.0;
      if (ub > 1.0) ub = 1.0;

      return ub;
    }
  };

  // ============================================================================
  // Early Stopping Policies
  // ============================================================================

  /**
   * @class NoEarlyStoppingPolicy
   * @brief Early stopping policy that never stops early.
   *
   * This is the default policy. It preserves the original behaviour of
   * DefaultPermuteMarketChangesPolicy exactly — every permutation runs to
   * completion regardless of intermediate counts. All existing unit tests
   * that rely on a fixed permutation count are unaffected.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template <typename Decimal>
  class NoEarlyStoppingPolicy
  {
  public:
    /**
     * @brief Always returns false — no early stopping.
     *
     * The compiler will eliminate the dead branch in the permutation loop
     * entirely for this policy.
     */
    template <typename PValueComputationPolicy>
    bool shouldStop(uint32_t  /*validPerms*/,
                    uint32_t  /*extremeCount*/,
                    const Decimal& /*targetAlpha*/) noexcept
    {
      return false;
    }
  };

  /**
   * @class ThresholdEarlyStoppingPolicy
   * @brief Early stopping policy that halts once the outcome is statistically clear.
   *
   * Checks stopping conditions every @p checkInterval completed permutations,
   * after a minimum of @p minBeforeStop valid permutations have accumulated.
   * Two conditions are evaluated using the same PValueComputationPolicy as the
   * final result, ensuring consistency between the stopping decision and the
   * reported p-value:
   *
   * **Clearly failing**: the implied p-value already exceeds
   * @p failingMultiplier * targetAlpha — it cannot fall below targetAlpha by
   * the time all permutations finish.
   *
   * **Clearly passing**: extremeCount is zero after at least @p minPassPerms
   * valid permutations — the p-value is already well below targetAlpha.
   *
   * Strategies near the decision boundary (implied p close to targetAlpha) do
   * not trigger either condition and continue to the full permutation count,
   * preserving precision exactly where it matters.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template <typename Decimal>
  class ThresholdEarlyStoppingPolicy
  {
  public:
    /**
     * @brief Construct with configurable stopping parameters.
     *
     * @param checkInterval    How often (in completed permutations) to evaluate
     *                         stopping conditions. Default: every 100.
     * @param minBeforeStop    Minimum valid permutations before any stopping check
     *                         is attempted. Default: 200.
     * @param minPassPerms     Minimum valid permutations before a zero extreme-count
     *                         is considered sufficient to declare a clear pass.
     *                         Default: 500.
     * @param failingMultiplier  Multiplier applied to targetAlpha for the failing
     *                           threshold. A strategy is considered clearly failing
     *                           when impliedP > failingMultiplier * targetAlpha.
     *                           Default: DecimalConstants<Decimal>::DecimalThree (3x).
     */
    explicit ThresholdEarlyStoppingPolicy(
        uint32_t checkInterval    = 100,
        uint32_t minBeforeStop    = 200,
        uint32_t minPassPerms     = 500,
        Decimal  failingMultiplier = DecimalConstants<Decimal>::DecimalThree)
      : m_checkInterval(checkInterval)
      , m_minBeforeStop(minBeforeStop)
      , m_minPassPerms(minPassPerms)
      , m_failingMultiplier(failingMultiplier)
    {}

    /**
     * @brief Evaluate whether the permutation loop should stop early.
     *
     * Must be called after the permutation's contribution to validPerms and
     * extremeCount has already been committed (i.e. after the atomic increments).
     *
     * Uses PValueComputationPolicy::computePermutationPValue for the implied
     * p-value so the stopping decision is consistent with the final reported
     * p-value, including any Wilson correction.
     *
     * @tparam PValueComputationPolicy  The same p-value policy used for the
     *                                  final result.
     * @param validPerms   Current count of completed valid permutations.
     * @param extremeCount Current count of permutations at least as extreme
     *                     as the baseline.
     * @param targetAlpha  The significance threshold for the test.
     * @return true if the loop should stop; false to continue.
     */
    template <typename PValueComputationPolicy>
    bool shouldStop(uint32_t       validPerms,
                    uint32_t       extremeCount,
                    const Decimal& targetAlpha) noexcept
    {
      // Do not evaluate until we have a meaningful sample.
      if (validPerms < m_minBeforeStop)
        return false;

      // Only check at multiples of checkInterval to avoid hammering atomics.
      if (validPerms % m_checkInterval != 0)
        return false;

      // --- Clearly passing ---
      // Zero extreme counts after minPassPerms valid permutations means the
      // p-value is already well below targetAlpha regardless of policy.
      if (extremeCount == 0 && validPerms >= m_minPassPerms)
        return true;

      // --- Clearly failing ---
      // Use the same PValueComputationPolicy as the final result so the
      // stopping threshold is measured on the same scale as the reported value.
      const Decimal impliedP =
          PValueComputationPolicy::computePermutationPValue(extremeCount, validPerms);

      if (impliedP > m_failingMultiplier * targetAlpha)
        return true;

      return false;
    }

  private:
    uint32_t m_checkInterval;
    uint32_t m_minBeforeStop;
    uint32_t m_minPassPerms;
    Decimal  m_failingMultiplier;
  };

  // ============================================================================
  // DefaultPermuteMarketChangesPolicy
  // ============================================================================

  /**
   * @class DefaultPermuteMarketChangesPolicy
   * @brief Performs a hypothesis test via Monte-Carlo permutation testing of a trading strategy.
   *
   * This class implements a Monte-Carlo permutation testing methodology to evaluate
   * the statistical significance of a trading strategy's performance. It operates on a
   * trading strategy represented by the BacktesterStrategy class and utilises a
   * backtesting engine provided by the BackTester class.
   *
   * The core principle involves permuting market changes to generate multiple synthetic
   * market scenarios. The strategy is then backtested on these scenarios to create a
   * distribution of performance metrics, which is used to assess the likelihood that
   * the original strategy's performance was due to chance.
   *
   * @tparam Decimal
   *   The numerical type used for calculations (e.g., double, mkc_timeseries::number).
   *
   * @tparam BackTestResultPolicy
   *   Policy that extracts the relevant test statistic from a backtest result.
   *   Must define:
   *   - static Decimal getPermutationTestStatistic(shared_ptr<BackTester<Decimal>>)
   *   - static uint32_t getMinStrategyTrades()
   *
   * @tparam _PermutationTestResultPolicy
   *   Policy determining the return type of runPermutationTest. Must define a
   *   nested ReturnType and:
   *   - static ReturnType createReturnValue(Decimal pValue, Decimal summaryTestStat,
   *                                         Decimal baseLineTestStat)
   *   Defaults to PValueReturnPolicy<Decimal>.
   *
   * @tparam _PermutationTestStatisticsCollectionPolicy
   *   Policy for collecting statistics across permutations (used by multiple-testing
   *   correction algorithms). Must implement:
   *   - void updateTestStatistic(Decimal)
   *   - Decimal getTestStat()
   *   Defaults to PermutationTestingNullTestStatisticPolicy<Decimal> (no-op).
   *
   * @tparam Executor
   *   Policy defining the execution model for the permutation loop.
   *   Defaults to concurrency::ThreadPoolExecutor<>.
   *   Use concurrency::SingleThreadExecutor for the inner loop when the outer
   *   loop is already parallelised to avoid oversubscription.
   *
   * @tparam PValueComputationPolicy
   *   Policy for computing the final p-value from (extremeCount, validPerms).
   *   Defaults to StandardPValueComputationPolicy<Decimal>.
   *
   * @tparam NullModel
   *   The synthetic null model used to generate permuted series.
   *   Defaults to SyntheticNullModel::N0_PairedDay, which is appropriate for
   *   price-action strategies that use only bar bodies (O and C) and fixed
   *   percentage exits. Use N1_MaxDestruction or N2_BlockDays if your strategies
   *   are sensitive to the gap/intraday-shape joint distribution or to
   *   inter-day clustering respectively.
   *
   * @tparam EarlyStoppingPolicy
   *   Policy controlling whether the permutation loop may stop before
   *   numPermutations when the outcome is already statistically clear.
   *   Defaults to NoEarlyStoppingPolicy<Decimal>, which preserves the original
   *   behaviour exactly and ensures existing unit tests are unaffected.
   *   Use ThresholdEarlyStoppingPolicy<Decimal> to enable early stopping for
   *   production runs (typically 3-8x faster on clearly failing strategies).
   */
  template <class Decimal,
            class BackTestResultPolicy,
            typename _PermutationTestResultPolicy              = PValueReturnPolicy<Decimal>,
            typename _PermutationTestStatisticsCollectionPolicy
                                                               = PermutationTestingNullTestStatisticPolicy<Decimal>,
            typename Executor                                  = concurrency::ThreadPoolExecutor<>,
            typename PValueComputationPolicy                   = StandardPValueComputationPolicy<Decimal>,
            SyntheticNullModel NullModel                       = SyntheticNullModel::N1_MaxDestruction,
            typename EarlyStoppingPolicy                       = NoEarlyStoppingPolicy<Decimal>>
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
     * For each permutation:
     *   a. Builds a synthetic price series via the configured NullModel.
     *   b. Runs the strategy on the synthetic series using a per-thread cloned
     *      BackTester (TLS — rebuilt only when the security changes).
     *   c. Computes the permutation test statistic.
     *   d. Increments validPerms; increments extremeCount if the statistic is
     *      at least as extreme as baseLineTestStat.
     *   e. Delegates to EarlyStoppingPolicy to determine whether to stop early.
     *
     * All permutations are dispatched via the Executor policy. With
     * SingleThreadExecutor they run serially on the calling thread; with
     * ThreadPoolExecutor<> they run across a thread pool.
     *
     * @param theBackTester    BackTester containing the strategy to test.
     * @param numPermutations  Maximum number of permutations to run.
     * @param baseLineTestStat Test statistic from the original unpermuted data.
     * @param targetAlpha      Significance threshold used by EarlyStoppingPolicy
     *                         to determine when the outcome is clear. Defaults to
     *                         DecimalConstants<Decimal>::SignificantPValue (0.05).
     *                         Has no effect when EarlyStoppingPolicy is
     *                         NoEarlyStoppingPolicy.
     * @return ReturnType      P-value (and optional summary statistic) as defined
     *                         by _PermutationTestResultPolicy.
     */
    ReturnType
    runPermutationTest(std::shared_ptr<BackTester<Decimal>> theBackTester,
                       uint32_t       numPermutations,
                       const Decimal& baseLineTestStat,
                       const Decimal& targetAlpha
                           = DecimalConstants<Decimal>::SignificantPValue)
    {
      if (numPermutations == 0)
        throw std::invalid_argument(
          "DefaultPermuteMarketChangesPolicy::runPermutationTest: "
          "numPermutations must be > 0");

      if (!theBackTester)
        throw std::invalid_argument(
          "DefaultPermuteMarketChangesPolicy::runPermutationTest: "
          "theBackTester cannot be null");

      // Grab the baseline strategy and its security/portfolio ONCE (before parallel work).
      auto aStrategy = *(theBackTester->beginStrategies());

      if (aStrategy->beginPortfolio() == aStrategy->endPortfolio())
        throw std::runtime_error(
          "DefaultPermuteMarketChangesPolicy::runPermutationTest: "
          "Strategy portfolio is empty - use getRandomPalStrategy(security) "
          "to create strategy with populated portfolio");

      auto theSecurity       = aStrategy->beginPortfolio()->second;
      auto originalPortfolio = aStrategy->getPortfolio();

      if (!theSecurity)
        throw std::runtime_error(
          "DefaultPermuteMarketChangesPolicy::runPermutationTest: Security is null");
      if (!originalPortfolio)
        throw std::runtime_error(
          "DefaultPermuteMarketChangesPolicy::runPermutationTest: Portfolio is null");

      // ---- Shared state accessed by all worker invocations ----
      std::atomic<uint32_t> validPerms{0}, extremeCount{0}, failedPerms{0};
      std::atomic<bool>     shouldStopFlag{false};

      // Optional summary collector (unchanged contract).
      _PermutationTestStatisticsCollectionPolicy testStatCollector;
      std::mutex                                 testStatMutex;

      // Early stopping policy instance — stateless for NoEarlyStoppingPolicy,
      // parameterised for ThresholdEarlyStoppingPolicy.
      EarlyStoppingPolicy earlyStop{};

      // ---- Work lambda: TLS cache/portfolio/bt, one per worker thread ----
      auto work = [=,
                   &validPerms, &extremeCount, &failedPerms,
                   &shouldStopFlag, &earlyStop,
                   &testStatCollector, &testStatMutex](uint32_t /*permIndex*/)
      {
        // Honour a stop signal set by a previous iteration on this thread.
        // With SingleThreadExecutor this is checked at the top of every
        // permutation; with ThreadPoolExecutor each worker thread checks it
        // independently.
        if (shouldStopFlag.load(std::memory_order_relaxed))
          return;

        // --- Thread-local state (initialised once per worker thread) ---
        static thread_local RandomMersenne tls_rng = RandomMersenne::withStream(
    static_cast<uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()))
);
        static thread_local std::unique_ptr<CacheType>           tls_cache;
        static thread_local std::shared_ptr<Portfolio<Decimal>>  tls_portfolio;
        static thread_local std::shared_ptr<BackTester<Decimal>> tls_bt;

        // Sentinel: rebuild all TLS objects atomically when the security
        // pointer changes between runPermutationTest calls on the same thread.
        static thread_local const Security<Decimal>* tls_security_key = nullptr;

        if (tls_security_key != theSecurity.get()) {
          tls_cache        = std::make_unique<CacheType>(theSecurity);
          tls_portfolio    = std::make_shared<Portfolio<Decimal>>(*originalPortfolio);
          tls_bt           = theBackTester->clone();
          tls_security_key = theSecurity.get();
        }

        try
        {
          // 1) Build synthetic series into the reusable per-thread Security.
          auto& synSec = tls_cache->shuffleAndRebuild(tls_rng);

          // 2) Swap that Security into the per-thread portfolio.
          tls_portfolio->replaceSecurity(synSec);

          // 3) Create a fresh strategy for this permutation (fresh broker state),
          //    then reuse the already-cloned per-thread BackTester.
          auto clonedStrat = aStrategy->clone_shallow(tls_portfolio);
          tls_bt->setSingleStrategy(clonedStrat);
          tls_bt->backtest();

          // 4) Compute permutation statistic from the reused tls_bt.
          const Decimal testStat =
              BackTestResultPolicy::getPermutationTestStatistic(tls_bt);

          // 5) Update valid/extreme counts BEFORE notifying observers so that
          //    any observer inspecting running counts sees up-to-date values.
          validPerms.fetch_add(1, std::memory_order_relaxed);
          if (testStat >= baseLineTestStat)
            extremeCount.fetch_add(1, std::memory_order_relaxed);

          // 6) Notify observers (after atomics are updated).
          this->notifyObservers(*tls_bt, testStat);

          // 7) Update optional summary collector (mutex guards non-atomic policy).
          {
            std::lock_guard<std::mutex> guard(testStatMutex);
            testStatCollector.updateTestStatistic(testStat);
          }

          // 8) Delegate early stopping decision to the policy.
          //    NoEarlyStoppingPolicy::shouldStop is a constexpr false — the
          //    compiler eliminates this entire block for the default policy.
          if (earlyStop.template shouldStop<PValueComputationPolicy>(
                validPerms.load(std::memory_order_relaxed),
                extremeCount.load(std::memory_order_relaxed),
                targetAlpha))
          {
            shouldStopFlag.store(true, std::memory_order_relaxed);
          }
        }
        catch (const std::exception& ex)
        {
          failedPerms.fetch_add(1, std::memory_order_relaxed);
          std::cerr << "DefaultPermuteMarketChangesPolicy: permutation failed: "
                    << ex.what() << '\n';

          // Invalidate TLS sentinel so the next permutation rebuilds all three
          // objects from scratch — they may be in a partially-modified state.
          tls_security_key = nullptr;
        }
        catch (...)
        {
          failedPerms.fetch_add(1, std::memory_order_relaxed);
          std::cerr << "DefaultPermuteMarketChangesPolicy: permutation failed "
                       "with unknown exception\n";
          tls_security_key = nullptr;
        }
      };

      // Execute permutations via the configured Executor.
      Executor executor{};
      concurrency::parallel_for_chunked(numPermutations, executor, work, 1u);

      // ---- Failure diagnostic ------------------------------------------------
      // If a significant fraction of permutations failed, the p-value is
      // computed from too few samples to be reliable.
      const uint32_t failed = failedPerms.load(std::memory_order_relaxed);
      if (failed > 0)
      {
        const double failRate =
            static_cast<double>(failed) / static_cast<double>(numPermutations);
        std::cerr << "DefaultPermuteMarketChangesPolicy: " << failed << " of "
                  << numPermutations << " permutations failed ("
                  << static_cast<int>(failRate * 100.0) << "%).\n";
        if (failRate > 0.05)
          throw std::runtime_error(
            "DefaultPermuteMarketChangesPolicy::runPermutationTest: "
            "more than 5% of permutations failed — p-value is unreliable");
      }

      // ---- Final aggregation -------------------------------------------------
      const uint32_t valid = validPerms.load(std::memory_order_relaxed);
      if (valid == 0)
      {
        if (failed == numPermutations)
        {
          // Every permutation threw — the failure diagnostic above has details.
          std::cerr << "DefaultPermuteMarketChangesPolicy: no valid permutations — "
                    << "all " << numPermutations << " permutations failed with "
                    << "exceptions. Returning p-value of 1 (cannot reject null).\n";
        }
        else
        {
          // No exceptions but nothing counted as valid — logically unreachable
          // in the current control flow; likely indicates a bug.
          std::cerr << "DefaultPermuteMarketChangesPolicy: no valid permutations — "
                    << failed << " of " << numPermutations
                    << " failed with exceptions, but the remaining "
                    << (numPermutations - failed)
                    << " produced no valid counts. This may indicate a bug. "
                    << "Returning p-value of 1 (cannot reject null).\n";
        }
        return _PermutationTestResultPolicy::createReturnValue(
            Decimal(1),
            testStatCollector.getTestStat(),
            baseLineTestStat);
      }

      const uint32_t extreme = extremeCount.load(std::memory_order_relaxed);
      const Decimal  pValue  =
          PValueComputationPolicy::computePermutationPValue(extreme, valid);
      const Decimal  summaryTestStat = testStatCollector.getTestStat();

      return _PermutationTestResultPolicy::createReturnValue(
          pValue, summaryTestStat, baseLineTestStat);
    }
  };

} // namespace mkc_timeseries

#endif
