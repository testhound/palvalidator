// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __MCPT_STRATEGY_ATTRIBUTES_H
#define __MCPT_STRATEGY_ATTRIBUTES_H 1

#include <exception>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <boost/date_time.hpp>
#include "MapUtilities.h"
#include "decimal.h"
#include "Security.h"
#include "TradingPosition.h"
#include "ThrowAssert.hpp"

namespace mkc_timeseries
{
  using dec::decimal;
  using boost::gregorian::date;

  class MCPTStrategyAttributesException : public std::runtime_error
  {
  public:
    MCPTStrategyAttributesException(const std::string msg) 
      : std::runtime_error(msg)
    {}
    
    ~MCPTStrategyAttributesException()
    {}
    
  };

  template <class Decimal> class MCPTStrategyAttributes
  {
  public:
    typedef std::map<date, int>::const_iterator PositionDirectionIterator;
    typedef typename std::map<date, Decimal>::const_iterator PositionReturnsIterator;

  public:
    MCPTStrategyAttributes()
      : mPositionDirection(),
	mBarReturns()
    {}

    ~MCPTStrategyAttributes()
    {}

    MCPTStrategyAttributes(const MCPTStrategyAttributes<Decimal>& rhs)
      : mPositionDirection(rhs.mPositionDirection),
	mBarReturns(rhs.mBarReturns)
      {}

      const MCPTStrategyAttributes<Decimal>&
      operator=(const MCPTStrategyAttributes<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	mPositionDirection = rhs.mPositionDirection;
	mBarReturns = rhs.mBarReturns;

	return *this;
      }

    void addLongPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
    {
      addPositionDirection (1, processingDate);
      Decimal percentReturn = getCloseToCloseReturn (aSecurity,
							   processingDate);
      addPositionReturn (percentReturn, processingDate);
    }

    void addShortPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
    {
      addPositionDirection (-1, processingDate);
      Decimal percentReturnTemp = getCloseToCloseReturn (aSecurity,
							   processingDate);
      Decimal percentReturn = getCloseToCloseReturn (aSecurity,
							   processingDate);

      addPositionReturn (percentReturn, processingDate);
    }

    void addFlatPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
    {
      addPositionDirection (0, processingDate);
      Decimal percentReturn = getCloseToCloseReturn (aSecurity,
							   processingDate);
      addPositionReturn (percentReturn, processingDate);
    }

    PositionDirectionIterator beginPositionDirection() const
    {
      return mPositionDirection.begin();
    }

    PositionDirectionIterator endPositionDirection() const
    {
      return mPositionDirection.end();
    }

    PositionReturnsIterator beginPositionReturns() const
    {
      return mBarReturns.begin();
    }

    PositionReturnsIterator endPositionReturns() const
    {
      return mBarReturns.end();
    }

    std::vector<int> getPositionDirection() const
    {
      std::vector<int> posDirectionVector;

      posDirectionVector.reserve (mPositionDirection.size());

      std::transform(mPositionDirection.begin(), 
		     mPositionDirection.end(), 
		     std::back_inserter(posDirectionVector), 
		       second(mPositionDirection));

      return posDirectionVector;
    }

    std::vector<Decimal> getPositionReturns() const
    {
      std::vector<Decimal> posReturnsVector;
      posReturnsVector.reserve (mBarReturns.size());

      std::transform(mBarReturns.begin(), 
		     mBarReturns.end(), 
		     std::back_inserter(posReturnsVector), 
		     second(mBarReturns));

      return posReturnsVector;
    }

    unsigned long numTradingOpportunities() const
    {
      throw_assert ((mBarReturns.size() == mPositionDirection.size()),
		    "MCPTStrategyAttributes::numTradingOpportunities() -  size of internal map is not the same");
      return mBarReturns.size();
    }

  private:
    void addPositionReturn (const Decimal& positionReturn, 
			    const date& processingDate)
    {
      PositionReturnsIterator it = mBarReturns.find(processingDate);
      if (it == mBarReturns.end())
	{
	  mBarReturns.insert(std::make_pair (processingDate, positionReturn));
	}
      else
	throw MCPTStrategyAttributesException(std::string("MCPTStrategyAttributes:addPositionReturn" +boost::gregorian::to_simple_string(processingDate) + std::string(" date already exists")));
    }

    void addPositionDirection (int direction, const date& processingDate)
    {
      PositionDirectionIterator it = mPositionDirection.find(processingDate);
      if (it == mPositionDirection.end())
	{
	  mPositionDirection.insert(std::make_pair (processingDate, direction));
	}
      else
	throw MCPTStrategyAttributesException(std::string("MCPTStrategyAttributes:addPositionDirection" +boost::gregorian::to_simple_string(processingDate) + std::string(" date already exists")));
    }

    Decimal getCloseToCloseReturn(std::shared_ptr<Security<Decimal>> aSecurity,
					const date& processingDate) const
    {
      typename Security<Decimal>::ConstRandomAccessIterator it = 
	aSecurity->getRandomAccessIterator (processingDate);

      Decimal todaysClose = aSecurity->getCloseValue (it, 0);
      Decimal previousClose = aSecurity->getCloseValue (it, 1);
	
      return calculatePercentReturn (previousClose, todaysClose);
    }

  private:
    std::map<date, int> mPositionDirection; // 0 = flat, 1 = long, -1 = short
    std::map<date, Decimal> mBarReturns;
  };
}
#endif
