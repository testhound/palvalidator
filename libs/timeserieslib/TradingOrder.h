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
  template <int Prec> class TradingOrderVisitor;

  template <int Prec> class TradingOrderState;
  template <int Prec> class MarketOnOpenLongOrder;
  template <int Prec> class MarketOnOpenShortOrder;
  template <int Prec> class MarketOnOpenSellOrder;
  template <int Prec> class MarketOnOpenCoverOrder;
  template <int Prec> class ExecutedOrderState;
  template <int Prec> class CanceledOrderState;
  template <int Prec> class PendingOrderState;
  template <int Prec> class SellAtLimitOrder;
  template <int Prec> class CoverAtLimitOrder;
  template <int Prec> class CoverAtStopOrder;
  template <int Prec> class SellAtStopOrder;
  template <int Prec> class TradingOrderObserver;
  template <int Prec> class TradingOrder;
  
  template <int Prec>
  class TradingOrderVisitor
  {
  public:
    TradingOrderVisitor()
    {}

    virtual ~TradingOrderVisitor()
    {}

    virtual void visit (MarketOnOpenLongOrder<Prec> *order) = 0;
    virtual void visit (MarketOnOpenShortOrder<Prec> *order) = 0;

    virtual void visit (MarketOnOpenSellOrder<Prec> *order) = 0;
    virtual void visit (MarketOnOpenCoverOrder<Prec> *order) = 0;

    virtual void visit (SellAtLimitOrder<Prec> *order) = 0;
    virtual void visit (CoverAtLimitOrder<Prec> *order) = 0;
    virtual void visit (CoverAtStopOrder<Prec> *order) = 0;
    virtual void visit (SellAtStopOrder<Prec> *order) = 0;
  };

  template <int Prec>
  class TradingOrderObserver
  {
  public:
    TradingOrderObserver()
    {}

    virtual ~TradingOrderObserver()
    {}

    virtual void OrderExecuted (MarketOnOpenLongOrder<Prec> *order) = 0;
    virtual void OrderExecuted (MarketOnOpenShortOrder<Prec> *order) = 0;
    virtual void OrderExecuted (MarketOnOpenSellOrder<Prec> *order) = 0;
    virtual void OrderExecuted (MarketOnOpenCoverOrder<Prec> *order) = 0;
    virtual void OrderExecuted (SellAtLimitOrder<Prec> *order) = 0;
    virtual void OrderExecuted (CoverAtLimitOrder<Prec> *order) = 0;
    virtual void OrderExecuted (CoverAtStopOrder<Prec> *order) = 0;
    virtual void OrderExecuted (SellAtStopOrder<Prec> *order) = 0;

    virtual void OrderCanceled (MarketOnOpenLongOrder<Prec> *order) = 0;
    virtual void OrderCanceled (MarketOnOpenShortOrder<Prec> *order) = 0;
    virtual void OrderCanceled (MarketOnOpenSellOrder<Prec> *order) = 0;
    virtual void OrderCanceled (MarketOnOpenCoverOrder<Prec> *order) = 0;
    virtual void OrderCanceled (SellAtLimitOrder<Prec> *order) = 0;
    virtual void OrderCanceled (CoverAtLimitOrder<Prec> *order) = 0;
    virtual void OrderCanceled (CoverAtStopOrder<Prec> *order) = 0;
    virtual void OrderCanceled (SellAtStopOrder<Prec> *order) = 0;
  };
 

  template <int Prec> class TradingOrder
  {
  public:
    typedef typename std::list<std::shared_ptr<TradingOrderObserver<Prec>>>::const_iterator ConstObserverIterator;

  public:
    TradingOrder(const std::string& tradingSymbol, 
		 const TradingVolume& unitsInOrder,
		 const TimeSeriesDate& orderDate);

    virtual ~TradingOrder()
    {}

    TradingOrder (const TradingOrder<Prec>& rhs)
      : mTradingSymbol(rhs.mTradingSymbol),
	mUnitsInOrder(rhs.mUnitsInOrder),
	mOrderDate (rhs.mOrderDate),
	mOrderState (rhs.mOrderState),
	mOrderID(rhs.mOrderID),
	mObservers(rhs.mObservers)
    {}

    TradingOrder<Prec>& 
    operator=(const TradingOrder<Prec> &rhs)
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
			   const decimal<Prec>& fillPrice);
    void MarkOrderCanceled();
    
    const decimal<Prec>& getFillPrice() const;
    const TimeSeriesDate& getFillDate() const;
    virtual void accept (TradingOrderVisitor<Prec> &visitor) = 0;

    void addObserver (std::shared_ptr<TradingOrderObserver<Prec>> observer)
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
					const decimal<Prec>& fillPrice) const = 0;

  private:
    void ChangeState (std::shared_ptr<TradingOrderState<Prec>> newState)
    {
      mOrderState = newState;
    }

    friend class TradingOrderState<Prec>;
    friend class PendingOrderState<Prec>;

  private:
    std::string mTradingSymbol;
    TradingVolume mUnitsInOrder;
    TimeSeriesDate mOrderDate;
    std::shared_ptr<TradingOrderState<Prec>> mOrderState;
    uint32_t mOrderID;
    static std::atomic<uint32_t> mOrderIDCount;
    std::list<std::shared_ptr<TradingOrderObserver<Prec>>> mObservers;
  };

  template <int Prec> std::atomic<uint32_t> TradingOrder<Prec>::mOrderIDCount{0};

  template <int Prec> class MarketOrder : public TradingOrder<Prec>
    {
    public:
      MarketOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : TradingOrder<Prec> (tradingSymbol, unitsInOrder, orderDate)
    {}

      virtual ~MarketOrder()
      {}

      MarketOrder (const MarketOrder<Prec>& rhs)
      : TradingOrder<Prec> (rhs)
      {}

      MarketOrder<Prec>& 
      operator=(const MarketOrder<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	TradingOrder<Prec>::operator=(rhs);
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
				  const decimal<Prec>& fillPrice) const
      {}
  };

  /////////////////////////

  template <int Prec> class MarketEntryOrder : public MarketOrder<Prec>
  {
  public:
      MarketEntryOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketOrder<Prec> (tradingSymbol, unitsInOrder, orderDate)
      {}

      virtual ~MarketEntryOrder()
      {}

      MarketEntryOrder (const MarketEntryOrder<Prec>& rhs)
      : MarketOrder<Prec> (rhs)
      {}

      MarketEntryOrder<Prec>& 
      operator=(const MarketEntryOrder<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	MarketOrder<Prec>::operator=(rhs);
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

  template <int Prec> class MarketOnOpenLongOrder : public MarketEntryOrder<Prec>
  {
  public:

    MarketOnOpenLongOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketEntryOrder<Prec> (tradingSymbol, unitsInOrder, orderDate)
    {}

    MarketOnOpenLongOrder (const MarketOnOpenLongOrder<Prec>& rhs)
      : MarketEntryOrder<Prec> (rhs)
    {}

    MarketOnOpenLongOrder<Prec>& 
    operator=(const MarketOnOpenLongOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketEntryOrder<Prec>::operator=(rhs);
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

    void accept (TradingOrderVisitor<Prec> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  template <int Prec> class MarketOnOpenShortOrder : public MarketEntryOrder<Prec>
  {
  public:
    MarketOnOpenShortOrder(const std::string& tradingSymbol,
			   const TradingVolume& unitsInOrder,
			   const TimeSeriesDate& orderDate)
      : MarketEntryOrder<Prec> (tradingSymbol, unitsInOrder, orderDate)
    {}

    MarketOnOpenShortOrder (const MarketOnOpenShortOrder<Prec>& rhs)
      : MarketEntryOrder<Prec> (rhs)
    {}

    MarketOnOpenShortOrder<Prec>& 
    operator=(const MarketOnOpenShortOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketEntryOrder<Prec>::operator=(rhs);
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

    void accept (TradingOrderVisitor<Prec> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };


  // Market on Open exit Orders

  template <int Prec> class MarketExitOrder : public MarketOrder<Prec>
  {
  public:
      MarketExitOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketOrder<Prec> (tradingSymbol, unitsInOrder, orderDate)
      {}

      virtual ~MarketExitOrder()
      {}

      MarketExitOrder (const MarketExitOrder<Prec>& rhs)
      : MarketOrder<Prec> (rhs)
      {}

      MarketExitOrder<Prec>& 
      operator=(const MarketExitOrder<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	MarketOrder<Prec>::operator=(rhs);
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

  template <int Prec> class MarketOnOpenSellOrder : public MarketExitOrder<Prec>
  {
  public:

    MarketOnOpenSellOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketExitOrder<Prec> (tradingSymbol, unitsInOrder, orderDate)
    {}

    MarketOnOpenSellOrder (const MarketOnOpenSellOrder<Prec>& rhs)
      : MarketExitOrder<Prec> (rhs)
    {}

    MarketOnOpenSellOrder<Prec>& 
    operator=(const MarketOnOpenSellOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketExitOrder<Prec>::operator=(rhs);
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

    void accept (TradingOrderVisitor<Prec> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };


  //
  // class MarketOnOpenCoverOrder
  //
  // Closes a long position
  //

  template <int Prec> class MarketOnOpenCoverOrder : public MarketExitOrder<Prec>
  {
  public:

    MarketOnOpenCoverOrder(const std::string& tradingSymbol, 
			  const TradingVolume& unitsInOrder,
			  const TimeSeriesDate& orderDate)
      : MarketExitOrder<Prec> (tradingSymbol, unitsInOrder, orderDate)
    {}

    MarketOnOpenCoverOrder (const MarketOnOpenCoverOrder<Prec>& rhs)
      : MarketExitOrder<Prec> (rhs)
    {}

    MarketOnOpenCoverOrder<Prec>& 
    operator=(const MarketOnOpenCoverOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      MarketExitOrder<Prec>::operator=(rhs);
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

    void accept (TradingOrderVisitor<Prec> &v)
    {
      v.visit(this);
    }

    void notifyOrderExecuted()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  // Limit Orders
  template <int Prec> class LimitOrder : public TradingOrder<Prec>
    {
    public:
      LimitOrder(const std::string& tradingSymbol, 
		 const TradingVolume& unitsInOrder,
		 const TimeSeriesDate& orderDate,
		 const dec::decimal<Prec>& limitPrice)
	: TradingOrder<Prec> (tradingSymbol, unitsInOrder, orderDate),
	  mLimitPrice(limitPrice)
      {}

      virtual ~LimitOrder()
      {}

      LimitOrder (const LimitOrder<Prec>& rhs)
	: TradingOrder<Prec> (rhs),
	  mLimitPrice(rhs.mLimitPrice)
      {}

      LimitOrder<Prec>& 
      operator=(const LimitOrder<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	TradingOrder<Prec>::operator=(rhs);
	mLimitPrice = rhs.mLimitPrice;
	return *this;
      }

      const dec::decimal<Prec>& getLimitPrice() const
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
      dec::decimal<Prec> mLimitPrice;
  };

  //
  // class LimitExitOrder
  //

  template <int Prec> class LimitExitOrder : public LimitOrder<Prec>
    {
    public:
      LimitExitOrder(const std::string& tradingSymbol, 
		 const TradingVolume& unitsInOrder,
		 const TimeSeriesDate& orderDate,
		 const dec::decimal<Prec>& limitPrice)
	: LimitOrder<Prec> (tradingSymbol, unitsInOrder, orderDate, limitPrice)
      {}

      virtual ~LimitExitOrder()
      {}

      LimitExitOrder (const LimitExitOrder<Prec>& rhs)
	: LimitOrder<Prec> (rhs)
      {}

      LimitExitOrder<Prec>& 
      operator=(const LimitExitOrder<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	LimitOrder<Prec>::operator=(rhs);
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

  template <int Prec> class SellAtLimitOrder : public LimitExitOrder<Prec>
  {
  public:
    SellAtLimitOrder(const std::string& tradingSymbol, 
		     const TradingVolume& unitsInOrder,
		     const TimeSeriesDate& orderDate,
		     const dec::decimal<Prec>& limitPrice)
      : LimitExitOrder<Prec> (tradingSymbol, unitsInOrder, orderDate, limitPrice)
    {}

    ~SellAtLimitOrder()
    {}

    SellAtLimitOrder (const SellAtLimitOrder<Prec>& rhs)
      : LimitExitOrder<Prec> (rhs)
    {}

    SellAtLimitOrder<Prec>& 
    operator=(const SellAtLimitOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      LimitExitOrder<Prec>::operator=(rhs);
      return *this;
    }

    void accept (TradingOrderVisitor<Prec> &v)
    {
      v.visit(this);
    }

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const decimal<Prec>& fillPrice) const
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
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };


  //
  // class CoverAtLimitOrder
  // 
  // Used to close a short position
  //

  template <int Prec> class CoverAtLimitOrder : public LimitExitOrder<Prec>
  {
  public:
    CoverAtLimitOrder(const std::string& tradingSymbol, 
		      const TradingVolume& unitsInOrder,
		      const TimeSeriesDate& orderDate,
		      const dec::decimal<Prec>& limitPrice)
      : LimitExitOrder<Prec> (tradingSymbol, unitsInOrder, orderDate, limitPrice)
    {}

    ~CoverAtLimitOrder()
    {}

    CoverAtLimitOrder (const CoverAtLimitOrder<Prec>& rhs)
      : LimitExitOrder<Prec> (rhs)
    {}

    CoverAtLimitOrder<Prec>& 
    operator=(const CoverAtLimitOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      LimitExitOrder<Prec>::operator=(rhs);
      return *this;
    }

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const decimal<Prec>& fillPrice) const
    {
      if (fillPrice > this->getLimitPrice())
	throw TradingOrderNotExecutedException ("CoverAtLimitOrder: fill price cannot be greater than limit price");
    }

    void accept (TradingOrderVisitor<Prec> &v)
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
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  ///////////////////////////////////////////////
  //  Classes for stop orders
  ///////////////////////////////////////////////

  template <int Prec> class StopOrder : public TradingOrder<Prec>
    {
    public:
      StopOrder(const std::string& tradingSymbol, 
		const TradingVolume& unitsInOrder,
		const TimeSeriesDate& orderDate,
		const dec::decimal<Prec>& stopPrice)
	: TradingOrder<Prec> (tradingSymbol, unitsInOrder, orderDate),
	  mStopPrice(stopPrice)
      {}

      virtual ~StopOrder()
      {}

      StopOrder (const StopOrder<Prec>& rhs)
	: TradingOrder<Prec> (rhs),
	  mStopPrice(rhs.mStopPrice)
      {}

      StopOrder<Prec>& 
      operator=(const StopOrder<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	TradingOrder<Prec>::operator=(rhs);
	mStopPrice = rhs.mStopPrice;
	return *this;
      }

      const dec::decimal<Prec>& getStopPrice() const
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
      dec::decimal<Prec> mStopPrice;
  };

  //
  // class StopExitOrder
  //

  template <int Prec> class StopExitOrder : public StopOrder<Prec>
    {
    public:
      StopExitOrder(const std::string& tradingSymbol, 
		 const TradingVolume& unitsInOrder,
		 const TimeSeriesDate& orderDate,
		 const dec::decimal<Prec>& stopPrice)
	: StopOrder<Prec> (tradingSymbol, unitsInOrder, orderDate, stopPrice)
      {}

      virtual ~StopExitOrder()
      {}

      StopExitOrder (const StopExitOrder<Prec>& rhs)
	: StopOrder<Prec> (rhs)
      {}

      StopExitOrder<Prec>& 
      operator=(const StopExitOrder<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;
	
	StopOrder<Prec>::operator=(rhs);
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

  template <int Prec> class SellAtStopOrder : public StopExitOrder<Prec>
  {
  public:
    SellAtStopOrder(const std::string& tradingSymbol, 
		     const TradingVolume& unitsInOrder,
		     const TimeSeriesDate& orderDate,
		     const dec::decimal<Prec>& stopPrice)
      : StopExitOrder<Prec> (tradingSymbol, unitsInOrder, orderDate, stopPrice)
    {}

    ~SellAtStopOrder()
    {}

    SellAtStopOrder (const SellAtStopOrder<Prec>& rhs)
      : StopExitOrder<Prec> (rhs)
    {}

    SellAtStopOrder<Prec>& 
    operator=(const SellAtStopOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StopExitOrder<Prec>::operator=(rhs);
      return *this;
    }

    void accept (TradingOrderVisitor<Prec> &v)
    {
      v.visit(this);
    }

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const decimal<Prec>& fillPrice) const
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
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  //
  // class CoverAtStopOrder
  // 
  // Used to close a short position
  //

  template <int Prec> class CoverAtStopOrder : public StopExitOrder<Prec>
  {
  public:
    CoverAtStopOrder(const std::string& tradingSymbol, 
		     const TradingVolume& unitsInOrder,
		     const TimeSeriesDate& orderDate,
		     const dec::decimal<Prec>& stopPrice)
      : StopExitOrder<Prec> (tradingSymbol, unitsInOrder, orderDate, stopPrice)
    {}

    ~CoverAtStopOrder()
    {}

    CoverAtStopOrder (const CoverAtStopOrder<Prec>& rhs)
      : StopExitOrder<Prec> (rhs)
    {}

    CoverAtStopOrder<Prec>& 
    operator=(const CoverAtStopOrder<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StopExitOrder<Prec>::operator=(rhs);
      return *this;
    }

    void accept (TradingOrderVisitor<Prec> &v)
    {
      v.visit(this);
    }

    void ValidateOrderExecution(const TimeSeriesDate& fillDate, 
				const decimal<Prec>& fillPrice) const
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
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderExecuted (this);
    }

    void notifyOrderCanceled()
    {
      typename TradingOrder<Prec>::ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it)->OrderCanceled (this);
    }
  };

  ///////////////////////////////////////////////
  // TradingOrderState Implementation
  //////////////////////////////////////////////////


  template <int Prec> class TradingOrderState
  {
  public:
    TradingOrderState()
      {}

    virtual ~TradingOrderState()
      {}

    virtual bool isOrderPending() const = 0;
    virtual bool isOrderExecuted() const = 0;
    virtual bool isOrderCanceled() const = 0;
    virtual void MarkOrderExecuted(TradingOrder<Prec>* order,
				   const TimeSeriesDate& fillDate, 
				   const decimal<Prec>& fillPrice) = 0;
    virtual void MarkOrderCanceled(TradingOrder<Prec>* order) = 0;
    virtual const decimal<Prec>& getFillPrice() const = 0;
    virtual const TimeSeriesDate& getFillDate() const = 0;
  };

  template <int Prec> class PendingOrderState : public TradingOrderState<Prec>
  {
  public:
    PendingOrderState()
      : TradingOrderState<Prec>()
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

    const decimal<Prec>& getFillPrice() const
    {
      throw TradingOrderNotExecutedException("No fill price in pending state");
    }

    const TimeSeriesDate& getFillDate() const
    {
      throw TradingOrderNotExecutedException("No fill date in pending state");
    }

    void MarkOrderExecuted(TradingOrder<Prec>* order,
			   const TimeSeriesDate& fillDate, 
			   const decimal<Prec>& fillPrice)
    {
      order->ChangeState (std::make_shared<ExecutedOrderState<Prec>>(fillDate, fillPrice));
    }

    void MarkOrderCanceled(TradingOrder<Prec>* order)
    {
      order->ChangeState (std::make_shared<CanceledOrderState<Prec>>());
    }
  };

  template <int Prec> class ExecutedOrderState : public TradingOrderState<Prec>
  {
  public:
    ExecutedOrderState(const TimeSeriesDate& fillDate, 
			    const decimal<Prec>& fillPrice)
      : TradingOrderState<Prec>(),
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

    const decimal<Prec>& getFillPrice() const
    {
      return mEntryPrice;
    }

    const TimeSeriesDate& getFillDate() const
    {
      return mEntryDate;
    }

    void MarkOrderExecuted(TradingOrder<Prec>* order,
			   const TimeSeriesDate& fillDate, 
			   const decimal<Prec>& fillPrice)
    {
      throw TradingOrderExecutedException("Trading order has already been executed");
    }

    void MarkOrderCanceled(TradingOrder<Prec>* order)
    {
      throw TradingOrderExecutedException("Cannot cancel a executed order");
    }

  private:
    TimeSeriesDate mEntryDate;
    decimal<Prec> mEntryPrice;
  };


  template <int Prec> class CanceledOrderState : public TradingOrderState<Prec>
  {
  public:
    CanceledOrderState()
      : TradingOrderState<Prec>()
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

    const decimal<Prec>& getFillPrice() const
    {
      throw TradingOrderNotExecutedException("No fill price in canceled state");
    }

    const TimeSeriesDate& getFillDate() const
    {
      throw TradingOrderNotExecutedException("No fill date in canceled state");
    }

    void MarkOrderExecuted(TradingOrder<Prec>* order,
			   const TimeSeriesDate& fillDate, 
			   const decimal<Prec>& fillPrice)
    {
      throw TradingOrderNotExecutedException("Cannot execute a cancelled order");
    }

    void MarkOrderCanceled(TradingOrder<Prec>* order)
    {
      throw TradingOrderExecutedException("Cannot cancel a already canceled order");
    }
  };

  template <int Prec>
  inline TradingOrder<Prec>::TradingOrder(const std::string& tradingSymbol, 
					  const TradingVolume& unitsInOrder,
					  const TimeSeriesDate& orderDate)
    : mTradingSymbol(tradingSymbol),
      mUnitsInOrder(unitsInOrder),
      mOrderDate (orderDate),
      mOrderState(new PendingOrderState<Prec>()),
      mOrderID(++TradingOrder<Prec>::mOrderIDCount),
      mObservers()
    {
      if (mUnitsInOrder.getTradingVolume() == 0)
	throw TradingOrderException ("TradingOrder constructor - order cannot have zero units for: " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
    }

  template <int Prec>
  inline bool TradingOrder<Prec>::isOrderPending() const
  {
    return mOrderState->isOrderPending();
  }
  
  template <int Prec>  
  inline bool TradingOrder<Prec>::isOrderExecuted() const
  {
    return mOrderState->isOrderExecuted();
  }

  template <int Prec>
  inline bool TradingOrder<Prec>::isOrderCanceled() const
  {
    return mOrderState->isOrderCanceled();
  }

  template <int Prec>
  inline void TradingOrder<Prec>::MarkOrderExecuted(const TimeSeriesDate& fillDate, 
						    const decimal<Prec>& fillPrice)
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

  template <int Prec>
  inline void TradingOrder<Prec>::MarkOrderCanceled()
  {
     mOrderState->MarkOrderCanceled (this);
     this->notifyOrderCanceled();
  }

  template <int Prec>
  inline const decimal<Prec>& TradingOrder<Prec>::getFillPrice() const
  {
    return mOrderState->getFillPrice();
  }

  template <int Prec>
  inline const TimeSeriesDate& TradingOrder<Prec>::getFillDate() const
  {
    return  mOrderState->getFillDate();
  }
}


#endif
