// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __BACKTESTER_STRATEGY_H
#define __BACKTESTER_STRATEGY_H 1

#include <exception>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/functional/hash.hpp>
#include "Portfolio.h"
#include "InstrumentPosition.h"
#include "StrategyBroker.h"
#include "SecurityBacktestProperties.h"
#include "PatternPositionRegistry.h"


namespace mkc_timeseries
{
  using boost::gregorian::date;
  using boost::posix_time::ptime;

  class StrategyOptions
  {
  public:
    StrategyOptions (bool pyramidingEnabled, unsigned int maxPyramidPositions, unsigned int maxHoldingPeriod)
      : mPyramidPositions (pyramidingEnabled),
	mMaxPyramidPositions(maxPyramidPositions),
	mMaxHoldingPeriod(maxHoldingPeriod)
    {}

    bool isPyramidingEnabled() const
      {
	return mPyramidPositions;
      }

    unsigned int getMaxPyramidPositions() const
      {
	return mMaxPyramidPositions;
      }

    unsigned int getMaxHoldingPeriod() const
    {
      return mMaxHoldingPeriod;
    }
    

  private:
    bool mPyramidPositions;
    unsigned int mMaxPyramidPositions;
    unsigned int mMaxHoldingPeriod;
  };

  extern StrategyOptions defaultStrategyOptions;

  /**
   * @class BacktesterStrategy
   * @brief Base class for trading strategies used during backtesting.
   *
   * Responsibilities:
   * - Define strategy-specific entry and exit rules (pure virtual hooks).
   * - Submit orders using helpers like EnterLongOnOpen or ExitLongAllUnitsAtStop.
   * - Track pyramiding status, order state, and current simulation bar.
   * - Delegate execution responsibilities to a StrategyBroker instance.
   *
   * Observer Pattern Collaboration:
   * - Acts as a producer of orders, not an observer.
   * - Delegates order submission to StrategyBroker.
   * - Receives callbacks indirectly via changes in position state.
   *
   * Collaborators:
   * - StrategyBroker: receives order requests and manages lifecycle.
   * - BackTester: invokes strategy events on each simulation step.
   */
  template <class Decimal> class BacktesterStrategy
    {
    private:
      static boost::uuids::random_generator sUuidGenerator;
      
    public:
      using Broker = StrategyBroker<Decimal,
                                    NysePre2001Fractions,                // simulate 1/8 → 1/16 → decimals
                                    Rule612SubPenny,                     // type name…
                                    /*PricesAreSplitAdjusted=*/true>;    // …but sub-penny under $1 is OFF

      typedef typename Portfolio<Decimal>::ConstPortfolioIterator PortfolioIterator;

      /**
       * @brief Copy constructor - generate NEW UUID for cloned strategies.
       * @param rhs  Strategy to clone state from (name, broker, portfolio, etc.).
       */
      BacktesterStrategy(const BacktesterStrategy<Decimal>& rhs)
 : mStrategyName(rhs.mStrategyName),
   mBroker(rhs.mBroker),
   mPortfolio(rhs.mPortfolio),
   mSecuritiesProperties(rhs.mSecuritiesProperties),
   mStrategyOptions(rhs.mStrategyOptions),
   mInstanceId(sUuidGenerator())  // NEW UUID for clone
      {}

      /**
       * @brief Assignment operator.
       * @param rhs  Strategy to copy state from.
       * @return Reference to *this.
       */
      const BacktesterStrategy<Decimal>&
      operator=(const BacktesterStrategy<Decimal>& rhs)
      {
 if (this == &rhs)
   return *this;

 mStrategyName = rhs.mStrategyName;
 mBroker = rhs.mBroker;
 mPortfolio = rhs.mPortfolio;
 mSecuritiesProperties = rhs.mSecuritiesProperties;
 mStrategyOptions = rhs.mStrategyOptions;
 mInstanceId = sUuidGenerator();  // Generate NEW UUID for assignment
     
 return *this;
      }

      /**
       * @brief Retrieve this strategy’s unique name.
       * @return The name given at construction.
       */
      const std::string& getStrategyName() const
      {
	return mStrategyName;
      }

      virtual ~BacktesterStrategy()
      {}

      /**
       * @brief Get the unique instance ID for this strategy
       * @return UUID that uniquely identifies this strategy instance
       */
      const boost::uuids::uuid& getInstanceId() const {
          return mInstanceId;
      }

      virtual unsigned long long deterministicHashCode() const
      {
	// Default: just use regular hashCode
	// Derived classes can override for deterministic behavior
	return hashCode();
      }

      /**
       * @brief Get unique hash combining instance ID with pattern-specific hash
       * @return Combined hash for unique strategy identification
       * @note This method should be overridden by derived classes to include pattern hash
       */
      virtual unsigned long long hashCode() const {
          // Base implementation uses only UUID
          boost::hash<boost::uuids::uuid> uuid_hasher;
          return uuid_hasher(mInstanceId);
      }

      /**
       * @brief Called once per bar to submit exit orders (profit‐target, stop‐loss, etc.).
       *
       * @details
       * Within each simulation step, BackTester does:
       *   1. strategy->eventUpdateSecurityBarNumber(symbol);
       *   2. if not flat: strategy->eventExitOrders(...);
       *   3. strategy->eventEntryOrders(...);
       *
       * Exits are processed before new entries so that:
       *  - Exiting positions can free up capital or pyramid slots.
       *  - You never simultaneously hold overlapping exit and entry orders for the same security.
       *  - The bar‐by‐bar return series (via getAllHighResReturns) will include any exit
       *    fill P&L on that bar, since StrategyBroker marks to market before executing fills.
       *
       * @param aSecurity       Pointer to the security being evaluated.
       * @param instrPos        Current multi‐unit InstrumentPosition for that security.
       * @param processingDate  Date (or timestamp) of this bar.
       */
      void eventExitOrders (Security<Decimal>* aSecurity,
       const InstrumentPosition<Decimal>& instrPos,
       const date& processingDate)
      {
        eventExitOrders(aSecurity, instrPos, ptime(processingDate, getDefaultBarTime()));
      }

      /**
       * @brief Called once per bar to submit exit orders using ptime (pure virtual).
       * @param aSecurity         Pointer to the security being evaluated.
       * @param instrPos          Current multi‐unit InstrumentPosition for that security.
       * @param processingDateTime DateTime of this bar.
       */
      virtual void eventExitOrders (Security<Decimal>* aSecurity,
        const InstrumentPosition<Decimal>& instrPos,
        const ptime& processingDateTime) = 0;

      /**
       * @brief Called once per bar to submit new entry orders based on strategy signals.
       *
       * @details
       * After exits are submitted, BackTester invokes this to allow the strategy to:
       *  - Check pattern triggers or indicator signals on the current bar.
       *  - Submit `EnterLongOnOpen` or `EnterShortOnOpen` with attached stops/targets.
       *  - Respect pyramiding rules and maximum position sizes.
       *
       * Entries run second so that:
       *  - You enter only after evaluating whether existing positions have exited.
       *  - Fresh capital or free pyramiding slots are available for new trades.
       *
       * @param aSecurity       Pointer to the security being evaluated.
       * @param instrPos        Current multi‐unit InstrumentPosition for that security.
       * @param processingDate  Date (or timestamp) of this bar.
       */
      void eventEntryOrders (Security<Decimal>* aSecurity,
        const InstrumentPosition<Decimal>& instrPos,
        const date& processingDate)
      {
        eventEntryOrders(aSecurity, instrPos, ptime(processingDate, getDefaultBarTime()));
      }

      /**
       * @brief Called once per bar to submit new entry orders using ptime (pure virtual).
       * @param aSecurity         Pointer to the security being evaluated.
       * @param instrPos          Current multi‐unit InstrumentPosition for that security.
       * @param processingDateTime DateTime of this bar.
       */
      virtual void eventEntryOrders (Security<Decimal>* aSecurity,
				     const InstrumentPosition<Decimal>& instrPos,
				     const ptime& processingDateTime) = 0;

       /**
	* @brief Determine the order size (shares/contracts) for aSecurity.
	* @param aSecurity  Security whose order size is requested.
	* @return A TradingVolume object indicating units to trade.
	*/
      virtual const TradingVolume& getSizeForOrder(const Security<Decimal>& aSecurity) const = 0;

      virtual std::shared_ptr<BacktesterStrategy<Decimal>>
      clone (const std::shared_ptr<Portfolio<Decimal>>& portfolio) const = 0;

      virtual std::shared_ptr<BacktesterStrategy<Decimal>>
      cloneForBackTesting () const = 0;

      virtual std::shared_ptr<BacktesterStrategy<Decimal>>
      clone_shallow(const std::shared_ptr<Portfolio<Decimal>>& portfolio) const
      {
	// Fallback: deep clone
	return clone(portfolio);
      }

      virtual std::vector<int> getPositionDirectionVector() const = 0;

      virtual std::vector<Decimal> getPositionReturnsVector() const = 0;

      virtual unsigned long numTradingOpportunities() const = 0;

      /**
       * @brief Get the pattern's maximum bars back requirement.
       * @return Maximum lookback period required by the pattern.
       * @note All derived strategy classes must implement this method.
       */
      virtual uint32_t getPatternMaxBarsBack() const
      {
        return 0;
      }

      /**
       * @brief Get the maximum holding period for this strategy.
       * @return Maximum number of bars a position can be held, or 0 if unlimited.
       */
      unsigned int getMaxHoldingPeriod() const
      {
        return mStrategyOptions.getMaxHoldingPeriod();
      }

      /**
       * @brief Whether this strategy allows pyramiding (multiple units) by configuration.
       * @return True if pyramidingEnabled was set in StrategyOptions.
       */
      bool isPyramidingEnabled() const
      {
	  return mStrategyOptions.isPyramidingEnabled();
      }

      /**
       * @brief Maximum allowed pyramiding layers.
       * @return Configured maxPyramidPositions from StrategyOptions.
       */
      unsigned int getMaxPyramidPositions() const
      {
	return mStrategyOptions.getMaxPyramidPositions();
      }

      /**
       * @brief Check if we can pyramid another unit in tradingSymbol.
       * @param tradingSymbol  Ticker symbol to test.
       * @return True if current units < 1 + maxPyramidPositions.
       */
      bool strategyCanPyramid(const std::string& tradingSymbol) const
      {
	if (isPyramidingEnabled())
	  {
	    InstrumentPosition<Decimal> instrPos = getInstrumentPosition(tradingSymbol);

	    // We can pyramid if the number of open positions is < 1 (intial position) +
	    // number of positions we are allowed to pyramid into

	    return instrPos.getNumPositionUnits() < (1 + getMaxPyramidPositions());
	  }
	return false;
      }

      /**
       * @brief Query whether a flat/long/short position exists for tradingSymbol.
       * @param tradingSymbol  Ticker symbol to check.
       * @return True if currently long (or short / flat).
       */
      bool isLongPosition(const std::string& tradingSymbol) const
      {
	return mBroker.isLongPosition (tradingSymbol);
      }

      bool isShortPosition(const std::string& tradingSymbol) const
      {
	return mBroker.isShortPosition (tradingSymbol);
      }

      bool isFlatPosition(const std::string& tradingSymbol) const
      {
	return mBroker.isFlatPosition (tradingSymbol);
      }

      /**
       * @brief Iterate all securities in the strategy’s portfolio.
       * @return Const iterator to the first security.
       */
      PortfolioIterator beginPortfolio() const
      {
	return mPortfolio->beginPortfolio();
      }

      /**
       * @brief End iterator for the portfolio securities.
       * @return Const iterator one past the last security.
       */
      PortfolioIterator endPortfolio() const
      {
	return mPortfolio->endPortfolio();
      }

      uint32_t getNumSecurities() const
      {
	return mPortfolio->getNumSecurities();
      }

       /**
	* @brief Exit all units (long or short) at open price on orderDate.
	* @param tradingSymbol  Ticker to exit.
	* @param orderDate      Date when the exit is placed.
	*/
      void ExitAllPositions(const std::string& tradingSymbol,
       const date& orderDate)
      {
        ExitAllPositions(tradingSymbol, ptime(orderDate, getDefaultBarTime()));
      }

      /**
       * @brief Exit all units (long or short) at open price on orderDateTime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       */
      void ExitAllPositions(const std::string& tradingSymbol,
       const ptime& orderDateTime)
      {
	if (isLongPosition(tradingSymbol))
	  ExitLongAllUnitsAtOpen(tradingSymbol, orderDateTime);
	else if (isShortPosition(tradingSymbol))
	  ExitShortAllUnitsAtOpen(tradingSymbol, orderDateTime);
      }

      /**
       * @brief Submit a market‐on‐open entry order (long side).
       * @param tradingSymbol  Ticker to enter.
       * @param orderDate      Date of the entry bar.
       * @param stopLoss       Optional stop‐loss price.
       * @param profitTarget   Optional profit‐target price.
       */
      void EnterLongOnOpen(const std::string& tradingSymbol,
      const date& orderDate,
      const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
      const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
        EnterLongOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopLoss, profitTarget);
      }

      /**
       * @brief Submit a market‐on‐open entry order (long side) using ptime.
       * @param tradingSymbol    Ticker to enter.
       * @param orderDateTime    DateTime of the entry bar.
       * @param stopLoss         Optional stop‐loss price.
       * @param profitTarget     Optional profit‐target price.
       */
      void EnterLongOnOpen(const std::string& tradingSymbol,
			   const ptime& orderDateTime,
			   const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			   const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
	auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
	mBroker.EnterLongOnOpen (tradingSymbol, orderDateTime,
				 getSizeForOrder(*aSecurity),
				 stopLoss,
				 profitTarget);
      }

      /**
       * @brief Submit a market‐on‐open entry order (short side).
       * @param tradingSymbol  Ticker to enter short.
       * @param orderDate      Date of the entry bar.
       * @param stopLoss       Optional stop‐loss price.
       * @param profitTarget   Optional profit‐target price.
       */
      void EnterShortOnOpen(const std::string& tradingSymbol,
       const date& orderDate,
       const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
       const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
        EnterShortOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopLoss, profitTarget);
      }

      /**
       * @brief Submit a market‐on‐open entry order (short side) using ptime.
       * @param tradingSymbol    Ticker to enter short.
       * @param orderDateTime    DateTime of the entry bar.
       * @param stopLoss         Optional stop‐loss price.
       * @param profitTarget     Optional profit‐target price.
       */
      void EnterShortOnOpen(const std::string& tradingSymbol,
       const ptime& orderDateTime,
       const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
       const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
 auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
 mBroker.EnterShortOnOpen (tradingSymbol, orderDateTime,
      getSizeForOrder(*aSecurity),
      stopLoss,
      profitTarget);
      }

      /**
       * @brief Submit a pattern-aware market‐on‐open entry order (long side).
       * @param tradingSymbol  Ticker to enter.
       * @param orderDate      Date of the entry bar.
       * @param pattern        Pattern that triggered this entry
       * @param stopLoss       Optional stop‐loss price.
       * @param profitTarget   Optional profit‐target price.
       */
      void EnterLongOnOpenWithPattern(const std::string& tradingSymbol,
                                      const date& orderDate,
                                      std::shared_ptr<PriceActionLabPattern> pattern,
                                      const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                                      const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
          EnterLongOnOpenWithPattern(tradingSymbol, ptime(orderDate, getDefaultBarTime()),
                                    pattern, stopLoss, profitTarget);
      }

      /**
       * @brief Submit a pattern-aware market‐on‐open entry order (long side) using ptime.
       * @param tradingSymbol    Ticker to enter.
       * @param orderDateTime    DateTime of the entry bar.
       * @param pattern          Pattern that triggered this entry
       * @param stopLoss         Optional stop‐loss price.
       * @param profitTarget     Optional profit‐target price.
       */
      void EnterLongOnOpenWithPattern(const std::string& tradingSymbol,
                                      const ptime& orderDateTime,
                                      std::shared_ptr<PriceActionLabPattern> pattern,
                                      const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                                      const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
          auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
          
          // Use the pattern-aware broker method
          mBroker.EnterLongOnOpenWithPattern(tradingSymbol, orderDateTime, pattern,
                                            getSizeForOrder(*aSecurity),
                                            stopLoss, profitTarget);
      }

      /**
       * @brief Submit a pattern-aware market‐on‐open entry order (short side).
       * @param tradingSymbol  Ticker to enter short.
       * @param orderDate      Date of the entry bar.
       * @param pattern        Pattern that triggered this entry
       * @param stopLoss       Optional stop‐loss price.
       * @param profitTarget   Optional profit‐target price.
       */
      void EnterShortOnOpenWithPattern(const std::string& tradingSymbol,
                                       const date& orderDate,
                                       std::shared_ptr<PriceActionLabPattern> pattern,
                                       const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                                       const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
          EnterShortOnOpenWithPattern(tradingSymbol, ptime(orderDate, getDefaultBarTime()),
                                     pattern, stopLoss, profitTarget);
      }

      /**
       * @brief Submit a pattern-aware market‐on‐open entry order (short side) using ptime.
       * @param tradingSymbol    Ticker to enter short.
       * @param orderDateTime    DateTime of the entry bar.
       * @param pattern          Pattern that triggered this entry
       * @param stopLoss         Optional stop‐loss price.
       * @param profitTarget     Optional profit‐target price.
       */
      void EnterShortOnOpenWithPattern(const std::string& tradingSymbol,
                                       const ptime& orderDateTime,
                                       std::shared_ptr<PriceActionLabPattern> pattern,
                                       const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                                       const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
          auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
          
          // Use the pattern-aware broker method
          mBroker.EnterShortOnOpenWithPattern(tradingSymbol, orderDateTime, pattern,
                                             getSizeForOrder(*aSecurity),
                                             stopLoss, profitTarget);
      }

      /**
       * @brief Exit all long units at the open of orderDate.
       * @param tradingSymbol  Ticker to exit.
       * @param orderDate      Date when the exit is placed.
       */
      void ExitLongAllUnitsAtOpen(const std::string& tradingSymbol,
      const date& orderDate)
      {
	ExitLongAllUnitsAtOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()));
      }

      /**
       * @brief Exit all long units at the open of orderDateTime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       */
      void ExitLongAllUnitsAtOpen(const std::string& tradingSymbol,
      const ptime& orderDateTime)
      {
	mBroker.ExitLongAllUnitsOnOpen(tradingSymbol, orderDateTime);
      }

      /**
       * @brief Exit all long units at a hard limit price.
       * @overload
       * @param limitPrice     Absolute price to exit at.
       * @param limitBasePrice Base price for percent‐based exit.
       * @param percentNum     PercentNumber to compute the limit from base.
       */
      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				   const date& orderDate,
				   const Decimal& limitPrice)
      {
	ExitLongAllUnitsAtLimit(tradingSymbol,
				ptime(orderDate, getDefaultBarTime()),
				limitPrice);
      }

      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				   const date& orderDate,
				   const Decimal& limitBasePrice,
				   const PercentNumber<Decimal>& percentNum)
      {
	ExitLongAllUnitsAtLimit(tradingSymbol,
				ptime(orderDate, getDefaultBarTime()),
				limitBasePrice,
				percentNum);
      }

      /**
       * @brief Exit all long units at a hard limit price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param limitPrice       Absolute price to exit at.
       */
      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				   const ptime& orderDateTime,
				   const Decimal& limitPrice)
      {
	mBroker.ExitLongAllUnitsAtLimit (tradingSymbol, orderDateTime, limitPrice);
      }

      /**
       * @brief Exit all long units at a percent-based limit price using ptime.
       * @param tradingSymbol      Ticker to exit.
       * @param orderDateTime      DateTime when the exit is placed.
       * @param limitBasePrice     Base price for percent‐based exit.
       * @param percentNum         PercentNumber to compute the limit from base.
       */
      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				   const ptime& orderDateTime,
				   const Decimal& limitBasePrice,
				   const PercentNumber<Decimal>& percentNum)
      {
	mBroker.ExitLongAllUnitsAtLimit (tradingSymbol, orderDateTime,
					 limitBasePrice, percentNum);
      }

      /**
       * @brief Exit all short units at a hard limit price.
       * @overload
       */
      void ExitShortAllUnitsAtOpen(const std::string& tradingSymbol,
				   const date& orderDate)
      {
	ExitShortAllUnitsAtOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()));
      }

      /**
       * @brief Exit all short units at the open of orderDateTime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       */
      void ExitShortAllUnitsAtOpen(const std::string& tradingSymbol,
				   const ptime& orderDateTime)
      {
	mBroker.ExitShortAllUnitsOnOpen(tradingSymbol, orderDateTime);
      }

      /**
       * @brief Exit all short units at a hard limit price.
       * @overload
       */
      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				    const date& orderDate,
				    const Decimal& limitPrice)
      {
	ExitShortAllUnitsAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitPrice);
      }

      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				    const date& orderDate,
				    const Decimal& limitBasePrice,
				    const PercentNumber<Decimal>& percentNum)
      {
	ExitShortAllUnitsAtLimit(tradingSymbol,
				 ptime(orderDate, getDefaultBarTime()),
				 limitBasePrice, percentNum);
      }

      /**
       * @brief Exit all short units at a hard limit price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param limitPrice       Absolute price to exit at.
       */
      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				    const ptime& orderDateTime,
				    const Decimal& limitPrice)
      {
	mBroker.ExitShortAllUnitsAtLimit (tradingSymbol, orderDateTime, limitPrice);
      }

      /**
       * @brief Exit all short units at a percent-based limit price using ptime.
       * @param tradingSymbol      Ticker to exit.
       * @param orderDateTime      DateTime when the exit is placed.
       * @param limitBasePrice     Base price for percent‐based exit.
       * @param percentNum         PercentNumber to compute the limit from base.
       */
      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				    const ptime& orderDateTime,
				    const Decimal& limitBasePrice,
				    const PercentNumber<Decimal>& percentNum)
      {
	mBroker.ExitShortAllUnitsAtLimit (tradingSymbol, orderDateTime,
					  limitBasePrice, percentNum);
      }

      /**
       * @brief Exit long positions at a stop‐loss price.
       * @overload
       */
      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				  const date& orderDate,
				  const Decimal& stopPrice)
      {
	ExitLongAllUnitsAtStop(tradingSymbol,
			       ptime(orderDate, getDefaultBarTime()),
			       stopPrice);
      }

      /**
       * @brief Exit long positions at a stop‐loss price.
       * @overload
       */
      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				  const date& orderDate,
				  const Decimal& stopBasePrice,
				  const PercentNumber<Decimal>& percentNum)
      {
	ExitLongAllUnitsAtStop(tradingSymbol,
			       ptime(orderDate, getDefaultBarTime()),
			       stopBasePrice,
			       percentNum);
      }

      /**
       * @brief Exit long positions at a stop‐loss price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopPrice        Absolute stop price.
       */
      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				  const ptime& orderDateTime,
				  const Decimal& stopPrice)
      {
	mBroker.ExitLongAllUnitsAtStop (tradingSymbol, orderDateTime, stopPrice);
      }

      /**
       * @brief Exit long positions at a percent-based stop‐loss price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopBasePrice    Base price for percent‐based stop.
       * @param percentNum       PercentNumber to compute the stop from base.
       */
      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				  const ptime& orderDateTime,
				  const Decimal& stopBasePrice,
				  const PercentNumber<Decimal>& percentNum)
      {
	mBroker.ExitLongAllUnitsAtStop (tradingSymbol, orderDateTime,
					stopBasePrice, percentNum);
      }

      /**
       * @brief Exit short positions at a stop‐loss price.
       * @overload
       */
      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				   const date& orderDate,
				   const Decimal& stopPrice)
      {
	ExitShortAllUnitsAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopPrice);
      }

      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				   const date& orderDate,
				   const Decimal& stopBasePrice,
				   const PercentNumber<Decimal>& percentNum)
      {
	ExitShortAllUnitsAtStop(tradingSymbol,
				ptime(orderDate, getDefaultBarTime()),
				stopBasePrice, percentNum);
      }

      /**
       * @brief Exit short positions at a stop‐loss price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopPrice        Absolute stop price.
       */
      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				   const ptime& orderDateTime,
				   const Decimal& stopPrice)
      {
	mBroker.ExitShortAllUnitsAtStop (tradingSymbol, orderDateTime, stopPrice);
      }

      /**
       * @brief Exit short positions at a percent-based stop‐loss price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopBasePrice    Base price for percent‐based stop.
       * @param percentNum       PercentNumber to compute the stop from base.
       */
      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				   const ptime& orderDateTime,
				   const Decimal& stopBasePrice,
				   const PercentNumber<Decimal>& percentNum)
      {
	mBroker.ExitShortAllUnitsAtStop (tradingSymbol, orderDateTime,
					 stopBasePrice, percentNum);
      }

      // Individual Unit Exit Methods for Pyramiding Support

      /**
       * @brief Exit a specific long unit at the open of orderDate.
       * @param tradingSymbol  Ticker to exit.
       * @param orderDate      Date when the exit is placed.
       * @param unitNumber     Unit number to exit (1-based).
       */
      void ExitLongUnitOnOpen(const std::string& tradingSymbol,
                              const date& orderDate,
                              uint32_t unitNumber)
      {
        ExitLongUnitOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()), unitNumber);
      }

      /**
       * @brief Exit a specific long unit at the open of orderDateTime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitLongUnitOnOpen(const std::string& tradingSymbol,
                              const ptime& orderDateTime,
                              uint32_t unitNumber)
      {
        mBroker.ExitLongUnitOnOpen(tradingSymbol, orderDateTime, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a hard limit price.
       * @param tradingSymbol  Ticker to exit.
       * @param orderDate      Date when the exit is placed.
       * @param limitPrice     Absolute price to exit at.
       * @param unitNumber     Unit number to exit (1-based).
       */
      void ExitLongUnitAtLimit(const std::string& tradingSymbol,
                               const date& orderDate,
                               const Decimal& limitPrice,
                               uint32_t unitNumber)
      {
        ExitLongUnitAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitPrice, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a percent-based limit price.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDate        Date when the exit is placed.
       * @param limitBasePrice   Base price for percent‐based exit.
       * @param percentNum       PercentNumber to compute the limit from base.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitLongUnitAtLimit(const std::string& tradingSymbol,
                               const date& orderDate,
                               const Decimal& limitBasePrice,
                               const PercentNumber<Decimal>& percentNum,
                               uint32_t unitNumber)
      {
        ExitLongUnitAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitBasePrice, percentNum, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a hard limit price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param limitPrice       Absolute price to exit at.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitLongUnitAtLimit(const std::string& tradingSymbol,
                               const ptime& orderDateTime,
                               const Decimal& limitPrice,
                               uint32_t unitNumber)
      {
        mBroker.ExitLongUnitAtLimit(tradingSymbol, orderDateTime, limitPrice, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a percent-based limit price using ptime.
       * @param tradingSymbol      Ticker to exit.
       * @param orderDateTime      DateTime when the exit is placed.
       * @param limitBasePrice     Base price for percent‐based exit.
       * @param percentNum         PercentNumber to compute the limit from base.
       * @param unitNumber         Unit number to exit (1-based).
       */
      void ExitLongUnitAtLimit(const std::string& tradingSymbol,
                               const ptime& orderDateTime,
                               const Decimal& limitBasePrice,
                               const PercentNumber<Decimal>& percentNum,
                               uint32_t unitNumber)
      {
        mBroker.ExitLongUnitAtLimit(tradingSymbol, orderDateTime, limitBasePrice, percentNum, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a hard stop price.
       * @param tradingSymbol  Ticker to exit.
       * @param orderDate      Date when the exit is placed.
       * @param stopPrice      Absolute stop price.
       * @param unitNumber     Unit number to exit (1-based).
       */
      void ExitLongUnitAtStop(const std::string& tradingSymbol,
                              const date& orderDate,
                              const Decimal& stopPrice,
                              uint32_t unitNumber)
      {
        ExitLongUnitAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopPrice, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a percent-based stop price.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDate        Date when the exit is placed.
       * @param stopBasePrice    Base price for percent‐based stop.
       * @param percentNum       PercentNumber to compute the stop from base.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitLongUnitAtStop(const std::string& tradingSymbol,
                              const date& orderDate,
                              const Decimal& stopBasePrice,
                              const PercentNumber<Decimal>& percentNum,
                              uint32_t unitNumber)
      {
        ExitLongUnitAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopBasePrice, percentNum, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a hard stop price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopPrice        Absolute stop price.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitLongUnitAtStop(const std::string& tradingSymbol,
                              const ptime& orderDateTime,
                              const Decimal& stopPrice,
                              uint32_t unitNumber)
      {
        mBroker.ExitLongUnitAtStop(tradingSymbol, orderDateTime, stopPrice, unitNumber);
      }

      /**
       * @brief Exit a specific long unit at a percent-based stop price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopBasePrice    Base price for percent‐based stop.
       * @param percentNum       PercentNumber to compute the stop from base.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitLongUnitAtStop(const std::string& tradingSymbol,
                              const ptime& orderDateTime,
                              const Decimal& stopBasePrice,
                              const PercentNumber<Decimal>& percentNum,
                              uint32_t unitNumber)
      {
        mBroker.ExitLongUnitAtStop(tradingSymbol, orderDateTime, stopBasePrice, percentNum, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at the open of orderDate.
       * @param tradingSymbol  Ticker to exit.
       * @param orderDate      Date when the exit is placed.
       * @param unitNumber     Unit number to exit (1-based).
       */
      void ExitShortUnitOnOpen(const std::string& tradingSymbol,
                               const date& orderDate,
                               uint32_t unitNumber)
      {
        ExitShortUnitOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()), unitNumber);
      }

      /**
       * @brief Exit a specific short unit at the open of orderDateTime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitShortUnitOnOpen(const std::string& tradingSymbol,
                               const ptime& orderDateTime,
                               uint32_t unitNumber)
      {
        mBroker.ExitShortUnitOnOpen(tradingSymbol, orderDateTime, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a hard limit price.
       * @param tradingSymbol  Ticker to exit.
       * @param orderDate      Date when the exit is placed.
       * @param limitPrice     Absolute price to exit at.
       * @param unitNumber     Unit number to exit (1-based).
       */
      void ExitShortUnitAtLimit(const std::string& tradingSymbol,
                                const date& orderDate,
                                const Decimal& limitPrice,
                                uint32_t unitNumber)
      {
        ExitShortUnitAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitPrice, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a percent-based limit price.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDate        Date when the exit is placed.
       * @param limitBasePrice   Base price for percent‐based exit.
       * @param percentNum       PercentNumber to compute the limit from base.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitShortUnitAtLimit(const std::string& tradingSymbol,
                                const date& orderDate,
                                const Decimal& limitBasePrice,
                                const PercentNumber<Decimal>& percentNum,
                                uint32_t unitNumber)
      {
        ExitShortUnitAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitBasePrice, percentNum, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a hard limit price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param limitPrice       Absolute price to exit at.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitShortUnitAtLimit(const std::string& tradingSymbol,
                                const ptime& orderDateTime,
                                const Decimal& limitPrice,
                                uint32_t unitNumber)
      {
        mBroker.ExitShortUnitAtLimit(tradingSymbol, orderDateTime, limitPrice, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a percent-based limit price using ptime.
       * @param tradingSymbol      Ticker to exit.
       * @param orderDateTime      DateTime when the exit is placed.
       * @param limitBasePrice     Base price for percent‐based exit.
       * @param percentNum         PercentNumber to compute the limit from base.
       * @param unitNumber         Unit number to exit (1-based).
       */
      void ExitShortUnitAtLimit(const std::string& tradingSymbol,
                                const ptime& orderDateTime,
                                const Decimal& limitBasePrice,
                                const PercentNumber<Decimal>& percentNum,
                                uint32_t unitNumber)
      {
        mBroker.ExitShortUnitAtLimit(tradingSymbol, orderDateTime, limitBasePrice, percentNum, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a hard stop price.
       * @param tradingSymbol  Ticker to exit.
       * @param orderDate      Date when the exit is placed.
       * @param stopPrice      Absolute stop price.
       * @param unitNumber     Unit number to exit (1-based).
       */
      void ExitShortUnitAtStop(const std::string& tradingSymbol,
                               const date& orderDate,
                               const Decimal& stopPrice,
                               uint32_t unitNumber)
      {
        ExitShortUnitAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopPrice, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a percent-based stop price.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDate        Date when the exit is placed.
       * @param stopBasePrice    Base price for percent‐based stop.
       * @param percentNum       PercentNumber to compute the stop from base.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitShortUnitAtStop(const std::string& tradingSymbol,
                               const date& orderDate,
                               const Decimal& stopBasePrice,
                               const PercentNumber<Decimal>& percentNum,
                               uint32_t unitNumber)
      {
        ExitShortUnitAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopBasePrice, percentNum, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a hard stop price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopPrice        Absolute stop price.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitShortUnitAtStop(const std::string& tradingSymbol,
                               const ptime& orderDateTime,
                               const Decimal& stopPrice,
                               uint32_t unitNumber)
      {
        mBroker.ExitShortUnitAtStop(tradingSymbol, orderDateTime, stopPrice, unitNumber);
      }

      /**
       * @brief Exit a specific short unit at a percent-based stop price using ptime.
       * @param tradingSymbol    Ticker to exit.
       * @param orderDateTime    DateTime when the exit is placed.
       * @param stopBasePrice    Base price for percent‐based stop.
       * @param percentNum       PercentNumber to compute the stop from base.
       * @param unitNumber       Unit number to exit (1-based).
       */
      void ExitShortUnitAtStop(const std::string& tradingSymbol,
                               const ptime& orderDateTime,
                               const Decimal& stopBasePrice,
                               const PercentNumber<Decimal>& percentNum,
                               uint32_t unitNumber)
      {
        mBroker.ExitShortUnitAtStop(tradingSymbol, orderDateTime, stopBasePrice, percentNum, unitNumber);
      }

       /**
 * @brief Drive the broker's mark‐to‐market and fill logic for this bar.
 * @param processingDate  Current bar date.
 * @see StrategyBroker::ProcessPendingOrders
 */
      void eventProcessPendingOrders(const date& processingDate)
      {
	eventProcessPendingOrders(ptime(processingDate, getDefaultBarTime()));
      }

      /**
       * @brief Drive the broker's mark‐to‐market and fill logic for this bar using ptime.
       * @param processingDateTime  Current bar datetime.
       * @see StrategyBroker::ProcessPendingOrders
       */
      void eventProcessPendingOrders(const ptime& processingDateTime)
      {
	mBroker.ProcessPendingOrders (processingDateTime);
      }

      /**
       * @brief Increment the per‐security bar count (used for lookback logic).
       * @param tradingSymbol  Ticker whose bar counter to advance.
       */
      void eventUpdateSecurityBarNumber(const std::string& tradingSymbol)
      {
	mSecuritiesProperties.updateBacktestBarNumber (tradingSymbol);
      }

      uint32_t getSecurityBarNumber(const std::string& tradingSymbol) const
      {
	return mSecuritiesProperties.getBacktestBarNumber (tradingSymbol); 
      }

      void setRMultipleStop (const std::string& tradingSymbol, 
			     const Decimal& riskStop)
      {
	this->setRMultipleStop (tradingSymbol, riskStop, 1);

      }
      void setRMultipleStop (const std::string& tradingSymbol, 
			     const Decimal& riskStop, 
			     uint32_t unitNumber)
      {
	InstrumentPosition<Decimal> instrPos = getInstrumentPosition(tradingSymbol);
	instrPos.setRMultipleStop (riskStop, unitNumber);
      }

     /**
      * @brief Access the current InstrumentPosition for a security.
      * @param tradingSymbol  Ticker to retrieve.
      * @return Const reference to the position object.
      */
      const InstrumentPosition<Decimal>& 
      getInstrumentPosition(const std::string& tradingSymbol) const
      {
	return mBroker.getInstrumentPosition(tradingSymbol);
      }
      
      /**
       * @brief Check if aSecurity has data at processingDate.
       * @param aSecurity      Security to probe.
       * @param processingDate Date to test.
       * @return True if time series contains an entry for processingDate.
       */

      bool doesSecurityHaveTradingData (const Security<Decimal>& aSecurity,
     const date& processingDate)
      {
	return doesSecurityHaveTradingData(aSecurity,
					   ptime(processingDate, getDefaultBarTime()));
      }

      /**
       * @brief Check if aSecurity has data at processingDateTime.
       * @param aSecurity          Security to probe.
       * @param processingDateTime DateTime to test.
       * @return True if time series contains an entry for processingDateTime.
       */
      bool doesSecurityHaveTradingData (const Security<Decimal>& aSecurity,
     const ptime& processingDateTime)
      {
 return aSecurity.isDateFound(processingDateTime);
      }

      const Broker& getStrategyBroker() const
      {
	return mBroker;
      }

      std::shared_ptr<Portfolio<Decimal>> getPortfolio() const
      {
 return mPortfolio;
      }

      const StrategyOptions& getStrategyOptions() const
      {
 return mStrategyOptions;
      }

    protected:
      /**
       * @brief Construct a base strategy with portfolio and options.
       * @param strategyName     Name to assign.
       * @param portfolio        Shared portfolio pointer.
       * @param strategyOptions  Risk and pyramiding config.
       */
      BacktesterStrategy (const std::string& strategyName,
     std::shared_ptr<Portfolio<Decimal>> portfolio,
     const StrategyOptions& strategyOptions)
 : mStrategyName(strategyName),
   mBroker (portfolio),
   mPortfolio (portfolio),
   mSecuritiesProperties(),
   mStrategyOptions(strategyOptions),
   mInstanceId(sUuidGenerator())  // Generate unique UUID for this instance
 {
   typename Portfolio<Decimal>::ConstPortfolioIterator it =
     mPortfolio->beginPortfolio();

   for (; it != mPortfolio->endPortfolio(); it++)
     {
       mSecuritiesProperties.addSecurity (it->second->getSymbol());
     }
 }

    private:
      std::string mStrategyName;
      Broker mBroker;
      std::shared_ptr<Portfolio<Decimal>> mPortfolio;
      SecurityBacktestPropertiesManager mSecuritiesProperties;
      StrategyOptions mStrategyOptions;
      boost::uuids::uuid mInstanceId;  // Moved here to match initialization order
      static TradingVolume OneShare;
      static TradingVolume OneContract;
    };

  template <class Decimal>
  const TradingVolume&
    BacktesterStrategy<Decimal>::getSizeForOrder(const Security<Decimal>& aSecurity) const
  {
    if (aSecurity.isEquitySecurity())
      return OneShare;
    else
      return OneContract;
  }

  template <class Decimal> TradingVolume BacktesterStrategy<Decimal>::OneShare(1, TradingVolume::SHARES);
  template <class Decimal> TradingVolume BacktesterStrategy<Decimal>::OneContract(1, TradingVolume::CONTRACTS);

  // Static member definition for UUID generator
  template <class Decimal>
  boost::uuids::random_generator BacktesterStrategy<Decimal>::sUuidGenerator;

}


#endif
