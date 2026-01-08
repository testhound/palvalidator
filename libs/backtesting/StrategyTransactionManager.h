// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
// IMPROVED VERSION - Incorporates modern C++ best practices

#ifndef __STRATEGY_TRANSACTION_MANAGER_H
#define __STRATEGY_TRANSACTION_MANAGER_H 1

#include <memory>
#include <cstdint>
#include <map>
#include "StrategyTransaction.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using std::shared_ptr;
using boost::gregorian::date;
using std::multimap;
using boost::posix_time::ptime;

namespace mkc_timeseries
{
  //
  // class StrategyTransactionManagerException
  //
  template <class Decimal> class StrategyTransactionManager;

  class StrategyTransactionManagerException : public std::runtime_error
  {
  public:
    StrategyTransactionManagerException(const std::string& msg)
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
   * IMPROVED VERSION NOTES:
   * - Added move constructor and move assignment operator
   * - Added noexcept specifications where appropriate
   * - Added hasTransaction() method for existence checks
   * - Added getTransactionCount() as an alias for getTotalTrades()
   * - Added size() method for STL-like container interface
   * - Added empty() method to check if manager has no transactions
   * - Added clear() method to remove all transactions
   * - Enhanced exception safety in addStrategyTransaction
   *
   * @details
   * ## Observer wiring and copy/move semantics
   *
   * StrategyTransactionManager observes StrategyTransaction instances to maintain open/closed counts.
   * StrategyTransaction stores observers as non-owning references (std::reference_wrapper), so observer
   * registration is treated as *wiring*.
   *
   * This manager follows the "Option A" rule:
   *
   * ### Option A (implemented): Observers are wiring -> rebuild wiring after copy/move
   *
   * - Copy construction/assignment:
   *   - Transactions are deep-copied (each StrategyTransaction is copied into a distinct new object).
   *   - StrategyTransaction does NOT copy its observer list, so copied transactions have no observers.
   *   - After copying transactions, this manager registers itself as observer for all *open* copied transactions.
   *   - Completion of transactions in the original manager does not affect the copy (and vice versa),
   *     because the transactions are distinct.
   *
   * - Move construction/assignment:
   *   - Transaction pointers are moved into the new manager.
   *   - Any moved-in open transactions might still have the moved-from manager registered as an observer.
   *   - To prevent dangling references, the new manager clears observer wiring on moved-in transactions and
   *     re-registers itself for all open transactions.
   *
   * ### Lifetime requirement (critical)
   * - StrategyTransaction holds observers by non-owning reference. Therefore:
   *   - This manager must not be destroyed while it is still registered as an observer on any live transaction.
   *   - In practice this is satisfied because transactions are owned by this manager (or share ownership with
   *     other structures that share the manager’s lifetime). If you ever share StrategyTransaction objects outside
   *     the manager, ensure observer wiring is removed or lifetimes are constrained accordingly.
   *
   * ### Modification during notification
   * - StrategyTransaction notifies observers during completeTransaction(). Observers should not add/remove
   * observers during notification unless StrategyTransaction iterates over a snapshot.
   *
   * Thread-safety:
   * - Not thread-safe. External synchronization required.
   * Key Responsibilities:
   *
   * - Storing `StrategyTransaction` objects using shared pointers.
   * - Providing access to transactions via position ID lookup (`findStrategyTransaction`).
   * - Providing iterators to traverse transactions sorted by entry date
   * - Tracking the number of total, open, and completed transactions.
   * - Observing `StrategyTransaction` objects to update counts when they are completed.
   *
   * Thread Safety: This class is NOT thread-safe. External synchronization required for concurrent access.
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
   * - boost::posix_time::ptime: Uses Boost dates as keys in the sorted transaction map.
   * - std::map: Used internally (`mTransactionByPositionId`) to store transactions keyed by position ID.
   * - std::multimap: Used internally (`mSortedTransactions`) to store transactions keyed by entry date, allowing multiple entries per date.
   * - std::shared_ptr: Manages the lifecycle of `StrategyTransaction` objects.
   */
  template <class Decimal> class StrategyTransactionManager : public StrategyTransactionObserver<Decimal>
  {
  public:
    typedef typename std::map<uint32_t, shared_ptr<StrategyTransaction<Decimal>>>::const_iterator 
      StrategyTransactionIterator;
    using DateMap = multimap<ptime, shared_ptr<StrategyTransaction<Decimal>>>;
    typedef typename DateMap::const_iterator SortedStrategyTransactionIterator;

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
	mTotalTransactions(0),
	mCompletedTransactions(0),
	mOpenTransactions(0),
	mTransactionByPositionId(),
	mSortedTransactions()
    {
      // Deep copy transactions (new StrategyTransaction objects).
      for (const auto& pair : rhs.mTransactionByPositionId)
	{
	  const uint32_t positionId = pair.first;
	  const auto& rhsTransaction = pair.second;

	  if (rhsTransaction)
	    mTransactionByPositionId[positionId] = std::make_shared<StrategyTransaction<Decimal>>(*rhsTransaction);
	  else
	    mTransactionByPositionId[positionId] = nullptr;
	}

      rebuildSortedTransactions();
      rebuildCountersFromTransactions();

      // Wire THIS manager as observer for open transactions.
      attachToOpenTransactions();
    }
    
    StrategyTransactionManager(StrategyTransactionManager<Decimal>&& rhs)
      : StrategyTransactionObserver<Decimal>(std::move(rhs)),
	mTotalTransactions(rhs.mTotalTransactions),
	mCompletedTransactions(rhs.mCompletedTransactions),
	mOpenTransactions(rhs.mOpenTransactions),
	mTransactionByPositionId(std::move(rhs.mTransactionByPositionId)),
	mSortedTransactions(std::move(rhs.mSortedTransactions))
    {
      // Transactions may still have rhs registered as an observer; remove rhs but keep other observers.
      for (const auto& pair : mTransactionByPositionId)
	{
	  const auto& txn = pair.second;
	  if (txn && txn->isTransactionOpen())
	    txn->removeObserver(rhs);
	}

      // Register this manager on open transactions.
      attachToOpenTransactions();
    }
    
    ~StrategyTransactionManager()
    {}

    StrategyTransactionManager<Decimal>& operator=(const StrategyTransactionManager<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;

      // Detach from any open transactions we currently observe (they may be shared elsewhere).
      detachFromOpenTransactions();

      StrategyTransactionObserver<Decimal>::operator=(rhs);

      mTransactionByPositionId.clear();
      mSortedTransactions.clear();

      for (const auto& pair : rhs.mTransactionByPositionId)
	{
	  const uint32_t positionId = pair.first;
	  const auto& rhsTransaction = pair.second;

	  if (rhsTransaction)
	    mTransactionByPositionId[positionId] = std::make_shared<StrategyTransaction<Decimal>>(*rhsTransaction);
	  else
	    mTransactionByPositionId[positionId] = nullptr;
	}

      rebuildSortedTransactions();
      rebuildCountersFromTransactions();
      attachToOpenTransactions();

      return *this;
    }
    
    /**
     * @brief Move assignment operator - transfers ownership efficiently.
     * @param rhs The manager to move from (left in valid but unspecified state).
     * @return Reference to this manager.
     */
    StrategyTransactionManager<Decimal>& operator=(StrategyTransactionManager<Decimal>&& rhs)
    {
      if (this == &rhs)
	return *this;

      // Detach from any open transactions we currently observe.
      detachFromOpenTransactions();

      StrategyTransactionObserver<Decimal>::operator=(std::move(rhs));

      mTotalTransactions = rhs.mTotalTransactions;
      mCompletedTransactions = rhs.mCompletedTransactions;
      mOpenTransactions = rhs.mOpenTransactions;

      mTransactionByPositionId = std::move(rhs.mTransactionByPositionId);
      mSortedTransactions = std::move(rhs.mSortedTransactions);

      // Remove rhs as observer from moved transactions, preserving other observers.
      for (const auto& pair : mTransactionByPositionId)
	{
	  const auto& txn = pair.second;
	  if (txn && txn->isTransactionOpen())
	    txn->removeObserver(rhs);
	}

      // Register this manager as observer.
      attachToOpenTransactions();

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
    void TransactionComplete (StrategyTransaction<Decimal> * /* transaction */)
    {
      mCompletedTransactions++;

      if (mOpenTransactions > 0)
	mOpenTransactions--;
    }

    /**
     * @brief Adds a new strategy transaction to the manager.
     * @param transaction A shared pointer to the StrategyTransaction to add.
     * @throws StrategyTransactionManagerException if:
     *   - transaction is null
     *   - transaction's position is null
     *   - a transaction with the same position ID already exists
     * @details Stores the transaction in both the ID-keyed map and the date-sorted multimap.
     * If the transaction represents an open position (`isTransactionOpen()` returns true),
     * this manager registers itself as an observer (`transaction->addObserver(*this)`)
     * to be notified via `TransactionComplete` when the transaction is closed.
     * Updates the total and open transaction counts.
     */
    void addStrategyTransaction (shared_ptr<StrategyTransaction<Decimal>> transaction)
    {
      // Validate input
      if (!transaction)
	throw StrategyTransactionManagerException ("StrategyTransactionManager::addStrategyTransaction - Null transaction pointer provided");

      auto position = transaction->getTradingPosition();
      if (!position)
	throw StrategyTransactionManagerException ("StrategyTransactionManager::addStrategyTransaction - Transaction has null position");

      uint32_t positionID = position->getPositionID();
      
      // Check for duplicate
      StrategyTransactionIterator it = mTransactionByPositionId.find(positionID);
      if (it != endStrategyTransaction())
	{
	  std::string positionIDString(std::to_string(positionID));
	  throw StrategyTransactionManagerException ("StrategyTransactionManager::addStrategyTransaction - Position ID " + positionIDString + " already exists");
	}

      // Add to both maps
      mTransactionByPositionId.insert (std::make_pair(positionID, transaction));
      mSortedTransactions.insert(std::make_pair(position->getEntryDateTime(), transaction));
      
      // Register as observer if transaction is open
      if (transaction->isTransactionOpen())
	{
	  transaction->addObserverUnique (*this);
	  mOpenTransactions++;
	}
      else
	{
	  // If transaction is already complete when added, count it as completed
	  mCompletedTransactions++;
	}

      mTotalTransactions++;
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
     * @brief Alias for getTotalTrades() - STL-like interface.
     * @return The total transaction count.
     */
    uint32_t getTransactionCount() const
    {
      return mTotalTransactions;
    }

    /**
     * @brief STL-like size method.
     * @return The total number of transactions.
     */
    size_t size() const noexcept
    {
      return mTotalTransactions;
    }

    /**
     * @brief Checks if the manager has no transactions.
     * @return true if there are no transactions, false otherwise.
     */
    bool empty() const noexcept
    {
      return mTotalTransactions == 0;
    }

    /**
     * @brief Checks if a transaction with the given position ID exists.
     * @param positionIDKey The unique ID of the TradingPosition.
     * @return true if a transaction with this ID exists, false otherwise.
     */
    bool hasTransaction(uint32_t positionIDKey) const
    {
      return mTransactionByPositionId.find(positionIDKey) != mTransactionByPositionId.end();
    }

    /**
     * @brief Removes all transactions from the manager.
     * @details Resets all counters and clears both internal maps.
     * Note: This does not remove this manager as an observer from the transactions,
     * as the shared_ptrs will maintain the transactions until all references are released.
     */
    void clear() noexcept
    {
      // Detach first to avoid leaving dangling observer refs in shared transactions.
      detachFromOpenTransactions();

      mTransactionByPositionId.clear();
      mSortedTransactions.clear();
      mTotalTransactions = 0;
      mCompletedTransactions = 0;
      mOpenTransactions = 0;
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
     * @brief Returns an iterator to the end of the date-sorted transactions multimap.
     * @return A `SortedStrategyTransactionIterator`.
     */
    SortedStrategyTransactionIterator endSortedStrategyTransaction() const
    {
      return mSortedTransactions.end();
    }

  private:
    void detachFromOpenTransactions() noexcept
    {
      // Remove ONLY *this* manager from observed open transactions.
      // This preserves other observers (logging/metrics/etc).
      for (const auto& pair : mTransactionByPositionId)
	{
	  const auto& txn = pair.second;
	  if (txn && txn->isTransactionOpen())
	    txn->removeObserver(*this);
	}
    }

    void attachToOpenTransactions()
    {
      // Attach *this* manager to any open transactions it owns.
      // (May allocate in the transaction's observer list.)
      for (const auto& pair : mTransactionByPositionId)
	{
	  const auto& txn = pair.second;
	  if (txn && txn->isTransactionOpen())
	    txn->addObserverUnique(*this);
	}
    }

    void rebuildCountersFromTransactions() noexcept
    {
      uint32_t total = 0;
      uint32_t open = 0;
      uint32_t complete = 0;

      for (const auto& pair : mTransactionByPositionId)
	{
	  const auto& txn = pair.second;
	  if (!txn)
	    continue;

	  ++total;
	  if (txn->isTransactionOpen())
	    ++open;
	  else
	    ++complete;
	}

      mTotalTransactions = total;
      mOpenTransactions = open;
      mCompletedTransactions = complete;
    }

    void rebuildSortedTransactions()
    {
      mSortedTransactions.clear();

      for (const auto& pair : mTransactionByPositionId)
	{
	  const auto& txn = pair.second;
	  if (!txn)
	    continue;

	  auto pos = txn->getTradingPositionPtr();
	  if (!pos)
	    continue;

	  mSortedTransactions.insert(std::make_pair(pos->getEntryDateTime(), txn));
	}
    }
    
  private:
    uint32_t mTotalTransactions;
    uint32_t mCompletedTransactions;
    uint32_t mOpenTransactions;

    std::map<uint32_t, shared_ptr<StrategyTransaction<Decimal>>> mTransactionByPositionId;
 
    // Sorted transactions; sorted by position entry date. Need multimap, because we can have multiple
    // positions entered on the same date
    DateMap mSortedTransactions;
  };
}

#endif
