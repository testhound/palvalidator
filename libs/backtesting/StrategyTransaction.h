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

  /**
   * @interface StrategyTransactionObserver
   * @brief Defines the interface for observers interested in StrategyTransaction completion events.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Classes that need to be notified when a `StrategyTransaction` is completed
   * (i.e., when an exit order is associated with it) should inherit from this class
   * and implement the `TransactionComplete` method. The observer instance should then
   * be registered with the `StrategyTransaction` object using its `addObserver` method.
   *
   * Collaborations:
   *
   * - StrategyTransaction: Holds a list of references to `StrategyTransactionObserver`
   * objects and calls `TransactionComplete` on them when the transaction is finalized.
   * - StrategyTransactionManager<Decimal> (Typical implementor): Often inherits from this
   * interface to track completed trades.
   */
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

    /**
     * @brief Pure virtual callback method invoked when an observed StrategyTransaction completes.
     * @param transaction A pointer to the `StrategyTransaction` object that has just completed.
     * Implementations should not delete this pointer as its lifetime is
     * managed elsewhere (typically by `StrategyTransactionManager` via shared_ptr).
     */
    virtual void TransactionComplete (StrategyTransaction<Decimal> *transaction) = 0;
  };


  /**
   * @class StrategyTransaction
   * @brief Represents a complete trading cycle, linking an entry order, the resulting position, and the eventual exit order.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details This class encapsulates a single round-trip trade. It is initialized with the
   * entry `TradingOrder` and the corresponding `TradingPosition`. It uses the State pattern
   * (`StrategyTransactionState`) to manage its lifecycle (Open or Complete). It also implements
   * the Observer pattern, allowing other components (like `StrategyTransactionManager`) to be
   * notified via `StrategyTransactionObserver` when the transaction is finalized by adding
   * an exit `TradingOrder`.
   *
   * Key Responsibilities:
   * - Linking entry order, position, and exit order.
   * - Managing the transaction's state (Open/Complete) via the State pattern.
   * - Notifying registered observers upon completion via the Observer pattern.
   * - Providing access to the constituent entry order, position, and (once complete) exit order.
   *
   * Collaborations:
   * - TradingOrder: Holds `shared_ptr`s to the entry order and (when completed) the exit order.
   * - TradingPosition: Holds a `shared_ptr` to the position created by the entry order.
   * - StrategyTransactionState<Decimal> (and subclasses `...Open`, `...Complete`): Uses the State pattern.
   * Holds a `shared_ptr` to the current state object (`mTransactionState`) and delegates state-dependent
   * operations (`isTransactionOpen`, `isTransactionComplete`, `getExitTradingOrder`, `completeTransaction`) to it.
   * - StrategyTransactionObserver: Manages a list (`mObservers`) of observers (via `std::reference_wrapper`)
   * and notifies them using `notifyTransactionComplete` when `completeTransaction` is called.
   * - StrategyTransactionManager<Decimal> (Usage Context): Typically observes `StrategyTransaction` objects.
   * - StrategyBroker<Decimal> (Usage Context): Typically creates `StrategyTransaction` instances and calls
   * `completeTransaction` when exit orders are filled.
   * - std::shared_ptr: Manages the lifecycle of orders, positions, and state objects.
   * - std::list, std::reference_wrapper: Used for managing observers.
   */
  template <class Decimal> class StrategyTransaction
  {
  public:
    /**
     * @brief Constructs a StrategyTransaction in the 'Open' state.
     * @param entryOrder Shared pointer to the TradingOrder that initiated the position.
     * @param aPosition Shared pointer to the TradingPosition resulting from the entry order.
     * @throws StrategyTransactionException if the trading symbols or direction (long/short)
     * of the entry order and position do not match, or if pointers are null.
     */
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

    /**
     * @brief Gets the entry trading order that initiated this transaction.
     * @return A shared pointer to the entry TradingOrder.
     */
    std::shared_ptr<TradingOrder<Decimal>> getEntryTradingOrder() const
    {
      return mEntryOrder;
    }

    /**
     * @brief Gets the trading position associated with this transaction.
     * @return A shared pointer to the TradingPosition.
     */
    std::shared_ptr<TradingPosition<Decimal>> getTradingPosition() const
    {
      return mPosition;
    }

    
    std::shared_ptr<TradingPosition<Decimal>>
    getTradingPositionPtr() const
    {
      return mPosition;
    }

    /**
     * @brief Gets the exit trading order that completed this transaction.
     * @return A shared pointer to the exit TradingOrder.
     * @throws StrategyTransactionException if the transaction is still open (state is `StrategyTransactionStateOpen`).
     * @details Delegates to the current `mTransactionState`.
     */
    std::shared_ptr<TradingOrder<Decimal>> getExitTradingOrder() const;

    /**
     * @brief Checks if the transaction is currently open (position exists, no exit order yet).
     * @return `true` if the transaction is open, `false` otherwise.
     * @details Delegates to the current `mTransactionState`.
     */
    bool isTransactionOpen() const;

    /**
     * @brief Checks if the transaction is complete (has an associated exit order).
     * @return `true` if the transaction is complete, `false` otherwise.
     * @details Delegates to the current `mTransactionState`.
     */
    bool isTransactionComplete() const;

    /**
     * @brief Completes the transaction by associating an exit order and changing state.
     * @param exitOrder Shared pointer to the TradingOrder that closed the position.
     * @throws StrategyTransactionException if the transaction is already complete (state is `StrategyTransactionStateComplete`).
     * @details Delegates the state change to the current `mTransactionState` (specifically handled by `StrategyTransactionStateOpen`).
     * After the state change, it calls `notifyTransactionComplete()` to inform observers.
     */
    void completeTransaction (std::shared_ptr<TradingOrder<Decimal>> exitOrder);

    /**
     * @brief Adds an observer to be notified when this transaction completes.
     * @param observer A reference wrapper around an object implementing `StrategyTransactionObserver`.
     * The observer's lifetime must be managed externally and exceed that of this transaction
     * or until it is implicitly removed when the transaction is destroyed.
     */
    void addObserver (reference_wrapper<StrategyTransactionObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

  private:
    typedef typename list<reference_wrapper<StrategyTransactionObserver<Decimal>>>::const_iterator ConstObserverIterator;

    /** @brief Returns an iterator to the beginning of the observer list. */
    ConstObserverIterator beginObserverList() const
    {
      return mObservers.begin();
    }

    ConstObserverIterator endObserverList() const
    {
      return mObservers.end();
    }

    /**
     * @brief Notifies all registered observers that the transaction has completed.
     * @details Iterates through the observer list and calls `TransactionComplete(this)` on each one.
     * This is typically called internally by `completeTransaction`.
     */
    void notifyTransactionComplete()
    {
      ConstObserverIterator it = this->beginObserverList();

      for (; it != this->endObserverList(); it++)
	(*it).get().TransactionComplete (this);
    }

    /**
     * @brief Changes the internal state of the transaction. Used by State classes.
     * @param newState A shared pointer to the new StrategyTransactionState.
     * @details This method is friended by the State classes to allow them to manage state transitions.
     */
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

  /**
   * @class StrategyTransactionState
   * @brief Abstract base class for the State pattern applied to StrategyTransaction.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Defines the common interface for all possible states of a `StrategyTransaction`.
   * Concrete states (`StrategyTransactionStateOpen`, `StrategyTransactionStateComplete`)
   * implement this interface to provide state-specific behavior.
   *
   * Collaborations:
   * - StrategyTransaction: Contains a pointer to a `StrategyTransactionState` object.
   * Delegates state-dependent operations to it. Declared as a friend to allow state transitions.
   * - StrategyTransactionStateOpen: Concrete state subclass.
   * - StrategyTransactionStateComplete: Concrete state subclass.
   */
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


  /**
   * @class StrategyTransactionStateComplete
   * @brief Concrete state representing a completed StrategyTransaction.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Implements the `StrategyTransactionState` interface for a transaction
   * that has been closed with an exit order. It stores the exit order.
   *
   * Collaborations:
   * - StrategyTransactionState: Inherits from this abstract base class.
   * - TradingOrder: Holds a `shared_ptr` (`mExitOrder`) to the exit order.
   * - StrategyTransaction: Instantiated by `StrategyTransactionStateOpen` during state transition.
   */
  template <class Decimal> class StrategyTransactionStateComplete : public StrategyTransactionState<Decimal>
  {
  public:
    /**
     * @brief Constructs the Complete state.
     * @param exitOrder Shared pointer to the TradingOrder that closed the position.
     */
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

    /**
     * @brief Checks if the transaction is open. Always returns false for the Complete state.
     * @return `false`.
     */
    bool isTransactionOpen() const
    {
      return false;
    }

    /**
     * @brief Checks if the transaction is complete. Always returns true for the Complete state.
     * @return `true`.
     */
    bool isTransactionComplete() const
    {
      return true;
    }

    /**
     * @brief Attempts to complete the transaction. Throws an exception as it's already complete.
     * @param transaction Pointer to the owning `StrategyTransaction`.
     * @param exitOrder Shared pointer to the exit `TradingOrder`.
     * @throws StrategyTransactionException always.
     */
    void completeTransaction (StrategyTransaction<Decimal> *transaction,
			      std::shared_ptr<TradingOrder<Decimal>> exitOrder)
    {
      throw StrategyTransactionException ("StrategyTransactionStateComplete::completeTransaction - transaction already complete");
    }

  private:
    std::shared_ptr<TradingOrder<Decimal>> mExitOrder;
  };

  
  /**
   * @class StrategyTransactionStateOpen
   * @brief Concrete state representing an open StrategyTransaction.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Implements the `StrategyTransactionState` interface for a transaction
   * that has an entry order and position but is not yet closed by an exit order.
   * This state handles the transition to the `StrategyTransactionStateComplete`.
   *
   * Collaborations:
   * - StrategyTransactionState: Inherits from this abstract base class.
   * - StrategyTransaction: Calls `transaction->ChangeState()` to transition the owning
   * transaction to the Complete state when `completeTransaction` is invoked. Friend relationship
   * allows access to `ChangeState`.
   * - StrategyTransactionStateComplete: Creates an instance of this state during the transition.
   */
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

    /**
     * @brief Checks if the transaction is open. Always returns true for the Open state.
     * @return `true`.
     */
    bool isTransactionOpen() const
    {
      return true;
    }

    /**
     * @brief Checks if the transaction is complete. Always returns false for the Open state.
     * @return `false`.
     */
    bool isTransactionComplete() const
    {
      return false;
    }

    /**
     * @brief Gets the exit trading order. Throws an exception as no exit order exists yet.
     * @return Never returns normally.
     * @throws StrategyTransactionException always.
     */
    std::shared_ptr<TradingOrder<Decimal>> getExitTradingOrder() const
    {
      throw StrategyTransactionException ("StrategyTransactionStateOpen:: No exit order available while position is open");
    }

    /**
     * @brief Completes the transaction by changing the owning transaction's state.
     * @param transaction Pointer to the owning `StrategyTransaction` object.
     * @param exitOrder Shared pointer to the exit `TradingOrder` that closes the position.
     * @details Creates a new `StrategyTransactionStateComplete` instance holding the `exitOrder`
     * and calls `transaction->ChangeState()` to update the owning transaction's state pointer.
     */
    void completeTransaction (StrategyTransaction<Decimal> *transaction,
			      std::shared_ptr<TradingOrder<Decimal>> exitOrder)
    {
      if (!transaction)
	throw StrategyTransactionException("StrategyTransactionStateOpen::completeTransaction - Null transaction pointer provided.");

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
