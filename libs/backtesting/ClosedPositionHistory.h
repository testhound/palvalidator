// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __POSITION_MANAGER_H
#define __POSITION_MANAGER_H 1

#include <exception>
#include <memory>
#include <map>
#include <vector>
#include <cstdint>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include "TradingPosition.h"
#include "TimeSeriesEntry.h"
#include "StatUtils.h"

namespace mkc_timeseries
{
  using boost::accumulators::accumulator_set;
  using boost::accumulators::stats;
  using boost::accumulators::median;
  using boost::accumulators::mean;
  using boost::accumulators::sum;
  // TimeSeriesDate is boost::gregorian::date, ptime is boost::posix_time::ptime
  // We will use ptime as the key for the map.

  typedef boost::accumulators::tag::median median_tag;
  typedef boost::accumulators::tag::mean mean_tag;
  typedef boost::accumulators::tag::sum sum_tag;

  class ClosedPositionHistoryException : public std::runtime_error
  {
  public:
    ClosedPositionHistoryException(const std::string msg)
      : std::runtime_error(msg)
    {}

    ~ClosedPositionHistoryException()
    {}

  };

  template <class Decimal>
  struct ExpandedBarMetrics
  {
    Decimal closeToClose;
    Decimal openToClose;
    Decimal highToOpen;
    Decimal lowToOpen;
  };
  
  template <class Decimal> class ClosedPositionHistory
  {
  public:
    // Changed map key from TimeSeriesDate (boost::gregorian::date) to ptime
    typedef typename std::multimap<ptime,std::shared_ptr<TradingPosition<Decimal>>>::iterator PositionIterator;
    typedef typename std::multimap<ptime,std::shared_ptr<TradingPosition<Decimal>>>::const_iterator ConstPositionIterator;
    typedef std::vector<unsigned int>::const_iterator ConstBarsInPositionIterator;
    typedef std::vector<double>::const_iterator ConstTradeReturnIterator;

    ClosedPositionHistory()
      : mPositions(),
        mSumWinners(DecimalConstants<Decimal>::DecimalZero),
        mSumLosers(DecimalConstants<Decimal>::DecimalZero),
	mLogSumWinners(DecimalConstants<Decimal>::DecimalZero),
	mLogSumLosers(DecimalConstants<Decimal>::DecimalZero),
        mNumWinners(0),
        mNumLosers(0),
        mNumBarsInMarket(0),
        mRMultipleSum(DecimalConstants<Decimal>::DecimalZero),
        mWinnersStats(),
        mLosersStats(),
        mWinnersVect(),
        mLosersVect(),
        mBarsPerPosition(),
        mBarsPerWinningPosition(),
        mBarsPerLosingPosition()
    {}

    ClosedPositionHistory(const ClosedPositionHistory<Decimal>& rhs)
      : mPositions(rhs.mPositions),
        mSumWinners(rhs.mSumWinners),
        mSumLosers(rhs.mSumLosers),
 mLogSumWinners(rhs.mLogSumWinners),
 mLogSumLosers(rhs.mLogSumLosers),
        mNumWinners(rhs.mNumWinners),
        mNumLosers(rhs.mNumLosers),
        mNumBarsInMarket(rhs.mNumBarsInMarket),
        mRMultipleSum(rhs.mRMultipleSum),
        mWinnersStats(rhs.mWinnersStats),
        mLosersStats(rhs.mLosersStats),
        mWinnersVect(rhs.mWinnersVect),
        mLosersVect(rhs.mLosersVect),
        mBarsPerPosition(rhs.mBarsPerPosition),
        mBarsPerWinningPosition(rhs.mBarsPerWinningPosition),
        mBarsPerLosingPosition(rhs.mBarsPerLosingPosition)
    {}

    ClosedPositionHistory<Decimal>&
    operator=(const ClosedPositionHistory<Decimal> &rhs)
    {
      if (this == &rhs)
        return *this;

      mPositions = rhs.mPositions;
      mSumWinners = rhs.mSumWinners;
      mSumLosers = rhs.mSumLosers;
      mLogSumWinners = rhs.mLogSumWinners;
      mLogSumLosers = rhs.mLogSumLosers;
      mNumWinners = rhs.mNumWinners;
      mNumLosers = rhs.mNumLosers;
      mNumBarsInMarket = rhs.mNumBarsInMarket;
      mRMultipleSum = rhs.mRMultipleSum;
      mWinnersStats = rhs.mWinnersStats;
      mLosersStats = rhs.mLosersStats;
      mWinnersVect = rhs.mWinnersVect;
      mLosersVect = rhs.mLosersVect;
      mBarsPerPosition = rhs.mBarsPerPosition;
      mBarsPerWinningPosition = rhs.mBarsPerWinningPosition;
      mBarsPerLosingPosition = rhs.mBarsPerLosingPosition;

      return *this;
    }

    ~ClosedPositionHistory()
    {}

    void addClosedPosition(std::shared_ptr<TradingPosition<Decimal>> position)
    {
      if (position->isPositionOpen())
        throw ClosedPositionHistoryException ("ClosedPositionHistory:addClosedPosition - cannot add open position");

      // Changed to use ptime and getEntryDateTime() for the map key
      ptime dt = position->getEntryDateTime(); //

      mBarsPerPosition.push_back (position->getNumBarsInPosition());
      mNumBarsInMarket += position->getNumBarsInPosition();

      if (position->RMultipleStopSet())
        mRMultipleSum += position->getRMultiple();

      // Insert using the ptime key
      mPositions.insert(std::make_pair(dt, position));

      Decimal percReturn (position->getPercentReturn());

      if (position->isWinningPosition())
        {
          mNumWinners++;
          mSumWinners += position->getPercentReturn();
	  mLogSumWinners += position->getLogTradeReturn();
          mWinnersStats (num::to_double(position->getPercentReturn()));
          mWinnersVect.push_back(num::to_double(position->getPercentReturn()));
          mBarsPerWinningPosition.push_back (position->getNumBarsInPosition());
        }
      else if (position->isLosingPosition())
        {
          mNumLosers++;
          mSumLosers += position->getPercentReturn();
	  mLogSumLosers += position->getLogTradeReturn();
          mLosersStats (num::to_double(percReturn));
          mLosersVect.push_back(num::to_double(num::abs(percReturn)));
          mBarsPerLosingPosition.push_back (position->getNumBarsInPosition());
        }
      else
        throw std::logic_error(std::string("ClosedPositionHistory:addClosedPosition - position not winner or lsoer"));
    }

    void addClosedPosition (const TradingPositionLong<Decimal>& position)
    {
      addClosedPosition (std::make_shared<TradingPositionLong<Decimal>>(position));
    }

    void addClosedPosition (const TradingPositionShort<Decimal>& position)
    {
      addClosedPosition (std::make_shared<TradingPositionShort<Decimal>>(position));
    }

    const Decimal getRMultipleExpectancy() const
    {
      uint32_t numPos = getNumPositions();

      if ((numPos > 0) && (mRMultipleSum > DecimalConstants<Decimal>::DecimalZero))
        return mRMultipleSum / Decimal(numPos);
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    uint32_t getNumPositions() const
    {
      return mPositions.size();
    }

    uint32_t getNumEntriesInBarsPerPosition() const
    {
      return mBarsPerPosition.size();
    }

    uint32_t getNumWinningPositions() const
    {
      return mNumWinners;
    }

    uint32_t getNumLosingPositions() const
    {
      return mNumLosers;
    }

    uint32_t getNumBarsInMarket() const
    {
      return mNumBarsInMarket;
    }

/**
     * @brief Extract high-resolution bar-by-bar returns from all closed trades.
     *
     * @details
     * Iterates through each closed `TradingPosition` and its bar history to
     * compute per-bar returns. This method provides a more accurate return
     * calculation for the final bar of the trade by using the actual `exitPrice`
     * from the `TradingPosition` instead of the bar's close price. This ensures
     * the return series perfectly reflects the realized P&L of the strategy.
     *
     * For all intermediate bars, a standard mark-to-market (close-to-reference)
     * return is used. The sequence of returns is:
     * 1. (First Bar Close - Entry Price) / Entry Price
     * 2. (Second Bar Close - First Bar Close) / First Bar Close
     * 3. ...
     * 4. (Exit Price - Last Bar's Previous Close) / Last Bar's Previous Close
     *
     * @return Vector of Decimal returns, one entry per bar across all closed trades.
     */
    std::vector<Decimal> getHighResBarReturns() const
    {
        std::vector<Decimal> allReturns;
        // Iterate through each closed position in the history
        for (auto it = mPositions.begin(); it != mPositions.end(); ++it)
        {
            const auto& pos = it->second; // pos is a std::shared_ptr<TradingPosition<Decimal>>

            auto bar_it = pos->beginPositionBarHistory();
            auto bar_end = pos->endPositionBarHistory();

            if (bar_it == bar_end) {
                continue; // Skip positions with no bar history
            }

            // The first reference price is the actual entry price of the trade
            Decimal prevReferencePrice = pos->getEntryPrice();

            // Loop through all bars that were recorded while the position was open
            for (; bar_it != bar_end; ++bar_it)
            {
                const auto& currentBar = bar_it->second;
                Decimal returnForThisBar;

                // Check if this is the last bar in the position's recorded history
                if (std::next(bar_it) == bar_end)
                {
                    // For the final bar, the return is calculated to the actual exit price
                    Decimal exitPrice = pos->getExitPrice();
                    if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                        returnForThisBar = (exitPrice - prevReferencePrice) / prevReferencePrice;
                    else
                        returnForThisBar = DecimalConstants<Decimal>::DecimalZero;

                }
                else
                {
                    // For all intermediate bars, calculate the mark-to-market return (close-to-reference)
                    Decimal currentClose = currentBar.getCloseValue();
                     if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                        returnForThisBar = (currentClose - prevReferencePrice) / prevReferencePrice;
                    else
                        returnForThisBar = DecimalConstants<Decimal>::DecimalZero;


                    // The reference for the next bar becomes the close of the current bar
                    prevReferencePrice = currentClose;
                }

                // For short positions, a decrease in price is a gain, so we invert the return.
                if (pos->isShortPosition())
                {
                    returnForThisBar *= -1;
                }

                allReturns.push_back(returnForThisBar);
            }
        }
        return allReturns;
    }

    std::vector<ExpandedBarMetrics<Decimal>> getExpandedHighResBarReturns() const
    {
      std::vector<ExpandedBarMetrics<Decimal>> result;

      for (const auto& posEntry : mPositions)
	{
	  const auto& pos = posEntry.second;
	  auto barIt = pos->beginPositionBarHistory();
	  auto endIt = pos->endPositionBarHistory();

	  if (barIt == endIt)
            continue;

	  auto prev = barIt;
	  for (auto curr = std::next(barIt); curr != endIt; ++curr)
	    {
	      const auto& prevBar = prev->second;
	      const auto& bar = curr->second;

	      Decimal prevClose = prevBar.getCloseValue();
	      Decimal open = bar.getOpenValue();
	      Decimal high = bar.getHighValue();
	      Decimal low = bar.getLowValue();
	      Decimal close = bar.getCloseValue();

	      if (prevClose == Decimal(0))
                continue;

	      ExpandedBarMetrics<Decimal> metrics;
	      metrics.closeToClose = (close - prevClose) / prevClose;
	      metrics.openToClose = (close - open) / open;
	      metrics.highToOpen  = (high - open) / open;
	      metrics.lowToOpen   = (low - open) / open;

	      result.push_back(metrics);
	      prev = curr;
	    }
	}

      return result;
    }

    Decimal getAverageWinningTrade() const
    {
      if (mNumWinners >= 1)
        return (Decimal(mSumWinners) /Decimal(mNumWinners));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricMean(std::vector<double> const&data) const
    {
      const double too_large = 1.e64;
      const double too_small = 1.e-64;
      double sum_log = 0.0;
      double product = 1.0;
      for(auto x:data) {
          product *= x;
          if(product > too_large || product < too_small) {
              sum_log+= std::log(product);
              product = 1;
            }
        }
      return (Decimal (std::exp((sum_log + std::log(product))/data.size())));
    }

    Decimal getGeometricWinningTrade() const
    {
      if (mNumWinners >= 1)
        return (Decimal (getGeometricMean (mWinnersVect)));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianWinningTrade() const
    {
      if (mNumWinners >= 1)
        return (Decimal(median (mWinnersStats)));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getAverageLosingTrade() const
    {
      if (mNumLosers >= 1)
        return (Decimal(mSumLosers) /Decimal(mNumLosers));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricLosingTrade() const
    {
      if (mNumLosers >= 1)
        return (Decimal (getGeometricMean (mLosersVect)));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianLosingTrade() const
    {
      if (mNumLosers >= 1)
        return (Decimal(median (mLosersStats)));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPercentWinners() const
    {
      if (getNumPositions() > 0)
        return ((Decimal(mNumWinners) / Decimal(getNumPositions())) *
                DecimalConstants<Decimal>::DecimalOneHundred);
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPercentLosers() const
    {
      if (getNumPositions() > 0)
        return ((Decimal(mNumLosers) / Decimal(getNumPositions())) *
                DecimalConstants<Decimal>::DecimalOneHundred);
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPayoffRatio() const
    {
      if (getNumPositions() > 0)
        {
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
            {
              Decimal avgLoser = num::abs(getAverageLosingTrade());
              if (avgLoser != DecimalConstants<Decimal>::DecimalZero)
                return (getAverageWinningTrade() / avgLoser);
              else
                return (getAverageWinningTrade());
            }
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (getAverageWinningTrade());
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getPayoffRatio - getNumPositions > 0 error"));

        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricPayoffRatio() const
    {
      if (getNumPositions() > 0)
        {
          if (getGeometricLosingTrade() == DecimalConstants<Decimal>::DecimalZero)
            return getPayoffRatio();
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
            return (getGeometricWinningTrade() / getGeometricLosingTrade());
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (getGeometricWinningTrade());
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getGeometricPayoffRatio - getNumPositions > 0 error"));

        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianPayoffRatio() const
    {
      if (getNumPositions() > 0)
        {
          if (getMedianLosingTrade() == DecimalConstants<Decimal>::DecimalZero)
            return getPayoffRatio();
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
            return (getMedianWinningTrade() / num::abs(getMedianLosingTrade()));
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (getMedianWinningTrade());
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getMedianPayoffRatio - getNumPositions > 0 error"));

        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPessimisticReturnRatio() const
    {
      if (getNumPositions() > 0)
        {
          if ((mNumWinners == 0) || (mNumWinners == 1))
            return (DecimalConstants<Decimal>::DecimalZero);

          Decimal numTrades(getNumPositions());
          Decimal numerator = (((Decimal(mNumWinners) -
                                 DecimalSqrtConstants<Decimal>::getSqrt (mNumWinners))/numTrades)
                               * getMedianWinningTrade());

          if (mNumLosers == 0)
            return numerator;

          Decimal denominator = (((Decimal(mNumLosers) +
                                   DecimalSqrtConstants<Decimal>::getSqrt (mNumLosers))/numTrades)
                                 * num::abs(getMedianLosingTrade()));

          if (denominator == DecimalConstants<Decimal>::DecimalZero)
            return numerator;
          else
            return numerator / denominator;
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getProfitFactorCommon(const Decimal& winnersSum, const Decimal& losersSum) const
    {
      if (getNumPositions() > 0)
        {
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
	    {
	      if (num::abs(losersSum) == DecimalConstants<Decimal>::DecimalZero)
		return (DecimalConstants<Decimal>::DecimalOneHundred);
	      else
		return (winnersSum / num::abs(losersSum));
	    }
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (DecimalConstants<Decimal>::DecimalOneHundred);
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getProfitFactor - getNumPositions > 0 error"));
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);

    }
    
    Decimal getProfitFactor() const
    {
      return getProfitFactorCommon(mSumWinners, mSumLosers);
    }

    // Add this calculation (natural log of trade returns) which is the preferred
    // method of Timothy Master's
    // as described in his book "Permutation and Randomization Tests for
    // Trading System Development
    Decimal getLogProfitFactor() const
    {
      return getProfitFactorCommon(mLogSumWinners, mLogSumLosers);
    }

    Decimal getHighResProfitFactor() const
    {
      auto returns = getHighResBarReturns();
      return StatUtils<Decimal>::computeProfitFactor(returns, false);
    }

    Decimal getHighResProfitability() const
    {
      auto returns = getHighResBarReturns();
      auto [pf, profitability] = StatUtils<Decimal>::computeProfitability(returns);
      return profitability;
    }
    
    Decimal getPALProfitability() const
    {
      if (getNumPositions() > 0)
        {
          Decimal pf(getProfitFactor());
          Decimal payoffRatio(getPayoffRatio());

          Decimal denominator (pf + payoffRatio);
          if (denominator > DecimalConstants<Decimal>::DecimalZero)
            return ((pf/denominator) * DecimalConstants<Decimal>::DecimalOneHundred);
          else
            return (DecimalConstants<Decimal>::DecimalZero);
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianPALProfitability() const
    {
      if (getNumPositions() > 0)
        {
          Decimal pf(getProfitFactor());
          Decimal payoffRatio(getMedianPayoffRatio());

          Decimal denominator (pf + payoffRatio);
          if (denominator > DecimalConstants<Decimal>::DecimalZero)
            {
              Decimal ratio(pf/denominator);
              return (ratio * DecimalConstants<Decimal>::DecimalOneHundred);
            }
          else
            return (DecimalConstants<Decimal>::DecimalZero);
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricPALProfitability() const
    {
      if (getNumPositions() > 0)
        {
          Decimal pf(getProfitFactor());
          Decimal payoffRatio(getGeometricPayoffRatio());

          Decimal denominator (pf + payoffRatio);
          if (denominator > DecimalConstants<Decimal>::DecimalZero)
            {
              Decimal ratio(pf/denominator);
              return (ratio * DecimalConstants<Decimal>::DecimalOneHundred);
            }
          else
            return (DecimalConstants<Decimal>::DecimalZero);
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }


    Decimal getCumulativeReturn() const
    {
      Decimal cumReturn(0);

      // Iterator type changes due to typedef
      ClosedPositionHistory::ConstPositionIterator it = beginTradingPositions();
      if (it != endTradingPositions())
        {
          cumReturn = it->second->getTradeReturnMultiplier();
          it++;

          for (; it != endTradingPositions(); it++)
            {
              cumReturn = cumReturn * it->second->getTradeReturnMultiplier();
            }

          cumReturn = cumReturn - DecimalConstants<Decimal>::DecimalOne;
        }

      return cumReturn;
    }

    // Return type changes due to typedef
    ClosedPositionHistory::ConstPositionIterator beginTradingPositions() const
    {
      return mPositions.begin();
    }

    // Return type changes due to typedef
    ClosedPositionHistory::ConstPositionIterator endTradingPositions() const
    {
      return mPositions.end();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator beginBarsPerPosition() const
    {
      return mBarsPerPosition.begin();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator endBarsPerPosition() const
    {
      return mBarsPerPosition.end();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator beginBarsPerWinningPosition() const
    {
      return mBarsPerWinningPosition.begin();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator endBarsPerWinningPosition() const
    {
      return mBarsPerWinningPosition.end();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator beginBarsPerLosingPosition() const
    {
      return mBarsPerLosingPosition.begin();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator endBarsPerLosingPosition() const
    {
      return mBarsPerLosingPosition.end();
    }

    ClosedPositionHistory::ConstTradeReturnIterator beginWinnersReturns() const
    {
      return mWinnersVect.begin();
    }

    ClosedPositionHistory::ConstTradeReturnIterator endWinnersReturns() const
    {
      return mWinnersVect.end();
    }

    ClosedPositionHistory::ConstTradeReturnIterator beginLosersReturns() const
    {
      return mLosersVect.begin();
    }

    ClosedPositionHistory::ConstTradeReturnIterator endLosersReturns() const
    {
      return mLosersVect.end();
    }

  private:
    // Changed map key from TimeSeriesDate to ptime
    std::multimap<ptime,std::shared_ptr<TradingPosition<Decimal>>> mPositions;
    Decimal mSumWinners;
    Decimal mSumLosers;
    Decimal mLogSumWinners;
    Decimal mLogSumLosers;
    unsigned int mNumWinners;
    unsigned int mNumLosers;
    unsigned int mNumBarsInMarket;
    Decimal mRMultipleSum;
    accumulator_set<double, stats<median_tag>> mWinnersStats;
    accumulator_set<double, stats<median_tag>> mLosersStats;
    std::vector<double> mWinnersVect;
    std::vector<double> mLosersVect;

    // Vector that holds total number of bars for each position
    std::vector<unsigned int> mBarsPerPosition;
    std::vector<unsigned int> mBarsPerWinningPosition;
    std::vector<unsigned int> mBarsPerLosingPosition;
  };
}
#endif
