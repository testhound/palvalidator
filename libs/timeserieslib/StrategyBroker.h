// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STRATEGY_BROKER_H
#define __STRATEGY_BROKER_H 1

#include <exception>
#include "Portfolio.h"
#include "TradingOrderManager.h"
#include "InstrumentPositionManager.h"
#include "ClosedPositionHistory.h"
#include "StrategyTransactionManager.h"
#include "ProfitTarget.h"
#include "StopLoss.h"


namespace mkc_timeseries
{
  using dec::decimal;
  using boost::gregorian::date;

  class StrategyBrokerException : public std::runtime_error
  {
  public:
  StrategyBrokerException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~StrategyBrokerException()
      {}
  };

  template <class Decimal> class StrategyBroker : 
    public TradingOrderObserver<Decimal>, 
    public TradingPositionObserver<Decimal>
  {
  public:
    typedef typename TradingOrderManager<Decimal>::PendingOrderIterator PendingOrderIterator;
    typedef typename StrategyTransactionManager<Decimal>::SortedStrategyTransactionIterator 
      StrategyTransactionIterator;
    typedef typename ClosedPositionHistory<Decimal>::ConstPositionIterator ClosedPositionIterator;

  public:
    StrategyBroker (std::shared_ptr<Portfolio<Decimal>> portfolio)
      : TradingOrderObserver<Decimal>(),
	TradingPositionObserver<Decimal>(),
	mOrderManager(portfolio),
	mInstrumentPositionManager(),
	mStrategyTrades(),
	mClosedTradeHistory(),
	mPortfolio(portfolio)
    {
      mOrderManager.addObserver (*this);
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->beginPortfolio();

      for (; symbolIterator != mPortfolio->endPortfolio(); symbolIterator++)
	  mInstrumentPositionManager.addInstrument(symbolIterator->second->getSymbol());
    }

    ~StrategyBroker()
    {}

    StrategyBroker(const StrategyBroker<Decimal> &rhs)
      : TradingOrderObserver<Decimal>(rhs),
	TradingPositionObserver<Decimal>(rhs),
	mOrderManager(rhs.mOrderManager),
	mInstrumentPositionManager(rhs.mInstrumentPositionManager),
	mStrategyTrades(rhs.mStrategyTrades),
	mClosedTradeHistory(rhs.mClosedTradeHistory),
	mPortfolio(rhs.mPortfolio)
    {}

    StrategyBroker<Decimal>& 
    operator=(const StrategyBroker<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingOrderObserver<Decimal>::operator=(rhs);
      TradingPositionObserver<Decimal>::operator=(rhs);

      mOrderManager = rhs.mOrderManager;
      mInstrumentPositionManager = rhs.mInstrumentPositionManager;
      mStrategyTrades = rhs.mStrategyTrades;
      mClosedTradeHistory = rhs.mClosedTradeHistory;
      mPortfolio = rhs.mPortfolio;

      return *this;
    }

    StrategyTransactionIterator beginStrategyTransactions() const
    {
      return mStrategyTrades.beginSortedStrategyTransaction();
    }

    StrategyTransactionIterator endStrategyTransactions() const
    {
      return mStrategyTrades.endSortedStrategyTransaction();
    }

    const ClosedPositionHistory<Decimal>&
    getClosedPositionHistory() const
    {
      return mClosedTradeHistory;
    }

    ClosedPositionIterator beginClosedPositions() const
    {
      return mClosedTradeHistory.beginTradingPositions();
    }

    ClosedPositionIterator endClosedPositions() const
    {
      return mClosedTradeHistory.beginTradingPositions();
    }

    uint32_t getTotalTrades() const
    {
      return  mStrategyTrades.getTotalTrades();
    }

    uint32_t getOpenTrades() const
    {
      return  mStrategyTrades.getOpenTrades();
    }

    uint32_t getClosedTrades() const
    {
      return  mStrategyTrades.getClosedTrades();
    }

    bool isLongPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.isLongPosition (tradingSymbol);
    }

    bool isShortPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.isShortPosition (tradingSymbol);
    }

    bool isFlatPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.isFlatPosition (tradingSymbol);
    }

    void EnterLongOnOpen(const std::string& tradingSymbol, 	
			 const date& orderDate,
			 const TradingVolume& unitsInOrder)
    {
      auto order = std::make_shared<MarketOnOpenLongOrder<Decimal>>(tradingSymbol,
								  unitsInOrder,
								  orderDate);
      
      mOrderManager.addTradingOrder (order);
    }

    void EnterShortOnOpen(const std::string& tradingSymbol,	
			  const date& orderDate,
			  const TradingVolume& unitsInOrder )
    {
      auto order = std::make_shared<MarketOnOpenShortOrder<Decimal>>(tradingSymbol,
								  unitsInOrder,
								  orderDate);
      
      mOrderManager.addTradingOrder (order);
    }

    void ExitLongAllUnitsOnOpen(const std::string& tradingSymbol,
				const date& orderDate,
				const TradingVolume& unitsInOrder)
    {
     if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<MarketOnOpenSellOrder<Decimal>>(tradingSymbol,
								     unitsInOrder,
								     orderDate);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtOpen - no long position for " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
	}
    }

    void ExitLongAllUnitsOnOpen(const std::string& tradingSymbol,
				const date& orderDate)
    {
     if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  ExitLongAllUnitsOnOpen(tradingSymbol, orderDate, 
				 mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol));
	}
           else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtOpen - no long position for " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
	}
    }

    void ExitShortAllUnitsOnOpen(const std::string& tradingSymbol,
				 const date& orderDate)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<MarketOnOpenCoverOrder<Decimal>>(tradingSymbol,
								       mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
								      orderDate);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtOpen - no short position for " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
	}
    }

   

    void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitPrice)
    {
      //std::cout << "Entering long profit target at: " << limitPrice << " on date: " << orderDate << std::endl;

      if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<SellAtLimitOrder<Decimal>>(tradingSymbol,
								mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
								orderDate,
								limitPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtLimit - no long position for " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
	}
    }

    void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitBasePrice,
				 const PercentNumber<Decimal>& percentNum)
    {
      //std::cout << "StrategyBroker::ExitLongAllUnitsAtLimit - limitBasePrice: " << limitBasePrice << " percentNum = " << percentNum.getAsPercent() << std::endl << std::endl;
      LongProfitTarget<Decimal> profitTarget(limitBasePrice, percentNum);
      
      this->ExitLongAllUnitsAtLimit (tradingSymbol, orderDate, profitTarget.getProfitTarget());
    }

    void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				  const date& orderDate,
				  const Decimal& limitPrice)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<CoverAtLimitOrder<Decimal>>(tradingSymbol,
								 mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
								 orderDate,
								 limitPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtLimit - no short position for " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
	}
    }

    void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitBasePrice,
				 const PercentNumber<Decimal>& percentNum)
    {
      //std::cout << "StrategyBroker::ExitShortAllUnitsAtLimit - limitBasePrice: " << limitBasePrice << " percentNum = " << percentNum.getAsPercent() << std::endl << std::endl;
      ShortProfitTarget<Decimal> percentTarget(limitBasePrice, percentNum);

      Decimal profitTarget(percentTarget.getProfitTarget());
      //std::cout << "StrategyBroker::ExitShortAllUnitsAtLimit - short profit target = " << profitTarget << " on date: " << orderDate << std::endl << std::endl;

      
      this->ExitShortAllUnitsAtLimit (tradingSymbol,orderDate,profitTarget);
    }

    
    void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const Decimal& stopPrice)
    {
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<SellAtStopOrder<Decimal>>(tradingSymbol,
							       mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
							       orderDate,
							       stopPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitLongAllUnitsAtStop - no long position for " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
	}
    }

    void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const Decimal& stopBasePrice,
				const PercentNumber<Decimal>& percentNum)
    {
      LongStopLoss<Decimal> percentStop(stopBasePrice, percentNum);
      Decimal stopLoss(percentStop.getStopLoss());

      //std::cout << "Entering long stop loss at: " << stopLoss << " on date: " << orderDate << std::endl;
      this->ExitLongAllUnitsAtStop(tradingSymbol, orderDate, stopLoss);
    }

    void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& stopPrice)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<CoverAtStopOrder<Decimal>>(tradingSymbol,
							       	mInstrumentPositionManager.getVolumeInAllUnits(tradingSymbol),
							       orderDate,
							       stopPrice);

	  mOrderManager.addTradingOrder (order);
	}
      else
	{
	  throw StrategyBrokerException("StrategyBroker::ExitShortAllUnitsAtStop - no short position for " +tradingSymbol +" with order date: " +boost::gregorian::to_simple_string (orderDate));
	}
    }

    void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& stopBasePrice,
				 const PercentNumber<Decimal>& percentNum)
    {
      ShortStopLoss<Decimal> aPercentStop(stopBasePrice, percentNum);
      Decimal stopLoss(aPercentStop.getStopLoss());
      this->ExitShortAllUnitsAtStop(tradingSymbol,orderDate,stopLoss);
    }

    PendingOrderIterator beginPendingOrders() const
    {
      return mOrderManager.beginPendingOrders();
    }

    PendingOrderIterator endPendingOrders() const
    {
      return mOrderManager.endPendingOrders();
    }

    void ProcessPendingOrders(const date& orderProcessingDate)
    {
      // Add historical bar for this date before possibly closing any open
      // positions
      mInstrumentPositionManager.addBarForOpenPosition (orderProcessingDate,
							mPortfolio);
      mOrderManager.processPendingOrders (orderProcessingDate,
					  mInstrumentPositionManager);
    }

    void OrderExecuted (MarketOnOpenLongOrder<Decimal> *order)
    {
      auto position = createLongTradingPosition (order);
      auto pOrder = std::make_shared<MarketOnOpenLongOrder<Decimal>>(*order);

      mInstrumentPositionManager.addPosition (position);
      mStrategyTrades.addStrategyTransaction (createStrategyTransaction (pOrder, position));
    }

    
    void OrderExecuted (MarketOnOpenShortOrder<Decimal> *order)
    {
      auto position = createShortTradingPosition (order);
      auto pOrder = std::make_shared<MarketOnOpenShortOrder<Decimal>>(*order);

      mInstrumentPositionManager.addPosition (position);
      mStrategyTrades.addStrategyTransaction (createStrategyTransaction (pOrder, position));
    }

    void OrderExecuted (MarketOnOpenSellOrder<Decimal> *order)
    {
      ExitOrderExecutedCommon<MarketOnOpenSellOrder<Decimal>>(order);
    }

    void OrderExecuted (MarketOnOpenCoverOrder<Decimal> *order)
    {
      ExitOrderExecutedCommon<MarketOnOpenCoverOrder<Decimal>>(order);
    }

    void OrderExecuted (SellAtLimitOrder<Decimal> *order)
    {
      ExitOrderExecutedCommon<SellAtLimitOrder<Decimal>>(order);
    }

    void OrderExecuted (CoverAtLimitOrder<Decimal> *order)
    {
      //std::cout << "Short profit target of " << order->getFillPrice() << " reached on " << order->getFillDate() << std::endl;

      ExitOrderExecutedCommon<CoverAtLimitOrder<Decimal>>(order);
    }

    void OrderExecuted (CoverAtStopOrder<Decimal> *order)
    {
      ExitOrderExecutedCommon<CoverAtStopOrder<Decimal>>(order);
    }

    void OrderExecuted (SellAtStopOrder<Decimal> *order)
    {
      ExitOrderExecutedCommon<SellAtStopOrder<Decimal>>(order);
    }

    void OrderCanceled (MarketOnOpenLongOrder<Decimal> *order)
    {

    }

    void OrderCanceled (MarketOnOpenShortOrder<Decimal> *order)
    {

    }

    void OrderCanceled (MarketOnOpenSellOrder<Decimal> *order)
    {

    }

    void OrderCanceled (MarketOnOpenCoverOrder<Decimal> *order)
    {

    }

    void OrderCanceled (SellAtLimitOrder<Decimal> *order)
    {

    }

    void OrderCanceled (CoverAtLimitOrder<Decimal> *order)
    {

    }

    void OrderCanceled (CoverAtStopOrder<Decimal> *order)
    {

    }

    void OrderCanceled (SellAtStopOrder<Decimal> *order)
    {

    }

    const InstrumentPosition<Decimal>& 
    getInstrumentPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.getInstrumentPosition (tradingSymbol);
    }

    // Method automatically called when TradingPosition is closed
    void PositionClosed (TradingPosition<Decimal> *aPosition)
    {
      typename StrategyTransactionManager<Decimal>::StrategyTransactionIterator it =
	mStrategyTrades.findStrategyTransaction (aPosition->getPositionID());

      if (it != mStrategyTrades.endStrategyTransaction())
	{
	  mClosedTradeHistory.addClosedPosition (it->second->getTradingPositionPtr());
	}
      else
	throw StrategyBrokerException("Unable to find strategy transaction for position id " +std::to_string(aPosition->getPositionID()));
    }

  private:
    OHLCTimeSeriesEntry<Decimal> getEntryBar (const std::string& tradingSymbol,
							const boost::gregorian::date& d)
    {
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (tradingSymbol);
      if (symbolIterator != mPortfolio->endPortfolio())
	{
	  typename Security<Decimal>::ConstRandomAccessIterator it = 
	    symbolIterator->second->getRandomAccessIterator (d);

	  return (*it);
	}
      else
	throw StrategyBrokerException ("StrategyBroker::getEntryBar - Cannot find " +tradingSymbol +" in portfolio");
    }

    std::shared_ptr<TradingPositionLong<Decimal>>
    createLongTradingPosition (TradingOrder<Decimal> *order)
    {
      auto position = std::make_shared<TradingPositionLong<Decimal>> (order->getTradingSymbol(), 
								   order->getFillPrice(),
								   getEntryBar (order->getTradingSymbol(), 
										order->getFillDate()),
								   order->getUnitsInOrder());
      position->addObserver (*this);
      return position;
    }

    std::shared_ptr<TradingPositionShort<Decimal>>
    createShortTradingPosition (TradingOrder<Decimal> *order)
    {
      auto position = 
	std::make_shared<TradingPositionShort<Decimal>> (order->getTradingSymbol(), 
						      order->getFillPrice(),
						      getEntryBar (order->getTradingSymbol(), 
								   order->getFillDate()),
						      order->getUnitsInOrder());

      position->addObserver (*this);
      return position;
    }

    std::shared_ptr <StrategyTransaction<Decimal>>
    createStrategyTransaction (std::shared_ptr<TradingOrder<Decimal>> order,
			       std::shared_ptr<TradingPosition<Decimal>> position)
    {
      return std::make_shared<StrategyTransaction<Decimal>>(order, position);
    }

    template <typename T>
    void ExitOrderExecutedCommon (T *order)
    {
      InstrumentPosition<Decimal> instrumentPosition =
	mInstrumentPositionManager.getInstrumentPosition (order->getTradingSymbol());
      typename InstrumentPosition<Decimal>::ConstInstrumentPositionIterator positionIterator = 
	instrumentPosition.beginInstrumentPosition();
      typename StrategyTransactionManager<Decimal>::StrategyTransactionIterator transactionIterator;
      shared_ptr<StrategyTransaction<Decimal>> aTransaction;
      std::shared_ptr<TradingPosition<Decimal>> pos;
      //std::shared_ptr<T> exitOrder(*order);
      auto exitOrder = std::make_shared<T>(*order);

      for (; positionIterator != instrumentPosition.endInstrumentPosition(); positionIterator++)
	{
	  pos = *positionIterator;
	  transactionIterator = mStrategyTrades.findStrategyTransaction (pos->getPositionID());
	  if (transactionIterator != mStrategyTrades.endStrategyTransaction())
	    {
	      aTransaction = transactionIterator->second;
	      aTransaction->completeTransaction (exitOrder);
	    }
	  else
	    {
	      throw StrategyBrokerException("Unable to find StrategyTransaction for symbol: " +order->getTradingSymbol());
	    }
	}

      mInstrumentPositionManager.closeAllPositions (order->getTradingSymbol(),
						    order->getFillDate(),
						    order->getFillPrice()); 
      
    }


  private:
    TradingOrderManager<Decimal> mOrderManager;
    InstrumentPositionManager<Decimal> mInstrumentPositionManager;
    StrategyTransactionManager<Decimal> mStrategyTrades;
    ClosedPositionHistory<Decimal> mClosedTradeHistory;
    std::shared_ptr<Portfolio<Decimal>> mPortfolio;
  };


}



#endif
