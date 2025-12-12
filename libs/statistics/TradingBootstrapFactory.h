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
		   uint64_t stageTag, uint64_t L, uint64_t fold)
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

    Bootstrap mn(B, CL, m_ratio, resampler);  // Executor default-constructed inside
    return std::make_pair(std::move(mn), std::move(crn));
  }

  // Raw strategyId overload (Executor defaults to SingleThreadExecutor)
  template<class Decimal, class Sampler, class Resampler,
	   class Executor = concurrency::SingleThreadExecutor>
  auto makeMOutOfN(std::size_t B, double CL, double m_ratio,
		   const Resampler& resampler,
		   uint64_t strategyId,
		   uint64_t stageTag, uint64_t L, uint64_t fold)
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

    Bootstrap mn(B, CL, m_ratio, resampler);  // Executor default-constructed inside
    return std::make_pair(std::move(mn), std::move(crn));
  }

  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeAdaptiveMOutOfN(std::size_t B, double CL,
                           const Resampler& resampler,
                           const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                           uint64_t stageTag, uint64_t L, uint64_t fold)
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
    auto mn = Bootstrap::template createAdaptive<Sampler>(B, CL, resampler);

    return std::make_pair(std::move(mn), std::move(crn));
  }

  // Raw strategyId overload: adaptive m/n (TailVolatilityAdaptivePolicy by default)
  template<class Decimal, class Sampler, class Resampler,
           class Executor = concurrency::SingleThreadExecutor>
  auto makeAdaptiveMOutOfN(std::size_t B, double CL,
                           const Resampler& resampler,
                           uint64_t strategyId,
                           uint64_t stageTag, uint64_t L, uint64_t fold)
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
    auto mn = Bootstrap::template createAdaptive<Sampler>(B, CL, resampler);

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
