// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STRATEGY_BROKER_H
#define __STRATEGY_BROKER_H 1

#include <exception>
#include <algorithm>  // std::max
#include <unordered_map>  // For tracking individual unit orders
#include <set>  // For tracking multiple orders per position
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

    ~StrategyBrokerException() {}
  };

  // ---------------------------------------------------------------------------
  // Policy types for execution tick adjustment (header-only, zero runtime cost)
  // ---------------------------------------------------------------------------

  /**
   * @brief No-op fractional policy (default).
   * Leaves the tick unchanged.
   */
  template <class Decimal>
  struct NoFractions {
    static Decimal apply(const boost::gregorian::date&,
                         const std::shared_ptr<SecurityAttributes<Decimal>>&,
                         Decimal tickIn) {
      return tickIn;
    }
  };

  /**
   * @brief NYSE-style pre-2001 fractional grid.
   * - < 1997-06-01 : 1/8
   * - < 2001-04-09 : 1/16
   * - >= 2001-04-09: decimal ticks
   * Applies only to equities.
   */
  template <class Decimal>
  struct NysePre2001Fractions {
    static Decimal apply(const boost::gregorian::date& d,
                         const std::shared_ptr<SecurityAttributes<Decimal>>& attrs,
                         Decimal tickIn)
    {
      if (!attrs || !attrs->isEquitySecurity())
	return tickIn;

      using boost::gregorian::date;

      const date cut_1_8  (1997, 6, 1);
      const date cut_1_16 (2001, 4, 9);
      const Decimal eighth    = num::fromString<Decimal>("0.125");
      const Decimal sixteenth = num::fromString<Decimal>("0.0625");

      if (d < cut_1_8)
	return std::max(tickIn, eighth);

      if (d < cut_1_16)
	return std::max(tickIn, sixteenth);

      return tickIn;
    }
  };

  /**
   * @brief SEC Rule 612 sub-penny policy.
   *
   * If PricesAreSplitAdjusted==true (typical for adjusted historical series),
   * sub-pennies for < $1 are DISABLED (to avoid spurious sub-$1 from split adjustments).
   * If PricesAreSplitAdjusted==false (unadjusted quotes), allows $0.0001 when refPrice < $1.
   * Always enforces a minimum of $0.01 for refPrice >= $1 for equities.
   * 
   * You not want Rule612SubPenny to modify the tick if prices are split adjusted because with
   * split-adjusted histories the number you see on a given day is not the price traders actually
   * quoted that day. A $50 stock in 2000 may appear as $0.25 after later splits. If we blindly
   * apply Rule 612 (“< $1 may quote in $0.0001”) to that adjusted $0.25, we’d allow sub-penny
   * orders that were impossible in the real market, biasing fills and P&L.
   *
   * Why sub-pennies + split-adjusted prices is risky
   *
   * False <$1 triggers. Split adjustment can push historic prices below $1 even when the contemporaneous price
   * was $10, $50, etc. Treating those as sub-$1 will:
   *
   * let you place tighter stops/targets,
   *
   * round to a finer grid ($0.0001) than the market allowed,
   *
   * create optimistic, non-realistic fills.
   *
   * Historical rules mismatch. Before 2001 you had fractions (1/8, 1/16 …), later cents,
   * and only much later (Rule 612) sub-pennies for true <$1 quotes. Split-adjusted levels
   * blur those regime boundaries.
   *
   * Conservative vs optimistic bias. Using coarser ticks than reality (e.g., always $0.01)
   * is conservative (harder to fill); using finer ticks than reality (allowing $0.0001 due
   * to artificial < $1) is optimistic (easier to fill). For backtests, we’d rather avoid the
   * ptimistic error.
   */
  template <class Decimal, bool PricesAreSplitAdjusted = true>
  struct Rule612SubPenny {
    static Decimal apply(const Decimal& refPrice,
                         const std::shared_ptr<SecurityAttributes<Decimal>>& attrs,
                         Decimal tickIn) {
      if (!attrs || !attrs->isEquitySecurity())
	return tickIn;

      const Decimal one   = DecimalConstants<Decimal>::DecimalOne;
      const Decimal cent  = DecimalConstants<Decimal>::EquityTick;        // 0.01
      const Decimal subp  = num::fromString<Decimal>("0.0001");

      // ≥ $1: at least a cent
      Decimal tick = std::max(tickIn, cent);

      // < $1: only enable sub-pennies if NOT split-adjusted
      if constexpr (!PricesAreSplitAdjusted)
	{
	  if (refPrice < one)
	    tick = std::min(tick, subp);
	}

      return tick;
    }
  };

  /**
   * @class StrategyBroker
   * @brief Manages trading order execution, instrument position tracking, and historical trade logging,
   * serving as a crucial component within a backtesting environment.
   *
   * @tparam Decimal The decimal type used for financial calculations.
   * @tparam FractionPolicy Policy to simulate historical fractional tick sizes (e.g., 1/8ths, 1/16ths). Defaults to NoFractions.
   * @tparam SubPennyPolicy Policy to enforce SEC Rule 612 regarding sub-penny pricing. Defaults to Rule612SubPenny.
   * @tparam PricesAreSplitAdjusted Informs policies if price data is split-adjusted, affecting rules for sub-penny pricing. Defaults to true.
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
   * These transactions are stored in the `StrategyTransactionManager`.
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
   * each representing the full lifecycle of a trade (entry order, position, exit order).
   * - `ClosedPositionHistory`: Stores a history of all closed trading positions (derived from completed
   * `StrategyTransaction`s).
   *
   * - `Portfolio`: Provides access to security information, such as tick size and historical price data,
   * necessary for order processing and position valuation.
   */
  template <class Decimal,
            template<class> class FractionPolicy   = NoFractions,
            template<class, bool> class SubPennyPolicy = Rule612SubPenny,
            bool PricesAreSplitAdjusted = true>
  class StrategyBroker :
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

    ~StrategyBroker() {}

    StrategyBroker(const StrategyBroker<Decimal, FractionPolicy, SubPennyPolicy, PricesAreSplitAdjusted> &rhs)
      : TradingOrderObserver<Decimal>(rhs),
        TradingPositionObserver<Decimal>(rhs),
        mOrderManager(rhs.mOrderManager),
        mInstrumentPositionManager(rhs.mInstrumentPositionManager),
        mStrategyTrades(rhs.mStrategyTrades),
        mClosedTradeHistory(rhs.mClosedTradeHistory),
        mPortfolio(rhs.mPortfolio)
    {}

    StrategyBroker<Decimal, FractionPolicy, SubPennyPolicy, PricesAreSplitAdjusted>&
    operator=(const StrategyBroker<Decimal, FractionPolicy, SubPennyPolicy, PricesAreSplitAdjusted> &rhs)
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
    uint32_t getTotalTrades() const { return  mStrategyTrades.getTotalTrades(); }
    
    /**
     * @brief Gets the number of currently open trades (strategy transactions that have an entry but no exit yet).
     * This count is derived from the `StrategyTransactionManager`.
     * @return The number of open trades.
     */
    uint32_t getOpenTrades()  const { return  mStrategyTrades.getOpenTrades(); }

    /**
     * @brief Gets the number of closed trades (strategy transactions that have both an entry and an exit).
     * This count is derived from the `StrategyTransactionManager`.
     * @return The number of closed trades.
     */
    uint32_t getClosedTrades()const { return  mStrategyTrades.getClosedTrades(); }

    /**
     * @brief Checks if there is an open long position for the specified trading symbol.
     * @param s The symbol of the instrument.
     * @return True if a long position exists, false otherwise.
     */
    bool isLongPosition (const std::string& s) const { return mInstrumentPositionManager.isLongPosition (s); }
    
    /**
     * @brief Checks if there is an open short position for the specified trading symbol.
     * @param s The symbol of the instrument.
     * @return True if a short position exists, false otherwise.
     */
    bool isShortPosition(const std::string& s) const { return mInstrumentPositionManager.isShortPosition(s); }

    /**
     * @brief Checks if there is no open position (flat) for the specified trading symbol.
     * @param s The symbol of the instrument.
     * @return True if the position is flat, false otherwise.
     */
    bool isFlatPosition (const std::string& s) const { return mInstrumentPositionManager.isFlatPosition (s); }

    // ---------------------------
    // Date-based convenience APIs
    // ---------------------------
    void EnterLongOnOpen (const std::string& s, const date& d, const TradingVolume& u,
                          const Decimal& sl = DecimalConstants<Decimal>::DecimalZero,
                          const Decimal& pt = DecimalConstants<Decimal>::DecimalZero)
    { EnterLongOnOpen(s, ptime(d, getDefaultBarTime()), u, sl, pt); }

    void EnterShortOnOpen(const std::string& s, const date& d, const TradingVolume& u,
                          const Decimal& sl = DecimalConstants<Decimal>::DecimalZero,
                          const Decimal& pt = DecimalConstants<Decimal>::DecimalZero)
    { EnterShortOnOpen(s, ptime(d, getDefaultBarTime()), u, sl, pt); }

    void ExitLongAllUnitsOnOpen (const std::string& s, const date& d, const TradingVolume& u)
    { ExitLongAllUnitsOnOpen(s, ptime(d, getDefaultBarTime()), u); }

    void ExitLongAllUnitsOnOpen (const std::string& s, const date& d)
    { ExitLongAllUnitsOnOpen(s, ptime(d, getDefaultBarTime())); }

    void ExitShortAllUnitsOnOpen(const std::string& s, const date& d)
    { ExitShortAllUnitsOnOpen(s, ptime(d, getDefaultBarTime())); }

    void ExitLongAllUnitsAtLimit (const std::string& s, const date& d, const Decimal& px)
    { ExitLongAllUnitsAtLimit(s, ptime(d, getDefaultBarTime()), px); }



    void ExitLongAllUnitsAtLimit (const std::string& s, const date& d,
                                  const Decimal& base, const PercentNumber<Decimal>& pct)
    { ExitLongAllUnitsAtLimit(s, ptime(d, getDefaultBarTime()), base, pct); }

    void ExitShortAllUnitsAtLimit(const std::string& s, const date& d, const Decimal& px)
    { ExitShortAllUnitsAtLimit(s, ptime(d, getDefaultBarTime()), px); }

    void ExitShortAllUnitsAtLimit(const std::string& s, const date& d,
                                  const Decimal& base, const PercentNumber<Decimal>& pct)
    { ExitShortAllUnitsAtLimit(s, ptime(d, getDefaultBarTime()), base, pct); }

    void ExitLongAllUnitsAtStop  (const std::string& s, const date& d, const Decimal& px)
    { ExitLongAllUnitsAtStop(s, ptime(d, getDefaultBarTime()), px); }

    void ExitLongAllUnitsAtStop  (const std::string& s, const date& d,
                                  const Decimal& base, const PercentNumber<Decimal>& pct)
    { ExitLongAllUnitsAtStop(s, ptime(d, getDefaultBarTime()), base, pct); }

    void ExitShortAllUnitsAtStop (const std::string& s, const date& d, const Decimal& px)
    { ExitShortAllUnitsAtStop(s, ptime(d, getDefaultBarTime()), px); }

    void ExitShortAllUnitsAtStop (const std::string& s, const date& d,
                                  const Decimal& base, const PercentNumber<Decimal>& pct)
    { ExitShortAllUnitsAtStop(s, ptime(d, getDefaultBarTime()), base, pct); }

    // -------------------------
    // Ptime-based order entries
    // -------------------------
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
     * @param unitsInOrder Number of units to exit.
     */
    void ExitLongAllUnitsOnOpen(const std::string& tradingSymbol,
                                const ptime& orderDateTime,
                                const TradingVolume& unitsInOrder)
    {
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol)) {
        auto order = std::make_shared<MarketOnOpenSellOrder<Decimal>>(tradingSymbol,
                                                                      unitsInOrder,
                                                                      orderDateTime); // Use ptime
        mOrderManager.addTradingOrder (order);
      } else {
        throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtOpen - no long position for " + tradingSymbol +
                                      " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
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
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol)) {
        ExitLongAllUnitsOnOpen(tradingSymbol, orderDateTime,
                               mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol));
      } else {
        throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtOpen - no long position for " + tradingSymbol +
                                      " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
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
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol)) {
        auto order = std::make_shared<MarketOnOpenCoverOrder<Decimal>>(tradingSymbol,
                                                                       mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
                                                                       orderDateTime); // Use ptime
        mOrderManager.addTradingOrder (order);
      } else {
        StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtOpen - no short position for " + tradingSymbol +
                                " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
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
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol)) {
        auto order = std::make_shared<SellAtLimitOrder<Decimal>>(tradingSymbol,
                                                                 mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
                                                                 orderDateTime, // Use ptime
                                                                 limitPrice);
        mOrderManager.addTradingOrder (order);
      } else {
        throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtLimit - no long position for " + tradingSymbol +
                                      " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
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
      const Decimal target = profitTarget.getProfitTarget();

      const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, limitBasePrice, target);
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
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol)) {
        auto order = std::make_shared<CoverAtLimitOrder<Decimal>>(tradingSymbol,
                                                                  mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
                                                                  orderDateTime, // Use ptime
                                                                  limitPrice);
        mOrderManager.addTradingOrder (order);
      } else {
        throw StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtLimit - no short position for " + tradingSymbol +
                                      " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
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
      const Decimal target = Decimal(percentTarget.getProfitTarget());

      const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, limitBasePrice, target);
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
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol)) {
        auto order = std::make_shared<SellAtStopOrder<Decimal>>(tradingSymbol,
                                                                mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
                                                                orderDateTime, // Use ptime
                                                                stopPrice);
        mOrderManager.addTradingOrder (order);
      } else {
        throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtStop - no long position for " + tradingSymbol +
                                      " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
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
      const Decimal stopPx = Decimal(percentStop.getStopLoss());

      const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, stopBasePrice, stopPx);
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
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol)) {
        auto order = std::make_shared<CoverAtStopOrder<Decimal>>(tradingSymbol,
                                                                 mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
                                                                 orderDateTime, // Use ptime
                                                                 stopPrice);
        mOrderManager.addTradingOrder (order);
      } else {
        throw StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtStop - no short position for " + tradingSymbol +
                                      " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
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
      const Decimal stopPx = Decimal(aPercentStop.getStopLoss());

      const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, stopBasePrice, stopPx);
      this->ExitShortAllUnitsAtStop(tradingSymbol, orderDateTime, orderPrice); // Calls ptime version
    }

    // -------------------------
    // Individual unit exit methods for pyramiding support
    // -------------------------

    /**
     * @brief Exit a specific long position unit at market-open using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no long position exists, unit number is invalid, or position data is unavailable.
     */
    void ExitLongUnitOnOpen(const std::string& tradingSymbol,
                            const ptime& orderDateTime,
                            uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isLongPosition(tradingSymbol))
 {
   // Get the specific unit
   const InstrumentPosition<Decimal>& instrPos =
     mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

   // Validate unit number
   if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
     {
       throw StrategyBrokerException("ExitLongUnitOnOpen - Invalid unit number " +
    	    std::to_string(unitNumber) + " for " + tradingSymbol +
    	    " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
     }

   // Get specific unit's volume
   auto unitIt = instrPos.getInstrumentPosition(unitNumber);
   TradingVolume unitVolume = (*unitIt)->getTradingUnits();
   uint32_t positionId = (*unitIt)->getPositionID();

   // Create order for this specific unit
   auto order = std::make_shared<MarketOnOpenSellOrder<Decimal>>(tradingSymbol,
    					unitVolume,
    					orderDateTime);
   // Track this order as an individual unit exit
   mUnitExitOrders[order->getOrderID()] = positionId;
   mPositionToOrders[positionId].insert(order->getOrderID());
   mOrderManager.addTradingOrder(order);
 }
      else
 {
   throw StrategyBrokerException("ExitLongUnitOnOpen - no long position for " + tradingSymbol +
    	" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
 }
    }

    /**
     * @brief Exit a specific short position unit at market-open using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no short position exists, unit number is invalid, or position data is unavailable.
     */
    void ExitShortUnitOnOpen(const std::string& tradingSymbol,
                             const ptime& orderDateTime,
                             uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isShortPosition(tradingSymbol))
	{
	  // Get the specific unit
	  const InstrumentPosition<Decimal>& instrPos =
	    mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

	  // Validate unit number
	  if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
	    {
	      throw StrategyBrokerException("ExitShortUnitOnOpen - Invalid unit number " +
					    std::to_string(unitNumber) + " for " + tradingSymbol +
					    " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
	    }

	  // Get specific unit's volume
	  auto unitIt = instrPos.getInstrumentPosition(unitNumber);
	  TradingVolume unitVolume = (*unitIt)->getTradingUnits();
	  uint32_t positionId = (*unitIt)->getPositionID();

	  // Create order for this specific unit
	  auto order = std::make_shared<MarketOnOpenCoverOrder<Decimal>>(tradingSymbol,
									 unitVolume,
									 orderDateTime);
        // Track this order as an individual unit exit
        mUnitExitOrders[order->getOrderID()] = positionId;
        mPositionToOrders[positionId].insert(order->getOrderID());
        mOrderManager.addTradingOrder(order);
	}
      else
	{
	  throw StrategyBrokerException("ExitShortUnitOnOpen - no short position for " + tradingSymbol +
					" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
	}
    }

    /**
     * @brief Exit a specific long position unit at a limit price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param limitPrice The limit price at which to sell.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no long position exists, unit number is invalid, or position data is unavailable.
     */
    void ExitLongUnitAtLimit(const std::string& tradingSymbol,
                             const ptime& orderDateTime,
                             const Decimal& limitPrice,
                             uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isLongPosition(tradingSymbol))
 {
   // Get the specific unit
   const InstrumentPosition<Decimal>& instrPos =
     mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

   // Validate unit number
   if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
     {
       throw StrategyBrokerException("ExitLongUnitAtLimit - Invalid unit number " +
    	    std::to_string(unitNumber) + " for " + tradingSymbol +
    	    " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
     }

   // Get specific unit's volume
   auto unitIt = instrPos.getInstrumentPosition(unitNumber);
   TradingVolume unitVolume = (*unitIt)->getTradingUnits();
   uint32_t positionId = (*unitIt)->getPositionID();

   // Create order for this specific unit
   auto order = std::make_shared<SellAtLimitOrder<Decimal>>(tradingSymbol,
    				   unitVolume,
    				   orderDateTime,
    			   limitPrice);
   // Track this order as an individual unit exit
   mUnitExitOrders[order->getOrderID()] = positionId;
   mPositionToOrders[positionId].insert(order->getOrderID());
   mOrderManager.addTradingOrder(order);
 }
      else
 {
   throw StrategyBrokerException("ExitLongUnitAtLimit - no long position for " + tradingSymbol +
    	" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
 }
    }

    /**
     * @brief Exit a specific long position unit at a limit price calculated as a percentage above the unit's entry price.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param limitBasePrice The base price for calculating the limit price (typically the unit's entry price).
     * @param percentNum The percentage above the base price to set the limit.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no long position exists, unit number is invalid, or tick data is unavailable.
     */
    void ExitLongUnitAtLimit(const std::string& tradingSymbol,
                             const ptime& orderDateTime,
                             const Decimal& limitBasePrice,
                             const PercentNumber<Decimal>& percentNum,
                             uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isLongPosition(tradingSymbol))
	{
	  // Get the specific unit
	  const InstrumentPosition<Decimal>& instrPos =
	    mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

        // Validate unit number
        if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
	  {
	    throw StrategyBrokerException("ExitLongUnitAtLimit - Invalid unit number " +
					  std::to_string(unitNumber) + " for " + tradingSymbol +
					  " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
	  }

        // Calculate limit price based on the provided base price (should be unit's entry price)
        LongProfitTarget<Decimal> profitTarget(limitBasePrice, percentNum);
        const Decimal target = profitTarget.getProfitTarget();
        const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, limitBasePrice, target);

        // Call the fixed-price version
        this->ExitLongUnitAtLimit(tradingSymbol, orderDateTime, orderPrice, unitNumber);
      }
      else
	{
	  throw StrategyBrokerException("ExitLongUnitAtLimit - no long position for " + tradingSymbol +
					" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
	}
    }

    /**
     * @brief Exit a specific short position unit at a limit price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param limitPrice The limit price at which to cover.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no short position exists, unit number is invalid, or position data is unavailable.
     */
    void ExitShortUnitAtLimit(const std::string& tradingSymbol,
                              const ptime& orderDateTime,
                              const Decimal& limitPrice,
                              uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isShortPosition(tradingSymbol))
	{
	  // Get the specific unit
	  const InstrumentPosition<Decimal>& instrPos =
	    mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

	  // Validate unit number
	  if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
	    {
	      throw StrategyBrokerException("ExitShortUnitAtLimit - Invalid unit number " +
					    std::to_string(unitNumber) + " for " + tradingSymbol +
					    " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
	    }

	  // Get specific unit's volume
	  auto unitIt = instrPos.getInstrumentPosition(unitNumber);
	  TradingVolume unitVolume = (*unitIt)->getTradingUnits();
	  uint32_t positionId = (*unitIt)->getPositionID();

	  // Create order for this specific unit
	  auto order = std::make_shared<CoverAtLimitOrder<Decimal>>(tradingSymbol,
								    unitVolume,
								    orderDateTime,
								    limitPrice);
	  // Track this order as an individual unit exit
	  mUnitExitOrders[order->getOrderID()] = positionId;
	  mPositionToOrders[positionId].insert(order->getOrderID());
	  mOrderManager.addTradingOrder(order);
      }
      else
	{
	  throw StrategyBrokerException("ExitShortUnitAtLimit - no short position for " + tradingSymbol +
					" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
	}
    }

    /**
     * @brief Exit a specific short position unit at a limit price calculated as a percentage below the unit's entry price.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param limitBasePrice The base price for calculating the limit price (typically the unit's entry price).
     * @param percentNum The percentage below the base price to set the limit.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no short position exists, unit number is invalid, or tick data is unavailable.
     */
    void ExitShortUnitAtLimit(const std::string& tradingSymbol,
                              const ptime& orderDateTime,
                              const Decimal& limitBasePrice,
                              const PercentNumber<Decimal>& percentNum,
                              uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isShortPosition(tradingSymbol))
	{
	  // Get the specific unit
	  const InstrumentPosition<Decimal>& instrPos =
	    mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

        // Validate unit number
        if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
	  {
	    throw StrategyBrokerException("ExitShortUnitAtLimit - Invalid unit number " +
					  std::to_string(unitNumber) + " for " + tradingSymbol +
					  " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
	  }

        // Calculate limit price based on the provided base price (should be unit's entry price)
        ShortProfitTarget<Decimal> profitTarget(limitBasePrice, percentNum);
        const Decimal target = profitTarget.getProfitTarget();
        const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, limitBasePrice, target);

        // Call the fixed-price version
        this->ExitShortUnitAtLimit(tradingSymbol, orderDateTime, orderPrice, unitNumber);
	}
      else
	{
	  throw StrategyBrokerException("ExitShortUnitAtLimit - no short position for " + tradingSymbol +
					" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
	}
    }

    /**
     * @brief Exit a specific long position unit at a stop price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param stopPrice The stop price at which to sell.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no long position exists, unit number is invalid, or position data is unavailable.
     */
    void ExitLongUnitAtStop(const std::string& tradingSymbol,
                            const ptime& orderDateTime,
                            const Decimal& stopPrice,
                            uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isLongPosition(tradingSymbol))
 {
   // Get the specific unit
   const InstrumentPosition<Decimal>& instrPos =
     mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

   // Validate unit number
   if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
     {
       throw StrategyBrokerException("ExitLongUnitAtStop - Invalid unit number " +
    	    std::to_string(unitNumber) + " for " + tradingSymbol +
    	    " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
     }

   // Get specific unit's volume
   auto unitIt = instrPos.getInstrumentPosition(unitNumber);
   TradingVolume unitVolume = (*unitIt)->getTradingUnits();
   uint32_t positionId = (*unitIt)->getPositionID();

   // Create order for this specific unit
   auto order = std::make_shared<SellAtStopOrder<Decimal>>(tradingSymbol,
    				  unitVolume,
    				  orderDateTime,
    				  stopPrice);
   // Track this order as an individual unit exit
   mUnitExitOrders[order->getOrderID()] = positionId;
   mPositionToOrders[positionId].insert(order->getOrderID());
   mOrderManager.addTradingOrder(order);
 }
      else
 {
   throw StrategyBrokerException("ExitLongUnitAtStop - no long position for " + tradingSymbol +
    	" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
 }
    }

    /**
     * @brief Exit a specific long position unit at a stop price calculated as a percentage below the unit's entry price.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param stopBasePrice The base price for calculating the stop price (typically the unit's entry price).
     * @param percentNum The percentage below the base price to set the stop.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no long position exists, unit number is invalid, or tick data is unavailable.
     */
    void ExitLongUnitAtStop(const std::string& tradingSymbol,
                            const ptime& orderDateTime,
                            const Decimal& stopBasePrice,
                            const PercentNumber<Decimal>& percentNum,
                            uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isLongPosition(tradingSymbol))
	{
	  // Get the specific unit
	  const InstrumentPosition<Decimal>& instrPos =
	    mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);
 
	  // Validate unit number
	  if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
	    {
	      throw StrategyBrokerException("ExitLongUnitAtStop - Invalid unit number " +
					    std::to_string(unitNumber) + " for " + tradingSymbol +
					    " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
	  }

	  // Calculate stop price based on the provided base price (should be unit's entry price)
	  LongStopLoss<Decimal> stopLoss(stopBasePrice, percentNum);
	  const Decimal stopPx = stopLoss.getStopLoss();
	  const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, stopBasePrice, stopPx);

        // Call the fixed-price version
        this->ExitLongUnitAtStop(tradingSymbol, orderDateTime, orderPrice, unitNumber);
      }
      else
	{
	  throw StrategyBrokerException("ExitLongUnitAtStop - no long position for " + tradingSymbol +
					" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
	}
    }

    /**
     * @brief Exit a specific short position unit at a stop price using ptime.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param stopPrice The stop price at which to cover.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no short position exists, unit number is invalid, or position data is unavailable.
     */
    void ExitShortUnitAtStop(const std::string& tradingSymbol,
                             const ptime& orderDateTime,
                             const Decimal& stopPrice,
                             uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isShortPosition(tradingSymbol))
	{
	  // Get the specific unit
	  const InstrumentPosition<Decimal>& instrPos =
	    mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

	  // Validate unit number
	  if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0) {
	    throw StrategyBrokerException("ExitShortUnitAtStop - Invalid unit number " +
					  std::to_string(unitNumber) + " for " + tradingSymbol +
					  " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
	  }

	  // Get specific unit's volume
	  auto unitIt = instrPos.getInstrumentPosition(unitNumber);
	  TradingVolume unitVolume = (*unitIt)->getTradingUnits();
	  uint32_t positionId = (*unitIt)->getPositionID();
        
	  // Create order for this specific unit
	  auto order = std::make_shared<CoverAtStopOrder<Decimal>>(tradingSymbol,
								   unitVolume,
								   orderDateTime,
								   stopPrice);
	  // Track this order as an individual unit exit
	  mUnitExitOrders[order->getOrderID()] = positionId;
	  mPositionToOrders[positionId].insert(order->getOrderID());
	  mOrderManager.addTradingOrder(order);
	}
      else
	{
	  throw StrategyBrokerException("ExitShortUnitAtStop - no short position for " + tradingSymbol +
					" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
	}
    }

    /**
     * @brief Exit a specific short position unit at a stop price calculated as a percentage above the unit's entry price.
     * @param tradingSymbol The symbol of the instrument.
     * @param orderDateTime The exact date and time of the exit order.
     * @param stopBasePrice The base price for calculating the stop price (typically the unit's entry price).
     * @param percentNum The percentage above the base price to set the stop.
     * @param unitNumber The 1-based index of the unit to exit.
     * @throws StrategyBrokerException if no short position exists, unit number is invalid, or tick data is unavailable.
     */
    void ExitShortUnitAtStop(const std::string& tradingSymbol,
                             const ptime& orderDateTime,
                             const Decimal& stopBasePrice,
                             const PercentNumber<Decimal>& percentNum,
                             uint32_t unitNumber)
    {
      if (mInstrumentPositionManager.isShortPosition(tradingSymbol))
	{
	  // Get the specific unit
	  const InstrumentPosition<Decimal>& instrPos =
	    mInstrumentPositionManager.getInstrumentPosition(tradingSymbol);

	  // Validate unit number
	  if (unitNumber > instrPos.getNumPositionUnits() || unitNumber == 0)
	    {
	      throw StrategyBrokerException("ExitShortUnitAtStop - Invalid unit number " +
					    std::to_string(unitNumber) + " for " + tradingSymbol +
					    " (valid range: 1-" + std::to_string(instrPos.getNumPositionUnits()) + ")");
	    }
        
	  // Calculate stop price based on the provided base price (should be unit's entry price)
	  ShortStopLoss<Decimal> stopLoss(stopBasePrice, percentNum);
	  const Decimal stopPx = stopLoss.getStopLoss();
	  const Decimal orderPrice = roundToExecutionTick(tradingSymbol, orderDateTime, stopBasePrice, stopPx);
        
	  // Call the fixed-price version
	  this->ExitShortUnitAtStop(tradingSymbol, orderDateTime, orderPrice, unitNumber);
	}
      else
	{
	  throw StrategyBrokerException("ExitShortUnitAtStop - no short position for " + tradingSymbol +
					" with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
	}
    }

    // -----------------------
    // Order manager plumbing
    // -----------------------
    /**
     * @brief Returns a constant iterator to the beginning of pending orders in the `TradingOrderManager`.
     * @return A PendingOrderIterator pointing to the first pending order.
     */
    PendingOrderIterator beginPendingOrders() const { return mOrderManager.beginPendingOrders(); }
    
    /**
     * @brief Returns a constant iterator to the end of pending orders in the `TradingOrderManager`.
     * @return A PendingOrderIterator pointing past the last pending order.
     */
    PendingOrderIterator endPendingOrders()   const { return mOrderManager.endPendingOrders();   }

    /**
     * @brief Processes all pending orders for a given date (using default bar time).
     * This method is critical in a backtesting loop. It first updates the open positions with the current day's bar data
     * and then instructs the TradingOrderManager to attempt to fill any pending orders based on this new data.
     * @param d The date for which orders are to be processed.
     */
    void ProcessPendingOrders(const date& d)
    { this->ProcessPendingOrders(ptime(d, getDefaultBarTime())); }

    /**
     * @brief Processes all pending orders for a given datetime.
     * This method is critical in a backtesting loop. It first updates the open positions with the current datetime's bar data
     * and then instructs the TradingOrderManager to attempt to fill any pending orders based on this new data.
     * @param dt The datetime for which orders are to be processed.
     */
    void ProcessPendingOrders(const ptime& dt)
    {
      // Add historical bar for this datetime before possibly closing any open positions
      mInstrumentPositionManager.addBarForOpenPosition (dt, mPortfolio.get());
      mOrderManager.processPendingOrders (dt, mInstrumentPositionManager);
    }

    // ----------------------------
    // Order/position event handlers
    // ----------------------------
    /**
     * @brief Callback invoked when a MarketOnOpenLongOrder is executed.
     * Creates a new long trading position and records the transaction.
     * @param order Pointer to the executed MarketOnOpenLongOrder.
     */
    void OrderExecuted (MarketOnOpenLongOrder <Decimal> *order) override
    {
      auto position = createLongTradingPosition (order, order->getStopLoss(), order->getProfitTarget());
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
      auto position = createShortTradingPosition (order, order->getStopLoss(), order->getProfitTarget());
      auto pOrder = std::make_shared<MarketOnOpenShortOrder<Decimal>>(*order);
      mInstrumentPositionManager.addPosition (position);
      mStrategyTrades.addStrategyTransaction (createStrategyTransaction (pOrder, position));
    }

    /**
     * @brief Callback invoked when a MarketOnOpenSellOrder is executed (exit long).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed MarketOnOpenSellOrder.
     */
    void OrderExecuted (MarketOnOpenSellOrder <Decimal> *order) override
    {
      // Check if this is a unit exit and cancel complementary orders IMMEDIATELY
      auto it = mUnitExitOrders.find(order->getOrderID());
      if (it != mUnitExitOrders.end()) {
        uint32_t positionId = it->second;
        cancelComplementaryOrdersForPosition(positionId, order->getOrderID());
      }
      ExitOrderExecutedCommon(order);
    }
    
    /**
     * @brief Callback invoked when a MarketOnOpenCoverOrder is executed (exit short).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed MarketOnOpenCoverOrder.
     */
    void OrderExecuted (MarketOnOpenCoverOrder<Decimal> *order) override
    {
      // Check if this is a unit exit and cancel complementary orders IMMEDIATELY
      auto it = mUnitExitOrders.find(order->getOrderID());
      if (it != mUnitExitOrders.end()) {
        uint32_t positionId = it->second;
        cancelComplementaryOrdersForPosition(positionId, order->getOrderID());
      }
      ExitOrderExecutedCommon(order);
    }
    
    /**
     * @brief Callback invoked when a SellAtLimitOrder is executed (exit long).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed SellAtLimitOrder.
     */
    void OrderExecuted (SellAtLimitOrder       <Decimal> *order) override
    {
      // Check if this is a unit exit and cancel complementary orders IMMEDIATELY
      auto it = mUnitExitOrders.find(order->getOrderID());
      if (it != mUnitExitOrders.end()) {
        uint32_t positionId = it->second;
        cancelComplementaryOrdersForPosition(positionId, order->getOrderID());
      }
      ExitOrderExecutedCommon(order);
    }
    
    /**
     * @brief Callback invoked when a CoverAtLimitOrder is executed (exit short).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed CoverAtLimitOrder.
     */
    void OrderExecuted (CoverAtLimitOrder      <Decimal> *order) override
    {
      // Check if this is a unit exit and cancel complementary orders IMMEDIATELY
      auto it = mUnitExitOrders.find(order->getOrderID());
      if (it != mUnitExitOrders.end()) {
        uint32_t positionId = it->second;
        cancelComplementaryOrdersForPosition(positionId, order->getOrderID());
      }
      ExitOrderExecutedCommon(order);
    }
    
    /**
     * @brief Callback invoked when a CoverAtStopOrder is executed (exit short).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed CoverAtStopOrder.
     */
    void OrderExecuted (CoverAtStopOrder       <Decimal> *order) override
    {
      // Check if this is a unit exit and cancel complementary orders IMMEDIATELY
      auto it = mUnitExitOrders.find(order->getOrderID());
      if (it != mUnitExitOrders.end()) {
        uint32_t positionId = it->second;
        cancelComplementaryOrdersForPosition(positionId, order->getOrderID());
      }
      ExitOrderExecutedCommon(order);
    }
    
    /**
     * @brief Callback invoked when a SellAtStopOrder is executed (exit long).
     * Handles common logic for exit order execution.
     * @param order Pointer to the executed SellAtStopOrder.
     */
    void OrderExecuted (SellAtStopOrder        <Decimal> *order) override
    {
      // Check if this is a unit exit and cancel complementary orders IMMEDIATELY
      auto it = mUnitExitOrders.find(order->getOrderID());
      if (it != mUnitExitOrders.end()) {
        uint32_t positionId = it->second;
        cancelComplementaryOrdersForPosition(positionId, order->getOrderID());
      }
      ExitOrderExecutedCommon(order);
    }

    void OrderCanceled (MarketOnOpenLongOrder <Decimal> * ) override {}
    void OrderCanceled (MarketOnOpenShortOrder<Decimal> * ) override {}
    void OrderCanceled (MarketOnOpenSellOrder <Decimal> * ) override {}
    void OrderCanceled (MarketOnOpenCoverOrder<Decimal> * ) override {}
    void OrderCanceled (SellAtLimitOrder      <Decimal> * ) override {}
    void OrderCanceled (CoverAtLimitOrder     <Decimal> * ) override {}
    void OrderCanceled (CoverAtStopOrder      <Decimal> * ) override {}
    void OrderCanceled (SellAtStopOrder       <Decimal> * ) override {}

    // -----------------------
    // Positions / transactions
    // -----------------------
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

      if (it != mStrategyTrades.endStrategyTransaction()) {
        mClosedTradeHistory.addClosedPosition (it->second->getTradingPositionPtr());
      } else {
        throw StrategyBrokerException("Unable to find strategy transaction for position id " +
                                      std::to_string(aPosition->getPositionID()));
      }
    }

  public:
    // -----------------
    // Baseline tick API
    // -----------------
    const Decimal getTick(const std::string& symbol) const
    {
      auto& factory = SecurityAttributesFactory<Decimal>::instance();
      auto it = factory.getSecurityAttributes (symbol);

      if (it != factory.endSecurityAttributes())
        return it->second->getTick();
      else
        throw StrategyBrokerException("Strategybroker::getTick - ticker symbol " + symbol + " is unknown");
    }

    const Decimal getTickDiv2(const std::string& symbol) const
    {
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (symbol);
      if (symbolIterator != mPortfolio->endPortfolio()) {
        return symbolIterator->second->getTickDiv2();
      } else {
        throw StrategyBrokerException("Strategybroker::getTickDiv2 - ticker symbol " + symbol + " is unknown");
      }
    }

  private:
    // -------------------
    // Entry bar retrieval
    // -------------------
    OHLCTimeSeriesEntry<Decimal> getEntryBar (const std::string& tradingSymbol, const date& d)
    { return getEntryBar(tradingSymbol, ptime(d, getDefaultBarTime())); }

    /**
     * @brief Retrieves the OHLC time series entry (bar data) for a given symbol and ptime from the `Portfolio`.
     * This data is used, for example, as the entry bar for a new `TradingPosition`.
     * @param tradingSymbol The trading symbol.
     * @param dt The ptime for which to retrieve the bar data.
     * @return The OHLCTimeSeriesEntry for the specified symbol and ptime.
     * @throws StrategyBrokerException if the symbol is not found in the portfolio or if data for the datetime is missing.
     */
    OHLCTimeSeriesEntry<Decimal> getEntryBar (const std::string& tradingSymbol, const ptime& dt)
    {
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (tradingSymbol);
      if (symbolIterator != mPortfolio->endPortfolio()) {
        try {
          return symbolIterator->second->getTimeSeriesEntry(dt);
        } catch (const mkc_timeseries::TimeSeriesDataNotFoundException& e) {
          throw StrategyBrokerException ("StrategyBroker::getEntryBar - Bar data not found for " + tradingSymbol +
                                         " at " + boost::posix_time::to_simple_string(dt) + ": " + e.what());
        }
      } else {
        throw StrategyBrokerException ("StrategyBroker::getEntryBar - Cannot find " + tradingSymbol + " in portfolio");
      }
    }

    // ---------------------------
    // Position / transaction make
    // ---------------------------
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
      auto position = std::make_shared<TradingPositionLong<Decimal>>(order->getTradingSymbol(),
                                                                     order->getFillPrice(),
                                                                     getEntryBar(order->getTradingSymbol(),
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
        std::make_shared<TradingPositionShort<Decimal>>(order->getTradingSymbol(),
                                                        order->getFillPrice(),
                                                        getEntryBar(order->getTradingSymbol(),
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
     * @brief Common logic to handle the execution of an exit order for a specific unit.
     * This method finds the specific `TradingPosition` unit, marks its corresponding
     * `StrategyTransaction` as complete, and closes only that specific unit.
     * @tparam T The type of the executed exit order.
     * @param order Pointer to the executed exit order.
     * @param positionId The ID of the position unit to close.
     * @throws StrategyBrokerException if the strategy transaction for the closing position cannot be found.
     */
    template <typename T>
    void ExitUnitOrderExecutedCommon (T *order, uint32_t positionId)
    {
      const InstrumentPosition<Decimal>& instrumentPosition =
        mInstrumentPositionManager.getInstrumentPosition (order->getTradingSymbol());

      uint32_t unitNumber = 0;
      uint32_t currentUnit = 1;
      std::shared_ptr<const TradingPosition<Decimal>> pos = nullptr;

      for (auto it = instrumentPosition.beginInstrumentPosition(); it != instrumentPosition.endInstrumentPosition(); ++it, ++currentUnit) {
          if ((*it)->getPositionID() == positionId) {
              unitNumber = currentUnit;
              pos = *it;
              break;
          }
      }

      if (pos == nullptr) {
          throw StrategyBrokerException("ExitUnitOrderExecutedCommon - Unable to find position with ID " +
                                      std::to_string(positionId) + " for symbol: " + order->getTradingSymbol());
      }
      
      // Find and complete the strategy transaction for this specific unit
      typename StrategyTransactionManager<Decimal>::StrategyTransactionIterator transactionIterator =
        mStrategyTrades.findStrategyTransaction (pos->getPositionID());
      
      if (transactionIterator != mStrategyTrades.endStrategyTransaction()) {
        auto aTransaction = transactionIterator->second;
        auto exitOrder = std::make_shared<T>(*order);
        aTransaction->completeTransaction (exitOrder);
        
        // Clean up tracking for this position - complementary orders were already canceled
        // in the OrderExecuted callback before this method was called
        auto positionOrdersIt = mPositionToOrders.find(positionId);
        if (positionOrdersIt != mPositionToOrders.end()) {
          // Remove all order IDs for this position from unit exit tracking
          for (uint32_t orderIdToCleanup : positionOrdersIt->second) {
            mUnitExitOrders.erase(orderIdToCleanup);
          }
          
          // Clean up the position-to-orders mapping
          mPositionToOrders.erase(positionOrdersIt);
        }
      } else {
        throw StrategyBrokerException("ExitUnitOrderExecutedCommon - Unable to find StrategyTransaction for position ID " +
                                    std::to_string(positionId) + " of symbol: " + order->getTradingSymbol());
      }

      // Close only the specific unit
      mInstrumentPositionManager.closeUnitPosition (order->getTradingSymbol(),
                                                   order->getFillDateTime(),
                                                   order->getFillPrice(),
                                                   unitNumber);
    }

    /**
     * @brief Common logic to handle the execution of an exit order (e.g., sell, cover).
     * This template method checks if the order is tracked as an individual unit exit order.
     * If so, it routes to ExitUnitOrderExecutedCommon. Otherwise, it closes all positions.
     * @tparam T The type of the executed exit order (e.g., MarketOnOpenSellOrder, CoverAtLimitOrder).
     * @param order Pointer to the executed exit order.
     * @throws StrategyBrokerException if the strategy transaction for a closing position cannot be found.
     */
    template <typename T>
    void ExitOrderExecutedCommon (T *order)
    {
      // Check if this order is tracked as an individual unit exit
      auto it = mUnitExitOrders.find(order->getOrderID());
      if (it != mUnitExitOrders.end())
	{
	  // This is an individual unit exit order
	  uint32_t positionId = it->second;

	  mUnitExitOrders.erase(it); // Remove from tracking map
	  ExitUnitOrderExecutedCommon(order, positionId);
	}
      else
	{
	  // This is a full exit order - close all positions (original behavior)
	  InstrumentPosition<Decimal> instrumentPosition =
	    mInstrumentPositionManager.getInstrumentPosition (order->getTradingSymbol());
	  typename InstrumentPosition<Decimal>::ConstInstrumentPositionIterator positionIterator =
	    instrumentPosition.beginInstrumentPosition();
	  typename StrategyTransactionManager<Decimal>::StrategyTransactionIterator transactionIterator;
	  std::shared_ptr<StrategyTransaction<Decimal>> aTransaction;
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
		throw StrategyBrokerException("Unable to find StrategyTransaction for symbol: " + order->getTradingSymbol());
	      }
	  }

        mInstrumentPositionManager.closeAllPositions (order->getTradingSymbol(),
                                                      order->getFillDateTime(), // Use ptime
                                                      order->getFillPrice());
	}
    }

    // ---------------------------
    // Execution tick computation
    // ---------------------------
    struct TickPair { Decimal tick; Decimal tickDiv2; };

    /**
     * @brief Retrieves the security attributes for a given symbol.
     *
     * This is a helper function that looks up the `SecurityAttributes` for a specified
     * trading symbol using the singleton `SecurityAttributesFactory`. This is the
     * central point for accessing static instrument data like base tick size.
     *
     * @param symbol The trading symbol (e.g., "AAPL", "@ES") to look up.
     * @return A shared pointer to the `SecurityAttributes` for the symbol.
     * @throws StrategyBrokerException if the symbol is not found in the factory.
     */
    std::shared_ptr<SecurityAttributes<Decimal>>
    lookupAttrs(const std::string& symbol) const
    {
      auto& fac = SecurityAttributesFactory<Decimal>::instance();
      auto it   = fac.getSecurityAttributes(symbol);
      if (it == fac.endSecurityAttributes())
        throw StrategyBrokerException("No SecurityAttributes for symbol: " + symbol);
      return it->second;
    }

    /**
     * @brief Computes the dynamic execution tick size for a given order context.
     *
     * This method determines the correct minimum price increment (tick) for a trade
     * at a specific point in time and at a specific reference price. It starts with the
     * security's baseline tick and then, only for equities, applies historical
     * fractional pricing rules (`FractionPolicy`) and sub-penny pricing rules
     * (`SubPennyPolicy`). This ensures that backtests use tick sizes that were
     * historically accurate, which is crucial for realistic fill simulation.
     *
     * @param symbol The trading symbol of the instrument.
     * @param when The `ptime` (timestamp) of the order, used by policies to determine
     * the correct historical pricing regime.
     * @param refPrice The reference price for the calculation, used by the sub-penny
     * policy to decide if the price is below $1.00.
     * @return A `TickPair` struct containing the final, context-aware `tick` and `tickDiv2` values
     * to be used for rounding order prices.
     */
    TickPair computeExecutionTick(const std::string& symbol,
                                  const ptime& when,
                                  const Decimal& refPrice) const
    {
      const auto attrs = lookupAttrs(symbol);
      const Decimal baseTick = attrs->getTick();
      Decimal execTick = baseTick;

      // Policies for fractional pricing and sub-pennies only apply to equities.
      if (attrs->isEquitySecurity())
      {
        // Apply fractional regime (equities only)
        execTick = FractionPolicy<Decimal>::apply(when.date(), attrs, execTick);

        // Apply Rule 612 (equities only); template bool selects split-adjusted semantics
        execTick = SubPennyPolicy<Decimal, PricesAreSplitAdjusted>::apply(refPrice, attrs, execTick);
      }

      TickPair out;
      out.tick = execTick;

      // Optimization: if the tick was not modified by policies, use the pre-calculated value.
      // Otherwise, compute it dynamically.
      if (execTick == baseTick)
      {
          out.tickDiv2 = attrs->getTickDiv2();
      }
      else
      {
          out.tickDiv2 = execTick / DecimalConstants<Decimal>::DecimalTwo;
      }

      return out;
    }

 /**
     * @brief Rounds a raw price to the nearest valid execution tick.
     *
     * This helper function takes a calculated price (e.g., a percentage-based stop-loss)
     * and rounds it to a price that can actually be executed in the market. It does this
     * by first calling `computeExecutionTick` to get the correct dynamic tick for the
     * given context (symbol, time, price) and then uses a numerical utility to perform
     * the rounding.
     *
     * @param symbol The trading symbol.
     * @param when The `ptime` of the order.
     * @param refPrice The reference price for the tick calculation.
     * @param rawPrice The unrounded, calculated price to be adjusted.
     * @return The `rawPrice` rounded to the nearest valid tick.
     */
    Decimal roundToExecutionTick(const std::string& symbol,
                                 const ptime& when,
                                 const Decimal& refPrice,
                                 const Decimal& rawPrice) const
    {
      const auto tp = computeExecutionTick(symbol, when, refPrice);
      return num::Round2Tick(rawPrice, tp.tick, tp.tickDiv2);
    }

  private:
    /**
     * @brief Immediately cancels all pending orders for a specific position except the executing order.
     * This prevents dual execution of complementary orders (e.g., limit + stop for same position).
     * @param positionId The position ID whose complementary orders should be canceled.
     * @param executingOrderId The order ID that is currently executing (don't cancel this one).
     */
    void cancelComplementaryOrdersForPosition(uint32_t positionId, uint32_t executingOrderId)
    {
      auto positionOrdersIt = mPositionToOrders.find(positionId);
      if (positionOrdersIt != mPositionToOrders.end()) {
        for (uint32_t orderIdToCancel : positionOrdersIt->second) {
          if (orderIdToCancel != executingOrderId) { // Don't cancel the order that's executing
            // Find and cancel the order by searching through pending orders
            (void) this->cancelOrderById(orderIdToCancel);
          }
        }
      }
    }

    /**
     * @brief Helper method to find and cancel an order by its ID.
     * Searches through all pending order collections to find the order with the specified ID
     * and calls MarkOrderCanceled() on it.
     * @param orderIdToCancel The ID of the order to cancel.
     * @return true if the order was found and canceled, false otherwise.
     */
    bool cancelOrderById(uint32_t orderIdToCancel)
    {
      // Search through all order types to find the order with this ID
      
      // Check market sell orders
      auto sellIt = mOrderManager.beginMarketSellOrders();
      for (; sellIt != mOrderManager.endMarketSellOrders(); ++sellIt) {
        if ((*sellIt)->getOrderID() == orderIdToCancel && (*sellIt)->isOrderPending()) {
          (*sellIt)->MarkOrderCanceled();
          return true;
        }
      }
      
      // Check market cover orders
      auto coverIt = mOrderManager.beginMarketCoverOrders();
      for (; coverIt != mOrderManager.endMarketCoverOrders(); ++coverIt)
	{
	  if ((*coverIt)->getOrderID() == orderIdToCancel && (*coverIt)->isOrderPending())
	    {
	      (*coverIt)->MarkOrderCanceled();
	      return true;
	    }
	}
      
      // Check limit sell orders
      auto limitSellIt = mOrderManager.beginLimitSellOrders();
      for (; limitSellIt != mOrderManager.endLimitSellOrders(); ++limitSellIt) {
        if ((*limitSellIt)->getOrderID() == orderIdToCancel && (*limitSellIt)->isOrderPending()) {
          (*limitSellIt)->MarkOrderCanceled();
          return true;
        }
      }
      
      // Check limit cover orders
      auto limitCoverIt = mOrderManager.beginLimitCoverOrders();
      for (; limitCoverIt != mOrderManager.endLimitCoverOrders(); ++limitCoverIt) {
        if ((*limitCoverIt)->getOrderID() == orderIdToCancel && (*limitCoverIt)->isOrderPending()) {
          (*limitCoverIt)->MarkOrderCanceled();
          return true;
        }
      }
      
      // Check stop sell orders
      auto stopSellIt = mOrderManager.beginStopSellOrders();
      for (; stopSellIt != mOrderManager.endStopSellOrders(); ++stopSellIt) {
        if ((*stopSellIt)->getOrderID() == orderIdToCancel && (*stopSellIt)->isOrderPending()) {
          (*stopSellIt)->MarkOrderCanceled();
          return true;
        }
      }
      
      // Check stop cover orders
      auto stopCoverIt = mOrderManager.beginStopCoverOrders();
      for (; stopCoverIt != mOrderManager.endStopCoverOrders(); ++stopCoverIt) {
        if ((*stopCoverIt)->getOrderID() == orderIdToCancel && (*stopCoverIt)->isOrderPending()) {
          (*stopCoverIt)->MarkOrderCanceled();
          return true;
        }
      }
      
      // Order not found - it was likely already processed
      return false;
    }

    TradingOrderManager<Decimal>       mOrderManager;
    InstrumentPositionManager<Decimal> mInstrumentPositionManager;
    StrategyTransactionManager<Decimal> mStrategyTrades;
    ClosedPositionHistory<Decimal>     mClosedTradeHistory;
    std::shared_ptr<Portfolio<Decimal>> mPortfolio;
    
    // Map to track individual unit exit orders: OrderID -> PositionID
    std::unordered_map<uint32_t, uint32_t> mUnitExitOrders;
    
    // Reverse mapping to track which orders target each position ID: PositionID -> set of OrderIDs
    std::unordered_map<uint32_t, std::set<uint32_t>> mPositionToOrders;
  };

} // namespace mkc_timeseries
#endif
