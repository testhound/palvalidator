// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __INSTRUMENT_POSITION_MANAGER_H
#define __INSTRUMENT_POSITION_MANAGER_H 1

#include <memory>
#include <map>
#include <vector>
#include <cstdint>
#include "ThrowAssert.hpp"
#include "InstrumentPositionManagerException.h"
#include "InstrumentPosition.h"
#include "Portfolio.h"

namespace mkc_timeseries
{
  /**
   * @class InstrumentPositionManager
   * @brief Manages a collection of InstrumentPosition objects, each representing the net position for a specific trading instrument.
   *
   * @tparam Decimal The decimal type used for financial calculations.
   *
   * @details
   * This class acts as a central repository for the current state of all positions across various financial instruments
   * within a trading strategy or backtest. It maps a trading symbol (string) to a shared pointer of an
   * `InstrumentPosition<Decimal>` object. The `InstrumentPosition` itself manages the details of being long,
   * short, or flat, and can consist of one or more individual `TradingPosition` units (e.g., when pyramiding).
   *
   * Key Responsibilities:
   * - Storing and providing access to `InstrumentPosition` objects for each traded symbol.
   * - Adding new instruments to be tracked.
   * - Adding new `TradingPosition` units to the appropriate `InstrumentPosition` when an order is filled.
   * - Updating all open positions with new market data (bars) during a backtesting loop via `addBarForOpenPosition`.
   * - Facilitating the closure of positions, either individual units or all units for an instrument.
   * - Providing query methods to determine if an instrument is long, short, or flat, and its total volume.
   *
   * In a Backtesting Context:
   * - The `StrategyBroker` relies heavily on the `InstrumentPositionManager` to:
   * - Determine current position states before placing new orders.
   * - Add new `TradingPosition` objects when entry orders are filled.
   * - Instruct the manager to close positions when exit orders are filled.
   * - The `TradingOrderManager` may also query position states via the `StrategyBroker` to validate
   * or process certain order types (e.g., ensuring an exit order corresponds to an existing position).
   * - The `addBarForOpenPosition` method is typically called by the `StrategyBroker`
   * (e.g., within its `ProcessPendingOrders` method)
   * at each step of the backtest to update all open positions with the latest market data. This is crucial for
   * mark-to-market calculations, and for checking if stop-loss or profit-target levels within individual `TradingPosition`
   * units have been hit by the current bar's high or low prices.
   *
   * Collaboration:
   * - Manages `InstrumentPosition<Decimal>` objects.
   * - `InstrumentPosition<Decimal>` objects, in turn, manage one or more `TradingPosition<Decimal>` units.
   * - Receives new `TradingPosition<Decimal>` objects from components like `StrategyBroker`.
   * - Uses `Portfolio<Decimal>` to fetch security data for updating positions with new bars.
   */
  template <class Decimal> class InstrumentPositionManager
  {
  public:
    typedef typename std::map<std::string, std::shared_ptr<InstrumentPosition<Decimal>>>::const_iterator ConstInstrumentPositionIterator;

  public:
    /**
     * @brief Default constructor. Initializes an empty manager.
     */
    InstrumentPositionManager()
      : mInstrumentPositions(),
	mBindings()
      {}

    /**
     * @brief Copy constructor.
     * @param rhs The InstrumentPositionManager to copy.
     */
    InstrumentPositionManager (const InstrumentPositionManager<Decimal>& rhs)
      : mInstrumentPositions(rhs.mInstrumentPositions),
	mBindings(rhs.mBindings)
    {}

    /**
     * @brief Assignment operator.
     * @param rhs The InstrumentPositionManager to assign from.
     * @return A reference to this manager.
     */
    InstrumentPositionManager<Decimal>& 
    operator=(const InstrumentPositionManager<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mInstrumentPositions = rhs.mInstrumentPositions;
      mBindings = rhs.mBindings;

      return *this;
    }

    /**
     * @brief Destructor.
     */
    ~InstrumentPositionManager()
      {}

    /**
     * @brief Gets the total trading volume for all open units of a specific instrument.
     * @param tradingSymbol The symbol of the instrument.
     * @return The total TradingVolume.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     * @throws InstrumentPositionException if the instrument is flat (delegated from InstrumentPosition).
     */
    TradingVolume getVolumeInAllUnits(const std::string& tradingSymbol) const
    {
      return getInstrumentPosition(tradingSymbol).getVolumeInAllUnits();
    }

    /**
     * @brief Retrieves a constant reference to the InstrumentPosition for a given trading symbol.
     * @param tradingSymbol The symbol of the instrument.
     * @return A const reference to the InstrumentPosition.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     */
    const InstrumentPosition<Decimal>&
    getInstrumentPosition(const std::string& tradingSymbol) const
    {
      auto ptr = getInstrumentPositionPtr (tradingSymbol);
      return *ptr;
    }

    /**
     * @brief Retrieves a constant reference to the InstrumentPosition using an iterator.
     * @param it A ConstInstrumentPositionIterator pointing to the desired instrument.
     * @return A const reference to the InstrumentPosition.
     */
    const InstrumentPosition<Decimal>&
    getInstrumentPosition(ConstInstrumentPositionIterator it) const
    {
      return *(it->second);
    }

    /**
     * @brief Checks if there is an open long position for the specified trading symbol.
     * @param tradingSymbol The symbol of the instrument.
     * @return True if a long position exists, false otherwise.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     */
    bool isLongPosition(const std::string& tradingSymbol) const
    {
       return getInstrumentPositionPtr (tradingSymbol)->isLongPosition();
    }

    /**
     * @brief Checks if there is an open short position for the specified trading symbol.
     * @param tradingSymbol The symbol of the instrument.
     * @return True if a short position exists, false otherwise.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     */
    bool isShortPosition(const std::string& tradingSymbol) const
    {
      return getInstrumentPositionPtr (tradingSymbol)->isShortPosition();
    }

    /**
     * @brief Checks if there is no open position (flat) for the specified trading symbol.
     * @param tradingSymbol The symbol of the instrument.
     * @return True if the position is flat, false otherwise.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     */
    bool isFlatPosition(const std::string& tradingSymbol) const
    {
      return getInstrumentPositionPtr (tradingSymbol)->isFlatPosition();
    }

    /**
     * @brief Returns a constant iterator to the beginning of the managed instrument positions.
     * @return A ConstInstrumentPositionIterator.
     */
    ConstInstrumentPositionIterator beginInstrumentPositions() const
    {
      return mInstrumentPositions.begin();
    }

    /**
     * @brief Returns a constant iterator to the end of the managed instrument positions.
     * @return A ConstInstrumentPositionIterator.
     */
    ConstInstrumentPositionIterator endInstrumentPositions() const
    {
      return mInstrumentPositions.end();
    }

    /**
     * @brief Gets the number of instruments currently being managed.
     * @return The count of instruments.
     */
    uint32_t getNumInstruments() const
    {
      return mInstrumentPositions.size();
    }

    /**
     * @brief Adds a new instrument to be managed.
     * If the instrument symbol already exists, an exception is thrown.
     * An `InstrumentPosition` object in a flat state is created for the new symbol.
     * @param tradingSymbol The symbol of the instrument to add.
     * @throws InstrumentPositionManagerException if the trading symbol already exists.
     */
    void addInstrument (const std::string& tradingSymbol)
    {
      ConstInstrumentPositionIterator pos = mInstrumentPositions.find (tradingSymbol);
      
      if (pos == endInstrumentPositions())
	{
	  auto instrPos = std::make_shared<InstrumentPosition<Decimal>>(tradingSymbol);
	  mInstrumentPositions.insert(std::make_pair(tradingSymbol, instrPos));
	}
      else
	throw InstrumentPositionManagerException("InstrumentPositionManager::addInstrument - trading symbol already exists");
    }

    /**
     * @brief Adds a new trading position unit to the corresponding instrument.
     * This is typically called when an entry order for an instrument is filled.
     * The method delegates the addition to the specific `InstrumentPosition` object associated with the trading symbol.
     * @param position A shared pointer to the TradingPosition unit to add.
     *
     * @throws InstrumentPositionManagerException if the trading symbol of the position is not found.
     *
     * @throws InstrumentPositionException if the position cannot be added
     * (e.g., adding a closed position, symbol mismatch, direction mismatch).
     */
    void addPosition(std::shared_ptr<TradingPosition<Decimal>> position)
    {
      getInstrumentPositionPtr (position->getTradingSymbol())->addPosition(position);
    }

   /**
     * @brief Adds a new bar's data to all open trading position units for a specific instrument using date.
     * This is used to update an open position with new market data as the simulation progresses.
     * @param tradingSymbol The symbol of the instrument whose positions should be updated.
     * @param entryBar The OHLCTimeSeriesEntry (bar data) to add.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     * @throws InstrumentPositionException if the instrument is flat (delegated from InstrumentPosition).
     */ 
    void addBar (const std::string& tradingSymbol,
		 const OHLCTimeSeriesEntry<Decimal>& entryBar)
    {
      getInstrumentPositionPtr (tradingSymbol)->addBar (entryBar);
    }

    /**
     * @brief Adds a new bar's data to all open positions based on date.
     * This overload uses only the date portion of the `ptime` object to fetch and add bar data.
     * It iterates all managed instruments and updates their positions if they are open
     * and have a corresponding bar in the portfolio for the given date.
     * 
     * @note This method is distinct from the overload that uses `ptime` as its parameter. 
     *       While the `ptime` overload considers both date and time, this method focuses solely on the date.
     * 
     * @param openPositionDate The date for which to fetch and add bar data.
     * @param portfolioOfSecurities A pointer to the Portfolio containing security data.
     */
    void addBarForOpenPosition (const boost::gregorian::date openPositionDate,
				Portfolio<Decimal>* portfolioOfSecurities)
    {
        this->addBarForOpenPosition(ptime(openPositionDate, getDefaultBarTime()), portfolioOfSecurities);
    }

    /**
     * @brief Adds a new bar's data to all open positions based on ptime.
     * This version iterates all managed instruments and updates their positions if they are open
     * and have a corresponding bar in the portfolio for the given ptime.
     * @param openPositionDateTime The ptime for which to fetch and add bar data.
     * @param portfolioOfSecurities A pointer to the Portfolio containing security data.
     */
    void addBarForOpenPosition (const ptime openPositionDateTime,
				Portfolio<Decimal>* portfolioOfSecurities)
    {
      // bind once on first bar
      if (mBindings.empty())
      {
	    bindToPortfolio(portfolioOfSecurities);
      }
      
      for (auto& binding : mBindings)
        {
	  auto* position = binding.first;
	  auto* security = binding.second;
	  
	  // only add if the position is currently open
	  if (position->isFlatPosition())
	  {
	    continue;
	  }
	  
	  // pull the bar from the OHLCTimeSeries
	         try
	         {
	           auto entry = security->getTimeSeriesEntry(openPositionDateTime);
	           position->addBar(entry);
	         }
	         catch (const mkc_timeseries::TimeSeriesDataNotFoundException&)
	         {
	           // No data available for this date/time, skip this position update
	           continue;
	         }
        }
    }

    /**
     * @brief Closes all open trading position units for a specific instrument using date.
     * @param tradingSymbol The symbol of the instrument whose positions are to be closed.
     * @param exitDate The date of the exit.
     * @param exitPrice The price at which the positions are exited.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     * @throws InstrumentPositionException if the instrument is already flat (delegated from InstrumentPosition).
     */
    void closeAllPositions(const std::string& tradingSymbol,
			   const boost::gregorian::date exitDate,
			   const Decimal& exitPrice)
    {
      this->closeAllPositions(tradingSymbol, ptime(exitDate, getDefaultBarTime()), exitPrice);
    }

    /**
     * @brief Closes all open trading position units for a specific instrument using ptime.
     * @param tradingSymbol The symbol of the instrument whose positions are to be closed.
     * @param exitDateTime The ptime of the exit.
     * @param exitPrice The price at which the positions are exited.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     * @throws InstrumentPositionException if the instrument is already flat (delegated from InstrumentPosition).
     */
    void closeAllPositions(const std::string& tradingSymbol,
			   const ptime exitDateTime,
			   const Decimal& exitPrice)
    {
      std::shared_ptr<InstrumentPosition<Decimal>> pos = findExistingInstrumentPosition (tradingSymbol);
      pos->closeAllPositions(exitDateTime, exitPrice);
    }

    /**
     * @brief Closes a specific trading position unit for an instrument using date.
     * This is used when pyramiding and exiting only a part of the total position.
     * @param tradingSymbol The symbol of the instrument.
     * @param exitDate The date of the exit.
     * @param exitPrice The price at which the unit is exited.
     * @param unitNumber The specific unit number of the TradingPosition to close (1-based index).
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     * @throws InstrumentPositionException if the unit number is invalid or the unit cannot be
     * closed (delegated from InstrumentPosition).
     */
    void closeUnitPosition(const std::string& tradingSymbol,
			   const boost::gregorian::date exitDate,
			   const Decimal& exitPrice,
			   uint32_t unitNumber)
    {
      this->closeUnitPosition(tradingSymbol, ptime(exitDate, getDefaultBarTime()), exitPrice, unitNumber);
    }

    /**
     * @brief Closes a specific trading position unit for an instrument using ptime.
     * This is used when pyramiding and exiting only a part of the total position.
     * @param tradingSymbol The symbol of the instrument.
     * @param exitDateTime The ptime of the exit.
     * @param exitPrice The price at which the unit is exited.
     * @param unitNumber The specific unit number of the TradingPosition to close (1-based index).
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     * @throws InstrumentPositionException if the unit number is invalid or the unit cannot be
     * closed (delegated from InstrumentPosition).
     */
    void closeUnitPosition(const std::string& tradingSymbol,
			   const ptime exitDateTime,
			   const Decimal& exitPrice,
			   uint32_t unitNumber)
    {
      std::shared_ptr<InstrumentPosition<Decimal>> pos = findExistingInstrumentPosition (tradingSymbol);
      pos->closeUnitPosition(exitDateTime, exitPrice, unitNumber);
    }


    /**
     * @brief Gets the number of open trading position units for a specific instrument.
     * Useful for strategies that allow pyramiding.
     * @param symbol The trading symbol of the instrument.
     * @return The number of open units.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     */
    uint32_t getNumPositionUnits(const std::string& symbol) const
    {
      std::shared_ptr<InstrumentPosition<Decimal>> pos = findExistingInstrumentPosition (symbol);
      return pos->getNumPositionUnits ();
    }

     /**
     * @brief Retrieves a specific trading position unit for an instrument.
     * @param symbol The trading symbol of the instrument.
     * @param unitNumber The 1-based index of the trading position unit to retrieve.
     * @return A shared pointer to the specified TradingPosition unit.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     * @throws InstrumentPositionException if the unit number is out of range (delegated from InstrumentPosition).
     */
    std::shared_ptr<TradingPosition<Decimal>>
    getTradingPosition (const std::string& symbol, uint32_t unitNumber) const
    {
      std::shared_ptr<InstrumentPosition<Decimal>> pos = findExistingInstrumentPosition (symbol);
      
      return *(pos->getInstrumentPosition(unitNumber));
    }

    
  private:
    void bindToPortfolio(Portfolio<Decimal>* portfolioOfSecurities)
    {
      mBindings.clear();
      mBindings.reserve(mInstrumentPositions.size());

      for (auto itPos = beginInstrumentPositions();
	   itPos != endInstrumentPositions();
	   ++itPos)
        {
	  InstrumentPosition<Decimal>* position = itPos->second.get();
	  // look up the matching security in the portfolio
	  auto secIt = portfolioOfSecurities
	    ->findSecurity(position->getInstrumentSymbol());
	  if (secIt != portfolioOfSecurities->endPortfolio())
            {
	      // capture raw pointers—no shared_ptr copies
	      mBindings.emplace_back(position, secIt->second.get());
            }
        }
    }

    /**
     * @brief Finds an existing instrument by symbol and returns an iterator to it.
     * @param symbol The trading symbol to find.
     * @return A ConstInstrumentPositionIterator to the found instrument.
     * @throws InstrumentPositionManagerException if the trading symbol is not found.
     */
    ConstInstrumentPositionIterator findExistingSymbol (const std::string& symbol) const
    {
      ConstInstrumentPositionIterator pos = mInstrumentPositions.find (symbol);
      if (pos != endInstrumentPositions())
      {
	return pos;
      }
      else
      {
	throw InstrumentPositionManagerException("InstrumentPositionManager::findExistingSymbol - trading symbol '" + symbol + "' not found");
      }
    }

    /**
     * @brief Finds an existing InstrumentPosition by symbol and returns a const shared pointer to it.
     * Internal helper method.
     * @param symbol The trading symbol to find.
     * @return A const shared_ptr to the InstrumentPosition.
     * @throws InstrumentPositionManagerException if the trading symbol is not found (via findExistingSymbol).
     */
    const std::shared_ptr<InstrumentPosition<Decimal>>& 
    findExistingInstrumentPosition (const std::string symbol) const
    {
      ConstInstrumentPositionIterator pos = findExistingSymbol (symbol);
      return pos->second;
    }

    /**
     * @brief Retrieves a const shared pointer to the InstrumentPosition for a given trading symbol.
     * Internal helper method that ensures the symbol exists.
     * @param tradingSymbol The symbol of the instrument.
     * @return A const shared_ptr to the InstrumentPosition.
     * @throws InstrumentPositionManagerException if the trading symbol is not found (via findExistingSymbol).
     */
    const std::shared_ptr<InstrumentPosition<Decimal>>&
    getInstrumentPositionPtr(const std::string& tradingSymbol) const
    {
      ConstInstrumentPositionIterator pos = findExistingSymbol (tradingSymbol);
      return pos->second;
    }

  private:
    std::map<std::string, std::shared_ptr<InstrumentPosition<Decimal>>> mInstrumentPositions;
    std::vector<std::pair<InstrumentPosition<Decimal>*, Security<Decimal>*>> mBindings;
  };
}

#endif
