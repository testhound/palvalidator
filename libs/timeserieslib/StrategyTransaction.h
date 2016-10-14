// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STRATEGY_TRANSACTION_H
#define __STRATEGY_TRANSACTION_H 1

#include <memory>
#include <functional>
#include <cstdint>
#include <list>
#include <boost/date_time.hpp>
#include "TradingOrder.h"
#include "TradingPosition.h"

using std::shared_ptr;
using std::list;
using std::reference_wrapper;

namespace mkc_timeseries
{
  class StrategyTransactionException : public std::runtime_error
  {
  public:
    StrategyTransactionException(const std::string msg) 
      : std::runtime_error(msg)
    {}
    
    ~StrategyTransactionException()
    {}
    
  };

  template <class Decimal> class StrategyTransaction;
  template <class Decimal> class StrategyTransactionState;
  template <class Decimal> class StrategyTransactionStateOpen;

  template <class Decimal>
  class StrategyTransactionObserver
  {
  public:
    StrategyTransactionObserver()
    {}

    StrategyTransactionObserver (const StrategyTransactionObserver<Decimal>& rhs)
      {}

    virtual ~StrategyTransactionObserver()
    {}

    StrategyTransactionObserver<Decimal>& 
    operator=(const StrategyTransactionObserver<Decimal> &rhs)
    {
      return *this;
    }

    virtual void TransactionComplete (StrategyTransaction<Decimal> *transaction) = 0;
  };


  //
  // class StrategyTransaction
  //
  // A strategy transaction consists of
  //
  // 1. A entry TradingOrder
  // 2. A TradingPosition
  // 3. A exit TradingOrder

  template <class Decimal> class StrategyTransaction
  {
  public:
    StrategyTransaction (std::shared_ptr<TradingOrder<Decimal>> entryOrder,
			 std::shared_ptr<TradingPosition<Decimal>> aPosition);

    StrategyTransaction (const StrategyTransaction<Decimal>& rhs)
      : mEntryOrder(rhs.mEntryOrder),
	mPosition(rhs.mPosition),
	mTransactionState(rhs.mTransactionState),
	mObservers(rhs.mObservers)
    {}

    StrategyTransaction<Decimal>& 
    operator=(const StrategyTransaction<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
      
      mEntryOrder = rhs.mEntryOrder;
      mPosition = rhs.mPosition;
      mTransactionState = rhs.mTransactionState;
      mObservers = rhs.mObservers;
      return *this;
    }

    ~StrategyTransaction()
    {}

    std::shared_ptr<TradingOrder<Decimal>> getEntryTradingOrder() const
    {
      return mEntryOrder;
    }

    std::shared_ptr<TradingPosition<Decimal>> getTradingPosition() const
    {
      return mPosition;
    }

    std::shared_ptr<TradingPosition<Decimal>>
    getTradingPositionPtr() const
    {
      return mPosition;
    }

    std::shared_ptr<TradingOrder<Decimal>> getExitTradingOrder() const;

    bool isTransactionOpen() const;

    bool isTransactionComplete() const;

    void completeTransaction (std::shared_ptr<TradingOrder<Decimal>> exitOrder);

    void addObserver (reference_wrapper<StrategyTransactionObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

  private:
    typedef typename list<reference_wrapper<StrategyTransactionObserver<Decimal>>>::const_iterator ConstObserverIterator;

    ConstObserverIterator beginObserverList() const
    {
      return mObservers.begin();
    }

    ConstObserverIterator endObserverList() const
    {
      return mObservers.end();
    }

    void notifyTransactionComplete()
    {
      ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it).get().TransactionComplete (this);
    }

  private:
    void ChangeState (std::shared_ptr<StrategyTransactionState<Decimal>> newState)
    {
      mTransactionState = newState;
    }

    friend class StrategyTransactionState<Decimal>;
    friend class StrategyTransactionStateOpen<Decimal>;

  private:
    std::shared_ptr<TradingOrder<Decimal>> mEntryOrder;
    std::shared_ptr<TradingPosition<Decimal>> mPosition;
    std::shared_ptr<StrategyTransactionState<Decimal>> mTransactionState;
    std::list<std::reference_wrapper<StrategyTransactionObserver<Decimal>>> mObservers;
  };

  //
  // class StrategyTransactionState
  //

  template <class Decimal> class StrategyTransactionState
  {
  public:
    StrategyTransactionState()
    {}

    StrategyTransactionState (const StrategyTransactionState<Decimal>& rhs)
    {}

    StrategyTransactionState<Decimal>& 
    operator=(const StrategyTransactionState<Decimal> &rhs)
    {
      return *this;
    }

    virtual ~StrategyTransactionState()
    {}

    virtual std::shared_ptr<TradingOrder<Decimal>> getExitTradingOrder() const = 0;
    virtual bool isTransactionOpen() const = 0;
    virtual bool isTransactionComplete() const = 0;
    virtual void completeTransaction (StrategyTransaction<Decimal> *transaction,
				      std::shared_ptr<TradingOrder<Decimal>> exitOrder) = 0;
  };


 //
  // class StrategyTransactionStateComplete
  //

  template <class Decimal> class StrategyTransactionStateComplete : public StrategyTransactionState<Decimal>
  {
  public:
    StrategyTransactionStateComplete(std::shared_ptr<TradingOrder<Decimal>> exitOrder)
      : StrategyTransactionState<Decimal>(),
	mExitOrder (exitOrder)
    {}

    StrategyTransactionStateComplete (const StrategyTransactionStateComplete<Decimal>& rhs)
      : StrategyTransactionState<Decimal> (rhs),
	mExitOrder(rhs.mExitOrder)
    {}

    StrategyTransactionStateComplete<Decimal>& 
    operator=(const StrategyTransactionStateComplete<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StrategyTransactionState<Decimal>::operator=(rhs);
      mExitOrder = rhs.mExitOrder;
      return *this;
    }

    ~StrategyTransactionStateComplete()
    {}

    std::shared_ptr<TradingOrder<Decimal>> getExitTradingOrder() const
    {
      return mExitOrder;
    }

    bool isTransactionOpen() const
    {
      return false;
    }

    bool isTransactionComplete() const
    {
      return true;
    }

    void completeTransaction (StrategyTransaction<Decimal> *transaction,
			      std::shared_ptr<TradingOrder<Decimal>> exitOrder)
    {
      throw StrategyTransactionException ("StrategyTransactionStateComplete::completeTransaction - transaction already complete");
    }

  private:
    std::shared_ptr<TradingOrder<Decimal>> mExitOrder;
  };

  //
  // class StrategyTransactionStateOpen
  //

  template <class Decimal> class StrategyTransactionStateOpen : public StrategyTransactionState<Decimal>
  {
  public:
    StrategyTransactionStateOpen()
      : StrategyTransactionState<Decimal>()
    {}

    StrategyTransactionStateOpen (const StrategyTransactionStateOpen<Decimal>& rhs)
      : StrategyTransactionState<Decimal> (rhs)
    {}

    StrategyTransactionStateOpen<Decimal>& 
    operator=(const StrategyTransactionStateOpen<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StrategyTransactionState<Decimal>::operator=(rhs);
      return *this;
    }

    ~StrategyTransactionStateOpen()
    {}


    bool isTransactionOpen() const
    {
      return true;
    }

    bool isTransactionComplete() const
    {
      return false;
    }

    std::shared_ptr<TradingOrder<Decimal>> getExitTradingOrder() const
    {
      throw StrategyTransactionException ("StrategyTransactionStateOpen:: No exit order available while position is open");
    }

    void completeTransaction (StrategyTransaction<Decimal> *transaction,
			      std::shared_ptr<TradingOrder<Decimal>> exitOrder)
    {
      transaction->ChangeState (std::make_shared<StrategyTransactionStateComplete<Decimal>>(exitOrder));
    }

  };

 

  template <class Decimal>
  inline StrategyTransaction<Decimal>::StrategyTransaction (std::shared_ptr<TradingOrder<Decimal>> entryOrder,
							 std::shared_ptr<TradingPosition<Decimal>> aPosition)
    : mEntryOrder (entryOrder),
      mPosition(aPosition),
      mTransactionState(new StrategyTransactionStateOpen<Decimal>()),
      mObservers()
  {
    if (entryOrder->getTradingSymbol() != aPosition->getTradingSymbol())
      {
	std::string orderSymbol(entryOrder->getTradingSymbol());
	std::string positionSymbol(aPosition->getTradingSymbol());
	throw StrategyTransactionException ("StrategyTransaction constructor - trading symbols for order " +orderSymbol +" differs from position symbol " +positionSymbol);
      }

    if ((entryOrder->isLongOrder() == aPosition->isLongPosition()) ||
	(entryOrder->isShortOrder() == aPosition->isShortPosition()))
      ;
    else
      throw StrategyTransactionException ("StrategyTransaction constructor -order and position direction do not agree");
  }

  template <class Decimal>
  bool StrategyTransaction<Decimal>::isTransactionOpen() const
  {
    return  mTransactionState->isTransactionOpen();
  }

  template <class Decimal>
  bool StrategyTransaction<Decimal>::isTransactionComplete() const
  {
    return mTransactionState->isTransactionComplete();
  }
  
  template <class Decimal>
  void StrategyTransaction<Decimal>::completeTransaction (std::shared_ptr<TradingOrder<Decimal>> exitOrder)
  {
    mTransactionState->completeTransaction (this, exitOrder);;
    notifyTransactionComplete();
  }

  template <class Decimal>
  std::shared_ptr<TradingOrder<Decimal>> StrategyTransaction<Decimal>::getExitTradingOrder() const
  {
    return mTransactionState->getExitTradingOrder();
  }

}



#endif
