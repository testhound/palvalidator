// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TRADING_ORDER_MANAGER_H
#define __TRADING_ORDER_MANAGER_H 1

#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <cstdint>
#include <string>
#include <boost/date_time.hpp>
#include "TradingOrder.h"
#include "Portfolio.h"
#include "InstrumentPositionManager.h"

using std::vector;
using std::shared_ptr;

namespace mkc_timeseries
{
  /**
   * @class ProcessOrderVisitor
   * @brief Implements the Visitor design pattern to apply order execution logic to various types of TradingOrder objects.
   *
   * @tparam Decimal The decimal type used for financial calculations.
   *
   * @details
   * The `ProcessOrderVisitor` is responsible for determining if a given `TradingOrder`
   * should be executed based on the market conditions of a specific trading bar.
   * It encapsulates the fill logic for different order types (market, limit, stop).
   * An instance of this visitor is typically created with the OHLC data of the current
   * bar being processed in a backtest. The `TradingOrderManager` then uses this visitor
   * to iterate over pending orders, dispatching each order to the appropriate `visit` method.
   *
   * Key Responsibilities:
   * - Encapsulate order execution logic for different `TradingOrder` subtypes.
   * - Evaluate fill conditions using the OHLC data of a specific trading bar.
   * - Mark orders as executed by calling `TradingOrder::MarkOrderExecuted()` with the fill date and price if conditions are met.
   * - Perform validation checks on orders before attempting to process them.
   *
   * Collaboration:
   * - Used by `TradingOrderManager::processPendingOrders()` to attempt to fill pending orders.
   * - Operates on `TradingOrder` objects and their derived classes.
   * - Requires an `OHLCTimeSeriesEntry` (trading bar data) to make execution decisions.
   */
  template <class Decimal> class ProcessOrderVisitor : public TradingOrderVisitor<Decimal>
  {
  public:
    /**
     * @brief Constructs a ProcessOrderVisitor with a specific trading bar.
     * @param tradingBar The OHLC data for the current trading bar against which orders will be evaluated.
     */
    ProcessOrderVisitor(OHLCTimeSeriesEntry<Decimal> tradingBar)
      : mTradingBar (tradingBar)
    {}

    /**
     * @brief Destructor.
     */
    ~ProcessOrderVisitor()
    {}

    /**
     * @brief Copy constructor.
     * @param rhs The ProcessOrderVisitor to copy.
     */
    ProcessOrderVisitor (const ProcessOrderVisitor<Decimal>& rhs)
      : mTradingBar(rhs.mTradingBar)
    {}

     /**
     * @brief Assignment operator.
     * @param rhs The ProcessOrderVisitor to assign from.
     * @return A reference to this visitor.
     */
    ProcessOrderVisitor<Decimal>& 
    operator=(const ProcessOrderVisitor<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mTradingBar = rhs.mTradingBar;
      return *this;
    }

    /**
     * @brief Processes a MarketOnOpenLongOrder.
     * Market-on-open orders are typically filled at the opening price of the current bar.
     * @param order Pointer to the MarketOnOpenLongOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (MarketOnOpenLongOrder<Decimal> *order)
    {
      ValidateOrder (order);
      // Market orders are unconditional

      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    /**
     * @brief Processes a MarketOnOpenSellOrder.
     * Market-on-open orders are typically filled at the opening price of the current bar.
     * @param order Pointer to the MarketOnOpenSellOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (MarketOnOpenSellOrder<Decimal> *order)
    {
      ValidateOrder (order);
      // Market orders are unconditional

      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    /**
     * @brief Processes a MarketOnOpenCoverOrder.
     * Market-on-open orders are typically filled at the opening price of the current bar.
     * @param order Pointer to the MarketOnOpenCoverOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (MarketOnOpenCoverOrder<Decimal> *order)
    {
      ValidateOrder (order);
      // Market orders are unconditional

      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    /**
     * @brief Processes a MarketOnOpenShortOrder.
     * Market-on-open orders are typically filled at the opening price of the current bar.
     * @param order Pointer to the MarketOnOpenShortOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (MarketOnOpenShortOrder<Decimal> *order)
    {
      ValidateOrder (order);

      // Market orders are unconditional
      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    /**
     * @brief Processes a SellAtLimitOrder.
     * A sell limit order executes if the market price trades at or above the limit price.
     * Fill occurs at the limit price or the open if gapping above the limit.
     * @param order Pointer to the SellAtLimitOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (SellAtLimitOrder<Decimal> *order)
    {
      ValidateOrder (order);
      if (mTradingBar.getHighValue() > order->getLimitPrice())
	{
	  // If we gapped up we assume we get the open price
	  if (mTradingBar.getOpenValue() > order->getLimitPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getLimitPrice());
	}
    }

    /**
     * @brief Processes a CoverAtLimitOrder (buy to cover a short position).
     * A cover limit order executes if the market price trades at or below the limit price.
     * Fill occurs at the limit price or the open if gapping below the limit.
     * @param order Pointer to the CoverAtLimitOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (CoverAtLimitOrder<Decimal> *order)
    {
      ValidateOrder (order);

      if (mTradingBar.getLowValue() < order->getLimitPrice())	{
	  // If we gapped down we assume we get the open price
	  if (mTradingBar.getOpenValue() < order->getLimitPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getLimitPrice());
	}
    }

    /**
     * @brief Processes a CoverAtStopOrder (buy stop to cover a short position).
     * A cover stop order executes if the market price trades at or above the stop price.
     * Fill occurs at the stop price or the open if gapping above the stop.
     * @param order Pointer to the CoverAtStopOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (CoverAtStopOrder<Decimal> *order)
    {
      ValidateOrder (order);
      if (mTradingBar.getHighValue() > order->getStopPrice())
	{
	  // If we gapped up we assume we get the open price
	  if (mTradingBar.getOpenValue() > order->getStopPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getStopPrice());
	}
    }

    /**
     * @brief Processes a SellAtStopOrder.
     * A sell stop order executes if the market price trades at or below the stop price.
     * Fill occurs at the stop price or the open if gapping below the stop.
     * @param order Pointer to the SellAtStopOrder to process.
     * @throws TradingOrderException if order validation fails.
     */
    void visit (SellAtStopOrder<Decimal> *order)
    {
      ValidateOrder (order);
      if (mTradingBar.getLowValue() < order->getStopPrice())
	{
	  // If we gapped down we assume we get the open price
	  if (mTradingBar.getOpenValue() < order->getStopPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getStopPrice());
	}
    }

    /**
     * @brief Updates the trading bar used by the visitor.
     * Allows reusing the visitor instance for multiple bars if desired, though typically a new one is created per bar.
     * @param tradingBar The new OHLCTimeSeriesEntry for subsequent order processing.
     */
    void updateTradingBar (OHLCTimeSeriesEntry<Decimal> tradingBar)
    {
      mTradingBar = tradingBar;
    }

  private:
    /**
     * @brief Validates a trading order before processing.
     * Checks if the bar date is after the order date and if the order is in a pending state.
     * @param order Pointer to the TradingOrder to validate.
     * @throws TradingOrderException if validation fails (e.g., bar date not after order date, order not pending).
     */
    void ValidateOrder (TradingOrder<Decimal>* order)
    {
      if (mTradingBar.getDateValue() <= order->getOrderDate())
	throw TradingOrderException ("Bar date " +to_simple_string (mTradingBar.getDateValue()) +" must be greater than order date " +to_simple_string (order->getOrderDate()));

      if (!order->isOrderPending())
	{
	  if (order->isOrderExecuted())
	    throw TradingOrderException ("ProcessOrderVisitor: Executed order cannot be processed");
	  else if (order->isOrderCanceled())
	    throw TradingOrderException ("ProcessOrderVisitor: Canceled order cannot be processed");
	  else
	    throw TradingOrderException ("ProcessOrderVisitor: unknow order state");
	}
    }

  private:
    OHLCTimeSeriesEntry<Decimal> mTradingBar;
  };

  /**
   * @class TradingOrderManager
   * @brief Manages the lifecycle of trading orders, including submission, processing, execution, and cancellation.
   *
   * @tparam Decimal The decimal type used for financial calculations.
   *
   * @details
   * The `TradingOrderManager` is a central component in a trading system or backtester, responsible for handling
   * all trading orders. It maintains collections of pending orders, organized by type (market, limit, stop).
   * When processing orders for a new trading bar, it uses a `ProcessOrderVisitor` to apply the
   * execution logic specific to each order type against the current market data.
   * It notifies registered observers (typically `StrategyBroker`) of order status changes (execution or cancellation).
   *
   * Key Responsibilities:
   * - Store and manage collections of pending `TradingOrder` objects.
   * - Provide an interface for adding new trading orders of various types.
   * - Process pending orders on each trading bar:
   * - Fetch relevant market data for each order's security.
   * - Use `ProcessOrderVisitor` to determine if an order should be filled.
   * - Handle potential cancellation of orders (e.g., if an exit order's position is already flat due to another order).
   * - Notify registered `TradingOrderObserver`s of order execution or cancellation events.
   * - Maintain an aggregated list of all pending orders, sortable by date, for client inspection.
   *
   * In a Backtesting Context:
   * - The `StrategyBroker` submits `TradingOrder`s to the `TradingOrderManager`.
   * - During the backtest loop, `StrategyBroker` calls `processPendingOrders()` on the `TradingOrderManager` for each new bar.
   * - The `TradingOrderManager` then attempts to fill orders based on that bar's data.
   * - Successful fills or cancellations trigger notifications back to the `StrategyBroker`, which then updates
   * `InstrumentPositionManager`, `StrategyTransactionManager`, etc.
   *
   * Collaboration:
   * - Receives `TradingOrder` objects, typically from `StrategyBroker`.
   * - Uses `Portfolio` to fetch `Security` data (including OHLC bars).
   * - Employs `ProcessOrderVisitor` to encapsulate order execution logic.
   * - Interacts with `InstrumentPositionManager` (passed during `processPendingOrders`) to check current position states,
   * for instance, to cancel exit orders if a position is already flat.
   * - Notifies `TradingOrderObserver`s (e.g., `StrategyBroker`) about order events.
   */
 template <class Decimal> class TradingOrderManager
  {
  public:
    typedef typename vector<shared_ptr<MarketOnOpenSellOrder<Decimal>>>::const_iterator MarketSellOrderIterator;
    typedef typename vector<shared_ptr<MarketOnOpenCoverOrder<Decimal>>>::const_iterator MarketCoverOrderIterator;
    typedef typename vector<shared_ptr<MarketOnOpenLongOrder<Decimal>>>::const_iterator MarketLongOrderIterator;
    typedef typename vector<shared_ptr<MarketOnOpenShortOrder<Decimal>>>::const_iterator MarketShortOrderIterator;

    typedef typename vector<shared_ptr<SellAtLimitOrder<Decimal>>>::const_iterator LimitSellOrderIterator;
    typedef typename vector<shared_ptr<CoverAtLimitOrder<Decimal>>>::const_iterator LimitCoverOrderIterator;

    typedef typename vector<shared_ptr<SellAtStopOrder<Decimal>>>::const_iterator StopSellOrderIterator;
    typedef typename vector<shared_ptr<CoverAtStopOrder<Decimal>>>::const_iterator StopCoverOrderIterator;

   
     typedef typename std::list<std::reference_wrapper<TradingOrderObserver<Decimal>>>::const_iterator ConstObserverIterator;
    typedef typename  std::multimap<boost::gregorian::date, std::shared_ptr<TradingOrder<Decimal>>>::const_iterator PendingOrderIterator;

  public:
    /**
     * @brief Constructs a TradingOrderManager.
     * @param portfolio A shared pointer to the Portfolio, used to access security data for order processing.
     */
    explicit TradingOrderManager(std::shared_ptr<Portfolio<Decimal>> portfolio)
      : mPortfolio(portfolio),
	mMarketSellOrders(),
	mMarketCoverOrders(),
	mMarketLongOrders(),
	mMarketShortOrders(),
	mLimitSellOrders(),
	mLimitCoverOrders(),
	mStopSellOrders(),
	mStopCoverOrders(),
	mObservers(),
	mPendingOrders(),
	mPendingOrdersUpToDate(false)
      {}

    /**
     * @brief Copy constructor.
     * @param rhs The TradingOrderManager to copy.
     */
    TradingOrderManager (const TradingOrderManager<Decimal>& rhs)
      :  mPortfolio(rhs.mPortfolio),
	 mMarketSellOrders(rhs.mMarketSellOrders),
	 mMarketCoverOrders(rhs.mMarketCoverOrders),
	 mMarketLongOrders(rhs.mMarketLongOrders),
	 mMarketShortOrders(rhs.mMarketShortOrders),
	 mLimitSellOrders(rhs.mLimitSellOrders),
	 mLimitCoverOrders(rhs.mLimitCoverOrders),
	 mStopSellOrders(rhs.mStopSellOrders),
	 mStopCoverOrders(rhs.mStopCoverOrders),
	 mObservers(rhs.mObservers),
	 mPendingOrders(rhs.mPendingOrders),
	 mPendingOrdersUpToDate(rhs.mPendingOrdersUpToDate)
    {}

    /**
     * @brief Destructor.
     */
    ~TradingOrderManager()
      {}

    /**
     * @brief Assignment operator.
     * @param rhs The TradingOrderManager to assign from.
     * @return A reference to this manager.
     */
    TradingOrderManager<Decimal>& 
    operator=(const TradingOrderManager<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mPortfolio = rhs.mPortfolio;

      mMarketSellOrders = rhs.mMarketSellOrders;
      mMarketCoverOrders = rhs.mMarketCoverOrders;
      mMarketLongOrders = rhs.mMarketLongOrders;
      mMarketShortOrders = rhs.mMarketShortOrders;
      mLimitSellOrders = rhs.mLimitSellOrders;
      mLimitCoverOrders = rhs.mLimitCoverOrders;
      mStopSellOrders = rhs.mStopSellOrders;
      mStopCoverOrders = rhs.mStopCoverOrders;
      mObservers = rhs.mObservers;
      mPendingOrders = rhs.mPendingOrders;
      mPendingOrdersUpToDate = rhs.mPendingOrdersUpToDate;
      return *this;
    }

    /**
     * @brief Adds a MarketOnOpenCoverOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<MarketOnOpenCoverOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketCoverOrders.push_back (order);
    }

     /**
     * @brief Adds a MarketOnOpenSellOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<MarketOnOpenSellOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketSellOrders.push_back (order);
    }

    /**
     * @brief Adds a MarketOnOpenLongOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<MarketOnOpenLongOrder<Decimal>>& order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketLongOrders.push_back (order);
    }

    /**
     * @brief Adds a MarketOnOpenShortOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<MarketOnOpenShortOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketShortOrders.push_back (order);
    }

    /**
     * @brief Adds a SellAtLimitOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<SellAtLimitOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mLimitSellOrders.push_back (order);
    }

    /**
     * @brief Adds a CoverAtLimitOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<CoverAtLimitOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mLimitCoverOrders.push_back (order);
    }

    /**
     * @brief Adds a SellAtStopOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<SellAtStopOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mStopSellOrders.push_back (order);
    }

     /**
     * @brief Adds a CoverAtStopOrder to the pending orders.
     * @param order A shared pointer to the order.
     * @throws TradingOrderManagerException if the order is not in a valid state to be added.
     */
    void addTradingOrder (std::shared_ptr<CoverAtStopOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mStopCoverOrders.push_back (order);
    }

    /**
     * @brief Returns a constant iterator to the beginning of all pending orders, sorted by order date.
     * The list is populated/updated on demand if it's marked as not up-to-date.
     * @return A PendingOrderIterator.
     */
    PendingOrderIterator beginPendingOrders() const
    {
      if (mPendingOrdersUpToDate == false)
	populatePendingOrders();

      return mPendingOrders.begin();
    }

    /**
     * @brief Returns a constant iterator to the end of all pending orders.
     * The list is populated/updated on demand if it's marked as not up-to-date.
     * @return A PendingOrderIterator.
     */
    PendingOrderIterator endPendingOrders() const
    {
       if (mPendingOrdersUpToDate == false)
	populatePendingOrders();

      return mPendingOrders.end();
    }

    /** @brief Iterator to the beginning of pending MarketOnOpenLongOrders. */
    MarketLongOrderIterator beginMarketLongOrders() const
    {
      return mMarketLongOrders.begin();
    }

    /** @brief Iterator to the end of pending MarketOnOpenLongOrders. */
    MarketLongOrderIterator endMarketLongOrders() const
    {
      return mMarketLongOrders.end();
    }

    /** @brief Iterator to the beginning of pending MarketOnOpenShortOrders. */
    MarketShortOrderIterator beginMarketShortOrders() const
    {
      return mMarketShortOrders.begin();
    }

    /** @brief Iterator to the end of pending MarketOnOpenShortOrders. */
    MarketShortOrderIterator endMarketShortOrders() const
    {
      return mMarketShortOrders.end();
    }

    /** @brief Iterator to the beginning of pending MarketOnOpenSellOrders. */
    MarketSellOrderIterator beginMarketSellOrders() const
    {
      return mMarketSellOrders.begin();
    }

    /** @brief Iterator to the end of pending MarketOnOpenSellOrders. */
    MarketSellOrderIterator endMarketSellOrders() const
    {
      return mMarketSellOrders.end();
    }

    /** @brief Iterator to the beginning of pending MarketOnOpenCoverOrders. */
    MarketCoverOrderIterator beginMarketCoverOrders() const
    {
      return mMarketCoverOrders.begin();
    }
    
    /** @brief Iterator to the end of pending MarketOnOpenCoverOrder. */
    MarketCoverOrderIterator endMarketCoverOrders() const
    {
      return mMarketCoverOrders.end();
    }

    /** @brief Iterator to the beginning of pending SellAtLimitOrder. */
    LimitSellOrderIterator beginLimitSellOrders() const
    {
      return mLimitSellOrders.begin();
    }

    /** @brief Iterator to the end of pending SellAtLimitOrder. */
    LimitSellOrderIterator endLimitSellOrders() const
    {
      return mLimitSellOrders.end();
    }

    /** @brief Iterator to the beginning of pending CoverAtLimitOrder. */
    LimitCoverOrderIterator beginLimitCoverOrders() const
    {
      return mLimitCoverOrders.begin();
    }

    /** @brief Iterator to the end of pending CoverAtLimitOrder. */
    LimitCoverOrderIterator endLimitCoverOrders() const
    {
      return mLimitCoverOrders.end();
    }

    //
    // Stop exit order iterator methods
    //

    StopSellOrderIterator beginStopSellOrders() const
    {
      return mStopSellOrders.begin();
    }

    StopSellOrderIterator endStopSellOrders() const
    {
      return mStopSellOrders.end();
    }

    StopCoverOrderIterator beginStopCoverOrders() const
    {
      return mStopCoverOrders.begin();
    }

    StopCoverOrderIterator endStopCoverOrders() const
    {
      return mStopCoverOrders.end();
    }

    /** @brief Gets the total number of pending market exit orders (sell or cover). */
    uint32_t getNumMarketExitOrders() const
    {
      return mMarketSellOrders.size() + mMarketCoverOrders.size();
    }

     /** @brief Gets the total number of pending market entry orders (long or short). */
    uint32_t getNumMarketEntryOrders() const
    {
      return mMarketLongOrders.size() + mMarketShortOrders.size();
    }

    /** @brief Gets the total number of pending limit exit orders. */
    uint32_t getNumLimitExitOrders() const
    {
      return mLimitSellOrders.size() + mLimitCoverOrders.size();
    }

    /** @brief Gets the total number of pending stop exit orders. */
    uint32_t getNumStopExitOrders() const
    {
      return mStopSellOrders.size() + mStopCoverOrders.size();
    }

    /**
     * @brief Adds an observer to be notified of order events (execution, cancellation).
     * Typically, a `StrategyBroker` instance would be an observer.
     * @param observer A reference wrapper to the TradingOrderObserver.
     */
    void addObserver (std::reference_wrapper<TradingOrderObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

    /**
     * @brief Processes all pending orders for a given date using the current market conditions.
     * This is a key method in a backtesting loop. It iterates through different types of orders
     * (market exits, market entries, stop exits, limit exits in that sequence).
     * For each relevant order, it fetches the security's bar data for `processingDate`,
     * then uses a `ProcessOrderVisitor` to attempt to fill the order.
     * Executed or canceled orders are removed from their pending lists, and observers are notified.
     *
     * @param processingDate The current date in the backtest for which orders are being processed.
     * @param positions A const reference to the InstrumentPositionManager, used to check current
     * position status (e.g., to cancel an exit order if the position is already flat).
     */
    void processPendingOrders (const boost::gregorian::date& processingDate,
			       const InstrumentPositionManager<Decimal>& positions)
    {
      ProcessPendingMarketExitOrders(processingDate, positions);
      ProcessPendingMarketEntryOrders(processingDate, positions);
      ProcessPendingStopExitOrders(processingDate, positions);
      ProcessPendingLimitExitOrders(processingDate, positions);

      // Since we have processed pending orders, our pending order map is no longer
      // up to date

      mPendingOrdersUpToDate = false;
      // NOTE: When closing a position compare number of shares/contracts in order
      // with number of shares/contracts in position in case position will remain open
    }

  private:
    /**
     * @brief Template helper method to process a vector of a specific order type.
     * Iterates through the given `vectorContainer` of orders. For each order:
     * - Checks if it's pending and its order date is before the `processingDate`.
     * - Fetches the security's trading bar for the `processingDate`.
     * - If the security traded on that date:
     * - Checks if an exit order's position is already flat (due to another fill) and cancels if so.
     * - Otherwise, uses `ProcessOrderVisitor` to attempt execution.
     * - Notifies observers of execution or cancellation.
     * - Removes the processed order from `vectorContainer`.
     * @tparam T The specific TradingOrder derived type (e.g., MarketOnOpenLongOrder).
     * @param processingDate The date for which orders are processed.
     * @param vectorContainer A reference to the vector holding orders of type T.
     * @param positions Const reference to InstrumentPositionManager for position status checks.
     */
    template <typename T>
    void ProcessingPendingOrders(const boost::gregorian::date& processingDate, 
				 std::vector<std::shared_ptr<T>>& vectorContainer,
				 const InstrumentPositionManager<Decimal>& positions)
    {
      typedef typename std::shared_ptr<T> OrderPtr;

      typename std::vector<std::shared_ptr<T>>::const_iterator it = vectorContainer.begin();
      OrderPtr order;
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIt;
      std::shared_ptr<Security<Decimal>> aSecurity;
      typename Security<Decimal>::ConstRandomAccessIterator timeSeriesEntryIt;

      for (; it != vectorContainer.end();)
	{
	  order = (*it);
	  
	  if (order->isOrderPending() && (processingDate > order->getOrderDate()))
	    {
	      symbolIt = mPortfolio->findSecurity (order->getTradingSymbol());
	      if (symbolIt != mPortfolio->endPortfolio())
		{
		  aSecurity = symbolIt->second;
		  // Make sure security trades on the processingDate. It's possible due to holidy or
		  // non-trading in certain futures market that there is no market data on the processing date

		  timeSeriesEntryIt = aSecurity->findTimeSeriesEntry (processingDate) ;
		  if (timeSeriesEntryIt != aSecurity->getRandomAccessIteratorEnd())
		    {
		      // Check to see if another order has already closed the position.
		      // This could happen if a stop order was executed on the same day as
		      // a limit order. 
		      if (order->isExitOrder() && 
			  (positions.isFlatPosition (order->getTradingSymbol()) == true))
			{
			  order->MarkOrderCanceled();
			  NotifyOrderCanceled (order);
			}
		      else
			{
			  ProcessOrderVisitor<Decimal> orderProcessor (*timeSeriesEntryIt);
			  orderProcessor.visit (order.get());

			  if (order->isOrderExecuted())
			    NotifyOrderExecuted (order);
			  else
			    {
			      // Note if a order has data for a trading day and the order is not executed
			      // we cancel it. The Strategy will need to resubmit the order again.
			      // Note market orders are always executed so there is not problem with them.

			      order->MarkOrderCanceled();
			      NotifyOrderCanceled (order);
			    }
			}

		      // Remove order from pending list
		      // Note that this returns the next element in the list after erasing

		      it = vectorContainer.erase (it);
		    }
		  else
		    {
		      ++it;
		    }
		}
	    }
	}
    }

     /** @brief Processes pending MarketOnOpenSellOrders and MarketOnOpenCoverOrders. */
    void ProcessPendingMarketExitOrders(const boost::gregorian::date& processingDate,
					const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<MarketOnOpenSellOrder<Decimal>> (processingDate, mMarketSellOrders, 
								  positions);
      this->ProcessingPendingOrders<MarketOnOpenCoverOrder<Decimal>> (processingDate, mMarketCoverOrders, 
								   positions);
    }

     /** @brief Processes pending MarketOnOpenLongOrders and MarketOnOpenShortOrders. */
    void ProcessPendingMarketEntryOrders(const boost::gregorian::date& processingDate,
					 const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<MarketOnOpenLongOrder<Decimal>> (processingDate, mMarketLongOrders,
								  positions);
      this->ProcessingPendingOrders<MarketOnOpenShortOrder<Decimal>> (processingDate, mMarketShortOrders,
								   positions);
    }

    /** @brief Processes pending SellAtStopOrders and CoverAtStopOrders. */
    void ProcessPendingStopExitOrders(const boost::gregorian::date& processingDate,
				      const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<SellAtStopOrder<Decimal>> (processingDate, mStopSellOrders,
							    positions);
      this->ProcessingPendingOrders<CoverAtStopOrder<Decimal>> (processingDate, mStopCoverOrders,
							     positions);
    }

    /** @brief Processes pending SellAtLimitOrder and CoverAtLimitOrder. */
    void ProcessPendingLimitExitOrders(const boost::gregorian::date& processingDate,
				       const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<SellAtLimitOrder<Decimal>> (processingDate, mLimitSellOrders,
							     positions);
      this->ProcessingPendingOrders<CoverAtLimitOrder<Decimal>> (processingDate, mLimitCoverOrders,
							      positions);
    }

    /**
     * @brief Notifies all registered observers that an order has been executed.
     * @tparam T The specific TradingOrder derived type.
     * @param order A shared pointer to the executed order.
     */
    template <typename T>
    void NotifyOrderExecuted (std::shared_ptr<T> order)
    {
      ConstObserverIterator it = beginObserverList();
      for (; it != endObserverList(); it++)
	(*it).get().OrderExecuted (order.get());
    }

    /**
     * @brief Notifies all registered observers that an order has been canceled.
     * @tparam T The specific TradingOrder derived type.
     * @param order A shared pointer to the canceled order.
     */
    template <typename T>
    void NotifyOrderCanceled (std::shared_ptr<T> order)
    {
      ConstObserverIterator it = beginObserverList();
      for (; it != endObserverList(); it++)
	(*it).get().OrderCanceled (order.get());
    }

     /**
     * @brief Validates a new order before adding it to the manager.
     * Ensures the order is not already executed or canceled.
     * @param order A const shared_ptr to the TradingOrder to validate.
     * @throws TradingOrderManagerException if the order is already executed or canceled.
     */
    void ValidateNewOrder (const std::shared_ptr<TradingOrder<Decimal>>& order) const
    {
      if (order->isOrderExecuted())
	throw TradingOrderManagerException ("Attempt to add executed trading order");

      if (order->isOrderCanceled())
	throw TradingOrderManagerException ("Attempt to add canceled trading order");
    }

    /** @brief Returns an iterator to the beginning of the observer list. */
    ConstObserverIterator beginObserverList() const
    {
      return mObservers.begin();
    }

    /** @brief Returns an iterator to the end of the observer list. */
    ConstObserverIterator endObserverList() const
    {
      return mObservers.end();
    }

     /**
     * @brief Populates the `mPendingOrders` multimap with all orders from the specific type vectors.
     * This method is called lazily when `beginPendingOrders` or `endPendingOrders` is accessed
     * and the `mPendingOrdersUpToDate` flag is false.
     * The `mPendingOrders` map stores orders sorted by their `getOrderDate()`.
     * This method is marked `mutable` as it modifies `mPendingOrders` and `mPendingOrdersUpToDate`
     * but is logically const from the perspective of the manager's observable state of pending orders.
     */
    void populatePendingOrders() const
    {
      mPendingOrders.clear();

      MarketLongOrderIterator longMarketIt = beginMarketLongOrders();
      for (; longMarketIt != endMarketLongOrders(); longMarketIt++)
	  addOrderToPending (*longMarketIt);
	
      MarketShortOrderIterator shortMarketIt = beginMarketShortOrders();
      for (; shortMarketIt != endMarketShortOrders(); shortMarketIt++)
	  addOrderToPending (*shortMarketIt);

      MarketSellOrderIterator sellMarketIt = beginMarketSellOrders();
      for (; sellMarketIt != endMarketSellOrders(); sellMarketIt++)
	  addOrderToPending (*sellMarketIt);

      MarketCoverOrderIterator coverMarketIt = beginMarketCoverOrders();
      for (; coverMarketIt != endMarketCoverOrders(); coverMarketIt++)
	  addOrderToPending (*coverMarketIt);

      StopSellOrderIterator stopSellIt = beginStopSellOrders();
      for (; stopSellIt != endStopSellOrders(); stopSellIt++)
	  addOrderToPending (*stopSellIt);

      StopCoverOrderIterator stopCoverIt = beginStopCoverOrders();
      for (; stopCoverIt != endStopCoverOrders(); stopCoverIt++)
	  addOrderToPending (*stopCoverIt);

      LimitSellOrderIterator sellLimitIt = beginLimitSellOrders();
      for (; sellLimitIt != endLimitSellOrders(); sellLimitIt++)
	  addOrderToPending (*sellLimitIt);

      LimitCoverOrderIterator coverLimitIt = beginLimitCoverOrders();
      for (; coverLimitIt != endLimitCoverOrders(); coverLimitIt++)
	  addOrderToPending (*coverLimitIt);

     

      mPendingOrdersUpToDate = true;
    }

     /**
     * @brief Helper method to add a single order to the `mPendingOrders` multimap.
     * This was part of the original `populatePendingOrders` logic, refactored for clarity if needed elsewhere,
     * though `populatePendingOrders` now uses a lambda.
     * @param aOrder A shared pointer to the TradingOrder to add.
     * @note Marked const as it's called by `populatePendingOrders` which is const. Modifies mutable members.
     */
    void addOrderToPending(std::shared_ptr<TradingOrder<Decimal>> aOrder) const
    {
      mPendingOrders.insert (std::make_pair (aOrder->getOrderDate(), aOrder));
    }

  private:
    std::shared_ptr<Portfolio<Decimal>> mPortfolio;
    std::vector<std::shared_ptr<MarketOnOpenSellOrder<Decimal>>> mMarketSellOrders;
    std::vector<std::shared_ptr<MarketOnOpenCoverOrder<Decimal>>> mMarketCoverOrders;
    std::vector<std::shared_ptr<MarketOnOpenLongOrder<Decimal>>> mMarketLongOrders;
    std::vector<std::shared_ptr<MarketOnOpenShortOrder<Decimal>>> mMarketShortOrders;
    std::vector<std::shared_ptr<SellAtLimitOrder<Decimal>>> mLimitSellOrders;
    std::vector<std::shared_ptr<CoverAtLimitOrder<Decimal>>> mLimitCoverOrders;
    std::vector<std::shared_ptr<SellAtStopOrder<Decimal>>> mStopSellOrders;
    std::vector<std::shared_ptr<CoverAtStopOrder<Decimal>>> mStopCoverOrders;
    std::list<std::reference_wrapper<TradingOrderObserver<Decimal>>> mObservers;

    // A temporary map to iterate over pending order if a client asks for them
    // The map is cleared before iterating and populate from the above vectors
    mutable std::multimap<boost::gregorian::date, std::shared_ptr<TradingOrder<Decimal>>> mPendingOrders;
    mutable bool mPendingOrdersUpToDate;
 };
}

#endif


