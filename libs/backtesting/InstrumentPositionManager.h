// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#pragma once

#include <memory>
#include <map>
#include <vector>
#include <cstdint>
#include <utility>  // for std::move, std::swap
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
   * Binding Optimization Note:
   * - `addBarForOpenPosition` uses an internal binding cache (mBindings) that pairs InstrumentPosition* with
   *   Security* to avoid repeated map lookups on every bar.
   * - The cache is rebuilt when:
   *     - instruments are added (bindings become "dirty"), or
   *     - the portfolio pointer changes, or
   *     - bindings are empty.
   * - If the portfolio contents change (e.g., new Security added) without changing the portfolio pointer,
   *   callers may invoke rebindToPortfolio(...) to force a rebuild.
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
        mBindings(),
        mBoundPortfolio(nullptr),
        mBindingsDirty(true)
      {}

    /**
     * @brief Copy constructor.
     * @param rhs The InstrumentPositionManager to copy.
     */
    InstrumentPositionManager (const InstrumentPositionManager<Decimal>& rhs)
      : mInstrumentPositions(),
        mBindings(),
        mBoundPortfolio(rhs.mBoundPortfolio),
        mBindingsDirty(true)
    {
      // Deep copy of InstrumentPosition objects
      for (const auto& pair : rhs.mInstrumentPositions)
      {
        auto instrumentCopy = std::make_shared<InstrumentPosition<Decimal>>(*pair.second);
        mInstrumentPositions[pair.first] = instrumentCopy;
      }
      // Note: mBindings will be rebuilt on first use since mBindingsDirty = true
    }

    /**
     * @brief Move constructor.
     * @param rhs The InstrumentPositionManager to move from.
     *
     * Transfers ownership of resources from rhs to this object.
     * After the move, rhs is left in a valid but unspecified state.
     */
    InstrumentPositionManager (InstrumentPositionManager<Decimal>&& rhs) noexcept
      : mInstrumentPositions(std::move(rhs.mInstrumentPositions)),
        mBindings(std::move(rhs.mBindings)),
        mBoundPortfolio(rhs.mBoundPortfolio),
        mBindingsDirty(rhs.mBindingsDirty)
    {
      rhs.mBoundPortfolio = nullptr;
      rhs.mBindingsDirty = true;
    }

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

      // Deep copy InstrumentPosition objects (match copy ctor semantics)
      mInstrumentPositions.clear();
      for (const auto& pair : rhs.mInstrumentPositions)
	{
	  if (pair.second)
	    {
	      auto instrumentCopy = std::make_shared<InstrumentPosition<Decimal>>(*pair.second);
	      mInstrumentPositions[pair.first] = instrumentCopy;
	    }
	  else
	    {
	      mInstrumentPositions[pair.first] = nullptr;
	    }
	}

      // Never copy raw pointer bindings across managers; rebuild lazily.
      mBindings.clear();
      
      mBoundPortfolio = rhs.mBoundPortfolio;
      mBindingsDirty = true;
      
      return *this;
    }
    
    /**
     * @brief Move assignment operator.
     * @param rhs The InstrumentPositionManager to move from.
     * @return A reference to this manager.
     *
     * Transfers ownership of resources from rhs to this object.
     * After the move, rhs is left in a valid but unspecified state.
     */
    InstrumentPositionManager<Decimal>&
    operator=(InstrumentPositionManager<Decimal>&& rhs) noexcept
    {
      if (this == &rhs)
        return *this;

      mInstrumentPositions = std::move(rhs.mInstrumentPositions);
      mBindings = std::move(rhs.mBindings);
      mBoundPortfolio = rhs.mBoundPortfolio;
      mBindingsDirty = rhs.mBindingsDirty;

      rhs.mBoundPortfolio = nullptr;
      rhs.mBindingsDirty = true;

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

          // New instrument => cached bindings are now stale/incomplete.
          mBindingsDirty = true;
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
     */
    void addBarForOpenPosition (const ptime openPositionDateTime,
                                Portfolio<Decimal>* portfolioOfSecurities)
    {
      if (portfolioOfSecurities == nullptr)
      {
        throw InstrumentPositionManagerException("InstrumentPositionManager::addBarForOpenPosition - portfolioOfSecurities is null");
      }

      ensureBindingsUpToDate(portfolioOfSecurities);

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
     * @brief Forces a binding rebuild against the given portfolio.
     *
     * Useful when:
     * - the portfolio pointer stays the same but portfolio contents change (e.g., security added later), or
     * - the caller wants to pay the rebind cost explicitly at a controlled point.
     */
    void rebindToPortfolio(Portfolio<Decimal>* portfolioOfSecurities)
    {
      if (portfolioOfSecurities == nullptr)
      {
        throw InstrumentPositionManagerException("InstrumentPositionManager::rebindToPortfolio - portfolioOfSecurities is null");
      }
      bindToPortfolio(portfolioOfSecurities);
    }

    /**
     * @brief Closes all open trading position units for a specific instrument using date.
     */
    void closeAllPositions(const std::string& tradingSymbol,
                           const boost::gregorian::date exitDate,
                           const Decimal& exitPrice)
    {
      this->closeAllPositions(tradingSymbol, ptime(exitDate, getDefaultBarTime()), exitPrice);
    }

    /**
     * @brief Closes all open trading position units for a specific instrument using ptime.
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
     */
    uint32_t getNumPositionUnits(const std::string& symbol) const
    {
      std::shared_ptr<InstrumentPosition<Decimal>> pos = findExistingInstrumentPosition (symbol);
      return pos->getNumPositionUnits ();
    }

    /**
     * @brief Retrieves a specific trading position unit for an instrument.
     */
    std::shared_ptr<TradingPosition<Decimal>>
    getTradingPosition (const std::string& symbol, uint32_t unitNumber) const
    {
      std::shared_ptr<InstrumentPosition<Decimal>> pos = findExistingInstrumentPosition (symbol);
      return *(pos->getInstrumentPosition(unitNumber));
    }

  private:
    void ensureBindingsUpToDate(Portfolio<Decimal>* portfolioOfSecurities)
    {
      // Rebuild if:
      // - never built, or
      // - instrument set changed since last bind, or
      // - portfolio pointer changed.
      if (mBindings.empty() || mBindingsDirty || (portfolioOfSecurities != mBoundPortfolio))
        {
          bindToPortfolio(portfolioOfSecurities);
        }
    }

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

      // Cache is now consistent with current instruments and this portfolio pointer.
      mBoundPortfolio = portfolioOfSecurities;
      mBindingsDirty = false;
    }

    /**
     * @brief Finds an existing instrument by symbol and returns an iterator to it.
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
     */
    const std::shared_ptr<InstrumentPosition<Decimal>>&
    findExistingInstrumentPosition (const std::string& symbol) const
    {
      ConstInstrumentPositionIterator pos = findExistingSymbol (symbol);
      return pos->second;
    }

    /**
     * @brief Retrieves a const shared pointer to the InstrumentPosition for a given trading symbol.
     */
    const std::shared_ptr<InstrumentPosition<Decimal>>&
    getInstrumentPositionPtr(const std::string& tradingSymbol) const
    {
      ConstInstrumentPositionIterator pos = findExistingSymbol (tradingSymbol);
      return pos->second;
    }

  private:
    std::map<std::string, std::shared_ptr<InstrumentPosition<Decimal>>> mInstrumentPositions;

    // Cached bindings to avoid repeated lookups on every bar update.
    std::vector<std::pair<InstrumentPosition<Decimal>*, Security<Decimal>*>> mBindings;

    // Portfolio pointer used to build mBindings (non-owning).
    Portfolio<Decimal>* mBoundPortfolio;

    // True if instruments changed since last bind (e.g., addInstrument).
    bool mBindingsDirty;
  };
}
