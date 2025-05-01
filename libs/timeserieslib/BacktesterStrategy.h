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
  using boost::gregorian::date;

  class StrategyOptions
  {
  public:
    StrategyOptions (bool pyramidingEnabled, unsigned int maxPyramidPositions)
      : mPyramidPositions (pyramidingEnabled),
	mMaxPyramidPositions(maxPyramidPositions)
    {}

    bool isPyramidingEnabled() const
      {
	return mPyramidPositions;
      }

    unsigned int getMaxPyramidPositions() const
      {
	return mMaxPyramidPositions;
      }

  private:
    bool mPyramidPositions;
    unsigned int mMaxPyramidPositions;
  };

  extern StrategyOptions defaultStrategyOptions;

  /**
   * @class BacktesterStrategy
   * @brief Base class for trading strategies used during backtesting.
   *
   * Responsibilities:
   * - Define strategy-specific entry and exit rules (pure virtual hooks).
   * - Submit orders using helpers like EnterLongOnOpen or ExitLongAllUnitsAtStop.
   * - Track pyramiding status, order state, and current simulation bar.
   * - Delegate execution responsibilities to a StrategyBroker instance.
   *
   * Observer Pattern Collaboration:
   * - Acts as a producer of orders, not an observer.
   * - Delegates order submission to StrategyBroker.
   * - Receives callbacks indirectly via changes in position state.
   *
   * Collaborators:
   * - StrategyBroker: receives order requests and manages lifecycle.
   * - BackTester: invokes strategy events on each simulation step.
   */
  template <class Decimal> class BacktesterStrategy
    {
    public:
      typedef typename Portfolio<Decimal>::ConstPortfolioIterator PortfolioIterator;

      BacktesterStrategy(const BacktesterStrategy<Decimal>& rhs)
	: mStrategyName(rhs.mStrategyName),
	  mBroker(rhs.mBroker),
	  mPortfolio(rhs.mPortfolio),
	  mSecuritiesProperties(rhs.mSecuritiesProperties),
	  mStrategyOptions(mStrategyOptions)
      {}

      const BacktesterStrategy<Decimal>&
      operator=(const BacktesterStrategy<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	mStrategyName = rhs.mStrategyName;
	mBroker = rhs.mBroker;
	mPortfolio = rhs.mPortfolio;
	mSecuritiesProperties = rhs.mSecuritiesProperties;
	mStrategyOptions = rhs.mStrategyOptions;
	    
	return *this;
      }

      const std::string& getStrategyName() const
      {
	return mStrategyName;
      }

      virtual ~BacktesterStrategy()
      {}

      virtual void eventExitOrders (const std::shared_ptr<Security<Decimal>>& aSecurity,
				    const InstrumentPosition<Decimal>& instrPos,
				    const date& processingDate) = 0;

      virtual void eventEntryOrders (const std::shared_ptr<Security<Decimal>>& aSecurity,
				     const InstrumentPosition<Decimal>& instrPos,
				     const date& processingDate) = 0;

      virtual const TradingVolume& getSizeForOrder(const Security<Decimal>& aSecurity) const = 0;

      virtual std::shared_ptr<BacktesterStrategy<Decimal>> 
      clone (const std::shared_ptr<Portfolio<Decimal>>& portfolio) const = 0;

      virtual std::shared_ptr<BacktesterStrategy<Decimal>> 
      cloneForBackTesting () const = 0;

      virtual std::vector<int> getPositionDirectionVector() const = 0;

      virtual std::vector<Decimal> getPositionReturnsVector() const = 0;

      virtual unsigned long numTradingOpportunities() const = 0;

      bool isPyramidingEnabled() const
      {
	  return mStrategyOptions.isPyramidingEnabled();
      }

      unsigned int getMaxPyramidPositions() const
      {
	return mStrategyOptions.getMaxPyramidPositions();
      }

      bool strategyCanPyramid(const std::string& tradingSymbol) const
      {
	if (isPyramidingEnabled())
	  {
	    InstrumentPosition<Decimal> instrPos = getInstrumentPosition(tradingSymbol);

	    // We can pyramid if the number of open positions is < 1 (intial position) +
	    // number of positions we are allowed to pyramid into

	    return instrPos.getNumPositionUnits() < (1 + getMaxPyramidPositions());
	  }
	return false;
      }

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

      void ExitAllPositions(const std::string& tradingSymbol,
			    const date& orderDate)
      {
	if (isLongPosition(tradingSymbol))
	  ExitLongAllUnitsAtOpen(tradingSymbol, orderDate);
	else if (isShortPosition(tradingSymbol))
	  ExitShortAllUnitsAtOpen(tradingSymbol, orderDate);
      }

      void EnterLongOnOpen(const std::string& tradingSymbol, 	
			   const date& orderDate,
			   const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			   const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
	auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
	mBroker.EnterLongOnOpen (tradingSymbol, orderDate, 
				 getSizeForOrder(*aSecurity),
				 stopLoss,
				 profitTarget); 
      }

      void EnterShortOnOpen(const std::string& tradingSymbol,	
			    const date& orderDate,
			    const Decimal& stopLoss = DecimalConstants<Decimal>::DecimalZero,
			    const Decimal& profitTarget = DecimalConstants<Decimal>::DecimalZero)
      {
	auto aSecurity = mPortfolio->findSecurity(tradingSymbol)->second;
	mBroker.EnterShortOnOpen (tradingSymbol, orderDate, 
				  getSizeForOrder(*aSecurity),
				  stopLoss,
				  profitTarget); 
      }

      void ExitLongAllUnitsAtOpen(const std::string& tradingSymbol,
				  const date& orderDate)
      {
	mBroker.ExitLongAllUnitsOnOpen(tradingSymbol, orderDate);
      }

      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitPrice)
      {
	mBroker.ExitLongAllUnitsAtLimit (tradingSymbol, orderDate, limitPrice);
      }

      void ExitLongAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitBasePrice,
				 const PercentNumber<Decimal>& percentNum)
      {
	//std::cout << "BacktesterStrategy::ExitLongAllUnitsAtLimit - limitBasePrice: " << limitBasePrice << " percentNum = " << percentNum.getAsPercent() << std::endl << std::endl;
	mBroker.ExitLongAllUnitsAtLimit (tradingSymbol, orderDate, 
					 limitBasePrice, percentNum);
      }

      void ExitShortAllUnitsAtOpen(const std::string& tradingSymbol,
				   const date& orderDate)
      {
	mBroker.ExitShortAllUnitsOnOpen(tradingSymbol, orderDate);
      }

      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				  const date& orderDate,
				  const Decimal& limitPrice)
      {
	mBroker.ExitShortAllUnitsAtLimit (tradingSymbol, orderDate, limitPrice);
      }

      void ExitShortAllUnitsAtLimit(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& limitBasePrice,
				 const PercentNumber<Decimal>& percentNum)
      {
	mBroker.ExitShortAllUnitsAtLimit (tradingSymbol, orderDate, 
					 limitBasePrice, percentNum);
      }

      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const Decimal& stopPrice)
      {
	mBroker.ExitLongAllUnitsAtStop (tradingSymbol, orderDate, stopPrice);
      }

      void ExitLongAllUnitsAtStop(const std::string& tradingSymbol,
				const date& orderDate,
				const Decimal& stopBasePrice,
				const PercentNumber<Decimal>& percentNum)
      {
	mBroker.ExitLongAllUnitsAtStop (tradingSymbol, orderDate, 
					 stopBasePrice, percentNum);
      }

      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& stopPrice)
      {
	mBroker.ExitShortAllUnitsAtStop (tradingSymbol, orderDate, stopPrice);
      }

      void ExitShortAllUnitsAtStop(const std::string& tradingSymbol,
				 const date& orderDate,
				 const Decimal& stopBasePrice,
				 const PercentNumber<Decimal>& percentNum)
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
			     const Decimal& riskStop)
      {
	this->setRMultipleStop (tradingSymbol, riskStop, 1);

      }
      void setRMultipleStop (const std::string& tradingSymbol, 
			     const Decimal& riskStop, 
			     uint32_t unitNumber)
      {
	InstrumentPosition<Decimal> instrPos = getInstrumentPosition(tradingSymbol);
	instrPos.setRMultipleStop (riskStop, unitNumber);
      }

      const InstrumentPosition<Decimal>& 
      getInstrumentPosition(const std::string& tradingSymbol) const
      {
	return mBroker.getInstrumentPosition(tradingSymbol);
      }
      
      // Checks to see if a security has trading data for a particular day.

      bool doesSecurityHaveTradingData (const Security<Decimal>& aSecurity,
					const date& processingDate)
      {
	typename Security<Decimal>::ConstRandomAccessIterator it = 
	  aSecurity.findTimeSeriesEntry (processingDate);

	return (it != aSecurity.getRandomAccessIteratorEnd());
      }

      const StrategyBroker<Decimal>& getStrategyBroker() const
      {
	return mBroker;
      }

      std::shared_ptr<Portfolio<Decimal>> getPortfolio() const
      {
	return mPortfolio;
      }

    protected:
      BacktesterStrategy (const std::string& strategyName,
			  std::shared_ptr<Portfolio<Decimal>> portfolio,
			  const StrategyOptions& strategyOptions) 
	: mStrategyName(strategyName),
	  mBroker (portfolio),
	  mPortfolio (portfolio),
	  mSecuritiesProperties(),
	  mStrategyOptions(strategyOptions)
	{
	  typename Portfolio<Decimal>::ConstPortfolioIterator it =
	    mPortfolio->beginPortfolio();

	  for (; it != mPortfolio->endPortfolio(); it++)
	    {
	      mSecuritiesProperties.addSecurity (it->second->getSymbol());
	    }
	}

    private:
      std::string mStrategyName;
      StrategyBroker<Decimal> mBroker;
      std::shared_ptr<Portfolio<Decimal>> mPortfolio;
      SecurityBacktestPropertiesManager mSecuritiesProperties;
      StrategyOptions mStrategyOptions;
      static TradingVolume OneShare;
      static TradingVolume OneContract;
    };

  template <class Decimal>
  const TradingVolume&
    BacktesterStrategy<Decimal>::getSizeForOrder(const Security<Decimal>& aSecurity) const
  {
    if (aSecurity.isEquitySecurity())
      return OneShare;
    else
      return OneContract;
  }

  template <class Decimal> TradingVolume BacktesterStrategy<Decimal>::OneShare(1, TradingVolume::SHARES);
  template <class Decimal> TradingVolume BacktesterStrategy<Decimal>::OneContract(1, TradingVolume::CONTRACTS);

}


#endif
