#pragma once

#include <vector>
#include <optional>
#include <ostream>
#include <stdexcept>

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

namespace palvalidator
{
  namespace analysis
  {
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
                             std::uint64_t fold)
        : m_numBootStrapReplications(numBootStrapReplications),
          m_blockSize(blockSize),
          m_confidenceLevel(confidenceLevel),
          m_stageTag(stageTag),
          m_fold(fold)
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

      /// Outer B for Percentile-T bootstrap.
      std::size_t getPercentileTNumOuterReplications() const
      {
        return m_numBootStrapReplications;
      }

      /// Inner B for Percentile-T bootstrap (outer / ratio, at least 1).
      std::size_t getPercentileTNumInnerReplications(double ratio) const
      {
        const double outer = static_cast<double>(m_numBootStrapReplications);
        double inner       = outer / ratio;
        if (inner < 1.0)
          inner = 1.0;
        return static_cast<std::size_t>(inner);
      }

    private:
      std::size_t   m_numBootStrapReplications;
      std::size_t   m_blockSize;
      double        m_confidenceLevel;
      std::uint64_t m_stageTag;
      std::uint64_t m_fold;
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
     *   - Uses a TradingBootstrapFactory to build concrete bootstrap engines:
     *     Normal, Basic, Percentile, M-out-of-N, Percentile-T, BCa
     *     (subject to BootstrapAlgorithmsConfiguration flags).
     *   - Converts each engine's result into an AutoBootstrapSelector::Candidate.
     *   - Calls AutoBootstrapSelector<Decimal>::select(...) and returns AutoCIResult<Decimal>.
     *
     * Template parameters:
     *   Decimal   : numeric type (e.g., mkc_timeseries::Decimal)
     *   Sampler   : statistic functor (e.g., mkc_timeseries::GeoMeanStat<Decimal>)
     *   Resampler : resampling adapter (e.g., StationaryMaskValueResamplerAdapter<Decimal>)
     */
    template <class Decimal, class Sampler, class Resampler>
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

      // BCa resampler is always a stationary block bootstrap in the current design.
      using BCaResampler = mkc_timeseries::StationaryBlockResampler<Decimal>;

      StrategyAutoBootstrap(Factory& factory,
                            const mkc_timeseries::BacktesterStrategy<Decimal>& strategy,
                            const BootstrapConfiguration& bootstrapConfiguration,
                            const BootstrapAlgorithmsConfiguration& algorithmsConfiguration)
        : m_factory(factory),
          m_strategy(strategy),
          m_bootstrapConfiguration(bootstrapConfiguration),
          m_algorithmsConfiguration(algorithmsConfiguration)
      {}

      /**
       * @brief Run all configured bootstrap engines on @p returns and select the best CI.
       *
       * @param returns  Return series (typically high-res mark-to-market returns).
       * @param os       Optional logging stream. If non-null, engine failures are logged.
       *
       * @return AutoCIResult<Decimal> encapsulating the chosen method and all candidates.
       *
       * @throws std::runtime_error if no engine produced a usable candidate.
       */
      Result run(const std::vector<Decimal>& returns, std::ostream* os = nullptr)
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
	Resampler resampler(blockSize);

	// 1) Normal bootstrap
	if (m_algorithmsConfiguration.enableNormal())
	  {
	    try
	      {
		auto [engine, crn] =
		  m_factory.template makeNormal<Decimal, Sampler, Resampler, Executor>(
										       B_single,
										       cl,
										       resampler,
										       m_strategy,
										       stageTag,
										       static_cast<uint64_t>(blockSize),
										       fold);

		auto res = engine.run(returns, Sampler(), crn);
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
		  m_factory.template makeBasic<Decimal, Sampler, Resampler, Executor>(
										      B_single,
										      cl,
										      resampler,
										      m_strategy,
										      stageTag,
										      static_cast<uint64_t>(blockSize),
										      fold);

		auto res = engine.run(returns, Sampler(), crn);
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
		  m_factory.template makePercentile<Decimal, Sampler, Resampler, Executor>(
											   B_single,
											   cl,
											   resampler,
											   m_strategy,
											   stageTag,
											   static_cast<uint64_t>(blockSize),
											   fold);

		auto res = engine.run(returns, Sampler(), crn);
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
		auto [engine, crn] =
		  m_factory.template makeAdaptiveMOutOfN<Decimal, Sampler, Resampler, Executor>(
												B_single,
												cl,
												resampler,
												m_strategy,
												stageTag,
												static_cast<uint64_t>(blockSize),
												fold);

		auto res = engine.run(returns, Sampler(), crn);
		candidates.push_back(
				     Selector::template summarizePercentileLike(
										MethodId::MOutOfN, engine, res));
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
		  m_factory.template makePercentileT<Decimal, Sampler, Resampler, Executor>(
											    B_outer_percentileT,
											    B_inner_percentileT,
											    cl,
											    resampler,
											    m_strategy,
											    stageTag,
											    static_cast<uint64_t>(blockSize),
											    fold);

		auto res = engine.run(returns, Sampler(), crn);
		candidates.push_back(
				     Selector::template summarizePercentileT(engine, res));
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
		BCaResampler bcaResampler(blockSize);

		// Use the same statistic as Sampler, but expressed as a std::function.
		std::function<Decimal(const std::vector<Decimal>&)> statFn =
		  [](const std::vector<Decimal>& r) { return Sampler()(r); };

		auto bcaEngine =
		  m_factory.template makeBCa<Decimal, BCaResampler>(
								    returns,
								    static_cast<unsigned>(B_single),
								    cl,
								    statFn,
								    bcaResampler,
								    m_strategy,
								    stageTag,
								    static_cast<uint64_t>(blockSize),
								    fold);

		// BCaBootstrap computes its statistics during construction; no run() needed.
		candidates.push_back(Selector::template summarizeBCa(bcaEngine));
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

	// -------------------------------------------------------------------
        // Choose scoring weights based on whether the statistic is a "ratio"
        // (e.g., LogProfitFactorStat) or a mean-like statistic (e.g., GeoMeanStat).
        //
        // For mean-like stats:
        //   - center and skew penalties matter more, length a bit less.
        //
        // For ratio stats:
        //   - interval LENGTH and BCa stability are more important;
        //     center shift is down-weighted because ratio centers are noisy.
        // -------------------------------------------------------------------
        typename Selector::ScoringWeights weights;

        const bool isRatioStatistic = Sampler::isRatioStatistic();

        if (isRatioStatistic)
          {
            // Ratio-based statistic (e.g., profit factor / log PF)
            //   w_center  ~ 0.25
            //   w_skew    ~ 0.5
            //   w_length  ~ 0.75
            //   w_stab    ~ 1.5
            weights = typename Selector::ScoringWeights(/*wCenterShift*/ 0.25,
							/*wSkew*/        0.5,
							/*wLength*/      0.75,
							/*wStability*/   1.5,
							/*enforcePos*/   true);
          }
        else
          {
            // Mean-like statistic (e.g., geometric mean of returns)
            //   w_center  ~ 1.0
            //   w_skew    ~ 0.5
            //   w_length  ~ 0.25
            //   w_stab    ~ 1.0
            weights = typename Selector::ScoringWeights(
                           /*wCenterShift*/ 1.0,
                           /*wSkew*/        0.5,
                           /*wLength*/      0.25,
                           /*wStability*/   1.0);
          }

        // Let AutoBootstrapSelector perform the selection using the chosen weights.
        Result result = Selector::select(candidates, weights);

	if (os)
	  {
	    const auto& diagnostics = result.getDiagnostics();
	    const auto& chosen      = result.getChosenCandidate();

	    // 1) Summary line for the selected method
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

	    // 2) High-level diagnostics for the chosen method / BCa status
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

	    // 3) Full candidate dump for detailed debugging / analysis
	    const auto& allCandidates = result.getCandidates();

	    (*os) << "   [AutoCI] Candidate diagnostics (one line per method):\n";
	    for (const auto& c : allCandidates)
	      {
		const char* mName = Result::methodIdToString(c.getMethod());

		(*os) << "      [AutoCI-Candidate] "
		      << "method="               << mName
		      << "  mean="               << c.getMean()
		      << "  LB="                 << c.getLower()
		      << "  UB="                 << c.getUpper()
		      << "  n="                  << c.getN()
		      << "  B_outer="            << c.getBOuter()
		      << "  B_inner="            << c.getBInner()
		      << "  B_eff="              << c.getEffectiveB()
		      << "  skipped="            << c.getSkippedTotal()
		      << "  se_boot="            << c.getSeBoot()
		      << "  skew_boot="          << c.getSkewBoot()
		      << "  center_shift_in_se=" << c.getCenterShiftInSe()
		      << "  normalized_length="  << c.getNormalizedLength()
		      << "  ordering_penalty="   << c.getOrderingPenalty()
		      << "  length_penalty="     << c.getLengthPenalty()
		      << "  stability_penalty="  << c.getStabilityPenalty()
		      << "  score="              << c.getScore()
		      << "  z0="                 << c.getZ0()
		      << "  a="                  << c.getAccel()
		      << "\n";
	      }
	  }

	return result;
      }
      
    private:
      Factory& m_factory;
      const mkc_timeseries::BacktesterStrategy<Decimal>& m_strategy;
      BootstrapConfiguration             m_bootstrapConfiguration;
      BootstrapAlgorithmsConfiguration   m_algorithmsConfiguration;
    };
  } // namespace analysis
} // namespace palvalidator
