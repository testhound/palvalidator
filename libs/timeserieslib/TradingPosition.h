// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TRADING_POSITION_H
#define __TRADING_POSITION_H 1

#include <exception>
#include <memory>
#include <functional>
#include <map>
#include <list>
#include <cstdint>
#include "TradingPositionException.h"
#include "TimeSeriesEntry.h"
#include "PercentNumber.h"
#include "ProfitTarget.h"
#include "StopLoss.h"
#include "DecimalConstants.h"
#include <atomic>

using dec::decimal;
using namespace boost::gregorian;

namespace mkc_timeseries
{
  template <class Decimal> class TradingPosition;

  template <class Decimal>
  Decimal calculateTradeReturn (const Decimal& referencePrice, 
					  const Decimal& secondPrice)
    {
      return ((secondPrice - referencePrice) / referencePrice);
    }

  template <class Decimal>
    Decimal calculatePercentReturn (const Decimal& referencePrice, 
					  const Decimal& secondPrice)
    {
return (calculateTradeReturn<Decimal>(referencePrice, secondPrice) * DecimalConstants<Decimal>::DecimalOneHundred);
    }

  template <class Decimal> class OpenPositionBar
  {
  public:
    explicit OpenPositionBar (OHLCTimeSeriesEntry<Decimal> entry)
      : mEntry(entry)
    {}

    OpenPositionBar(const OpenPositionBar<Decimal>& rhs) 
      : mEntry(rhs.mEntry)
    {}

    OpenPositionBar<Decimal>& 
    operator=(const OpenPositionBar<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mEntry = rhs.mEntry;
      return *this;
    }

    ~OpenPositionBar()
      {}

    const boost::gregorian::date& getDate() const
    {
      return mEntry.getDateValue();
    }

    const Decimal& getOpenValue() const
    {
      return mEntry.getOpenValue();
    }

    const Decimal& getHighValue() const
    {
      return mEntry.getHighValue();
    }

    const Decimal& getLowValue() const
    {
      return mEntry.getLowValue();

    }

    const Decimal& getCloseValue() const
    {
      return mEntry.getCloseValue();
    }

    const volume_t getVolume() const
    {
      return mEntry.getVolume();
    }

    const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry() const
    {
      return mEntry;
    }

  private:
    OHLCTimeSeriesEntry<Decimal> mEntry;
  };

  template <class Decimal>
  bool operator==(const OpenPositionBar<Decimal>& lhs, const OpenPositionBar<Decimal>& rhs)
  {
    return (*lhs.getTimeSeriesEntry() == *rhs.getTimeSeriesEntry());
  }

  template <class Decimal>
  bool operator!=(const OpenPositionBar<Decimal>& lhs, const OpenPositionBar<Decimal>& rhs)
  { 
    return !(lhs == rhs); 
  }



  // class OpenPositionHistory
  template <class Decimal> class OpenPositionHistory
  {
  public:
    typedef typename std::map<TimeSeriesDate, OpenPositionBar<Decimal>>::iterator PositionBarIterator;
    typedef typename std::map<TimeSeriesDate, OpenPositionBar<Decimal>>::const_iterator ConstPositionBarIterator;

    explicit OpenPositionHistory(OHLCTimeSeriesEntry<Decimal> entryBar) :
      mPositionBarHistory()
    {
      addFirstBar(entryBar);
    }

    OpenPositionHistory(const OpenPositionHistory<Decimal>& rhs) 
      : mPositionBarHistory(rhs.mPositionBarHistory)
    {}

    OpenPositionHistory<Decimal>& 
    operator=(const OpenPositionHistory<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mPositionBarHistory = rhs.mPositionBarHistory;
      return *this;
    }

    ~OpenPositionHistory()
    {}

    void addBar(const OpenPositionBar<Decimal>& entry)
    {
      boost::gregorian::date d = entry.getDate();
      PositionBarIterator pos = mPositionBarHistory.find (d);

      if (pos ==  mPositionBarHistory.end())
	{
	   mPositionBarHistory.insert(std::make_pair(d, entry));
	}
      else
	throw std::domain_error(std::string("OpenPositionHistory:" +boost::gregorian::to_simple_string(d) + std::string(" date already exists")));
    }

    unsigned int numBarsInPosition() const
    {
      return mPositionBarHistory.size();
    }

    OpenPositionHistory::PositionBarIterator beginPositionBarHistory()
    {
      return mPositionBarHistory.begin();
    }

    OpenPositionHistory::PositionBarIterator endPositionBarHistory()
    {
      return mPositionBarHistory.end();
    }

    OpenPositionHistory::ConstPositionBarIterator beginPositionBarHistory() const
    {
      return mPositionBarHistory.begin();
    }

    OpenPositionHistory::ConstPositionBarIterator endPositionBarHistory() const
    {
      return mPositionBarHistory.end();
    }

    const TimeSeriesDate& getFirstDate() const
    {
      if (numBarsInPosition() > 0)
	{
	  OpenPositionHistory::ConstPositionBarIterator it = beginPositionBarHistory();
	  return it->first;
	}
      else
	throw std::domain_error(std::string("OpenPositionHistory:getPositionFirstDate: no bars in position "));
    }

    const TimeSeriesDate& getLastDate() const
    {
      if (numBarsInPosition() > 0)
	{
	  OpenPositionHistory::ConstPositionBarIterator it = endPositionBarHistory();
	  it--;
	  return it->first;
	}
      else
	throw std::domain_error(std::string("OpenPositionHistory:getPositionLastDate: no bars in position "));
    }

    const Decimal& getLastClose() const
    {
      if (numBarsInPosition() > 0)
	{
	  OpenPositionHistory::ConstPositionBarIterator it = endPositionBarHistory();
	  it--;
	  return it->second.getCloseValue();
	}
      else
	throw std::domain_error(std::string("OpenPositionHistory:getLastClose: no bars in position "));
    }

  private:
    void addFirstBar(const OHLCTimeSeriesEntry<Decimal> entry)
    {
      addBar(OpenPositionBar<Decimal>(entry));
    }

  private:
    std::map<TimeSeriesDate, OpenPositionBar<Decimal>> mPositionBarHistory;
  };

  template <class Decimal> class TradingPositionState
  {
  public:
    typedef typename OpenPositionHistory<Decimal>::PositionBarIterator PositionBarIterator;
    typedef typename OpenPositionHistory<Decimal>::ConstPositionBarIterator ConstPositionBarIterator;

    TradingPositionState()
    {}

    virtual ~TradingPositionState()
    {}

    TradingPositionState(const TradingPositionState<Decimal>& rhs)
    {}

    TradingPositionState<Decimal>& 
    operator=(const TradingPositionState<Decimal> &rhs)
    {}

    virtual bool isPositionOpen() const = 0;
    virtual bool isPositionClosed() const = 0;
    virtual const TimeSeriesDate& getEntryDate() const = 0;
    virtual const Decimal& getEntryPrice() const = 0;
    virtual const Decimal& getExitPrice() const = 0;
    virtual const boost::gregorian::date& getExitDate() const = 0;
    virtual void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar) = 0;
    virtual const TradingVolume& getTradingUnits() const = 0;
    virtual unsigned int getNumBarsInPosition() const = 0;
    virtual unsigned int getNumBarsSinceEntry() const = 0;
    virtual const Decimal& getLastClose() const = 0;
    virtual Decimal getPercentReturn() const = 0;
    virtual Decimal getTradeReturn() const = 0;
    virtual Decimal getTradeReturnMultiplier() const = 0;
    virtual bool isWinningPosition() const = 0;
    virtual bool isLosingPosition() const = 0;
    // NOTE: To implement these in the ClosePositonState pass the open position to closed
    // position at creation time
    virtual TradingPositionState::ConstPositionBarIterator beginPositionBarHistory() const = 0;
    virtual TradingPositionState::ConstPositionBarIterator endPositionBarHistory() const = 0;
    virtual void ClosePosition (TradingPosition<Decimal>* position,
				std::shared_ptr<TradingPositionState<Decimal>> openPosition,
				const boost::gregorian::date exitDate,
				const Decimal& exitPrice) = 0;
  };
  
  template <class Decimal>
  class OpenPosition : public TradingPositionState<Decimal>
  {
  public:
    typedef typename OpenPositionHistory<Decimal>::PositionBarIterator PositionBarIterator;
    typedef typename OpenPositionHistory<Decimal>::ConstPositionBarIterator ConstPositionBarIterator;

    OpenPosition (const Decimal& entryPrice, 
		  OHLCTimeSeriesEntry<Decimal> entryBar,
		  const TradingVolume& unitsInPosition) 
      : TradingPositionState<Decimal>(),
      mEntryPrice(entryPrice),
      mEntryDate (entryBar.getDateValue()),
      mUnitsInPosition(unitsInPosition),
      mPositionBarHistory (entryBar),
      mBarsInPosition(1),
      mNumBarsSinceEntry(0)	
    {
      if (entryPrice <= DecimalConstants<Decimal>::DecimalZero)
	throw TradingPositionException (std::string("OpenPosition constructor: entry price < 0"));
    }

    OpenPosition(const OpenPosition<Decimal>& rhs) 
      : TradingPositionState<Decimal>(rhs),
      mEntryPrice(rhs.mEntryPrice),
      mEntryDate (rhs.mEntryDate),
      mUnitsInPosition(rhs.mUnitsInPosition),
      mPositionBarHistory (rhs.mPositionBarHistory),
      mBarsInPosition (rhs.mBarsInPosition),
      mNumBarsSinceEntry(rhs.mNumBarsSinceEntry)
    {}

    OpenPosition<Decimal>& 
    operator=(const OpenPosition<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPositionState<Decimal>::operator=(rhs);

      mEntryPrice = rhs.mEntryPrice;
      mEntryDate  = rhs.mEntryDate;
      mUnitsInPosition = rhs.mUnitsInPosition;
      mPositionBarHistory  = rhs.mPositionBarHistory;
      mBarsInPosition  = rhs.mBarsInPosition;
      mNumBarsSinceEntry = rhs.mNumBarsSinceEntry;
      return *this;
    }

    virtual ~OpenPosition()
    {}

    bool isPositionOpen() const
    {
      return true;
    }

    bool isPositionClosed() const
    {
      return false;
    }

    virtual void addBar(const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      addBar(OpenPositionBar<Decimal>(entryBar));
    }

    const Decimal& getEntryPrice() const
    {
      return mEntryPrice;
    }

    const TimeSeriesDate& getEntryDate() const
    {
      return mEntryDate;
    }

    const Decimal& getExitPrice() const
    {
      throw TradingPositionException ("No exit price for open position");
    }

    const boost::gregorian::date& getExitDate() const
    {
      throw TradingPositionException ("No exit date for open position");
    }

    const TradingVolume& getTradingUnits() const
    {
      return mUnitsInPosition;
    }

    unsigned int getNumBarsInPosition() const
    {
      return mBarsInPosition;
    }

    unsigned int getNumBarsSinceEntry() const
    {
      return mNumBarsSinceEntry;
    }

    const Decimal& getLastClose() const
    {
      return mPositionBarHistory.getLastClose();
    }

    OpenPosition::PositionBarIterator beginPositionBarHistory()
    {
      return mPositionBarHistory.beginPositionBarHistory();
    }

    OpenPosition::PositionBarIterator endPositionBarHistory()
    {
      return mPositionBarHistory.endPositionBarHistory();
    }

    OpenPosition::ConstPositionBarIterator beginPositionBarHistory() const
    {
      return mPositionBarHistory.beginPositionBarHistory();
    }

    OpenPosition::ConstPositionBarIterator endPositionBarHistory() const
    {
      return mPositionBarHistory.endPositionBarHistory();
    }

  private:
    void addBar(const OpenPositionBar<Decimal>& positionBar)
    {
      mPositionBarHistory.addBar(positionBar);
      mBarsInPosition++;
      mNumBarsSinceEntry++;
    }

  private:
    Decimal mEntryPrice;
    TimeSeriesDate mEntryDate;
    TradingVolume mUnitsInPosition;
    OpenPositionHistory<Decimal> mPositionBarHistory;
    unsigned int mBarsInPosition;
    unsigned int mNumBarsSinceEntry;
  };

  template <class Decimal>
  class OpenLongPosition : public OpenPosition<Decimal>
  {
  public:
    OpenLongPosition (const Decimal& entryPrice, 
		      OHLCTimeSeriesEntry<Decimal> entryBar,
		      const TradingVolume& unitsInPosition)
      : OpenPosition<Decimal>(entryPrice, entryBar, unitsInPosition)
    {}

    OpenLongPosition(const OpenLongPosition<Decimal>& rhs) 
      : OpenPosition<Decimal>(rhs)
    {}

    OpenLongPosition<Decimal>& 
    operator=(const OpenLongPosition<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      OpenPosition<Decimal>::operator=(rhs);
      return *this;
    }

    void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      OpenPosition<Decimal>::addBar(entryBar);
    }

    Decimal getPercentReturn() const
    {
      return (calculatePercentReturn (OpenPosition<Decimal>::getEntryPrice(), 
				      OpenPosition<Decimal>::getLastClose()));
    }

    Decimal getTradeReturn() const
    {
      return (calculateTradeReturn (OpenPosition<Decimal>::getEntryPrice(), 
				      OpenPosition<Decimal>::getLastClose()));
    }

    Decimal getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Decimal>::DecimalOne + getTradeReturn());
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Decimal>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    void ClosePosition (TradingPosition<Decimal>* position,
			std::shared_ptr<TradingPositionState<Decimal>> openPosition,
			const boost::gregorian::date exitDate,
			const Decimal& exitPrice);
  };

  template <class Decimal>
  class OpenShortPosition : public OpenPosition<Decimal>
  {
  public:
    OpenShortPosition (const Decimal& entryPrice, 
		       OHLCTimeSeriesEntry<Decimal>& entryBar,
		       const TradingVolume& unitsInPosition)
      : OpenPosition<Decimal>(entryPrice, entryBar, unitsInPosition)
    {}

    OpenShortPosition(const OpenShortPosition<Decimal>& rhs) 
      : OpenPosition<Decimal>(rhs)
    {}

    OpenShortPosition<Decimal>& 
    operator=(const OpenShortPosition<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      OpenPosition<Decimal>::operator=(rhs);
      return *this;
    }

    void addBar(const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      OpenPosition<Decimal>::addBar(entryBar);
    }

    Decimal getTradeReturn() const
    {
      return -(calculateTradeReturn (OpenPosition<Decimal>::getEntryPrice(), 
				      OpenPosition<Decimal>::getLastClose()));
    }

    Decimal getPercentReturn() const
    {
      return -(calculatePercentReturn (OpenPosition<Decimal>::getEntryPrice(), 
				       OpenPosition<Decimal>::getLastClose()));

    }

    Decimal getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Decimal>::DecimalOne + getTradeReturn());
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Decimal>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    void ClosePosition (TradingPosition<Decimal>* position,
			std::shared_ptr<TradingPositionState<Decimal>> openPosition,
			const boost::gregorian::date exitDate,
			const Decimal& exitPrice);
  };


  template <class Decimal> class ClosedPosition : public TradingPositionState<Decimal>
  {
  public:
    ClosedPosition (std::shared_ptr<TradingPositionState<Decimal>> openPosition,
		    const boost::gregorian::date exitDate,
		    const Decimal& exitPrice) 
      : TradingPositionState<Decimal>(),
	mOpenPosition (openPosition),
	mExitDate(exitDate),
	mExitPrice(exitPrice)
    {
      if (exitDate < openPosition->getEntryDate())
	throw std::domain_error (std::string("ClosedPosition: exit Date" +to_simple_string (exitDate) +" cannot occur before entry date " +to_simple_string (openPosition->getEntryDate())));
    }

    ClosedPosition(const ClosedPosition<Decimal>& rhs) 
      : TradingPositionState<Decimal>(rhs),
	mOpenPosition (rhs.mOpenPosition),
	mExitDate(rhs.mExitDate),
	mExitPrice(rhs.mExitPrice)
    {}

    ClosedPosition<Decimal>& 
    operator=(const ClosedPosition<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPositionState<Decimal>::operator=(rhs);
      mOpenPosition = rhs.mOpenPosition;
      mExitDate = rhs.mExitDate;
      mExitPrice = rhs.mExitPrice;
      return *this;
    }

    virtual ~ClosedPosition()
    {}

    bool isPositionOpen() const
    {
      return false;
    }

    bool isPositionClosed() const
    {
      return true;
    }

    const boost::gregorian::date& getEntryDate() const
    {
      return mOpenPosition->getEntryDate();
    }

    const Decimal& getEntryPrice() const
    {
      return mOpenPosition->getEntryPrice();
    }

    const boost::gregorian::date& getExitDate() const
    {
      return mExitDate;
    }

    const Decimal& getExitPrice() const
    {
      return mExitPrice;
    }

    const TradingVolume& getTradingUnits() const
    {
      return mOpenPosition->getTradingUnits();
    }

    unsigned int getNumBarsInPosition() const
    {
      return mOpenPosition->getNumBarsInPosition();
    }

    unsigned int getNumBarsSinceEntry() const
    {
      return mOpenPosition->getNumBarsSinceEntry();
    }

    const Decimal& getLastClose() const
    {
      return mOpenPosition->getLastClose();
    }

    void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      throw TradingPositionException ("Cannot add bar to a closed position");
    }

    typename TradingPositionState<Decimal>::ConstPositionBarIterator beginPositionBarHistory() const
    {
      return mOpenPosition->beginPositionBarHistory();
    }

    typename TradingPositionState<Decimal>::ConstPositionBarIterator endPositionBarHistory() const
    {
      return mOpenPosition->endPositionBarHistory();
    }

  private:
    std::shared_ptr<TradingPositionState<Decimal>> mOpenPosition;
    boost::gregorian::date mExitDate;
    Decimal mExitPrice;
  };


  template <class Decimal>
  bool operator==(const ClosedPosition<Decimal>& lhs, const ClosedPosition<Decimal>& rhs)
  {
    return (lhs.getEntryDate() == rhs.getEntryDate() &&
	    lhs.getEntryPrice() == rhs.getEntryPrice() &&
	    lhs.getExitDate() == rhs.getExitDate() &&
	    lhs.getExitPrice() == rhs.getExitPrice() &&
	    lhs.getUnitsInPosition() == rhs.getUnitsInPosition() &&
	    lhs.isLongPosition() == rhs.isLongPosition() &&
	    lhs.isShortPosition() == rhs.isShortPosition());
  }

  template <class Decimal>
  bool operator!=(const ClosedPosition<Decimal>& lhs, const ClosedPosition<Decimal>& rhs)
  { 
    return !(lhs == rhs); 
  }

  template <class Decimal>
  class ClosedLongPosition : public ClosedPosition<Decimal>
  {
  public:
    ClosedLongPosition (std::shared_ptr<TradingPositionState<Decimal>> openPosition,
			const boost::gregorian::date exitDate,
			const Decimal& exitPrice)
      : ClosedPosition<Decimal>(openPosition, exitDate, exitPrice)
    {}

    ClosedLongPosition(const ClosedLongPosition<Decimal>& rhs) 
      : ClosedPosition<Decimal>(rhs)
    {}

    ClosedLongPosition<Decimal>& 
    operator=(const ClosedLongPosition<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      ClosedPosition<Decimal>::operator=(rhs);
      return *this;
    }

    ~ClosedLongPosition()
    {}

     Decimal getPercentReturn() const
    {
      return (calculatePercentReturn (ClosedPosition<Decimal>::getEntryPrice(), 
				      ClosedPosition<Decimal>::getExitPrice()));
    }

    Decimal getTradeReturn() const
    {
      return (calculateTradeReturn (ClosedPosition<Decimal>::getEntryPrice(), 
				    ClosedPosition<Decimal>::getExitPrice()));
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Decimal>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    Decimal getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Decimal>::DecimalOne + ClosedLongPosition<Decimal>::getTradeReturn());
    }

    void ClosePosition (TradingPosition<Decimal>* position,
			std::shared_ptr<TradingPositionState<Decimal>> openPosition,
			const boost::gregorian::date exitDate,
			const Decimal& exitPrice);
  };


  template <class Decimal>
  class ClosedShortPosition : public ClosedPosition<Decimal>
  {
  public:
    ClosedShortPosition (std::shared_ptr<TradingPositionState<Decimal>> openPosition,
			const boost::gregorian::date exitDate,
			const Decimal& exitPrice)
      : ClosedPosition<Decimal>(openPosition, exitDate, exitPrice)
    {}

    ClosedShortPosition(const ClosedShortPosition<Decimal>& rhs) 
      : ClosedPosition<Decimal>(rhs)
    {}

    ClosedShortPosition<Decimal>& 
    operator=(const ClosedShortPosition<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      ClosedPosition<Decimal>::operator=(rhs);
      return *this;
    }

    ~ClosedShortPosition()
    {}

    Decimal getTradeReturn() const
    {
      return -(calculateTradeReturn (ClosedPosition<Decimal>::getEntryPrice(), 
				     (ClosedPosition<Decimal>::getExitPrice())));
    }

    Decimal getPercentReturn() const
    {
      return -(calculatePercentReturn (ClosedPosition<Decimal>::getEntryPrice(), 
				       ClosedPosition<Decimal>::getExitPrice()));
    }

    Decimal getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Decimal>::DecimalOne + ClosedShortPosition<Decimal>::getTradeReturn());
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Decimal>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    void ClosePosition (TradingPosition<Decimal>* position,
			std::shared_ptr<TradingPositionState<Decimal>> openPosition,
			const boost::gregorian::date exitDate,
			const Decimal& exitPrice);
  };

  template <class Decimal>
  class TradingPositionObserver
    {
    public:
     TradingPositionObserver()
      {}

      virtual ~TradingPositionObserver()
      {}

      virtual void PositionClosed (TradingPosition<Decimal> *aPosition) = 0;
  };

  template <class Decimal> class TradingPosition
  {
  public:
    typedef typename OpenPosition<Decimal>::ConstPositionBarIterator ConstPositionBarIterator;

    TradingPosition (const TradingPosition<Decimal>& rhs)
      : mTradingSymbol(rhs.mTradingSymbol),
      mPositionState(rhs.mPositionState),
      mPositionID(rhs.mPositionID),
      mObservers(rhs.mObservers),
      mRMultipleStop(rhs.mRMultipleStop),
      mRMultipleStopSet(rhs.mRMultipleStopSet)
    {}

    TradingPosition<Decimal>& 
    operator=(const TradingPosition<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mTradingSymbol = rhs.mTradingSymbol;
      mPositionState = rhs.mPositionState;
      mPositionID = rhs.mPositionID;
      mObservers = rhs.mObservers;
      mRMultipleStop = rhs.mRMultipleStop;
      mRMultipleStopSet = rhs.mRMultipleStopSet;

      return *this;
    }

    virtual ~TradingPosition()
    {}

    const std::string& getTradingSymbol() const
    {
      return mTradingSymbol;
    }

    uint32_t getPositionID() const
    {
      return mPositionID;
    }

    virtual bool isLongPosition() const = 0;
    virtual bool isShortPosition() const = 0;
    virtual Decimal getRMultiple() = 0;

    bool isPositionOpen() const
    {
      return mPositionState->isPositionOpen();
    }

    bool isPositionClosed() const
    {
      return mPositionState->isPositionClosed();
    }

    const TimeSeriesDate& getEntryDate() const
    {
      return mPositionState->getEntryDate();
    }

    const Decimal& getEntryPrice() const
    {
      return mPositionState->getEntryPrice();
    }

    const Decimal& getExitPrice() const
    {
      return mPositionState->getExitPrice();
    }

    const boost::gregorian::date& getExitDate() const
    {
      return mPositionState->getExitDate();
    }

    void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      mPositionState->addBar(entryBar);
    }

    const TradingVolume& getTradingUnits() const
    {
      return mPositionState->getTradingUnits();
    }

    unsigned int getNumBarsInPosition() const
    {
      return mPositionState->getNumBarsInPosition();
    }

    unsigned int getNumBarsSinceEntry() const
    {
      return mPositionState->getNumBarsSinceEntry();
    }

    const Decimal& getLastClose() const
    {
      return mPositionState->getLastClose();
    }

    Decimal getPercentReturn() const
    {
      return mPositionState->getPercentReturn();
    }

    Decimal getTradeReturn() const
    {
      return mPositionState->getTradeReturn();
    }

    Decimal getTradeReturnMultiplier() const
    {
      return mPositionState->getTradeReturnMultiplier();
    }

    bool isWinningPosition() const
    {
      return mPositionState->isWinningPosition();
    }

    bool isLosingPosition() const
    {
      return mPositionState->isLosingPosition();
    }

    // NOTE: To implement these in the ClosePositonState pass the open position to closed
    // position at creation time
    ConstPositionBarIterator beginPositionBarHistory() const
    {
      return mPositionState->beginPositionBarHistory();
    }

    ConstPositionBarIterator endPositionBarHistory() const
    {
      return mPositionState->endPositionBarHistory();
    }

    void ClosePosition (const boost::gregorian::date exitDate,
			const Decimal& exitPrice)
    {
      mPositionState->ClosePosition (this, mPositionState, exitDate, exitPrice);
      NotifyPositionClosed (this);
    }

    void addObserver (std::reference_wrapper<TradingPositionObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

    void setRMultipleStop(const Decimal& rMultipleStop)
    {
      if (rMultipleStop <= DecimalConstants<Decimal>::DecimalZero)
	throw TradingPositionException (std::string("TradingPosition:setRMultipleStop =< 0"));

      mRMultipleStop = rMultipleStop;
      mRMultipleStopSet = true;
    }

    bool RMultipleStopSet() const
    {
      return mRMultipleStopSet;
    }

    Decimal getRMultipleStop() const
    {
      return mRMultipleStop;
    }

  protected:
    TradingPosition(const std::string& tradingSymbol,
		    std::shared_ptr<TradingPositionState<Decimal>> positionState)
      : mTradingSymbol (tradingSymbol),
	mPositionState (positionState),
	mPositionID(++mPositionIDCount),
	mObservers(),
	mRMultipleStop(DecimalConstants<Decimal>::DecimalZero),
	mRMultipleStopSet(false)
    {}

    
  private:
    typedef typename std::list<std::reference_wrapper<TradingPositionObserver<Decimal>>>::const_iterator ConstObserverIterator;

    void ChangeState (std::shared_ptr<TradingPositionState<Decimal>> newState)
    {
      mPositionState = newState;
    }

    ConstObserverIterator beginObserverList() const
    {
      return mObservers.begin();
    }

    ConstObserverIterator endObserverList() const
    {
      return mObservers.end();
    }

    void NotifyPositionClosed (TradingPosition<Decimal> *aPosition)
    {
      ConstObserverIterator it = beginObserverList();
      for (; it != endObserverList(); it++)
	(*it).get().PositionClosed (aPosition);
    }

    friend class OpenLongPosition<Decimal>;
    friend class OpenShortPosition<Decimal>;

  private:
    std::string mTradingSymbol;
    std::shared_ptr<TradingPositionState<Decimal>> mPositionState;
    uint32_t mPositionID;
    static std::atomic<uint32_t> mPositionIDCount;
    std::list<std::reference_wrapper<TradingPositionObserver<Decimal>>> mObservers;
    Decimal mRMultipleStop;
    bool mRMultipleStopSet;
  };

  template <class Decimal> std::atomic<uint32_t>  TradingPosition<Decimal>::mPositionIDCount{0};

  template <class Decimal> 
  class TradingPositionLong : public TradingPosition<Decimal>
  {
  public:
    explicit TradingPositionLong (const std::string& tradingSymbol,
				  const Decimal& entryPrice, 
				  OHLCTimeSeriesEntry<Decimal> entryBar,
				  const TradingVolume& unitsInPosition)
      : TradingPosition<Decimal>(tradingSymbol,std::make_shared<OpenLongPosition<Decimal>>(entryPrice, entryBar, unitsInPosition))
    {}

    TradingPositionLong (const TradingPositionLong<Decimal>& rhs)
      : TradingPosition<Decimal>(rhs)
    {}

    TradingPositionLong<Decimal>& 
    operator=(const TradingPositionLong<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPosition<Decimal>::operator=(rhs);

      return *this;
    }

    ~TradingPositionLong()
    {}

    bool isLongPosition() const
    {
      return true;
    }

    bool isShortPosition() const
    {
      return false;
    }
    
    Decimal getRMultiple()
    {
      if (this->isPositionOpen())
	throw TradingPositionException("TradingPositionLong::getRMultiple - r multiple not available for open position");
      
      if (this->getRMultipleStop() <= DecimalConstants<Decimal>::DecimalZero)
 	throw TradingPositionException("TradingPositionLong::getRMultiple - r multiple not available because R multiple stop not set");

      Decimal exit = this->getExitPrice();
      Decimal entry = this->getEntryPrice();

      if (this->isWinningPosition())
	{
	  return ((exit - entry)/(entry - this->getRMultipleStop()));
	}
      else
	{
	  if (exit == this->getRMultipleStop())
	    return -exit/this->getRMultipleStop();
	  else
	    return -this->getRMultipleStop() / exit;
	}
    }
  };

  template <class Decimal> 
  class TradingPositionShort : public TradingPosition<Decimal>
  {
  public:
    explicit TradingPositionShort (const std::string& tradingSymbol,
				  const Decimal& entryPrice, 
				  OHLCTimeSeriesEntry<Decimal> entryBar,
				  const TradingVolume& unitsInPosition)
      : TradingPosition<Decimal>(tradingSymbol, std::make_shared<OpenShortPosition<Decimal>>(entryPrice, entryBar, unitsInPosition))
    {}

    TradingPositionShort (const TradingPositionShort<Decimal>& rhs)
      : TradingPosition<Decimal>(rhs)
    {}

    TradingPositionShort<Decimal>& 
    operator=(const TradingPositionShort<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPosition<Decimal>::operator=(rhs);

      return *this;
    }

    ~TradingPositionShort()
    {}

    bool isLongPosition() const
    {
      return false;
    }

    bool isShortPosition() const
    {
      return true;
    }

    Decimal getRMultiple()
    {
      if (this->isPositionOpen())
	throw TradingPositionException("TradingPositionShort::getRMultiple - r multiple not available for open position");

      if (this->getRMultipleStop() <= DecimalConstants<Decimal>::DecimalZero)      
	throw TradingPositionException("TradingPositionShort::getRMultiple - r multiple not available because R multiple stop not set");

      Decimal exit = this->getExitPrice();
      Decimal entry = this->getEntryPrice();

      if (this->isWinningPosition())
	{
	  return ((entry - exit)/(this->getRMultipleStop() - entry));
	}
      else
	return -exit/this->getRMultipleStop();
    }
  };

  template <class Decimal>
  inline void OpenLongPosition<Decimal>::ClosePosition (TradingPosition<Decimal>* position,
						     std::shared_ptr<TradingPositionState<Decimal>> openPosition,
						     const boost::gregorian::date exitDate,
						     const Decimal& exitPrice)
    {
      position->ChangeState (std::make_shared<ClosedLongPosition<Decimal>>(openPosition, exitDate, exitPrice));
    }

  template <class Decimal>
  inline void OpenShortPosition<Decimal>::ClosePosition (TradingPosition<Decimal>* position,
						     std::shared_ptr<TradingPositionState<Decimal>> openPosition,
						     const boost::gregorian::date exitDate,
						     const Decimal& exitPrice)
    {
      position->ChangeState (std::make_shared<ClosedShortPosition<Decimal>>(openPosition, exitDate, exitPrice));
    }

  template <class Decimal>
  inline void ClosedLongPosition<Decimal>::ClosePosition (TradingPosition<Decimal>* position,
						       std::shared_ptr<TradingPositionState<Decimal>> openPosition,
						       const boost::gregorian::date exitDate,
						       const Decimal& exitPrice)
    {
      throw TradingPositionException("ClosedLongPosition: Cannot close an already closed position");
    }

  template <class Decimal>
  inline void ClosedShortPosition<Decimal>::ClosePosition (TradingPosition<Decimal>* position,
						       std::shared_ptr<TradingPositionState<Decimal>> openPosition,
						       const boost::gregorian::date exitDate,
						       const Decimal& exitPrice)
    {
      throw TradingPositionException("ClosedShortPosition: Cannot close an already closed position");
    }
}
#endif


