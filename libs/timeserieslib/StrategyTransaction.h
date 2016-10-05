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

  template <int Prec> class StrategyTransaction;
  template <int Prec> class StrategyTransactionState;
  template <int Prec> class StrategyTransactionStateOpen;

  template <int Prec>
  class StrategyTransactionObserver
  {
  public:
    StrategyTransactionObserver()
    {}

    StrategyTransactionObserver (const StrategyTransactionObserver<Prec>& rhs)
      {}

    virtual ~StrategyTransactionObserver()
    {}

    StrategyTransactionObserver<Prec>& 
    operator=(const StrategyTransactionObserver<Prec> &rhs)
    {
      return *this;
    }

    virtual void TransactionComplete (StrategyTransaction<Prec> *transaction) = 0;
  };


  //
  // class StrategyTransaction
  //
  // A strategy transaction consists of
  //
  // 1. A entry TradingOrder
  // 2. A TradingPosition
  // 3. A exit TradingOrder

  template <int Prec> class StrategyTransaction
  {
  public:
    StrategyTransaction (std::shared_ptr<TradingOrder<Prec>> entryOrder,
			 std::shared_ptr<TradingPosition<Prec>> aPosition);

    StrategyTransaction (const StrategyTransaction<Prec>& rhs)
      : mEntryOrder(rhs.mEntryOrder),
	mPosition(rhs.mPosition),
	mTransactionState(rhs.mTransactionState),
	mObservers(rhs.mObservers)
    {}

    StrategyTransaction<Prec>& 
    operator=(const StrategyTransaction<Prec> &rhs)
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

    std::shared_ptr<TradingOrder<Prec>> getEntryTradingOrder() const
    {
      return mEntryOrder;
    }

    std::shared_ptr<TradingPosition<Prec>> getTradingPosition() const
    {
      return mPosition;
    }

    std::shared_ptr<TradingPosition<Prec>>
    getTradingPositionPtr() const
    {
      return mPosition;
    }

    std::shared_ptr<TradingOrder<Prec>> getExitTradingOrder() const;

    bool isTransactionOpen() const;

    bool isTransactionComplete() const;

    void completeTransaction (std::shared_ptr<TradingOrder<Prec>> exitOrder);

    void addObserver (reference_wrapper<StrategyTransactionObserver<Prec>> observer)
    {
      mObservers.push_back(observer);
    }

  private:
    typedef typename list<reference_wrapper<StrategyTransactionObserver<Prec>>>::const_iterator ConstObserverIterator;

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
    void ChangeState (std::shared_ptr<StrategyTransactionState<Prec>> newState)
    {
      mTransactionState = newState;
    }

    friend class StrategyTransactionState<Prec>;
    friend class StrategyTransactionStateOpen<Prec>;

  private:
    std::shared_ptr<TradingOrder<Prec>> mEntryOrder;
    std::shared_ptr<TradingPosition<Prec>> mPosition;
    std::shared_ptr<StrategyTransactionState<Prec>> mTransactionState;
    std::list<std::reference_wrapper<StrategyTransactionObserver<Prec>>> mObservers;
  };

  //
  // class StrategyTransactionState
  //

  template <int Prec> class StrategyTransactionState
  {
  public:
    StrategyTransactionState()
    {}

    StrategyTransactionState (const StrategyTransactionState<Prec>& rhs)
    {}

    StrategyTransactionState<Prec>& 
    operator=(const StrategyTransactionState<Prec> &rhs)
    {
      return *this;
    }

    virtual ~StrategyTransactionState()
    {}

    virtual std::shared_ptr<TradingOrder<Prec>> getExitTradingOrder() const = 0;
    virtual bool isTransactionOpen() const = 0;
    virtual bool isTransactionComplete() const = 0;
    virtual void completeTransaction (StrategyTransaction<Prec> *transaction,
				      std::shared_ptr<TradingOrder<Prec>> exitOrder) = 0;
  };


 //
  // class StrategyTransactionStateComplete
  //

  template <int Prec> class StrategyTransactionStateComplete : public StrategyTransactionState<Prec>
  {
  public:
    StrategyTransactionStateComplete(std::shared_ptr<TradingOrder<Prec>> exitOrder)
      : StrategyTransactionState<Prec>(),
	mExitOrder (exitOrder)
    {}

    StrategyTransactionStateComplete (const StrategyTransactionStateComplete<Prec>& rhs)
      : StrategyTransactionState<Prec> (rhs),
	mExitOrder(rhs.mExitOrder)
    {}

    StrategyTransactionStateComplete<Prec>& 
    operator=(const StrategyTransactionStateComplete<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StrategyTransactionState<Prec>::operator=(rhs);
      mExitOrder = rhs.mExitOrder;
      return *this;
    }

    ~StrategyTransactionStateComplete()
    {}

    std::shared_ptr<TradingOrder<Prec>> getExitTradingOrder() const
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

    void completeTransaction (StrategyTransaction<Prec> *transaction,
			      std::shared_ptr<TradingOrder<Prec>> exitOrder)
    {
      throw StrategyTransactionException ("StrategyTransactionStateComplete::completeTransaction - transaction already complete");
    }

  private:
    std::shared_ptr<TradingOrder<Prec>> mExitOrder;
  };

  //
  // class StrategyTransactionStateOpen
  //

  template <int Prec> class StrategyTransactionStateOpen : public StrategyTransactionState<Prec>
  {
  public:
    StrategyTransactionStateOpen()
      : StrategyTransactionState<Prec>()
    {}

    StrategyTransactionStateOpen (const StrategyTransactionStateOpen<Prec>& rhs)
      : StrategyTransactionState<Prec> (rhs)
    {}

    StrategyTransactionStateOpen<Prec>& 
    operator=(const StrategyTransactionStateOpen<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;
	
      StrategyTransactionState<Prec>::operator=(rhs);
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

    std::shared_ptr<TradingOrder<Prec>> getExitTradingOrder() const
    {
      throw StrategyTransactionException ("StrategyTransactionStateOpen:: No exit order available while position is open");
    }

    void completeTransaction (StrategyTransaction<Prec> *transaction,
			      std::shared_ptr<TradingOrder<Prec>> exitOrder)
    {
      transaction->ChangeState (std::make_shared<StrategyTransactionStateComplete<Prec>>(exitOrder));
    }

  };

 

  template <int Prec>
  inline StrategyTransaction<Prec>::StrategyTransaction (std::shared_ptr<TradingOrder<Prec>> entryOrder,
							 std::shared_ptr<TradingPosition<Prec>> aPosition)
    : mEntryOrder (entryOrder),
      mPosition(aPosition),
      mTransactionState(new StrategyTransactionStateOpen<Prec>()),
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

  template <int Prec>
  bool StrategyTransaction<Prec>::isTransactionOpen() const
  {
    return  mTransactionState->isTransactionOpen();
  }

  template <int Prec>
  bool StrategyTransaction<Prec>::isTransactionComplete() const
  {
    return mTransactionState->isTransactionComplete();
  }
  
  template <int Prec>
  void StrategyTransaction<Prec>::completeTransaction (std::shared_ptr<TradingOrder<Prec>> exitOrder)
  {
    mTransactionState->completeTransaction (this, exitOrder);;
    notifyTransactionComplete();
  }

  template <int Prec>
  std::shared_ptr<TradingOrder<Prec>> StrategyTransaction<Prec>::getExitTradingOrder() const
  {
    return mTransactionState->getExitTradingOrder();
  }

}



#endif
