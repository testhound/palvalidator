// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TRADING_ORDER_MANAGER_H
#define __TRADING_ORDER_MANAGER_H 1

#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <cstdint>
#include <string>
#include <boost/date_time.hpp>
#include "TradingOrder.h"
#include "Portfolio.h"
#include "InstrumentPositionManager.h"

using std::vector;
using std::shared_ptr;

namespace mkc_timeseries
{
  template <class Decimal> class ProcessOrderVisitor : public TradingOrderVisitor<Decimal>
  {
  public:
    ProcessOrderVisitor(OHLCTimeSeriesEntry<Decimal> tradingBar)
      : mTradingBar (tradingBar)
    {}

    ~ProcessOrderVisitor()
    {}

    ProcessOrderVisitor (const ProcessOrderVisitor<Decimal>& rhs)
      : mTradingBar(rhs.mTradingBar)
    {}

    ProcessOrderVisitor<Decimal>& 
    operator=(const ProcessOrderVisitor<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mTradingBar = rhs.mTradingBar;
      return *this;
    }

    void visit (MarketOnOpenLongOrder<Decimal> *order)
    {
      ValidateOrder (order);
      // Market orders are unconditional

      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    void visit (MarketOnOpenSellOrder<Decimal> *order)
    {
      ValidateOrder (order);
      // Market orders are unconditional

      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    void visit (MarketOnOpenCoverOrder<Decimal> *order)
    {
      ValidateOrder (order);
      // Market orders are unconditional

      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    void visit (MarketOnOpenShortOrder<Decimal> *order)
    {
      ValidateOrder (order);

      // Market orders are unconditional
      order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
    }

    void visit (SellAtLimitOrder<Decimal> *order)
    {
      ValidateOrder (order);
      if (mTradingBar.getHighValue() > order->getLimitPrice())
	{
	  // If we gapped up we assume we get the open price
	  if (mTradingBar.getOpenValue() > order->getLimitPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getLimitPrice());
	}
    }

    void visit (CoverAtLimitOrder<Decimal> *order)
    {
      ValidateOrder (order);

      if (mTradingBar.getLowValue() < order->getLimitPrice())	{
	  // If we gapped down we assume we get the open price
	  if (mTradingBar.getOpenValue() < order->getLimitPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getLimitPrice());
	}
    }

    void visit (CoverAtStopOrder<Decimal> *order)
    {
      ValidateOrder (order);
      if (mTradingBar.getHighValue() > order->getStopPrice())
	{
	  // If we gapped up we assume we get the open price
	  if (mTradingBar.getOpenValue() > order->getStopPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getStopPrice());
	}
    }

    void visit (SellAtStopOrder<Decimal> *order)
    {
      ValidateOrder (order);
      if (mTradingBar.getLowValue() < order->getStopPrice())
	{
	  // If we gapped down we assume we get the open price
	  if (mTradingBar.getOpenValue() < order->getStopPrice())
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), mTradingBar.getOpenValue());
	  else
	    order->MarkOrderExecuted (mTradingBar.getDateValue(), order->getStopPrice());
	}
    }

    void updateTradingBar (OHLCTimeSeriesEntry<Decimal> tradingBar)
    {
      mTradingBar = tradingBar;
    }

  private:
    void ValidateOrder (TradingOrder<Decimal>* order)
    {
      if (mTradingBar.getDateValue() <= order->getOrderDate())
	throw TradingOrderException ("Bar date " +to_simple_string (mTradingBar.getDateValue()) +" must be greater than order date " +to_simple_string (order->getOrderDate()));

      if (!order->isOrderPending())
	{
	  if (order->isOrderExecuted())
	    throw TradingOrderException ("ProcessOrderVisitor: Executed order cannot be processed");
	  else if (order->isOrderCanceled())
	    throw TradingOrderException ("ProcessOrderVisitor: Canceled order cannot be processed");
	  else
	    throw TradingOrderException ("ProcessOrderVisitor: unknow order state");
	}
    }

  private:
    OHLCTimeSeriesEntry<Decimal> mTradingBar;
  };

 
 template <class Decimal> class TradingOrderManager
  {
  public:
    typedef typename vector<shared_ptr<MarketOnOpenSellOrder<Decimal>>>::const_iterator MarketSellOrderIterator;
    typedef typename vector<shared_ptr<MarketOnOpenCoverOrder<Decimal>>>::const_iterator MarketCoverOrderIterator;
    typedef typename vector<shared_ptr<MarketOnOpenLongOrder<Decimal>>>::const_iterator MarketLongOrderIterator;
    typedef typename vector<shared_ptr<MarketOnOpenShortOrder<Decimal>>>::const_iterator MarketShortOrderIterator;

    typedef typename vector<shared_ptr<SellAtLimitOrder<Decimal>>>::const_iterator LimitSellOrderIterator;
    typedef typename vector<shared_ptr<CoverAtLimitOrder<Decimal>>>::const_iterator LimitCoverOrderIterator;

    typedef typename vector<shared_ptr<SellAtStopOrder<Decimal>>>::const_iterator StopSellOrderIterator;
    typedef typename vector<shared_ptr<CoverAtStopOrder<Decimal>>>::const_iterator StopCoverOrderIterator;

   
     typedef typename std::list<std::reference_wrapper<TradingOrderObserver<Decimal>>>::const_iterator ConstObserverIterator;
    typedef typename  std::multimap<boost::gregorian::date, std::shared_ptr<TradingOrder<Decimal>>>::const_iterator PendingOrderIterator;

  public:
    explicit TradingOrderManager(std::shared_ptr<Portfolio<Decimal>> portfolio)
      : mPortfolio(portfolio),
	mMarketSellOrders(),
	mMarketCoverOrders(),
	mMarketLongOrders(),
	mMarketShortOrders(),
	mLimitSellOrders(),
	mLimitCoverOrders(),
	mStopSellOrders(),
	mStopCoverOrders(),
	mObservers(),
	mPendingOrders(),
	mPendingOrdersUpToDate(false)
      {}

    TradingOrderManager (const TradingOrderManager<Decimal>& rhs)
      :  mPortfolio(rhs.mPortfolio),
	 mMarketSellOrders(rhs.mMarketSellOrders),
	 mMarketCoverOrders(rhs.mMarketCoverOrders),
	 mMarketLongOrders(rhs.mMarketLongOrders),
	 mMarketShortOrders(rhs.mMarketShortOrders),
	 mLimitSellOrders(rhs.mLimitSellOrders),
	 mLimitCoverOrders(rhs.mLimitCoverOrders),
	 mStopSellOrders(rhs.mStopSellOrders),
	 mStopCoverOrders(rhs.mStopCoverOrders),
	 mObservers(rhs.mObservers),
	 mPendingOrders(rhs.mPendingOrders),
	 mPendingOrdersUpToDate(rhs.mPendingOrdersUpToDate)
    {}

    ~TradingOrderManager()
      {}

    TradingOrderManager<Decimal>& 
    operator=(const TradingOrderManager<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mPortfolio = rhs.mPortfolio;

      mMarketSellOrders = rhs.mMarketSellOrders;
      mMarketCoverOrders = rhs.mMarketCoverOrders;
      mMarketLongOrders = rhs.mMarketLongOrders;
      mMarketShortOrders = rhs.mMarketShortOrders;
      mLimitSellOrders = rhs.mLimitSellOrders;
      mLimitCoverOrders = rhs.mLimitCoverOrders;
      mStopSellOrders = rhs.mStopSellOrders;
      mStopCoverOrders = rhs.mStopCoverOrders;
      mObservers = rhs.mObservers;
      mPendingOrders = rhs.mPendingOrders;
      mPendingOrdersUpToDate = rhs.mPendingOrdersUpToDate;
      return *this;
    }

    // Pending order with no position
    void addTradingOrder (std::shared_ptr<MarketOnOpenCoverOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketCoverOrders.push_back (order);
    }

    // Pending order with no position
    void addTradingOrder (std::shared_ptr<MarketOnOpenSellOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketSellOrders.push_back (order);
    }

    void addTradingOrder (std::shared_ptr<MarketOnOpenLongOrder<Decimal>>& order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketLongOrders.push_back (order);
    }

    void addTradingOrder (std::shared_ptr<MarketOnOpenShortOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mMarketShortOrders.push_back (order);
    }

    void addTradingOrder (std::shared_ptr<SellAtLimitOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mLimitSellOrders.push_back (order);
    }

    void addTradingOrder (std::shared_ptr<CoverAtLimitOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mLimitCoverOrders.push_back (order);
    }

    void addTradingOrder (std::shared_ptr<SellAtStopOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mStopSellOrders.push_back (order);
    }

    void addTradingOrder (std::shared_ptr<CoverAtStopOrder<Decimal>> order)
    {
      ValidateNewOrder (order);
      mPendingOrdersUpToDate = false;
      mStopCoverOrders.push_back (order);
    }

    PendingOrderIterator beginPendingOrders() const
    {
      if (mPendingOrdersUpToDate == false)
	populatePendingOrders();

      return mPendingOrders.begin();
    }

    PendingOrderIterator endPendingOrders() const
    {
       if (mPendingOrdersUpToDate == false)
	populatePendingOrders();

      return mPendingOrders.end();
    }

    // Market entry order iterators
    MarketLongOrderIterator beginMarketLongOrders() const
    {
      return mMarketLongOrders.begin();
    }

    MarketLongOrderIterator endMarketLongOrders() const
    {
      return mMarketLongOrders.end();
    }

    MarketShortOrderIterator beginMarketShortOrders() const
    {
      return mMarketShortOrders.begin();
    }

    MarketShortOrderIterator endMarketShortOrders() const
    {
      return mMarketShortOrders.end();
    }

    // Market exit order iterators
    MarketSellOrderIterator beginMarketSellOrders() const
    {
      return mMarketSellOrders.begin();
    }

    MarketSellOrderIterator endMarketSellOrders() const
    {
      return mMarketSellOrders.end();
    }

    MarketCoverOrderIterator beginMarketCoverOrders() const
    {
      return mMarketCoverOrders.begin();
    }

    MarketCoverOrderIterator endMarketCoverOrders() const
    {
      return mMarketCoverOrders.end();
    }


    // Limit exit order iterators

    LimitSellOrderIterator beginLimitSellOrders() const
    {
      return mLimitSellOrders.begin();
    }

    LimitSellOrderIterator endLimitSellOrders() const
    {
      return mLimitSellOrders.end();
    }

    LimitCoverOrderIterator beginLimitCoverOrders() const
    {
      return mLimitCoverOrders.begin();
    }

    LimitCoverOrderIterator endLimitCoverOrders() const
    {
      return mLimitCoverOrders.end();
    }

    //
    // Stop exit order iterator methods
    //

    StopSellOrderIterator beginStopSellOrders() const
    {
      return mStopSellOrders.begin();
    }

    StopSellOrderIterator endStopSellOrders() const
    {
      return mStopSellOrders.end();
    }

    StopCoverOrderIterator beginStopCoverOrders() const
    {
      return mStopCoverOrders.begin();
    }

    StopCoverOrderIterator endStopCoverOrders() const
    {
      return mStopCoverOrders.end();
    }

    uint32_t getNumMarketExitOrders() const
    {
      return mMarketSellOrders.size() + mMarketCoverOrders.size();
    }

    uint32_t getNumMarketEntryOrders() const
    {
      return mMarketLongOrders.size() + mMarketShortOrders.size();
    }

    uint32_t getNumLimitExitOrders() const
    {
      return mLimitSellOrders.size() + mLimitCoverOrders.size();
    }

    uint32_t getNumStopExitOrders() const
    {
      return mStopSellOrders.size() + mStopCoverOrders.size();
    }

    void addObserver (std::reference_wrapper<TradingOrderObserver<Decimal>> observer)
    {
      mObservers.push_back(observer);
    }

    void processPendingOrders (const boost::gregorian::date& processingDate,
			       const InstrumentPositionManager<Decimal>& positions)
    {
      ProcessPendingMarketExitOrders(processingDate, positions);
      ProcessPendingMarketEntryOrders(processingDate, positions);
      ProcessPendingStopExitOrders(processingDate, positions);
      ProcessPendingLimitExitOrders(processingDate, positions);

      // Since we have processed pending orders, our pending order map is no longer
      // up to date

      mPendingOrdersUpToDate = false;
      // NOTE: When closing a position compare number of shares/contracts in order
      // with number of shares/contracts in position in case position will remain open
    }

  private:
    template <typename T>
    void ProcessingPendingOrders(const boost::gregorian::date& processingDate, 
				 std::vector<std::shared_ptr<T>>& vectorContainer,
				 const InstrumentPositionManager<Decimal>& positions)
    {
      typedef typename std::shared_ptr<T> OrderPtr;

      typename std::vector<std::shared_ptr<T>>::const_iterator it = vectorContainer.begin();
      OrderPtr order;
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIt;
      std::shared_ptr<Security<Decimal>> aSecurity;
      typename Security<Decimal>::ConstRandomAccessIterator timeSeriesEntryIt;

      for (; it != vectorContainer.end();)
	{
	  order = (*it);
	  
	  if (order->isOrderPending() && (processingDate > order->getOrderDate()))
	    {
	      symbolIt = mPortfolio->findSecurity (order->getTradingSymbol());
	      if (symbolIt != mPortfolio->endPortfolio())
		{
		  aSecurity = symbolIt->second;
		  // Make sure security trades on the processingDate. It's possible due to holidy or
		  // non-trading in certain futures market that there is no market data on the processing date

		  timeSeriesEntryIt = aSecurity->findTimeSeriesEntry (processingDate) ;
		  if (timeSeriesEntryIt != aSecurity->getRandomAccessIteratorEnd())
		    {
		      // Check to see if another order has already closed the position.
		      // This could happen if a stop order was executed on the same day as
		      // a limit order. 
		      if (order->isExitOrder() && 
			  (positions.isFlatPosition (order->getTradingSymbol()) == true))
			{
			  order->MarkOrderCanceled();
			  NotifyOrderCanceled (order);
			}
		      else
			{
			  ProcessOrderVisitor<Decimal> orderProcessor (*timeSeriesEntryIt);
			  orderProcessor.visit (order.get());

			  if (order->isOrderExecuted())
			    NotifyOrderExecuted (order);
			  else
			    {
			      // Note if a order has data for a trading day and the order is not executed
			      // we cancel it. The Strategy will need to resubmit the order again.
			      // Note market orders are always executed so there is not problem with them.

			      order->MarkOrderCanceled();
			      NotifyOrderCanceled (order);
			    }
			}

		      // Remove order from pending list
		      // Note that this returns the next element in the list after erasing

		      it = vectorContainer.erase (it);
		    }
		  else
		    {
		      ++it;
		    }
		}
	    }
	}
    }

    void ProcessPendingMarketExitOrders(const boost::gregorian::date& processingDate,
					const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<MarketOnOpenSellOrder<Decimal>> (processingDate, mMarketSellOrders, 
								  positions);
      this->ProcessingPendingOrders<MarketOnOpenCoverOrder<Decimal>> (processingDate, mMarketCoverOrders, 
								   positions);
    }

    void ProcessPendingMarketEntryOrders(const boost::gregorian::date& processingDate,
					 const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<MarketOnOpenLongOrder<Decimal>> (processingDate, mMarketLongOrders,
								  positions);
      this->ProcessingPendingOrders<MarketOnOpenShortOrder<Decimal>> (processingDate, mMarketShortOrders,
								   positions);
    }

    void ProcessPendingStopExitOrders(const boost::gregorian::date& processingDate,
				      const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<SellAtStopOrder<Decimal>> (processingDate, mStopSellOrders,
							    positions);
      this->ProcessingPendingOrders<CoverAtStopOrder<Decimal>> (processingDate, mStopCoverOrders,
							     positions);
    }

    void ProcessPendingLimitExitOrders(const boost::gregorian::date& processingDate,
				       const InstrumentPositionManager<Decimal>& positions)
    {
      this->ProcessingPendingOrders<SellAtLimitOrder<Decimal>> (processingDate, mLimitSellOrders,
							     positions);
      this->ProcessingPendingOrders<CoverAtLimitOrder<Decimal>> (processingDate, mLimitCoverOrders,
							      positions);
    }

    template <typename T>
    void NotifyOrderExecuted (std::shared_ptr<T> order)
    {
      ConstObserverIterator it = beginObserverList();
      for (; it != endObserverList(); it++)
	(*it).get().OrderExecuted (order.get());
    }

    template <typename T>
    void NotifyOrderCanceled (std::shared_ptr<T> order)
    {
      ConstObserverIterator it = beginObserverList();
      for (; it != endObserverList(); it++)
	(*it).get().OrderCanceled (order.get());
    }

    void ValidateNewOrder (const std::shared_ptr<TradingOrder<Decimal>>& order) const
    {
      if (order->isOrderExecuted())
	throw TradingOrderManagerException ("Attempt to add executed trading order");

      if (order->isOrderCanceled())
	throw TradingOrderManagerException ("Attempt to add canceled trading order");
    }

    ConstObserverIterator beginObserverList() const
    {
      return mObservers.begin();
    }

    ConstObserverIterator endObserverList() const
    {
      return mObservers.end();
    }

    void populatePendingOrders() const
    {
      mPendingOrders.clear();

      MarketLongOrderIterator longMarketIt = beginMarketLongOrders();
      for (; longMarketIt != endMarketLongOrders(); longMarketIt++)
	  addOrderToPending (*longMarketIt);
	
      MarketShortOrderIterator shortMarketIt = beginMarketShortOrders();
      for (; shortMarketIt != endMarketShortOrders(); shortMarketIt++)
	  addOrderToPending (*shortMarketIt);

      MarketSellOrderIterator sellMarketIt = beginMarketSellOrders();
      for (; sellMarketIt != endMarketSellOrders(); sellMarketIt++)
	  addOrderToPending (*sellMarketIt);

      MarketCoverOrderIterator coverMarketIt = beginMarketCoverOrders();
      for (; coverMarketIt != endMarketCoverOrders(); coverMarketIt++)
	  addOrderToPending (*coverMarketIt);

      StopSellOrderIterator stopSellIt = beginStopSellOrders();
      for (; stopSellIt != endStopSellOrders(); stopSellIt++)
	  addOrderToPending (*stopSellIt);

      StopCoverOrderIterator stopCoverIt = beginStopCoverOrders();
      for (; stopCoverIt != endStopCoverOrders(); stopCoverIt++)
	  addOrderToPending (*stopCoverIt);

      LimitSellOrderIterator sellLimitIt = beginLimitSellOrders();
      for (; sellLimitIt != endLimitSellOrders(); sellLimitIt++)
	  addOrderToPending (*sellLimitIt);

      LimitCoverOrderIterator coverLimitIt = beginLimitCoverOrders();
      for (; coverLimitIt != endLimitCoverOrders(); coverLimitIt++)
	  addOrderToPending (*coverLimitIt);

     

      mPendingOrdersUpToDate = true;
    }

    void addOrderToPending(std::shared_ptr<TradingOrder<Decimal>> aOrder) const
    {
      mPendingOrders.insert (std::make_pair (aOrder->getOrderDate(), aOrder));
    }

  private:
    std::shared_ptr<Portfolio<Decimal>> mPortfolio;
    std::vector<std::shared_ptr<MarketOnOpenSellOrder<Decimal>>> mMarketSellOrders;
    std::vector<std::shared_ptr<MarketOnOpenCoverOrder<Decimal>>> mMarketCoverOrders;
    std::vector<std::shared_ptr<MarketOnOpenLongOrder<Decimal>>> mMarketLongOrders;
    std::vector<std::shared_ptr<MarketOnOpenShortOrder<Decimal>>> mMarketShortOrders;
    std::vector<std::shared_ptr<SellAtLimitOrder<Decimal>>> mLimitSellOrders;
    std::vector<std::shared_ptr<CoverAtLimitOrder<Decimal>>> mLimitCoverOrders;
    std::vector<std::shared_ptr<SellAtStopOrder<Decimal>>> mStopSellOrders;
    std::vector<std::shared_ptr<CoverAtStopOrder<Decimal>>> mStopCoverOrders;
    std::list<std::reference_wrapper<TradingOrderObserver<Decimal>>> mObservers;

    // A temporary map to iterate over pending order if a client asks for them
    // The map is cleared before iterating and populate from the above vectors
    mutable std::multimap<boost::gregorian::date, std::shared_ptr<TradingOrder<Decimal>>> mPendingOrders;
    mutable bool mPendingOrdersUpToDate;
 };
}

#endif


