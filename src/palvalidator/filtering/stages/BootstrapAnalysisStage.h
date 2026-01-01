#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/BootstrapConfig.h"
#include "TradingBootstrapFactory.h"
#include <ostream>
#include "AutoBootstrapSelector.h"
#include "diagnostics/IBootstrapObserver.h"
#include "diagnostics/IBootstrapObserver.h"

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  using palvalidator::bootstrap_cfg::BootstrapFactory;

  /**
   * @brief Stage tag and fold constants for Common Random Number (CRN) stream differentiation.
   *
   * These constants are used as stageTag and fold parameters when creating bootstrap engines
   * to ensure that different statistical metrics and cross-validation folds draw from
   * independent random number streams. This is critical for proper bootstrap variance
   * estimation - each metric should have its own random stream to avoid artificial
   * correlation in the results.
   *
   * The hierarchical CRN key structure is:
   *   masterSeed -> strategyId -> stageTag -> blockLength -> fold -> replicate
   *
   * Stage tags differentiate between different statistical metrics (mean, geometric mean,
   * profit factor, etc.). Fold values differentiate between cross-validation folds or
   * other analysis subdivisions within the same metric.
   *
   * @see TradingBootstrapFactory::makeCRNKey()
   * @see mkc_timeseries::rng_utils::CRNKey
   */
  namespace BootstrapStages
  {
    /// BCa bootstrap for arithmetic mean (used in runBCaMeanBootstrap)
    constexpr std::uint64_t BCA_MEAN = 0;
    
    /// Auto-selection bootstrap for geometric mean / CAGR (used in runAutoGeoBootstrap)
    constexpr std::uint64_t GEO_MEAN = 1;
    
    /// Auto-selection bootstrap for profit factor (used in runAutoProfitFactorBootstrap)
    constexpr std::uint64_t PROFIT_FACTOR = 2;

    // ========== Fold Values (Cross-Validation or Analysis Subdivision) ==========
    
    /// No cross-validation; single full-sample bootstrap analysis
    constexpr std::uint64_t NO_FOLD = 0;
    
    /// First fold in cross-validation (use NO_FOLD + 1, NO_FOLD + 2, etc. for additional folds)
    constexpr std::uint64_t FOLD_1 = 1;
  }
  
  class BootstrapAnalysisStage
  {
  public:
    BootstrapAnalysisStage(const Num& confidenceLevel,
	   unsigned int numResamples,
	   BootstrapFactory& bootstrapFactory);

    void reportDiagnostics(const StrategyAnalysisContext& ctx,
                           palvalidator::diagnostics::MetricType metricType,
                           const palvalidator::analysis::AutoCIResult<Num>& result) const;

    void setObserver(std::shared_ptr<palvalidator::diagnostics::IBootstrapObserver> observer) { mObserver = observer; }

    /**
     * Compute BCa bootstrap bounds and annualize results.
     * Populates and returns BootstrapAnalysisResult.
     */
    BootstrapAnalysisResult execute(StrategyAnalysisContext& ctx, std::ostream& os) const;

    /**
     * Compute block length based on median holding period.
     * Made public for pipeline coordinator access.
     */
    size_t computeBlockLength(const StrategyAnalysisContext& ctx, std::ostream& os) const;

    /**
     * Compute annualization factor based on timeframe.
     * Made public for pipeline coordinator access.
     */
    double computeAnnualizationFactor(const StrategyAnalysisContext& ctx) const;

  private:
    
    // Helper classes for organizing bootstrap results
    class DistributionDiagnostics
    {
    public:
      DistributionDiagnostics(double skew, double excessKurtosis, bool heavyTails,
                              bool runSmallN, bool runPercentileT, bool runBCaGeo)
        : mSkew(skew)
        , mExcessKurtosis(excessKurtosis)
        , mHeavyTails(heavyTails)
        , mRunSmallN(runSmallN)
        , mRunPercentileT(runPercentileT)
        , mRunBCaGeo(runBCaGeo)
      {}

      double getSkew() const { return mSkew; }
      double getExcessKurtosis() const { return mExcessKurtosis; }
      bool hasHeavyTails() const { return mHeavyTails; }
      bool shouldRunSmallN() const { return mRunSmallN; }
      bool shouldRunPercentileT() const { return mRunPercentileT; }
      bool shouldRunBCaGeo() const { return mRunBCaGeo; }

    private:
      double mSkew;
      double mExcessKurtosis;
      bool mHeavyTails;
      bool mRunSmallN;
      bool mRunPercentileT;
      bool mRunBCaGeo;
    };

    class SmallNResult
    {
    public:
      SmallNResult(const Num& lowerBoundPeriod, const Num& lowerBoundAnnualized,
                   const std::string& resamplerName, std::size_t mSub,
                   std::size_t lUsed, std::size_t effectiveB)
        : mLowerBoundPeriod(lowerBoundPeriod)
        , mLowerBoundAnnualized(lowerBoundAnnualized)
        , mResamplerName(resamplerName)
        , mMSub(mSub)
        , mLUsed(lUsed)
        , mEffectiveB(effectiveB)
        , mDuelRatio(std::numeric_limits<double>::quiet_NaN())
        , mDuelRatioValid(false)
      {}

      SmallNResult(const Num& lowerBoundPeriod, const Num& lowerBoundAnnualized,
                   const std::string& resamplerName, std::size_t mSub,
                   std::size_t lUsed, std::size_t effectiveB,
                   double duelRatio, bool duelRatioValid)
        : mLowerBoundPeriod(lowerBoundPeriod)
        , mLowerBoundAnnualized(lowerBoundAnnualized)
        , mResamplerName(resamplerName)
        , mMSub(mSub)
        , mLUsed(lUsed)
        , mEffectiveB(effectiveB)
        , mDuelRatio(duelRatio)
        , mDuelRatioValid(duelRatioValid)
      {}

      const Num& getLowerBoundPeriod() const { return mLowerBoundPeriod; }
      const Num& getLowerBoundAnnualized() const { return mLowerBoundAnnualized; }
      const std::string& getResamplerName() const { return mResamplerName; }
      std::size_t getMSub() const { return mMSub; }
      std::size_t getLUsed() const { return mLUsed; }
      std::size_t getEffectiveB() const { return mEffectiveB; }
      bool hasDuelRatio() const { return mDuelRatioValid; }
      double getDuelRatio() const { return mDuelRatio; }

    private:
      Num mLowerBoundPeriod;
      Num mLowerBoundAnnualized;
      std::string mResamplerName;
      std::size_t mMSub;
      std::size_t mLUsed;
      std::size_t mEffectiveB;
      double mDuelRatio;
      bool mDuelRatioValid;
    };

    class PercentileTResult
    {
    public:
      PercentileTResult(const Num& lowerBoundPeriod, const std::string& resamplerName,
                        std::size_t mOuter, std::size_t mInner,
                        std::size_t lUsed, std::size_t effectiveB)
        : mLowerBoundPeriod(lowerBoundPeriod)
        , mResamplerName(resamplerName)
        , mMOuter(mOuter)
        , mMInner(mInner)
        , mLUsed(lUsed)
        , mEffectiveB(effectiveB)
      {}

      const Num& getLowerBoundPeriod() const { return mLowerBoundPeriod; }
      const std::string& getResamplerName() const { return mResamplerName; }
      std::size_t getMOuter() const { return mMOuter; }
      std::size_t getMInner() const { return mMInner; }
      std::size_t getLUsed() const { return mLUsed; }
      std::size_t getEffectiveB() const { return mEffectiveB; }

    private:
      Num mLowerBoundPeriod;
      std::string mResamplerName;
      std::size_t mMOuter;
      std::size_t mMInner;
      std::size_t mLUsed;
      std::size_t mEffectiveB;
    };

    class BCaMeanResult
    {
    public:
      BCaMeanResult(const Num& lowerBoundPeriod, const Num& lowerBoundAnnualized)
        : mLowerBoundPeriod(lowerBoundPeriod)
        , mLowerBoundAnnualized(lowerBoundAnnualized)
      {}

      const Num& getLowerBoundPeriod() const { return mLowerBoundPeriod; }
      const Num& getLowerBoundAnnualized() const { return mLowerBoundAnnualized; }

    private:
      Num mLowerBoundPeriod;
      Num mLowerBoundAnnualized;
    };

    class BCaGeoResult
    {
    public:
      BCaGeoResult(const Num& lowerBoundPeriod)
        : mLowerBoundPeriod(lowerBoundPeriod)
      {}

      const Num& getLowerBoundPeriod() const { return mLowerBoundPeriod; }

    private:
      Num mLowerBoundPeriod;
    };

    class BCaPFResult
    {
    public:
      explicit BCaPFResult(const Num& lowerBoundPeriod)
        : mLowerBoundPeriod(lowerBoundPeriod)
      {}

      const Num& getLowerBoundPeriod() const { return mLowerBoundPeriod; }

    private:
      Num mLowerBoundPeriod;
    };

    // Core computation methods
    /**
     * Initialize backtester if not already present in context
     */
    bool initializeBacktester(StrategyAnalysisContext& ctx, std::ostream& os) const;

    /**
     * Validate that context has sufficient data for bootstrap analysis
     */
    bool validateContext(const StrategyAnalysisContext& ctx,
                         BootstrapAnalysisResult& result,
                         std::ostream& os) const;

    /**
     * Compute annualization parameters (block length, bars/year, lambda)
     */
    struct AnnualizationParams
    {
      size_t blockLength;
      unsigned int medianHoldBars;
      double barsPerYear;
      double lambdaTradesPerYear;
      double baseAnnFactor;
    };
    
    AnnualizationParams computeAnnualizationParams(const StrategyAnalysisContext& ctx,
                                                    std::ostream& os) const;

    /**
     * Execute profit factor bootstrap analysis
     */
    void executeProfitFactorBootstrap(const StrategyAnalysisContext& ctx,
                                       const DistributionDiagnostics& diagnostics,
                                       double confidenceLevel,
                                       size_t blockLength,
                                       BootstrapAnalysisResult& result,
                                       std::ostream& os) const;

    /**
     * Assemble final bootstrap results from individual components
     */
    void assembleFinalResults(const std::optional<SmallNResult>& smallNResult,
                              const std::optional<PercentileTResult>& percentileTResult,
                              const std::optional<BCaGeoResult>& bcaGeoResult,
                              const BCaMeanResult& bcaMeanResult,
                              const AnnualizationParams& annParams,
                              BootstrapAnalysisResult& result,
                              std::ostream& os) const;

    /**
     * Analyze distribution characteristics and determine which bootstrap methods to run
     */
    DistributionDiagnostics analyzeDistribution(const StrategyAnalysisContext& ctx,
                                                 std::ostream& os) const;

    /**
     * Run small-N conservative bootstrap (min of m/n and BCa)
     */
    std::optional<SmallNResult> runSmallNBootstrap(const StrategyAnalysisContext& ctx,
                                                    double confidenceLevel,
                                                    double annualizationFactor,
                                                    size_t blockLength,
                                                    std::ostream& os) const;

    std::optional<BootstrapAnalysisStage::SmallNResult>
    runSmallNProfitFactorBootstrap(const std::vector<Num>& highResReturns,
				   const std::shared_ptr<PalStrategy<Num>>& clonedStrategy,
				   double confidenceLevel,
				   std::size_t blockLength,
				   std::ostream& os) const;

    BCaPFResult
    runBCaProfitFactorBootstrap(const std::vector<Num>& highResReturns,
				const std::shared_ptr<PalStrategy<Num>>& clonedStrategy,
				double confidenceLevel,
				size_t blockLength,
				std::ostream& os) const;


    /**
     * Run percentile-t bootstrap
     */
    std::optional<PercentileTResult> runPercentileTBootstrap(const StrategyAnalysisContext& ctx,
                                                              double confidenceLevel,
                                                              size_t blockLength,
                                                              std::ostream& os) const;

    /**
     * Run BCa bootstrap for mean statistic
     */
    BCaMeanResult runBCaMeanBootstrap(const StrategyAnalysisContext& ctx,
                                      double confidenceLevel,
                                      double annualizationFactor,
                                      size_t blockLength,
                                      std::ostream& os) const;

    /**
     * Run BCa bootstrap for geometric mean statistic
     */
    BCaGeoResult runBCaGeoBootstrap(const StrategyAnalysisContext& ctx,
                                    double confidenceLevel,
                                    size_t blockLength,
                                    std::ostream& os) const;

    Num runAutoGeoBootstrap(const StrategyAnalysisContext& ctx,
                          double confidenceLevel,
                          std::size_t blockLength,
                          BootstrapAnalysisResult& result,
                          std::ostream& os) const;

    std::optional<Num> runAutoProfitFactorBootstrap(const StrategyAnalysisContext& ctx,
                                                  double confidenceLevel,
                                                  std::size_t blockLength,
                                                  BootstrapAnalysisResult& result,
                                                  std::ostream& os) const;

    /**
     * Combine geometric lower bounds from multiple bootstrap methods
     */
    Num combineGeometricLowerBounds(const std::optional<SmallNResult>& smallN,
                                     const std::optional<PercentileTResult>& percentileT,
                                     const std::optional<BCaGeoResult>& bcaGeo,
                                     std::ostream& os) const;

    /**
     * Log the final policy selection and parameters
     */
    void logFinalPolicy(const std::optional<SmallNResult>& smallN,
                        const std::optional<PercentileTResult>& percentileT,
			 const std::optional<BCaGeoResult>& bcaGeo,
                        size_t n,
                        size_t blockLength,
                        double skew,
                        double excessKurtosis,
                        bool heavyTails,
                        std::ostream& os) const;

    Num mConfidenceLevel;
    unsigned int mNumResamples;
    BootstrapFactory& mBootstrapFactory;
    std::shared_ptr<palvalidator::diagnostics::IBootstrapObserver> mObserver;
  };

} // namespace palvalidator::filtering::stages
