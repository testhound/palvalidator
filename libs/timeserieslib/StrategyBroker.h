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

  template <int Prec> class StrategyBroker : 
    public TradingOrderObserver<Prec>, 
    public TradingPositionObserver<Prec>
  {
  public:
    typedef typename TradingOrderManager<Prec>::PendingOrderIterator PendingOrderIterator;
    typedef typename StrategyTransactionManager<Prec>::SortedStrategyTransactionIterator 
      StrategyTransactionIterator;
    typedef typename ClosedPositionHistory<Prec>::ConstPositionIterator ClosedPositionIterator;

  public:
    StrategyBroker (std::shared_ptr<Portfolio<Prec>> portfolio)
      : TradingOrderObserver<Prec>(),
	TradingPositionObserver<Prec>(),
	mOrderManager(portfolio),
	mInstrumentPositionManager(),
	mStrategyTrades(),
	mClosedTradeHistory(),
	mPortfolio(portfolio)
    {
      mOrderManager.addObserver (*this);
      typename Portfolio<Prec>::ConstPortfolioIterator symbolIterator = mPortfolio->beginPortfolio();

      for (; symbolIterator != mPortfolio->endPortfolio(); symbolIterator++)
	  mInstrumentPositionManager.addInstrument(symbolIterator->second->getSymbol());
    }

    ~StrategyBroker()
    {}

    StrategyBroker(const StrategyBroker<Prec> &rhs)
      : TradingOrderObserver<Prec>(rhs),
	TradingPositionObserver<Prec>(rhs),
	mOrderManager(rhs.mOrderManager),
	mInstrumentPositionManager(rhs.mInstrumentPositionManager),
	mStrategyTrades(rhs.mStrategyTrades),
	mClosedTradeHistory(rhs.mClosedTradeHistory),
	mPortfolio(rhs.mPortfolio)
    {}

    StrategyBroker<Prec>& 
    operator=(const StrategyBroker<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      TradingOrderObserver<Prec>::operator=(rhs);
      TradingPositionObserver<Prec>::operator=(rhs);

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

    const ClosedPositionHistory<Prec>&
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
      auto order = std::make_shared<MarketOnOpenLongOrder<Prec>>(tradingSymbol,
								  unitsInOrder,
								  orderDate);
      
      mOrderManager.addTradingOrder (order);
    }

    void EnterShortOnOpen(const std::string& tradingSymbol,	
			  const date& orderDate,
			  const TradingVolume& unitsInOrder )
    {
      auto order = std::make_shared<MarketOnOpenShortOrder<Prec>>(tradingSymbol,
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
	  auto order = std::make_shared<MarketOnOpenSellOrder<Prec>>(tradingSymbol,
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
	  auto order = std::make_shared<MarketOnOpenCoverOrder<Prec>>(tradingSymbol,
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
				 const decimal<Prec>& limitPrice)
    {
      //std::cout << "Entering long profit target at: " << limitPrice << " on date: " << orderDate << std::endl;

      if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<SellAtLimitOrder<Prec>>(tradingSymbol,
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
				 const decimal<Prec>& limitBasePrice,
				 const PercentNumber<Prec>& percentNum)
    {
      //std::cout << "StrategyBroker::ExitLongAllUnitsAtLimit - limitBasePrice: " << limitBasePrice << " percentNum = " << percentNum.getAsPercent() << std::endl << std::endl;
      LongProfitTarget<Prec> profitTarget(limitBasePrice, percentNum);
      
      this->ExitLongAllUnitsAtLimit (tradingSymbol, orderDate, profitTarget.getProfitTarget());
    }

    void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				  const date& orderDate,
				  const decimal<Prec>& limitPrice)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<CoverAtLimitOrder<Prec>>(tradingSymbol,
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
				 const decimal<Prec>& limitBasePrice,
				 const PercentNumber<Prec>& percentNum)
    {
      //std::cout << "StrategyBroker::ExitShortAllUnitsAtLimit - limitBasePrice: " << limitBasePrice << " percentNum = " << percentNum.getAsPercent() << std::endl << std::endl;
      ShortProfitTarget<Prec> percentTarget(limitBasePrice, percentNum);

      decimal<Prec> profitTarget(percentTarget.getProfitTarget());
      //std::cout << "StrategyBroker::ExitShortAllUnitsAtLimit - short profit target = " << profitTarget << " on date: " << orderDate << std::endl << std::endl;

      
      this->ExitShortAllUnitsAtLimit (tradingSymbol,orderDate,profitTarget);
    }

    
    void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const decimal<Prec>& stopPrice)
    {
      if (mInstrumentPositionManager.isLongPosition (tradingSymbol))
	{
	  auto order = std::make_shared<SellAtStopOrder<Prec>>(tradingSymbol,
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
				const decimal<Prec>& stopBasePrice,
				const PercentNumber<Prec>& percentNum)
    {
      LongStopLoss<Prec> percentStop(stopBasePrice, percentNum);
      decimal<Prec> stopLoss(percentStop.getStopLoss());

      //std::cout << "Entering long stop loss at: " << stopLoss << " on date: " << orderDate << std::endl;
      this->ExitLongAllUnitsAtStop(tradingSymbol, orderDate, stopLoss);
    }

    void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const date& orderDate,
				 const decimal<Prec>& stopPrice)
    {
      if (mInstrumentPositionManager.isShortPosition (tradingSymbol))
	{
	  auto order = std::make_shared<CoverAtStopOrder<Prec>>(tradingSymbol,
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
				 const decimal<Prec>& stopBasePrice,
				 const PercentNumber<Prec>& percentNum)
    {
      ShortStopLoss<Prec> aPercentStop(stopBasePrice, percentNum);
      decimal<Prec> stopLoss(aPercentStop.getStopLoss());
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

    void OrderExecuted (MarketOnOpenLongOrder<Prec> *order)
    {
      auto position = createLongTradingPosition (order);
      auto pOrder = std::make_shared<MarketOnOpenLongOrder<Prec>>(*order);

      mInstrumentPositionManager.addPosition (position);
      mStrategyTrades.addStrategyTransaction (createStrategyTransaction (pOrder, position));
    }

    
    void OrderExecuted (MarketOnOpenShortOrder<Prec> *order)
    {
      auto position = createShortTradingPosition (order);
      auto pOrder = std::make_shared<MarketOnOpenShortOrder<Prec>>(*order);

      mInstrumentPositionManager.addPosition (position);
      mStrategyTrades.addStrategyTransaction (createStrategyTransaction (pOrder, position));
    }

    void OrderExecuted (MarketOnOpenSellOrder<Prec> *order)
    {
      ExitOrderExecutedCommon<MarketOnOpenSellOrder<Prec>>(order);
    }

    void OrderExecuted (MarketOnOpenCoverOrder<Prec> *order)
    {
      ExitOrderExecutedCommon<MarketOnOpenCoverOrder<Prec>>(order);
    }

    void OrderExecuted (SellAtLimitOrder<Prec> *order)
    {
      ExitOrderExecutedCommon<SellAtLimitOrder<Prec>>(order);
    }

    void OrderExecuted (CoverAtLimitOrder<Prec> *order)
    {
      //std::cout << "Short profit target of " << order->getFillPrice() << " reached on " << order->getFillDate() << std::endl;

      ExitOrderExecutedCommon<CoverAtLimitOrder<Prec>>(order);
    }

    void OrderExecuted (CoverAtStopOrder<Prec> *order)
    {
      ExitOrderExecutedCommon<CoverAtStopOrder<Prec>>(order);
    }

    void OrderExecuted (SellAtStopOrder<Prec> *order)
    {
      ExitOrderExecutedCommon<SellAtStopOrder<Prec>>(order);
    }

    void OrderCanceled (MarketOnOpenLongOrder<Prec> *order)
    {

    }

    void OrderCanceled (MarketOnOpenShortOrder<Prec> *order)
    {

    }

    void OrderCanceled (MarketOnOpenSellOrder<Prec> *order)
    {

    }

    void OrderCanceled (MarketOnOpenCoverOrder<Prec> *order)
    {

    }

    void OrderCanceled (SellAtLimitOrder<Prec> *order)
    {

    }

    void OrderCanceled (CoverAtLimitOrder<Prec> *order)
    {

    }

    void OrderCanceled (CoverAtStopOrder<Prec> *order)
    {

    }

    void OrderCanceled (SellAtStopOrder<Prec> *order)
    {

    }

    const InstrumentPosition<Prec>& 
    getInstrumentPosition(const std::string& tradingSymbol) const
    {
      return mInstrumentPositionManager.getInstrumentPosition (tradingSymbol);
    }

    // Method automatically called when TradingPosition is closed
    void PositionClosed (TradingPosition<Prec> *aPosition)
    {
      typename StrategyTransactionManager<Prec>::StrategyTransactionIterator it =
	mStrategyTrades.findStrategyTransaction (aPosition->getPositionID());

      if (it != mStrategyTrades.endStrategyTransaction())
	{
	  mClosedTradeHistory.addClosedPosition (it->second->getTradingPositionPtr());
	}
      else
	throw StrategyBrokerException("Unable to find strategy transaction for position id " +std::to_string(aPosition->getPositionID()));
    }

  private:
    OHLCTimeSeriesEntry<Prec> getEntryBar (const std::string& tradingSymbol,
							const boost::gregorian::date& d)
    {
      typename Portfolio<Prec>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (tradingSymbol);
      if (symbolIterator != mPortfolio->endPortfolio())
	{
	  typename Security<Prec>::ConstRandomAccessIterator it = 
	    symbolIterator->second->getRandomAccessIterator (d);

	  return (*it);
	}
      else
	throw StrategyBrokerException ("StrategyBroker::getEntryBar - Cannot find " +tradingSymbol +" in portfolio");
    }

    std::shared_ptr<TradingPositionLong<Prec>>
    createLongTradingPosition (TradingOrder<Prec> *order)
    {
      auto position = std::make_shared<TradingPositionLong<Prec>> (order->getTradingSymbol(), 
								   order->getFillPrice(),
								   getEntryBar (order->getTradingSymbol(), 
										order->getFillDate()),
								   order->getUnitsInOrder());
      position->addObserver (*this);
      return position;
    }

    std::shared_ptr<TradingPositionShort<Prec>>
    createShortTradingPosition (TradingOrder<Prec> *order)
    {
      auto position = 
	std::make_shared<TradingPositionShort<Prec>> (order->getTradingSymbol(), 
						      order->getFillPrice(),
						      getEntryBar (order->getTradingSymbol(), 
								   order->getFillDate()),
						      order->getUnitsInOrder());

      position->addObserver (*this);
      return position;
    }

    std::shared_ptr <StrategyTransaction<Prec>>
    createStrategyTransaction (std::shared_ptr<TradingOrder<Prec>> order,
			       std::shared_ptr<TradingPosition<Prec>> position)
    {
      return std::make_shared<StrategyTransaction<Prec>>(order, position);
    }

    template <typename T>
    void ExitOrderExecutedCommon (T *order)
    {
      InstrumentPosition<Prec> instrumentPosition =
	mInstrumentPositionManager.getInstrumentPosition (order->getTradingSymbol());
      typename InstrumentPosition<Prec>::ConstInstrumentPositionIterator positionIterator = 
	instrumentPosition.beginInstrumentPosition();
      typename StrategyTransactionManager<Prec>::StrategyTransactionIterator transactionIterator;
      shared_ptr<StrategyTransaction<Prec>> aTransaction;
      std::shared_ptr<TradingPosition<Prec>> pos;
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
    TradingOrderManager<Prec> mOrderManager;
    InstrumentPositionManager<Prec> mInstrumentPositionManager;
    StrategyTransactionManager<Prec> mStrategyTrades;
    ClosedPositionHistory<Prec> mClosedTradeHistory;
    std::shared_ptr<Portfolio<Prec>> mPortfolio;
  };


}



#endif
