// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
// IMPROVED VERSION - Incorporates modern C++ best practices

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
  template <class Decimal> class StrategyTransactionStateComplete;

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
   * IMPROVED VERSION NOTES:
   * - Added move semantics (move constructor and move assignment)
   * - Added noexcept specifications
   * - Fixed memory leak in constructor (use make_shared)
   * - Added null pointer validation
   * - Changed observer copying behavior (observers not copied)
   * - Added equality operators
   * - Restored getTradingPositionPtr() for compatibility
   * - Improved exception safety in observer notifications
   *
   * Key Responsibilities:
   * - Linking entry order, position, and exit order.
   * - Managing the transaction's state (Open/Complete) via the State pattern.
   * - Notifying registered observers upon completion via the Observer pattern.
   * - Providing access to the constituent entry order, position, and (once complete) exit order.
   *
   * Thread Safety: This class is NOT thread-safe. External synchronization required for concurrent access.
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
     * @throws StrategyTransactionException if:
     *   - entryOrder is null
     *   - aPosition is null
     *   - the trading symbols do not match
     *   - the direction (long/short) of order and position do not match
     */
    StrategyTransaction (std::shared_ptr<TradingOrder<Decimal>> entryOrder,
                         std::shared_ptr<TradingPosition<Decimal>> aPosition);

    /**
     * @brief Copy constructor - creates a shallow copy with independent state and NO observers.
     * @details The copied transaction shares the same entry order and position objects
     * but has its own state management and no observers (observers are not copied).
     * This allows the copy to transition through states independently.
     */
    StrategyTransaction(const StrategyTransaction& rhs)
      : mEntryOrder(rhs.mEntryOrder),
	mPosition(rhs.mPosition),
	mTransactionState(/* NEW state based on rhs */),
	mObservers()
    {
      if (rhs.isTransactionOpen())
	mTransactionState = std::make_shared<StrategyTransactionStateOpen<Decimal>>();
      else
	mTransactionState = std::make_shared<StrategyTransactionStateComplete<Decimal>>(rhs.getExitTradingOrder());
    }

    /**
     * @brief Move constructor - transfers ownership efficiently.
     * @param rhs The transaction to move from (left in valid but unspecified state).
     */
    StrategyTransaction(StrategyTransaction<Decimal>&& rhs) noexcept
      : mEntryOrder(std::move(rhs.mEntryOrder)),
        mPosition(std::move(rhs.mPosition)),
        mTransactionState(std::move(rhs.mTransactionState)),
        mObservers() // IMPORTANT: observers are wiring; never move them
    {}

    /**
     * @brief Copy assignment operator using copy-and-swap idiom.
     * @param rhs The transaction to copy from.
     * @return Reference to this transaction.
     */
    StrategyTransaction<Decimal>&
    operator=(const StrategyTransaction<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;
      
      // Copy the shared "identity" parts (you chose to share these)
      mEntryOrder = rhs.mEntryOrder;
      mPosition   = rhs.mPosition;

      // Rebuild independent state (do NOT share the state object)
      if (rhs.isTransactionOpen())
	{
	  mTransactionState = std::make_shared<StrategyTransactionStateOpen<Decimal>>();
	}
      else
	{
	  // rhs is complete; capture its exit order to preserve completed state
	  // This will throw if rhs is open, but we are in the 'else' branch.
	  mTransactionState =
	    std::make_shared<StrategyTransactionStateComplete<Decimal>>(rhs.getExitTradingOrder());
	}
      
      // Observers are wiring; never copy them
      mObservers.clear();
      
      return *this;
    }

    void clearObservers() noexcept
    {
      mObservers.clear();
    }

    void removeObserver(StrategyTransactionObserver<Decimal>& observer)
    {
      StrategyTransactionObserver<Decimal>* target = &observer;

      mObservers.remove_if(
			   [target](const std::reference_wrapper<StrategyTransactionObserver<Decimal>>& wrapped) -> bool
			   {
			     return &wrapped.get() == target;
			   });
    }

    bool hasObserver(StrategyTransactionObserver<Decimal>& observer) const
    {
      StrategyTransactionObserver<Decimal>* target = &observer;

      for (const auto& wrapped : mObservers)
	{
	  if (&wrapped.get() == target)
	    return true;
	}
      return false;
    }

    void addObserverUnique(StrategyTransactionObserver<Decimal>& observer)
    {
      if (!hasObserver(observer))
	mObservers.push_back(observer);
    }

    /**
     * @brief Move assignment operator - transfers ownership efficiently.
     * @param rhs The transaction to move from.
     * @return Reference to this transaction.
     */
    StrategyTransaction<Decimal>& operator=(StrategyTransaction<Decimal>&& rhs) noexcept
    {
      if (this != &rhs)
	{
	  mEntryOrder = std::move(rhs.mEntryOrder);
	  mPosition = std::move(rhs.mPosition);
	  mTransactionState = std::move(rhs.mTransactionState);
	  
	  // IMPORTANT: observers are wiring; never move them
	  mObservers.clear();
	}
      return *this;
    }

    ~StrategyTransaction() noexcept = default;

    /**
     * @brief Equality comparison based on entry order and position.
     * @param rhs The transaction to compare with.
     * @return true if both transactions reference the same order and position.
     */
    bool operator==(const StrategyTransaction<Decimal>& rhs) const
    {
      return mEntryOrder == rhs.mEntryOrder && mPosition == rhs.mPosition;
    }

    /**
     * @brief Inequality comparison.
     * @param rhs The transaction to compare with.
     * @return true if transactions differ.
     */
    bool operator!=(const StrategyTransaction<Decimal>& rhs) const
    {
      return !(*this == rhs);
    }

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

    /**
     * @brief Gets the trading position associated with this transaction (alternative name for compatibility).
     * @return A shared pointer to the TradingPosition.
     */
    std::shared_ptr<TradingPosition<Decimal>> getTradingPositionPtr() const
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
     * Observer notifications are exception-safe - an exception from one observer won't prevent others from being notified.
     */
    void completeTransaction (std::shared_ptr<TradingOrder<Decimal>> exitOrder);

    /**
     * @brief Adds an observer to be notified when this transaction completes.
     * @param observer A reference wrapper around an object implementing `StrategyTransactionObserver`.
     * The observer's lifetime must be managed externally and exceed that of this transaction
     * (or until the observer is removed via `removeObserver`, if implemented).
     * @details The same observer can be added multiple times; it will be notified once per addition.
     */
    void addObserver (std::reference_wrapper<StrategyTransactionObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

    // TODO: Consider implementing removeObserver for completeness
    // void removeObserver(std::reference_wrapper<StrategyTransactionObserver<Decimal>> observer);

  private:
    /**
     * @brief Changes the internal state of this transaction.
     * @param newState Shared pointer to the new state object.
     * @details This method is called by state classes (via friend relationship) during state transitions.
     */
    void ChangeState (std::shared_ptr<StrategyTransactionState<Decimal>> newState)
    {
      mTransactionState = newState;
    }

    /**
     * @brief Notifies all registered observers that this transaction has completed.
     * @details Exception-safe: If one observer throws, other observers are still notified.
     * The first exception encountered is rethrown after all observers have been processed.
     */
    void notifyTransactionComplete()
    {
      std::exception_ptr firstException;
      
      for (auto& observer : mObservers)
	{
	  try {
	    observer.get().TransactionComplete(this);
	  }
	  catch (...)
	    {
	      if (!firstException) {
		firstException = std::current_exception();
	      }
	      // Continue notifying other observers
	    }
	}
      
      // Rethrow first exception if any occurred
      if (firstException) {
        std::rethrow_exception(firstException);
      }
    }

  private:
    std::shared_ptr<TradingOrder<Decimal>> mEntryOrder;
    std::shared_ptr<TradingPosition<Decimal>> mPosition;
    std::shared_ptr<StrategyTransactionState<Decimal>> mTransactionState;
    std::list<std::reference_wrapper<StrategyTransactionObserver<Decimal>>> mObservers;

    friend class StrategyTransactionStateOpen<Decimal>;
  };

  /**
   * @class StrategyTransactionState
   * @brief Abstract base class for StrategyTransaction states using the State pattern.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Defines the interface for state-dependent behavior of a `StrategyTransaction`.
   * Concrete states (`StrategyTransactionStateOpen`, `StrategyTransactionStateComplete`)
   * implement this interface to provide specific behavior for open and completed transactions.
   *
   * Collaborations:
   * - StrategyTransaction: Holds a `shared_ptr` to a state object and delegates state-dependent methods.
   * - StrategyTransactionStateOpen, StrategyTransactionStateComplete: Concrete implementations.
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

    /**
     * @brief Gets the exit trading order for this transaction.
     * @return Shared pointer to the exit TradingOrder.
     * @throws StrategyTransactionException if called on a state that doesn't have an exit order (e.g., Open).
     */
    virtual std::shared_ptr<TradingOrder<Decimal>> getExitTradingOrder() const = 0;

    /**
     * @brief Checks if the transaction is in an open state.
     * @return `true` if open, `false` otherwise.
     */
    virtual bool isTransactionOpen() const = 0;

    /**
     * @brief Checks if the transaction is in a complete state.
     * @return `true` if complete, `false` otherwise.
     */
    virtual bool isTransactionComplete() const = 0;

    /**
     * @brief Attempts to complete the transaction with an exit order.
     * @param transaction Pointer to the owning `StrategyTransaction`.
     * @param exitOrder Shared pointer to the exit `TradingOrder`.
     * @throws StrategyTransactionException if the operation is invalid for the current state.
     */
    virtual void completeTransaction (StrategyTransaction<Decimal> *transaction,
				      std::shared_ptr<TradingOrder<Decimal>> exitOrder) = 0;
  };

  /**
   * @class StrategyTransactionStateComplete
   * @brief Concrete state representing a completed StrategyTransaction.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Implements the `StrategyTransactionState` interface for a transaction
   * that has been closed by an exit order. In this state, attempts to complete the
   * transaction again will throw an exception.
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

    StrategyTransactionStateComplete(StrategyTransactionStateComplete<Decimal>&& rhs) noexcept
      : StrategyTransactionState<Decimal>(std::move(rhs)),
        mExitOrder(std::move(rhs.mExitOrder))
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

    StrategyTransactionStateComplete<Decimal>& 
    operator=(StrategyTransactionStateComplete<Decimal>&& rhs) noexcept
    {
      if (this == &rhs)
        return *this;
        
      StrategyTransactionState<Decimal>::operator=(std::move(rhs));
      mExitOrder = std::move(rhs.mExitOrder);
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
    void completeTransaction (StrategyTransaction<Decimal> * /* transaction */,
			      std::shared_ptr<TradingOrder<Decimal>> /* exitOrder */)
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

    StrategyTransactionStateOpen(StrategyTransactionStateOpen<Decimal>&& rhs) noexcept
      : StrategyTransactionState<Decimal>(std::move(rhs))
    {}

    StrategyTransactionStateOpen<Decimal>& 
    operator=(const StrategyTransactionStateOpen<Decimal> &rhs)
    {
      if (this == &rhs)
        return *this;
        
      StrategyTransactionState<Decimal>::operator=(rhs);
      return *this;
    }

    StrategyTransactionStateOpen<Decimal>& 
    operator=(StrategyTransactionStateOpen<Decimal>&& rhs) noexcept
    {
      if (this == &rhs)
        return *this;
        
      StrategyTransactionState<Decimal>::operator=(std::move(rhs));
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
     * @throws StrategyTransactionException if transaction pointer is null.
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

  // Template method implementations

  template <class Decimal>
  inline StrategyTransaction<Decimal>::StrategyTransaction (std::shared_ptr<TradingOrder<Decimal>> entryOrder,
							    std::shared_ptr<TradingPosition<Decimal>> aPosition)
    : mEntryOrder (entryOrder),
      mPosition(aPosition),
      mTransactionState(std::make_shared<StrategyTransactionStateOpen<Decimal>>()),  // FIXED: use make_shared
      mObservers()
  {
    // ADDED: Null pointer validation
    if (!entryOrder)
      throw StrategyTransactionException("StrategyTransaction constructor - null entryOrder provided");
    
    if (!aPosition)
      throw StrategyTransactionException("StrategyTransaction constructor - null aPosition provided");

    // Existing validation
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
    mTransactionState->completeTransaction (this, exitOrder);
    notifyTransactionComplete();
  }

  template <class Decimal>
  std::shared_ptr<TradingOrder<Decimal>> StrategyTransaction<Decimal>::getExitTradingOrder() const
  {
    return mTransactionState->getExitTradingOrder();
  }

}



#endif
