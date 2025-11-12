#pragma once

#include <vector>
#include <ostream>
#include <string>
#include "StatisticalTypes.h"
#include "number.h"
#include "filtering/BootstrapConfig.h"

namespace mkc_timeseries {
    template<class Decimal> class BacktesterStrategy;
}

namespace palvalidator
{
namespace analysis
{

/**
 * @brief Analyzer for strategy robustness using various statistical tests
 * 
 * This class provides comprehensive robustness analysis for trading strategies,
 * including L-sensitivity testing, split-sample validation, and tail-risk assessment.
 * All tests are based on geometric mean (GM) bootstrap confidence intervals.
 */
class RobustnessAnalyzer
{
public:
    using Num = num::DefaultNumber;
    using BootstrapFactory = palvalidator::bootstrap_cfg::BootstrapFactory;

    /**
     * @brief Run comprehensive robustness checks on a strategy
     *
     * This function performs GM-only robustness checks for strategies flagged by AM–GM divergence.
     * It includes L-sensitivity testing, split-sample stability checks, and tail-risk assessment.
     *
     * @param label Strategy name for logging
     * @param returns Per-period net returns after costs/slippage
     * @param L_in Block length for the block sampler (median holding period)
     * @param annualizationFactor Multiplier to annualize per-period LBs
     * @param finalRequiredReturn Annualized hurdle used in the main filter
     * @param cfg Configuration parameters for the robustness checks
     * @param strategy Strategy object for bootstrap factory (must not be null)
     * @param bootstrapFactory Factory for creating bootstrap instances
     * @param os Output stream for diagnostic logging
     * @return RobustnessResult indicating pass/fail and reason
     */
    static RobustnessResult runFlaggedStrategyRobustness(
        const std::string& label,
        const std::vector<Num>& returns,
        size_t L_in,
        double annualizationFactor,
        const Num& finalRequiredReturn,
        const RobustnessChecksConfig<Num>& cfg,
        const mkc_timeseries::BacktesterStrategy<Num>& strategy,
        BootstrapFactory& bootstrapFactory,
        std::ostream& os);

private:
  // ========== Helper Classes ==========
  
  /**
   * @brief Results from baseline bootstrap analysis
   */
  class BaselineAnalysis
  {
  public:
    BaselineAnalysis(const Num& perPeriodLB, const Num& annualizedLB, size_t effectiveL, bool isSmallN)
      : mPerPeriodLB(perPeriodLB)
      , mAnnualizedLB(annualizedLB)
      , mEffectiveL(effectiveL)
      , mIsSmallN(isSmallN)
    {
    }
    
    const Num& getPerPeriodLB() const { return mPerPeriodLB; }
    const Num& getAnnualizedLB() const { return mAnnualizedLB; }
    size_t getEffectiveL() const { return mEffectiveL; }
    bool isSmallN() const { return mIsSmallN; }
    
  private:
    Num mPerPeriodLB;
    Num mAnnualizedLB;
    size_t mEffectiveL;
    bool mIsSmallN;
  };
  
  /**
   * @brief Results from L-sensitivity sweep analysis
   */
  class LSensitivityAnalysis
  {
  public:
    LSensitivityAnalysis(const Num& minAnnualized, const Num& maxAnnualized,
                         double relativeVariability, bool anyFailure)
      : mMinAnnualized(minAnnualized)
      , mMaxAnnualized(maxAnnualized)
      , mRelativeVariability(relativeVariability)
      , mAnyFailure(anyFailure)
    {
    }
    
    const Num& getMinAnnualized() const { return mMinAnnualized; }
    const Num& getMaxAnnualized() const { return mMaxAnnualized; }
    double getRelativeVariability() const { return mRelativeVariability; }
    bool hasAnyFailure() const { return mAnyFailure; }
    
  private:
    Num mMinAnnualized;
    Num mMaxAnnualized;
    double mRelativeVariability;
    bool mAnyFailure;
  };
  
  /**
   * @brief Results from split-sample validation
   */
  class SplitSampleAnalysis
  {
  public:
    SplitSampleAnalysis(bool passed, const Num& half1LB, const Num& half2LB)
      : mPassed(passed)
      , mHalf1LB(half1LB)
      , mHalf2LB(half2LB)
    {
    }
    
    bool passed() const { return mPassed; }
    const Num& getHalf1LB() const { return mHalf1LB; }
    const Num& getHalf2LB() const { return mHalf2LB; }
    
  private:
    bool mPassed;
    Num mHalf1LB;
    Num mHalf2LB;
  };
  
  /**
   * @brief Tail statistics for risk assessment
   */
  class TailRiskAnalysis
  {
  public:
    TailRiskAnalysis(const Num& quantile, const Num& expectedShortfall,
                     bool isSevere, bool isBorderline)
      : mQuantile(quantile)
      , mExpectedShortfall(expectedShortfall)
      , mIsSevere(isSevere)
      , mIsBorderline(isBorderline)
    {
    }
    
    const Num& getQuantile() const { return mQuantile; }
    const Num& getExpectedShortfall() const { return mExpectedShortfall; }
    bool isSevere() const { return mIsSevere; }
    bool isBorderline() const { return mIsBorderline; }
    bool shouldFail() const { return mIsSevere && mIsBorderline; }
    
  private:
    Num mQuantile;
    Num mExpectedShortfall;
    bool mIsSevere;
    bool mIsBorderline;
  };

  // ========== Analysis Methods ==========
  
  static BaselineAnalysis computeBaselineAnalysis_(
      const std::vector<Num>& returns,
      size_t L_eff,
      double annualizationFactor,
      const RobustnessChecksConfig<Num>& cfg,
      const mkc_timeseries::BacktesterStrategy<Num>& strategy,
      BootstrapFactory& bootstrapFactory,
      std::ostream& os);
  
  static LSensitivityAnalysis performLSensitivityAnalysis_(
      const std::vector<Num>& returns,
      size_t L_baseline,
      double annualizationFactor,
      const Num& lbAnnual_base,
      const RobustnessChecksConfig<Num>& cfg,
      const mkc_timeseries::BacktesterStrategy<Num>& strategy,
      BootstrapFactory& bootstrapFactory,
      std::ostream& os);
  
  static SplitSampleAnalysis performSplitSampleAnalysis_(
      const std::vector<Num>& returns,
      double annualizationFactor,
      const Num& finalRequiredReturn,
      const RobustnessChecksConfig<Num>& cfg,
      const mkc_timeseries::BacktesterStrategy<Num>& strategy,
      BootstrapFactory& bootstrapFactory,
      std::ostream& os);
  
  static TailRiskAnalysis performTailRiskAnalysis_(
      const std::vector<Num>& returns,
      const Num& lbPeriod_base,
      const Num& lbAnnual_base,
      const Num& finalRequiredReturn,
      const RobustnessChecksConfig<Num>& cfg,
      std::ostream& os);

  // ========== Utility Structures ==========
  
  struct TailStats
  {
    Num q_alpha;   // type-7 quantile at alpha
    Num es_alpha;  // fractional Expected Shortfall at alpha
  };

  struct HurdleCloseness
  {
    bool near;
    double distAbs;  // baseA - hurA
    double distRel;  // (baseA - hurA)/max(|hurA|, tiny)
  };

  struct LSweepResult
  {
    Num ann_min, ann_max;
    double relVar;
    bool anyFail;
  };

  // ========== Utility Methods ==========
  
  static TailStats computeTailStatsType7_(const std::vector<Num>& sortedAsc, double alpha);
  static Num toLog1p_(const Num& r);
  static void toLog1pVector_(const std::vector<Num>& in, std::vector<Num>& out);
  static size_t clampBlockLen_(size_t Ltry, size_t n, size_t minL);
  static size_t suggestHalfLfromACF_(const std::vector<Num>& rHalf,
                                     size_t minL,
                                     size_t hardMaxL);
  static size_t adjustBforHalf_(size_t B, size_t nHalf);
  static Num safeAnnualizeLB_(const Num& perPeriodLB, double k, double eps = 1e-12);
  static HurdleCloseness nearHurdle_(const Num& lbAnnual_base,
                                     const Num& finalRequiredReturn,
                                     const RobustnessChecksConfig<Num>& cfg);
  static LSweepResult runLSensitivityWithCache_(
      const std::vector<Num>& returns,
      size_t L_baseline,
      double annualizationFactor,
      const Num& lbAnnual_base,
      const RobustnessChecksConfig<Num>& cfg,
      const mkc_timeseries::BacktesterStrategy<Num>& strategy,
      BootstrapFactory& bootstrapFactory,
      std::ostream& os);
  static double effectiveTailAlpha_(size_t n, double alpha);

  /**
     * @brief Annualize a per-period lower bound
     * @param perPeriodLB Per-period lower bound
     * @param k Annualization factor
     * @return Annualized lower bound
     */
    static Num annualizeLB_(const Num& perPeriodLB, double k);

  

    /**
     * @brief Get absolute value of a number
     * @param x Input number
     * @return Absolute value
     */
    static Num absNum_(const Num& x);

    /**
     * @brief Log tail risk explanation to output stream
     * @param os Output stream
     * @param perPeriodGMLB Per-period geometric mean lower bound
     * @param q05 5% quantile
     * @param es05 Expected shortfall at 5%
     * @param severeMultiple Threshold multiple for severe classification
     */
    static void logTailRiskExplanation(
        std::ostream& os,
        const Num& perPeriodGMLB,
        const Num& q05,
        const Num& es05,
        double severeMultiple);
};

} // namespace analysis
} // namespace palvalidator
