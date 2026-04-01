/**
 * @file TradingBootstrapFactory.h
 * @brief Factory for constructing bootstrap engines with hierarchical CRN streams.
 *
 * Provides TradingBootstrapFactory, which creates BCa, Basic, Normal,
 * Percentile, M-out-of-N, and Percentile-T bootstrap engines pre-configured
 * with Common Random Number (CRN) keys derived from a master seed and a
 * hierarchy of domain-specific tags (strategy, metric, method, block length,
 * fold).
 *
 * Copyright (C) MKC Associates, LLC - All Rights Reserved.
 * Written by Michael K. Collison
 */
#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "BootstrapTypes.h"
#include "BasicBootstrap.h"
#include "NormalBootstrap.h"
#include "PercentileBootstrap.h"
#include "BiasCorrectedBootstrap.h"
#include "TradeResampling.h"
#include "RngUtils.h"
#include "StatUtils.h"
#include "randutils.hpp"
#include "BacktesterStrategy.h"
#include "MOutOfNPercentileBootstrap.h"
#include "PercentileTBootstrap.h"

// Bring BCaBootStrap into scope
using mkc_timeseries::BCaBootStrap;
using mkc_timeseries::IIDResampler;
using mkc_timeseries::StationaryBlockResampler;
using palvalidator::analysis::IntervalType;

namespace palvalidator::analysis
{
  template <class Decimal, class Sampler, class Rng, class Provider,
	    class SampleType>
  class BCaCompatibleTBootstrap;
}

/**
 * @brief Bootstrap method identifiers for CRN hierarchy.
 *
 * These constants are used as tags in the CRN hierarchy to ensure each
 * bootstrap algorithm (Basic, Percentile, BCa, etc.) receives independent
 * random streams, even when analyzing the same strategy with the same
 * parameters. This is essential for tournament-style method selection where
 * each algorithm should be independently validated on different resamples.
 */
namespace BootstrapMethods {
    constexpr uint64_t BASIC        = 0;
    constexpr uint64_t NORMAL       = 1;
    constexpr uint64_t PERCENTILE   = 2;
    constexpr uint64_t MOUTOFN      = 3;
    constexpr uint64_t PERCENTILE_T = 4;
    constexpr uint64_t BCA          = 5;
}

/**
 * @brief Factory for creating bootstrap engines with hierarchical Common Random Numbers (CRN).
 *
 * @section overview Overview
 *
 * TradingBootstrapFactory constructs various bootstrap analysis engines (BCa, Basic, Normal,
 * Percentile, M-out-of-N, Percentile-T) with properly configured CRN streams. This ensures
 * reproducibility, independence between different analyses, and variance reduction when
 * comparing related bootstrap procedures.
 *
 * @section crn_hierarchy CRN Hierarchical Tag Structure
 *
 * This factory implements the following CRN key hierarchy for trading strategy bootstrap:
 *
 * @code
 * masterSeed → strategyId → stageTag → methodId → blockLength → fold → replicate
 *     │            │            │          │            │          │         │
 *     │            │            │          │            │          │         └─ Bootstrap iteration [0, B)
 *     │            │            │          │            │          └─────────── CV fold or NO_FOLD (0)
 *     │            │            │          │            └────────────────────── Block length parameter
 *     │            │            │          └─────────────────────────────────── Bootstrap method (Basic, BCa, etc)
 *     │            │            └────────────────────────────────────────────── Metric type (see below)
 *     │            └─────────────────────────────────────────────────────────── Strategy hash
 *     └──────────────────────────────────────────────────────────────────────── Factory's master seed
 * @endcode
 *
 * @subsection tag_semantics Tag Level Semantics
 *
 * **masterSeed**: Set at factory construction; controls reproducibility of all analyses
 * created by this factory instance.
 *
 * **strategyId**: Derived from `strategy.deterministicHashCode()` or passed explicitly. Ensures each
 * trading strategy being analyzed uses an independent random stream.
 *
 * **stageTag**: Identifies the metric type. Standard values from `BootstrapStages`:
 * - `BCA_MEAN = 0`: Arithmetic mean bootstrap
 * - `GEO_MEAN = 1`: Geometric mean / CAGR bootstrap
 * - `PROFIT_FACTOR = 2`: Profit factor bootstrap
 * - Additional values can be added for new metrics
 *
 * **methodId**: Identifies the bootstrap algorithm. Values from `BootstrapMethods`:
 * - `BASIC = 0`: Basic bootstrap
 * - `NORMAL = 1`: Normal approximation bootstrap
 * - `PERCENTILE = 2`: Percentile bootstrap
 * - `MOUTOFN = 3`: M-out-of-N percentile bootstrap
 * - `PERCENTILE_T = 4`: Percentile-T (double bootstrap)
 * - `BCA = 5`: Bias-corrected and accelerated bootstrap
 *
 * This ensures each bootstrap method receives independent random streams, which is
 * essential for tournament-style method selection where algorithms should be validated
 * independently rather than on synchronized resamples.
 *
 * **blockLength (L)**: The block size parameter for stationary block bootstrap. Typically
 * computed as max(median_hold_period, n^(1/3)).
 *
 * **fold**: Cross-validation fold identifier:
 * - `BootstrapStages::NO_FOLD = 0`: Single full-sample analysis (no CV)
 * - `BootstrapStages::FOLD_1 = 1, FOLD_2 = 2, ...`: For k-fold cross-validation
 *
 * **replicate**: Automatically added by CRNKey::make_seed_for(b) for bootstrap iteration b.
 *
 * @section usage Usage Examples
 *
 * @subsection ex_basic Basic Factory Usage
 * @code
 * // Create factory with master seed
 * TradingBootstrapFactory<> factory(0x123456789ABCDEF0);
 *
 * // Create BCa bootstrap for geometric mean
 * auto bcaEngine = factory.makeBCa(
 *     returns,
 *     1000,                              // B = 1000 resamples
 *     0.95,                              // 95% confidence level
 *     geoMeanStatFn,                     // Statistic function
 *     stationaryResampler,               // Block resampler
 *     myStrategy,                        // Strategy (for deterministicHashCode)
 *     BootstrapStages::GEO_MEAN,         // stageTag
 *     5,                                 // L = blockLength
 *     BootstrapStages::NO_FOLD           // fold
 * );
 * @endcode
 *
 * @subsection ex_multi Multiple Metrics for Same Strategy
 * @code
 * TradingBootstrapFactory<> factory(masterSeed);
 *
 * // Geometric mean bootstrap
 * auto [geoEngine, geoCRN] = factory.makePercentile<double, GeoMeanStat, ...>(
 *     1000, 0.95, resampler, myStrategy,
 *     BootstrapStages::GEO_MEAN, 5, BootstrapStages::NO_FOLD
 * );
 *
 * // Profit factor bootstrap (independent stream - no correlation with geo)
 * auto [pfEngine, pfCRN] = factory.makePercentile<double, ProfitFactorStat, ...>(
 *     1000, 0.95, resampler, myStrategy,
 *     BootstrapStages::PROFIT_FACTOR, 5, BootstrapStages::NO_FOLD
 * );
 *
 * // Run both with their independent CRN streams
 * auto geoResult = geoEngine.run(returns, geoMeanStat, geoCRN);
 * auto pfResult = pfEngine.run(returns, pfStat, pfCRN);
 * @endcode
 *
 * @subsection ex_cv Cross-Validation Setup
 * @code
 * TradingBootstrapFactory<> factory(masterSeed);
 * const int numFolds = 5;
 *
 * for (int fold = 0; fold < numFolds; ++fold) {
 *     auto [engine, crn] = factory.makeBasic<double, MeanStat, ...>(
 *         1000, 0.95, resampler, myStrategy,
 *         BootstrapStages::BCA_MEAN,
 *         5,
 *         BootstrapStages::FOLD_1 + fold  // Each fold gets independent stream
 *     );
 *
 *     auto result = engine.run(foldData[fold], meanStat, crn);
 *     // Store result for fold
 * }
 * @endcode
 *
 * @subsection ex_comparison Parameter Comparison with Variance Reduction
 * @code
 * // Compare block lengths using synchronized randomness
 * TradingBootstrapFactory<> factory(masterSeed);
 *
 * auto [engine_L5, crn_L5] = factory.makePercentile<...>(
 *     1000, 0.95, resampler_L5, myStrategy,
 *     BootstrapStages::GEO_MEAN, 5, BootstrapStages::NO_FOLD
 * );
 *
 * auto [engine_L10, crn_L10] = factory.makePercentile<...>(
 *     1000, 0.95, resampler_L10, myStrategy,
 *     BootstrapStages::GEO_MEAN, 10, BootstrapStages::NO_FOLD
 * );
 *
 * // These share masterSeed, strategyId, stageTag, methodId, and fold
 * // Only blockLength differs → variance reduction when comparing
 * @endcode
 *
 * @subsection ex_tournament Bootstrap Tournament (Independent Method Validation)
 * @code
 * // When running multiple bootstrap methods as a tournament, each method
 * // gets independent random draws via the methodId tag. This ensures:
 * // 1. Each method is independently validated
 * // 2. Methods don't all fail on the same pathological resamples
 * // 3. True robustness testing for method selection
 *
 * TradingBootstrapFactory<> factory(masterSeed);
 *
 * // BCa gets BootstrapMethods::BCA → unique resamples
 * auto bcaEngine = factory.makeBCa(...);
 *
 * // Percentile gets BootstrapMethods::PERCENTILE → different resamples
 * auto [pEngine, pCRN] = factory.makePercentile(...);
 *
 * // Basic gets BootstrapMethods::BASIC → different resamples
 * auto [bEngine, bCRN] = factory.makeBasic(...);
 *
 * // Even though all methods analyze the same strategy with same parameters,
 * // they each validate on independent bootstrap draws
 * @endcode
 *
 * @section methods Factory Methods
 *
 * The factory provides make*() methods for each bootstrap type:
 * - `makeBCa()`: Bias-corrected and accelerated bootstrap
 * - `makeBasic()`: Basic bootstrap
 * - `makeNormal()`: Normal approximation bootstrap
 * - `makePercentile()`: Percentile bootstrap
 * - `makeMOutOfN()`: M-out-of-N percentile bootstrap
 * - `makeAdaptiveMOutOfN()`: Adaptive M-out-of-N with tail volatility policy
 * - `makePercentileT()`: Percentile-T (double bootstrap)
 *
 * Each method returns:
 * - For BCa/PercentileT: The configured bootstrap engine
 * - For others: A pair of (engine, CRNRng) for manual stream management
 *
 * @section parameters Common Parameters
 *
 * @param strategy Either a `BacktesterStrategy<Decimal>` object (calls `.deterministicHashCode()`) or
 * a raw `uint64_t` strategy identifier. Use consistent IDs for the same strategy across
 * analyses to enable CRN synchronization.
 *
 * @param stageTag Identifies the metric type. Use constants from `BootstrapStages`
 * namespace (BCA_MEAN, GEO_MEAN, PROFIT_FACTOR) to ensure independence between metrics.
 *
 * @param L The block length parameter for block bootstrap resamplers. This enters the
 * CRN hierarchy to allow comparing results with different block lengths.
 *
 * @param fold Cross-validation fold identifier. Use `BootstrapStages::NO_FOLD` for
 * single-sample analysis, or `FOLD_1 + k` for fold k in cross-validation.
 *
 * @section best_practices Best Practices
 *
 * 1. **Use Named Constants**: Always use `BootstrapStages::*` constants rather than
 *    raw integers for stageTag and fold parameters.
 *
 * 2. **Strategy Consistency**: Use the same strategy identifier (hash) when comparing
 *    different metrics or parameters for the same strategy.
 *
 * 3. **Metric Independence**: Always use different stageTag values for different
 *    statistical metrics to prevent artificial correlation.
 *
 * 4. **Method Independence**: The factory automatically assigns unique methodId values
 *    to each bootstrap algorithm, ensuring independent validation in tournament scenarios.
 *
 * 5. **Document Custom Tags**: If adding new metric types, document them in the
 *    BootstrapStages namespace with clear semantic meaning.
 *
 * 6. **CV Fold Numbering**: Start cross-validation folds at FOLD_1, reserving NO_FOLD
 *    for non-CV analyses.
 *
 * @tparam Engine The random number engine type (default: randutils::mt19937_rng).
 * Must be compatible with CRNEngineProvider.
 *
 * @see mkc_timeseries::rng_utils::CRNKey for low-level CRN key documentation
 * @see mkc_timeseries::rng_utils::CRNEngineProvider for engine construction
 * @see BootstrapStages namespace in BootstrapAnalysisStage.h for standard tag constants
 * @see BootstrapMethods namespace for bootstrap algorithm identifiers
 */
template<class Engine = randutils::mt19937_rng>
class TradingBootstrapFactory
{
public:
  /**
   * @brief Constructs a factory seeded with the given master seed for CRN reproducibility.
   *
   * All bootstrap engines created by this factory derive their CRN keys from
   * this seed, ensuring reproducible and independent random streams across
   * strategies, metrics, methods, and folds.
   *
   * @param masterSeed  Root seed for the CRN hierarchy.
   */
  explicit TradingBootstrapFactory(uint64_t masterSeed) : m_masterSeed(masterSeed) {}

  /**
   * @brief Constructs a factory with a pre-created shared executor.
   *
   * The supplied executor is injected into every heavy bootstrap engine
   * (BCa, PercentileT, MOutOfN) that this factory constructs, ensuring the
   * same thread pool is reused across all engines in a tournament run rather
   * than spawning and joining a new pool for each engine.
   *
   * Pass the same shared_ptr to StrategyAutoBootstrap (which calls
   * setSharedExecutor on this factory at the start of each run()) to get
   * full persistence across all strategies in the outer tournament loop.
   *
   * @param masterSeed  Root seed for the CRN hierarchy.
   * @param sharedExec  A live ThreadPoolExecutor to share across engines.
   */
  TradingBootstrapFactory(uint64_t masterSeed,
                          std::shared_ptr<concurrency::ThreadPoolExecutor<>> sharedExec)
    : m_masterSeed(masterSeed)
    , m_sharedExec(std::move(sharedExec))
  {}

  /**
   * @brief Replaces the shared executor used for subsequent engine construction.
   *
   * Called by StrategyAutoBootstrap::run() to inject its own persistent pool
   * into this factory before the six make* calls. Passing nullptr clears the
   * shared executor so each subsequent engine creates its own pool.
   *
   * @param exec  New shared executor (may be nullptr).
   */
  void setSharedExecutor(std::shared_ptr<concurrency::ThreadPoolExecutor<>> exec)
  {
    m_sharedExec = std::move(exec);
  }

  /**
   * @brief Returns the currently stored shared executor, or nullptr if none is set.
   *
   * Used by StrategyAutoBootstrap's constructor to inherit an executor that was
   * installed at the factory level (e.g. by PerformanceFilter) without requiring
   * callers between the two — such as BootstrapAnalysisStage — to be aware of
   * executor management at all.
   *
   * @return The shared executor, or nullptr.
   */
  std::shared_ptr<concurrency::ThreadPoolExecutor<>> getSharedExecutor() const
  {
    return m_sharedExec;
  }

  // ========== Overloads accepting BacktesterStrategy (calls deterministicHashCode()) ==========

  /**
   * @brief Creates a BCa bootstrap engine with a custom statistic and BacktesterStrategy identity.
   *
   * Constructs a bias-corrected and accelerated (BCa) bootstrap engine for
   * bar-level return data. The strategy's deterministic hash code is used as
   * the CRN strategy tag.
   *
   * @tparam Decimal   Numeric type for returns (e.g., double, dec::decimal<8>).
   * @tparam Resampler Resampling policy (e.g., IIDResampler, StationaryBlockResampler).
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  returns       The observed return series.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1), e.g. 0.95 for 95%.
   * @param  statFn        User-supplied statistic function applied to each resample.
   * @param  sampler       Resampling policy instance.
   * @param  strategy      BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter for the CRN hierarchy.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness (default: TWO_SIDED).
   * @return A fully configured BCaBootStrap engine with CRNEngineProvider.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               std::function<Decimal(const std::vector<Decimal>&)> statFn,
               Resampler sampler,
               const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
               uint64_t stageTag, uint64_t L, uint64_t fold,
	       IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    Decimal, Executor>
  {
    return makeBCaImpl<Decimal, Resampler, Executor>(
        returns, B, CL, std::move(statFn), std::move(sampler),
        strategy.deterministicHashCode(), stageTag, L, fold, interval_type);
  }

  /**
   * @brief Creates a BCa bootstrap engine using the arithmetic mean and BacktesterStrategy identity.
   *
   * Convenience overload that defaults the statistic to StatUtils::computeMean.
   *
   * @tparam Decimal   Numeric type for returns.
   * @tparam Resampler Resampling policy.
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  returns       The observed return series.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  sampler       Resampling policy instance.
   * @param  strategy      BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter for the CRN hierarchy.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness (default: TWO_SIDED).
   * @return A fully configured BCaBootStrap engine with CRNEngineProvider.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               Resampler sampler,
               const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
               uint64_t stageTag, uint64_t L, uint64_t fold,
	       IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    Decimal, Executor>
  {
    using Stat = mkc_timeseries::StatUtils<Decimal>;
    return makeBCaImpl<Decimal, Resampler, Executor>(
        returns, B, CL,
        std::function<Decimal(const std::vector<Decimal>&)>(&Stat::computeMean),
        std::move(sampler), strategy.deterministicHashCode(), stageTag, L, fold,
        interval_type);
  }

  // ========== Overloads accepting raw uint64_t strategy ID ==========

  /**
   * @brief Creates a BCa bootstrap engine with a custom statistic and raw strategy ID.
   *
   * Identical to the BacktesterStrategy overload but accepts a pre-computed
   * strategy identifier directly, avoiding the need for a strategy object.
   *
   * @tparam Decimal   Numeric type for returns.
   * @tparam Resampler Resampling policy.
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  returns       The observed return series.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  statFn        User-supplied statistic function.
   * @param  sampler       Resampling policy instance.
   * @param  strategyId    Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter for the CRN hierarchy.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness (default: TWO_SIDED).
   * @return A fully configured BCaBootStrap engine with CRNEngineProvider.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               std::function<Decimal(const std::vector<Decimal>&)> statFn,
               Resampler sampler,
               uint64_t strategyId,
               uint64_t stageTag, uint64_t L, uint64_t fold,
	       IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    Decimal, Executor>
  {
    return makeBCaImpl<Decimal, Resampler, Executor>(
        returns, B, CL, std::move(statFn), std::move(sampler),
        strategyId, stageTag, L, fold, interval_type);
  }

  /**
   * @brief Creates a BCa bootstrap engine using the arithmetic mean and raw strategy ID.
   *
   * Convenience overload that defaults the statistic to StatUtils::computeMean
   * and accepts a raw strategy identifier.
   *
   * @tparam Decimal   Numeric type for returns.
   * @tparam Resampler Resampling policy.
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  returns       The observed return series.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  sampler       Resampling policy instance.
   * @param  strategyId    Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter for the CRN hierarchy.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness (default: TWO_SIDED).
   * @return A fully configured BCaBootStrap engine with CRNEngineProvider.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               Resampler sampler,
               uint64_t strategyId,
               uint64_t stageTag, uint64_t L, uint64_t fold,
	       IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    Decimal, Executor>
  {
    using Stat = mkc_timeseries::StatUtils<Decimal>;
    return makeBCaImpl<Decimal, Resampler, Executor>(
        returns, B, CL,
        std::function<Decimal(const std::vector<Decimal>&)>(&Stat::computeMean),
        std::move(sampler), strategyId, stageTag, L, fold, interval_type);
  }

  // ===========================================================================
  //  makeBCa — trade-level overloads (SampleType = Trade<Decimal>)
  //
  //  BCaBootStrap receives the data vector at construction time, so the factory
  //  can offer distinct overloads: the first argument type
  //  (vector<Decimal> vs vector<Trade<Decimal>>) lets the compiler select the
  //  right overload unambiguously, and Decimal is deduced from that argument.
  //
  //  No default-statistic convenience overload is provided — there is no
  //  analogue of computeMean for a vector of Trade objects.
  //
  //  Usage:
  //    GeoMeanStat<Decimal> geoMean;
  //    auto bca = factory.makeBCa(
  //        tradeVec, 1000, 0.95, geoMean,
  //        IIDResampler<Trade<Decimal>>{},
  //        strategy, BootstrapStages::GEO_MEAN, 1, BootstrapStages::NO_FOLD);
  // ===========================================================================

  /**
   * @brief Creates a trade-level BCa bootstrap engine with a BacktesterStrategy identity.
   *
   * Operates on a vector of Trade objects rather than raw returns. The caller
   * must supply an explicit statistic function because there is no default
   * statistic for trade vectors.
   *
   * @tparam Decimal   Numeric type used within Trade objects.
   * @tparam Resampler Resampling policy (must accept Trade<Decimal> vectors).
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  trades        The observed trade vector.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  statFn        Statistic function mapping a trade vector to a scalar.
   * @param  sampler       Resampling policy instance for Trade<Decimal>.
   * @param  strategy      BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter for the CRN hierarchy.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness (default: TWO_SIDED).
   * @return A BCaBootStrap engine parameterised on Trade<Decimal> as SampleType.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCa(const std::vector<mkc_timeseries::Trade<Decimal>>& trades,
               unsigned B, double CL,
               std::function<Decimal(const std::vector<mkc_timeseries::Trade<Decimal>>&)> statFn,
               Resampler sampler,
               const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
               uint64_t stageTag, uint64_t L, uint64_t fold,
               IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    mkc_timeseries::Trade<Decimal>, Executor>
  {
    return makeBCaTradeImpl<Decimal, Resampler, Executor>(
        trades, B, CL, std::move(statFn), std::move(sampler),
        static_cast<uint64_t>(strategy.deterministicHashCode()),
        stageTag, L, fold, interval_type);
  }

  /**
   * @brief Creates a trade-level BCa bootstrap engine with a raw strategy ID.
   *
   * Same as the BacktesterStrategy trade-level overload but accepts a
   * pre-computed strategy identifier directly.
   *
   * @tparam Decimal   Numeric type used within Trade objects.
   * @tparam Resampler Resampling policy (must accept Trade<Decimal> vectors).
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  trades        The observed trade vector.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  statFn        Statistic function mapping a trade vector to a scalar.
   * @param  sampler       Resampling policy instance for Trade<Decimal>.
   * @param  strategyId    Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter for the CRN hierarchy.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness (default: TWO_SIDED).
   * @return A BCaBootStrap engine parameterised on Trade<Decimal> as SampleType.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCa(const std::vector<mkc_timeseries::Trade<Decimal>>& trades,
               unsigned B, double CL,
               std::function<Decimal(const std::vector<mkc_timeseries::Trade<Decimal>>&)> statFn,
               Resampler sampler,
               uint64_t strategyId,
               uint64_t stageTag, uint64_t L, uint64_t fold,
               IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    mkc_timeseries::Trade<Decimal>, Executor>
  {
    return makeBCaTradeImpl<Decimal, Resampler, Executor>(
        trades, B, CL, std::move(statFn), std::move(sampler),
        strategyId, stageTag, L, fold, interval_type);
  }

  // ===========================================================================
  //                    MOutOfNPercentileBootstrap
  // ===========================================================================
  //
  //  MOutOfNPercentileBootstrap receives data at run() time, not at construction
  //  time, so the factory never sees a data vector. This means:
  //    • Overloads distinguished by vector<Decimal> vs vector<Trade<Decimal>>
  //      are impossible — nothing in the factory's argument list differs.
  //    • The correct mechanism is an explicit SampleType template parameter on
  //      the factory method, defaulting to Decimal so all existing call sites
  //      compile unchanged.
  //    • Callers opting into trade-level specify SampleType = Trade<Decimal>
  //      explicitly: factory.makeMOutOfN<Decimal, MySampler, IIDResampler<Trade<Decimal>>,
  //                                      Trade<Decimal>>(...);
  //
  //  NOTE: makeAdaptiveMOutOfN deliberately does NOT expose SampleType.
  //  Adaptive mode is blocked at trade level by a static_assert inside
  //  MOutOfNPercentileBootstrap — surfacing SampleType on the factory method
  //  would imply support for a path that always detonates at compile time.
  // ===========================================================================

  /**
   * @brief Creates an M-out-of-N percentile bootstrap engine with a BacktesterStrategy identity.
   *
   * Constructs an MOutOfNPercentileBootstrap that draws sub-samples of size
   * m = m_ratio * n from the original data. Useful when the full-sample
   * bootstrap is unreliable (e.g., heavy tails or dependent data).
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal; use Trade<Decimal> for trade-level).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  m_ratio        Sub-sample ratio m/n, in (0, 1].
   * @param  resampler      Resampling policy instance.
   * @param  strategy       BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  rescale_to_n   If true, rescale the sub-sample quantiles to full-sample scale.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (MOutOfNPercentileBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makeMOutOfN(std::size_t B, double CL, double m_ratio,
		   const Resampler& resampler,
		   const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
		   uint64_t stageTag, uint64_t L, uint64_t fold,
		   bool rescale_to_n = false,
		   IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
    palvalidator::analysis::MOutOfNPercentileBootstrap<
      Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
    mkc_timeseries::rng_utils::CRNRng<Engine>
    >
  {
    using Bootstrap =
      palvalidator::analysis::MOutOfNPercentileBootstrap<
	Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.deterministicHashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, BootstrapMethods::MOUTOFN, L, fold) );

    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    Bootstrap mn(B, CL, m_ratio, resampler, rescale_to_n, interval_type,
                 std::move(typedExec));
    return std::make_pair(std::move(mn), std::move(crn));
  }

  /**
   * @brief Creates an M-out-of-N percentile bootstrap engine with a raw strategy ID.
   *
   * Same as the BacktesterStrategy overload but accepts a pre-computed
   * strategy identifier directly.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  m_ratio        Sub-sample ratio m/n, in (0, 1].
   * @param  resampler      Resampling policy instance.
   * @param  strategyId     Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  rescale_to_n   If true, rescale the sub-sample quantiles to full-sample scale.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (MOutOfNPercentileBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makeMOutOfN(std::size_t B, double CL, double m_ratio,
		   const Resampler& resampler,
		   uint64_t strategyId,
		   uint64_t stageTag, uint64_t L,
		   uint64_t fold,
		   bool rescale_to_n = false,
		   IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
    palvalidator::analysis::MOutOfNPercentileBootstrap<
      Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
    mkc_timeseries::rng_utils::CRNRng<Engine>
    >
  {
    using Bootstrap =
      palvalidator::analysis::MOutOfNPercentileBootstrap<
	Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, BootstrapMethods::MOUTOFN, L, fold) );

    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    Bootstrap mn(B, CL, m_ratio, resampler, rescale_to_n, interval_type,
                 std::move(typedExec));
    return std::make_pair(std::move(mn), std::move(crn));
  }

  /**
   * @brief Creates an adaptive M-out-of-N bootstrap with TailVolatilityAdaptivePolicy and BacktesterStrategy identity.
   *
   * Delegates to MOutOfNPercentileBootstrap::createAdaptive, which selects the
   * sub-sample size m automatically based on the tail volatility of the data.
   * Does not expose SampleType because adaptive mode is blocked at the trade
   * level by a static_assert inside MOutOfNPercentileBootstrap.
   *
   * @tparam Decimal   Numeric type for returns.
   * @tparam Sampler   Statistic functor type.
   * @tparam Resampler Resampling policy.
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategy       BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  rescale_to_n   If true, rescale the sub-sample quantiles to full-sample scale.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (MOutOfNPercentileBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeAdaptiveMOutOfN(std::size_t B, double CL,
                           const Resampler& resampler,
                           const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                           uint64_t stageTag, uint64_t L, uint64_t fold,
                           bool rescale_to_n = false,
			   IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::MOutOfNPercentileBootstrap<
           Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap =
      palvalidator::analysis::MOutOfNPercentileBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.deterministicHashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, BootstrapMethods::MOUTOFN, L, fold) );

    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    // Use the default TailVolatilityAdaptivePolicy<Decimal, Sampler>
    // via MOutOfNPercentileBootstrap::createAdaptive.
    auto mn = Bootstrap::template createAdaptive<Sampler>(
                B, CL, resampler, rescale_to_n, interval_type, std::move(typedExec));

    return std::make_pair(std::move(mn), std::move(crn));
  }

  /**
   * @brief Creates an adaptive M-out-of-N bootstrap with TailVolatilityAdaptivePolicy and raw strategy ID.
   *
   * Same as the BacktesterStrategy overload but accepts a pre-computed
   * strategy identifier directly.
   *
   * @tparam Decimal   Numeric type for returns.
   * @tparam Sampler   Statistic functor type.
   * @tparam Resampler Resampling policy.
   * @tparam Executor  Parallel execution policy (default: SingleThreadExecutor).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategyId     Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  rescale_to_n   If true, rescale the sub-sample quantiles to full-sample scale.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (MOutOfNPercentileBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeAdaptiveMOutOfN(std::size_t B, double CL,
                           const Resampler& resampler,
                           uint64_t strategyId,
                           uint64_t stageTag, uint64_t L, uint64_t fold,
                           bool rescale_to_n = false,
			   IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::MOutOfNPercentileBootstrap<
           Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap =
      palvalidator::analysis::MOutOfNPercentileBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, BootstrapMethods::MOUTOFN, L, fold) );

    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    // Use the default TailVolatilityAdaptivePolicy<Decimal, Sampler>
    auto mn = Bootstrap::template createAdaptive<Sampler>(
                B, CL, resampler, rescale_to_n, interval_type, std::move(typedExec));

    return std::make_pair(std::move(mn), std::move(crn));
  }

  // ===========================================================================
  //                     PercentileTBootstrap
  // ===========================================================================

  /**
   * @brief Creates a Percentile-T (double bootstrap) engine with a BacktesterStrategy identity.
   *
   * Constructs a PercentileTBootstrap that performs a nested (outer + inner)
   * bootstrap to studentize the pivot statistic. Returns the engine paired
   * with its CRNRng for caller-managed streaming.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B_outer        Number of outer-loop bootstrap resamples.
   * @param  B_inner        Number of inner-loop bootstrap resamples per outer replicate.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategy       BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @param  m_ratio_outer  Sub-sample ratio for the outer loop (default: 1.0).
   * @param  m_ratio_inner  Sub-sample ratio for the inner loop (default: 1.0).
   * @return A pair of (PercentileTBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makePercentileT(std::size_t B_outer, std::size_t B_inner,
                       double CL,
                       const Resampler& resampler,
                       const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                       uint64_t stageTag,
		       uint64_t L,
		       uint64_t fold,
		       IntervalType interval_type = IntervalType::TWO_SIDED,
                       double m_ratio_outer = 1.0,
                       double m_ratio_inner = 1.0)
    -> std::pair<
         palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using PT  = palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.deterministicHashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, BootstrapMethods::PERCENTILE_T, L, fold) );

    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    PT pt(B_outer, B_inner, CL, resampler, m_ratio_outer, m_ratio_inner, interval_type,
          std::move(typedExec));
    return std::make_pair(std::move(pt), std::move(crn));
  }

  /**
   * @brief Creates a Percentile-T (double bootstrap) engine with a raw strategy ID.
   *
   * Same as the BacktesterStrategy overload but accepts a pre-computed
   * strategy identifier directly.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B_outer        Number of outer-loop bootstrap resamples.
   * @param  B_inner        Number of inner-loop bootstrap resamples per outer replicate.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategyId     Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @param  m_ratio_outer  Sub-sample ratio for the outer loop (default: 1.0).
   * @param  m_ratio_inner  Sub-sample ratio for the inner loop (default: 1.0).
   * @return A pair of (PercentileTBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makePercentileT(std::size_t B_outer, std::size_t B_inner,
                       double CL,
                       const Resampler& resampler,
                       uint64_t strategyId,
                       uint64_t stageTag, uint64_t L, uint64_t fold,
		       IntervalType interval_type = IntervalType::TWO_SIDED,
                       double m_ratio_outer = 1.0,
                       double m_ratio_inner = 1.0
		       )
    -> std::pair<
         palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using PT  = palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, BootstrapMethods::PERCENTILE_T, L, fold) );

    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    PT pt(B_outer, B_inner, CL, resampler, m_ratio_outer, m_ratio_inner, interval_type,
          std::move(typedExec));
    return std::make_pair(std::move(pt), std::move(crn));
  }

  /**
   * @brief Creates a studentized-T bootstrap engine compatible with BCa diagnostics.
   *
   * Constructs a BCaCompatibleTBootstrap that combines the Percentile-T
   * studentization approach with BCa-style bias correction. Accepts a raw
   * strategy ID only; no BacktesterStrategy overload is provided.
   *
   * @tparam Decimal   Numeric type for returns.
   * @tparam Resampler Resampling policy.
   * @param  returns       The observed return series.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  statFn        User-supplied statistic function.
   * @param  sampler       Resampling policy instance.
   * @param  strategyId    Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter for the CRN hierarchy.
   * @param  fold          Cross-validation fold identifier.
   * @return A fully configured BCaCompatibleTBootstrap engine.
   */
  template<class Decimal, class Resampler>
  auto makeStudentizedT(const std::vector<Decimal>& returns,
			unsigned B, double CL,
			std::function<Decimal(const std::vector<Decimal>&)> statFn,
			Resampler sampler,
			uint64_t strategyId,
			uint64_t stageTag, uint64_t L, uint64_t fold)
    -> palvalidator::analysis::BCaCompatibleTBootstrap<
    Decimal,
    Resampler,
    Engine,
    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>>
  {
    using PTB = palvalidator::analysis::BCaCompatibleTBootstrap<
      Decimal, Resampler, Engine, mkc_timeseries::rng_utils::CRNEngineProvider<Engine>>;

    using Provider = mkc_timeseries::rng_utils::CRNEngineProvider<Engine>;
    using Key = mkc_timeseries::rng_utils::CRNKey;

    Provider prov(
		  Key(m_masterSeed)
		  .with_tag(strategyId)
		  .with_tags({ stageTag, BootstrapMethods::PERCENTILE_T, L, fold })
		  );

    return PTB(returns, B, CL, std::move(statFn), std::move(sampler), prov);
  }

  /**
   * @brief Creates a basic bootstrap engine with a BacktesterStrategy identity.
   *
   * Constructs a BasicBootstrap that computes confidence intervals using the
   * simple pivot method (2*theta_hat - theta_star). Returns the engine paired
   * with its CRNRng for caller-managed streaming.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategy       BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (BasicBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makeBasic(std::size_t B, double CL,
                 const Resampler& resampler,
                 const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                 uint64_t stageTag,
		 uint64_t L,
		 uint64_t fold,
		 IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::BasicBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::BasicBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.deterministicHashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, BootstrapMethods::BASIC, L, fold) );

    Bootstrap bb(B, CL, resampler, interval_type);
    return std::make_pair(std::move(bb), std::move(crn));
  }

  /**
   * @brief Creates a basic bootstrap engine with a raw strategy ID.
   *
   * Same as the BacktesterStrategy overload but accepts a pre-computed
   * strategy identifier directly.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategyId     Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (BasicBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makeBasic(std::size_t B, double CL,
                 const Resampler& resampler,
                 uint64_t strategyId,
                 uint64_t stageTag,
		 uint64_t L,
		 uint64_t fold,
		 IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::BasicBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::BasicBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, BootstrapMethods::BASIC, L, fold) );

    Bootstrap bb(B, CL, resampler, interval_type);
    return std::make_pair(std::move(bb), std::move(crn));
  }

  /**
   * @brief Creates a normal-approximation bootstrap engine with a BacktesterStrategy identity.
   *
   * Constructs a NormalBootstrap that computes confidence intervals assuming
   * the bootstrap distribution of the statistic is approximately Gaussian.
   * Returns the engine paired with its CRNRng.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategy       BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (NormalBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makeNormal(std::size_t B, double CL,
                  const Resampler& resampler,
                  const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                  uint64_t stageTag,
		  uint64_t L,
		  uint64_t fold,
		  IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::NormalBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::NormalBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.deterministicHashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, BootstrapMethods::NORMAL, L, fold) );

    Bootstrap nb(B, CL, resampler, interval_type);
    return std::make_pair(std::move(nb), std::move(crn));
  }

  /**
   * @brief Creates a normal-approximation bootstrap engine with a raw strategy ID.
   *
   * Same as the BacktesterStrategy overload but accepts a pre-computed
   * strategy identifier directly.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategyId     Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (NormalBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makeNormal(std::size_t B, double CL,
                  const Resampler& resampler,
                  uint64_t strategyId,
                  uint64_t stageTag,
		  uint64_t L,
		  uint64_t fold,
		  IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::NormalBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::NormalBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, BootstrapMethods::NORMAL, L, fold) );

    Bootstrap nb(B, CL, resampler, interval_type);
    return std::make_pair(std::move(nb), std::move(crn));
  }

  /**
   * @brief Creates a percentile bootstrap engine with a BacktesterStrategy identity.
   *
   * Constructs a PercentileBootstrap that computes confidence intervals
   * directly from the quantiles of the bootstrap distribution. Returns the
   * engine paired with its CRNRng.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategy       BacktesterStrategy whose hash seeds the CRN stream.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (PercentileBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makePercentile(std::size_t B, double CL,
                      const Resampler& resampler,
                      const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                      uint64_t stageTag,
		      uint64_t L,
		      uint64_t fold,
		      IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::PercentileBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::PercentileBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.deterministicHashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, BootstrapMethods::PERCENTILE, L, fold) );

    Bootstrap pb(B, CL, resampler, interval_type);
    return std::make_pair(std::move(pb), std::move(crn));
  }

  /**
   * @brief Creates a percentile bootstrap engine with a raw strategy ID.
   *
   * Same as the BacktesterStrategy overload but accepts a pre-computed
   * strategy identifier directly.
   *
   * @tparam Decimal    Numeric type for returns.
   * @tparam Sampler    Statistic functor type.
   * @tparam Resampler  Resampling policy.
   * @tparam Executor   Parallel execution policy (default: SingleThreadExecutor).
   * @tparam SampleType Element type of the data vector (default: Decimal).
   * @param  B              Number of bootstrap resamples.
   * @param  CL             Confidence level in (0, 1).
   * @param  resampler      Resampling policy instance.
   * @param  strategyId     Raw strategy identifier for the CRN hierarchy.
   * @param  stageTag       Metric identifier from BootstrapStages.
   * @param  L              Block length parameter for the CRN hierarchy.
   * @param  fold           Cross-validation fold identifier.
   * @param  interval_type  Confidence interval sidedness (default: TWO_SIDED).
   * @return A pair of (PercentileBootstrap engine, CRNRng stream).
   */
  template<class Decimal, class Sampler, class Resampler,
           class Executor   = concurrency::SingleThreadExecutor,
           class SampleType = Decimal>
  auto makePercentile(std::size_t B, double CL,
                      const Resampler& resampler,
                      uint64_t strategyId,
                      uint64_t stageTag,
		      uint64_t L,
		      uint64_t fold,
		      IntervalType interval_type = IntervalType::TWO_SIDED)
    -> std::pair<
         palvalidator::analysis::PercentileBootstrap<Decimal, Sampler, Resampler, Engine, Executor, SampleType>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::PercentileBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor, SampleType>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, BootstrapMethods::PERCENTILE, L, fold) );

    Bootstrap pb(B, CL, resampler, interval_type);
    return std::make_pair(std::move(pb), std::move(crn));
  }

private:
  /// Master seed from which all CRN keys in this factory are derived.
  uint64_t m_masterSeed;

  /// Optional shared executor injected into heavy engines (BCa, PercentileT, MOutOfN).
  /// Null by default; set via the two-argument constructor or setSharedExecutor().
  /// When non-null and the engine's Executor type matches ThreadPoolExecutor<>,
  /// this pool is reused rather than constructing a new one per engine.
  std::shared_ptr<concurrency::ThreadPoolExecutor<>> m_sharedExec;

  /**
   * @brief Shared implementation for all bar-level BCa overloads.
   *
   * Constructs a CRNEngineProvider from the hierarchical key and forwards
   * all arguments to the BCaBootStrap constructor.
   *
   * @tparam Decimal   Numeric type for returns.
   * @tparam Resampler Resampling policy.
   * @tparam Executor  Parallel execution policy.
   * @param  returns       The observed return series.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  statFn        User-supplied statistic function.
   * @param  sampler       Resampling policy instance.
   * @param  strategyHash  Strategy identifier for the CRN hierarchy.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness.
   * @return A fully configured BCaBootStrap engine.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCaImpl(const std::vector<Decimal>& returns,
                   unsigned B, double CL,
                   std::function<Decimal(const std::vector<Decimal>&)> statFn,
                   Resampler sampler,
                   uint64_t strategyHash,
                   uint64_t stageTag, uint64_t L,
		   uint64_t fold,
		   IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    Decimal, Executor>
  {
    using Provider = mkc_timeseries::rng_utils::CRNEngineProvider<Engine>;
    using Key      = mkc_timeseries::rng_utils::CRNKey;

    Provider prov(
      Key(m_masterSeed)
        .with_tag(strategyHash)
        .with_tags({ stageTag, BootstrapMethods::BCA, L, fold })
    );

    // Pass the shared executor when the template Executor matches the stored pool type.
    // For any other Executor (e.g. SingleThreadExecutor in unit tests) typedExec is
    // nullptr and the engine creates its own — zero-cost since SingleThreadExecutor
    // has no threads to spawn.
    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    return BCaBootStrap<Decimal, Resampler, Engine, Provider, Decimal, Executor>(
        returns, B, CL, std::move(statFn), std::move(sampler), prov,
        interval_type, std::move(typedExec));
  }

  // ---------------------------------------------------------------------------
  // Trade-level BCa implementation.
  //
  // BCaBootStrap receives data at construction time, so the public makeBCa
  // overloads (distinguished by vector<Decimal> vs vector<Trade<Decimal>>) can
  // deduce Decimal from the data argument. This private helper exists purely to
  // avoid duplicating the CRN key construction between the BacktesterStrategy
  // and raw-strategyId public overloads, exactly mirroring makeBCaImpl.
  //
  // The CRN key hierarchy is identical to bar-level BCa — same
  // masterSeed → strategyHash → stageTag → BCA → L → fold chain — ensuring
  // a trade-level analysis and its bar-level counterpart for the same
  // strategy/metric each draw from independent streams without changing
  // the key structure.
  // ---------------------------------------------------------------------------

  /**
   * @brief Shared implementation for all trade-level BCa overloads.
   *
   * Mirrors makeBCaImpl but operates on Trade<Decimal> vectors. The CRN
   * key hierarchy is identical to bar-level BCa.
   *
   * @tparam Decimal   Numeric type used within Trade objects.
   * @tparam Resampler Resampling policy.
   * @tparam Executor  Parallel execution policy.
   * @param  trades        The observed trade vector.
   * @param  B             Number of bootstrap resamples.
   * @param  CL            Confidence level in (0, 1).
   * @param  statFn        Statistic function mapping a trade vector to a scalar.
   * @param  sampler       Resampling policy instance.
   * @param  strategyHash  Strategy identifier for the CRN hierarchy.
   * @param  stageTag      Metric identifier from BootstrapStages.
   * @param  L             Block length parameter.
   * @param  fold          Cross-validation fold identifier.
   * @param  interval_type Confidence interval sidedness.
   * @return A BCaBootStrap engine parameterised on Trade<Decimal>.
   */
  template<class Decimal, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBCaTradeImpl(
      const std::vector<mkc_timeseries::Trade<Decimal>>& trades,
      unsigned B, double CL,
      std::function<Decimal(const std::vector<mkc_timeseries::Trade<Decimal>>&)> statFn,
      Resampler sampler,
      uint64_t strategyHash,
      uint64_t stageTag, uint64_t L,
      uint64_t fold,
      IntervalType interval_type = IntervalType::TWO_SIDED)
    -> BCaBootStrap<Decimal, Resampler, Engine,
                    mkc_timeseries::rng_utils::CRNEngineProvider<Engine>,
                    mkc_timeseries::Trade<Decimal>, Executor>
  {
    using TradeT   = mkc_timeseries::Trade<Decimal>;
    using Provider = mkc_timeseries::rng_utils::CRNEngineProvider<Engine>;
    using Key      = mkc_timeseries::rng_utils::CRNKey;

    Provider prov(
      Key(m_masterSeed)
        .with_tag(strategyHash)
        .with_tags({ stageTag, BootstrapMethods::BCA, L, fold })
    );

    std::shared_ptr<Executor> typedExec;
    if constexpr (std::is_same_v<Executor, concurrency::ThreadPoolExecutor<>>)
      typedExec = m_sharedExec;

    return BCaBootStrap<Decimal, Resampler, Engine, Provider, TradeT, Executor>(
        trades, B, CL, std::move(statFn), std::move(sampler), prov,
        interval_type, std::move(typedExec));
  }

  /**
   * @brief Builds a CRNKey from domain-level tags.
   *
   * Utility for constructing the full hierarchical CRN key used by all
   * non-BCa factory methods (which manage CRNRng externally rather than
   * embedding a CRNEngineProvider).
   *
   * @param strategyId Strategy identifier (hash or raw ID).
   * @param stageTag   Metric identifier from BootstrapStages.
   * @param methodId   Bootstrap algorithm identifier from BootstrapMethods.
   * @param L          Block length parameter.
   * @param fold       Cross-validation fold identifier.
   * @return A fully constructed CRNKey.
   */
  inline mkc_timeseries::rng_utils::CRNKey
  makeCRNKey(uint64_t strategyId, uint64_t stageTag, uint64_t methodId,
             uint64_t L, uint64_t fold) const
  {
    using mkc_timeseries::rng_utils::CRNKey;

    return CRNKey(m_masterSeed)
              .with_tag(strategyId)
              .with_tags({ stageTag, methodId, L, fold });
  }
};
