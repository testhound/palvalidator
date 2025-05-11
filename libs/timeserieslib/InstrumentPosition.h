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
#include "DecimalConstants.h"

namespace mkc_timeseries
{
  template <class Decimal> class InstrumentPosition;

  /**
   * @class InstrumentPositionState
   * @brief Abstract base class defining the interface for different states of an instrument's position (e.g., long, short, flat).
   *
   * @tparam Decimal The numeric type used for financial calculations (e.g., double, a fixed-point decimal class).
   *
   * Responsibilities:
   * - Define a common interface for operations that depend on the current position state,
   * such as adding new position units, closing positions, or querying position status.
   * - Enable polymorphic behavior for InstrumentPosition to transition between states.
   *
   * Collaboration:
   * - Implemented by concrete state classes: FlatInstrumentPositionState, LongInstrumentPositionState, ShortInstrumentPositionState.
   * - Used internally by InstrumentPosition to delegate state-specific actions.
   */
  template <class Decimal> class InstrumentPositionState
  {
  public:
    typedef typename std::vector<std::shared_ptr<TradingPosition<Decimal>>>::const_iterator ConstInstrumentPositionIterator;

  public:
    /**
     * @brief Default constructor.
     */
    InstrumentPositionState()
    {}

    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes.
     */
    virtual ~InstrumentPositionState()
    {}

    /**
     * @brief Copy constructor.
     * @param rhs The InstrumentPositionState object to copy.
     */
    InstrumentPositionState (const InstrumentPositionState<Decimal>& rhs)
    {}

    /**
     * @brief Assignment operator.
     * @param rhs The InstrumentPositionState object to assign from.
     * @return A reference to this object.
     */
    InstrumentPositionState<Decimal>& 
    operator=(const InstrumentPositionState<Decimal> &rhs)
    {
      return *this;
    }

    /**
     * @brief Checks if the current state represents a long position.
     * @return True if the position is long, false otherwise.
     */
    virtual bool isLongPosition() const = 0;

     /**
     * @brief Checks if the current state represents a short position.
     * @return True if the position is short, false otherwise.
     */
    virtual bool isShortPosition() const = 0;

    /**
     * @brief Checks if the current state represents a flat position (no open trades).
     * @return True if the position is flat, false otherwise.
     */
    virtual bool isFlatPosition() const = 0;

    /**
     * @brief Gets the number of active trading units in the current position.
     * @return The count of trading units.
     */
    virtual uint32_t getNumPositionUnits() const = 0;

    /**
     * @brief Retrieves a specific trading position unit.
     * @param unitNumber The 1-based index of the trading unit to retrieve.
     * @return A constant iterator pointing to the specified TradingPosition.
     * @throw InstrumentPositionException if the unitNumber is out of range for the current state.
     */
    virtual ConstInstrumentPositionIterator getInstrumentPosition (uint32_t unitNumber) const = 0;

    /**
     * @brief Updates all trading units in the position with a new market data bar.
     * @param entryBar The OHLCTimeSeriesEntry representing the new bar data.
     * @throw InstrumentPositionException if called on a state that cannot process bars (e.g., flat).
     */
    virtual void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar) = 0;

    /**
     * @brief Adds a new trading position unit to the instrument's overall position.
     * This method handles the logic for transitioning state if necessary (e.g., from flat to long).
     * @param iPosition Pointer to the owning InstrumentPosition object (to allow state changes).
     * @param position A shared pointer to the TradingPosition unit to add.
     * @throw InstrumentPositionException if the position cannot be added (e.g., type mismatch in non-flat states).
     */
    virtual void addPosition(InstrumentPosition<Decimal>* iPosition,
			     std::shared_ptr<TradingPosition<Decimal>> position) = 0;

    /**
     * @brief Gets a constant iterator to the beginning of the trading position units.
     * @return ConstInstrumentPositionIterator pointing to the first unit.
     * @throw InstrumentPositionException if called on a state with no positions (e.g., flat).
     */
    virtual ConstInstrumentPositionIterator beginInstrumentPosition() const = 0;

    /**
     * @brief Gets a constant iterator to the end of the trading position units.
     * @return ConstInstrumentPositionIterator pointing past the last unit.
     * @throw InstrumentPositionException if called on a state with no positions (e.g., flat).
     */
    virtual ConstInstrumentPositionIterator endInstrumentPosition() const = 0;

    /**
     * @brief Closes a specific trading position unit.
     * @param iPosition Pointer to the owning InstrumentPosition object (to allow state
     * changes if all units are closed).
     * @param exitDate The date of the exit.
     * @param exitPrice The price at which the unit is exited.
     * @param unitNumber The 1-based index of the trading unit to close.
     * @throw InstrumentPositionException if the unit cannot be closed or is not found.
     */
    virtual void closeUnitPosition(InstrumentPosition<Decimal>* iPosition,
				   const boost::gregorian::date exitDate,
				   const Decimal& exitPrice,
				   uint32_t unitNumber) = 0;

    /**
     * @brief Closes all open trading position units.
     * @param iPosition Pointer to the owning InstrumentPosition object (to allow state changes to flat).
     * @param exitDate The date of the exit.
     * @param exitPrice The price at which all units are exited.
     * @throw InstrumentPositionException if called on a state with no positions to close.
     */
    virtual void closeAllPositions(InstrumentPosition<Decimal>* iPosition,
				   const boost::gregorian::date exitDate,
				   const Decimal& exitPrice) = 0;
 
  
  };

  /**
   * @class FlatInstrumentPositionState
   * @brief Represents the state where there is no open position for the instrument (i.e., flat).
   *
   * @tparam Decimal The numeric type used for financial calculations.
   * This is a Singleton class accessed via getInstance().
   */
  template <class Decimal> class FlatInstrumentPositionState : public InstrumentPositionState<Decimal>
  {
  public:
    typedef typename InstrumentPositionState<Decimal>::ConstInstrumentPositionIterator ConstInstrumentPositionIterator;
  public:
    FlatInstrumentPositionState (const FlatInstrumentPositionState<Decimal>& rhs)
      : InstrumentPositionState<Decimal>(rhs)
    {}

    FlatInstrumentPositionState<Decimal>& 
    operator=(const FlatInstrumentPositionState<Decimal> &rhs) 
    {
       if (this == &rhs)
	return *this;

      InstrumentPositionState<Decimal>::operator=(rhs);
      return *this;
    }

    ~FlatInstrumentPositionState()
    {}

    static std::shared_ptr<FlatInstrumentPositionState<Decimal>> getInstance()
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

    void addPosition(InstrumentPosition<Decimal>* iPosition,
		     std::shared_ptr<TradingPosition<Decimal>> position);

    

    ConstInstrumentPositionIterator beginInstrumentPosition() const
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: beginInstrumentPosition - no positions avaialble in flat state");
    }

    ConstInstrumentPositionIterator endInstrumentPosition() const
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: endInstrumentPosition - no positions avaialble in flat state");
    }

    void closeUnitPosition(InstrumentPosition<Decimal>* iPosition,
			   const boost::gregorian::date exitDate,
			   const Decimal& exitPrice,
			   uint32_t unitNumber)
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: closeUnitPosition - no positions avaialble in flat state");
    }

    void closeAllPositions(InstrumentPosition<Decimal>* iPosition,
			   const boost::gregorian::date exitDate,
			   const Decimal& exitPrice)
    {
      throw InstrumentPositionException("FlatInstrumentPositionState: closeAllPositions - no positions avaialble in flat state");
    }

  private:
    FlatInstrumentPositionState()
      : InstrumentPositionState<Decimal>()
    {}

  private:
    static std::shared_ptr<FlatInstrumentPositionState<Decimal>> mInstance;
  };

  template <class Decimal> std::shared_ptr<FlatInstrumentPositionState<Decimal>> FlatInstrumentPositionState<Decimal>::mInstance(new FlatInstrumentPositionState<Decimal>());

  /**
   * @class InMarketPositionState
   * @brief Abstract base class for states where the instrument has an open market position (either long or short).
   *
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * Responsibilities:
   * - Manages a collection of TradingPosition units that make up the current market exposure.
   * - Provides common functionality for adding bars, closing positions, and querying unit details.
   */
  template <class Decimal> class InMarketPositionState : public InstrumentPositionState<Decimal>
  {
  public:
    typedef typename InstrumentPositionState<Decimal>::ConstInstrumentPositionIterator ConstInstrumentPositionIterator;
  protected:
    /**
     * @brief Constructor for initializing with the first trading position unit.
     * @param position The initial TradingPosition unit that establishes this market state.
     * @throw InstrumentPositionException if the initial position is already closed.
     */
    InMarketPositionState(std::shared_ptr<TradingPosition<Decimal>> position)
      : InstrumentPositionState<Decimal>(),
	mTradingPositionUnits()
    {
      this->addPositionCommon(position);
    }

  public:
    /**
     * @brief Copy constructor.
     * @param rhs The InMarketPositionState to copy.
     */
    InMarketPositionState (const InMarketPositionState<Decimal>& rhs)
      : InstrumentPositionState<Decimal>(rhs),
	mTradingPositionUnits(rhs.mTradingPositionUnits)
    {}

    /**
     * @brief Virtual destructor.
     */
    virtual ~InMarketPositionState()
    {}

    /**
     * @brief Assignment operator.
     * @param rhs The InMarketPositionState to assign from.
     * @return Reference to this object.
     */
    InMarketPositionState<Decimal>& 
    operator=(const InMarketPositionState<Decimal> &rhs) 
    {
      if (this == &rhs)
	return *this;
      
      InstrumentPositionState<Decimal>::operator=(rhs);
      mTradingPositionUnits = rhs.mTradingPositionUnits;
    }

    /**
     * @brief Gets the number of active trading units in the position.
     * @return The count of trading units.
     */
    uint32_t getNumPositionUnits() const
    {
      return mTradingPositionUnits.size();
    }

    /**
     * @brief Retrieves a specific trading position unit.
     * @param unitNumber The 1-based index of the trading unit to retrieve.
     * @return A constant iterator pointing to the specified TradingPosition.
     * @throw InstrumentPositionException if unitNumber is out of range.
     */
    ConstInstrumentPositionIterator
    getInstrumentPosition (uint32_t unitNumber) const
    {
      checkUnitNumber (unitNumber);

      // We subtract one because the stl vector starts at 0, but unit numbers start at 1
      return (this->beginInstrumentPosition() + (unitNumber - 1));
      //return mTradingPositionUnits.at (unitNumber - 1);
    }

    void addPositionCommon(std::shared_ptr<TradingPosition<Decimal>> position)
    {
      if (position->isPositionClosed())
	throw InstrumentPositionException ("InstrumentPosition: cannot add a closed position");

      mTradingPositionUnits.push_back (position);
    }

    /**
     * @brief Updates all open trading units with a new market data bar.
     * Only adds the bar if its date is after the entry date of a unit.
     * @param entryBar The OHLCTimeSeriesEntry representing the new bar data.
     */
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

    /**
     * @brief Gets a constant iterator to the beginning of the trading position units.
     * @return ConstInstrumentPositionIterator pointing to the first unit.
     */
    ConstInstrumentPositionIterator beginInstrumentPosition() const
    {
      return  mTradingPositionUnits.begin();
    }

    /**
     * @brief Gets a constant iterator to the end of the trading position units.
     * @return ConstInstrumentPositionIterator pointing past the last unit.
     */
    ConstInstrumentPositionIterator endInstrumentPosition() const
    {
      return  mTradingPositionUnits.end();
    }

    /**
     * @brief Closes a specific trading position unit.
     * If closing this unit results in no open units, transitions the InstrumentPosition to Flat state.
     * @param iPosition Pointer to the owning InstrumentPosition for state transition.
     * @param exitDate The date of the exit.
     * @param exitPrice The price at which the unit is exited.
     * @param unitNumber The 1-based index of the trading unit to close.
     * @throw InstrumentPositionException if the unit number is invalid, unit not found, or unit already closed.
     */
    void closeUnitPosition(InstrumentPosition<Decimal>* iPosition,
			   const boost::gregorian::date exitDate,
			   const Decimal& exitPrice,
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
	iPosition->ChangeState (FlatInstrumentPositionState<Decimal>::getInstance());
 
    }

    /**
     * @brief Closes all open trading position units.
     * Transitions the InstrumentPosition to Flat state.
     * @param iPosition Pointer to the owning InstrumentPosition for state transition.
     * @param exitDate The date of the exit.
     * @param exitPrice The price at which all units are exited.
     */
    void closeAllPositions(InstrumentPosition<Decimal>* iPosition,
			   const boost::gregorian::date exitDate,
			   const Decimal& exitPrice)
    {
      ConstInstrumentPositionIterator it = beginInstrumentPosition();
      for (; it != this->endInstrumentPosition(); it++)
	{
	  if ((*it)->isPositionOpen())
	    (*it)->ClosePosition (exitDate, exitPrice);
	}

      mTradingPositionUnits.clear();
      iPosition->ChangeState (FlatInstrumentPositionState<Decimal>::getInstance());
    }

  private:
    /**
     * @brief Validates the given unit number.
     * @param unitNumber The 1-based unit number to check.
     * @throw InstrumentPositionException if unitNumber is 0 or greater than the number of units.
     */
    void checkUnitNumber (uint32_t unitNumber) const
    {
      if (unitNumber > getNumPositionUnits())
	throw InstrumentPositionException ("InstrumentPosition:getInstrumentPosition: unitNumber " + std::to_string (unitNumber) + " is out range");

      if (unitNumber == 0)
	throw InstrumentPositionException ("InstrumentPosition:getInstrumentPosition: unitNumber - unit numbers start at one");
    }

  private:
    std::vector<std::shared_ptr<TradingPosition<Decimal>>> mTradingPositionUnits;
  };

  
  /**
   * @class LongInstrumentPositionState
   * @brief Represents the state where the instrument has an open long position.
   *
   * @tparam Decimal The numeric type used for financial calculations.
   */
  template <class Decimal> class LongInstrumentPositionState : public InMarketPositionState<Decimal>
  {
  public:
    LongInstrumentPositionState(std::shared_ptr<TradingPosition<Decimal>> position)
      : InMarketPositionState<Decimal>(position)
    {}

    LongInstrumentPositionState (const LongInstrumentPositionState<Decimal>& rhs)
      : InMarketPositionState<Decimal>(rhs)
    {}

    ~ LongInstrumentPositionState()
    {}

    LongInstrumentPositionState<Decimal>& 
    operator=(const LongInstrumentPositionState<Decimal> &rhs) 
    {
      if (this == &rhs)
	return *this;
      
      InMarketPositionState<Decimal>::operator=(rhs);
      return *this;
    }

    /**
     * @brief Adds a new long trading position unit.
     * @param iPosition Pointer to the owning InstrumentPosition (unused in this specific override but required by base).
     * @param position The long TradingPosition unit to add.
     * @throw InstrumentPositionException if trying to add a non-long position or a closed position.
     */
    void addPosition(InstrumentPosition<Decimal>* iPosition,
		     std::shared_ptr<TradingPosition<Decimal>> position)
    {
      if (position->isLongPosition())
	this->addPositionCommon(position);
      else
	throw InstrumentPositionException ("InstrumentPosition: cannot add short position unit to long position");
    }

    /**
     * @brief Checks if the current state represents a long position.
     * @return Always true for this state.
     */
    bool isLongPosition() const
    {
      return true;
    }

    /**
     * @brief Checks if the current state represents a short position.
     * @return Always false for this state.
     */
    bool isShortPosition() const
    {
      return false;
    }

    /**
     * @brief Checks if the current state represents a flat position.
     * @return Always false for this state.
     */
    bool isFlatPosition() const
    {
      return false;
    }
  };

  /**
   * @class ShortInstrumentPositionState
   * @brief Represents the state where the instrument has an open short position.
   *
   * @tparam Decimal The numeric type used for financial calculations.
   */
  template <class Decimal> class ShortInstrumentPositionState : public InMarketPositionState<Decimal>
  {
  public:
    ShortInstrumentPositionState(std::shared_ptr<TradingPosition<Decimal>> position)
      : InMarketPositionState<Decimal>(position)
    {}

    ShortInstrumentPositionState (const ShortInstrumentPositionState<Decimal>& rhs)
      : InMarketPositionState<Decimal>(rhs)
    {}

    ShortInstrumentPositionState<Decimal>& 
    operator=(const ShortInstrumentPositionState<Decimal> &rhs) 
    {
      if (this == &rhs)
	return *this;
      
      InMarketPositionState<Decimal>::operator=(rhs);
      return *this;
    }

    void addPosition(InstrumentPosition<Decimal>* iPosition,
		     std::shared_ptr<TradingPosition<Decimal>> position)
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

  /**
   * @class InstrumentPosition
   * @brief Encapsulates position state and logic for a single trading symbol.
   *
   * Responsibilities:
   * - Store and manage a list of active positions for a specific symbol.
   * - Delegate position lifecycle logic to an internal state class (e.g., LongInstrumentPositionState).
   * - Notify StrategyBroker and other observers of state changes.
   *
   * Collaboration:
   * - Owned by InstrumentPositionManager.
   * - Works with TradingPosition state objects to reflect live positions.
   *
   * An InstrumentPosition is made up of one
   * or more TradingPosition objects
   *
   * Each TradingPosition object is assigned a
   * unit number (starting at 1)
   *
   * This will allow closing a single unit at
   * a time or closing all units at the same
   * time.
   */

  template <class Decimal> class InstrumentPosition
  {
  public:
    typedef typename InstrumentPositionState<Decimal>::ConstInstrumentPositionIterator ConstInstrumentPositionIterator;

  public:
    InstrumentPosition (const std::string& instrumentSymbol)
      : mInstrumentSymbol(instrumentSymbol),
	mInstrumentPositionState(FlatInstrumentPositionState<Decimal>::getInstance())
    {}

    InstrumentPosition (const InstrumentPosition<Decimal>& rhs)
      : mInstrumentSymbol(rhs.mInstrumentSymbol),
	mInstrumentPositionState(rhs.mInstrumentPositionState)
    {}

    InstrumentPosition<Decimal>& 
    operator=(const InstrumentPosition<Decimal> &rhs)
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

    const Decimal& getFillPrice() const
    {
      return getFillPrice(1);
    }

    const Decimal& getFillPrice(uint32_t unitNumber) const
    {
      ConstInstrumentPositionIterator pos = getInstrumentPosition(unitNumber);
      return (*pos)->getEntryPrice();
    }

    void setRMultipleStop (const Decimal& riskStop) const
    {
      this->setRMultipleStop (riskStop, 1);
    }

    void setRMultipleStop (const Decimal& riskStop, uint32_t unitNumber) const
    {
      ConstInstrumentPositionIterator pos = getInstrumentPosition(unitNumber);
      (*pos)->setRMultipleStop (riskStop);
    }

    void addBar (const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      mInstrumentPositionState->addBar(entryBar);
    }

    void addPosition(std::shared_ptr<TradingPosition<Decimal>> position)
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

      throw InstrumentPositionException ("InstrumentPosition::getVolumeInAllUnits - Cannot get volume when position is flat");
    }

    void closeUnitPosition(const boost::gregorian::date exitDate,
			   const Decimal& exitPrice,
			   uint32_t unitNumber)
    {
      mInstrumentPositionState->closeUnitPosition(this, exitDate, exitPrice, unitNumber);
    }

    void closeAllPositions(const boost::gregorian::date exitDate,
			   const Decimal& exitPrice)
    {
      mInstrumentPositionState->closeAllPositions(this, exitDate, exitPrice);
    }

  private:
    void ChangeState (std::shared_ptr<InstrumentPositionState<Decimal>> newState)
    {
      mInstrumentPositionState = newState;
    }

    friend class FlatInstrumentPositionState<Decimal>;
    friend class LongInstrumentPositionState<Decimal>;
    friend class ShortInstrumentPositionState<Decimal>;
    friend class InMarketPositionState<Decimal>;

  private:
    std::string mInstrumentSymbol;
    std::shared_ptr<InstrumentPositionState<Decimal>> mInstrumentPositionState;
  };

  template <class Decimal>
  inline void FlatInstrumentPositionState<Decimal>::addPosition(InstrumentPosition<Decimal>* iPosition,
							     std::shared_ptr<TradingPosition<Decimal>> position)
    {
      if (position->isLongPosition())
	iPosition->ChangeState (std::make_shared<LongInstrumentPositionState<Decimal>>(position));
      else if (position->isShortPosition())
	iPosition->ChangeState (std::make_shared<ShortInstrumentPositionState<Decimal>>(position));
      else
	throw InstrumentPositionException ("FlatInstrumentPositionState<Decimal>::addPosition: position is neither long or short");
      
    }
}

#endif
