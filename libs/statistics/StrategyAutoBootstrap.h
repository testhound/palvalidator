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
       * @throws std::invalid_argument if @p returns contains fewer than 2 elements.
       * @throws std::runtime_error if no engine produced a usable candidate.
       */
      Result run(const std::vector<SampleType>& returns, std::ostream* os = nullptr)
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

		    // BUG-2 FIX: Require at least 6 trades so the subsample has a
		    // minimum of ~4 observations (0.75 * 6 = 4.5 → 4).
		    // For n_trades < 6 the min_m6_floor below would exceed 1.0, asking
		    // the engine to draw more observations than exist in the original
		    // sample — oversampling, not subsampling. Skip the engine instead.
		    if (n_trades < 6)
		      {
			if (os)
			  (*os) << "   [AutoCI] MOutOfNPercentileBootstrap skipped: "
				   "fewer than 6 trades (n=" << n_trades << ")\n";
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
		    // wasBCaRejectedForInstability() fires for two distinct causes;
		    // inspect the BCa candidate directly to emit the specific message.
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
		      (*os) << "disqualified: dominant jackknife observation"
			       " + non-monotone BCa transform\n";
		    else if (accel_bad)
		      (*os) << "disqualified: dominant jackknife observation"
			       " in acceleration estimate\n";
		    else if (nonmono_bad)
		      (*os) << "disqualified: BCa percentile-transform"
			       " mapping reversed direction\n";
		    else
		      (*os) << "disqualified: BCa parameter instability"
			       " (sample size or skew gate)\n";
		  }
		else if (diagnostics.wasBCaRejectedForLength())
		  (*os) << "disqualified: interval too wide\n";
		else
		  (*os) << "outscored by winner\n";

		// When BCa was disqualified for instability, print the parameters
		// that triggered the gates.  Two independent failure modes can set
		// wasBCaRejectedForInstability(); both are surfaced here:
		//
		//   getAccelIsReliable()      — false if a single jackknife LOO
		//     observation contributed > 50% of the total absolute cubic
		//     influence Σ|d³|.  The acceleration estimate is then an
		//     artifact of that observation, not a distributional property,
		//     regardless of â's magnitude, z0, or skew_boot.
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
			      << (cand.getAccelIsReliable() ? "yes" : "NO — dominant jackknife observation")
			      << "\n";

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
