#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>

#include "ClosedPositionHistory.h"
#include "MetaExitAnalytics.h"
#include "MetaExitCalibrator.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{

  enum class TuningObjective
    {
      AvgPnL_R,
      HitRate
    };

  template<class Decimal>
  class ExitTunerOptions
  {
  public:
    ExitTunerOptions(int    maxBarsToAnalyze,
		     double trainFraction         = 0.70,
		     int    embargoTrades         = 0,
		     const Decimal& thresholdR    = DecimalConstants<Decimal>::DecimalZero,
		     const Decimal& epsilonR      = DecimalConstants<Decimal>::DecimalZero,
		     double fracNonPosHigh        = 0.65,
		     double targetHazardLow       = 0.20,
		     double alphaMfeR             = 0.33,
		     int    neighborSpan          = 1,
		     bool   useFullGridIfEmpty    = true,
		     TuningObjective objective    = TuningObjective::AvgPnL_R)
    : mMaxBarsToAnalyze(maxBarsToAnalyze)
    , mTrainFraction(trainFraction)
    , mEmbargoTrades(embargoTrades)
    , mThresholdR(thresholdR)
    , mEpsilonR(epsilonR)
    , mFracNonPosHigh(fracNonPosHigh)
    , mTargetHazardLow(targetHazardLow)
    , mAlphaMfeR(alphaMfeR)
    , mNeighborSpan(neighborSpan)
    , mUseFullGridIfEmpty(useFullGridIfEmpty)
    , mObjective(objective)
    {
    }

    // Getters (unchanged)
    int getMaxBarsToAnalyze()   const { return mMaxBarsToAnalyze; }
    double getTrainFraction()   const { return mTrainFraction; }
    int getEmbargoTrades()      const { return mEmbargoTrades; }
    const Decimal& getThresholdR() const { return mThresholdR; }
    const Decimal& getEpsilonR()   const { return mEpsilonR; }
    double getFracNonPosHigh()  const { return mFracNonPosHigh; }
    double getTargetHazardLow() const { return mTargetHazardLow; }
    double getAlphaMfeR()       const { return mAlphaMfeR; }
    int getNeighborSpan()       const { return mNeighborSpan; }
    bool getUseFullGridIfEmpty()const { return mUseFullGridIfEmpty; }
    TuningObjective getObjective() const { return mObjective; }

  private:
    int mMaxBarsToAnalyze;
    double mTrainFraction;
    int mEmbargoTrades;
    Decimal mThresholdR;
    Decimal mEpsilonR;
    double mFracNonPosHigh;
    double mTargetHazardLow;
    double mAlphaMfeR;
    int mNeighborSpan;
    bool mUseFullGridIfEmpty;
    TuningObjective mObjective;
  };

  // ===== ExitTuningReportBase (no default ctor, no setters) =====
  class ExitTuningReportBase
  {
  public:
    ExitTuningReportBase(int K, int N,
			 const PolicyResult& trainK,
			 const PolicyResult& testK,
			 const PolicyResult& trainN,
			 const PolicyResult& testN,
			 std::vector<int> Kgrid,
			 std::vector<int> Ngrid)
      : mK(K), mN(N),
	mTrainK(trainK), mTestK(testK),
	mTrainN(trainN), mTestN(testN),
	mKgrid(std::move(Kgrid)), mNgrid(std::move(Ngrid)) {}

    int getK() const { return mK; }
    int getN() const { return mN; }

    // Friendly aliases:
    int getFailureToPerformBars() const { return mK; }
    int getBreakevenActivationBars() const { return mN; }
  
    const PolicyResult& getTrainK() const { return mTrainK; }
    const PolicyResult& getTestK()  const { return mTestK; }
    const PolicyResult& getTrainN() const { return mTrainN; }
    const PolicyResult& getTestN()  const { return mTestN; }

    const std::vector<int>& getKgrid() const { return mKgrid; }
    const std::vector<int>& getNgrid() const { return mNgrid; }

  private:
    int mK, mN;
    PolicyResult mTrainK, mTestK, mTrainN, mTestN;
    std::vector<int> mKgrid, mNgrid;
  };

  template<class Decimal>
  class ExitTuningReport : public ExitTuningReportBase
  {
  public:
    ExitTuningReport(int K, int N,
		     const PolicyResult& trainK,
		     const PolicyResult& testK,
		     const PolicyResult& trainN,
		     const PolicyResult& testN,
		     std::vector<int> Kgrid,
		     std::vector<int> Ngrid)
      : ExitTuningReportBase(K, N, trainK, testK, trainN, testN, std::move(Kgrid), std::move(Ngrid)) {}
  };

  // ===== ExitPolicyAutoTuner =====
  template<class Decimal>
  class ExitPolicyAutoTuner
  {
  public:
    ExitPolicyAutoTuner(const ClosedPositionHistory<Decimal>& cph,
			const ExitTunerOptions<Decimal>& opts)
      : mClosedPositionHistory(cph), mOpts(opts) {}

    ExitPolicyAutoTuner(const ClosedPositionHistory<Decimal>& cph,
			int maxBarsToAnalyze)
      : mClosedPositionHistory(cph),
	mOpts(ExitTunerOptions<Decimal>(maxBarsToAnalyze))
    {}

    ExitTuningReport<Decimal> tune();

  private:
    void splitTrainTest(ClosedPositionHistory<Decimal>& train,
			ClosedPositionHistory<Decimal>& test) const;

    std::vector<int> proposeKGrid(const std::vector<BarAgeAggregate>& aggs) const;
    std::vector<int> proposeNGrid(const std::vector<BarAgeAggregate>& aggs) const;

    int selectBestK(const ClosedPositionHistory<Decimal>& train,
		    const std::vector<int>& Kgrid,
		    PolicyResult& bestTrain) const;

    int selectBestN(const ClosedPositionHistory<Decimal>& train,
		    const std::vector<int>& Ngrid,
		    PolicyResult& bestTrain) const;

    double score(const PolicyResult& r) const;

  private:
    const ClosedPositionHistory<Decimal>& mClosedPositionHistory;
    ExitTunerOptions<Decimal> mOpts;
  };

  template<class Decimal>
  void ExitPolicyAutoTuner<Decimal>::splitTrainTest(ClosedPositionHistory<Decimal>& train,
						    ClosedPositionHistory<Decimal>& test) const
  {
    std::vector<std::shared_ptr<TradingPosition<Decimal>>> all;
    for (auto it = mClosedPositionHistory.beginTradingPositions();
	 it != mClosedPositionHistory.endTradingPositions(); ++it)
      {
	all.push_back(it->second);
      }

    const int n = static_cast<int>(all.size());
    if (n == 0)
      return;

    const int cut = std::max(0, std::min(n, static_cast<int>(std::floor(n * mOpts.getTrainFraction()))));
    const int embargo = std::max(0, std::min(mOpts.getEmbargoTrades(), n));

    for (int i = 0; i < cut; ++i)
      train.addClosedPosition(all[i]);

    for (int i = cut + embargo; i < n; ++i)
      test.addClosedPosition(all[i]);
  }

  // -------------------- Candidate grids --------------------
  template<class Decimal>
  std::vector<int> ExitPolicyAutoTuner<Decimal>::proposeKGrid(const std::vector<BarAgeAggregate>& aggs) const
  {
    const int T = static_cast<int>(aggs.size());
    if (T == 0)
      return {};

    int tPick = -1;
    double bestScore = -1e9;
    for (int t = 0; t < T; ++t)
      {
	const double fnp = aggs[t].getFracNonPositive();
	const double pT1 = aggs[t].getProbTargetNextBar();
	if (tPick < 0 && fnp >= mOpts.getFracNonPosHigh() && pT1 <= mOpts.getTargetHazardLow())
	  tPick = t;

	const double s = fnp - pT1;
	if (s > bestScore)
	  bestScore = s;
      }

    std::vector<int> grid;
    auto pushNeighbor = [&](int t)
    {
      for (int d = -mOpts.getNeighborSpan(); d <= mOpts.getNeighborSpan(); ++d)
	{
	  int x = t + d;
	  if (0 <= x && x < T)
	    grid.push_back(x);
	}
    };

    if (tPick >= 0)
      pushNeighbor(tPick);

    if (grid.empty() && mOpts.getUseFullGridIfEmpty())
      {
	grid.resize(T);
	std::iota(grid.begin(), grid.end(), 0);
      }

    std::sort(grid.begin(), grid.end());
    grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
    return grid;
  }

  template<class Decimal>
  std::vector<int> ExitPolicyAutoTuner<Decimal>::proposeNGrid(const std::vector<BarAgeAggregate>& aggs) const
  {
    const int T = static_cast<int>(aggs.size());
    if (T == 0)
      return {};

    int tPick = -1;
    for (int t = 0; t < T; ++t)
      {
	const double medMfeR = aggs[t].getMedianMfeRSoFar();
	if (!std::isnan(medMfeR) && medMfeR >= mOpts.getAlphaMfeR())
	  {
	    tPick = t;
	    break;
	  }
      }

    std::vector<int> grid;
    auto pushNeighbor = [&](int t)
    {
      for (int d = -mOpts.getNeighborSpan(); d <= mOpts.getNeighborSpan(); ++d)
	{
	  int x = t + d;
	  if (0 <= x && x < T)
	    grid.push_back(x);
	}
    };

    if (tPick >= 0)
      pushNeighbor(tPick);

    if (grid.empty() && mOpts.getUseFullGridIfEmpty())
      {
	for (int x : {0,1,2})
	  if (x < T)
	    grid.push_back(x);
      }

    std::sort(grid.begin(), grid.end());
    grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
    return grid;
  }

  // -------------------- Scoring --------------------
  template<class Decimal>
  double ExitPolicyAutoTuner<Decimal>::score(const PolicyResult& r) const {
    switch (mOpts.getObjective()) {
    case TuningObjective::HitRate: return r.getHitRate();
    case TuningObjective::AvgPnL_R:
    default: return r.getAvgPnL_R();
    }
  }

  // -------------------- Selectors --------------------
  template<class Decimal>
  int ExitPolicyAutoTuner<Decimal>::selectBestK(const ClosedPositionHistory<Decimal>& train,
						const std::vector<int>& Kgrid,
						PolicyResult& bestTrain) const
  {
    if (Kgrid.empty())
      {
	bestTrain = PolicyResult(0.0,0.0,0.0,0);
	return 0;
      }

    MetaExitCalibrator<Decimal> cal(train);
    double bestScore = -1e99;
    int bestK = Kgrid.front();
    bestTrain = PolicyResult(0.0,0.0,0.0,0);

    for (int K : Kgrid)
      {
	auto res = cal.evaluateFailureToPerformBars(K, mOpts.getThresholdR(), FailureExitFill::OpenOfKPlus1);
	const double s = score(res);
	if (s > bestScore || (s == bestScore && K < bestK)
	    || (s == bestScore && res.getHitRate() > bestTrain.getHitRate()))
	  {
	    bestScore = s; bestK = K; bestTrain = res;
	  }
      }
    return bestK;
  }

  template<class Decimal>
  int ExitPolicyAutoTuner<Decimal>::selectBestN(const ClosedPositionHistory<Decimal>& train,
						const std::vector<int>& Ngrid,
						PolicyResult& bestTrain) const
  {
    if (Ngrid.empty())
      {
	bestTrain = PolicyResult(0.0,0.0,0.0,0);
	return 0;
      }

    MetaExitCalibrator<Decimal> cal(train);
    double bestScore = -1e99;
    int bestN = Ngrid.front();
    bestTrain = PolicyResult(0.0,0.0,0.0,0);

    for (int N : Ngrid)
      {
	auto res = cal.evaluateBreakevenAfterBars(N, mOpts.getEpsilonR());
	const double s = score(res);
	if (s > bestScore || (s == bestScore && N < bestN)
	    || (s == bestScore && res.getHitRate() > bestTrain.getHitRate()))
	  {
	    bestScore = s; bestN = N; bestTrain = res;
	  }
      }
    return bestN;
  }

  // -------------------- Orchestrator --------------------
  template<class Decimal>
  ExitTuningReport<Decimal> ExitPolicyAutoTuner<Decimal>::tune()
  {
    // 1) Summaries to seed grids
    MetaExitAnalytics<Decimal> mex(mClosedPositionHistory);
    const auto aggs = mex.summarizeByBarAge(mOpts.getMaxBarsToAnalyze());

    const auto Kgrid = proposeKGrid(aggs);
    const auto Ngrid = proposeNGrid(aggs);

    // 2) Train/Test split
    ClosedPositionHistory<Decimal> train, test;
    splitTrainTest(train, test);
    const bool useFull = (test.beginTradingPositions() == test.endTradingPositions());

    // 3) Select K & N on train (or on all if no test)
    PolicyResult bestTrainK(0.0,0.0,0.0,0), bestTrainN(0.0,0.0,0.0,0);
    const int K = selectBestK(useFull ? mClosedPositionHistory : train, Kgrid, bestTrainK);
    const int N = selectBestN(useFull ? mClosedPositionHistory : train, Ngrid, bestTrainN);

    // 4) Test metrics
    PolicyResult testK(0.0,0.0,0.0,0), testN(0.0,0.0,0.0,0);
    if (useFull)
      {
	testK = bestTrainK;
	testN = bestTrainN;
      }
    else
      {
	MetaExitCalibrator<Decimal> calTest(test);
	testK = calTest.evaluateFailureToPerformBars(K, mOpts.getThresholdR(), FailureExitFill::OpenOfKPlus1);
	testN = calTest.evaluateBreakevenAfterBars(N, mOpts.getEpsilonR());
      }

    // 5) Build immutable report
    return ExitTuningReport<Decimal>(K, N,
				     bestTrainK, testK,
				     bestTrainN, testN,
				     Kgrid, Ngrid);
  }  
} // namespace mkc_timeseries
