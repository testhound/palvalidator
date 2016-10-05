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

 template <int Prec> class ClosedPositionHistory
  {
  public:
    typedef typename std::multimap<TimeSeriesDate,std::shared_ptr<TradingPosition<Prec>>>::iterator PositionIterator;
    typedef typename std::multimap<TimeSeriesDate,std::shared_ptr<TradingPosition<Prec>>>::const_iterator ConstPositionIterator;
    typedef std::vector<unsigned int>::const_iterator ConstBarsInPositionIterator;
    typedef std::vector<double>::const_iterator ConstTradeReturnIterator;

    ClosedPositionHistory()
      : mPositions(),
	mSumWinners(DecimalConstants<Prec>::DecimalZero),
	mSumLosers(DecimalConstants<Prec>::DecimalZero),
	mNumWinners(0),
	mNumLosers(0),
	mRMultipleSum(DecimalConstants<Prec>::DecimalZero),
	mWinnersStats(),
	mLosersStats(),
	mWinnersVect(),
	mLosersVect(),
	mBarsPerPosition()
    {}

    ClosedPositionHistory(const ClosedPositionHistory<Prec>& rhs) 
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
	mBarsPerPosition(rhs.mBarsPerPosition)
    {}

    ClosedPositionHistory<Prec>& 
    operator=(const ClosedPositionHistory<Prec> &rhs)
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
      return *this;
    }

    ~ClosedPositionHistory()
    {}

    void addClosedPosition(std::shared_ptr<TradingPosition<Prec>> position)
    {
      if (position->isPositionOpen())
	throw ClosedPositionHistoryException ("ClosedPositionHistory:addClosedPosition - cannot add open position");

      boost::gregorian::date d = position->getEntryDate();

      mBarsPerPosition.push_back (position->getNumBarsInPosition());

      if (position->RMultipleStopSet())
	mRMultipleSum += position->getRMultiple();

      mPositions.insert(std::make_pair(d, position));

      decimal<Prec> percReturn (position->getPercentReturn());
      
      if (position->isWinningPosition())
	{
	  mNumWinners++;
	  mSumWinners += position->getPercentReturn();
	  mWinnersStats (position->getPercentReturn().getAsDouble());
	  mWinnersVect.push_back(position->getPercentReturn().getAsDouble());
	}
      else if (position->isLosingPosition())
	{
	  mNumLosers++;
	  mSumLosers += position->getPercentReturn();
	  mLosersStats (percReturn.getAsDouble());
	  mLosersVect.push_back(percReturn.abs().getAsDouble());
	}
      else
	throw std::logic_error(std::string("ClosedPositionHistory:addClosedPosition - position not winner or lsoer"));
    }

    void addClosedPosition (const TradingPositionLong<Prec>& position)
    {
      addClosedPosition (std::make_shared<TradingPositionLong<Prec>>(position));
    }

    void addClosedPosition (const TradingPositionShort<Prec>& position)
    {
      addClosedPosition (std::make_shared<TradingPositionShort<Prec>>(position));
    }

    const dec::decimal<Prec> getRMultipleExpectancy() const
    {
      uint32_t numPos = getNumPositions();

      if ((numPos > 0) && (mRMultipleSum > DecimalConstants<Prec>::DecimalZero))
	return mRMultipleSum / dec::decimal<Prec>(numPos);
      else
	return (DecimalConstants<Prec>::DecimalZero);
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

    decimal<Prec> getAverageWinningTrade() const
    {
      if (mNumWinners >= 1)
	return (decimal_cast<Prec> (mSumWinners) /decimal_cast<Prec>(mNumWinners));
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getGeometricMean(std::vector<double> const&data) const
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
      return (decimal<Prec> (std::exp((sum_log + std::log(product))/data.size())));
    }

    decimal<Prec> getGeometricWinningTrade() const
    {
      if (mNumWinners >= 1)
	return (decimal<Prec> (getGeometricMean (mWinnersVect)));
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getMedianWinningTrade() const
    {
      if (mNumWinners >= 1)
	return (decimal<Prec>(median (mWinnersStats)));
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getAverageLosingTrade() const
    {
      if (mNumLosers >= 1)
	return (decimal_cast<Prec> (mSumLosers) /decimal_cast<Prec>(mNumLosers));
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getGeometricLosingTrade() const
    {
      if (mNumLosers >= 1)
	return (decimal<Prec> (getGeometricMean (mLosersVect)));
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getMedianLosingTrade() const
    {
      if (mNumLosers >= 1)
	return (decimal<Prec>(median (mLosersStats)));
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getPercentWinners() const
    {
      if (getNumPositions() > 0)
	return ((decimal_cast<Prec> (mNumWinners) / decimal_cast<Prec>(getNumPositions())) * 
		DecimalConstants<Prec>::DecimalOneHundred);
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getPercentLosers() const
    {
      if (getNumPositions() > 0)
	return ((decimal_cast<Prec> (mNumLosers) / decimal_cast<Prec>(getNumPositions())) * 
		DecimalConstants<Prec>::DecimalOneHundred);
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getPayoffRatio() const
    {
      if (getNumPositions() > 0)
	{
	  if ((mNumWinners >= 1) and (mNumLosers >= 1))
	    {
	      dec::decimal<Prec> avgLoser = getAverageLosingTrade().abs();
	      if (avgLoser != DecimalConstants<Prec>::DecimalZero)
		return (getAverageWinningTrade() / avgLoser);
	      else
		return (getAverageWinningTrade());
	    }
	  else if (mNumWinners == 0)
	    return (DecimalConstants<Prec>::DecimalZero);
	  else if (mNumLosers == 0)
	    return (getAverageWinningTrade());
	  else
	    throw std::logic_error(std::string("ClosedPositionHistory:getPayoffRatio - getNumPositions > 0 error"));
	  
	}
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getGeometricPayoffRatio() const
    {
      if (getNumPositions() > 0)
	{
	  if (getGeometricLosingTrade() == DecimalConstants<Prec>::DecimalZero)
	    return getPayoffRatio();
	  if ((mNumWinners >= 1) and (mNumLosers >= 1))
	    return (getGeometricWinningTrade() / getGeometricLosingTrade());
	  else if (mNumWinners == 0)
	    return (DecimalConstants<Prec>::DecimalZero);
	  else if (mNumLosers == 0)
	    return (getGeometricWinningTrade());
	  else
	    throw std::logic_error(std::string("ClosedPositionHistory:getGeometricPayoffRatio - getNumPositions > 0 error"));
	  
	}
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getMedianPayoffRatio() const
    {
      if (getNumPositions() > 0)
	{
	  if (getMedianLosingTrade() == DecimalConstants<Prec>::DecimalZero)
	    return getPayoffRatio();
	  if ((mNumWinners >= 1) and (mNumLosers >= 1))
	    return (getMedianWinningTrade() / getMedianLosingTrade().abs());
	  else if (mNumWinners == 0)
	    return (DecimalConstants<Prec>::DecimalZero);
	  else if (mNumLosers == 0)
	    return (getMedianWinningTrade());
	  else
	    throw std::logic_error(std::string("ClosedPositionHistory:getMedianPayoffRatio - getNumPositions > 0 error"));
	  
	}
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getPessimisticReturnRatio() const
      {
	if (getNumPositions() > 0)
	  {
	    if ((mNumWinners == 0) || (mNumWinners == 1))
	      return (DecimalConstants<Prec>::DecimalZero);

	    dec::decimal<Prec> numTrades(getNumPositions());
	    dec::decimal<Prec> numerator = (((dec::decimal<Prec>(mNumWinners) -
					      DecimalSqrtConstants<Prec>::getSqrt (mNumWinners))/numTrades)
					    * getMedianWinningTrade());

	    if (mNumLosers == 0)
	      return numerator;

	    dec::decimal<Prec> denominator = (((dec::decimal<Prec>(mNumLosers) +
					      DecimalSqrtConstants<Prec>::getSqrt (mNumLosers))/numTrades)
					      * getMedianLosingTrade().abs());

	    if (denominator == DecimalConstants<Prec>::DecimalZero)
	      return numerator;
	    else
	      return numerator / denominator;
	  }
	else
	  return (DecimalConstants<Prec>::DecimalZero);
      }

    decimal<Prec> getProfitFactor() const
    {
      if (getNumPositions() > 0)
	{
	  if ((mNumWinners >= 1) and (mNumLosers >= 1))
	    return (mSumWinners / mSumLosers.abs());
	  else if (mNumWinners == 0)
	    return (DecimalConstants<Prec>::DecimalZero);
	  else if (mNumLosers == 0)
	    return (DecimalConstants<Prec>::DecimalOneHundred);
	  else
	    throw std::logic_error(std::string("ClosedPositionHistory:getProfitFactor - getNumPositions > 0 error"));
	}
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getPALProfitability() const
    {
      if (getNumPositions() > 0)
	{
	  decimal<Prec> pf(getProfitFactor());
	  decimal<Prec> payoffRatio(getPayoffRatio());

	  decimal<Prec> denominator (pf + payoffRatio);
	  if (denominator > DecimalConstants<Prec>::DecimalZero)
	    return ((pf/denominator) * DecimalConstants<Prec>::DecimalOneHundred);
	  else
	    return (DecimalConstants<Prec>::DecimalZero);
	}
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getMedianPALProfitability() const
    {
      if (getNumPositions() > 0)
	{
	  decimal<Prec> pf(getProfitFactor());
	  decimal<Prec> payoffRatio(getMedianPayoffRatio());

	  decimal<Prec> denominator (pf + payoffRatio);
	  if (denominator > DecimalConstants<Prec>::DecimalZero)
	    {
	      decimal<Prec> ratio(pf/denominator);
	      return (ratio * DecimalConstants<Prec>::DecimalOneHundred);
	    }
	  else
	    return (DecimalConstants<Prec>::DecimalZero);
	}
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }

    decimal<Prec> getGeometricPALProfitability() const
    {
      if (getNumPositions() > 0)
	{
	  decimal<Prec> pf(getProfitFactor());
	  decimal<Prec> payoffRatio(getGeometricPayoffRatio());

	  decimal<Prec> denominator (pf + payoffRatio);
	  if (denominator > DecimalConstants<Prec>::DecimalZero)
	    {
	      decimal<Prec> ratio(pf/denominator);
	      return (ratio * DecimalConstants<Prec>::DecimalOneHundred);
	    }
	  else
	    return (DecimalConstants<Prec>::DecimalZero);
	}
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }


    dec::decimal<Prec> getCumulativeReturn() const
    {
      dec::decimal<Prec> cumReturn(0);

      ClosedPositionHistory::ConstPositionIterator it = beginTradingPositions();
      if (it != endTradingPositions())
	{
	  cumReturn = it->second->getTradeReturnMultiplier();
	  it++;

	  for (; it != endTradingPositions(); it++)
	    {
	      cumReturn = cumReturn *  it->second->getTradeReturnMultiplier();
	    }
	  
	  cumReturn = cumReturn - DecimalConstants<Prec>::DecimalOne;
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
    std::multimap<TimeSeriesDate,std::shared_ptr<TradingPosition<Prec>>> mPositions;
    decimal<Prec> mSumWinners;
    decimal<Prec> mSumLosers;
    unsigned int mNumWinners;
    unsigned int mNumLosers;
    decimal<Prec> mRMultipleSum;
    accumulator_set<double, stats<median_tag>> mWinnersStats;
    accumulator_set<double, stats<median_tag>> mLosersStats;
    std::vector<double> mWinnersVect;
    std::vector<double> mLosersVect;

    // Vector that holds total number of bars for each position
    std::vector<unsigned int> mBarsPerPosition;
  };

 /*
 template <int Prec> class PositionManager
  {
    bool isInstrumentLong (const std::string& symbol);
    bool isInstrumentShort (const std::string& symbol);
    bool isInstrumentFlat (const std::string& symbol);

  private:
    typedef std::shared_ptr<TradingPosition<Prec> TradingPositionPtr;
    
    std::map<uint32_t, std::shared_ptr<TradingPosition<Prec>> orderIDToPositionMap;
    std::multimap<uint32_t, std::shared_ptr<TradingOrder<Prec>> PositionIDToOrderMap;
    std::map<uint32_t, std::shared_ptr<TradingOrder<Prec>> PositionIDToPositionMap;
    std::map<std::string, std::vector<TradingPositionPtr>> SymbolToOpenPositionMap;
  };
 */
 }
#endif
