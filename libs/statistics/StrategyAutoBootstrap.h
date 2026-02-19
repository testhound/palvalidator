#pragma once

#include <vector>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <cmath>
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

      std::size_t getPercentileTNumInnerReplications(double ratio) const
      {
	const std::size_t outer_replications = m_numBootStrapReplications;

	// Use the publicly accessible constant from PercentileTBootstrap
	constexpr std::size_t kMinInnerReplications = percentile_t_constants::MIN_INNER;

	// Practical cap: diminishing returns beyond this because the PT engine
	// already has early stopping in the inner loop.
	constexpr std::size_t kMaxInnerReplications = 2000;

	// If ratio is nonsensical, fall back to the minimum workable inner size.
	if (!(std::isfinite(ratio)) || !(ratio > 0.0))
	  {
	    return std::min<std::size_t>(std::max<std::size_t>(kMinInnerReplications, 1),
					 kMaxInnerReplications);
	  }

	const double outer = static_cast<double>(outer_replications);
	double inner_d = outer / ratio;

	// Clamp inner draws to a sane / usable range
	if (inner_d < static_cast<double>(kMinInnerReplications))
	  inner_d = static_cast<double>(kMinInnerReplications);
	if (inner_d > static_cast<double>(kMaxInnerReplications))
	  inner_d = static_cast<double>(kMaxInnerReplications);

	return static_cast<std::size_t>(inner_d);
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
       * * @param sampler_instance An instance of Sampler. This allows passing a configured
       * statistic (e.g., LogProfitFactorStat with a specific stop-loss) rather than
       * default-constructing one. Defaults to Sampler() if not provided.
       */
      StrategyAutoBootstrap(Factory& factory,
                            const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                            const BootstrapConfiguration& bootstrapConfiguration,
                            const BootstrapAlgorithmsConfiguration& algorithmsConfiguration,
                            Sampler sampler_instance = Sampler(),
			    IntervalType interval_type = IntervalType::TWO_SIDED)
        : m_factory(factory),
          m_strategy(strategy),
          m_bootstrapConfiguration(bootstrapConfiguration),
          m_algorithmsConfiguration(algorithmsConfiguration),
          m_sampler_instance(sampler_instance),
	  m_interval_type(interval_type)
      {}

      /**
       * @brief Run all configured bootstrap engines on @p returns and select the best CI.
       *
       * @param returns  Bar-level return series (SampleType = Decimal) or trade-level
       *                 series (SampleType = Trade<Decimal>). The element type must match
       *                 the SampleType template parameter of this class.
       * @param os       Optional logging stream. If non-null, engine failures are logged.
       *
       * @return AutoCIResult<Decimal> encapsulating the chosen method and all candidates.
       *
       * @throws std::runtime_error if no engine produced a usable candidate.
       */
      Result run(const std::vector<SampleType>& returns, std::ostream* os = nullptr)
      {
	std::vector<Candidate> candidates;
	candidates.reserve(6);

	if (returns.size() < 2)
	  {
	    throw std::invalid_argument(
					"StrategyAutoBootstrap::run: requires at least 2 returns.");
	  }

	const std::size_t blockSize         = m_bootstrapConfiguration.getBlockSize();
	const double      cl                = m_bootstrapConfiguration.getConfidenceLevel();
	const std::size_t B_single          = m_bootstrapConfiguration.getNumBootStrapReplications();
	const std::uint64_t stageTag        = m_bootstrapConfiguration.getStageTag();
	const std::uint64_t fold            = m_bootstrapConfiguration.getFold();
	const std::size_t B_outer_percentileT =
	  m_bootstrapConfiguration.getPercentileTNumOuterReplications();
	const std::size_t B_inner_percentileT =
	  m_bootstrapConfiguration.getPercentileTNumInnerReplications(10.0);

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

		    auto [engine, crn] =
		      m_factory.template makeMOutOfN<Decimal, Sampler, Resampler, Executor, SampleType>(
							  B_single,
							  cl,
							  TRADE_LEVEL_MOUTOFN_RATIO,
							  resampler,
							  m_strategy,
							  stageTag,
							  static_cast<uint64_t>(1), // IID: block length = 1
							  fold,
							  rescaleMOutOfN,
							  m_interval_type);

		    auto res = engine.run(returns, m_sampler_instance, crn);
		    candidates.push_back(
		      Selector::template summarizePercentileLike(MethodId::MOutOfN, engine, res));
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
		  m_factory.template makeBCa<Decimal, Resampler>(
								    returns,
								    static_cast<unsigned>(B_single),
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
	    // (Logging code remains identical)
	    const auto& diagnostics = result.getDiagnostics();
	    const auto& chosen      = result.getChosenCandidate();

	    if (result.getChosenMethod() == MethodId::MOutOfN)
	      {
		(*os) << "\n[!] CRITICAL: Safety Valve Triggered (M-out-of-N chosen)\n";
		(*os) << "--------------------------------------------------------\n";
    
		// Find the BCa candidate to see why it failed (it usually wins)
		for (const auto& cand : result.getCandidates())
		  {
		    if (cand.getMethod() == MethodId::BCa)
		      {
			(*os) << "    BCa Stats (REJECTED):\n"
			      << "    - z0 (Bias): " << cand.getZ0() << "\n"
			      << "    - a (Accel): " << cand.getAccel() << "\n"
			      << "    - Stability Penalty: " << cand.getStabilityPenalty() << "\n"
			      << "    - Normalized Length: " << cand.getNormalizedLength() << "\n";
			
			if (std::abs(cand.getZ0()) > 0.4)
			  (*os) << "    -> DIAGNOSIS: Excessive Bias (z0 > 0.4)\n";
			if (std::abs(cand.getAccel()) > 0.1)
			  (*os) << "    -> DIAGNOSIS: Excessive Skew Sensitivity (a > 0.1)\n";
		      }
		    
		    if (cand.getMethod() == MethodId::Percentile)
		      {
			(*os) << "    Percentile Stats:\n"
			      << "    - Skewness: " << cand.getSkewBoot() << "\n"
			      << "    - Length Penalty: " << cand.getLengthPenalty() << "\n";
		      }
		  }
		(*os) << "--------------------------------------------------------\n\n";
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
    };
  } // namespace analysis
} // namespace palvalidator
