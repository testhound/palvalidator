// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STRATEGY_TRANSACTION_MANAGER_H
#define __STRATEGY_TRANSACTION_MANAGER_H 1

#include <memory>
#include <cstdint>
#include <map>
#include "StrategyTransaction.h"

using std::shared_ptr;
using boost::gregorian::date;
using std::multimap;

namespace mkc_timeseries
{
  //
  // class StrategyTransactionManagerException
  //
  template <class Decimal> class StrategyTransactionManager;

  class StrategyTransactionManagerException : public std::runtime_error
  {
  public:
    StrategyTransactionManagerException(const std::string msg) 
      : std::runtime_error(msg)
    {}
    
    ~StrategyTransactionManagerException()
    {}
    
  };

  //
  // class StrategyTransactionManager
  //

  /**
   * @class StrategyTransactionManager
   * @brief Manages and tracks a collection of strategy transactions generated during a backtest or live trading.
   * @tparam Decimal The numeric type used for financial calculations (e.g., prices, P/L).
   *
   * @details This class acts as a central repository for all `StrategyTransaction` objects created by a strategy,
   * typically via the `StrategyBroker`. It stores transactions indexed by their associated `TradingPosition` ID
   * and also provides a view sorted by the position's entry date.
   *
   * It implements the `StrategyTransactionObserver` interface to monitor the completion status
   * of individual open transactions, allowing it to maintain counts of total, open, and closed trades.
   *
   * Key Responsibilities:
   *
   * - Storing `StrategyTransaction` objects using shared pointers.
   * - Providing access to transactions via position ID lookup (`findStrategyTransaction`).
   * - Providing iterators to traverse transactions sorted by entry date
   * - Tracking the number of total, open, and completed transactions.
   * - Observing `StrategyTransaction` objects to update counts when they are completed.
   *
   * Collaborations:
   *
   * - StrategyTransaction: Aggregates `shared_ptr<StrategyTransaction>`. Calls methods like
   * `getTradingPosition()`, `isTransactionOpen()`, and `addObserver()` on `StrategyTransaction` objects. It receives
   * notifications from observed transactions via the `TransactionComplete` method.
   *
   * - StrategyTransactionObserver: Inherits from this interface and implements the `TransactionComplete`
   * method to react to transaction completion events.
   *
   * - TradingPosition: Interacts indirectly via `StrategyTransaction`. Uses `TradingPosition::getPositionID()`
   * as the key for the primary map and `TradingPosition::getEntryDate()` as the key for the sorted multimap.
   *
   * - StrategyBroker: uses this manager to:
   *
   * - Add new transactions (`addStrategyTransaction`) when entry orders are filled.
   * - Retrieve transactions (`findStrategyTransaction`) when exit orders are filled or when associating closed positions
   * with their original transaction in `PositionClosed`.
   * - Query trade counts (`getTotalTrades`, `getOpenTrades`, `getClosedTrades`).
   * - Iterate through sorted transactions (`beginSortedStrategyTransaction`, `endSortedStrategyTransaction`) for reporting.
   * - boost::gregorian::date: Uses Boost dates as keys in the sorted transaction map.
   * - std::map: Used internally (`mTransactionByPositionId`) to store transactions keyed by position ID.
   * - std::multimap: Used internally (`mSortedTransactions`) to store transactions keyed by entry date, allowing multiple entries per date.
   * - std::shared_ptr: Manages the lifecycle of `StrategyTransaction` objects.
   */
  template <class Decimal> class StrategyTransactionManager : public StrategyTransactionObserver<Decimal>
  {
  public:
    typedef typename std::map<uint32_t, shared_ptr<StrategyTransaction<Decimal>>>::const_iterator 
      StrategyTransactionIterator;
    typedef typename multimap<date, shared_ptr<StrategyTransaction<Decimal>>>::const_iterator 
      SortedStrategyTransactionIterator;

  public:
    StrategyTransactionManager()
      : StrategyTransactionObserver<Decimal>(),
	mTotalTransactions(0),
	mCompletedTransactions(0),
	mOpenTransactions(0),
	mTransactionByPositionId(),
	mSortedTransactions()
    {}

    StrategyTransactionManager (const StrategyTransactionManager<Decimal>& rhs)
      : StrategyTransactionObserver<Decimal>(rhs),
	mTotalTransactions(rhs.mTotalTransactions),
	mCompletedTransactions(rhs.mCompletedTransactions),
	mOpenTransactions(rhs.mOpenTransactions),
	mTransactionByPositionId(rhs.mTransactionByPositionId),
	mSortedTransactions(rhs.mSortedTransactions)
    {}

    ~StrategyTransactionManager()
    {}

    StrategyTransactionManager<Decimal>& 
    operator=(const StrategyTransactionManager<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      StrategyTransactionObserver<Decimal>::operator=(rhs);
      mTotalTransactions = rhs.mTotalTransactions;
      mCompletedTransactions = rhs.mCompletedTransactions;
      mOpenTransactions = rhs.mOpenTransactions;
      mTransactionByPositionId = rhs.mTransactionByPositionId;
      mSortedTransactions = rhs.mSortedTransactions;
      return *this;
    }

    /**
     * @brief Callback function invoked when an observed StrategyTransaction is completed.
     * @param transaction Pointer to the completed StrategyTransaction object.
     * @details This method increments the completed count and decrements the open count.
     * It is called because this manager registers itself as an observer on open transactions
     * when they are added via `addStrategyTransaction`.
     * Implements the `StrategyTransactionObserver` interface.
     */
    void TransactionComplete (StrategyTransaction<Decimal> *transaction)
    {
      mCompletedTransactions++;

      if (mOpenTransactions > 0)
	mOpenTransactions--;
    }

    /**
     * @brief Adds a new strategy transaction to the manager.
     * @param transaction A shared pointer to the StrategyTransaction to add.
     * @throws StrategyTransactionManagerException if a transaction with the same position ID already exists.
     * @details Stores the transaction in both the ID-keyed map and the date-sorted multimap.
     * If the transaction represents an open position (`isTransactionOpen()` returns true),
     * this manager registers itself as an observer (`transaction->addObserver(*this)`)
     * to be notified via `TransactionComplete` when the transaction is closed.
     * Updates the total and open transaction counts.
     */
    void addStrategyTransaction (shared_ptr<StrategyTransaction<Decimal>> transaction)
    {
      StrategyTransactionIterator it = 
	mTransactionByPositionId.find(transaction->getTradingPosition()->getPositionID());
      if (it == endStrategyTransaction())
	{  
	  mTransactionByPositionId.insert (std::make_pair(transaction->getTradingPosition()->getPositionID(), 
							  transaction));
	  mSortedTransactions.insert(std::make_pair(transaction->getTradingPosition()->getEntryDate(), transaction));
	  if (transaction->isTransactionOpen())
	    {
	      // We want to observe when the transaction is complete so
	      // we can log it

	      transaction->addObserver (*this);
	      mOpenTransactions++;
	    }

	  mTotalTransactions++;
	}
      else
	{
	  std::string positionIDString(std::to_string (transaction->getTradingPosition()->getPositionID()));
	  throw StrategyTransactionManagerException ("StrategyTransactionManager::addStrategyTransaction - Position ID " + positionIDString + " already exists");
	}
    }

    /**
     * @brief Gets the total number of transactions added (both open and closed).
     * @return The total transaction count.
     */
    uint32_t getTotalTrades() const
    {
      return mTotalTransactions;
    }

    /**
     * @brief Gets the number of currently open transactions.
     * @return The open transaction count.
     */
    uint32_t getOpenTrades() const
    {
      return mOpenTransactions;
    }

    /**
     * @brief Gets the number of completed/closed transactions.
     * @return The closed transaction count.
     */
    uint32_t getClosedTrades() const
    {
      return mCompletedTransactions;
    }

    /**
     * @brief Finds a transaction by its associated TradingPosition ID.
     * @param positionIDKey The unique ID of the TradingPosition.
     * @return A `StrategyTransactionIterator` pointing to the transaction if found,
     * or `endStrategyTransaction()` if not found.
     */
    StrategyTransactionIterator findStrategyTransaction (uint32_t positionIDKey) const
    {
      return mTransactionByPositionId.find(positionIDKey);
    }

    /**
     * @brief Returns an iterator to the beginning of the transactions map (keyed by position ID).
     * @return A `StrategyTransactionIterator`.
     */
    StrategyTransactionIterator beginStrategyTransaction() const
    {
      return mTransactionByPositionId.begin();
    }

    /**
     * @brief Returns an iterator to the end of the transactions map (keyed by position ID).
     * @return A `StrategyTransactionIterator`.
     */
    StrategyTransactionIterator endStrategyTransaction() const
    {
      return mTransactionByPositionId.end();
    }

    /**
     * @brief Returns an iterator to the beginning of the date-sorted transactions multimap.
     * @return A `SortedStrategyTransactionIterator`.
     */
    SortedStrategyTransactionIterator beginSortedStrategyTransaction() const
    {
      return mSortedTransactions.begin();
    }

    /**
     * @brief Returns an iterator to the beginning of the date-sorted transactions multimap.
     * @return A `SortedStrategyTransactionIterator`.
     */
    SortedStrategyTransactionIterator endSortedStrategyTransaction() const
    {
      return mSortedTransactions.end();
    }

  private:
    uint32_t mTotalTransactions;
    uint32_t mCompletedTransactions;
    uint32_t mOpenTransactions;

    std::map<uint32_t, shared_ptr<StrategyTransaction<Decimal>>> mTransactionByPositionId;
 
    // Sorted transactions; sorted by position entry date. Need multimap, because we can have multiple
    // positions entered on the same date
    std::multimap< boost::gregorian::date, shared_ptr<StrategyTransaction<Decimal>>> mSortedTransactions;
  };
}
#endif
