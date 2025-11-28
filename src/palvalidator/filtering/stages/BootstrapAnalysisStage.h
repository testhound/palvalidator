#pragma once

#include "filtering/FilteringTypes.h"
#include "filtering/BootstrapConfig.h"
#include "TradingBootstrapFactory.h"
#include <ostream>

namespace palvalidator::filtering::stages
{
  using namespace palvalidator::filtering;

  using palvalidator::bootstrap_cfg::BootstrapFactory;

  class BootstrapAnalysisStage
  {
  public:
    BootstrapAnalysisStage(const Num& confidenceLevel,
  	   unsigned int numResamples,
  	   BootstrapFactory& bootstrapFactory);

    /**
     * Compute BCa bootstrap bounds and annualize results.
     * Populates and returns BootstrapAnalysisResult.
     */
    BootstrapAnalysisResult execute(StrategyAnalysisContext& ctx, std::ostream& os) const;

    /**
     * Compute block length based on median holding period.
     * Made public for pipeline coordinator access.
     */
    size_t computeBlockLength(const StrategyAnalysisContext& ctx) const;

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
      {}

      const Num& getLowerBoundPeriod() const { return mLowerBoundPeriod; }
      const Num& getLowerBoundAnnualized() const { return mLowerBoundAnnualized; }
      const std::string& getResamplerName() const { return mResamplerName; }
      std::size_t getMSub() const { return mMSub; }
      std::size_t getLUsed() const { return mLUsed; }
      std::size_t getEffectiveB() const { return mEffectiveB; }

    private:
      Num mLowerBoundPeriod;
      Num mLowerBoundAnnualized;
      std::string mResamplerName;
      std::size_t mMSub;
      std::size_t mLUsed;
      std::size_t mEffectiveB;
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

    // Core computation methods
    /**
     * Initialize backtester if not already present in context
     */
    bool initializeBacktester(StrategyAnalysisContext& ctx, std::ostream& os) const;

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
  };

} // namespace palvalidator::filtering::stages
