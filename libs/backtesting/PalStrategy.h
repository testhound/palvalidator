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
#include <iostream>
#include "MCPTStrategyAttributes.h"
#include "PalAst.h"
#include "BacktesterStrategy.h"
#include "PALPatternInterpreter.h" // Includes new PatternEvaluator signature
#include "TimeSeriesEntry.h" // For getDefaultBarTime, ptime, date
#include "PortfolioFilter.h" // For portfolio filtering functionality

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using boost::posix_time::ptime;

  // Assuming mkc_timeseries::getDefaultBarTime() is declared in TimeSeriesEntry.h 
  // or accessible in this scope. It's used in StrategyBroker.h and TimeSeries.h
  // extern boost::posix_time::time_duration getDefaultBarTime(); // Example declaration if not in TimeSeriesEntry.h

  class PalStrategyException : public std::runtime_error
  {
  public:
    PalStrategyException(const std::string msg)
      : std::runtime_error(msg)
    {}

    // virtual destructor for base class not strictly necessary if not inheriting from it,
    // but good practice if it might be. Default is fine.
    ~PalStrategyException() override = default; // Changed to override and default
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
    virtual void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
                                   std::shared_ptr<PriceActionLabPattern> pattern,
                                   Security<Decimal>* aSecurity,
                                   const ptime& processingDateTime) const = 0;

    virtual ~EntryOrderConditions() = default;
  };

  template <class Decimal> class FlatEntryOrderConditions : public EntryOrderConditions<Decimal>
  {
  public:
    bool canEnterMarket(BacktesterStrategy<Decimal> *strategy,
                        Security<Decimal>* aSecurity) const override
    {
      return true;
    }

    bool canTradePattern(BacktesterStrategy<Decimal> *strategy,
                         std::shared_ptr<PriceActionLabPattern> pattern,
                         Security<Decimal>* aSecurity) const override
    {
      return strategy->getSecurityBarNumber(aSecurity->getSymbol()) > pattern->getMaxBarsBack();
    }

    void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
                           std::shared_ptr<PriceActionLabPattern> pattern,
                           Security<Decimal>* aSecurity,
                           const date& processingDate) const override
    {
      createEntryOrders(strategy,
			pattern,
			aSecurity,
			ptime(processingDate,
			      getDefaultBarTime()));
    }

    void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
                           std::shared_ptr<PriceActionLabPattern> pattern,
                           Security<Decimal>* aSecurity,
                           const ptime& processingDateTime) const override
    {
      // Core logic using ptime from b1cb953
      Decimal target = pattern->getProfitTargetAsDecimal();
      Decimal stop = pattern->getStopLossAsDecimal();

      if (pattern->isLongPattern())
      {
        strategy->EnterLongOnOpen (aSecurity->getSymbol(), processingDateTime, stop, target);
      }
      else
      {
        strategy->EnterShortOnOpen (aSecurity->getSymbol(), processingDateTime, stop, target);
      }
    }
  };

  template <class Decimal> class LongEntryOrderConditions : public EntryOrderConditions<Decimal>
  {
  public:
    bool canEnterMarket(BacktesterStrategy<Decimal> *strategy,
                        Security<Decimal>* aSecurity) const override
    {
      return (strategy->strategyCanPyramid(aSecurity->getSymbol()));
    }

    bool canTradePattern(BacktesterStrategy<Decimal> *strategy,
                         std::shared_ptr<PriceActionLabPattern> pattern,
                         Security<Decimal>* aSecurity) const override
    {
      return (pattern->isLongPattern() &&
              (strategy->getSecurityBarNumber(aSecurity->getSymbol()) >
               pattern->getMaxBarsBack()));
    }

    void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
                           std::shared_ptr<PriceActionLabPattern> pattern,
                           Security<Decimal>* aSecurity,
                           const date& processingDate) const override
    {
      createEntryOrders(strategy, pattern, aSecurity,
			ptime(processingDate, getDefaultBarTime()));
    }

    void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
                           std::shared_ptr<PriceActionLabPattern> pattern,
                           Security<Decimal>* aSecurity,
                           const ptime& processingDateTime) const override
    {
      Decimal target = pattern->getProfitTargetAsDecimal();
      Decimal stop = pattern->getStopLossAsDecimal();

      strategy->EnterLongOnOpen (aSecurity->getSymbol(), processingDateTime, stop, target);
    }
  };

  template <class Decimal> class ShortEntryOrderConditions : public EntryOrderConditions<Decimal>
  {
  public:
    bool canEnterMarket(BacktesterStrategy<Decimal> *strategy,
                        Security<Decimal>* aSecurity) const override
    {
       // When in a short position, only allow pyramiding if enabled
       return (strategy->strategyCanPyramid(aSecurity->getSymbol()));
    }

    bool canTradePattern(BacktesterStrategy<Decimal> *strategy,
                         std::shared_ptr<PriceActionLabPattern> pattern,
                         Security<Decimal>* aSecurity) const override
    {
      return (pattern->isShortPattern() &&
              (strategy->getSecurityBarNumber(aSecurity->getSymbol()) >
               pattern->getMaxBarsBack()));
    }

    void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
                           std::shared_ptr<PriceActionLabPattern> pattern,
                           Security<Decimal>* aSecurity,
                           const date& processingDate) const override
    {
      createEntryOrders(strategy, pattern, aSecurity,
			ptime(processingDate, getDefaultBarTime()));
    }

    void createEntryOrders(BacktesterStrategy<Decimal> *strategy,
                           std::shared_ptr<PriceActionLabPattern> pattern,
                           Security<Decimal>* aSecurity,
                           const ptime& processingDateTime) const override
    {
      Decimal target = pattern->getProfitTargetAsDecimal();
      Decimal stop = pattern->getStopLossAsDecimal();

      strategy->EnterShortOnOpen (aSecurity->getSymbol(), processingDateTime, stop, target);
    }
  };

  template <class Decimal, class Filter = NoPortfolioFilter<Decimal>>
  class PalMetaStrategy : public BacktesterStrategy<Decimal>
  {
  public:
    typedef typename std::list<std::shared_ptr<PriceActionLabPattern>> PalPatterns;
    typedef typename PalPatterns::const_iterator ConstStrategiesIterator;
    using PatternEvaluator = typename PALPatternInterpreter<Decimal>::PatternEvaluator;

    PalMetaStrategy(const std::string& strategyName,
      std::shared_ptr<Portfolio<Decimal>> portfolio,
      const StrategyOptions& strategyOptions = defaultStrategyOptions)
      : BacktesterStrategy<Decimal>(strategyName, portfolio, strategyOptions),
	mPalPatterns(),
	mPatternEvaluators(),
	mMCPTAttributes(),
	mStrategyMaxBarsBack(0),
	mPortfolioFilter(createPortfolioFilter(portfolio)),
	mBreakevenEnabled(false),
	mBreakevenActivationBars(0),
	mSkipIfBothSidesFire(false)
    {}

    PalMetaStrategy(const PalMetaStrategy<Decimal, Filter>& rhs)
      : BacktesterStrategy<Decimal>(rhs),
      mPalPatterns(rhs.mPalPatterns),
      mPatternEvaluators(rhs.mPatternEvaluators),
      mMCPTAttributes(rhs.mMCPTAttributes),
      mStrategyMaxBarsBack(rhs.mStrategyMaxBarsBack),
      mPortfolioFilter(rhs.mPortfolioFilter),
      mBreakevenEnabled(rhs.mBreakevenEnabled),
      mBreakevenActivationBars(rhs.mBreakevenActivationBars),
      mSkipIfBothSidesFire(rhs.mSkipIfBothSidesFire)
    {}

    PalMetaStrategy<Decimal, Filter>&
    operator=(const PalMetaStrategy<Decimal, Filter>& rhs)
    {
      if (this == &rhs) {
	return *this;
      }

      BacktesterStrategy<Decimal>::operator=(rhs);
      mPalPatterns               = rhs.mPalPatterns;
      mPatternEvaluators         = rhs.mPatternEvaluators; // If evaluators become stateful, revisit
      mMCPTAttributes            = rhs.mMCPTAttributes;
      mStrategyMaxBarsBack       = rhs.mStrategyMaxBarsBack;
      mPortfolioFilter           = rhs.mPortfolioFilter;
      mBreakevenEnabled          = rhs.mBreakevenEnabled;
      mBreakevenActivationBars   = rhs.mBreakevenActivationBars;
      mSkipIfBothSidesFire       = rhs.mSkipIfBothSidesFire; // [BOTH-SIDES]

      return *this;
    }

    ~PalMetaStrategy() override = default;

    void addPricePattern(std::shared_ptr<PriceActionLabPattern> pattern)
    {
      // Strategy needs the lookback bars to be present on the current bar,
      // so store "required bars" = maxBarsBack + 1
      const unsigned int requiredBars = pattern->getMaxBarsBack() + 1;
      if (requiredBars > mStrategyMaxBarsBack)
	mStrategyMaxBarsBack = requiredBars;

      mPalPatterns.push_back(pattern);

      // Compile & cache evaluator for this pattern
      auto eval = PALPatternInterpreter<Decimal>::compileEvaluator(pattern->getPatternExpression().get());
      mPatternEvaluators.push_back(eval);
    }

    uint32_t getPatternMaxBarsBack() const override
    {
      return mStrategyMaxBarsBack;
    }

    std::shared_ptr<PriceActionLabPattern> getPatternAt(uint32_t idx)
    {
      auto it = mPalPatterns.begin();
      std::advance(it, idx);
      return *it;
    }

    ConstStrategiesIterator beginPricePatterns() const
    {
      return mPalPatterns.begin();
    }

    ConstStrategiesIterator endPricePatterns() const
    {
      return mPalPatterns.end();
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    clone(const std::shared_ptr<Portfolio<Decimal>>& portfolio) const override
    {
      // Using HEAD's more detailed clone logic that handles options
      auto cloned = std::make_shared<PalMetaStrategy<Decimal, Filter>>(this->getStrategyName(),
								       portfolio,
								       this->getStrategyOptions());
      // Copy patterns (which updates mStrategyMaxBarsBack)
      for (const auto& pattern : mPalPatterns) {
	cloned->addPricePattern(pattern);
      }
      cloned->mStrategyMaxBarsBack     = this->mStrategyMaxBarsBack;
      cloned->mMCPTAttributes          = this->mMCPTAttributes;
      cloned->mBreakevenEnabled        = this->mBreakevenEnabled;
      cloned->mBreakevenActivationBars = this->mBreakevenActivationBars;
      cloned->mSkipIfBothSidesFire     = this->mSkipIfBothSidesFire; // [BOTH-SIDES]

      return cloned;
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    clone_shallow(const std::shared_ptr<Portfolio<Decimal>>& portfolio) const override
    {
      // Build with the target portfolio; copy patterns + compiled evaluators (no recompile)
      auto cloned = std::make_shared<PalMetaStrategy<Decimal, Filter>>(this->getStrategyName(),
								       portfolio,
								       this->getStrategyOptions());
      cloned->mPalPatterns              = this->mPalPatterns;
      cloned->mPatternEvaluators        = this->mPatternEvaluators;
      cloned->mStrategyMaxBarsBack      = this->mStrategyMaxBarsBack;
      cloned->mPortfolioFilter          = cloned->createPortfolioFilter(portfolio); // bind to new portfolio
      cloned->mMCPTAttributes           = this->mMCPTAttributes;
      cloned->mBreakevenEnabled         = this->mBreakevenEnabled;
      cloned->mBreakevenActivationBars  = this->mBreakevenActivationBars;
      cloned->mSkipIfBothSidesFire      = this->mSkipIfBothSidesFire; // [BOTH-SIDES]

      return cloned;
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    cloneForBackTesting () const override
    {
      auto cloned = std::make_shared<PalMetaStrategy<Decimal, Filter>>(this->getStrategyName(),
								       this->getPortfolio(),
								       this->getStrategyOptions());
      for (const auto& pattern : mPalPatterns) {
	cloned->addPricePattern(pattern);
      }
      cloned->mStrategyMaxBarsBack     = this->mStrategyMaxBarsBack;
      cloned->mMCPTAttributes          = this->mMCPTAttributes;
      cloned->mBreakevenEnabled        = this->mBreakevenEnabled;
      cloned->mBreakevenActivationBars = this->mBreakevenActivationBars;
      cloned->mSkipIfBothSidesFire     = this->mSkipIfBothSidesFire; // [BOTH-SIDES]

      return cloned;
    }

    /**
     * @brief Enable a breakeven stop that becomes active starting at bar N
     *        (t = N, where t=0 is the first bar after entry).
     */
    void addBreakEvenStop(unsigned int activationBars)
    {
      mBreakevenEnabled = true;
      mBreakevenActivationBars = activationBars;
    }

    /** Disable the breakeven stop behavior. */
    void disableBreakEvenStop()
    {
      mBreakevenEnabled = false;
      mBreakevenActivationBars = 0;
    }

    void setSkipIfBothSidesFire(bool enable) { mSkipIfBothSidesFire = enable; }

    // Implement pure virtual methods from BacktesterStrategy
    const TradingVolume& getSizeForOrder(const Security<Decimal>& aSecurity) const override
    {
       return BacktesterStrategy<Decimal>::getSizeForOrder(aSecurity);
    }

    std::vector<int> getPositionDirectionVector() const override
    {
      throw PalStrategyException("getPositionDirectionVector is no longer supported for PalMetaStrategy");
    }

    std::vector<Decimal> getPositionReturnsVector() const override
    {
      throw PalStrategyException("getPositionReturnsVector is no longer supported for PalMetaStrategy");
    }

    unsigned long numTradingOpportunities() const override
    {
      throw PalStrategyException("numTradingOpportunities is no longer supported for PalMetaStrategy");
    }

    // Bring base class date-based methods into scope if BacktesterStrategy defines them.
    using BacktesterStrategy<Decimal>::eventEntryOrders;
    using BacktesterStrategy<Decimal>::eventExitOrders;

    void eventEntryOrders (Security<Decimal>* aSecurity,
			   const InstrumentPosition<Decimal>& instrPos,
			   const ptime& processingDateTime) override
    {
      if (this->isFlatPosition(aSecurity->getSymbol()))
	{
	  // [BOTH-SIDES] Optional neutrality: stand aside if both long & short fire on same bar
	  if (mSkipIfBothSidesFire) {
	    auto both = detectBothSidesFire(aSecurity, processingDateTime);
	    if (both.first && both.second) {
	      return; // conflicting evidence — skip this bar
	    }
	  }

	  entryOrdersCommon(aSecurity, instrPos, processingDateTime, FlatEntryOrderConditions<Decimal>());
	}
      else if (this->isLongPosition(aSecurity->getSymbol()))
	{
	  // When in a long position, only allow long patterns
	  entryOrdersCommon(aSecurity, instrPos, processingDateTime, LongEntryOrderConditions<Decimal>());
	}
      else if (this->isShortPosition(aSecurity->getSymbol()))
	{
	  // When in a short position, only allow short patterns
	  entryOrdersCommon(aSecurity, instrPos, processingDateTime, ShortEntryOrderConditions<Decimal>());
	}
      else
	{
	  throw PalStrategyException(std::string("PalMetaStrategy::eventEntryOrders - Unknown position state"));
	}
    }

    void eventExitOrders(Security<Decimal>* aSecurity,
			 const InstrumentPosition<Decimal>& instrPos,
			 const ptime& processingDateTime) override
    {
      uint32_t numUnits = instrPos.getNumPositionUnits();
      if (numUnits == 0) {
	return;
      }

      // Iterate newest-to-oldest to avoid invalidation when units close intraloop
      for (uint32_t unitNum = numUnits; unitNum >= 1; --unitNum)
	{
	  auto it  = instrPos.getInstrumentPosition(unitNum);
	  auto pos = *it;

	  // 1) Max holding period (if any) — unconditional time exit
	  unsigned int maxHold = this->getStrategyOptions().getMaxHoldingPeriod();
	  if (maxHold > 0 && pos->getNumBarsSinceEntry() >= maxHold)
	    {
	      if (this->isLongPosition(aSecurity->getSymbol()))
		{
		  this->ExitLongUnitOnOpen(aSecurity->getSymbol(), processingDateTime, unitNum);
		}
	      else if (this->isShortPosition(aSecurity->getSymbol()))
		{
		  this->ExitShortUnitOnOpen(aSecurity->getSymbol(), processingDateTime, unitNum);
		}
	      continue; // Skip other exits for this unit
	    }

	  // 2) Profit target / stop inputs for THIS unit (pattern-defined)
	  const Decimal target    = pos->getProfitTarget();   // percent (e.g., +X%)
	  const Decimal stop      = pos->getStopLoss();       // percent (e.g., -Y%)
	  const Decimal entryPx   = pos->getEntryPrice();
	  const Decimal lastClose = pos->getLastClose();      // last completed bar

	  // Percent-based orders (original behavior)
	  PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber(target);
	  PercentNumber<Decimal> stopAsPercent   = PercentNumber<Decimal>::createPercentNumber(stop);

	  // 3) Breakeven logic (active starting at t = N and only if currently profitable)
	  if (mBreakevenEnabled && pos->getNumBarsSinceEntry() >= mBreakevenActivationBars)
	    {
	      bool currentlyProfitable =
		(this->isLongPosition(aSecurity->getSymbol())  && lastClose > entryPx) ||
		(this->isShortPosition(aSecurity->getSymbol()) && lastClose < entryPx);

	      if (currentlyProfitable)
		{
		  // Replace original percent stop with a BE stop at entry price
		  if (this->isLongPosition(aSecurity->getSymbol()))
		    {
		      this->ExitLongUnitAtStop(aSecurity->getSymbol(),
		          processingDateTime,
		          entryPx, // be stop
		          unitNum);
		    }
		  else
		    {
		      this->ExitShortUnitAtStop(aSecurity->getSymbol(),
		    processingDateTime,
		    entryPx, // be stop
		    unitNum);
		    }

		  // Keep profit target active
		  if (this->isLongPosition(aSecurity->getSymbol()))
		    {
		      this->ExitLongUnitAtLimit(aSecurity->getSymbol(),
						processingDateTime,
						entryPx,
						targetAsPercent,
						unitNum);
		    }
		  else
		    {
		      this->ExitShortUnitAtLimit(aSecurity->getSymbol(),
						 processingDateTime,
						 entryPx,
						 targetAsPercent,
						 unitNum);
		    }

		  // Maintain R-multiple bookkeeping based on BE level if desired (kept original)
		  instrPos.setRMultipleStop(entryPx, unitNum);
		  continue; // finished exits for this unit
		}
	    }

	  // 4) Original stop/target exits (no BE armed, or not profitable)
	  if (this->isLongPosition(aSecurity->getSymbol()))
	    {
	      // Original percent target (above entry)
	      this->ExitLongUnitAtLimit(aSecurity->getSymbol(),
					processingDateTime,
					entryPx,
					targetAsPercent,
					unitNum);

	      if (stopAsPercent.getAsPercent() > DecimalConstants<Decimal>::DecimalZero) {
		// Original percent stop (below entry for longs)
		this->ExitLongUnitAtStop(aSecurity->getSymbol(),
					 processingDateTime,
					 entryPx,
					 stopAsPercent,
					 unitNum);
	      }

	      // Maintain R-multiple bookkeeping as before
	      instrPos.setRMultipleStop(LongStopLoss<Decimal>(entryPx, stopAsPercent).getStopLoss(), unitNum);
	    }
	  else if (this->isShortPosition(aSecurity->getSymbol()))
	    {
	      // Original percent target (below entry)
	      this->ExitShortUnitAtLimit(aSecurity->getSymbol(),
					 processingDateTime,
					 entryPx,
					 targetAsPercent,
					 unitNum);

	      if (stopAsPercent.getAsPercent() > DecimalConstants<Decimal>::DecimalZero) {
		// Original percent stop (above entry for shorts)
		this->ExitShortUnitAtStop(aSecurity->getSymbol(),
					  processingDateTime,
					  entryPx,
					  stopAsPercent,
					  unitNum);
	      }

	      // Maintain R-multiple bookkeeping as before
	      instrPos.setRMultipleStop(ShortStopLoss<Decimal>(entryPx, stopAsPercent).getStopLoss(), unitNum);
	    }
	  else
	    {
	      throw PalStrategyException("PalMetaStrategy::eventExitOrders - expecting long or short position but found none or error state");
	    }
	} // end for each unit
    }

  private:
    // Helper method to create portfolio filter from portfolio's first security
    std::shared_ptr<Filter> createPortfolioFilter(std::shared_ptr<Portfolio<Decimal>> portfolio)
    {
      if (portfolio->getNumSecurities() == 0) {
	throw PalStrategyException("PalMetaStrategy: Portfolio must contain at least one security for filter initialization");
      }

      auto firstSecurity  = portfolio->beginPortfolio()->second;
      auto ohlcTimeSeries = firstSecurity->getTimeSeries();

      return std::make_shared<Filter>(*ohlcTimeSeries);
    }

    std::pair<bool,bool>
    detectBothSidesFire(Security<Decimal>* aSecurity,
			const boost::posix_time::ptime& processingDateTime) const
    {
      bool foundLong  = false;
      bool foundShort = false;

      auto patIt  = mPalPatterns.begin();
      auto evalIt = mPatternEvaluators.begin();

      for (; patIt != mPalPatterns.end() && evalIt != mPatternEvaluators.end(); ++patIt, ++evalIt)
	{
	  const auto& pat   = *patIt;
	  const bool  isLong = pat->isLongPattern();

	  // If we've already confirmed this side on this bar, no need to evaluate more of the same side.
	  if ((isLong && foundLong) || (!isLong && foundShort))
	    continue;

	  // Evaluate only the still-needed side.
	  if ((*evalIt)(aSecurity, processingDateTime))
	    {
	      if (isLong)
		foundLong  = true;
	      else
		foundShort = true;

	      // Early exit as soon as both sides are detected.
	      if (foundLong && foundShort)
		break;
	    }
	}

      return {foundLong, foundShort};
    }

    // Takes ptime for intraday order timing, but uses date for PatternEvaluator
    void entryOrdersCommon (Security<Decimal>* aSecurity,
			    const InstrumentPosition<Decimal>& instrPos, // (unused in this logic)
			    const ptime& processingDateTime,
			    const EntryOrderConditions<Decimal>& entryConditions)
    {
      // Portfolio filter check first
      if (!mPortfolioFilter->areEntriesAllowed(processingDateTime)) {
	return;
      }

      // Evaluate patterns in order; first match issues entry orders via entryConditions
      if (entryConditions.canEnterMarket(this, aSecurity))
	{
	  auto patIt  = mPalPatterns.begin();
	  auto evalIt = mPatternEvaluators.begin();
	  for (; patIt != mPalPatterns.end() && evalIt != mPatternEvaluators.end(); ++patIt, ++evalIt)
	    {
	      std::shared_ptr<PriceActionLabPattern> pricePattern = *patIt;

	      if (!entryConditions.canTradePattern(this, pricePattern, aSecurity)) {
		continue;
	      }

	      if ((*evalIt)(aSecurity, processingDateTime))
		{
		  entryConditions.createEntryOrders(this, pricePattern, aSecurity, processingDateTime);
		  break; // single pattern per bar for meta strategy
		}
	    }
	}
    }

  private:
    // --- state ---
    PalPatterns mPalPatterns;
    std::list<PatternEvaluator> mPatternEvaluators;

    MCPTStrategyAttributes<Decimal> mMCPTAttributes;

    uint32_t mStrategyMaxBarsBack;

    std::shared_ptr<Filter> mPortfolioFilter;

    // Breakeven controls
    bool mBreakevenEnabled;
    unsigned int mBreakevenActivationBars;

    // [BOTH-SIDES] Neutrality toggle: when true, skip entries if both directions fire simultaneously
    bool mSkipIfBothSidesFire;
  };
  
  /**
   * @brief Base class for price-action-based strategies using a single pattern.
   */
  template <class Decimal> class PalStrategy : public BacktesterStrategy<Decimal>
  {
  public:
    using PatternEvaluator = typename PALPatternInterpreter<Decimal>::PatternEvaluator;

    PalStrategy(const std::string& strategyName,
                std::shared_ptr<PriceActionLabPattern> pattern,
                std::shared_ptr<Portfolio<Decimal>> portfolio,
                const StrategyOptions& strategyOptions)
      : BacktesterStrategy<Decimal>(strategyName, portfolio, strategyOptions),
        mPalPattern(pattern),
        mMCPTAttributes()
    {
      if (!mPalPattern)
	throw PalStrategyException("PalStrategy: mPalPattern.");

      if (mPalPattern->getPatternExpression())
      {
        mPatternEvaluator =
          PALPatternInterpreter<Decimal>::compileEvaluator(mPalPattern->getPatternExpression().get());
      }
      else
      {
        // No pattern, so never match
        mPatternEvaluator = [](Security<Decimal>*, const boost::posix_time::ptime& )
        {
          return false;
        };
      }
    }

    PalStrategy(const PalStrategy<Decimal>& rhs)
      : BacktesterStrategy<Decimal>(rhs),
        mPalPattern(rhs.mPalPattern),
        mMCPTAttributes(rhs.mMCPTAttributes),
        mPatternEvaluator(rhs.mPatternEvaluator)
    {}

    PalStrategy<Decimal>&
    operator=(const PalStrategy<Decimal>& rhs)
    {
      if (this == &rhs)
      {
        return *this;
      }
      BacktesterStrategy<Decimal>::operator=(rhs);
      mPalPattern = rhs.mPalPattern;
      mMCPTAttributes = rhs.mMCPTAttributes;
      mPatternEvaluator = rhs.mPatternEvaluator;
      return *this;
    }

    ~PalStrategy() override = default;

    virtual std::shared_ptr<PalStrategy<Decimal>>
    clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const = 0;

    const TradingVolume& getSizeForOrder(const Security<Decimal>& aSecurity) const override
    {
      if (aSecurity.isEquitySecurity())
      {
        return OneShare;
      }
      else
      {
        return OneContract;
      }
    }

    uint32_t getPatternMaxBarsBack() const override
    {
      return mPalPattern->getMaxBarsBack();
    }

    std::shared_ptr<PriceActionLabPattern> getPalPattern() const
    {
      return mPalPattern;
    }

    unsigned long long hashCode() const override
    {
      // Get base UUID hash
      unsigned long long uuidHash = BacktesterStrategy<Decimal>::hashCode();
      
          // Get pattern-specific hash
      unsigned long long patternHash = mPalPattern->hashCode();
      
      // Combine using boost::hash_combine algorithm
      return uuidHash ^ (patternHash + 0x9e3779b9 + (uuidHash << 6) + (uuidHash >> 2));
    }

    /**
     * @brief Get the pattern hash component only (for debugging/analysis)
     * @return Hash from the underlying PriceActionLabPattern
     */
    unsigned long long getPatternHash() const
    {
      return mPalPattern->hashCode();
    }

    [[deprecated("Use of this getPositionDirectionVector will throw an exception")]] // Attribute from HEAD
    std::vector<int> getPositionDirectionVector() const
    {
      throw PalStrategyException("getPositionDirectionVector is no longer supported"); // Message from HEAD
      // return mMCPTAttributes.getPositionDirection(); // Original code commented out in HEAD
    }

    [[deprecated("Use of this getPositionReturnsVector will throw an exception")]] // Attribute from HEAD
    std::vector<Decimal> getPositionReturnsVector() const
    {
      throw PalStrategyException("getPositionReturnsVector is no longer supported"); // Message from HEAD
      // return mMCPTAttributes.getPositionReturns(); // Original code commented out in HEAD
    }

    [[deprecated("Use of this numTradingOpportunities will throw an exception")]] // Attribute from HEAD
    unsigned long numTradingOpportunities() const
    {
      throw PalStrategyException("numTradingOpportunities is no longer supported"); // Message from HEAD
      // return mMCPTAttributes.numTradingOpportunities(); // Original code commented out in HEAD
    }

    bool isLongStrategy() const
    {
      return mPalPattern->isLongPattern();
    }

    bool isShortStrategy() const
    {
      return mPalPattern->isShortPattern();
    }

  protected:
    const PatternEvaluator& getPatternEvaluator() const
    {
      return mPatternEvaluator;
    }

    void setPatternEvaluator(const PatternEvaluator& eval)
    {
      mPatternEvaluator = eval;
    }

    [[deprecated("Use of this addLongPositionBar no longer supported")]]
    void addLongPositionBar(std::shared_ptr<Security<Decimal>> /*aSecurity*/,
                            const date& /*processingDate*/)
    {
      //mMCPTAttributes.addLongPositionBar (aSecurity, processingDate); // Original code commented out in HEAD
    }

    [[deprecated("Use of this addShortPositionBar no longer supported")]]
    void addShortPositionBar(std::shared_ptr<Security<Decimal>> /*aSecurity*/,
                             const date& /*processingDate*/)
    {
      //mMCPTAttributes.addShortPositionBar (aSecurity, processingDate); // Original code commented out in HEAD
    }

    [[deprecated("Use of this addFlatPositionBar no longer supported")]]
    void addFlatPositionBar(std::shared_ptr<Security<Decimal>> /*aSecurity*/,
                            const date& /*processingDate*/)
    {
      //mMCPTAttributes.addFlatPositionBar (aSecurity, processingDate); // Original code commented out in HEAD
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
   * - **Entry**: on each bar, if flat or pyramiding is allowed and the
   * pattern evaluator fires, it issues an `EnterLongOnOpen` with the
   * configured stop‐loss and profit‐target.
   * - **Exit**: for open long positions, it submits both a limit exit at
   * profit‐target and a stop‐loss exit, then updates the R‐multiple
   * on the filled bar.
   *
   * @details
   * When used under our BackTester, every bar’s P&L—including the bar on which
   * a profit‐target or stop‐loss fires—is recorded at the finest resolution.
   * This is critical for building accurate null distributions in both
   * permutation tests (e.g., Masters’s step‐down algorithm) and bootstrap
   * confidence intervals, since it:
   * - Maintains a large, homogeneous sample of bar‐returns for resampling.
   * - Preserves time‐series properties (autocorrelation, volatility clustering).
   * - Ensures exit‐bar P&L is never dropped, giving robust, low‐variance
   * statistics for significance testing and interval estimation.
   *
   * @tparam Decimal  Numeric type for price/return calculations (e.g., double).
   */
  template <class Decimal> class PalLongStrategy : public PalStrategy<Decimal>
  {
  public:
    // Bring base class date-based methods into scope

    using BacktesterStrategy<Decimal>::eventEntryOrders;
    using BacktesterStrategy<Decimal>::eventExitOrders;

    PalLongStrategy(const std::string& strategyName,
                    std::shared_ptr<PriceActionLabPattern> pattern,
                    std::shared_ptr<Portfolio<Decimal>> portfolio,
                    const StrategyOptions& strategyOptions = defaultStrategyOptions)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio, strategyOptions)
    {}

    PalLongStrategy(const PalLongStrategy<Decimal>& rhs)
      : PalStrategy<Decimal>(rhs)
    {}

    PalLongStrategy<Decimal>& // Return Type& from HEAD
    operator=(const PalLongStrategy<Decimal>& rhs)
    {
      if (this == &rhs)
      {
        return *this;
      }
      PalStrategy<Decimal>::operator=(rhs);
      return *this;
    }

    ~PalLongStrategy() override = default;

    std::shared_ptr<BacktesterStrategy<Decimal>>
    clone (const std::shared_ptr<Portfolio<Decimal>>& portfolio) const override
    {
      return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
                                                        this->getPalPattern(),
                                                        portfolio,
                                                        this->getStrategyOptions());
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    clone_shallow(const std::shared_ptr<Portfolio<Decimal>>& portfolio) const override
    {
      // Construct a new strategy bound to the new portfolio, but DO NOT recompile with evaluater.
      auto cloned = std::make_shared<PalLongStrategy<Decimal>>(
							       this->getStrategyName(),
							       this->getPalPattern(),      // same (immutable) pattern
							       portfolio,
							       this->getStrategyOptions());
      
      // Reuse the precompiled evaluator to avoid per-permutation compilation cost.
      cloned->setPatternEvaluator(this->getPatternEvaluator());
      return cloned;
    }

    std::shared_ptr<PalStrategy<Decimal>>
    clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const override
    {
      return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
                                                        this->getPalPattern(),
                                                        portfolio,
                                                        this->getStrategyOptions());
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    cloneForBackTesting () const override
    {
      return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
                                                        this->getPalPattern(),
                                                        this->getPortfolio(),
                                                        this->getStrategyOptions());
    }

    /**
     * @brief Evaluate and submit exit orders for long positions on this bar.
     */
    void eventExitOrders (Security<Decimal>* aSecurity,
                          const InstrumentPosition<Decimal>& instrPos,
                          const ptime& processingDateTime) override
    {
      if (this->isLongPosition (aSecurity->getSymbol()))
      {
        // NEW: Check for 8-bar exit rule first (takes priority)
        uint32_t numUnits = instrPos.getNumPositionUnits();
        if (numUnits > 0)
        {
          auto it = instrPos.getInstrumentPosition(numUnits);
          auto pos = *it;

	  unsigned int maxHold = this->getStrategyOptions().getMaxHoldingPeriod();
	  
          if (maxHold > 0 && pos->getNumBarsSinceEntry() >= maxHold)
          {
            this->ExitLongAllUnitsAtOpen(aSecurity->getSymbol(), processingDateTime);
            return; // Don't place other exit orders
          }
        }

        // EXISTING: Profit target and stop loss logic
        std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();

        Decimal target = pattern->getProfitTargetAsDecimal();
        PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);
        Decimal stop = pattern->getStopLossAsDecimal();
        PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);
        Decimal fillPrice = instrPos.getFillPrice();

        this->ExitLongAllUnitsAtLimit(aSecurity->getSymbol(), processingDateTime,
                                      fillPrice, targetAsPercent);
        this->ExitLongAllUnitsAtStop(aSecurity->getSymbol(), processingDateTime,
                                     fillPrice, stopAsPercent);
        instrPos.setRMultipleStop (LongStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());
      }
    }

    /**
     * @brief Evaluate and submit new long‐entry orders based on the pattern.
     */
    void eventEntryOrders (Security<Decimal>* aSecurity,
                           const InstrumentPosition<Decimal>& /* instrPos */,
                           const ptime& processingDateTime) override
    {
      auto sym = aSecurity->getSymbol();

      if (this->isFlatPosition (sym) || this->strategyCanPyramid(sym))
      {
        std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();

        if (this->getSecurityBarNumber(sym) > pattern->getMaxBarsBack())
        {
          // PatternEvaluator now uses full processingDateTime
          if (this->getPatternEvaluator()(aSecurity, processingDateTime))
          {
            Decimal targetValue = pattern->getProfitTargetAsDecimal();
            Decimal stopValue = pattern->getStopLossAsDecimal();
            this->EnterLongOnOpen (sym, processingDateTime, stopValue, targetValue);
          }
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
   * - **Entry**: on each bar, if flat or pyramiding is allowed and the
   * pattern evaluator fires, it issues an `EnterShortOnOpen` with the
   * configured stop‐loss and profit‐target.
   * - **Exit**: for open short positions, it submits both a limit exit at
   * profit‐target and a stop‐loss exit, then updates the R‐multiple
   * on the filled bar.
   *
   * @details
   * As with long trades, every bar’s P&L—including the bar on which a short‐side
   * profit‐target or stop‐loss fires—is captured at the bar level.  This fine‐grained
   * return series is essential for:
   * - Stable permutation‐test null distributions (strong FWE control).
   * - Accurate bootstrap of out‐of‐sample performance (tight CI’s).
   * - Fair comparison across strategies, since exit‐bar outcomes are never lost.
   *
   * @tparam Decimal  Numeric type for price/return calculations (e.g., double).
   */
  template <class Decimal> class PalShortStrategy : public PalStrategy<Decimal>
  {
  public:
    // Bring base class date-based methods into scope
    using BacktesterStrategy<Decimal>::eventEntryOrders;
    using BacktesterStrategy<Decimal>::eventExitOrders;

    PalShortStrategy(const std::string& strategyName,
                     std::shared_ptr<PriceActionLabPattern> pattern,
                     std::shared_ptr<Portfolio<Decimal>> portfolio,
                     const StrategyOptions& strategyOptions = defaultStrategyOptions)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio, strategyOptions)
    {}

    PalShortStrategy(const PalShortStrategy<Decimal>& rhs)
      : PalStrategy<Decimal>(rhs)
    {}

    PalShortStrategy<Decimal>&
    operator=(const PalShortStrategy<Decimal>& rhs)
    {
      if (this == &rhs)
      {
        return *this;
      }
      PalStrategy<Decimal>::operator=(rhs);
      return *this;
    }

    ~PalShortStrategy() override = default;

    std::shared_ptr<BacktesterStrategy<Decimal>>
    clone (const std::shared_ptr<Portfolio<Decimal>>& portfolio) const override
    {
      return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
                                                         this->getPalPattern(),
                                                         portfolio,
                                                         this->getStrategyOptions());
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    clone_shallow(const std::shared_ptr<Portfolio<Decimal>>& portfolio) const override
    {
      auto cloned = std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
								this->getPalPattern(),
								portfolio,
								this->getStrategyOptions());
      
      cloned->setPatternEvaluator(this->getPatternEvaluator());
      return cloned;
    }

    std::shared_ptr<PalStrategy<Decimal>>
    clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const override
    {
      return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
                                                         this->getPalPattern(),
                                                         portfolio,
                                                         this->getStrategyOptions());
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    cloneForBackTesting () const override
    {
      return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
                                                         this->getPalPattern(),
                                                         this->getPortfolio(),
                                                         this->getStrategyOptions());
    }

    /**
     * @brief Evaluate and submit exit orders for short positions on this bar.
     *
     * @details
     * Called before entry each bar.  For short trades, submits:
     * - A limit‐to‐cover at the profit‐target price.
     * - A stop‐to‐cover at the stop‐loss price.
     * - Records the exit bar’s P&L in the high‐res series.
     *
     * @param aSecurity			Security to exit.
     * @param instrPos			InstrumentPosition for the current bar.
     * @param processingDateTime	Date/Time of this bar.
       */

    void eventExitOrders (Security<Decimal>* aSecurity,
                          const InstrumentPosition<Decimal>& instrPos,
                          const ptime& processingDateTime) override
    {
      if (this->isShortPosition (aSecurity->getSymbol()))
      {
        // NEW: Check for 8-bar exit rule first (takes priority)
        uint32_t numUnits = instrPos.getNumPositionUnits();
        if (numUnits > 0)
        {
          auto it = instrPos.getInstrumentPosition(numUnits);
          auto pos = *it;

	  unsigned int maxHold = this->getStrategyOptions().getMaxHoldingPeriod();
          if (maxHold > 0 && pos->getNumBarsSinceEntry() >= maxHold)
          {
            // Exit all units at market after maxHold bars
            this->ExitShortAllUnitsAtOpen(aSecurity->getSymbol(), processingDateTime);
            return; // Don't place other exit orders
          }
        }

        // EXISTING: Profit target and stop loss logic
        std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();

        Decimal target = pattern->getProfitTargetAsDecimal();
        PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);
        Decimal stop = pattern->getStopLossAsDecimal();
        PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);
        Decimal fillPrice = instrPos.getFillPrice();

        this->ExitShortAllUnitsAtLimit(aSecurity->getSymbol(), processingDateTime,
                                       fillPrice, targetAsPercent);
        this->ExitShortAllUnitsAtStop(aSecurity->getSymbol(), processingDateTime,
                                      fillPrice, stopAsPercent);
        instrPos.setRMultipleStop (ShortStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());
      }
    }

    /**
     * @brief Evaluate and submit new short‐entry orders based on the pattern.
     *
     * @details
     * Called immediately after exits.  Checks if flat/pyramiding allowed,
     * tests the pattern on this bar, and issues `EnterShortOnOpen` if triggered.
     *
     * @param aSecurity			Security to evaluate for entry.
     * @param instrPos        		InstrumentPosition for the current bar.
     * @param processingDateTime	Date of this bar.
     */
    void eventEntryOrders (Security<Decimal>* aSecurity,
                           const InstrumentPosition<Decimal>& /* instrPos */,
                           const ptime& processingDateTime) override
    {
      auto sym = aSecurity->getSymbol();
      if (this->isFlatPosition (sym) || this->strategyCanPyramid(sym))
      {
        std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();

        if (this->getSecurityBarNumber(sym) > pattern->getMaxBarsBack())
        {
          // PatternEvaluator now uses full processingDateTime
          if (this->getPatternEvaluator()(aSecurity, processingDateTime))
          {
            // Orders placed with ptime. Explicit stop/target from HEAD
            Decimal targetValue = pattern->getProfitTargetAsDecimal();
            Decimal stopValue = pattern->getStopLossAsDecimal();
            this->EnterShortOnOpen (sym, processingDateTime, stopValue, targetValue);
          }
        }
      }
    }
  };

  template<typename Decimal>
  std::shared_ptr<PalStrategy<Decimal>> makePalStrategy(const std::string& name,
                                                        const std::shared_ptr<PriceActionLabPattern>& pattern,
                                                        const std::shared_ptr<Portfolio<Decimal>>& portfolio,
                                                        const StrategyOptions& strategyOptions = defaultStrategyOptions)
  {
    if (pattern->isLongPattern())
    {
      return std::make_shared<PalLongStrategy<Decimal>>(name, pattern, portfolio, strategyOptions);
    }
    else
    {
      return std::make_shared<PalShortStrategy<Decimal>>(name, pattern, portfolio, strategyOptions);
    }
  }

  template<typename Decimal>
  std::shared_ptr<PalStrategy<Decimal>> makePalStrategy(const std::string& name,
                                                        const std::shared_ptr<PriceActionLabPattern>& pattern,
                                                        const StrategyOptions& strategyOptions = defaultStrategyOptions)
  {
    auto newPortfolio = std::make_shared<Portfolio<Decimal>>(name + " Portfolio");
    return makePalStrategy(name, pattern, newPortfolio, strategyOptions);
  }

  // NEW OVERLOAD: Four-argument makePalStrategy to create Portfolio and add Security
  template<typename Decimal>
  std::shared_ptr<PalStrategy<Decimal>> makePalStrategy(const std::string& name,
                                                        const std::shared_ptr<PriceActionLabPattern>& pattern,
                                                        const std::shared_ptr<const Security<Decimal>>& security, // New argument
                                                        const StrategyOptions& strategyOptions = defaultStrategyOptions)
  {
      auto newPortfolio = std::make_shared<Portfolio<Decimal>>(name + " Portfolio");
      // Cast const Security to non-const Security for addSecurity method
      auto nonConstSecurity = std::const_pointer_cast<Security<Decimal>>(security);
      newPortfolio->addSecurity(nonConstSecurity); // Add the security to the newly created portfolio

      if (pattern->isLongPattern())
      {
          return std::make_shared<PalLongStrategy<Decimal>>(name, pattern, newPortfolio, strategyOptions);
      }
      else
      {
          return std::make_shared<PalShortStrategy<Decimal>>(name, pattern, newPortfolio, strategyOptions);
      }
  }
}

#endif
