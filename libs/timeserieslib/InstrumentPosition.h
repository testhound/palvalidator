// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __INSTRUMENT_POSITION_H
#define __INSTRUMENT_POSITION_H 1

#include <memory>
#include <vector>
#include <cstdint>
#include "ThrowAssert.hpp"
#include "TradingPosition.h"

namespace mkc_timeseries
{
  template <int Prec> class InstrumentPosition;

  template <int Prec> class InstrumentPositionState
  {
    using Decimal = decimal<Prec>;

  public:
    typedef typename std::vector<std::shared_ptr<TradingPosition<Prec>>>::const_iterator ConstInstrumentPositionIterator;

  public:
    InstrumentPositionState()
    {}

    virtual ~InstrumentPositionState()
    {}

    InstrumentPositionState (const InstrumentPositionState<Prec>& rhs)
    {}

    InstrumentPositionState<Prec>& 
    operator=(const InstrumentPositionState<Prec> &rhs)
    {
      return *this;
    }

    virtual bool isLongPosition() const = 0;
    virtual bool isShortPosition() const = 0;
    virtual bool isFlatPosition() const = 0;
    virtual uint32_t getNumPositionUnits() const = 0;

    virtual ConstInstrumentPositionIterator getInstrumentPosition (uint32_t unitNumber) const = 0;
    virtual void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar) = 0;
    virtual void addPosition(InstrumentPosition<Prec>* iPosition,
			     std::shared_ptr<TradingPosition<Prec>> position) = 0;
    virtual ConstInstrumentPositionIterator beginInstrumentPosition() const = 0;
    virtual ConstInstrumentPositionIterator endInstrumentPosition() const = 0;
    virtual void closeUnitPosition(InstrumentPosition<Prec>* iPosition,
				   const boost::gregorian::date exitDate,
				   const dec::decimal<Prec>& exitPrice,
				   uint32_t unitNumber) = 0;
    virtual void closeAllPositions(InstrumentPosition<Prec>* iPosition,
				   const boost::gregorian::date exitDate,
				   const dec::decimal<Prec>& exitPrice) = 0;
 
  
  };

  template <int Prec> class FlatInstrumentPositionState : public InstrumentPositionState<Prec>
  {
    using Decimal = decimal<Prec>;

  public:
    typedef typename InstrumentPositionState<Prec>::ConstInstrumentPositionIterator ConstInstrumentPositionIterator;
  public:
    FlatInstrumentPositionState (const FlatInstrumentPositionState<Prec>& rhs)
      : InstrumentPositionState<Prec>(rhs)
    {}

    FlatInstrumentPositionState<Prec>& 
    operator=(const FlatInstrumentPositionState<Prec> &rhs) 
    {
       if (this == &rhs)
	return *this;

      InstrumentPositionState<Prec>::operator=(rhs);
      return *this;
    }

    ~FlatInstrumentPositionState()
    {}

    static std::shared_ptr<FlatInstrumentPositionState<Prec>> getInstance()
    {
      return mInstance;
    }

    uint32_t getNumPositionUnits() const
    {
      return 0;
    }

    bool isLongPosition() const
    {
      return false;
    }

    bool isShortPosition() const
    {
      return false;
    }

    bool isFlatPosition() const
    {
      return true;
    }

    void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: addBar - no positions available in flat state");
    }


    ConstInstrumentPositionIterator getInstrumentPosition (uint32_t unitNumber) const
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: getInstrumentPosition - no positions avaialble in flat state");
    }

    void addPosition(InstrumentPosition<Prec>* iPosition,
		     std::shared_ptr<TradingPosition<Prec>> position);

    

    ConstInstrumentPositionIterator beginInstrumentPosition() const
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: beginInstrumentPosition - no positions avaialble in flat state");
    }

    ConstInstrumentPositionIterator endInstrumentPosition() const
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: endInstrumentPosition - no positions avaialble in flat state");
    }

    void closeUnitPosition(InstrumentPosition<Prec>* iPosition,
			   const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice,
			   uint32_t unitNumber)
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: closeUnitPosition - no positions avaialble in flat state");
    }

    void closeAllPositions(InstrumentPosition<Prec>* iPosition,
			   const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice)
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: closeAllPositions - no positions avaialble in flat state");
    }

  private:
    FlatInstrumentPositionState()
      : InstrumentPositionState<Prec>()
    {}

  private:
    static std::shared_ptr<FlatInstrumentPositionState<Prec>> mInstance;
  };

  template <int Prec> std::shared_ptr<FlatInstrumentPositionState<Prec>> FlatInstrumentPositionState<Prec>::mInstance(new FlatInstrumentPositionState<Prec>());

  //
  // class InMarketPositionState
  //

  template <int Prec> class InMarketPositionState : public InstrumentPositionState<Prec>
  {
    using Decimal = decimal<Prec>;

  public:
    typedef typename InstrumentPositionState<Prec>::ConstInstrumentPositionIterator ConstInstrumentPositionIterator;
  protected:
    InMarketPositionState(std::shared_ptr<TradingPosition<Prec>> position)
      : InstrumentPositionState<Prec>(),
	mTradingPositionUnits()
    {
      this->addPositionCommon(position);
    }

  public:
    InMarketPositionState (const InMarketPositionState<Prec>& rhs)
      : InstrumentPositionState<Prec>(rhs),
	mTradingPositionUnits(rhs.mTradingPositionUnits)
    {}

    virtual ~InMarketPositionState()
    {}

    InMarketPositionState<Prec>& 
    operator=(const InMarketPositionState<Prec> &rhs) 
    {
      if (this == &rhs)
	return *this;
      
      InstrumentPositionState<Prec>::operator=(rhs);
      mTradingPositionUnits = rhs.mTradingPositionUnits;
    }

    uint32_t getNumPositionUnits() const
    {
      return mTradingPositionUnits.size();
    }

    ConstInstrumentPositionIterator
    getInstrumentPosition (uint32_t unitNumber) const
    {
      checkUnitNumber (unitNumber);

      // We subtract one because the stl vector starts at 0, but unit numbers start at 1
      return (this->beginInstrumentPosition() + (unitNumber - 1));
      //return mTradingPositionUnits.at (unitNumber - 1);
    }

    void addPositionCommon(std::shared_ptr<TradingPosition<Prec>> position)
    {
      if (position->isPositionClosed())
	throw InstrumentPositionException ("InstrumentPosition: cannot add a closed position");

      mTradingPositionUnits.push_back (position);
    }

    void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      ConstInstrumentPositionIterator it = this->beginInstrumentPosition();

      for (; it != this->endInstrumentPosition(); it++)
	{
	  // Only add a date that is after the entry date. We
	  // already added the first bar when we created the position

	  if (entryBar.getDateValue() > (*it)->getEntryDate())
	    (*it)->addBar(entryBar);
	}
    }

    ConstInstrumentPositionIterator beginInstrumentPosition() const
    {
      return  mTradingPositionUnits.begin();
    }

    ConstInstrumentPositionIterator endInstrumentPosition() const
    {
      return  mTradingPositionUnits.end();
    }

    void closeUnitPosition(InstrumentPosition<Prec>* iPosition,
			   const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice,
			   uint32_t unitNumber)
    {
      ConstInstrumentPositionIterator it = getInstrumentPosition (unitNumber);
      if (it == this->endInstrumentPosition())
	throw InstrumentPositionException ("InMarketPositionState: closeUnitPosition -cannot find unit number to close position");

      if ((*it)->isPositionOpen())
	{
	  (*it)->ClosePosition (exitDate, exitPrice);
	  mTradingPositionUnits.erase (it);
	}
      else
	throw InstrumentPositionException ("InMarketPositionState: closeUnitPosition - unit already closed");

      if (getNumPositionUnits() == 0)
	iPosition->ChangeState (FlatInstrumentPositionState<Prec>::getInstance());
 
    }

    void closeAllPositions(InstrumentPosition<Prec>* iPosition,
			   const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice)
    {
      ConstInstrumentPositionIterator it = beginInstrumentPosition();
      for (; it != this->endInstrumentPosition(); it++)
	{
	  if ((*it)->isPositionOpen())
	    (*it)->ClosePosition (exitDate, exitPrice);
	}

      mTradingPositionUnits.clear();
      iPosition->ChangeState (FlatInstrumentPositionState<Prec>::getInstance());
    }

  private:
    void checkUnitNumber (uint32_t unitNumber) const
    {
      if (unitNumber > getNumPositionUnits())
	throw InstrumentPositionException ("InstrumentPosition:getInstrumentPosition: unitNumber " + std::to_string (unitNumber) + " is out range");

      if (unitNumber == 0)
	throw InstrumentPositionException ("InstrumentPosition:getInstrumentPosition: unitNumber - unit numbers start at one");
    }

  private:
    std::vector<std::shared_ptr<TradingPosition<Prec>>> mTradingPositionUnits;
  };

  //
  // class LongInstrumentPositionState
  //

  template <int Prec> class LongInstrumentPositionState : public InMarketPositionState<Prec>
  {
  public:
    LongInstrumentPositionState(std::shared_ptr<TradingPosition<Prec>> position)
      : InMarketPositionState<Prec>(position)
    {}

    LongInstrumentPositionState (const LongInstrumentPositionState<Prec>& rhs)
      : InMarketPositionState<Prec>(rhs)
    {}

    ~ LongInstrumentPositionState()
    {}

    LongInstrumentPositionState<Prec>& 
    operator=(const LongInstrumentPositionState<Prec> &rhs) 
    {
      if (this == &rhs)
	return *this;
      
      InMarketPositionState<Prec>::operator=(rhs);
      return *this;
    }

    void addPosition(InstrumentPosition<Prec>* iPosition,
		     std::shared_ptr<TradingPosition<Prec>> position)
    {
      if (position->isLongPosition())
	this->addPositionCommon(position);
      else
	throw InstrumentPositionException ("InstrumentPosition: cannot add short position unit to long position");
    }

    bool isLongPosition() const
    {
      return true;
    }

    bool isShortPosition() const
    {
      return false;
    }

    bool isFlatPosition() const
    {
      return false;
    }
  };

  template <int Prec> class ShortInstrumentPositionState : public InMarketPositionState<Prec>
  {
  public:
    ShortInstrumentPositionState(std::shared_ptr<TradingPosition<Prec>> position)
      : InMarketPositionState<Prec>(position)
    {}

    ShortInstrumentPositionState (const ShortInstrumentPositionState<Prec>& rhs)
      : InMarketPositionState<Prec>(rhs)
    {}

    ShortInstrumentPositionState<Prec>& 
    operator=(const ShortInstrumentPositionState<Prec> &rhs) 
    {
      if (this == &rhs)
	return *this;
      
      InMarketPositionState<Prec>::operator=(rhs);
      return *this;
    }

    void addPosition(InstrumentPosition<Prec>* iPosition,
		     std::shared_ptr<TradingPosition<Prec>> position)
    {
      if (position->isShortPosition())
	this->addPositionCommon(position);
      else
	throw InstrumentPositionException ("InstrumentPosition: cannot add long position unit to short position");
    }
    bool isLongPosition() const
    {
      return false;
    }

    bool isShortPosition() const
    {
      return true;
    }

    bool isFlatPosition() const
    {
      return false;
    }
  };

  // An InstrumentPosition is made up of one
  // or more TradingPosition objects
  //
  // Each TradingPosition object is assigned a
  // unit number (starting at 1)
  //
  // This will allow closing a single unit at
  // a time or closing all units at the same
  // time.

  template <int Prec> class InstrumentPosition
  {
    using Decimal = decimal<Prec>;

  public:
    typedef typename InstrumentPositionState<Prec>::ConstInstrumentPositionIterator ConstInstrumentPositionIterator;

  public:
    InstrumentPosition (const std::string& instrumentSymbol)
      : mInstrumentSymbol(instrumentSymbol),
	mInstrumentPositionState(FlatInstrumentPositionState<Prec>::getInstance())
    {}

    InstrumentPosition (const InstrumentPosition<Prec>& rhs)
      : mInstrumentSymbol(rhs.mInstrumentSymbol),
	mInstrumentPositionState(rhs.mInstrumentPositionState)
    {}

    InstrumentPosition<Prec>& 
    operator=(const InstrumentPosition<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      mInstrumentSymbol = rhs.mInstrumentSymbol;
      mInstrumentPositionState = rhs.mInstrumentPositionState;

      return *this;
    }

    ~InstrumentPosition()
    {}

    const std::string& getInstrumentSymbol() const
    {
      return mInstrumentSymbol;
    }

    bool isLongPosition() const
    {
      return mInstrumentPositionState->isLongPosition();
    }

    bool isShortPosition() const
    {
      return mInstrumentPositionState->isShortPosition();
    }

    bool isFlatPosition() const
    {
      return mInstrumentPositionState->isFlatPosition();
    }

    uint32_t getNumPositionUnits() const
    {
      return mInstrumentPositionState->getNumPositionUnits();
    }

    ConstInstrumentPositionIterator
    getInstrumentPosition (uint32_t unitNumber) const
    {
      return mInstrumentPositionState->getInstrumentPosition(unitNumber);
    }

    const dec::decimal<Prec>& getFillPrice() const
    {
      return getFillPrice(1);
    }

    const dec::decimal<Prec>& getFillPrice(uint32_t unitNumber) const
    {
      ConstInstrumentPositionIterator pos = getInstrumentPosition(unitNumber);
      return (*pos)->getEntryPrice();
    }

    void setRMultipleStop (const dec::decimal<Prec>& riskStop) const
    {
      this->setRMultipleStop (riskStop, 1);
    }

    void setRMultipleStop (const dec::decimal<Prec>& riskStop, uint32_t unitNumber) const
    {
      ConstInstrumentPositionIterator pos = getInstrumentPosition(unitNumber);
      (*pos)->setRMultipleStop (riskStop);
    }

    void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      mInstrumentPositionState->addBar(entryBar);
    }

    void addPosition(std::shared_ptr<TradingPosition<Prec>> position)
    {
      if (position->isPositionClosed())
	throw InstrumentPositionException ("InstrumentPosition: cannot add a closed position");

      if (getInstrumentSymbol() != position->getTradingSymbol())
	throw InstrumentPositionException ("InstrumentPosition: cannot add position with different symbols");

      mInstrumentPositionState->addPosition (this, position);
    }

    ConstInstrumentPositionIterator beginInstrumentPosition() const
    {
      return mInstrumentPositionState->beginInstrumentPosition();
    }

    ConstInstrumentPositionIterator endInstrumentPosition() const
    {
      return  mInstrumentPositionState->endInstrumentPosition();
    }

    TradingVolume getVolumeInAllUnits() const
    {
      if (isFlatPosition() == false)
	{
	  volume_t totalVolume = 0;
	  ConstInstrumentPositionIterator it = beginInstrumentPosition();

	  for (; it != endInstrumentPosition(); it++)
	    totalVolume += (*it)->getTradingUnits().getTradingVolume();

	  if (totalVolume > 0)
	    {
	      it = beginInstrumentPosition();
	      TradingVolume::VolumeUnit volUnit = 
		(*it)->getTradingUnits().getVolumeUnits();

	      return TradingVolume(totalVolume, volUnit);
	    }
	  // Need to throw an exception here
	  else
	    throw InstrumentPositionException ("InstrumentPosition::getVolumeInAllUnits - Cannot get volume when position is flat");
	}
    }

    void closeUnitPosition(const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice,
			   uint32_t unitNumber)
    {
      mInstrumentPositionState->closeUnitPosition(this, exitDate, exitPrice, unitNumber);
    }

    void closeAllPositions(const boost::gregorian::date exitDate,
			   const dec::decimal<Prec>& exitPrice)
    {
      mInstrumentPositionState->closeAllPositions(this, exitDate, exitPrice);
    }

  private:
    void ChangeState (std::shared_ptr<InstrumentPositionState<Prec>> newState)
    {
      mInstrumentPositionState = newState;
    }

    friend class FlatInstrumentPositionState<Prec>;
    friend class LongInstrumentPositionState<Prec>;
    friend class ShortInstrumentPositionState<Prec>;
    friend class InMarketPositionState<Prec>;

  private:
    std::string mInstrumentSymbol;
    std::shared_ptr<InstrumentPositionState<Prec>> mInstrumentPositionState;
  };

  template <int Prec>
  inline void FlatInstrumentPositionState<Prec>::addPosition(InstrumentPosition<Prec>* iPosition,
							     std::shared_ptr<TradingPosition<Prec>> position)
    {
      if (position->isLongPosition())
	iPosition->ChangeState (std::make_shared<LongInstrumentPositionState<Prec>>(position));
      else if (position->isShortPosition())
	iPosition->ChangeState (std::make_shared<ShortInstrumentPositionState<Prec>>(position));
      else
	throw InstrumentPositionException ("FlatInstrumentPositionState<Prec>::addPosition: position is neither long or short");
      
    }
}

#endif
