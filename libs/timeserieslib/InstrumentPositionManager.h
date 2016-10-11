// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __INSTRUMENT_POSITION_MANAGER_H
#define __INSTRUMENT_POSITION_MANAGER_H 1

#include <memory>
#include <map>
#include <cstdint>
#include "ThrowAssert.hpp"
#include "InstrumentPositionManagerException.h"
#include "InstrumentPosition.h"
#include "Portfolio.h"

namespace mkc_timeseries
{
  template <int Prec> class InstrumentPositionManager
  {
  public:
    typedef typename std::map<std::string, std::shared_ptr<InstrumentPosition<Prec>>>::const_iterator ConstInstrumentPositionIterator;

  public:
    InstrumentPositionManager()
      : mInstrumentPositions()
      {}

    InstrumentPositionManager (const InstrumentPositionManager<Prec>& rhs)
      : mInstrumentPositions(rhs.mInstrumentPositions)
    {}

    InstrumentPositionManager<Prec>& 
    operator=(const InstrumentPositionManager<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      mInstrumentPositions = rhs.mInstrumentPositions;

      return *this;
    }

    ~InstrumentPositionManager()
      {}

    TradingVolume getVolumeInAllUnits(const std::string& tradingSymbol) const
    {
      return getInstrumentPosition(tradingSymbol).getVolumeInAllUnits();
    }

    const InstrumentPosition<Prec>&
    getInstrumentPosition(const std::string& tradingSymbol) const
    {
      auto ptr = getInstrumentPositionPtr (tradingSymbol);
      return *ptr;
    }

    const InstrumentPosition<Prec>&
    getInstrumentPosition(ConstInstrumentPositionIterator it) const
    {
      return *(it->second);
    }

    bool isLongPosition(const std::string& tradingSymbol) const
    {
       return getInstrumentPositionPtr (tradingSymbol)->isLongPosition();
    }

    bool isShortPosition(const std::string& tradingSymbol) const
    {
      return getInstrumentPositionPtr (tradingSymbol)->isShortPosition();
    }

    bool isFlatPosition(const std::string& tradingSymbol) const
    {
      return getInstrumentPositionPtr (tradingSymbol)->isFlatPosition();
    }

    ConstInstrumentPositionIterator beginInstrumentPositions() const
    {
      return mInstrumentPositions.begin();
    }

    ConstInstrumentPositionIterator endInstrumentPositions() const
    {
      return mInstrumentPositions.end();
    }

    uint32_t getNumInstruments() const
    {
      return mInstrumentPositions.size();
    }

    void addInstrument (const std::string& tradingSymbol)
    {
      ConstInstrumentPositionIterator pos = mInstrumentPositions.find (tradingSymbol);
      
      if (pos == endInstrumentPositions())
	{
	  auto instrPos = std::make_shared<InstrumentPosition<Prec>>(tradingSymbol);
	  mInstrumentPositions.insert(std::make_pair(tradingSymbol, instrPos));
	}
      else
	throw InstrumentPositionManagerException("InstrumentPositionManager::addInstrument - trading symbol already exists");
    }

    void addPosition(std::shared_ptr<TradingPosition<Prec>> position)
    {
      getInstrumentPositionPtr (position->getTradingSymbol())->addPosition(position);
    }

    // addBar is used to add a bar to a open position

    void addBar (const std::string& tradingSymbol,
		 const OHLCTimeSeriesEntry<Prec>& entryBar)
    {
      getInstrumentPositionPtr (tradingSymbol)->addBar (entryBar);
    }

    void addBarForOpenPosition (const boost::gregorian::date openPositionDate,
				std::shared_ptr<Portfolio<Prec>> portfolioOfSecurities)
    {
      ConstInstrumentPositionIterator posIt = beginInstrumentPositions();
      std::shared_ptr<InstrumentPosition<Prec>> position;
      typename Portfolio<Prec>::ConstPortfolioIterator securityIterator;
      std::shared_ptr<Security<Prec>> aSecurity;
      typename Security<Prec>::ConstRandomAccessIterator timeSeriesEntryIterator;

      for (; posIt != endInstrumentPositions(); posIt++)
	{
	  position = posIt->second;

	  if (isFlatPosition (position->getInstrumentSymbol()) == false)
	    {
	      securityIterator = portfolioOfSecurities->findSecurity(position->getInstrumentSymbol());
	      if (securityIterator != portfolioOfSecurities->endPortfolio())
		{
		  aSecurity = securityIterator->second;

		  // Make sure the security has historical data for the date in question before
		  // adding it to the position.
		  timeSeriesEntryIterator = aSecurity->findTimeSeriesEntry (openPositionDate);
		  if (timeSeriesEntryIterator != aSecurity->getRandomAccessIteratorEnd())
		    {
		      addBar (position->getInstrumentSymbol(), *timeSeriesEntryIterator);
		    }
		}
	    }
	}
    }

    void closeAllPositions(const std::string& tradingSymbol,
			   const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice)
    {
      //std::cout << "Closing all positions for symbol "+tradingSymbol +" on date " << exitDate << std::endl;
      std::shared_ptr<InstrumentPosition<Prec>> pos = findExistingInstrumentPosition (tradingSymbol);
      pos->closeAllPositions(exitDate, exitPrice);
    }

    void closeUnitPosition(const std::string& tradingSymbol,
			   const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice,
			   uint32_t unitNumber)
    {
      std::shared_ptr<InstrumentPosition<Prec>> pos = findExistingInstrumentPosition (tradingSymbol);
      pos->closeUnitPosition(exitDate, exitPrice, unitNumber);
    }

    uint32_t getNumPositionUnits(const std::string& symbol) const
    {
      std::shared_ptr<InstrumentPosition<Prec>> pos = findExistingInstrumentPosition (symbol);
      return pos->getNumPositionUnits ();
    }

    std::shared_ptr<TradingPosition<Prec>>
    getTradingPosition (const std::string& symbol, uint32_t unitNumber) const
    {
      std::shared_ptr<InstrumentPosition<Prec>> pos = findExistingInstrumentPosition (symbol);
      
      return *(pos->getInstrumentPosition(unitNumber));
    }

    
  private:
    ConstInstrumentPositionIterator findExistingSymbol (const std::string& symbol) const
    {
      ConstInstrumentPositionIterator pos = mInstrumentPositions.find (symbol);
      if (pos != endInstrumentPositions())
	return pos;
      else
	throw InstrumentPositionManagerException("InstrumentPositionManager::addInstrument - trading symbol not found");
    }

    const std::shared_ptr<InstrumentPosition<Prec>>& 
    findExistingInstrumentPosition (const std::string symbol) const
    {
      ConstInstrumentPositionIterator pos = findExistingSymbol (symbol);
      return pos->second;
    }
    
    const std::shared_ptr<InstrumentPosition<Prec>>&
    getInstrumentPositionPtr(const std::string& tradingSymbol) const
    {
      ConstInstrumentPositionIterator pos = findExistingSymbol (tradingSymbol);
      return pos->second;
    }

  private:
    std::map<std::string, std::shared_ptr<InstrumentPosition<Prec>>> mInstrumentPositions;
  };
}

#endif
