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
  template <int Prec> class StrategyTransactionManager;

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

  template <int Prec> class StrategyTransactionManager : public StrategyTransactionObserver<Prec>
  {
  public:
    typedef typename std::map<uint32_t, shared_ptr<StrategyTransaction<Prec>>>::const_iterator 
      StrategyTransactionIterator;
    typedef typename multimap<date, shared_ptr<StrategyTransaction<Prec>>>::const_iterator 
      SortedStrategyTransactionIterator;

  public:
    StrategyTransactionManager()
      : StrategyTransactionObserver<Prec>(),
	mTotalTransactions(0),
	mCompletedTransactions(0),
	mOpenTransactions(0),
	mTransactionByPositionId(),
	mSortedTransactions()
    {}

    StrategyTransactionManager (const StrategyTransactionManager<Prec>& rhs)
      : StrategyTransactionObserver<Prec>(rhs),
	mTotalTransactions(rhs.mTotalTransactions),
	mCompletedTransactions(rhs.mCompletedTransactions),
	mOpenTransactions(rhs.mOpenTransactions),
	mTransactionByPositionId(rhs.mTransactionByPositionId),
	mSortedTransactions(rhs.mSortedTransactions)
    {}

    ~StrategyTransactionManager()
    {}

    StrategyTransactionManager<Prec>& 
    operator=(const StrategyTransactionManager<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      StrategyTransactionObserver<Prec>::operator=(rhs);
      mTotalTransactions = rhs.mTotalTransactions;
      mCompletedTransactions = rhs.mCompletedTransactions;
      mOpenTransactions = rhs.mOpenTransactions;
      mTransactionByPositionId = rhs.mTransactionByPositionId;
      mSortedTransactions = rhs.mSortedTransactions;
      return *this;
    }

    void TransactionComplete (StrategyTransaction<Prec> *transaction)
    {
      mCompletedTransactions++;

      if (mOpenTransactions > 0)
	mOpenTransactions--;
    }

    void addStrategyTransaction (shared_ptr<StrategyTransaction<Prec>> transaction)
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

    uint32_t getTotalTrades() const
    {
      return mTotalTransactions;
    }

    uint32_t getOpenTrades() const
    {
      return mOpenTransactions;
    }

    uint32_t getClosedTrades() const
    {
      return mCompletedTransactions;
    }

    StrategyTransactionIterator findStrategyTransaction (uint32_t positionIDKey) const
    {
      return mTransactionByPositionId.find(positionIDKey);
    }

    StrategyTransactionIterator beginStrategyTransaction() const
    {
      return mTransactionByPositionId.begin();
    }

    StrategyTransactionIterator endStrategyTransaction() const
    {
      return mTransactionByPositionId.end();
    }

    //

    SortedStrategyTransactionIterator beginSortedStrategyTransaction() const
    {
      return mSortedTransactions.begin();
    }

    SortedStrategyTransactionIterator endSortedStrategyTransaction() const
    {
      return mSortedTransactions.end();
    }

  private:
    uint32_t mTotalTransactions;
    uint32_t mCompletedTransactions;
    uint32_t mOpenTransactions;

    std::map<uint32_t, shared_ptr<StrategyTransaction<Prec>>> mTransactionByPositionId;
 
    // Sorted transactions; sorted by position entry date. Need multimap, because we can have multiple
    // positions entered on the same date
    std::multimap< boost::gregorian::date, shared_ptr<StrategyTransaction<Prec>>> mSortedTransactions;
  };
}
#endif
