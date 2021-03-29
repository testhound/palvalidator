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
#include <list>
#include "MCPTStrategyAttributes.h"
#include "PalAst.h"
#include "BacktesterStrategy.h"
#include "PALPatternInterpreter.h"

namespace mkc_timeseries
{
  using boost::gregorian::date;

  class PalStrategyException : public std::runtime_error
  {
  public:
    PalStrategyException(const std::string msg) 
      : std::runtime_error(msg)
    {}
    
    ~PalStrategyException()
    {}
    
  };

  // A PalMetaStrategy is composed of individual Pal strategies (patterns): long and/or short

  template <class Decimal> class PalMetaStrategy : public BacktesterStrategy<Decimal>
  {
  public:
    typedef typename std::list<shared_ptr<PriceActionLabPattern>> PalPatterns;
    typedef typename PalPatterns::const_iterator ConstStrategiesIterator;

    PalMetaStrategy(const std::string& strategyName,
		    std::shared_ptr<Portfolio<Decimal>> portfolio,
		    const StrategyOptions& strategyOptions = defaultStrategyOptions)
      : BacktesterStrategy<Decimal>(strategyName, portfolio, strategyOptions),
      mPalPatterns(),
      mMCPTAttributes(),
      mStrategyMaxBarsBack(0)
    {}

    PalMetaStrategy(const PalMetaStrategy<Decimal>& rhs)
	: BacktesterStrategy<Decimal>(rhs),
      mPalPatterns(rhs.mPalPatterns),
      mMCPTAttributes(rhs.mMCPTAttributes),
      mStrategyMaxBarsBack(rhs.mStrategyMaxBarsBack)
      {}

    const PalMetaStrategy<Decimal>&
    operator=(const PalMetaStrategy<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	BacktesterStrategy<Decimal>::operator=(rhs);
	mPalPatterns = rhs.mPalPatterns;
	mMCPTAttributes = rhs.mMCPTAttributes;
	mStrategyMaxBarsBack = rhs.mStrategyMaxBarsBack;
	return *this;
      }

    virtual ~PalMetaStrategy()
      {}

    void addPricePattern(std::shared_ptr<PriceActionLabPattern> pattern)
      {
	if (pattern->getMaxBarsBack() > mStrategyMaxBarsBack)
	  mStrategyMaxBarsBack = pattern->getMaxBarsBack();

	mPalPatterns.push_back(pattern);
      }

    uint32_t getPatternMaxBarsBack() const
    {
	return mStrategyMaxBarsBack;
    }

    std::shared_ptr<PriceActionLabPattern> getPalPattern() const
    {
      throw PalStrategyException("PalMetaStrategy::getPalPattern not implemented.");
    }

    ConstStrategiesIterator beginPricePatterns() const
    {
      return mPalPatterns.begin();
    }

    ConstStrategiesIterator endPricePatterns() const
    {
      return mPalPatterns.end();
    }

    const TradingVolume& getSizeForOrder(const Security<Decimal>& aSecurity) const
      {
	return BacktesterStrategy<Decimal>::getSizeForOrder(aSecurity);
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

    std::shared_ptr<BacktesterStrategy<Decimal>> 
    clone (std::shared_ptr<Portfolio<Decimal>> portfolio) const
    {
      return std::make_shared<PalMetaStrategy<Decimal>>(this->getStrategyName(),
							portfolio);
    }

    std::shared_ptr<BacktesterStrategy<Decimal>> 
    cloneForBackTesting () const
    {
      return std::make_shared<PalMetaStrategy<Decimal>>(this->getStrategyName(),
							this->getPortfolio());
    }

    void eventEntryOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			   const InstrumentPosition<Decimal>& instrPos,
			   const date& processingDate)
    {
      typename PalMetaStrategy<Decimal>::ConstStrategiesIterator it = this->beginPricePatterns();

      if (this->isFlatPosition (aSecurity->getSymbol()))
	{
	  for (; it != this->endPricePatterns(); it++)
	    {
	      std::shared_ptr<PriceActionLabPattern> pricePattern = *it;
	      if (this->getSecurityBarNumber(aSecurity->getSymbol()) > 
		  pricePattern->getMaxBarsBack())
		{
		  PatternExpression *expr = pricePattern->getPatternExpression().get();
		  typename Security<Decimal>::ConstRandomAccessIterator it = 
		    aSecurity->getRandomAccessIterator (processingDate);

		  if (PALPatternInterpreter<Decimal>::evaluateExpression (expr, aSecurity, it))
		    {
		      if (pricePattern->isLongPattern())
			this->EnterLongOnOpen (aSecurity->getSymbol(), processingDate);
		      else
			this->EnterShortOnOpen (aSecurity->getSymbol(), processingDate);
		      //std::cout << "PalLongStrategy entered LongOnOpen Order on " << processingDate << std::endl;
		    }
		  this->addFlatPositionBar (aSecurity, processingDate);
		}
	    }
	}
    }

      void eventExitOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			    const InstrumentPosition<Decimal>& instrPos,
			    const date& processingDate)
      {
	if (this->isLongPosition (aSecurity->getSymbol()))
	  {
	    eventExitLongOrders (aSecurity, instrPos, processingDate);

	  }
      }

  private:

    void eventEntryLongOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			       const InstrumentPosition<Decimal>& instrPos,
			       const date& processingDate)
      {
	auto sym = aSecurity->getSymbol();

	if (this->strategyCanPyramid(sym))
	  {
	    typename PalMetaStrategy<Decimal>::ConstStrategiesIterator it = this->beginPricePatterns();

	    for (; it != this->endPricePatterns(); it++)
	    {
	      std::shared_ptr<PriceActionLabPattern> pricePattern = *it;

	      // We only pyramid in the direction
	      if (pricePattern->isLongPattern() &&
		  (this->getSecurityBarNumber(aSecurity->getSymbol()) > 
		   pricePattern->getMaxBarsBack()))
		{
		  PatternExpression *expr = pricePattern->getPatternExpression().get();
		  typename Security<Decimal>::ConstRandomAccessIterator it = 
		    aSecurity->getRandomAccessIterator (processingDate);

		  if (PALPatternInterpreter<Decimal>::evaluateExpression (expr, aSecurity, it))
		    this->EnterLongOnOpen (aSecurity->getSymbol(), processingDate);

		  this->addFlatPositionBar (aSecurity, processingDate);
		}
	    }
	  }
      }

    void eventEntryShortOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			       const InstrumentPosition<Decimal>& instrPos,
			       const date& processingDate)
      {
	auto sym = aSecurity->getSymbol();

	if (this->strategyCanPyramid(sym))
	  {
	    typename PalMetaStrategy<Decimal>::ConstStrategiesIterator it = this->beginPricePatterns();

	    for (; it != this->endPricePatterns(); it++)
	    {
	      std::shared_ptr<PriceActionLabPattern> pricePattern = *it;

	      // We only pyramid in the direction
	      if (pricePattern->isShortPattern() &&
		  (this->getSecurityBarNumber(aSecurity->getSymbol()) > 
		   pricePattern->getMaxBarsBack()))
		{
		  PatternExpression *expr = pricePattern->getPatternExpression().get();
		  typename Security<Decimal>::ConstRandomAccessIterator it = 
		    aSecurity->getRandomAccessIterator (processingDate);

		  if (PALPatternInterpreter<Decimal>::evaluateExpression (expr, aSecurity, it))
		    this->EnterShortOnOpen (aSecurity->getSymbol(), processingDate);

		  this->addFlatPositionBar (aSecurity, processingDate);
		}
	    }
	  }
      }


    void eventExitLongOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			      const InstrumentPosition<Decimal>& instrPos,
			      const date& processingDate)
      {
	typename PalMetaStrategy<Decimal>::ConstStrategiesIterator it = this->beginPricePatterns();

	// HACK: For now we are assuming only one position open at a time

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

    void eventExitShortOrders (std::shared_ptr<Security<Decimal>> aSecurity,
			       const InstrumentPosition<Decimal>& instrPos,
			       const date& processingDate)
      {

      }
    
  private:
    PalPatterns mPalPatterns;
    MCPTStrategyAttributes<Decimal> mMCPTAttributes;
    unsigned int mStrategyMaxBarsBack;
  };

  template <class Decimal> class PalStrategy : public BacktesterStrategy<Decimal>
    {
    public:
    PalStrategy(const std::string& strategyName,
		std::shared_ptr<PriceActionLabPattern> pattern,
		std::shared_ptr<Portfolio<Decimal>> portfolio,
		const StrategyOptions& strategyOptions)
      : BacktesterStrategy<Decimal>(strategyName, portfolio, strategyOptions),
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
		    std::shared_ptr<Portfolio<Decimal>> portfolio,
		    const StrategyOptions& strategyOptions = defaultStrategyOptions)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio, strategyOptions)
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
	auto sym = aSecurity->getSymbol();

	if (this->isFlatPosition (sym) || this->strategyCanPyramid(sym))
	  {
	    if (this->getSecurityBarNumber(sym) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
		typename Security<Decimal>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (PALPatternInterpreter<Decimal>::evaluateExpression (expr, aSecurity, it))
		  {
		    this->EnterLongOnOpen (sym, processingDate);
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
		     std::shared_ptr<Portfolio<Decimal>> portfolio,
		     const StrategyOptions& strategyOptions = defaultStrategyOptions)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio, strategyOptions)
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
	auto sym = aSecurity->getSymbol();
	if (this->isFlatPosition (sym) || this->strategyCanPyramid(sym))
	  {
	    if (this->getSecurityBarNumber(sym) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
		typename Security<Decimal>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (PALPatternInterpreter<Decimal>::evaluateExpression (expr, aSecurity, it))
		  {
		    //std::cout << "PalShortStrategy entered ShortOnOpen Order on " << processingDate << std::endl;
		    this->EnterShortOnOpen (sym, processingDate);
		  }
		this->addFlatPositionBar (aSecurity, processingDate);
	      }
	  }
      }
  };
}

#endif
