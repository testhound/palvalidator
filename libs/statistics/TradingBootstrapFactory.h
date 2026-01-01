// TradingBootstrapFactory.h
#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "BasicBootstrap.h"
#include "NormalBootstrap.h"
#include "PercentileBootstrap.h"
#include "BiasCorrectedBootstrap.h"
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

namespace palvalidator::analysis {
    template <class Decimal, class Sampler, class Rng, class Provider>
    class BCaCompatibleTBootstrap; // As defined above
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
 * masterSeed → strategyId → stageTag → blockLength → fold → replicate
 *     │            │            │            │          │         │
 *     │            │            │            │          │         └─ Bootstrap iteration [0, B)
 *     │            │            │            │          └─────────── CV fold or NO_FOLD (0)
 *     │            │            │            └────────────────────── Block length parameter
 *     │            │            └─────────────────────────────────── Metric type (see below)
 *     │            └──────────────────────────────────────────────── Strategy hash
 *     └───────────────────────────────────────────────────────────── Factory's master seed
 * @endcode
 *
 * @subsection tag_semantics Tag Level Semantics
 *
 * **masterSeed**: Set at factory construction; controls reproducibility of all analyses
 * created by this factory instance.
 *
 * **strategyId**: Derived from `strategy.hashCode()` or passed explicitly. Ensures each
 * trading strategy being analyzed uses an independent random stream.
 *
 * **stageTag**: Identifies the metric type. Standard values from `BootstrapStages`:
 * - `BCA_MEAN = 0`: Arithmetic mean bootstrap
 * - `GEO_MEAN = 1`: Geometric mean / CAGR bootstrap
 * - `PROFIT_FACTOR = 2`: Profit factor bootstrap
 * - Additional values can be added for new metrics
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
 *     myStrategy,                        // Strategy (for hashCode)
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
 * // These share masterSeed, strategyId, stageTag, and fold
 * // Only blockLength differs → variance reduction when comparing
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
 * @param strategy Either a `BacktesterStrategy<Decimal>` object (calls `.hashCode()`) or
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
 * 4. **Document Custom Tags**: If adding new metric types, document them in the
 *    BootstrapStages namespace with clear semantic meaning.
 *
 * 5. **CV Fold Numbering**: Start cross-validation folds at FOLD_1, reserving NO_FOLD
 *    for non-CV analyses.
 *
 * @tparam Engine The random number engine type (default: randutils::mt19937_rng).
 * Must be compatible with CRNEngineProvider.
 *
 * @see mkc_timeseries::rng_utils::CRNKey for low-level CRN key documentation
 * @see mkc_timeseries::rng_utils::CRNEngineProvider for engine construction
 * @see BootstrapStages namespace in BootstrapAnalysisStage.h for standard tag constants
 */
template<class Engine = randutils::mt19937_rng>
class TradingBootstrapFactory
{
public:
  explicit TradingBootstrapFactory(uint64_t masterSeed) : m_masterSeed(masterSeed) {}

  // ========== Overloads accepting BacktesterStrategy (calls hashCode()) ==========

  // Full control: custom statFn + BacktesterStrategy object
  template<class Decimal, class Resampler>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               std::function<Decimal(const std::vector<Decimal>&)> statFn,
               Resampler sampler,
               const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
               uint64_t stageTag, uint64_t L, uint64_t fold)
    -> BCaBootStrap<Decimal, Resampler, Engine, mkc_timeseries::rng_utils::CRNEngineProvider<Engine>>
  {
    return makeBCaImpl(returns, B, CL, std::move(statFn), std::move(sampler),
                       strategy.hashCode(), stageTag, L, fold);
  }

  // Convenience: default statistic (mean) + BacktesterStrategy object
  template<class Decimal, class Resampler>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               Resampler sampler,
               const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
               uint64_t stageTag, uint64_t L, uint64_t fold)
    -> BCaBootStrap<Decimal, Resampler, Engine, mkc_timeseries::rng_utils::CRNEngineProvider<Engine>>
  {
    using Stat = mkc_timeseries::StatUtils<Decimal>;
    return makeBCaImpl(returns, B, CL,
                       std::function<Decimal(const std::vector<Decimal>&)>(&Stat::computeMean),
                       std::move(sampler), strategy.hashCode(), stageTag, L, fold);
  }

  // ========== Overloads accepting raw uint64_t strategy ID ==========

  // Full control: custom statFn + raw strategy ID
  template<class Decimal, class Resampler>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               std::function<Decimal(const std::vector<Decimal>&)> statFn,
               Resampler sampler,
               uint64_t strategyId,
               uint64_t stageTag, uint64_t L, uint64_t fold)
    -> BCaBootStrap<Decimal, Resampler, Engine, mkc_timeseries::rng_utils::CRNEngineProvider<Engine>>
  {
    return makeBCaImpl(returns, B, CL, std::move(statFn), std::move(sampler),
                       strategyId, stageTag, L, fold);
  }

  // Convenience: default statistic (mean) + raw strategy ID
  template<class Decimal, class Resampler>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               Resampler sampler,
               uint64_t strategyId,
               uint64_t stageTag, uint64_t L, uint64_t fold)
    -> BCaBootStrap<Decimal, Resampler, Engine, mkc_timeseries::rng_utils::CRNEngineProvider<Engine>>
  {
    using Stat = mkc_timeseries::StatUtils<Decimal>;
    return makeBCaImpl(returns, B, CL,
                       std::function<Decimal(const std::vector<Decimal>&)>(&Stat::computeMean),
                       std::move(sampler), strategyId, stageTag, L, fold);
  }

  // ===========================================================================
  //                    MOutOfNPercentileBootstrap
  // ===========================================================================

  // BacktesterStrategy overload (Executor defaults to SingleThreadExecutor)
  template<class Decimal, class Sampler, class Resampler,
	   class Executor = concurrency::SingleThreadExecutor>
  auto makeMOutOfN(std::size_t B, double CL, double m_ratio,
		   const Resampler& resampler,
		   const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
		   uint64_t stageTag, uint64_t L, uint64_t fold,
		   bool rescale_to_n = false)
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

    const uint64_t sid = static_cast<uint64_t>(strategy.hashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, L, fold) );

    Bootstrap mn(B, CL, m_ratio, resampler, rescale_to_n);
    return std::make_pair(std::move(mn), std::move(crn));
  }

  // Raw strategyId overload (Executor defaults to SingleThreadExecutor)
  template<class Decimal, class Sampler, class Resampler,
	   class Executor = concurrency::SingleThreadExecutor>
  auto makeMOutOfN(std::size_t B, double CL, double m_ratio,
		   const Resampler& resampler,
		   uint64_t strategyId,
		   uint64_t stageTag, uint64_t L, uint64_t fold,
		   bool rescale_to_n = false)
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

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, L, fold) );

    Bootstrap mn(B, CL, m_ratio, resampler, rescale_to_n);
    return std::make_pair(std::move(mn), std::move(crn));
  }

  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeAdaptiveMOutOfN(std::size_t B, double CL,
                           const Resampler& resampler,
                           const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                           uint64_t stageTag, uint64_t L, uint64_t fold,
                           bool rescale_to_n = false)
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

    const uint64_t sid = static_cast<uint64_t>(strategy.hashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, L, fold) );

    // Use the default TailVolatilityAdaptivePolicy<Decimal, Sampler>
    // via MOutOfNPercentileBootstrap::createAdaptive.
    auto mn = Bootstrap::template createAdaptive<Sampler>(B, CL, resampler, rescale_to_n);

    return std::make_pair(std::move(mn), std::move(crn));
  }

  // Raw strategyId overload: adaptive m/n (TailVolatilityAdaptivePolicy by default)
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeAdaptiveMOutOfN(std::size_t B, double CL,
                           const Resampler& resampler,
                           uint64_t strategyId,
                           uint64_t stageTag, uint64_t L, uint64_t fold,
                           bool rescale_to_n = false)
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

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, L, fold) );

    // Use the default TailVolatilityAdaptivePolicy<Decimal, Sampler>
    auto mn = Bootstrap::template createAdaptive<Sampler>(B, CL, resampler, rescale_to_n);

    return std::make_pair(std::move(mn), std::move(crn));
  }

  // ===========================================================================
  //                     PercentileTBootstrap
  // ===========================================================================

  // Returns (bootstrap, CRNRng); Executor defaults to SingleThreadExecutor
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makePercentileT(std::size_t B_outer, std::size_t B_inner,
                       double CL,
                       const Resampler& resampler,
                       const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                       uint64_t stageTag, uint64_t L, uint64_t fold,
                       double m_ratio_outer = 1.0,
                       double m_ratio_inner = 1.0)
    -> std::pair<
         palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using PT  = palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.hashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, L, fold) );

    PT pt(B_outer, B_inner, CL, resampler, m_ratio_outer, m_ratio_inner);
    return std::make_pair(std::move(pt), std::move(crn));
  }

  // Raw strategyId
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makePercentileT(std::size_t B_outer, std::size_t B_inner,
                       double CL,
                       const Resampler& resampler,
                       uint64_t strategyId,
                       uint64_t stageTag, uint64_t L, uint64_t fold,
                       double m_ratio_outer = 1.0,
                       double m_ratio_inner = 1.0)
    -> std::pair<
         palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using PT  = palvalidator::analysis::PercentileTBootstrap<Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, L, fold) );
    PT pt(B_outer, B_inner, CL, resampler, m_ratio_outer, m_ratio_inner);
    return std::make_pair(std::move(pt), std::move(crn));
  }

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
		  .with_tags({ stageTag, L, fold })
		  );

    return PTB(returns, B, CL, std::move(statFn), std::move(sampler), prov);
  }

  // BacktesterStrategy overload
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBasic(std::size_t B, double CL,
                 const Resampler& resampler,
                 const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                 uint64_t stageTag, uint64_t L, uint64_t fold)
    -> std::pair<
         palvalidator::analysis::BasicBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::BasicBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.hashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, L, fold) );

    Bootstrap bb(B, CL, resampler);
    return std::make_pair(std::move(bb), std::move(crn));
  }

  // Raw strategyId overload
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeBasic(std::size_t B, double CL,
                 const Resampler& resampler,
                 uint64_t strategyId,
                 uint64_t stageTag, uint64_t L, uint64_t fold)
    -> std::pair<
         palvalidator::analysis::BasicBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::BasicBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, L, fold) );

    Bootstrap bb(B, CL, resampler);
    return std::make_pair(std::move(bb), std::move(crn));
  }

  // BacktesterStrategy overload for NormalBootstrap
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeNormal(std::size_t B, double CL,
                  const Resampler& resampler,
                  const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                  uint64_t stageTag, uint64_t L, uint64_t fold)
    -> std::pair<
         palvalidator::analysis::NormalBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::NormalBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.hashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, L, fold) );

    Bootstrap nb(B, CL, resampler);
    return std::make_pair(std::move(nb), std::move(crn));
  }

  // Raw strategyId overload for NormalBootstrap
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeNormal(std::size_t B, double CL,
                  const Resampler& resampler,
                  uint64_t strategyId,
                  uint64_t stageTag, uint64_t L, uint64_t fold)
    -> std::pair<
         palvalidator::analysis::NormalBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::NormalBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, L, fold) );

    Bootstrap nb(B, CL, resampler);
    return std::make_pair(std::move(nb), std::move(crn));
  }

  // BacktesterStrategy overload for PercentileBootstrap
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makePercentile(std::size_t B, double CL,
                      const Resampler& resampler,
                      const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                      uint64_t stageTag, uint64_t L, uint64_t fold)
    -> std::pair<
         palvalidator::analysis::PercentileBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::PercentileBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    const uint64_t sid = static_cast<uint64_t>(strategy.hashCode());
    CRNRng<Engine> crn( makeCRNKey(sid, stageTag, L, fold) );

    Bootstrap pb(B, CL, resampler);
    return std::make_pair(std::move(pb), std::move(crn));
  }

  // Raw strategyId overload for PercentileBootstrap
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makePercentile(std::size_t B, double CL,
                      const Resampler& resampler,
                      uint64_t strategyId,
                      uint64_t stageTag, uint64_t L, uint64_t fold)
    -> std::pair<
         palvalidator::analysis::PercentileBootstrap<Decimal, Sampler, Resampler, Engine, Executor>,
         mkc_timeseries::rng_utils::CRNRng<Engine>
       >
  {
    using Bootstrap = palvalidator::analysis::PercentileBootstrap<
        Decimal, Sampler, Resampler, Engine, Executor>;
    using mkc_timeseries::rng_utils::CRNRng;

    CRNRng<Engine> crn( makeCRNKey(strategyId, stageTag, L, fold) );

    Bootstrap pb(B, CL, resampler);
    return std::make_pair(std::move(pb), std::move(crn));
  }
  
private:
  uint64_t m_masterSeed;

  // Common implementation used by all overloads
  template<class Decimal, class Resampler>
  auto makeBCaImpl(const std::vector<Decimal>& returns,
                   unsigned B, double CL,
                   std::function<Decimal(const std::vector<Decimal>&)> statFn,
                   Resampler sampler,
                   uint64_t strategyHash,
                   uint64_t stageTag, uint64_t L, uint64_t fold)
    -> BCaBootStrap<Decimal, Resampler, Engine, mkc_timeseries::rng_utils::CRNEngineProvider<Engine>>
  {
    using Provider = mkc_timeseries::rng_utils::CRNEngineProvider<Engine>;
    using Key      = mkc_timeseries::rng_utils::CRNKey;

    Provider prov(
      Key(m_masterSeed)
        .with_tag(strategyHash)
        .with_tags({ stageTag, L, fold })
    );

    return BCaBootStrap<Decimal, Resampler, Engine, Provider>(
      returns, B, CL, std::move(statFn), std::move(sampler), prov
    );
  }

  // Build a CRNKey from domain tags (handy if you compose CRN outside)
  inline mkc_timeseries::rng_utils::CRNKey
  makeCRNKey(uint64_t strategyId, uint64_t stageTag, uint64_t L, uint64_t fold) const
  {
    using mkc_timeseries::rng_utils::CRNKey;

    return CRNKey(m_masterSeed)
              .with_tag(strategyId)
              .with_tags({ stageTag, L, fold });
  }
};
