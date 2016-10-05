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
  using dec::decimal;
  using boost::gregorian::date;

  template <int Prec> class PalStrategy : public BacktesterStrategy<Prec>
    {
    public:
    PalStrategy(const std::string& strategyName,
		std::shared_ptr<PriceActionLabPattern> pattern,
		std::shared_ptr<Portfolio<Prec>> portfolio)
      : BacktesterStrategy<Prec>(strategyName, portfolio),
	mPalPattern(pattern),
	mMCPTAttributes()
	{}

      PalStrategy(const PalStrategy<Prec>& rhs)
	: BacktesterStrategy<Prec>(rhs),
	  mPalPattern(rhs.mPalPattern),
	  mMCPTAttributes(rhs.mMCPTAttributes)
      {}

      const PalStrategy<Prec>&
      operator=(const PalStrategy<Prec>& rhs)
      {
	if (this == &rhs)
	  return *this;

	BacktesterStrategy<Prec>::operator=(rhs);
	mPalPattern = rhs.mPalPattern;
	mMCPTAttributes = rhs.mMCPTAttributes;
	return *this;
      }

      virtual ~PalStrategy()
      {}

      const TradingVolume& getSizeForOrder(const Security<Prec>& aSecurity) const
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

      std::vector<decimal<Prec>> getPositionReturnsVector() const
      {
	return mMCPTAttributes.getPositionReturns();
      }

      unsigned long numTradingOpportunities() const
      {
	return mMCPTAttributes.numTradingOpportunities();
      }

    protected:
      void addLongPositionBar(std::shared_ptr<Security<Prec>> aSecurity,
			    const date& processingDate)
      {
	mMCPTAttributes.addLongPositionBar (aSecurity, processingDate);
      }

      void addShortPositionBar(std::shared_ptr<Security<Prec>> aSecurity,
			    const date& processingDate)
      {
	mMCPTAttributes.addShortPositionBar (aSecurity, processingDate);
      }

      void addFlatPositionBar(std::shared_ptr<Security<Prec>> aSecurity,
			    const date& processingDate)
      {
	mMCPTAttributes.addFlatPositionBar (aSecurity, processingDate);
      }

    private:
      std::shared_ptr<PriceActionLabPattern> mPalPattern;
      MCPTStrategyAttributes<Prec> mMCPTAttributes;
      static TradingVolume OneShare;
      static TradingVolume OneContract;
    };

template <int Prec> TradingVolume PalStrategy<Prec>::OneShare(1, TradingVolume::SHARES);
template <int Prec> TradingVolume PalStrategy<Prec>::OneContract(1, TradingVolume::CONTRACTS);

  template <int Prec> class PalLongStrategy : public PalStrategy<Prec>
    {
    public:
    PalLongStrategy(const std::string& strategyName,
		    std::shared_ptr<PriceActionLabPattern> pattern,
		    std::shared_ptr<Portfolio<Prec>> portfolio)
      : PalStrategy<Prec>(strategyName, pattern, portfolio)
	{}

      PalLongStrategy(const PalLongStrategy<Prec>& rhs)
	: PalStrategy<Prec>(rhs)
      {}

      const PalLongStrategy<Prec>&
      operator=(const PalLongStrategy<Prec>& rhs)
      {
	if (this == &rhs)
	  return *this;

	PalStrategy<Prec>::operator=(rhs);

	return *this;
      }

      ~PalLongStrategy()
      {}
      
      std::shared_ptr<BacktesterStrategy<Prec>> 
      clone (std::shared_ptr<Portfolio<Prec>> portfolio) const
      {
	return std::make_shared<PalLongStrategy<Prec>>(this->getStrategyName(),
						       this->getPalPattern(),
						       portfolio);
      }

      std::shared_ptr<BacktesterStrategy<Prec>> 
      cloneForBackTesting () const
      {
	return std::make_shared<PalLongStrategy<Prec>>(this->getStrategyName(),
						       this->getPalPattern(),
						       this->getPortfolio());
      }

      void eventExitOrders (std::shared_ptr<Security<Prec>> aSecurity,
			    const InstrumentPosition<Prec>& instrPos,
			    const date& processingDate)
      {
	if (this->isLongPosition (aSecurity->getSymbol()))
	  {
	    std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
	    decimal<Prec> target = pattern->getProfitTargetAsDecimal();
	    //std::cout << "PalLongStrategy::eventExitOrders, getProfitTargetAsDecimal(): " << pattern->getProfitTargetAsDecimal() << std::endl << std::endl;
	    PercentNumber<Prec> targetAsPercent = PercentNumber<Prec>::createPercentNumber (target);
	    //std::cout << "PalLongStrategy::eventExitOrders, createPercentNumber(): " << targetAsPercent.getAsPercent() << std::endl << std::endl;

	    decimal<Prec> stop = pattern->getStopLossAsDecimal();
	    PercentNumber<Prec> stopAsPercent = PercentNumber<Prec>::createPercentNumber (stop);

	    decimal<Prec> fillPrice = instrPos.getFillPrice();

	    //std::cout << "PalLongStrategy::eventExitOrders, fill Price =  " << fillPrice << std::endl;
	    this->ExitLongAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitLongAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (LongStopLoss<Prec> (fillPrice, stopAsPercent).getStopLoss());

	    this->addLongPositionBar (aSecurity, processingDate);
	  }
      }

      void eventEntryOrders (std::shared_ptr<Security<Prec>> aSecurity,
			     const InstrumentPosition<Prec>& instrPos,
			     const date& processingDate)
      {
	if (this->isFlatPosition (aSecurity->getSymbol()))
	  {
	    if (this->getSecurityBarNumber(aSecurity->getSymbol()) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
		typename Security<Prec>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (PALPatternInterpreter<Prec>::evaluateExpression (expr, aSecurity, it))
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

  template <int Prec> class PalShortStrategy : public PalStrategy<Prec>
    {
    public:
    PalShortStrategy(const std::string& strategyName,
		     std::shared_ptr<PriceActionLabPattern> pattern,
		     std::shared_ptr<Portfolio<Prec>> portfolio)
      : PalStrategy<Prec>(strategyName, pattern, portfolio)
	{}

      PalShortStrategy(const PalShortStrategy<Prec>& rhs)
	: PalStrategy<Prec>(rhs)
      {}

      const PalShortStrategy<Prec>&
      operator=(const PalShortStrategy<Prec>& rhs)
      {
	if (this == &rhs)
	  return *this;

	PalStrategy<Prec>::operator=(rhs);

	return *this;
      }

      ~PalShortStrategy()
      {}

      std::shared_ptr<BacktesterStrategy<Prec>> 
      clone (std::shared_ptr<Portfolio<Prec>> portfolio) const
      {
	return std::make_shared<PalShortStrategy<Prec>>(this->getStrategyName(),
						       this->getPalPattern(),
						       portfolio);
      }

      std::shared_ptr<BacktesterStrategy<Prec>> 
      cloneForBackTesting () const
      {
	return std::make_shared<PalShortStrategy<Prec>>(this->getStrategyName(),
						       this->getPalPattern(),
						       this->getPortfolio());
      }
      void eventExitOrders (std::shared_ptr<Security<Prec>> aSecurity,
			    const InstrumentPosition<Prec>& instrPos,
			    const date& processingDate)
      {
	//std::cout << "PalShortStrategy::eventExitOrders - In eventExitOrders" << std::endl;
	if (this->isShortPosition (aSecurity->getSymbol()))
	  {
	    //std::cout << "!!!! PalShortStrategy::eventExitOrders - isShortPosition true" << std::endl << std::endl;
	    std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
	    decimal<Prec> target = pattern->getProfitTargetAsDecimal();

	    //std::cout << "PalShortStrategy::eventExitOrders, getProfitTargetAsDecimal(): " << pattern->getProfitTargetAsDecimal() << std::endl << std::endl;
	    PercentNumber<Prec> targetAsPercent = PercentNumber<Prec>::createPercentNumber (target);
	    //std::cout << "PalShortStrategy::eventExitOrders, createPercentNumber(): " << targetAsPercent.getAsPercent() << std::endl << std::endl;

	    decimal<Prec> stop = pattern->getStopLossAsDecimal();
	    //std::cout << "PalShortStrategy::eventExitOrders, getStopLossAsDecimal(): " << pattern->getStopLossAsDecimal() << std::endl << std::endl;
	    PercentNumber<Prec> stopAsPercent = PercentNumber<Prec>::createPercentNumber (stop);

	    decimal<Prec> fillPrice = instrPos.getFillPrice();
	    //std::cout << "PalShortStrategy::eventExitOrders, fill Price =  " << fillPrice << std::endl;

	    this->ExitShortAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitShortAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (ShortStopLoss<Prec> (fillPrice, stopAsPercent).getStopLoss());
	    this->addShortPositionBar (aSecurity, processingDate);
	  }
      }

      void eventEntryOrders (std::shared_ptr<Security<Prec>> aSecurity,
			     const InstrumentPosition<Prec>& instrPos,
			     const date& processingDate)
      {
	if (this->isFlatPosition (aSecurity->getSymbol()))
	  {
	    if (this->getSecurityBarNumber(aSecurity->getSymbol()) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
		typename Security<Prec>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (PALPatternInterpreter<Prec>::evaluateExpression (expr, aSecurity, it))
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
