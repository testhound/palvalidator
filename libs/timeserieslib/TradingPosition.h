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
  template <int Prec> class TradingPosition;

  template <int Prec>
  decimal<Prec> calculateTradeReturn (const decimal<Prec>& referencePrice, 
					  const decimal<Prec>& secondPrice)
    {
      return ((secondPrice - referencePrice) / referencePrice);
    }

  template <int Prec>
    decimal<Prec> calculatePercentReturn (const decimal<Prec>& referencePrice, 
					  const decimal<Prec>& secondPrice)
    {
return (calculateTradeReturn<Prec>(referencePrice, secondPrice) * DecimalConstants<Prec>::DecimalOneHundred);
    }

  template <int Prec> class OpenPositionBar
  {
  public:
    explicit OpenPositionBar (OHLCTimeSeriesEntry<Prec> entry)
      : mEntry(entry)
    {}

    OpenPositionBar(const OpenPositionBar<Prec>& rhs) 
      : mEntry(rhs.mEntry)
    {}

    OpenPositionBar<Prec>& 
    operator=(const OpenPositionBar<Prec> &rhs)
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

    const decimal<Prec>& getOpenValue() const
    {
      return mEntry.getOpenValue();
    }

    const decimal<Prec>& getHighValue() const
    {
      return mEntry.getHighValue();
    }

    const decimal<Prec>& getLowValue() const
    {
      return mEntry.getLowValue();

    }

    const decimal<Prec>& getCloseValue() const
    {
      return mEntry.getCloseValue();
    }

    const volume_t getVolume() const
    {
      return mEntry.getVolume();
    }

    const OHLCTimeSeriesEntry<Prec>& getTimeSeriesEntry() const
    {
      return mEntry;
    }

  private:
    OHLCTimeSeriesEntry<Prec> mEntry;
  };

  template <int Prec>
  bool operator==(const OpenPositionBar<Prec>& lhs, const OpenPositionBar<Prec>& rhs)
  {
    return (*lhs.getTimeSeriesEntry() == *rhs.getTimeSeriesEntry());
  }

  template <int Prec>
  bool operator!=(const OpenPositionBar<Prec>& lhs, const OpenPositionBar<Prec>& rhs)
  { 
    return !(lhs == rhs); 
  }



  // class OpenPositionHistory
  template <int Prec> class OpenPositionHistory
  {
  public:
    typedef typename std::map<TimeSeriesDate, OpenPositionBar<Prec>>::iterator PositionBarIterator;
    typedef typename std::map<TimeSeriesDate, OpenPositionBar<Prec>>::const_iterator ConstPositionBarIterator;

    explicit OpenPositionHistory(OHLCTimeSeriesEntry<Prec> entryBar) :
      mPositionBarHistory()
    {
      addFirstBar(entryBar);
    }

    OpenPositionHistory(const OpenPositionHistory<Prec>& rhs) 
      : mPositionBarHistory(rhs.mPositionBarHistory)
    {}

    OpenPositionHistory<Prec>& 
    operator=(const OpenPositionHistory<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      mPositionBarHistory = rhs.mPositionBarHistory;
      return *this;
    }

    ~OpenPositionHistory()
    {}

    void addBar(const OpenPositionBar<Prec>& entry)
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

    const dec::decimal<Prec>& getLastClose() const
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
    void addFirstBar(const OHLCTimeSeriesEntry<Prec> entry)
    {
      addBar(OpenPositionBar<Prec>(entry));
    }

  private:
    std::map<TimeSeriesDate, OpenPositionBar<Prec>> mPositionBarHistory;
  };

  template <int Prec> class TradingPositionState
  {
  public:
    typedef typename OpenPositionHistory<Prec>::PositionBarIterator PositionBarIterator;
    typedef typename OpenPositionHistory<Prec>::ConstPositionBarIterator ConstPositionBarIterator;

    TradingPositionState()
    {}

    virtual ~TradingPositionState()
    {}

    TradingPositionState(const TradingPositionState<Prec>& rhs)
    {}

    TradingPositionState<Prec>& 
    operator=(const TradingPositionState<Prec> &rhs)
    {}

    virtual bool isPositionOpen() const = 0;
    virtual bool isPositionClosed() const = 0;
    virtual const TimeSeriesDate& getEntryDate() const = 0;
    virtual const dec::decimal<Prec>& getEntryPrice() const = 0;
    virtual const dec::decimal<Prec>& getExitPrice() const = 0;
    virtual const boost::gregorian::date& getExitDate() const = 0;
    virtual void addBar (const OHLCTimeSeriesEntry<Prec>& entryBar) = 0;
    virtual const TradingVolume& getTradingUnits() const = 0;
    virtual unsigned int getNumBarsInPosition() const = 0;
    virtual unsigned int getNumBarsSinceEntry() const = 0;
    virtual const dec::decimal<Prec>& getLastClose() const = 0;
    virtual dec::decimal<Prec> getPercentReturn() const = 0;
    virtual dec::decimal<Prec> getTradeReturn() const = 0;
    virtual dec::decimal<Prec> getTradeReturnMultiplier() const = 0;
    virtual bool isWinningPosition() const = 0;
    virtual bool isLosingPosition() const = 0;
    // NOTE: To implement these in the ClosePositonState pass the open position to closed
    // position at creation time
    virtual TradingPositionState::ConstPositionBarIterator beginPositionBarHistory() const = 0;
    virtual TradingPositionState::ConstPositionBarIterator endPositionBarHistory() const = 0;
    virtual void ClosePosition (TradingPosition<Prec>* position,
				std::shared_ptr<TradingPositionState<Prec>> openPosition,
				const boost::gregorian::date exitDate,
				const dec::decimal<Prec>& exitPrice) = 0;
  };
  
  template <int Prec>
  class OpenPosition : public TradingPositionState<Prec>
  {
  public:
    typedef typename OpenPositionHistory<Prec>::PositionBarIterator PositionBarIterator;
    typedef typename OpenPositionHistory<Prec>::ConstPositionBarIterator ConstPositionBarIterator;

    OpenPosition (const dec::decimal<Prec>& entryPrice, 
		  OHLCTimeSeriesEntry<Prec> entryBar,
		  const TradingVolume& unitsInPosition) 
      : TradingPositionState<Prec>(),
      mEntryPrice(entryPrice),
      mEntryDate (entryBar.getDateValue()),
      mUnitsInPosition(unitsInPosition),
      mPositionBarHistory (entryBar),
      mBarsInPosition(1),
      mNumBarsSinceEntry(0)	
    {
      if (entryPrice <= DecimalConstants<Prec>::DecimalZero)
	throw TradingPositionException (std::string("OpenPosition constructor: entry price < 0"));
    }

    OpenPosition(const OpenPosition<Prec>& rhs) 
      : TradingPositionState<Prec>(rhs),
      mEntryPrice(rhs.mEntryPrice),
      mEntryDate (rhs.mEntryDate),
      mUnitsInPosition(rhs.mUnitsInPosition),
      mPositionBarHistory (rhs.mPositionBarHistory),
      mBarsInPosition (rhs.mBarsInPosition),
      mNumBarsSinceEntry(rhs.mNumBarsSinceEntry)
    {}

    OpenPosition<Prec>& 
    operator=(const OpenPosition<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPositionState<Prec>::operator=(rhs);

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

    virtual void addBar(const OHLCTimeSeriesEntry<Prec>& entryBar)
    {
      addBar(OpenPositionBar<Prec>(entryBar));
    }

    const dec::decimal<Prec>& getEntryPrice() const
    {
      return mEntryPrice;
    }

    const TimeSeriesDate& getEntryDate() const
    {
      return mEntryDate;
    }

    const dec::decimal<Prec>& getExitPrice() const
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

    const dec::decimal<Prec>& getLastClose() const
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
    void addBar(const OpenPositionBar<Prec>& positionBar)
    {
      mPositionBarHistory.addBar(positionBar);
      mBarsInPosition++;
      mNumBarsSinceEntry++;
    }

  private:
    dec::decimal<Prec> mEntryPrice;
    TimeSeriesDate mEntryDate;
    TradingVolume mUnitsInPosition;
    OpenPositionHistory<Prec> mPositionBarHistory;
    unsigned int mBarsInPosition;
    unsigned int mNumBarsSinceEntry;
  };

  template <int Prec>
  class OpenLongPosition : public OpenPosition<Prec>
  {
  public:
    OpenLongPosition (const dec::decimal<Prec>& entryPrice, 
		      OHLCTimeSeriesEntry<Prec> entryBar,
		      const TradingVolume& unitsInPosition)
      : OpenPosition<Prec>(entryPrice, entryBar, unitsInPosition)
    {}

    OpenLongPosition(const OpenLongPosition<Prec>& rhs) 
      : OpenPosition<Prec>(rhs)
    {}

    OpenLongPosition<Prec>& 
    operator=(const OpenLongPosition<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      OpenPosition<Prec>::operator=(rhs);
      return *this;
    }

    void addBar (const OHLCTimeSeriesEntry<Prec>& entryBar)
    {
      OpenPosition<Prec>::addBar(entryBar);
    }

    dec::decimal<Prec> getPercentReturn() const
    {
      return (calculatePercentReturn (OpenPosition<Prec>::getEntryPrice(), 
				      OpenPosition<Prec>::getLastClose()));
    }

    dec::decimal<Prec> getTradeReturn() const
    {
      return (calculateTradeReturn (OpenPosition<Prec>::getEntryPrice(), 
				      OpenPosition<Prec>::getLastClose()));
    }

    dec::decimal<Prec> getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Prec>::DecimalOne + getTradeReturn());
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Prec>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    void ClosePosition (TradingPosition<Prec>* position,
			std::shared_ptr<TradingPositionState<Prec>> openPosition,
			const boost::gregorian::date exitDate,
			const dec::decimal<Prec>& exitPrice);
  };

  template <int Prec>
  class OpenShortPosition : public OpenPosition<Prec>
  {
  public:
    OpenShortPosition (const dec::decimal<Prec>& entryPrice, 
		       OHLCTimeSeriesEntry<Prec>& entryBar,
		       const TradingVolume& unitsInPosition)
      : OpenPosition<Prec>(entryPrice, entryBar, unitsInPosition)
    {}

    OpenShortPosition(const OpenShortPosition<Prec>& rhs) 
      : OpenPosition<Prec>(rhs)
    {}

    OpenShortPosition<Prec>& 
    operator=(const OpenShortPosition<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      OpenPosition<Prec>::operator=(rhs);
      return *this;
    }

    void addBar(const OHLCTimeSeriesEntry<Prec>& entryBar)
    {
      OpenPosition<Prec>::addBar(entryBar);
    }

    dec::decimal<Prec> getTradeReturn() const
    {
      return -(calculateTradeReturn (OpenPosition<Prec>::getEntryPrice(), 
				      OpenPosition<Prec>::getLastClose()));
    }

    dec::decimal<Prec> getPercentReturn() const
    {
      return -(calculatePercentReturn (OpenPosition<Prec>::getEntryPrice(), 
				       OpenPosition<Prec>::getLastClose()));

    }

    dec::decimal<Prec> getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Prec>::DecimalOne + getTradeReturn());
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Prec>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    void ClosePosition (TradingPosition<Prec>* position,
			std::shared_ptr<TradingPositionState<Prec>> openPosition,
			const boost::gregorian::date exitDate,
			const dec::decimal<Prec>& exitPrice);
  };


  template <int Prec> class ClosedPosition : public TradingPositionState<Prec>
  {
  public:
    ClosedPosition (std::shared_ptr<TradingPositionState<Prec>> openPosition,
		    const boost::gregorian::date exitDate,
		    const dec::decimal<Prec>& exitPrice) 
      : TradingPositionState<Prec>(),
	mOpenPosition (openPosition),
	mExitDate(exitDate),
	mExitPrice(exitPrice)
    {
      if (exitDate < openPosition->getEntryDate())
	throw std::domain_error (std::string("ClosedPosition: exit Date" +to_simple_string (exitDate) +" cannot occur before entry date " +to_simple_string (openPosition->getEntryDate())));
    }

    ClosedPosition(const ClosedPosition<Prec>& rhs) 
      : TradingPositionState<Prec>(rhs),
	mOpenPosition (rhs.mOpenPosition),
	mExitDate(rhs.mExitDate),
	mExitPrice(rhs.mExitPrice)
    {}

    ClosedPosition<Prec>& 
    operator=(const ClosedPosition<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPositionState<Prec>::operator=(rhs);
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

    const dec::decimal<Prec>& getEntryPrice() const
    {
      return mOpenPosition->getEntryPrice();
    }

    const boost::gregorian::date& getExitDate() const
    {
      return mExitDate;
    }

    const dec::decimal<Prec>& getExitPrice() const
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

    const dec::decimal<Prec>& getLastClose() const
    {
      return mOpenPosition->getLastClose();
    }

    void addBar (const OHLCTimeSeriesEntry<Prec>& entryBar)
    {
      throw TradingPositionException ("Cannot add bar to a closed position");
    }

    typename TradingPositionState<Prec>::ConstPositionBarIterator beginPositionBarHistory() const
    {
      return mOpenPosition->beginPositionBarHistory();
    }

    typename TradingPositionState<Prec>::ConstPositionBarIterator endPositionBarHistory() const
    {
      return mOpenPosition->endPositionBarHistory();
    }

  private:
    std::shared_ptr<TradingPositionState<Prec>> mOpenPosition;
    boost::gregorian::date mExitDate;
    dec::decimal<Prec> mExitPrice;
  };


  template <int Prec>
  bool operator==(const ClosedPosition<Prec>& lhs, const ClosedPosition<Prec>& rhs)
  {
    return (lhs.getEntryDate() == rhs.getEntryDate() &&
	    lhs.getEntryPrice() == rhs.getEntryPrice() &&
	    lhs.getExitDate() == rhs.getExitDate() &&
	    lhs.getExitPrice() == rhs.getExitPrice() &&
	    lhs.getUnitsInPosition() == rhs.getUnitsInPosition() &&
	    lhs.isLongPosition() == rhs.isLongPosition() &&
	    lhs.isShortPosition() == rhs.isShortPosition());
  }

  template <int Prec>
  bool operator!=(const ClosedPosition<Prec>& lhs, const ClosedPosition<Prec>& rhs)
  { 
    return !(lhs == rhs); 
  }

  template <int Prec>
  class ClosedLongPosition : public ClosedPosition<Prec>
  {
  public:
    ClosedLongPosition (std::shared_ptr<TradingPositionState<Prec>> openPosition,
			const boost::gregorian::date exitDate,
			const dec::decimal<Prec>& exitPrice)
      : ClosedPosition<Prec>(openPosition, exitDate, exitPrice)
    {}

    ClosedLongPosition(const ClosedLongPosition<Prec>& rhs) 
      : ClosedPosition<Prec>(rhs)
    {}

    ClosedLongPosition<Prec>& 
    operator=(const ClosedLongPosition<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      ClosedPosition<Prec>::operator=(rhs);
      return *this;
    }

    ~ClosedLongPosition()
    {}

     dec::decimal<Prec> getPercentReturn() const
    {
      return (calculatePercentReturn (ClosedPosition<Prec>::getEntryPrice(), 
				      ClosedPosition<Prec>::getExitPrice()));
    }

    dec::decimal<Prec> getTradeReturn() const
    {
      return (calculateTradeReturn (ClosedPosition<Prec>::getEntryPrice(), 
				    ClosedPosition<Prec>::getExitPrice()));
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Prec>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    dec::decimal<Prec> getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Prec>::DecimalOne + ClosedLongPosition<Prec>::getTradeReturn());
    }

    void ClosePosition (TradingPosition<Prec>* position,
			std::shared_ptr<TradingPositionState<Prec>> openPosition,
			const boost::gregorian::date exitDate,
			const dec::decimal<Prec>& exitPrice);
  };


  template <int Prec>
  class ClosedShortPosition : public ClosedPosition<Prec>
  {
  public:
    ClosedShortPosition (std::shared_ptr<TradingPositionState<Prec>> openPosition,
			const boost::gregorian::date exitDate,
			const dec::decimal<Prec>& exitPrice)
      : ClosedPosition<Prec>(openPosition, exitDate, exitPrice)
    {}

    ClosedShortPosition(const ClosedShortPosition<Prec>& rhs) 
      : ClosedPosition<Prec>(rhs)
    {}

    ClosedShortPosition<Prec>& 
    operator=(const ClosedShortPosition<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      ClosedPosition<Prec>::operator=(rhs);
      return *this;
    }

    ~ClosedShortPosition()
    {}

    dec::decimal<Prec> getTradeReturn() const
    {
      return -(calculateTradeReturn (ClosedPosition<Prec>::getEntryPrice(), 
				     (ClosedPosition<Prec>::getExitPrice())));
    }

    dec::decimal<Prec> getPercentReturn() const
    {
      return -(calculatePercentReturn (ClosedPosition<Prec>::getEntryPrice(), 
				       ClosedPosition<Prec>::getExitPrice()));
    }

    dec::decimal<Prec> getTradeReturnMultiplier() const
    {
      return (DecimalConstants<Prec>::DecimalOne + ClosedShortPosition<Prec>::getTradeReturn());
    }

    bool isWinningPosition() const
    {
      return (getTradeReturn() > DecimalConstants<Prec>::DecimalZero);
    }

    bool isLosingPosition() const
    {
      return !isWinningPosition();
    }

    void ClosePosition (TradingPosition<Prec>* position,
			std::shared_ptr<TradingPositionState<Prec>> openPosition,
			const boost::gregorian::date exitDate,
			const dec::decimal<Prec>& exitPrice);
  };

  template <int Prec>
  class TradingPositionObserver
    {
    public:
     TradingPositionObserver()
      {}

      virtual ~TradingPositionObserver()
      {}

      virtual void PositionClosed (TradingPosition<Prec> *aPosition) = 0;
  };

  template <int Prec> class TradingPosition
  {
  public:
    typedef typename OpenPosition<Prec>::ConstPositionBarIterator ConstPositionBarIterator;

    TradingPosition (const TradingPosition<Prec>& rhs)
      : mTradingSymbol(rhs.mTradingSymbol),
      mPositionState(rhs.mPositionState),
      mPositionID(rhs.mPositionID),
      mObservers(rhs.mObservers),
      mRMultipleStop(rhs.mRMultipleStop),
      mRMultipleStopSet(rhs.mRMultipleStopSet)
    {}

    TradingPosition<Prec>& 
    operator=(const TradingPosition<Prec> &rhs)
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
    virtual dec::decimal<Prec> getRMultiple() = 0;

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

    const dec::decimal<Prec>& getEntryPrice() const
    {
      return mPositionState->getEntryPrice();
    }

    const dec::decimal<Prec>& getExitPrice() const
    {
      return mPositionState->getExitPrice();
    }

    const boost::gregorian::date& getExitDate() const
    {
      return mPositionState->getExitDate();
    }

    void addBar (const OHLCTimeSeriesEntry<Prec>& entryBar)
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

    const dec::decimal<Prec>& getLastClose() const
    {
      return mPositionState->getLastClose();
    }

    dec::decimal<Prec> getPercentReturn() const
    {
      return mPositionState->getPercentReturn();
    }

    dec::decimal<Prec> getTradeReturn() const
    {
      return mPositionState->getTradeReturn();
    }

    dec::decimal<Prec> getTradeReturnMultiplier() const
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
			const dec::decimal<Prec>& exitPrice)
    {
      mPositionState->ClosePosition (this, mPositionState, exitDate, exitPrice);
      NotifyPositionClosed (this);
    }

    void addObserver (std::reference_wrapper<TradingPositionObserver<Prec>> observer)
    {
      mObservers.push_back(observer);
    }

    void setRMultipleStop(const dec::decimal<Prec>& rMultipleStop)
    {
      if (rMultipleStop <= DecimalConstants<Prec>::DecimalZero)
	throw TradingPositionException (std::string("TradingPosition:setRMultipleStop =< 0"));

      mRMultipleStop = rMultipleStop;
      mRMultipleStopSet = true;
    }

    bool RMultipleStopSet() const
    {
      return mRMultipleStopSet;
    }

    dec::decimal<Prec> getRMultipleStop() const
    {
      return mRMultipleStop;
    }

  protected:
    TradingPosition(const std::string& tradingSymbol,
		    std::shared_ptr<TradingPositionState<Prec>> positionState)
      : mTradingSymbol (tradingSymbol),
	mPositionState (positionState),
	mPositionID(++mPositionIDCount),
	mObservers(),
	mRMultipleStop(DecimalConstants<Prec>::DecimalZero),
	mRMultipleStopSet(false)
    {}

    
  private:
    typedef typename std::list<std::reference_wrapper<TradingPositionObserver<Prec>>>::const_iterator ConstObserverIterator;

    void ChangeState (std::shared_ptr<TradingPositionState<Prec>> newState)
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

    void NotifyPositionClosed (TradingPosition<Prec> *aPosition)
    {
      ConstObserverIterator it = beginObserverList();
      for (; it != endObserverList(); it++)
	(*it).get().PositionClosed (aPosition);
    }

    friend class OpenLongPosition<Prec>;
    friend class OpenShortPosition<Prec>;

  private:
    std::string mTradingSymbol;
    std::shared_ptr<TradingPositionState<Prec>> mPositionState;
    uint32_t mPositionID;
    static std::atomic<uint32_t> mPositionIDCount;
    std::list<std::reference_wrapper<TradingPositionObserver<Prec>>> mObservers;
    dec::decimal<Prec> mRMultipleStop;
    bool mRMultipleStopSet;
  };

  template <int Prec> std::atomic<uint32_t>  TradingPosition<Prec>::mPositionIDCount{0};

  template <int Prec> 
  class TradingPositionLong : public TradingPosition<Prec>
  {
  public:
    explicit TradingPositionLong (const std::string& tradingSymbol,
				  const dec::decimal<Prec>& entryPrice, 
				  OHLCTimeSeriesEntry<Prec> entryBar,
				  const TradingVolume& unitsInPosition)
      : TradingPosition<Prec>(tradingSymbol,std::make_shared<OpenLongPosition<Prec>>(entryPrice, entryBar, unitsInPosition))
    {}

    TradingPositionLong (const TradingPositionLong<Prec>& rhs)
      : TradingPosition<Prec>(rhs)
    {}

    TradingPositionLong<Prec>& 
    operator=(const TradingPositionLong<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPosition<Prec>::operator=(rhs);

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
    
    dec::decimal<Prec> getRMultiple()
    {
      if (this->isPositionOpen())
	throw TradingPositionException("TradingPositionLong::getRMultiple - r multiple not available for open position");
      
      if (this->getRMultipleStop() <= DecimalConstants<Prec>::DecimalZero)
 	throw TradingPositionException("TradingPositionLong::getRMultiple - r multiple not available because R multiple stop not set");

      dec::decimal<Prec> exit = this->getExitPrice();
      dec::decimal<Prec> entry = this->getEntryPrice();

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

  template <int Prec> 
  class TradingPositionShort : public TradingPosition<Prec>
  {
  public:
    explicit TradingPositionShort (const std::string& tradingSymbol,
				  const dec::decimal<Prec>& entryPrice, 
				  OHLCTimeSeriesEntry<Prec> entryBar,
				  const TradingVolume& unitsInPosition)
      : TradingPosition<Prec>(tradingSymbol, std::make_shared<OpenShortPosition<Prec>>(entryPrice, entryBar, unitsInPosition))
    {}

    TradingPositionShort (const TradingPositionShort<Prec>& rhs)
      : TradingPosition<Prec>(rhs)
    {}

    TradingPositionShort<Prec>& 
    operator=(const TradingPositionShort<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingPosition<Prec>::operator=(rhs);

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

    dec::decimal<Prec> getRMultiple()
    {
      if (this->isPositionOpen())
	throw TradingPositionException("TradingPositionShort::getRMultiple - r multiple not available for open position");

      if (this->getRMultipleStop() <= DecimalConstants<Prec>::DecimalZero)      
	throw TradingPositionException("TradingPositionShort::getRMultiple - r multiple not available because R multiple stop not set");

      dec::decimal<Prec> exit = this->getExitPrice();
      dec::decimal<Prec> entry = this->getEntryPrice();

      if (this->isWinningPosition())
	{
	  return ((entry - exit)/(this->getRMultipleStop() - entry));
	}
      else
	return -exit/this->getRMultipleStop();
    }
  };

  template <int Prec>
  inline void OpenLongPosition<Prec>::ClosePosition (TradingPosition<Prec>* position,
						     std::shared_ptr<TradingPositionState<Prec>> openPosition,
						     const boost::gregorian::date exitDate,
						     const dec::decimal<Prec>& exitPrice)
    {
      position->ChangeState (std::make_shared<ClosedLongPosition<Prec>>(openPosition, exitDate, exitPrice));
    }

  template <int Prec>
  inline void OpenShortPosition<Prec>::ClosePosition (TradingPosition<Prec>* position,
						     std::shared_ptr<TradingPositionState<Prec>> openPosition,
						     const boost::gregorian::date exitDate,
						     const dec::decimal<Prec>& exitPrice)
    {
      position->ChangeState (std::make_shared<ClosedShortPosition<Prec>>(openPosition, exitDate, exitPrice));
    }

  template <int Prec>
  inline void ClosedLongPosition<Prec>::ClosePosition (TradingPosition<Prec>* position,
						       std::shared_ptr<TradingPositionState<Prec>> openPosition,
						       const boost::gregorian::date exitDate,
						       const dec::decimal<Prec>& exitPrice)
    {
      throw TradingPositionException("ClosedLongPosition: Cannot close an already closed position");
    }

  template <int Prec>
  inline void ClosedShortPosition<Prec>::ClosePosition (TradingPosition<Prec>* position,
						       std::shared_ptr<TradingPositionState<Prec>> openPosition,
						       const boost::gregorian::date exitDate,
						       const dec::decimal<Prec>& exitPrice)
    {
      throw TradingPositionException("ClosedShortPosition: Cannot close an already closed position");
    }
}
#endif


