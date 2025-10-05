#pragma once

#include "number.h"
#include "utils/ValidationTypes.h"
#include "analysis/StatisticalTypes.h"
#include "DateRange.h"
#include "TimeFrame.h"
#include <memory>
#include <vector>
#include <optional>
#include <string>

// Forward declarations for mkc_timeseries types to avoid heavy includes
namespace mkc_timeseries {
  template <typename Decimal> class BackTester;
  template <typename Decimal> class Portfolio;
  template <typename Decimal> class Security;
  template <typename Decimal> class PalStrategy;
}

namespace palvalidator
{
  namespace filtering
  {
    // Explicitly import the specific mkc_timeseries types we use here.
    // This keeps names short (Security, Portfolio, BackTester, PalStrategy)
    // without polluting the global namespace.
    using mkc_timeseries::BackTester;
    using mkc_timeseries::Portfolio;
    using mkc_timeseries::Security;
    using mkc_timeseries::PalStrategy;
    using Num = num::DefaultNumber;
    using RiskParameters = palvalidator::utils::RiskParameters;
    using RobustnessChecksConfig = palvalidator::analysis::RobustnessChecksConfig<Num>;
    using FragileEdgePolicy = palvalidator::analysis::FragileEdgePolicy;

    template <typename NumT>
    struct OOSSpreadStatsT
    {
      NumT mean;  // proportional, e.g. 0.008 = 0.8%
      NumT qn;    // robust Qn scale in same units
    };

    // Project-wide alias for your Num
    using OOSSpreadStats = OOSSpreadStatsT<Num>;

    /**
     * @brief Summary statistics for performance filtering results
     */
    class FilteringSummary
    {
    public:
      /**
       * @brief Default constructor initializing all counters to zero
       */
      FilteringSummary();

      /**
       * @brief Get the number of strategies with insufficient sample size
       * @return Number of strategies filtered due to insufficient returns
       */
      size_t getInsufficientCount() const
      {
        return mInsufficientCount;
      }

      /**
       * @brief Get the number of strategies flagged for divergence
       * @return Number of strategies flagged for AM vs GM divergence
       */
      size_t getFlaggedCount() const
      {
        return mFlaggedCount;
      }

      /**
       * @brief Get the number of flagged strategies that passed robustness checks
       * @return Number of flagged strategies that passed robustness
       */
      size_t getFlagPassCount() const
      {
        return mFlagPassCount;
      }

      /**
       * @brief Get the number of strategies that failed L-bound checks
       * @return Number of strategies that failed L-sensitivity bound checks
       */
      size_t getFailLBoundCount() const
      {
        return mFailLBoundCount;
      }

      /**
       * @brief Get the number of strategies that failed L-variability checks
       * @return Number of strategies that failed L-sensitivity variability checks
       */
      size_t getFailLVarCount() const
      {
        return mFailLVarCount;
      }

      /**
       * @brief Get the number of strategies that failed split-sample checks
       * @return Number of strategies that failed split-sample tests
       */
      size_t getFailSplitCount() const
      {
        return mFailSplitCount;
      }

      /**
       * @brief Get the number of strategies that failed tail-risk checks
       * @return Number of strategies that failed tail-risk tests
       */
      size_t getFailTailCount() const
      {
        return mFailTailCount;
      }

      /**
       * @brief Increment the insufficient sample count
       */
      void incrementInsufficientCount()
      {
        ++mInsufficientCount;
      }

      /**
       * @brief Increment the flagged count
       */
      void incrementFlaggedCount()
      {
        ++mFlaggedCount;
      }

      /**
       * @brief Increment the flag pass count
       */
      void incrementFlagPassCount()
      {
        ++mFlagPassCount;
      }

      /**
       * @brief Increment the L-bound failure count
       */
      void incrementFailLBoundCount()
      {
        ++mFailLBoundCount;
      }

      /**
       * @brief Increment the L-variability failure count
       */
      void incrementFailLVarCount()
      {
        ++mFailLVarCount;
      }

      /**
       * @brief Increment the split-sample failure count
       */
      void incrementFailSplitCount()
      {
        ++mFailSplitCount;
      }

      /**
       * @brief Increment the tail-risk failure count
       */
      void incrementFailTailCount()
      {
        ++mFailTailCount;
      }

      /**
       * @brief Get the number of strategies that failed regime-mix checks
       * @return Number of strategies that failed regime-mix tests
       */
      size_t getFailRegimeMixCount() const
      {
        return mFailRegimeMixCount;
      }

      // ... existing incrementers ...

      /**
       * @brief Increment the regime-mix failure count
       */
      void incrementFailRegimeMixCount()
      {
        ++mFailRegimeMixCount;
      }

    private:
      size_t mInsufficientCount;  ///< Number of strategies with insufficient sample size
      size_t mFlaggedCount;       ///< Number of strategies flagged for divergence
      size_t mFlagPassCount;      ///< Number of flagged strategies that passed robustness
      size_t mFailLBoundCount;    ///< Number of strategies that failed L-bound checks
      size_t mFailLVarCount;      ///< Number of strategies that failed L-variability checks
      size_t mFailSplitCount;     ///< Number of strategies that failed split-sample checks
      size_t mFailTailCount;      ///< Number of strategies that failed tail-risk check
      size_t mFailRegimeMixCount;
    };

    // -------------------------------------------------------------------------
    // Additional refactoring types (Phase 1)
    // These types are added to the existing FilteringTypes.h as part of the
    // PerformanceFilter refactor design. They are deliberately lightweight and
    // reuse existing project types via includes above.
    // -------------------------------------------------------------------------

    /**
     * @brief Context carrying inputs and intermediate state for per-strategy analysis
     *
     * Note: stores shared_ptr references to existing project types to avoid
     * unnecessary copies and preserve ownership semantics used elsewhere.
     */
    struct StrategyAnalysisContext
    {
      std::shared_ptr<PalStrategy<Num>> strategy;
      std::shared_ptr<Security<Num>> baseSecurity;
      mkc_timeseries::DateRange inSampleDates;
      mkc_timeseries::DateRange oosDates;
      mkc_timeseries::TimeFrame::Duration timeFrame;
      std::optional<OOSSpreadStats> oosSpreadStats;

      // Populated during analysis
      std::shared_ptr<Portfolio<Num>> portfolio;
      std::shared_ptr<PalStrategy<Num>> clonedStrategy;
      std::shared_ptr<BackTester<Num>> backtester;
      std::vector<Num> highResReturns;

      // Analysis parameters / outputs
      size_t blockLength{0};
      double annualizationFactor{0.0};
      Num finalRequiredReturn;

      StrategyAnalysisContext(
        std::shared_ptr<PalStrategy<Num>> strat,
        std::shared_ptr<Security<Num>> sec,
        const mkc_timeseries::DateRange& inSample,
        const mkc_timeseries::DateRange& oos,
        mkc_timeseries::TimeFrame::Duration tf,
        std::optional<OOSSpreadStats> spread = std::nullopt)
        : strategy(std::move(strat))
        , baseSecurity(std::move(sec))
        , inSampleDates(inSample)
        , oosDates(oos)
        , timeFrame(tf)
        , oosSpreadStats(spread)
      {}
    };

    /**
     * @brief Results produced by the BCa bootstrap + annualization step
     */
    struct BootstrapAnalysisResult
    {
      Num lbGeoPeriod;                 // per-period geometric mean lower bound
      Num lbMeanPeriod;                // per-period arithmetic mean lower bound
      Num annualizedLowerBoundGeo;     // annualized GM lower bound
      Num annualizedLowerBoundMean;    // annualized arithmetic mean lower bound
      unsigned int medianHoldBars{0};
      size_t blockLength{0};

      bool isValid() const
      {
        return (annualizedLowerBoundGeo > Num(0)) || (annualizedLowerBoundMean > Num(0));
      }
    };

    /**
     * @brief Hurdle calculation outputs and pass/fail flags
     */
    struct HurdleAnalysisResult
    {
      // Minimal hurdle result needed by pipeline stages.
      // We avoid including CostStressUtils.h here to prevent circular includes;
      // stages that need the full CostStressHurdles should call makeCostStressHurdles()
      // and keep the full struct locally.
      Num annualizedTrades;
      Num finalRequiredReturn;
      bool passedBase{false};
      bool passed1Qn{false};

      bool passed() const { return passedBase && passed1Qn; }
    };

    /**
     * @brief Canonical filter decision returned by stages and pipeline
     */
    enum class FilterDecisionType
    {
      Pass,
      FailInsufficientData,
      FailHurdle,
      FailRobustness,
      FailLSensitivity,
      FailRegimeMix,
      FailFragileEdge
    };

    struct FilterDecision
    {
      FilterDecisionType decision{FilterDecisionType::Pass};
      std::string rationale;

      static FilterDecision Pass(const std::string& reason = "")
      {
        return FilterDecision{FilterDecisionType::Pass, reason};
      }

      static FilterDecision Fail(FilterDecisionType type, const std::string& reason = "")
      {
        return FilterDecision{type, reason};
      }

      bool passed() const { return decision == FilterDecisionType::Pass; }
    };

  } // namespace filtering
} // namespace palvalidator
