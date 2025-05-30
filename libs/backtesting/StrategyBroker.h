// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STRATEGY_BROKER_H
#define __STRATEGY_BROKER_H 1

#include <exception>
#include "Portfolio.h"
#include "TradingOrderManager.h"
#include "InstrumentPositionManager.h"
#include "ClosedPositionHistory.h"
#include "StrategyTransactionManager.h"
#include "ProfitTarget.h"
#include "StopLoss.h"
#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
// Ensure ptime and getDefaultBarTime are available
#include "TimeSeriesEntry.h" // For OHLCTimeSeriesEntry, ptime, getDefaultBarTime


namespace mkc_timeseries
{
  // Using date from boost::gregorian and ptime from boost::posix_time
  using boost::gregorian::date;
  using boost::posix_time::ptime;

  /**
   * @class StrategyBrokerException
   * @brief Exception class for StrategyBroker specific errors.
   */
  class StrategyBrokerException : public std::runtime_error
  {
  public:
  StrategyBrokerException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~StrategyBrokerException()
      {}
  };

  /**
   * @class StrategyBroker
   * @brief Manages trading order execution, instrument position tracking, and historical trade logging,
   * serving as a crucial component within a backtesting environment.
   *
   * @tparam Decimal The decimal type used for financial calculations.
   *
   * @details
   * The StrategyBroker is central to simulating trading activities. It acts as the intermediary between a
   * trading strategy's logic and the simulated market.
   * In a backtesting context, the `BacktesterStrategy` makes calls to the `StrategyBroker` to place,
   * modify, or cancel orders based on its internal logic and market data.
   *
   * The `StrategyBroker` then processes these requests, simulates order execution via the `TradingOrderManager`,
   * updates the state of open positions using the
   * `InstrumentPositionManager`, and records all transactional details and closed trades in the
   * `StrategyTransactionManager` and `ClosedPositionHistory` respectively.
   *
   * Key Responsibilities in Backtesting:
   * - Order Submission: Receives order requests (e.g., EnterLongOnOpen, ExitShortAllUnitsAtStop) from
   * the `BacktesterStrategy` and forwards them to the `TradingOrderManager` for processing.
   *
   * - Position Management: Tracks the current state (long, short, flat) and volume of positions for each
   * instrument, managed by `InstrumentPositionManager`. This information is vital for the `BacktesterStrategy`
   * to make informed decisions on subsequent trading signals.
   *
   * - Fill Simulation and Notification: As an observer of `TradingOrderManager`, it reacts to simulated order
   * fills (`OrderExecuted` callbacks). Upon execution, it creates or updates `TradingPosition` instances.
   *
   * - Trade Lifecycle Management: Manages the lifecycle of a trade from order submission to position closing.
   * This includes creating `StrategyTransaction` objects that link entry orders, positions, and eventual exit orders.
   ( These transactions are stored in the `StrategyTransactionManager`.
   *
   * - Observer Role: Implements `TradingOrderObserver` and `TradingPositionObserver` to react to events like
   * order executions and position closures. For instance, when a position is closed, the `PositionClosed` method is
   * invoked, allowing the broker to update its records and complete the relevant `StrategyTransaction`.
   *
   * Workflow in Backtesting Framework:
   * 1. The `BackTester` drives the simulation on a bar-by-bar basis.
   *
   * 2. On each bar, the `BackTester` invokes event handlers (e.g., `eventEntryOrders`, `eventExitOrders`) on
   * the `BacktesterStrategy`.
   *
   * 3. The `BacktesterStrategy` implements the specific trading logic and, based on this logic, issues trading
   * commands (e.g., buy, sell, set stop-loss) by calling methods on its `StrategyBroker` instance.
   *
   * 4. The `StrategyBroker` processes these commands:
   * - Adds new orders to the `TradingOrderManager`.
   * - When an entry order is filled, a `TradingPosition` is created, and a `StrategyTransaction` is initiated and
   * stored in `mStrategyTrades` (an instance of `StrategyTransactionManager`).
   * - `TradingOrderManager` attempts to fill orders based on market conditions for the current bar.
   * - If an order is filled, `StrategyBroker` is notified (via `OrderExecuted`) and updates the
   * `InstrumentPositionManager` and the state of the `StrategyTransaction`.
   *
   * 5. `StrategyBroker` also processes pending orders at the appropriate time in the simulation loop,
   * typically triggered by `BacktesterStrategy` calling `StrategyBroker::ProcessPendingOrders`.
   *
   * This class ensures that the trading strategy's decisions are accurately reflected in the simulated portfolio,
   * providing a realistic assessment of performance.
   *
   * Collaborators:
   * - `BacktesterStrategy`: Generates order requests based on trading logic. The `StrategyBroker`
   * is a member of `BacktesterStrategy`.
   *
   * - `TradingOrderManager`: Queues, tracks, and processes pending trading orders. `StrategyBroker`
   * adds orders to it and observes it for fills.
   *
   * - `InstrumentPositionManager`: Maintains the current state (long, short, flat, volume) of all open positions
   * for each instrument.
   *
   * - `StrategyTransactionManager` (`mStrategyTrades`): Records and manages `StrategyTransaction` objects,
   ( each representing the full lifecycle of a trade (entry order, position, exit order).
   * - `ClosedPositionHistory`: Stores a history of all closed trading positions (derived from completed
   * `StrategyTransaction`s).
   *
   * - `Portfolio`: Provides access to security information, such as tick size and historical price data,
   * necessary for order processing and position valuation.
   */
  template <class Decimal> class StrategyBroker : 
    public TradingOrderObserver<Decimal>, 
    public TradingPositionObserver<Decimal>
  {
  public:
    typedef typename TradingOrderManager<Decimal>::PendingOrderIterator PendingOrderIterator;
    typedef typename StrategyTransactionManager<Decimal>::SortedStrategyTransactionIterator 
      StrategyTransactionIterator;
    typedef typename ClosedPositionHistory<Decimal>::ConstPositionIterator ClosedPositionIterator;

  public:
    /**
     * @brief Construct a StrategyBroker for the given portfolio.
     * Registers as an observer with the order manager and initializes instrument positions.
     * @param portfolio Shared pointer to the portfolio of securities.
     */
    StrategyBroker (std::shared_ptr<Portfolio<Decimal>> portfolio)
      : TradingOrderObserver<Decimal>(),
	TradingPositionObserver<Decimal>(),
	mOrderManager(portfolio),
	mInstrumentPositionManager(),
	mStrategyTrades(),
	mClosedTradeHistory(),
	mPortfolio(portfolio)
    {
      mOrderManager.addObserver (*this);
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->beginPortfolio();

      for (; symbolIterator != mPortfolio->endPortfolio(); symbolIterator++)
	  mInstrumentPositionManager.addInstrument(symbolIterator->second->getSymbol());
    }

    ~StrategyBroker()
    {}

    StrategyBroker(const StrategyBroker<Decimal> &rhs)
      : TradingOrderObserver<Decimal>(rhs),
	TradingPositionObserver<Decimal>(rhs),
	mOrderManager(rhs.mOrderManager),
	mInstrumentPositionManager(rhs.mInstrumentPositionManager),
	mStrategyTrades(rhs.mStrategyTrades),
	mClosedTradeHistory(rhs.mClosedTradeHistory),
	mPortfolio(rhs.mPortfolio)
    {}

    StrategyBroker<Decimal>& 
    operator=(const StrategyBroker<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingOrderObserver<Decimal>::operator=(rhs);
      TradingPositionObserver<Decimal>::operator=(rhs);

      mOrderManager = rhs.mOrderManager;
      mInstrumentPositionManager = rhs.mInstrumentPositionManager;
      mStrategyTrades = rhs.mStrategyTrades;
      mClosedTradeHistory = rhs.mClosedTradeHistory;
      mPortfolio = rhs.mPortfolio;

      return *this;
    }

     /**
     * @brief Returns a constant iterator to the beginning of sorted strategy transactions.
     *
     * A `StrategyTransaction` encapsulates the entire lifecycle of a single trade,
     * including the initial entry order, the resulting trading position, and the eventual
     * exit order (once the trade is closed). These transactions are managed by the
     * internal `StrategyTransactionManager` (`mStrategyTrades`).
     *
     * Clients would use this method, along with `endStrategyTransactions()`, to iterate
     * over all recorded trades (both open and closed) for detailed analysis, reporting,
     * or debugging purposes. Each `StrategyTransaction` object provides access to the
     * entry order, the position details, and the exit order (if applicable).
     * The transactions are typically sorted by their entry date/time.
     *
     * @return A `StrategyTransactionIterator` pointing to the first strategy transaction.
     * @see StrategyTransaction
     * @see StrategyTransactionManager
     * @see endStrategyTransactions()
     */
    StrategyTransactionIterator beginStrategyTransactions() const
    {
      return mStrategyTrades.beginSortedStrategyTransaction();
    }

    /**
     * @brief Returns a constant iterator to the end of sorted strategy transactions.
     *
     * This method provides the end iterator for the collection of `StrategyTransaction` objects
     * managed by the internal `StrategyTransactionManager` (`mStrategyTrades`). It is used in conjunction
     * with `beginStrategyTransactions()` to iterate over all recorded trades.
     *
     * @return A `StrategyTransactionIterator` pointing past the last strategy transaction.
     * @see StrategyTransaction
     * @see StrategyTransactionManager
     * @see beginStrategyTransactions()
     */
    StrategyTransactionIterator endStrategyTransactions() const
    {
      return mStrategyTrades.endSortedStrategyTransaction();
    }

    /**
     * @brief Retrieves the history of closed trading positions.
     * This provides a more direct way to access only the positions that have been fully closed.
     * Each closed position here corresponds to a completed `StrategyTransaction`.
     * @return A constant reference to the ClosedPositionHistory object.
     */
    const ClosedPositionHistory<Decimal>&
    getClosedPositionHistory() const
    {
      return mClosedTradeHistory;
    }

    /**
     * @brief Returns a constant iterator to the beginning of closed trading positions.
     * @return A ClosedPositionIterator pointing to the first closed position.
     */
    ClosedPositionIterator beginClosedPositions() const
    {
      return mClosedTradeHistory.beginTradingPositions();
    }

    ClosedPositionIterator endClosedPositions() const
    {
      return mClosedTradeHistory.endTradingPositions();
    }

    /**
     * @brief Gets the total number of trades (strategy transactions) initiated, both open and closed.
     * This count is derived from the `StrategyTransactionManager`.
     * @return The total number of trades.
     */
    uint32_t getTotalTrades() const
    {
      return  mStrategyTrades.getTotalTrades();
    }

    /**
     * @brief Gets the number of currently open trades (strategy transactions that have an entry but no exit yet).
     * This count is derived from the `StrategyTransactionManager`.
     * @return The number of open trades.
     */
    uint32_t getOpenTrades() const
    {
      return  mStrategyTrades.getOpenTrades();
    }

    /**
     * @brief Gets the number of closed trades (strategy transactions that have both an entry and an exit).
     * This count is derived from the `StrategyTransactionManager`.
     * @return The number of closed trades.
     */
    uint32_t getClosedTrades() const
    {
      return  mStrategyTrades.getClosedTrades();
    }

    /**
     * @brief Checks if there is an open long position for the specified trading symbol.
     * @param tradingSymbol The symbol of the instrument.
     * @return True if a long position exists, false otherwise.
     */
    bool isLongPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.isLongPosition (tradingSymbol);
    }

    /**
     * @brief Checks if there is an open short position for the specified trading symbol.
     * @param tradingSymbol The symbol of the instrument.
     * @return True if a short position exists, false otherwise.
     */
    bool isShortPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.isShortPosition (tradingSymbol);
    }

    /**
     * @brief Checks if there is no open position (flat) for the specified trading symbol.
     * @param tradingSymbol The symbol of the instrument.
     * @return True if the position is flat, false otherwise.
     */
    bool isFlatPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.isFlatPosition (tradingSymbol);
    }

    // Date based versions
    void EnterLongOnOpen(const std::string& tradingSymbol, 	
			 const date& orderDate,
			 const TradingVolume& unitsInOrder,
			 const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			 const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
    {
        EnterLongOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()), unitsInOrder, stopLoss, profitTarget);
    }

    void EnterShortOnOpen(const std::string& tradingSymbol,	
			  const date& orderDate,
			  const TradingVolume& unitsInOrder,
			  const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			  const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
    {
        EnterShortOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()), unitsInOrder, stopLoss, profitTarget);
    }

    void ExitLongAllUnitsOnOpen(const std::string& tradingSymbol,
				const date& orderDate,
				const TradingVolume& unitsInOrder)
    {
        ExitLongAllUnitsOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()), unitsInOrder);
    }

    void ExitLongAllUnitsOnOpen(const std::string& tradingSymbol,
				const date& orderDate)
    {
        ExitLongAllUnitsOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()));
    }

    void ExitShortAllUnitsOnOpen(const std::string& tradingSymbol,
				 const date& orderDate)
    {
        ExitShortAllUnitsOnOpen(tradingSymbol, ptime(orderDate, getDefaultBarTime()));
    }

    void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitPrice)
    {
        ExitLongAllUnitsAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitPrice);
    }

    void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitBasePrice,
				 const PercentNumber<Decimal>& percentNum)
    {
        ExitLongAllUnitsAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitBasePrice, percentNum);
    }

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
        ExitShortAllUnitsAtLimit(tradingSymbol, ptime(orderDate, getDefaultBarTime()), limitBasePrice, percentNum);
    }

    void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const Decimal& stopPrice)
    {
        ExitLongAllUnitsAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopPrice);
    }

    void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const Decimal& stopBasePrice,
				const PercentNumber<Decimal>& percentNum)
    {
        ExitLongAllUnitsAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopBasePrice, percentNum);
    }

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
        ExitShortAllUnitsAtStop(tradingSymbol, ptime(orderDate, getDefaultBarTime()), stopBasePrice, percentNum);
    }


    // Ptime based versions
    /**
     * @brief Submit a market-on-open long order using ptime.
     *
     * @param tradingSymbol Ticker symbol to trade.
     * @param orderDateTime Exact date and time of the order.
     * @param unitsInOrder Number of units to enter.
     * @param stopLoss Optional stop-loss price.
     * @param profitTarget Optional profit-target price.
     */
    void EnterLongOnOpen(const std::string& tradingSymbol, 	
			 const ptime& orderDateTime,
			 const TradingVolume& unitsInOrder,
			 const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			 const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
    {
      auto order = std::make_shared<MarketOnOpenLongOrder<Decimal>>(tradingSymbol,
								    unitsInOrder,
								    orderDateTime, // Use ptime
								    stopLoss,
								    profitTarget);
      
      mOrderManager.addTradingOrder (order);
    }

    /**
     * @brief Submit a market-on-open short order using ptime.
     *
     * @param tradingSymbol Ticker symbol to short.
     * @param orderDateTime Exact date and time of the order.
     * @param unitsInOrder Number of units to enter.
     * @param stopLoss Optional stop-loss price.
     * @param profitTarget Optional profit-target price.
     */
    void EnterShortOnOpen(const std::string& tradingSymbol,	
			  const ptime& orderDateTime,
			  const TradingVolume& unitsInOrder,
			  const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			  const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
    {
      auto order = std::make_shared<MarketOnOpenShortOrder<Decimal>>(tradingSymbol,
								     unitsInOrder,
								     orderDateTime, // Use ptime
								     stopLoss,
								     profitTarget);
      
      mOrderManager.addTradingOrder (order);
    }

    /**
     * @brief Exit all long units at market-open using ptime.
     * @param tradingSymbol Ticker symbol to exit.
     * @param orderDateTime Exact date and time of the exit order.
     */
    void ExitLongAllUnitsOnOpen(const std::string& tradingSymbol,
				const ptime& orderDateTime,
				const TradingVolume& unitsInOrder)
    {
     if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<MarketOnOpenSellOrder<Decimal>>(tradingSymbol,
								     unitsInOrder,
								     orderDateTime); // Use ptime

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtOpen - no long position for " +tradingSymbol +" with order datetime: " +boost::posix_time::to_simple_string (orderDateTime));
	}
    }

     /**
      * @brief Submits a market-on-open order to exit all units of an existing long position using ptime.
      * The volume is determined automatically from the current position.
      * @param tradingSymbol The symbol of the instrument.
      * @param orderDateTime The exact date and time on which the order should be placed.
      * @throws StrategyBrokerException if no long position exists for the symbol.
      */
    void ExitLongAllUnitsOnOpen(const std::string& tradingSymbol,
				const ptime& orderDateTime)
    {
     if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  ExitLongAllUnitsOnOpen(tradingSymbol, orderDateTime, 
				 mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol));
	}
           else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtOpen - no long position for " +tradingSymbol +" with order datetime: " +boost::posix_time::to_simple_string (orderDateTime));
	}
    }

    /**
     * @brief Exit all short units at market-open using ptime.
     * @param tradingSymbol Ticker symbol to exit.
     * @param orderDateTime Exact date and time of the exit order.
     */
    void ExitShortAllUnitsOnOpen(const std::string& tradingSymbol,
				 const ptime& orderDateTime)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<MarketOnOpenCoverOrder<Decimal>>(tradingSymbol,
								       mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
								      orderDateTime); // Use ptime

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtOpen - no short position for " +tradingSymbol +" with order datetime: " +boost::posix_time::to_simple_string (orderDateTime));
	}
    }

    /**
     * @brief Submits a limit order to sell (exit) all units of an existing long position at a specified limit price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param limitPrice The limit price at which to sell.
     * @throws StrategyBrokerException if no long position exists for the symbol.
     */
    void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const ptime& orderDateTime,
				 const Decimal& limitPrice)
    {
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<SellAtLimitOrder<Decimal>>(tradingSymbol,
								mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
								orderDateTime, // Use ptime
								limitPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtLimit - no long position for " +tradingSymbol +" with order datetime: " +boost::posix_time::to_simple_string (orderDateTime));
	}
    }

    /**
     * @brief Submits a limit order to sell (exit) all units of an existing long position using ptime,
     * with the limit price calculated as a percentage above a base price.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param limitBasePrice The base price for calculating the limit price.
     * @param percentNum The percentage above the base price to set the limit.
     * @throws StrategyBrokerException if no long position exists for the symbol or if tick data is unavailable.
     */
    void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const ptime& orderDateTime,
				 const Decimal& limitBasePrice,
				 const PercentNumber<Decimal>& percentNum)
    {
      LongProfitTarget<Decimal> profitTarget(limitBasePrice, percentNum);
      Decimal orderPrice = num::Round2Tick (profitTarget.getProfitTarget(), this->getTick (tradingSymbol),
					    this->getTickDiv2(tradingSymbol));
      this->ExitLongAllUnitsAtLimit (tradingSymbol, orderDateTime, orderPrice); // Calls ptime version
    }

    /**
     * @brief Submits a limit order to cover (exit) all units of an existing short position at a specified limit price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param limitPrice The limit price at which to cover.
     * @throws StrategyBrokerException if no short position exists for the symbol.
     */
    void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				  const ptime& orderDateTime,
				  const Decimal& limitPrice)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<CoverAtLimitOrder<Decimal>>(tradingSymbol,
								 mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
								 orderDateTime, // Use ptime
								 limitPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtLimit - no short position for " +tradingSymbol +" with order datetime: " +boost::posix_time::to_simple_string (orderDateTime));
	}
    }

    /**
     * @brief Submits a limit order to cover (exit) all units of an existing short position using ptime,
     * with the limit price calculated as a percentage below a base price.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param limitBasePrice The base price for calculating the limit price.
     * @param percentNum The percentage below the base price to set the limit.
     * @throws StrategyBrokerException if no short position exists for the symbol or if tick data is unavailable.
     */
    void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				 const ptime& orderDateTime,
				 const Decimal& limitBasePrice,
				 const PercentNumber<Decimal>& percentNum)
    {
      ShortProfitTarget<Decimal> percentTarget(limitBasePrice, percentNum);
      Decimal profitTarget(percentTarget.getProfitTarget());
      Decimal orderPrice = num::Round2Tick (profitTarget, this->getTick (tradingSymbol), this->getTickDiv2(tradingSymbol));
      this->ExitShortAllUnitsAtLimit (tradingSymbol, orderDateTime, orderPrice); // Calls ptime version
    }

    /**
     * @brief Submits a stop order to sell (exit) all units of an existing long position at a specified stop price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param stopPrice The stop price at which to sell.
     * @throws StrategyBrokerException if no long position exists for the symbol.
     */
    void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const ptime& orderDateTime,
				const Decimal& stopPrice)
    {
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<SellAtStopOrder<Decimal>>(tradingSymbol,
							       mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
							       orderDateTime, // Use ptime
							       stopPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtStop - no long position for " +tradingSymbol +" with order datetime: " +boost::posix_time::to_simple_string (orderDateTime));
	}
    }

    /**
     * @brief Submits a stop order to sell (exit) all units of an existing long position using ptime,
     * with the stop price calculated as a percentage below a base price.
     *
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param stopBasePrice The base price for calculating the stop price.
     * @param percentNum The percentage below the base price to set the stop.
     * @throws StrategyBrokerException if no long position exists for the symbol or if tick data is unavailable.
     */
    void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const ptime& orderDateTime,
				const Decimal& stopBasePrice,
				const PercentNumber<Decimal>& percentNum)
    {
      LongStopLoss<Decimal> percentStop(stopBasePrice, percentNum);
      Decimal stopLoss(percentStop.getStopLoss());
      Decimal orderPrice = num::Round2Tick (stopLoss, this->getTick (tradingSymbol), this->getTickDiv2(tradingSymbol));
      this->ExitLongAllUnitsAtStop(tradingSymbol, orderDateTime, orderPrice); // Calls ptime version
    }

    /**
     * @brief Submits a stop order to cover (exit) all units of an existing short position at a specified stop price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param stopPrice The stop price at which to cover.
     * @throws StrategyBrokerException if no short position exists for the symbol.
     */
    void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const ptime& orderDateTime,
				 const Decimal& stopPrice)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<CoverAtStopOrder<Decimal>>(tradingSymbol,
							       	mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
							       orderDateTime, // Use ptime
							       stopPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtStop - no short position for " +tradingSymbol +" with order datetime: " +boost::posix_time::to_simple_string (orderDateTime));
	}
    }

    /**
     * @brief Submits a stop order to cover (exit) all units of an existing short position using ptime,
     * with the stop price calculated as a percentage above a base price.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time on which the order should be placed.
     * @param stopBasePrice The base price for calculating the stop price.
     * @param percentNum The percentage above the base price to set the stop.
     * @throws StrategyBrokerException if no short position exists for the symbol or if tick data is unavailable.
     */
    void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const ptime& orderDateTime,
				 const Decimal& stopBasePrice,
				 const PercentNumber<Decimal>& percentNum)
    {
      ShortStopLoss<Decimal> aPercentStop(stopBasePrice, percentNum);
      Decimal stopLoss(aPercentStop.getStopLoss());
      Decimal orderPrice = num::Round2Tick (stopLoss, this->getTick (tradingSymbol), this->getTickDiv2(tradingSymbol));
      this->ExitShortAllUnitsAtStop(tradingSymbol, orderDateTime, orderPrice); // Calls ptime version
    }

    /**
     * @brief Returns a constant iterator to the beginning of pending orders in the `TradingOrderManager`.
     * @return A PendingOrderIterator pointing to the first pending order.
     */
    PendingOrderIterator beginPendingOrders() const
    {
      return mOrderManager.beginPendingOrders();
    }

    /**
     * @brief Returns a constant iterator to the end of pending orders in the `TradingOrderManager`.
     * @return A PendingOrderIterator pointing past the last pending order.
     */
    PendingOrderIterator endPendingOrders() const
    {
      return mOrderManager.endPendingOrders();
    }

    /**
     * @brief Processes all pending orders for a given date (using default bar time).
     * This method is critical in a backtesting loop. It first updates the open positions with the current day's bar data
     * and then instructs the TradingOrderManager to attempt to fill any pending orders based on this new data.
     * @param orderProcessingDate The date for which orders are to be processed.
     */
    void ProcessPendingOrders(const date& orderProcessingDate)
    {
        this->ProcessPendingOrders(ptime(orderProcessingDate, getDefaultBarTime()));
    }

    /**
     * @brief Processes all pending orders for a given datetime.
     * This method is critical in a backtesting loop. It first updates the open positions with the current datetime's bar data
     * and then instructs the TradingOrderManager to attempt to fill any pending orders based on this new data.
     * @param orderProcessingDateTime The datetime for which orders are to be processed.
     */
    void ProcessPendingOrders(const ptime& orderProcessingDateTime)
    {
      // Add historical bar for this datetime before possibly closing any open
      // positions
      mInstrumentPositionManager.addBarForOpenPosition (orderProcessingDateTime, // Use ptime version
							mPortfolio.get());
      mOrderManager.processPendingOrders (orderProcessingDateTime, // Use ptime version
					  mInstrumentPositionManager);
    }


    /**
     * @brief Callback invoked when a MarketOnOpenLongOrder is executed.
     * Creates a new long trading position and records the transaction.
     * @param order Pointer to the executed MarketOnOpenLongOrder.
     */
    void OrderExecuted (MarketOnOpenLongOrder<Decimal> *order) override
    {
      auto position = createLongTradingPosition (order,
						 order->getStopLoss(),
						 order->getProfitTarget());
      auto pOrder = std::make_shared<MarketOnOpenLongOrder<Decimal>>(*order);

      mInstrumentPositionManager.addPosition (position);
      mStrategyTrades.addStrategyTransaction (createStrategyTransaction (pOrder, position));
    }

    /**
     * @brief Callback invoked when a MarketOnOpenShortOrder is executed.
     * Creates a new short trading position and records the transaction.
     * @param order Pointer to the executed MarketOnOpenShortOrder.
     */
    void OrderExecuted (MarketOnOpenShortOrder<Decimal> *order) override
    {
      auto position = createShortTradingPosition (order,
						  order->getStopLoss(),
						  order->getProfitTarget());
      auto pOrder = std::make_shared<MarketOnOpenShortOrder<Decimal>>(*order);

      mInstrumentPositionManager.addPosition (position);
      mStrategyTrades.addStrategyTransaction (createStrategyTransaction (pOrder, position));
    }

    /**
     * @brief Callback invoked when a MarketOnOpenSellOrder is executed (exit long).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed MarketOnOpenSellOrder.
     */
    void OrderExecuted (MarketOnOpenSellOrder<Decimal> *order) override
    {
      ExitOrderExecutedCommon<MarketOnOpenSellOrder<Decimal>>(order);
    }

    /**
     * @brief Callback invoked when a MarketOnOpenCoverOrder is executed (exit short).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed MarketOnOpenCoverOrder.
     */
    void OrderExecuted (MarketOnOpenCoverOrder<Decimal> *order) override
    {
      ExitOrderExecutedCommon<MarketOnOpenCoverOrder<Decimal>>(order);
    }

    /**
     * @brief Callback invoked when a SellAtLimitOrder is executed (exit long).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed SellAtLimitOrder.
     */
    void OrderExecuted (SellAtLimitOrder<Decimal> *order) override
    {
      ExitOrderExecutedCommon<SellAtLimitOrder<Decimal>>(order);
    }

    /**
     * @brief Callback invoked when a CoverAtLimitOrder is executed (exit short).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed CoverAtLimitOrder.
     */
    void OrderExecuted (CoverAtLimitOrder<Decimal> *order) override
    {
      ExitOrderExecutedCommon<CoverAtLimitOrder<Decimal>>(order);
    }

    /**
     * @brief Callback invoked when a CoverAtStopOrder is executed (exit short).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed CoverAtStopOrder.
     */
    void OrderExecuted (CoverAtStopOrder<Decimal> *order) override
    {
      ExitOrderExecutedCommon<CoverAtStopOrder<Decimal>>(order);
    }

    /**
     * @brief Callback invoked when a SellAtStopOrder is executed (exit long).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed SellAtStopOrder.
     */
    void OrderExecuted (SellAtStopOrder<Decimal> *order) override
    {
      ExitOrderExecutedCommon<SellAtStopOrder<Decimal>>(order);
    }

    // OrderCanceled methods
    void OrderCanceled (MarketOnOpenLongOrder<Decimal> *order) override {}
    void OrderCanceled (MarketOnOpenShortOrder<Decimal> *order) override {}
    void OrderCanceled (MarketOnOpenSellOrder<Decimal> *order) override {}
    void OrderCanceled (MarketOnOpenCoverOrder<Decimal> *order) override {}
    void OrderCanceled (SellAtLimitOrder<Decimal> *order) override {}
    void OrderCanceled (CoverAtLimitOrder<Decimal> *order) override {}
    void OrderCanceled (CoverAtStopOrder<Decimal> *order) override {}
    void OrderCanceled (SellAtStopOrder<Decimal> *order) override {}


    /**
     * @brief Retrieves the current instrument position for a given trading symbol from the `InstrumentPositionManager`.
     * @param tradingSymbol The symbol of the instrument.
     * @return A constant reference to the InstrumentPosition object.
     */
    const InstrumentPosition<Decimal>& 
    getInstrumentPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.getInstrumentPosition (tradingSymbol);
    }

    /**
     * @brief Callback invoked by an observed `TradingPosition` when it is closed.
     * Finds the corresponding `StrategyTransaction` in `mStrategyTrades` and adds the now-closed
     * `TradingPosition` to the `mClosedTradeHistory`.
     * @param aPosition Pointer to the TradingPosition that has been closed.
     * @throws StrategyBrokerException if the strategy transaction for the closed position cannot be found.
     */
    void PositionClosed (TradingPosition<Decimal> *aPosition) override
    {
      typename StrategyTransactionManager<Decimal>::StrategyTransactionIterator it =
	mStrategyTrades.findStrategyTransaction (aPosition->getPositionID());

      if (it != mStrategyTrades.endStrategyTransaction())
	{
	  mClosedTradeHistory.addClosedPosition (it->second->getTradingPositionPtr());
	}
      else
	throw StrategyBrokerException("Unable to find strategy transaction for position id " +std::to_string(aPosition->getPositionID()));
    }

  public:
    const Decimal getTick(const std::string& symbol) const
    {
      auto& factory = SecurityAttributesFactory<Decimal>::instance();
      auto it = factory.getSecurityAttributes (symbol);

      if (it != factory.endSecurityAttributes())
	return it->second->getTick();
      else
	throw StrategyBrokerException("Strategybroker::getTick - ticker symbol " +symbol +" is unkown");

    }

    const Decimal getTickDiv2(const std::string& symbol) const
    {
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (symbol);
      if (symbolIterator != mPortfolio->endPortfolio())
	{
	  return symbolIterator->second->getTickDiv2();
	}
      else
	throw StrategyBrokerException("Strategybroker::getTickDiv2 - ticker symbol " +symbol +" is unkown");

    }

  private:
    // Date based getEntryBar
    OHLCTimeSeriesEntry<Decimal> getEntryBar (const std::string& tradingSymbol,
							const date& d)
    {
        return getEntryBar(tradingSymbol, ptime(d, getDefaultBarTime()));
    }

    /**
     * @brief Retrieves the OHLC time series entry (bar data) for a given symbol and ptime from the `Portfolio`.
     * This data is used, for example, as the entry bar for a new `TradingPosition`.
     * @param tradingSymbol The trading symbol.
     * @param dt The ptime for which to retrieve the bar data.
     * @return The OHLCTimeSeriesEntry for the specified symbol and ptime.
     * @throws StrategyBrokerException if the symbol is not found in the portfolio or if data for the datetime is missing.
     */
    OHLCTimeSeriesEntry<Decimal> getEntryBar (const std::string& tradingSymbol,
							const ptime& dt)
    {
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (tradingSymbol);
      if (symbolIterator != mPortfolio->endPortfolio())
	{
        // Assumes getRandomAccessIterator or a new findTimeSeriesEntry(ptime) exists in Security
	  typename Security<Decimal>::ConstRandomAccessIterator it = 
	    symbolIterator->second->getRandomAccessIterator (dt); // Or findTimeSeriesEntry(dt)

	  if (it != symbolIterator->second->getRandomAccessIteratorEnd()) // Check against correct end iterator
	    return (*it);
	  else
	    throw StrategyBrokerException ("StrategyBroker::getEntryBar - Bar data not found for " + tradingSymbol + " at " + boost::posix_time::to_simple_string(dt));
	}
      else
	throw StrategyBrokerException ("StrategyBroker::getEntryBar - Cannot find " +tradingSymbol +" in portfolio");
    }


     /**
     * @brief Creates a new `TradingPositionLong` instance based on an executed order.
     * The new position observes this `StrategyBroker` instance for closure events.
     * @param order Pointer to the executed `TradingOrder` that opened the position.
     * @param stopLoss The stop-loss price for the position.
     * @param profitTarget The profit target price for the position.
     * @return A shared pointer to the newly created `TradingPositionLong`.
     */
    std::shared_ptr<TradingPositionLong<Decimal>>
    createLongTradingPosition (TradingOrder<Decimal> *order, 
			       const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			       const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
    {
      auto position = std::make_shared<TradingPositionLong<Decimal>> (order->getTradingSymbol(), 
								      order->getFillPrice(),
								      // Use ptime version of getEntryBar
								      getEntryBar (order->getTradingSymbol(), 
										   order->getFillDateTime()),
								      order->getUnitsInOrder());
      position->setStopLoss(stopLoss);
      position->setProfitTarget(profitTarget);
      position->addObserver (*this);
      return position;
    }

    /**
     * @brief Creates a new `TradingPositionShort` instance based on an executed order.
     * The new position observes this `StrategyBroker` instance for closure events.
     * @param order Pointer to the executed `TradingOrder` that opened the position.
     * @param stopLoss The stop-loss price for the position.
     * @param profitTarget The profit target price for the position.
     * @return A shared pointer to the newly created `TradingPositionShort`.
     */
    std::shared_ptr<TradingPositionShort<Decimal>>
    createShortTradingPosition (TradingOrder<Decimal> *order,
				const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
				const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
    {
      auto position = 
	std::make_shared<TradingPositionShort<Decimal>> (order->getTradingSymbol(), 
						      order->getFillPrice(),
						      // Use ptime version of getEntryBar
						      getEntryBar (order->getTradingSymbol(), 
								   order->getFillDateTime()),
						      order->getUnitsInOrder());

      position->setStopLoss(stopLoss);
      position->setProfitTarget(profitTarget);

      position->addObserver (*this);
      return position;
    }

    /**
     * @brief Creates a new `StrategyTransaction` linking an entry order with its resulting trading position.
     * This transaction represents the start of a trade's lifecycle.
     * @param order A shared pointer to the entry `TradingOrder`.
     * @param position A shared pointer to the `TradingPosition` created by the order.
     * @return A shared pointer to the newly created `StrategyTransaction`.
     */
    std::shared_ptr <StrategyTransaction<Decimal>>
    createStrategyTransaction (std::shared_ptr<TradingOrder<Decimal>> order,
			       std::shared_ptr<TradingPosition<Decimal>> position)
    {
      return std::make_shared<StrategyTransaction<Decimal>>(order, position);
    }

     /**
     * @brief Common logic to handle the execution of an exit order (e.g., sell, cover).
     * This template method finds all `TradingPosition` units for the given symbol managed by `InstrumentPositionManager`,
     * marks their corresponding `StrategyTransaction` in `mStrategyTrades` as complete with the provided exit order,
     * and then instructs the `InstrumentPositionManager` to close out all positions for the symbol at the fill price and datetime.
     * @tparam T The type of the executed exit order (e.g., MarketOnOpenSellOrder, CoverAtLimitOrder).
     * @param order Pointer to the executed exit order.
     * @throws StrategyBrokerException if the strategy transaction for a closing position cannot be found.
     */
    template <typename T>
    void ExitOrderExecutedCommon (T *order)
    {
      InstrumentPosition<Decimal> instrumentPosition =
	mInstrumentPositionManager.getInstrumentPosition (order->getTradingSymbol());
      typename InstrumentPosition<Decimal>::ConstInstrumentPositionIterator positionIterator = 
	instrumentPosition.beginInstrumentPosition();
      typename StrategyTransactionManager<Decimal>::StrategyTransactionIterator transactionIterator;
      shared_ptr<StrategyTransaction<Decimal>> aTransaction;
      std::shared_ptr<TradingPosition<Decimal>> pos;
      auto exitOrder = std::make_shared<T>(*order);

      for (; positionIterator != instrumentPosition.endInstrumentPosition(); positionIterator++)
	{
	  pos = *positionIterator;
	  transactionIterator = mStrategyTrades.findStrategyTransaction (pos->getPositionID());
	  if (transactionIterator != mStrategyTrades.endStrategyTransaction())
	    {
	      aTransaction = transactionIterator->second;
	      aTransaction->completeTransaction (exitOrder);
	    }
	  else
	    {
	      throw StrategyBrokerException("Unable to find StrategyTransaction for symbol: " +order->getTradingSymbol());
	    }
	}

      // Use ptime version for closing positions
      mInstrumentPositionManager.closeAllPositions (order->getTradingSymbol(),
						    order->getFillDateTime(), // Use ptime
						    order->getFillPrice()); 
      
    }

  private:
    TradingOrderManager<Decimal> mOrderManager;
    InstrumentPositionManager<Decimal> mInstrumentPositionManager;
    StrategyTransactionManager<Decimal> mStrategyTrades;
    ClosedPositionHistory<Decimal> mClosedTradeHistory;
    std::shared_ptr<Portfolio<Decimal>> mPortfolio;
  };


}
#endif
