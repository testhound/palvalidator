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

namespace mkc_timeseries
{
  using boost::accumulators::accumulator_set;
  using boost::accumulators::stats;
  using boost::accumulators::median;
  using boost::accumulators::mean;
  using boost::accumulators::sum;

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

 template <class Decimal> class ClosedPositionHistory
  {
  public:
    typedef typename std::multimap<TimeSeriesDate,std::shared_ptr<TradingPosition<Decimal>>>::iterator PositionIterator;
    typedef typename std::multimap<TimeSeriesDate,std::shared_ptr<TradingPosition<Decimal>>>::const_iterator ConstPositionIterator;
    typedef std::vector<unsigned int>::const_iterator ConstBarsInPositionIterator;
    typedef std::vector<double>::const_iterator ConstTradeReturnIterator;

    ClosedPositionHistory()
      : mPositions(),
	mSumWinners(DecimalConstants<Decimal>::DecimalZero),
	mSumLosers(DecimalConstants<Decimal>::DecimalZero),
	mNumWinners(0),
	mNumLosers(0),
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
	mNumWinners(rhs.mNumWinners),
	mNumLosers(rhs.mNumLosers),
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
      mNumWinners = rhs.mNumWinners;
      mNumLosers = rhs.mNumLosers;
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

      boost::gregorian::date d = position->getEntryDate();

      mBarsPerPosition.push_back (position->getNumBarsInPosition());

      if (position->RMultipleStopSet())
	mRMultipleSum += position->getRMultiple();

      mPositions.insert(std::make_pair(d, position));

      Decimal percReturn (position->getPercentReturn());
      
      if (position->isWinningPosition())
	{
	  mNumWinners++;
	  mSumWinners += position->getPercentReturn();
	  mWinnersStats (num::to_double(position->getPercentReturn()));
	  mWinnersVect.push_back(num::to_double(position->getPercentReturn()));
	  mBarsPerWinningPosition.push_back (position->getNumBarsInPosition());
	}
      else if (position->isLosingPosition())
	{
	  mNumLosers++;
	  mSumLosers += position->getPercentReturn();
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

    Decimal getProfitFactor() const
    {
      if (getNumPositions() > 0)
	{
	  if ((mNumWinners >= 1) and (mNumLosers >= 1))
	    return (mSumWinners / num::abs(mSumLosers));
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

      ClosedPositionHistory::ConstPositionIterator it = beginTradingPositions();
      if (it != endTradingPositions())
	{
	  cumReturn = it->second->getTradeReturnMultiplier();
	  it++;

	  for (; it != endTradingPositions(); it++)
	    {
	      cumReturn = cumReturn *  it->second->getTradeReturnMultiplier();
	    }
	  
	  cumReturn = cumReturn - DecimalConstants<Decimal>::DecimalOne;
	}

      return cumReturn;
    }

    ClosedPositionHistory::ConstPositionIterator beginTradingPositions() const
    {
      return mPositions.begin();
    }

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
    std::multimap<TimeSeriesDate,std::shared_ptr<TradingPosition<Decimal>>> mPositions;
    Decimal mSumWinners;
    Decimal mSumLosers;
    unsigned int mNumWinners;
    unsigned int mNumLosers;
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

 /*
 template <class Decimal> class PositionManager
  {
    bool isInstrumentLong (const std::string& symbol);
    bool isInstrumentShort (const std::string& symbol);
    bool isInstrumentFlat (const std::string& symbol);

  private:
    typedef std::shared_ptr<TradingPosition<Decimal> TradingPositionPtr;
    
    std::map<uint32_t, std::shared_ptr<TradingPosition<Decimal>> orderIDToPositionMap;
    std::multimap<uint32_t, std::shared_ptr<TradingOrder<Decimal>> PositionIDToOrderMap;
    std::map<uint32_t, std::shared_ptr<TradingOrder<Decimal>> PositionIDToPositionMap;
    std::map<std::string, std::vector<TradingPositionPtr>> SymbolToOpenPositionMap;
  };
 */
 }
#endif
