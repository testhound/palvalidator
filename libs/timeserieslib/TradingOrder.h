// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TRADING_ORDER_H
#define __TRADING_ORDER_H 1

#include <exception>
#include <memory>
#include <list>
#include <cstdint>
#include <string>
#include "TradingOrderException.h"
#include "TimeSeriesEntry.h"

#include <atomic>

using dec::decimal;
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
 

  template <class Decimal> class TradingOrder
  {
  public:
    typedef typename std::list<std::shared_ptr<TradingOrderObserver<Decimal>>>::const_iterator ConstObserverIterator;

  public:
    TradingOrder(const std::string& tradingSymbol, 
		 const TradingVolume& unitsInOrder,
		 const TimeSeriesDate& orderDate);

    virtual ~TradingOrder()
    {}

    TradingOrder (const TradingOrder<Decimal>& rhs)
      : mTradingSymbol(rhs.mTradingSymbol),
	mUnitsInOrder(rhs.mUnitsInOrder),
	mOrderDate (rhs.mOrderDate),
	mOrderState (rhs.mOrderState),
	mOrderID(rhs.mOrderID),
	mObservers(rhs.mObservers)
    {}

    TradingOrder<Decimal>& 
    operator=(const TradingOrder<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mTradingSymbol = rhs.mTradingSymbol;
      mUnitsInOrder = rhs.mUnitsInOrder;
      mOrderDate = rhs.mOrderDate;
      mOrderState = rhs.mOrderState;
      mOrderID = rhs.mOrderID;
      mObservers = rhs.mObservers;

      return *this;
    }

    const std::string& getTradingSymbol() const
    {
      return mTradingSymbol;
    }

    const TradingVolume& getUnitsInOrder() const
    {
      return mUnitsInOrder;
    }

    const TimeSeriesDate& getOrderDate() const
    {
      return mOrderDate;
    }

    uint32_t getOrderID() const
    {
      return mOrderID;
    }

    

    virtual uint32_t getOrderPriority() const = 0;
    virtual bool isLongOrder() const = 0;
    virtual bool isShortOrder() const = 0;
    virtual bool isEntryOrder() const = 0;
    virtual bool isExitOrder() const = 0;
    virtual bool isMarketOrder() const = 0;
    virtual bool isStopOrder() const = 0;
    virtual bool isLimitOrder() const = 0;

    bool isOrderPending() const;
    bool isOrderExecuted() const;
    bool isOrderCanceled() const;
    void MarkOrderExecuted(const TimeSeriesDate& fillDate, 
			   const Decimal& fillPrice);
    void MarkOrderCanceled();
    
    const Decimal& getFillPrice() const;
    const TimeSeriesDate& getFillDate() const;
    virtual void accept (TradingOrderVisitor<Decimal> &visitor) = 0;

    void addObserver (std::shared_ptr<TradingOrderObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

  protected:
    ConstObserverIterator beginObserverList() const
    {
      return mObservers.begin();
    }

    ConstObserverIterator endObserverList() const
    {
      return mObservers.end();
    }

    virtual void notifyOrderExecuted() = 0;
    virtual void notifyOrderCanceled() = 0;
    virtual void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
					const Decimal& fillPrice) const = 0;

  private:
    void ChangeState (std::shared_ptr<TradingOrderState<Decimal>> newState)
    {
      mOrderState = newState;
    }

    friend class TradingOrderState<Decimal>;
    friend class PendingOrderState<Decimal>;

  private:
    std::string mTradingSymbol;
    TradingVolume mUnitsInOrder;
    TimeSeriesDate mOrderDate;
    std::shared_ptr<TradingOrderState<Decimal>> mOrderState;
    uint32_t mOrderID;
    static std::atomic<uint32_t> mOrderIDCount;
    std::list<std::shared_ptr<TradingOrderObserver<Decimal>>> mObservers;
  };

  template <class Decimal> std::atomic<uint32_t> TradingOrder<Decimal>::mOrderIDCount{0};

  template <class Decimal> class MarketOrder : public TradingOrder<Decimal>
    {
    public:
      MarketOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : TradingOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
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
      
      void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				  const Decimal& fillPrice) const
      {}
  };

  /////////////////////////

  template <class Decimal> class MarketEntryOrder : public MarketOrder<Decimal>
  {
  public:
      MarketEntryOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
      {}

      virtual ~MarketEntryOrder()
      {}

      MarketEntryOrder (const MarketEntryOrder<Decimal>& rhs)
      : MarketOrder<Decimal> (rhs)
      {}

      MarketEntryOrder<Decimal>& 
      operator=(const MarketEntryOrder<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	MarketOrder<Decimal>::operator=(rhs);
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
  };

  template <class Decimal> class MarketOnOpenLongOrder : public MarketEntryOrder<Decimal>
  {
  public:

    MarketOnOpenLongOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketEntryOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
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

  template <class Decimal> class MarketOnOpenShortOrder : public MarketEntryOrder<Decimal>
  {
  public:
    MarketOnOpenShortOrder(const std::string& tradingSymbol,
			   const TradingVolume& unitsInOrder,
			   const TimeSeriesDate& orderDate)
      : MarketEntryOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
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


  // Market on Open exit Orders

  template <class Decimal> class MarketExitOrder : public MarketOrder<Decimal>
  {
  public:
      MarketExitOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
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

  //
  // class MarketOnOpenSellOrder
  //
  // Closes a long position
  //

  template <class Decimal> class MarketOnOpenSellOrder : public MarketExitOrder<Decimal>
  {
  public:

    MarketOnOpenSellOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
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


  //
  // class MarketOnOpenCoverOrder
  //
  // Closes a long position
  //

  template <class Decimal> class MarketOnOpenCoverOrder : public MarketExitOrder<Decimal>
  {
  public:

    MarketOnOpenCoverOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate)
    {}

    MarketOnOpenCoverOrder (const MarketOnOpenCoverOrder<Decimal>& rhs)
      : MarketExitOrder<Decimal> (rhs)
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

  // Limit Orders
  template <class Decimal> class LimitOrder : public TradingOrder<Decimal>
    {
    public:
      LimitOrder(const std::string& tradingSymbol, 
		 const TradingVolume& unitsInOrder,
		 const TimeSeriesDate& orderDate,
		 const Decimal& limitPrice)
	: TradingOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate),
	  mLimitPrice(limitPrice)
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
		 const TimeSeriesDate& orderDate,
		 const Decimal& limitPrice)
	: LimitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate, limitPrice)
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


  //
  // class SellAtLimitOrder
  // 
  // Used to close a long position
  //

  template <class Decimal> class SellAtLimitOrder : public LimitExitOrder<Decimal>
  {
  public:
    SellAtLimitOrder(const std::string& tradingSymbol, 
		     const TradingVolume& unitsInOrder,
		     const TimeSeriesDate& orderDate,
		     const Decimal& limitPrice)
      : LimitExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate, limitPrice)
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

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const Decimal& fillPrice) const
    {
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


  //
  // class CoverAtLimitOrder
  // 
  // Used to close a short position
  //

  template <class Decimal> class CoverAtLimitOrder : public LimitExitOrder<Decimal>
  {
  public:
    CoverAtLimitOrder(const std::string& tradingSymbol, 
		      const TradingVolume& unitsInOrder,
		      const TimeSeriesDate& orderDate,
		      const Decimal& limitPrice)
      : LimitExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate, limitPrice)
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

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const Decimal& fillPrice) const
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

  ///////////////////////////////////////////////
  //  Classes for stop orders
  ///////////////////////////////////////////////

  template <class Decimal> class StopOrder : public TradingOrder<Decimal>
    {
    public:
      StopOrder(const std::string& tradingSymbol, 
		const TradingVolume& unitsInOrder,
		const TimeSeriesDate& orderDate,
		const Decimal& stopPrice)
	: TradingOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate),
	  mStopPrice(stopPrice)
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
		 const TimeSeriesDate& orderDate,
		 const Decimal& stopPrice)
	: StopOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate, stopPrice)
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

  //
  // class SellAtStopOrder
  // 
  // Used to close a long position
  //

  template <class Decimal> class SellAtStopOrder : public StopExitOrder<Decimal>
  {
  public:
    SellAtStopOrder(const std::string& tradingSymbol, 
		     const TradingVolume& unitsInOrder,
		     const TimeSeriesDate& orderDate,
		     const Decimal& stopPrice)
      : StopExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate, stopPrice)
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

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const Decimal& fillPrice) const
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

  //
  // class CoverAtStopOrder
  // 
  // Used to close a short position
  //

  template <class Decimal> class CoverAtStopOrder : public StopExitOrder<Decimal>
  {
  public:
    CoverAtStopOrder(const std::string& tradingSymbol, 
		     const TradingVolume& unitsInOrder,
		     const TimeSeriesDate& orderDate,
		     const Decimal& stopPrice)
      : StopExitOrder<Decimal> (tradingSymbol, unitsInOrder, orderDate, stopPrice)
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

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const Decimal& fillPrice) const
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

  ///////////////////////////////////////////////
  // TradingOrderState Implementation
  //////////////////////////////////////////////////


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
				   const TimeSeriesDate& fillDate, 
				   const Decimal& fillPrice) = 0;
    virtual void MarkOrderCanceled(TradingOrder<Decimal>* order) = 0;
    virtual const Decimal& getFillPrice() const = 0;
    virtual const TimeSeriesDate& getFillDate() const = 0;
  };

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

    const TimeSeriesDate& getFillDate() const
    {
      throw TradingOrderNotExecutedException("No fill date in pending state");
    }

    void MarkOrderExecuted(TradingOrder<Decimal>* order,
			   const TimeSeriesDate& fillDate, 
			   const Decimal& fillPrice)
    {
      order->ChangeState (std::make_shared<ExecutedOrderState<Decimal>>(fillDate, fillPrice));
    }

    void MarkOrderCanceled(TradingOrder<Decimal>* order)
    {
      order->ChangeState (std::make_shared<CanceledOrderState<Decimal>>());
    }
  };

  template <class Decimal> class ExecutedOrderState : public TradingOrderState<Decimal>
  {
  public:
    ExecutedOrderState(const TimeSeriesDate& fillDate, 
			    const Decimal& fillPrice)
      : TradingOrderState<Decimal>(),
	mEntryDate(fillDate),
	mEntryPrice(fillPrice)
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

    const TimeSeriesDate& getFillDate() const
    {
      return mEntryDate;
    }

    void MarkOrderExecuted(TradingOrder<Decimal>* order,
			   const TimeSeriesDate& fillDate, 
			   const Decimal& fillPrice)
    {
      throw TradingOrderExecutedException("Trading order has already been executed");
    }

    void MarkOrderCanceled(TradingOrder<Decimal>* order)
    {
      throw TradingOrderExecutedException("Cannot cancel a executed order");
    }

  private:
    TimeSeriesDate mEntryDate;
    Decimal mEntryPrice;
  };


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

    const TimeSeriesDate& getFillDate() const
    {
      throw TradingOrderNotExecutedException("No fill date in canceled state");
    }

    void MarkOrderExecuted(TradingOrder<Decimal>* order,
			   const TimeSeriesDate& fillDate, 
			   const Decimal& fillPrice)
    {
      throw TradingOrderNotExecutedException("Cannot execute a cancelled order");
    }

    void MarkOrderCanceled(TradingOrder<Decimal>* order)
    {
      throw TradingOrderExecutedException("Cannot cancel a already canceled order");
    }
  };

  template <class Decimal>
  inline TradingOrder<Decimal>::TradingOrder(const std::string& tradingSymbol, 
					  const TradingVolume& unitsInOrder,
					  const TimeSeriesDate& orderDate)
    : mTradingSymbol(tradingSymbol),
      mUnitsInOrder(unitsInOrder),
      mOrderDate (orderDate),
      mOrderState(new PendingOrderState<Decimal>()),
      mOrderID(++TradingOrder<Decimal>::mOrderIDCount),
      mObservers()
    {
      if (mUnitsInOrder.getTradingVolume() == 0)
	throw TradingOrderException ("TradingOrder constructor - order cannot have zero units for: " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
    }

  template <class Decimal>
  inline bool TradingOrder<Decimal>::isOrderPending() const
  {
    return mOrderState->isOrderPending();
  }
  
  template <class Decimal>  
  inline bool TradingOrder<Decimal>::isOrderExecuted() const
  {
    return mOrderState->isOrderExecuted();
  }

  template <class Decimal>
  inline bool TradingOrder<Decimal>::isOrderCanceled() const
  {
    return mOrderState->isOrderCanceled();
  }

  template <class Decimal>
  inline void TradingOrder<Decimal>::MarkOrderExecuted(const TimeSeriesDate& fillDate, 
						    const Decimal& fillPrice)
  {
    ValidateOrderExecution (fillDate, fillPrice);

    if (fillDate >= getOrderDate())
      {
	mOrderState->MarkOrderExecuted (this, fillDate, fillPrice);
	this->notifyOrderExecuted();
      }
    else
      throw TradingOrderNotExecutedException ("Order fill date cannot occur before order date");
  }

  template <class Decimal>
  inline void TradingOrder<Decimal>::MarkOrderCanceled()
  {
     mOrderState->MarkOrderCanceled (this);
     this->notifyOrderCanceled();
  }

  template <class Decimal>
  inline const Decimal& TradingOrder<Decimal>::getFillPrice() const
  {
    return mOrderState->getFillPrice();
  }

  template <class Decimal>
  inline const TimeSeriesDate& TradingOrder<Decimal>::getFillDate() const
  {
    return  mOrderState->getFillDate();
  }
}


#endif
