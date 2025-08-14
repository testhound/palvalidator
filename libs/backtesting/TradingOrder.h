// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

/**
 * @file TradingOrder.h
 * @brief Defines the trading order classes used for simulating order lifecycle in the backtesting framework.
 *
 * This header implements the class hierarchy and state transitions for all order types: market,
 * limit, and stop orders, both for entry and exit. It provides observer-based notification of order
 * state changes and enables polymorphic visitation of order types using the Visitor pattern.
 */
#ifndef __TRADING_ORDER_H
#define __TRADING_ORDER_H 1

#include <exception>
#include <memory>
#include <list>
#include <cstdint>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TradingOrderException.h"
#include "TimeSeriesEntry.h"
#include "DecimalConstants.h"
#include <atomic>

using namespace boost::gregorian;

namespace mkc_timeseries
{
  template <class Decimal> class TradingOrderVisitor;

  template <class Decimal> class TradingOrderState;
  template <class Decimal> class MarketOnOpenLongOrder;
  template <class Decimal> class MarketOnOpenShortOrder;
  template <class Decimal> class MarketOnOpenSellOrder;
  template <class Decimal> class MarketOnOpenCoverOrder;
  template <class Decimal> class ExecutedOrderState;
  template <class Decimal> class CanceledOrderState;
  template <class Decimal> class PendingOrderState;
  template <class Decimal> class SellAtLimitOrder;
  template <class Decimal> class CoverAtLimitOrder;
  template <class Decimal> class CoverAtStopOrder;
  template <class Decimal> class SellAtStopOrder;
  template <class Decimal> class TradingOrderObserver;
  template <class Decimal> class TradingOrder;

  /**
   * @class TradingOrderVisitor
   * @brief Visitor interface for processing all order types.
   *
   * Responsibilities:
   * - Provides `visit()` overloads for each supported order type.
   */
  template <class Decimal>
  class TradingOrderVisitor
  {
  public:
    TradingOrderVisitor()
    {}

    virtual ~TradingOrderVisitor()
    {}

    virtual void visit (MarketOnOpenLongOrder<Decimal> *order) = 0;
    virtual void visit (MarketOnOpenShortOrder<Decimal> *order) = 0;

    virtual void visit (MarketOnOpenSellOrder<Decimal> *order) = 0;
    virtual void visit (MarketOnOpenCoverOrder<Decimal> *order) = 0;

    virtual void visit (SellAtLimitOrder<Decimal> *order) = 0;
    virtual void visit (CoverAtLimitOrder<Decimal> *order) = 0;
    virtual void visit (CoverAtStopOrder<Decimal> *order) = 0;
    virtual void visit (SellAtStopOrder<Decimal> *order) = 0;
  };

  /**
   * @class TradingOrderObserver
   * @brief Interface for observing state transitions of trading orders.
   *
   * Responsibilities:
   * - Notified upon order execution or cancellation.
   *
   * Collaboration:
   * - StrategyBroker implements this interface to track fills.
   */
  template <class Decimal>
  class TradingOrderObserver
  {
  public:
    TradingOrderObserver()
    {}

    virtual ~TradingOrderObserver()
    {}

    virtual void OrderExecuted (MarketOnOpenLongOrder<Decimal> *order) = 0;
    virtual void OrderExecuted (MarketOnOpenShortOrder<Decimal> *order) = 0;
    virtual void OrderExecuted (MarketOnOpenSellOrder<Decimal> *order) = 0;
    virtual void OrderExecuted (MarketOnOpenCoverOrder<Decimal> *order) = 0;
    virtual void OrderExecuted (SellAtLimitOrder<Decimal> *order) = 0;
    virtual void OrderExecuted (CoverAtLimitOrder<Decimal> *order) = 0;
    virtual void OrderExecuted (CoverAtStopOrder<Decimal> *order) = 0;
    virtual void OrderExecuted (SellAtStopOrder<Decimal> *order) = 0;

    virtual void OrderCanceled (MarketOnOpenLongOrder<Decimal> *order) = 0;
    virtual void OrderCanceled (MarketOnOpenShortOrder<Decimal> *order) = 0;
    virtual void OrderCanceled (MarketOnOpenSellOrder<Decimal> *order) = 0;
    virtual void OrderCanceled (MarketOnOpenCoverOrder<Decimal> *order) = 0;
    virtual void OrderCanceled (SellAtLimitOrder<Decimal> *order) = 0;
    virtual void OrderCanceled (CoverAtLimitOrder<Decimal> *order) = 0;
    virtual void OrderCanceled (CoverAtStopOrder<Decimal> *order) = 0;
    virtual void OrderCanceled (SellAtStopOrder<Decimal> *order) = 0;
  };
 
 /**
  * @class TradingOrder
  * @brief Abstract base class for all order types.
  *
  * Responsibilities:
  * - Encapsulate shared data like symbol, units, order date, and state.
  * - Implement state transitions (pending → executed or canceled).
  * - Enforce invariant: fill date must be >= order date.
  * - Notify observers of order execution or cancellation.
  *
  * Collaboration:
  * - Observed by TradingOrderObserver (e.g., StrategyBroker).
  * - Transition logic implemented via TradingOrderState and its subclasses.
  * - Order logic extended via subclasses like MarketOrder, LimitOrder, etc.
  *
  * Interface Contract:
  * Subclasses must implement the following core methods:
  *
  * - `void notifyOrderExecuted()`
  *   - Purpose: Invoked after the order has been filled.
  *   - Contract: Must notify all registered observers that this order is now executed.
  *   - Should typically include a loop through observer list and call observer->OrderExecuted(this).
  *
  * - `void notifyOrderCanceled()`
  *   - Purpose: Called when the order is canceled.
  *   - Contract: Must notify all registered observers that this order has been canceled.
  *   - May also trigger cleanup of any associated strategy state.
  *
  * - `void ValidateOrderExecution(const TimeSeriesDate& fillDate, const Decimal& fillPrice) const`
  *   - Purpose: Validates that the provided fill data is consistent with this order’s contract.
  *   - Contract: Must throw an exception if fillDate is before the order date, or if the price violates limit/stop conditions.
  *   - Used to enforce correctness in order processing and simulation integrity.
  */ 
  template <class Decimal>
  class TradingOrder
  {
  public:
    typedef typename std::list<std::shared_ptr<TradingOrderObserver<Decimal>>>::const_iterator ConstObserverIterator;

  public:
    // New ptime-based constructor
    TradingOrder(const std::string& tradingSymbol,
                 const TradingVolume& unitsInOrder,
                 const ptime& orderDateTime)
      : mTradingSymbol(tradingSymbol),
	mUnitsInOrder(unitsInOrder),
	mOrderDateTime(orderDateTime),
	mOrderState(new PendingOrderState<Decimal>()),
	mOrderID(++TradingOrder<Decimal>::mOrderIDCount),
	mObservers()
    {
      if (mUnitsInOrder.getTradingVolume() == 0)
	throw TradingOrderException("TradingOrder constructor - order cannot have zero units for: " + tradingSymbol + " with order datetime: " + boost::posix_time::to_simple_string(orderDateTime));
    }

    // Legacy constructor (date only)
    TradingOrder(const std::string& tradingSymbol,
                 const TradingVolume& unitsInOrder,
                 const boost::gregorian::date& orderDate)
      : TradingOrder<Decimal>(tradingSymbol, unitsInOrder, ptime(orderDate, getDefaultBarTime()))
    {}

    TradingOrder(const TradingOrder<Decimal>& rhs)
      : mTradingSymbol(rhs.mTradingSymbol),
	mUnitsInOrder(rhs.mUnitsInOrder),
	mOrderDateTime(rhs.mOrderDateTime),
	mOrderState(rhs.mOrderState),
	mOrderID(rhs.mOrderID),
	mObservers(rhs.mObservers)
    {}

    TradingOrder<Decimal>& operator=(const TradingOrder<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;
      mTradingSymbol = rhs.mTradingSymbol;
      mUnitsInOrder = rhs.mUnitsInOrder;
      mOrderDateTime = rhs.mOrderDateTime;
      mOrderState = rhs.mOrderState;
      mOrderID = rhs.mOrderID;
      mObservers = rhs.mObservers;
      return *this;
    }

    const std::string& getTradingSymbol() const { return mTradingSymbol; }
    const TradingVolume& getUnitsInOrder() const { return mUnitsInOrder; }

    // New: get order datetime
    const ptime& getOrderDateTime() const { return mOrderDateTime; }
    // Legacy: get only the date portion
    const boost::gregorian::date getOrderDate() const { return mOrderDateTime.date(); }

    uint32_t getOrderID() const { return mOrderID; }

    virtual uint32_t getOrderPriority() const = 0;
    virtual bool isLongOrder() const = 0;
    virtual bool isShortOrder() const = 0;
    virtual bool isEntryOrder() const = 0;
    virtual bool isExitOrder() const = 0;
    virtual bool isMarketOrder() const = 0;
    virtual bool isStopOrder() const = 0;
    virtual bool isLimitOrder() const = 0;

    bool isOrderPending() const { return mOrderState->isOrderPending(); }
    bool isOrderExecuted() const { return mOrderState->isOrderExecuted(); }
    bool isOrderCanceled() const { return mOrderState->isOrderCanceled(); }

    // New: Mark order executed with datetime
    void MarkOrderExecuted(const ptime& fillDateTime, const Decimal& fillPrice)
    {
      ValidateOrderExecution(fillDateTime, fillPrice);
      if (fillDateTime >= getOrderDateTime())
        {
	  mOrderState->MarkOrderExecuted(this, fillDateTime, fillPrice);
	  this->notifyOrderExecuted();
        }
      else
	throw TradingOrderNotExecutedException("Order fill datetime cannot occur before order datetime");
    }

    void MarkOrderExecuted(const boost::gregorian::date& fillDate, const Decimal& fillPrice)
    {
      MarkOrderExecuted(ptime(fillDate, getDefaultBarTime()), fillPrice);
    }

    void MarkOrderCanceled()
    {
      mOrderState->MarkOrderCanceled(this);
      this->notifyOrderCanceled();
    }

    const ptime& getFillDateTime() const { return mOrderState->getFillDateTime(); }

    boost::gregorian::date getFillDate() const { return mOrderState->getFillDateTime().date(); }

    const Decimal& getFillPrice() const { return mOrderState->getFillPrice(); }

    virtual void accept(TradingOrderVisitor<Decimal>& visitor) = 0;

    void addObserver(std::shared_ptr<TradingOrderObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

  protected:
    ConstObserverIterator beginObserverList() const { return mObservers.begin(); }
    ConstObserverIterator endObserverList() const { return mObservers.end(); }

    virtual void notifyOrderExecuted() = 0;
    virtual void notifyOrderCanceled() = 0;

    // New: Validate using datetime
    virtual void ValidateOrderExecution(const ptime& fillDateTime, const Decimal& fillPrice) const = 0;

    // Legacy: Validate using date
    virtual void ValidateOrderExecution(const boost::gregorian::date& fillDate,
					const Decimal& fillPrice) const
    {
      ValidateOrderExecution(ptime(fillDate, getDefaultBarTime()), fillPrice);
    }

  private:
    void ChangeState(std::shared_ptr<TradingOrderState<Decimal>> newState)
    {
      mOrderState = newState;
    }

    friend class TradingOrderState<Decimal>;
    friend class PendingOrderState<Decimal>;

  private:
    std::string mTradingSymbol;
    TradingVolume mUnitsInOrder;
    ptime mOrderDateTime;
    std::shared_ptr<TradingOrderState<Decimal>> mOrderState;
    uint32_t mOrderID;
    static std::atomic<uint32_t> mOrderIDCount;
    std::list<std::shared_ptr<TradingOrderObserver<Decimal>>> mObservers;
  };

  template <class Decimal> std::atomic<uint32_t> TradingOrder<Decimal>::mOrderIDCount{0};

  /**
   * @class MarketOrder
   * @brief Represents an unconditional order to be filled immediately at market price.
   *
   * Responsibilities:
   * - Define execution priority (always highest).
   * - Serves as base class for entry/exit market orders.
   */
  template <class Decimal> class MarketOrder : public TradingOrder<Decimal>
    {
    public:
      // ptime-based constructor
    MarketOrder(const std::string& tradingSymbol,
                const TradingVolume& unitsInOrder,
                const ptime& orderDateTime)
      : TradingOrder<Decimal>(tradingSymbol, unitsInOrder, orderDateTime)
      {}

      MarketOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
	: MarketOrder<Decimal> (tradingSymbol, unitsInOrder, ptime(orderDate, getDefaultBarTime()))
      {}

      virtual ~MarketOrder()
      {}

      MarketOrder (const MarketOrder<Decimal>& rhs)
      : TradingOrder<Decimal> (rhs)
      {}

      MarketOrder<Decimal>& 
      operator=(const MarketOrder<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	TradingOrder<Decimal>::operator=(rhs);
	return *this;
      }

      bool isMarketOrder() const
      {
	return true;
      }

      bool isStopOrder() const
      {
	return false;
      }

      bool isLimitOrder() const
      {
	return false;
      }

      // Market order because they are unconditional have the highest priority
      uint32_t getOrderPriority() const
      {
	return 1;
      }

      // Market orders are always executed
      void ValidateOrderExecution(const ptime& /* fillDateTime */, const Decimal& /* fillPrice */) const
      {}
  };

  /**
   * @class MarketEntryOrder
   * @brief Abstract base for all market entry orders.
   *
   * Responsibilities:
   * - Add stop loss and profit target percent parameters.
   * - Provides base logic for long/short entry orders.
   */

  template <class Decimal> class MarketEntryOrder : public MarketOrder<Decimal>
  {
  public:
    MarketEntryOrder(const std::string& tradingSymbol,
                     const TradingVolume& unitsInOrder,
                     const ptime& orderDate,
                     const Decimal& stopLoss,
                     const Decimal& profitTarget)
      : MarketOrder<Decimal>(tradingSymbol, unitsInOrder, orderDate),
        mStopLoss(stopLoss),
        mProfitTarget(profitTarget)
    {}

    MarketEntryOrder(const std::string& tradingSymbol,
		     const TradingVolume& unitsInOrder,
		     const TimeSeriesDate& orderDate,
		     const Decimal& stopLoss,
		     const Decimal& profitTarget)
      : MarketEntryOrder<Decimal> (tradingSymbol,
				   unitsInOrder,
				   ptime(orderDate, getDefaultBarTime()),
				   stopLoss,
				   profitTarget)
    {}

    virtual ~MarketEntryOrder()
    {}

    MarketEntryOrder (const MarketEntryOrder<Decimal>& rhs)
      : MarketOrder<Decimal> (rhs)
    {
      mStopLoss = rhs.mStopLoss;
      mProfitTarget = rhs.mProfitTarget;
    }

    MarketEntryOrder<Decimal>&
    operator=(const MarketEntryOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      MarketOrder<Decimal>::operator=(rhs);

      mStopLoss = rhs.mStopLoss;
      mProfitTarget = rhs.mProfitTarget;

      return *this;
    }

    bool isEntryOrder() const
    {
      return true;
    } 

    bool isExitOrder() const
    {
      return false;
    }

    const Decimal& getStopLoss() const
    {
      return mStopLoss;
    }

    const Decimal& getProfitTarget() const
    {
      return mProfitTarget;
    }

  private:
    Decimal mStopLoss;       // Stop loss in percent from PAL pattern. This is NOT the stop price
    Decimal mProfitTarget;   // Profit target in percent from PAL pattern. This is NOT the profit target
  };

  /**
   * @class MarketOnOpenLongOrder
   * @brief Long entry order to be executed at market open.
   *
   * Responsibilities:
   * - Implements order identity (long, entry, market).
   * - Executes observer notification on fill or cancel.
   */
  template <class Decimal> class MarketOnOpenLongOrder : public MarketEntryOrder<Decimal>
  {
  public:
    MarketOnOpenLongOrder(const std::string& tradingSymbol,
                         const TradingVolume& unitsInOrder,
                         const ptime& orderDateTime,
                         const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                         const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      : MarketEntryOrder<Decimal>(tradingSymbol, unitsInOrder, orderDateTime, stopLoss, profitTarget)
    {}

    MarketOnOpenLongOrder(const std::string& tradingSymbol,
                         const TradingVolume& unitsInOrder,
                         const date& orderDate,
                         const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                         const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      : MarketOnOpenLongOrder<Decimal>(tradingSymbol,
				       unitsInOrder,
				       ptime(orderDate, getDefaultBarTime()),
				       stopLoss,
				       profitTarget)
    {}

    MarketOnOpenLongOrder (const MarketOnOpenLongOrder<Decimal>& rhs)
      : MarketEntryOrder<Decimal> (rhs)
    {}

    MarketOnOpenLongOrder<Decimal>& 
    operator=(const MarketOnOpenLongOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketEntryOrder<Decimal>::operator=(rhs);
      return *this;
    }

    ~MarketOnOpenLongOrder()
    {}

    bool isLongOrder() const
    {
      return true;
    }

    bool isShortOrder() const
    {
      return false;
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  /**
   * @class MarketOnOpenShortOrder
   * @brief Short entry order to be executed at market open.
   *
   * Responsibilities:
   * - Implements order identity (short, entry, market).
   * - Executes observer notification on fill or cancel.
   */
  template <class Decimal> class MarketOnOpenShortOrder : public MarketEntryOrder<Decimal>
  {
  public:
    MarketOnOpenShortOrder(const std::string& tradingSymbol,
                         const TradingVolume& unitsInOrder,
                         const ptime& orderDateTime,
                         const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                         const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      : MarketEntryOrder<Decimal>(tradingSymbol, unitsInOrder, orderDateTime, stopLoss, profitTarget)
    {}

    MarketOnOpenShortOrder(const std::string& tradingSymbol,
                         const TradingVolume& unitsInOrder,
                         const date& orderDate,
                         const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
                         const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      : MarketOnOpenShortOrder<Decimal>(tradingSymbol,
				       unitsInOrder,
				       ptime(orderDate, getDefaultBarTime()),
				       stopLoss,
				       profitTarget)
    {}

    MarketOnOpenShortOrder (const MarketOnOpenShortOrder<Decimal>& rhs)
      : MarketEntryOrder<Decimal> (rhs)
    {}

    MarketOnOpenShortOrder<Decimal>& 
    operator=(const MarketOnOpenShortOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketEntryOrder<Decimal>::operator=(rhs);
      return *this;
    }

    ~MarketOnOpenShortOrder()
    {}

    bool isLongOrder() const
    {
      return false;
    }

    bool isShortOrder() const
    {
      return true;
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };


  /**
   * @class MarketExitOrder
   * @brief Abstract base for all market exit orders.
   *
   * Responsibilities:
   * - Common functionality for position-closing market orders.
   */

  template <class Decimal> class MarketExitOrder : public MarketOrder<Decimal>
  {
  public:
      MarketExitOrder(const std::string& tradingSymbol,
		      const TradingVolume& unitsInOrder,
		      const ptime& orderDate)
      : MarketOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
      {}

    MarketExitOrder(const std::string& tradingSymbol, 
		      const TradingVolume& unitsInOrder,
		      const date& orderDate)
      : MarketExitOrder<Decimal> (tradingSymbol,
				  unitsInOrder,
				  ptime(orderDate, getDefaultBarTime()))
      {}

      virtual ~MarketExitOrder()
      {}

      MarketExitOrder (const MarketExitOrder<Decimal>& rhs)
      : MarketOrder<Decimal> (rhs)
      {}

      MarketExitOrder<Decimal>& 
      operator=(const MarketExitOrder<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	MarketOrder<Decimal>::operator=(rhs);
	return *this;
      }

      bool isEntryOrder() const
      {
	return false;
      }

      bool isExitOrder() const
      {
	return true;
      }
  };

  /**
   * @class MarketOnOpenSellOrder
   * @brief Closes a long position at market open.
   */

  template <class Decimal> class MarketOnOpenSellOrder : public MarketExitOrder<Decimal>
  {
  public:

    MarketOnOpenSellOrder(const std::string& tradingSymbol,
                          const TradingVolume& unitsInOrder,
                          const ptime& orderDate)
      : MarketExitOrder<Decimal>(tradingSymbol, unitsInOrder, orderDate)
    {}

    MarketOnOpenSellOrder(const std::string& tradingSymbol,
                          const TradingVolume& unitsInOrder,
                          const date& orderDate)
      : MarketOnOpenSellOrder(tradingSymbol, unitsInOrder, ptime(orderDate, getDefaultBarTime()))
    {}

    MarketOnOpenSellOrder (const MarketOnOpenSellOrder<Decimal>& rhs)
      : MarketExitOrder<Decimal> (rhs)
    {}

    MarketOnOpenSellOrder<Decimal>& 
    operator=(const MarketOnOpenSellOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketExitOrder<Decimal>::operator=(rhs);
      return *this;
    }

    ~MarketOnOpenSellOrder()
    {}

    bool isLongOrder() const
    {
      return true;
    }

    bool isShortOrder() const
    {
      return false;
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };


  /**
   * @class MarketOnOpenCoverOrder
   * @brief Closes a short position at market open.
   */

  template <class Decimal> class MarketOnOpenCoverOrder : public MarketExitOrder<Decimal>
  {
  public:
    MarketOnOpenCoverOrder(const std::string& tradingSymbol,
                           const TradingVolume& unitsInOrder,
                           const ptime& orderDateTime)
      : MarketExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime)
    {}

    MarketOnOpenCoverOrder(const std::string& tradingSymbol,
                           const TradingVolume& unitsInOrder,
                           const date& orderDate)
      : MarketOnOpenCoverOrder<Decimal>(tradingSymbol, unitsInOrder, ptime(orderDate, getDefaultBarTime()))
    {}

    MarketOnOpenCoverOrder<Decimal>& 
    operator=(const MarketOnOpenCoverOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketExitOrder<Decimal>::operator=(rhs);
      return *this;
    }

    ~MarketOnOpenCoverOrder()
    {}

    bool isLongOrder() const
    {
      return false;
    }

    bool isShortOrder() const
    {
      return true;
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  /**
   * @class LimitOrder
   * @brief Abstract base class for all limit orders.
   *
   * Responsibilities:
   * - Define the limit price condition.
   */
  template <class Decimal> class LimitOrder : public TradingOrder<Decimal>
    {
    public:
      LimitOrder(const std::string& tradingSymbol,
                 const TradingVolume& unitsInOrder,
                 const ptime& orderDateTime,
                 const Decimal& limitPrice)
        : TradingOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime),
          mLimitPrice(limitPrice)
      {}

      LimitOrder(const std::string& tradingSymbol, 
                 const TradingVolume& unitsInOrder,
                 const date& orderDate,
                 const Decimal& limitPrice)
        : LimitOrder<Decimal> (tradingSymbol,
			       unitsInOrder,
			       ptime(orderDate, getDefaultBarTime()),
			       limitPrice)
      {}

      virtual ~LimitOrder()
      {}

      LimitOrder (const LimitOrder<Decimal>& rhs)
	: TradingOrder<Decimal> (rhs),
	  mLimitPrice(rhs.mLimitPrice)
      {}

      LimitOrder<Decimal>& 
      operator=(const LimitOrder<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	TradingOrder<Decimal>::operator=(rhs);
	mLimitPrice = rhs.mLimitPrice;
	return *this;
      }

      const Decimal& getLimitPrice() const
      {
	return mLimitPrice;
      }

      bool isMarketOrder() const
      {
	return false;
      }

      bool isStopOrder() const
      {
	return false;
      }

      bool isLimitOrder() const
      {
	return true;
      }

    private:
      Decimal mLimitPrice;
  };

  //
  // class LimitExitOrder
  //

  template <class Decimal> class LimitExitOrder : public LimitOrder<Decimal>
    {
    public:
      LimitExitOrder(const std::string& tradingSymbol, 
                     const TradingVolume& unitsInOrder,
                     const ptime& orderDateTime,
                     const Decimal& limitPrice)
        : LimitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime, limitPrice)
      {}

      LimitExitOrder(const std::string& tradingSymbol,
                     const TradingVolume& unitsInOrder,
                     const date& orderDate, // Changed TimeSeriesDate to date
                     const Decimal& limitPrice)
        : LimitExitOrder<Decimal> (tradingSymbol,
				   unitsInOrder,
				   ptime(orderDate, getDefaultBarTime()),
				   limitPrice)
      {}

      virtual ~LimitExitOrder()
      {}

      LimitExitOrder (const LimitExitOrder<Decimal>& rhs)
	: LimitOrder<Decimal> (rhs)
      {}

      LimitExitOrder<Decimal>& 
      operator=(const LimitExitOrder<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	LimitOrder<Decimal>::operator=(rhs);
	return *this;
      }

      bool isEntryOrder() const
      {
	return false;
      }

      bool isExitOrder() const
      {
	return true;
      }

      uint32_t getOrderPriority() const
      {
	return 10;
      }
  };

  /**
   * @class SellAtLimitOrder
   * @brief Closes a long position when price >= limit.
   */
  template <class Decimal> class SellAtLimitOrder : public LimitExitOrder<Decimal>
  {
  public:
    using TradingOrder<Decimal>::ValidateOrderExecution;

    SellAtLimitOrder(const std::string& tradingSymbol,
                     const TradingVolume& unitsInOrder,
                     const ptime& orderDateTime,
                     const Decimal& limitPrice)
      : LimitExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime, limitPrice)
    {}

    SellAtLimitOrder(const std::string& tradingSymbol, 
                     const TradingVolume& unitsInOrder,
                     const date& orderDate,
                     const Decimal& limitPrice)
      : SellAtLimitOrder<Decimal> (tradingSymbol,
				   unitsInOrder,
				   ptime(orderDate, getDefaultBarTime()),
				   limitPrice)
    {}

    ~SellAtLimitOrder()
    {}

    SellAtLimitOrder (const SellAtLimitOrder<Decimal>& rhs)
      : LimitExitOrder<Decimal> (rhs)
    {}

    SellAtLimitOrder<Decimal>& 
    operator=(const SellAtLimitOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      LimitExitOrder<Decimal>::operator=(rhs);
      return *this;
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

     void ValidateOrderExecution(const ptime& /* fillDateTime */, const Decimal& fillPrice) const override
    {
      // Base class TradingOrder::MarkOrderExecuted already checks fillDateTime >= orderDateTime
      if (fillPrice < this->getLimitPrice())
        throw TradingOrderNotExecutedException ("SellAtLimitOrder: fill price cannot be less than limit price");
    }
    

    // True in the sense that it closes a long position
    bool isLongOrder() const
    {
      return true;
    }

    bool isShortOrder() const
    {
      return false;
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };


  /**
   * @class CoverAtLimitOrder
   * @brief Closes a short position when price <= limit.
   */

  template <class Decimal> class CoverAtLimitOrder : public LimitExitOrder<Decimal>
  {
  public:
    using TradingOrder<Decimal>::ValidateOrderExecution;

    CoverAtLimitOrder(const std::string& tradingSymbol, 
                      const TradingVolume& unitsInOrder,
                      const ptime& orderDateTime,
                      const Decimal& limitPrice)
      : LimitExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime, limitPrice)
    {}

    CoverAtLimitOrder(const std::string& tradingSymbol,
                      const TradingVolume& unitsInOrder,
                      const date& orderDate, // Changed TimeSeriesDate to date
                      const Decimal& limitPrice)
      : CoverAtLimitOrder<Decimal> (tradingSymbol,
				    unitsInOrder,
				    ptime(orderDate, getDefaultBarTime()),
				    limitPrice)
    {}

    ~CoverAtLimitOrder()
    {}

    CoverAtLimitOrder (const CoverAtLimitOrder<Decimal>& rhs)
      : LimitExitOrder<Decimal> (rhs)
    {}

    CoverAtLimitOrder<Decimal>& 
    operator=(const CoverAtLimitOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      LimitExitOrder<Decimal>::operator=(rhs);
      return *this;
    }

    void ValidateOrderExecution(const ptime& /* fillDateTime */, const Decimal& fillPrice) const override
    {
      if (fillPrice > this->getLimitPrice())
        throw TradingOrderNotExecutedException ("CoverAtLimitOrder: fill price cannot be greater than limit price");
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

    bool isLongOrder() const
    {
      return false;
    }

    // True in the sense that is covers a short position

    bool isShortOrder() const
    {
      return true;
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  /**
   * @class StopOrder
   * @brief Abstract base class for all stop orders.
   *
   * Responsibilities:
   * - Define the stop price condition.
   */

  template <class Decimal> class StopOrder : public TradingOrder<Decimal>
    {
    public:
StopOrder(const std::string& tradingSymbol,
                const TradingVolume& unitsInOrder,
                const ptime& orderDateTime,
                const Decimal& stopPrice)
        : TradingOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime),
          mStopPrice(stopPrice)
      {}

      StopOrder(const std::string& tradingSymbol,
                const TradingVolume& unitsInOrder,
                const date& orderDate,
                const Decimal& stopPrice)
        : StopOrder<Decimal> (tradingSymbol,
			      unitsInOrder,
			      ptime(orderDate, getDefaultBarTime()),
			      stopPrice)
      {}

      virtual ~StopOrder()
      {}

      StopOrder (const StopOrder<Decimal>& rhs)
	: TradingOrder<Decimal> (rhs),
	  mStopPrice(rhs.mStopPrice)
      {}

      StopOrder<Decimal>& 
      operator=(const StopOrder<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	TradingOrder<Decimal>::operator=(rhs);
	mStopPrice = rhs.mStopPrice;
	return *this;
      }

      const Decimal& getStopPrice() const
      {
	return mStopPrice;
      }

      bool isMarketOrder() const
      {
	return false;
      }

      bool isStopOrder() const
      {
	return true;
      }

      bool isLimitOrder() const
      {
	return false;
      }


    private:
      Decimal mStopPrice;
  };

  //
  // class StopExitOrder
  //

  template <class Decimal> class StopExitOrder : public StopOrder<Decimal>
    {
    public:
      StopExitOrder(const std::string& tradingSymbol,
                    const TradingVolume& unitsInOrder,
                    const ptime& orderDateTime,
                    const Decimal& stopPrice)
        : StopOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime, stopPrice)
      {}

      StopExitOrder(const std::string& tradingSymbol, 
                    const TradingVolume& unitsInOrder,
                    const date& orderDate,
                    const Decimal& stopPrice)
        : StopExitOrder<Decimal> (tradingSymbol,
				  unitsInOrder,
				  ptime(orderDate, getDefaultBarTime()),
				  stopPrice)
      {}

      virtual ~StopExitOrder()
      {}

      StopExitOrder (const StopExitOrder<Decimal>& rhs)
	: StopOrder<Decimal> (rhs)
      {}

      StopExitOrder<Decimal>& 
      operator=(const StopExitOrder<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	StopOrder<Decimal>::operator=(rhs);
	return *this;
      }

      bool isEntryOrder() const
      {
	return false;
      }

      bool isExitOrder() const
      {
	return true;
      }

      uint32_t getOrderPriority() const
      {
	return 5;
      }
  };

  /**
   * @class SellAtStopOrder
   * @brief Closes a long position when price <= stop.
   */
  template <class Decimal> class SellAtStopOrder : public StopExitOrder<Decimal>
  {
  public:
    using TradingOrder<Decimal>::ValidateOrderExecution;

    SellAtStopOrder(const std::string& tradingSymbol,
                    const TradingVolume& unitsInOrder,
                    const ptime& orderDateTime,
                    const Decimal& stopPrice)
      : StopExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime, stopPrice)
    {}

    SellAtStopOrder(const std::string& tradingSymbol,
                    const TradingVolume& unitsInOrder,
                    const date& orderDate,
                    const Decimal& stopPrice)
      : SellAtStopOrder<Decimal> (tradingSymbol,
				  unitsInOrder,
				  ptime(orderDate, getDefaultBarTime()),
				  stopPrice)
    {}

    ~SellAtStopOrder()
    {}

    SellAtStopOrder (const SellAtStopOrder<Decimal>& rhs)
      : StopExitOrder<Decimal> (rhs)
    {}

    SellAtStopOrder<Decimal>& 
    operator=(const SellAtStopOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StopExitOrder<Decimal>::operator=(rhs);
      return *this;
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

    void ValidateOrderExecution(const ptime& /* fillDateTime */, const Decimal& fillPrice) const override
    {
      if (fillPrice > this->getStopPrice())
        throw TradingOrderNotExecutedException ("SellAtStopOrder: fill price cannot be greater than stop price");
    }

    // True in the sense that it closes a long position
    bool isLongOrder() const
    {
      return true;
    }

    bool isShortOrder() const
    {
      return false;
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  /**
   * @class CoverAtStopOrder
   * @brief Closes a short position when price >= stop.
   */

  template <class Decimal> class CoverAtStopOrder : public StopExitOrder<Decimal>
  {
  public:
    using TradingOrder<Decimal>::ValidateOrderExecution;

    CoverAtStopOrder(const std::string& tradingSymbol,
                     const TradingVolume& unitsInOrder,
                     const ptime& orderDateTime,
                     const Decimal& stopPrice)
      : StopExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDateTime, stopPrice)
    {}

    CoverAtStopOrder(const std::string& tradingSymbol, 
                     const TradingVolume& unitsInOrder,
                     const date& orderDate,
                     const Decimal& stopPrice)
      : CoverAtStopOrder<Decimal> (tradingSymbol,
				   unitsInOrder,
				   ptime(orderDate, getDefaultBarTime()),
				   stopPrice)
    {}

    ~CoverAtStopOrder()
    {}

    CoverAtStopOrder (const CoverAtStopOrder<Decimal>& rhs)
      : StopExitOrder<Decimal> (rhs)
    {}

    CoverAtStopOrder<Decimal>& 
    operator=(const CoverAtStopOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StopExitOrder<Decimal>::operator=(rhs);
      return *this;
    }

    void accept (TradingOrderVisitor<Decimal> &v)
    {
      v.visit(this);
    }

    void ValidateOrderExecution(const ptime& /* fillDateTime */, const Decimal& fillPrice) const override
    {
      if (fillPrice < this->getStopPrice())
        throw TradingOrderNotExecutedException ("CoverAtStopOrder: fill price cannot be less than stop price");
    }


    bool isLongOrder() const
    {
      return false;
    }

    // True in the sense that it closes a short position
    bool isShortOrder() const
    {
      return true;
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Decimal>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  /**
   * @class TradingOrderState
   * @brief Abstract base class representing the state of a trading order.
   *
   * Responsibilities:
   * - Encapsulate order state (pending, executed, or canceled).
   * - Delegate fill/cancel logic to state-specific subclasses.
   */

  template <class Decimal> class TradingOrderState
  {
  public:
    TradingOrderState()
      {}

    virtual ~TradingOrderState()
      {}

    virtual bool isOrderPending() const = 0;
    virtual bool isOrderExecuted() const = 0;
    virtual bool isOrderCanceled() const = 0;
    virtual void MarkOrderExecuted(TradingOrder<Decimal>* order,
                           const ptime& fillDateTime, 
                           const Decimal& fillPrice) = 0;

    virtual void MarkOrderExecuted(TradingOrder<Decimal>* order,
				   const TimeSeriesDate& fillDate,
				   const Decimal& fillPrice) = 0;
    virtual void MarkOrderCanceled(TradingOrder<Decimal>* order) = 0;
    virtual const Decimal& getFillPrice() const = 0;
    virtual TimeSeriesDate getFillDate() const = 0;
    virtual const ptime& getFillDateTime() const = 0;
  };

  /**
   * @class PendingOrderState
   * @brief Represents an order awaiting execution.
   */
  template <class Decimal> class PendingOrderState : public TradingOrderState<Decimal>
  {
  public:
    PendingOrderState()
      : TradingOrderState<Decimal>()
      {}

    ~PendingOrderState()
      {}

    bool isOrderPending() const
    {
      return true;
    }

    bool isOrderExecuted() const
    {
      return false;
    }

    bool isOrderCanceled() const
    {
      return false;
    }

    const Decimal& getFillPrice() const
    {
      throw TradingOrderNotExecutedException("No fill price in pending state");
    }

    TimeSeriesDate getFillDate() const
    {
      throw TradingOrderNotExecutedException("No fill date in pending state");
    }

    const ptime& getFillDateTime() const
    {
      throw TradingOrderNotExecutedException("No fill date in pending state");
    }

    // Takes ptime for fillDateTime
    void MarkOrderExecuted(TradingOrder<Decimal>* order,
                           const ptime& fillDateTime,
                           const Decimal& fillPrice) override
    {
      order->ChangeState (std::make_shared<ExecutedOrderState<Decimal>>(fillDateTime, fillPrice));
    }
    
    void MarkOrderExecuted(TradingOrder<Decimal>* order,
			   const TimeSeriesDate& fillDate, 
			   const Decimal& fillPrice)
    {
      MarkOrderExecuted(order, ptime(fillDate, getDefaultBarTime()), fillPrice);
    }

    void MarkOrderCanceled(TradingOrder<Decimal>* order)
    {
      order->ChangeState (std::make_shared<CanceledOrderState<Decimal>>());
    }
  };

  /**
   * @class ExecutedOrderState
   * @brief Represents an order that has been filled.
   */
  template <class Decimal> class ExecutedOrderState : public TradingOrderState<Decimal>
  {
  public:
    ExecutedOrderState(const ptime& fillDateTime,
                       const Decimal& fillPrice)
      : TradingOrderState<Decimal>(),
        mEntryDateTime(fillDateTime),
        mEntryPrice(fillPrice)
      {}

    ExecutedOrderState(const TimeSeriesDate& fillDate, 
			    const Decimal& fillPrice)
      : ExecutedOrderState<Decimal>(ptime(fillDate, getDefaultBarTime()), fillPrice)
      {}

    ~ExecutedOrderState()
      {}

    bool isOrderPending() const
    {
      return false;
    }

    bool isOrderExecuted() const
    {
      return true;
    }

    bool isOrderCanceled() const
    {
      return false;
    }

    const Decimal& getFillPrice() const
    {
      return mEntryPrice;
    }

    TimeSeriesDate getFillDate() const
    {
      return mEntryDateTime.date();
    }

    const ptime& getFillDateTime() const
    {
      return mEntryDateTime;
    }

    void MarkOrderExecuted(TradingOrder<Decimal>* /* order */,
                           const ptime& /* fillDateTime */,
                           const Decimal& /* fillPrice */) override // Added override
    {
      throw TradingOrderExecutedException("Trading order has already been executed");
    }

    void MarkOrderExecuted(TradingOrder<Decimal>* /* order */,
      const TimeSeriesDate& /* fillDate */,
      const Decimal& /* fillPrice */)
    {
      throw TradingOrderExecutedException("Trading order has already been executed");
    }

    void MarkOrderCanceled(TradingOrder<Decimal>* /* order */)
    {
      throw TradingOrderExecutedException("Cannot cancel a executed order");
    }

  private:
    ptime mEntryDateTime;
    Decimal mEntryPrice;
  };

  /**
   * @class CanceledOrderState
   * @brief Represents an order that has been canceled.
   */
  template <class Decimal> class CanceledOrderState : public TradingOrderState<Decimal>
  {
  public:
    CanceledOrderState()
      : TradingOrderState<Decimal>()
      {}

    ~CanceledOrderState()
      {}

    bool isOrderPending() const
    {
      return false;
    }

    bool isOrderExecuted() const
    {
      return false;
    }

    bool isOrderCanceled() const
    {
      return true;
    }

    const Decimal& getFillPrice() const
    {
      throw TradingOrderNotExecutedException("No fill price in canceled state");
    }

    TimeSeriesDate getFillDate() const
    {
      throw TradingOrderNotExecutedException("No fill date in canceled state");
    }

    const ptime& getFillDateTime() const
    {
      throw TradingOrderNotExecutedException("No fill date/time in canceled state");
    }

    void MarkOrderExecuted(TradingOrder<Decimal>* /* order */,
      const TimeSeriesDate& /* fillDate */,
      const Decimal& /* fillPrice */)
    {
      throw TradingOrderNotExecutedException("Cannot execute a cancelled order");
    }

    void MarkOrderExecuted(TradingOrder<Decimal>* /* order */,
                           const ptime& /* fillDateTime */,
                           const Decimal& /* fillPrice */) override
    {
      throw TradingOrderNotExecutedException("Cannot execute a cancelled order");
    }

    void MarkOrderCanceled(TradingOrder<Decimal>* /* order */)
    {
      throw TradingOrderExecutedException("Cannot cancel a already canceled order");
    }
  };
}
#endif
