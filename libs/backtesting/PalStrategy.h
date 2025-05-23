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
#include <functional>
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

  // EntryOrderConditions factors out into a common class the
  // code for entry condition testing. The assumption is that
  // the strategy is in the state: flat, long or short when
  // the methods are called

 template <class Decimal> class EntryOrderConditions
    {
    public:
      virtual bool canEnterMarket(BacktesterStrategy<Decimal> *strategy, 
				  Security<Decimal>* aSecurity) const = 0;
      virtual bool canTradePattern(BacktesterStrategy<Decimal> *strategy,
				   std::shared_ptr<PriceActionLabPattern> pattern, 
				   Security<Decimal>* aSecurity) const = 0;
      virtual void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
				     std::shared_ptr<PriceActionLabPattern> pattern, 
				     Security<Decimal>* aSecurity,
				     const date& processingDate) const = 0;
    };

 template <class Decimal> class FlatEntryOrderConditions : public  EntryOrderConditions<Decimal>
   {
     bool canEnterMarket(BacktesterStrategy<Decimal> *strategy, 
			 Security<Decimal>* aSecurity) const
       {
	 return true;
       }

      bool canTradePattern(BacktesterStrategy<Decimal> *strategy,
			   std::shared_ptr<PriceActionLabPattern> pattern, 
			   Security<Decimal>* aSecurity) const
	{
	  return strategy->getSecurityBarNumber(aSecurity->getSymbol()) > pattern->getMaxBarsBack();
	}

      void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
			     std::shared_ptr<PriceActionLabPattern> pattern, 
			     Security<Decimal>* aSecurity,
			     const date& processingDate) const
	{
	  Decimal target = pattern->getProfitTargetAsDecimal();
	  Decimal stop = pattern->getStopLossAsDecimal();

	  if (pattern->isLongPattern())
	    strategy->EnterLongOnOpen (aSecurity->getSymbol(), processingDate, stop, target);
	  else
	    strategy->EnterShortOnOpen (aSecurity->getSymbol(), processingDate, stop, target);
	}
	
    };

 template <class Decimal> class LongEntryOrderConditions : public  EntryOrderConditions<Decimal>
   {
     bool canEnterMarket(BacktesterStrategy<Decimal> *strategy, 
			 Security<Decimal>* aSecurity) const
       {
	return (strategy->strategyCanPyramid(aSecurity->getSymbol()));
       }

      bool canTradePattern(BacktesterStrategy<Decimal> *strategy,
			   std::shared_ptr<PriceActionLabPattern> pattern, 
			   Security<Decimal>* aSecurity) const
	{
	  return (pattern->isLongPattern() &&
		  (strategy->getSecurityBarNumber(aSecurity->getSymbol()) > 
		   pattern->getMaxBarsBack()));
	}

      void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
			     std::shared_ptr<PriceActionLabPattern> pattern, 
			     Security<Decimal>* aSecurity,
			     const date& processingDate) const
	{
	  Decimal target = pattern->getProfitTargetAsDecimal();
	  Decimal stop = pattern->getStopLossAsDecimal();

	  strategy->EnterLongOnOpen (aSecurity->getSymbol(), processingDate, stop, target);
	}
	
    };

 template <class Decimal> class ShortEntryOrderConditions : public  EntryOrderConditions<Decimal>
   {
     bool canEnterMarket(BacktesterStrategy<Decimal> *strategy, 
			 Security<Decimal>* aSecurity) const
       {
	return (strategy->strategyCanPyramid(aSecurity->getSymbol()));
       }

      bool canTradePattern(BacktesterStrategy<Decimal> *strategy,
			   std::shared_ptr<PriceActionLabPattern> pattern, 
			   Security<Decimal>* aSecurity) const
	{
	  return (pattern->isShortPattern() &&
		  (strategy->getSecurityBarNumber(aSecurity->getSymbol()) > 
		   pattern->getMaxBarsBack()));
	}

      void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
			     std::shared_ptr<PriceActionLabPattern> pattern, 
			     Security<Decimal>* aSecurity,
			     const date& processingDate) const
	{
	  Decimal target = pattern->getProfitTargetAsDecimal();
	  Decimal stop = pattern->getStopLossAsDecimal();

	  strategy->EnterShortOnOpen (aSecurity->getSymbol(), processingDate, stop, target);
	}
	
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
	mPatternEvaluators(),
	mMCPTAttributes(),
	mStrategyMaxBarsBack(0)
    {}

    PalMetaStrategy(const PalMetaStrategy<Decimal>& rhs)
	: BacktesterStrategy<Decimal>(rhs),
      mPalPatterns(rhs.mPalPatterns),
      mPatternEvaluators(rhs.mPatternEvaluators),
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
	mPatternEvaluators = rhs.mPatternEvaluators;
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

	// compile & cache
	auto eval = PALPatternInterpreter<Decimal>::compileEvaluator(pattern->getPatternExpression().get());
	mPatternEvaluators.push_back(eval);
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

    [[deprecated("Use of this getPositionDirectionVector will throw an exception")]]
    std::vector<int> getPositionDirectionVector() const
      {
	throw PalStrategyException("getPositionDirectionVector is no longer supported");
	return mMCPTAttributes.getPositionDirection();
      }

    [[deprecated("Use of this getPositionReturnsVector will throw an exception")]]
    std::vector<Decimal> getPositionReturnsVector() const
      {
	throw PalStrategyException("getPositionReturnsVector is no longer supported");
	return mMCPTAttributes.getPositionReturns();
      }

    [[deprecated("Use of this numTradingOpportunities will throw an exception")]]
    unsigned long numTradingOpportunities() const
    {
      	throw PalStrategyException("numTradingOpportunities is no longer supported");
	return mMCPTAttributes.numTradingOpportunities();
    }

    std::shared_ptr<BacktesterStrategy<Decimal>> 
    clone (const std::shared_ptr<Portfolio<Decimal>>& portfolio) const
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

    void eventEntryOrders (Security<Decimal>* aSecurity,
			   const InstrumentPosition<Decimal>& instrPos,
			   const date& processingDate)
    {
      if (this->isFlatPosition (aSecurity->getSymbol()))
	entryOrdersCommon(aSecurity, instrPos, processingDate, FlatEntryOrderConditions<Decimal>());
      else if (this->isLongPosition (aSecurity->getSymbol()))
	entryOrdersCommon(aSecurity, instrPos, processingDate, LongEntryOrderConditions<Decimal>());
      else if (this->isShortPosition (aSecurity->getSymbol()))
	entryOrdersCommon(aSecurity, instrPos, processingDate, ShortEntryOrderConditions<Decimal>());
      else
	throw PalStrategyException(std::string("PalMetaStrategy::eventEntryOrders - Unknow position state"));
    }

    void eventExitOrders (Security<Decimal>* aSecurity,
			  const InstrumentPosition<Decimal>& instrPos,
			  const date& processingDate)
    {
      // We could be pyramiding or not, either way get the latest position
      uint32_t numUnits = instrPos.getNumPositionUnits();
      auto it = instrPos.getInstrumentPosition(numUnits);
      auto pos = *it;

      // Get stop loss, profit target and fill price from latest position

      Decimal target = pos->getProfitTarget();
      PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);

      Decimal stop = pos->getStopLoss();
      PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);
	
      Decimal fillPrice = instrPos.getFillPrice(numUnits);

      if (this->isLongPosition (aSecurity->getSymbol()))
	eventExitLongOrders (aSecurity, instrPos, processingDate, fillPrice, stopAsPercent, targetAsPercent);
      else if (this->isShortPosition (aSecurity->getSymbol()))
	eventExitShortOrders (aSecurity, instrPos, processingDate, fillPrice, stopAsPercent, targetAsPercent);
      else
	throw PalStrategyException(std::string("PalMetaStrategy::eventExitOrders - Expecting long or short positon"));
    }

  private:
    void entryOrdersCommon (Security<Decimal>* aSecurity,
			    const InstrumentPosition<Decimal>& instrPos,
			    const date& processingDate,
			    const EntryOrderConditions<Decimal>& entryConditions)
      {
	auto it = aSecurity->getRandomAccessIterator(processingDate);
	
	if (entryConditions.canEnterMarket(this, aSecurity))
	  {
	    auto patIt  = mPalPatterns.begin();
	    auto evalIt = mPatternEvaluators.begin();
	    for (; patIt != mPalPatterns.end() && evalIt != mPatternEvaluators.end();
		 ++patIt, ++evalIt)
	      {
		std::shared_ptr<PriceActionLabPattern> pricePattern = *patIt;
		
		if (!entryConditions.canTradePattern (this, pricePattern, aSecurity))
		  continue;

		if ((*evalIt)(aSecurity, it))
		  {
		    entryConditions.createEntryOrders(this, pricePattern, aSecurity, processingDate);
		    break;
		  }
	      }
	  }
      }

    void eventExitLongOrders (Security<Decimal>* aSecurity,
			      const InstrumentPosition<Decimal>& instrPos,
			      const date& processingDate,
			      const Decimal& positionEntryPrice,
			      const PercentNumber<Decimal>& stopAsPercent,
			      const PercentNumber<Decimal>& targetAsPercent)
      {
	//std::cout << "PalLongStrategy::eventExitOrders, fill Price =  " << positionEntryPrice << std::endl;
	this->ExitLongAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
				      positionEntryPrice, targetAsPercent);
	this->ExitLongAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
				     positionEntryPrice, stopAsPercent);
	instrPos.setRMultipleStop (LongStopLoss<Decimal> (positionEntryPrice, stopAsPercent).getStopLoss());
      }

    void eventExitShortOrders (Security<Decimal>* aSecurity,
			       const InstrumentPosition<Decimal>& instrPos,
			       const date& processingDate,
			       const Decimal& positionEntryPrice,
			       const PercentNumber<Decimal>& stopAsPercent,
			       const PercentNumber<Decimal>& targetAsPercent) 
      {
	this->ExitShortAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
				       positionEntryPrice, targetAsPercent);
	this->ExitShortAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
				      positionEntryPrice, stopAsPercent);
	instrPos.setRMultipleStop (ShortStopLoss<Decimal> (positionEntryPrice, stopAsPercent).getStopLoss());
      }

    [[deprecated("Use of this addLongPositionBar no longer supported")]]
    void addLongPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
    {
      //mMCPTAttributes.addLongPositionBar (aSecurity, processingDate);
    }

    [[deprecated("Use of this addShortPositionBar no longer supported")]]
    void addShortPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			     const date& processingDate)
    {
      //mMCPTAttributes.addShortPositionBar (aSecurity, processingDate);
    }

    [[deprecated("Use of this addFlatPositionBar no longer supported")]]
    void addFlatPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
    {
      //mMCPTAttributes.addFlatPositionBar (aSecurity, processingDate);
    }
    
  private:
    PalPatterns mPalPatterns;
    std::vector<typename PALPatternInterpreter<Decimal>::PatternEvaluator> mPatternEvaluators;
    MCPTStrategyAttributes<Decimal> mMCPTAttributes;
    unsigned int mStrategyMaxBarsBack;
  };

  /**
   * @brief Base class for price-action-based strategies using a single pattern.
   *
   * PalStrategy drives entry and exit logic for a single PriceActionLabPattern, evaluating
   * pattern expressions bar-by-bar.
   *
   * @tparam Decimal Numeric type for prices and returns (e.g., double).
   */
  template <class Decimal> class PalStrategy : public BacktesterStrategy<Decimal>
    {
    public:
      using PatternEvaluator = typename PALPatternInterpreter<Decimal>::PatternEvaluator;

      /**
     * @brief Construct a PalStrategy with a given pattern and portfolio.
     * @param strategyName  Unique name for this strategy instance.
     * @param pattern       Shared pointer to the PriceActionLabPattern to trade.
     * @param portfolio     Shared portfolio of securities and cash.
     * @param strategyOptions  Configuration options (risk, slippage, etc.).
     */
    PalStrategy(const std::string& strategyName,
		std::shared_ptr<PriceActionLabPattern> pattern,
		std::shared_ptr<Portfolio<Decimal>> portfolio,
		const StrategyOptions& strategyOptions)
      : BacktesterStrategy<Decimal>(strategyName, portfolio, strategyOptions),
	mPalPattern(pattern),
	mMCPTAttributes()
	{
	  if (mPalPattern)
	    {
	      // compile the real expression once
	      mPatternEvaluator =
		PALPatternInterpreter<Decimal>::compileEvaluator(mPalPattern->getPatternExpression().get());
	    }
	  else
	    {
	      // no pattern ⇒ never match
	      mPatternEvaluator = [](Security<Decimal>*, auto){ return false; };
	    }
	}

      PalStrategy(const PalStrategy<Decimal>& rhs)
	: BacktesterStrategy<Decimal>(rhs),
	  mPalPattern(rhs.mPalPattern),
	  mMCPTAttributes(rhs.mMCPTAttributes),
	  mPatternEvaluator(rhs.mPatternEvaluator)
      {}

      const PalStrategy<Decimal>&
      operator=(const PalStrategy<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	BacktesterStrategy<Decimal>::operator=(rhs);
	mPalPattern = rhs.mPalPattern;
	mMCPTAttributes = rhs.mMCPTAttributes;
	mPatternEvaluator = rhs.mPatternEvaluator;
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

      [[deprecated("Use of this getPositionDirectionVector will throw an exception")]]
      std::vector<int> getPositionDirectionVector() const
      {
	throw PalStrategyException("getPositionDirectionVector is no longer supported");
	return mMCPTAttributes.getPositionDirection();
      }

      [[deprecated("Use of this getPositionReturnsVector will throw an exception")]]
      std::vector<Decimal> getPositionReturnsVector() const
      {
	throw PalStrategyException("getPositionReturnsVector is no longer supported");
	return mMCPTAttributes.getPositionReturns();
      }

      [[deprecated("Use of this numTradingOpportunities will throw an exception")]]
      unsigned long numTradingOpportunities() const
      {
	throw PalStrategyException("numTradingOpportunities is no longer supported");
	return mMCPTAttributes.numTradingOpportunities();
      }

    protected:
      const PatternEvaluator& getPatternEvaluator() const
      {
	return mPatternEvaluator;
      }
      
      [[deprecated("Use of this addLongPositionBar no longer supported")]]
      void addLongPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
      {
	//mMCPTAttributes.addLongPositionBar (aSecurity, processingDate);
      }

      [[deprecated("Use of this addShortPositionBar no longer supported")]]
      void addShortPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
      {
	//mMCPTAttributes.addShortPositionBar (aSecurity, processingDate);
      }

      [[deprecated("Use of this addFlatPositionBar no longer supported")]]
      void addFlatPositionBar(std::shared_ptr<Security<Decimal>> aSecurity,
			    const date& processingDate)
      {
	//mMCPTAttributes.addFlatPositionBar (aSecurity, processingDate);
      }

    private:
      std::shared_ptr<PriceActionLabPattern> mPalPattern;
      MCPTStrategyAttributes<Decimal> mMCPTAttributes;
      PatternEvaluator mPatternEvaluator;
      static TradingVolume OneShare;
      static TradingVolume OneContract;
    };

  template <class Decimal> TradingVolume PalStrategy<Decimal>::OneShare(1, TradingVolume::SHARES);
  template <class Decimal> TradingVolume PalStrategy<Decimal>::OneContract(1, TradingVolume::CONTRACTS);

  /**
   * @class PalLongStrategy
   * @brief Concrete PalStrategy for long‐only price‐action patterns.
   *
   * This class implements all the entry/exit logic needed to run a long‐only
   * version of a single PriceActionLabPattern:
   *  - **Entry**: on each bar, if flat or pyramiding is allowed and the
   *    pattern evaluator fires, it issues an `EnterLongOnOpen` with the
   *    configured stop‐loss and profit‐target.
   *  - **Exit**: for open long positions, it submits both a limit exit at
   *    profit‐target and a stop‐loss exit, then updates the R‐multiple
   *    on the filled bar.
   *
   * @details
   * When used under our BackTester, every bar’s P&L—including the bar on which
   * a profit‐target or stop‐loss fires—is recorded at the finest resolution.
   * This is critical for building accurate null distributions in both
   * permutation tests (e.g., Masters’s step‐down algorithm) and bootstrap
   * confidence intervals, since it:
   *  - Maintains a large, homogeneous sample of bar‐returns for resampling.
   *  - Preserves time‐series properties (autocorrelation, volatility clustering).
   *  - Ensures exit‐bar P&L is never dropped, giving robust, low‐variance
   *    statistics for significance testing and interval estimation.
   *
   * @tparam Decimal  Numeric type for price/return calculations (e.g., double).
   */
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
      clone (const std::shared_ptr<Portfolio<Decimal>>& portfolio) const
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

      /**
       * @brief Evaluate and submit exit orders for long positions on this bar.
       *
       * @details
       * BackTester calls this before entries each bar.  For long trades, this method:
       *  - Retrieves the latest fill price for the current unit(s).
       *  - Issues a limit order at the configured profit‐target.
       *  - Issues a stop‐loss order at the configured stop level.
       *  - Updates the position’s R‐multiple for performance tracking.
       *
       * Because StrategyBroker first marks all open positions at the bar‐close,
       * the exit fill’s P&L (which may occur intrabar) is added to the high‐resolution
       * returns series via getAllHighResReturns, ensuring no P&L is lost.
       *
       * @param aSecurity       Security to exit.
       * @param instrPos        InstrumentPosition for the current bar.
       * @param processingDate  Date of this bar.
       */
      void eventExitOrders (Security<Decimal>* aSecurity,
			    const InstrumentPosition<Decimal>& instrPos,
			    const date& processingDate)
      {
	if (this->isLongPosition (aSecurity->getSymbol()))
	  {
	    std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
	    Decimal target = pattern->getProfitTargetAsDecimal();
	    PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);

	    Decimal stop = pattern->getStopLossAsDecimal();
	    PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);

	    Decimal fillPrice = instrPos.getFillPrice();

	    this->ExitLongAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitLongAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (LongStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());

	    //this->addLongPositionBar (aSecurity, processingDate);
	  }
      }

      /**
       * @brief Evaluate and submit new long‐entry orders based on the pattern.
       *
       * @details
       * BackTester invokes this immediately after exits each bar.  In a long strategy:
       *  - If the position is flat or pyramiding is allowed, and
       *    the bar count exceeds the pattern’s lookback,
       *    we evaluate the compiled pattern expression on the bar’s data.
       *  - If the pattern fires, we submit an `EnterLongOnOpen`.
       *
       * Ordering ensures that any exits freeing up capital/pyramiding slots happen
       * before new entries.  All bar‐by‐bar P&L—including exit fills—will
       * be captured in the unified high‐res series.
       *
       * @param aSecurity       Security to evaluate for entry.
       * @param instrPos        InstrumentPosition for the current bar.
       * @param processingDate  Date of this bar.
       */

      void eventEntryOrders (Security<Decimal>* aSecurity,
			     const InstrumentPosition<Decimal>& instrPos,
			     const date& processingDate)
      {
	auto sym = aSecurity->getSymbol();

	if (this->isFlatPosition (sym) || this->strategyCanPyramid(sym))
	  {
	    if (this->getSecurityBarNumber(sym) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		typename Security<Decimal>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (this->getPatternEvaluator()(aSecurity, it))
		  {
		    this->EnterLongOnOpen (sym, processingDate);
		  }
		//this->addFlatPositionBar (aSecurity, processingDate);
	      }
	  }
      }
  };

  /**
   * @class PalShortStrategy
   * @brief Concrete PalStrategy for short‐only price‐action patterns.
   *
   * This class implements all the entry/exit logic needed to run a short‐only
   * version of a single PriceActionLabPattern:
   *  - **Entry**: on each bar, if flat or pyramiding is allowed and the
   *    pattern evaluator fires, it issues an `EnterShortOnOpen` with the
   *    configured stop‐loss and profit‐target.
   *  - **Exit**: for open short positions, it submits both a limit exit at
   *    profit‐target and a stop‐loss exit, then updates the R‐multiple
   *    on the filled bar.
   *
   * @details
   * As with long trades, every bar’s P&L—including the bar on which a short‐side
   * profit‐target or stop‐loss fires—is captured at the bar level.  This fine‐grained
   * return series is essential for:
   *  - Stable permutation‐test null distributions (strong FWE control).
   *  - Accurate bootstrap of out‐of‐sample performance (tight CI’s).
   *  - Fair comparison across strategies, since exit‐bar outcomes are never lost.
   *
   * @tparam Decimal  Numeric type for price/return calculations (e.g., double).
 */
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
      clone (const std::shared_ptr<Portfolio<Decimal>>& portfolio) const
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

      /**
       * @brief Evaluate and submit exit orders for short positions on this bar.
       *
       * @details
       * Called before entry each bar.  For short trades, submits:
       *  - A limit‐to‐cover at the profit‐target price.
       *  - A stop‐to‐cover at the stop‐loss price.
       *  - Records the exit bar’s P&L in the high‐res series.
       *
       * @param aSecurity       Security to exit.
       * @param instrPos        InstrumentPosition for the current bar.
       * @param processingDate  Date of this bar.
       */
      void eventExitOrders (Security<Decimal>* aSecurity,
			    const InstrumentPosition<Decimal>& instrPos,
			    const date& processingDate)
      {
	if (this->isShortPosition (aSecurity->getSymbol()))
	  {
	    std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
	    Decimal target = pattern->getProfitTargetAsDecimal();

	    PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);

	    Decimal stop = pattern->getStopLossAsDecimal();

	    PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);

	    Decimal fillPrice = instrPos.getFillPrice();

	    this->ExitShortAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitShortAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (ShortStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());
	    //this->addShortPositionBar (aSecurity, processingDate);
	  }
      }

      /**
       * @brief Evaluate and submit new short‐entry orders based on the pattern.
       *
       * @details
       * Called immediately after exits.  Checks if flat/pyramiding allowed,
       * tests the pattern on this bar, and issues `EnterShortOnOpen` if triggered.
       *
       * @param aSecurity       Security to evaluate for entry.
       * @param instrPos        InstrumentPosition for the current bar.
       * @param processingDate  Date of this bar.
       */
      void eventEntryOrders (Security<Decimal>* aSecurity,
			     const InstrumentPosition<Decimal>& instrPos,
			     const date& processingDate)
      {
	auto sym = aSecurity->getSymbol();
	if (this->isFlatPosition (sym) || this->strategyCanPyramid(sym))
	  {
	    if (this->getSecurityBarNumber(sym) > 
		this->getPalPattern()->getMaxBarsBack())
	      {
		typename Security<Decimal>::ConstRandomAccessIterator it = 
		  aSecurity->getRandomAccessIterator (processingDate);

		if (this->getPatternEvaluator()(aSecurity, it))
		  {
		    this->EnterShortOnOpen (sym, processingDate);
		  }
		//this->addFlatPositionBar (aSecurity, processingDate);
	      }
	  }
      }
  };

  template<typename Decimal>
  std::shared_ptr<PalStrategy<Decimal>> makePalStrategy(const std::string& name,
							const std::shared_ptr<PriceActionLabPattern>& pattern,
							const std::shared_ptr<Portfolio<Decimal>>& portfolio)
  {
    if (pattern->isLongPattern())
      return std::make_shared<PalLongStrategy<Decimal>>(name, pattern, portfolio);
    else
      return std::make_shared<PalShortStrategy<Decimal>>(name, pattern, portfolio);
  }
}

#endif
