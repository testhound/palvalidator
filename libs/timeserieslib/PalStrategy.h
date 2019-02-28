// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PAL_STRATEGY_H
#define __PAL_STRATEGY_H 1

#include <exception>
#include <vector>
#include <map>
#include "MCPTStrategyAttributes.h"
#include "PalAst.h"
#include "BacktesterStrategy.h"
#include "PALPatternInterpreter.h"

namespace mkc_timeseries
{
  using boost::gregorian::date;

  template <class Decimal> class PalStrategy : public BacktesterStrategy<Decimal>
    {
    public:
    PalStrategy(const std::string& strategyName,
		std::shared_ptr<PriceActionLabPattern> pattern,
		std::shared_ptr<Portfolio<Decimal>> portfolio)
      : BacktesterStrategy<Decimal>(strategyName, portfolio),
	mPalPattern(pattern),
	mMCPTAttributes()
	{}

      PalStrategy(const PalStrategy<Decimal>& rhs)
	: BacktesterStrategy<Decimal>(rhs),
	  mPalPattern(rhs.mPalPattern),
	  mMCPTAttributes(rhs.mMCPTAttributes)
      {}

      const PalStrategy<Decimal>&
      operator=(const PalStrategy<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	BacktesterStrategy<Decimal>::operator=(rhs);
	mPalPattern = rhs.mPalPattern;
	mMCPTAttributes = rhs.mMCPTAttributes;
	return *this;
      }

      virtual ~PalStrategy()
      {}

      virtual std::shared_ptr<PalStrategy<Decimal>>
      clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const = 0;

      const TradingVolume& getSizeForOrder(const Security<Decimal>& aSecurity) const
      {
	if (aSecurity.isEquitySecurity())
	  return OneShare;
	else
	  return OneContract;
      }

      uint32_t getPatternMaxBarsBack() const
      {
	return mPalPattern->getMaxBarsBack();
      }

      std::shared_ptr<PriceActionLabPattern> getPalPattern() const
      {
	return mPalPattern;
      }

      std::vector<int> getPositionDirectionVector() const
      {
	return mMCPTAttributes.getPositionDirection();
      }

      std::vector<Decimal> getPositionReturnsVector() const
      {
	return mMCPTAttributes.getPositionReturns();
      }

      unsigned long numTradingOpportunities() const
      {
	return mMCPTAttributes.numTradingOpportunities();
      }

    protected:
      void addLongPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
      {
	mMCPTAttributes.addLongPositionBar (aSecurity, processingDate);
      }

      void addShortPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
      {
	mMCPTAttributes.addShortPositionBar (aSecurity, processingDate);
      }

      void addFlatPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
      {
	mMCPTAttributes.addFlatPositionBar (aSecurity, processingDate);
      }

    private:
      std::shared_ptr<PriceActionLabPattern> mPalPattern;
      MCPTStrategyAttributes<Decimal> mMCPTAttributes;
      static TradingVolume OneShare;
      static TradingVolume OneContract;
    };

template <class Decimal> TradingVolume PalStrategy<Decimal>::OneShare(1, TradingVolume::SHARES);
template <class Decimal> TradingVolume PalStrategy<Decimal>::OneContract(1, TradingVolume::CONTRACTS);

  template <class Decimal> class PalLongStrategy : public PalStrategy<Decimal>
    {
    public:
    PalLongStrategy(const std::string& strategyName,
		    std::shared_ptr<PriceActionLabPattern> pattern,
		    std::shared_ptr<Portfolio<Decimal>> portfolio)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio)
	{}

      PalLongStrategy(const PalLongStrategy<Decimal>& rhs)
	: PalStrategy<Decimal>(rhs)
      {}

      const PalLongStrategy<Decimal>&
      operator=(const PalLongStrategy<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	PalStrategy<Decimal>::operator=(rhs);

	return *this;
      }

      ~PalLongStrategy()
      {}
      
      std::shared_ptr<BacktesterStrategy<Decimal>> 
      clone (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
	return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
						       this->getPalPattern(),
						       portfolio);
      }

      std::shared_ptr<PalStrategy<Decimal>> 
      clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
	return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
						       this->getPalPattern(),
						       portfolio);
      }

      std::shared_ptr<BacktesterStrategy<Decimal>> 
      cloneForBackTesting () const
      {
	return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
						       this->getPalPattern(),
						       this->getPortfolio());
      }

      void eventExitOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			    const InstrumentPosition<Decimal>& instrPos,
			    const date& processingDate)
      {
	if (this->isLongPosition (aSecurity->getSymbol()))
	  {
	    std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
	    Decimal target = pattern->getProfitTargetAsDecimal();
	    //std::cout << "PalLongStrategy::eventExitOrders, getProfitTargetAsDecimal(): " << pattern->getProfitTargetAsDecimal() << std::endl << std::endl;
	    PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);
	    //std::cout << "PalLongStrategy::eventExitOrders, createPercentNumber(): " << targetAsPercent.getAsPercent() << std::endl << std::endl;

	    Decimal stop = pattern->getStopLossAsDecimal();
	    PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);

	    Decimal fillPrice = instrPos.getFillPrice();

	    //std::cout << "PalLongStrategy::eventExitOrders, fill Price =  " << fillPrice << std::endl;
	    this->ExitLongAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitLongAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (LongStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());

	    this->addLongPositionBar (aSecurity, processingDate);
	  }
      }

      void eventEntryOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			     const InstrumentPosition<Decimal>& instrPos,
			     const date& processingDate)
      {
	if (this->isFlatPosition (aSecurity->getSymbol()))
	  {
	    if (this->getSecurityBarNumber(aSecurity->getSymbol()) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
		typename Security<Decimal>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (PALPatternInterpreter<Decimal>::evaluateExpression (expr, aSecurity, it))
		  {
		    this->EnterLongOnOpen (aSecurity->getSymbol(), processingDate);
		    //std::cout << "PalLongStrategy entered LongOnOpen Order on " << processingDate << std::endl;
		  }
		this->addFlatPositionBar (aSecurity, processingDate);
	      }
	  }
      }
  };

  //

  template <class Decimal> class PalShortStrategy : public PalStrategy<Decimal>
    {
    public:
    PalShortStrategy(const std::string& strategyName,
		     std::shared_ptr<PriceActionLabPattern> pattern,
		     std::shared_ptr<Portfolio<Decimal>> portfolio)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio)
	{}

      PalShortStrategy(const PalShortStrategy<Decimal>& rhs)
	: PalStrategy<Decimal>(rhs)
      {}

      const PalShortStrategy<Decimal>&
      operator=(const PalShortStrategy<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	PalStrategy<Decimal>::operator=(rhs);

	return *this;
      }

      ~PalShortStrategy()
      {}

      std::shared_ptr<BacktesterStrategy<Decimal>> 
      clone (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
	return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
						       this->getPalPattern(),
						       portfolio);
      }

      std::shared_ptr<PalStrategy<Decimal>> 
      clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
	return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
						       this->getPalPattern(),
						       portfolio);
      }

      std::shared_ptr<BacktesterStrategy<Decimal>> 
      cloneForBackTesting () const
      {
	return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
						       this->getPalPattern(),
						       this->getPortfolio());
      }
      void eventExitOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			    const InstrumentPosition<Decimal>& instrPos,
			    const date& processingDate)
      {
	//std::cout << "PalShortStrategy::eventExitOrders - In eventExitOrders" << std::endl;
	if (this->isShortPosition (aSecurity->getSymbol()))
	  {
	    //std::cout << "!!!! PalShortStrategy::eventExitOrders - isShortPosition true" << std::endl << std::endl;
	    std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
	    Decimal target = pattern->getProfitTargetAsDecimal();

	    //std::cout << "PalShortStrategy::eventExitOrders, getProfitTargetAsDecimal(): " << pattern->getProfitTargetAsDecimal() << std::endl << std::endl;
	    PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);
	    //std::cout << "PalShortStrategy::eventExitOrders, createPercentNumber(): " << targetAsPercent.getAsPercent() << std::endl << std::endl;

	    Decimal stop = pattern->getStopLossAsDecimal();
	    //std::cout << "PalShortStrategy::eventExitOrders, getStopLossAsDecimal(): " << pattern->getStopLossAsDecimal() << std::endl << std::endl;
	    PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);

	    Decimal fillPrice = instrPos.getFillPrice();
	    //std::cout << "PalShortStrategy::eventExitOrders, fill Price =  " << fillPrice << std::endl;

	    this->ExitShortAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitShortAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (ShortStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());
	    this->addShortPositionBar (aSecurity, processingDate);
	  }
      }

      void eventEntryOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			     const InstrumentPosition<Decimal>& instrPos,
			     const date& processingDate)
      {
	if (this->isFlatPosition (aSecurity->getSymbol()))
	  {
	    if (this->getSecurityBarNumber(aSecurity->getSymbol()) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
		typename Security<Decimal>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (PALPatternInterpreter<Decimal>::evaluateExpression (expr, aSecurity, it))
		  {
		    //std::cout << "PalShortStrategy entered ShortOnOpen Order on " << processingDate << std::endl;
		    this->EnterShortOnOpen (aSecurity->getSymbol(), processingDate);
		  }
		this->addFlatPositionBar (aSecurity, processingDate);
	      }
	  }
      }
  };
}

#endif
