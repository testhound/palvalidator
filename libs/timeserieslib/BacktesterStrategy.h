// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __BACKTESTER_STRATEGY_H
#define __BACKTESTER_STRATEGY_H 1

#include <exception>
#include <vector>
#include "Portfolio.h"
#include "InstrumentPosition.h"
#include "StrategyBroker.h"
#include "SecurityBacktestProperties.h"


namespace mkc_timeseries
{
  using dec::decimal;
  using boost::gregorian::date;

  template <int Prec> class BacktesterStrategy
    {
    public:
      typedef typename Portfolio<Prec>::ConstPortfolioIterator PortfolioIterator;

      BacktesterStrategy(const BacktesterStrategy<Prec>& rhs)
	: mStrategyName(rhs.mStrategyName),
	  mBroker(rhs.mBroker),
	  mPortfolio(rhs.mPortfolio),
	  mSecuritiesProperties(rhs.mSecuritiesProperties)
      {}

      const BacktesterStrategy<Prec>&
      operator=(const BacktesterStrategy<Prec>& rhs)
      {
	if (this == &rhs)
	  return *this;

	mStrategyName = rhs.mStrategyName;
	mBroker = rhs.mBroker;
	mPortfolio = rhs.mPortfolio;
	mSecuritiesProperties = rhs.mSecuritiesProperties;

	return *this;
      }

      const std::string& getStrategyName() const
      {
	return mStrategyName;
      }

      virtual ~BacktesterStrategy()
      {}

      virtual void eventExitOrders (std::shared_ptr<Security<Prec>> aSecurity,
				    const InstrumentPosition<Prec>& instrPos,
				    const date& processingDate) = 0;

      virtual void eventEntryOrders (std::shared_ptr<Security<Prec>> aSecurity,
				     const InstrumentPosition<Prec>& instrPos,
				     const date& processingDate) = 0;

      virtual const TradingVolume& getSizeForOrder(const Security<Prec>& aSecurity) const = 0;

      virtual std::shared_ptr<BacktesterStrategy<Prec>> 
      clone (std::shared_ptr<Portfolio<Prec>> portfolio) const = 0;

      virtual std::shared_ptr<BacktesterStrategy<Prec>> 
      cloneForBackTesting () const = 0;

      virtual std::vector<int> getPositionDirectionVector() const = 0;

      virtual std::vector<decimal<Prec>> getPositionReturnsVector() const = 0;

      virtual unsigned long numTradingOpportunities() const = 0;

      bool isLongPosition(const std::string& tradingSymbol) const
      {
	return mBroker.isLongPosition (tradingSymbol);
      }

      bool isShortPosition(const std::string& tradingSymbol) const
      {
	return mBroker.isShortPosition (tradingSymbol);
      }

      bool isFlatPosition(const std::string& tradingSymbol) const
      {
	return mBroker.isFlatPosition (tradingSymbol);
      }

      PortfolioIterator beginPortfolio() const
      {
	return mPortfolio->beginPortfolio();
      }

      PortfolioIterator endPortfolio() const
      {
	return mPortfolio->endPortfolio();
      }

      uint32_t getNumSecurities() const
      {
	return mPortfolio->getNumSecurities();
      }

      void EnterLongOnOpen(const std::string& tradingSymbol, 	
			   const date& orderDate)
      {
	auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
	mBroker.EnterLongOnOpen (tradingSymbol, orderDate, 
				 getSizeForOrder(*aSecurity)); 
      }

      void EnterShortOnOpen(const std::string& tradingSymbol,	
			    const date& orderDate)
      {
	auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
	mBroker.EnterShortOnOpen (tradingSymbol, orderDate, 
				  getSizeForOrder(*aSecurity)); 
      }

      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const decimal<Prec>& limitPrice)
      {
	mBroker.ExitLongAllUnitsAtLimit (tradingSymbol, orderDate, limitPrice);
      }

      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const decimal<Prec>& limitBasePrice,
				 const PercentNumber<Prec>& percentNum)
      {
	//std::cout << "BacktesterStrategy::ExitLongAllUnitsAtLimit - limitBasePrice: " << limitBasePrice << " percentNum = " << percentNum.getAsPercent() << std::endl << std::endl;
	mBroker.ExitLongAllUnitsAtLimit (tradingSymbol, orderDate, 
					 limitBasePrice, percentNum);
      }

      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				  const date& orderDate,
				  const decimal<Prec>& limitPrice)
      {
	mBroker.ExitShortAllUnitsAtLimit (tradingSymbol, orderDate, limitPrice);
      }

      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const decimal<Prec>& limitBasePrice,
				 const PercentNumber<Prec>& percentNum)
      {
	mBroker.ExitShortAllUnitsAtLimit (tradingSymbol, orderDate, 
					 limitBasePrice, percentNum);
      }

      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const decimal<Prec>& stopPrice)
      {
	mBroker.ExitLongAllUnitsAtStop (tradingSymbol, orderDate, stopPrice);
      }

      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const decimal<Prec>& stopBasePrice,
				const PercentNumber<Prec>& percentNum)
      {
	mBroker.ExitLongAllUnitsAtStop (tradingSymbol, orderDate, 
					 stopBasePrice, percentNum);
      }

      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const date& orderDate,
				 const decimal<Prec>& stopPrice)
      {
	mBroker.ExitShortAllUnitsAtStop (tradingSymbol, orderDate, stopPrice);
      }

      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const date& orderDate,
				 const decimal<Prec>& stopBasePrice,
				 const PercentNumber<Prec>& percentNum)
      {
	mBroker.ExitShortAllUnitsAtStop (tradingSymbol, orderDate, 
					 stopBasePrice, percentNum);
      }

      void eventProcessPendingOrders(const date& processingDate) 
      {
	mBroker.ProcessPendingOrders (processingDate);
      }

      void eventUpdateSecurityBarNumber(const std::string& tradingSymbol)
      {
	mSecuritiesProperties.updateBacktestBarNumber (tradingSymbol);
      }

      uint32_t getSecurityBarNumber(const std::string& tradingSymbol) const
      {
	return mSecuritiesProperties.getBacktestBarNumber (tradingSymbol); 
      }

      void setRMultipleStop (const std::string& tradingSymbol, 
			     const dec::decimal<Prec>& riskStop)
      {
	this->setRMultipleStop (tradingSymbol, riskStop, 1);

      }
      void setRMultipleStop (const std::string& tradingSymbol, 
			     const dec::decimal<Prec>& riskStop, 
			     uint32_t unitNumber)
      {
	InstrumentPosition<Prec> instrPos = getInstrumentPosition(tradingSymbol);
	instrPos.setRMultipleStop (riskStop, unitNumber);
      }

      const InstrumentPosition<Prec>& 
      getInstrumentPosition(const std::string& tradingSymbol) const
      {
	return mBroker.getInstrumentPosition(tradingSymbol);
      }
      
      // Checks to see if a security has trading data for a particular day.

      bool doesSecurityHaveTradingData (const Security<Prec>& aSecurity,
					const date& processingDate)
      {
	typename Security<Prec>::ConstRandomAccessIterator it = 
	  aSecurity.findTimeSeriesEntry (processingDate);

	return (it != aSecurity.getRandomAccessIteratorEnd());
      }

      const StrategyBroker<Prec>& getStrategyBroker() const
      {
	return mBroker;
      }

      std::shared_ptr<Portfolio<Prec>> getPortfolio() const
      {
	return mPortfolio;
      }

    protected:
      BacktesterStrategy (const std::string& strategyName,
			  std::shared_ptr<Portfolio<Prec>> portfolio) 
	: mStrategyName(strategyName),
	  mBroker (portfolio),
	  mPortfolio (portfolio),
	  mSecuritiesProperties()
	{
	  typename Portfolio<Prec>::ConstPortfolioIterator it =
	    mPortfolio->beginPortfolio();

	  for (; it != mPortfolio->endPortfolio(); it++)
	    {
	      mSecuritiesProperties.addSecurity (it->second->getSymbol());
	    }
	}

    private:
      std::string mStrategyName;
      StrategyBroker<Prec> mBroker;
      std::shared_ptr<Portfolio<Prec>> mPortfolio;
      SecurityBacktestPropertiesManager mSecuritiesProperties;
    };

}


#endif
