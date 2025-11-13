// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __BACKTESTER_H
#define __BACKTESTER_H 1

#include <exception>
#include <list>
#include <vector>
#include <map>
#include <string>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "number.h"
#include "BoostDateHelper.h"
#include "BacktesterStrategy.h"
#include "MarketHours.h"
#include "TimeFrameDiscovery.h"
#include "StatUtils.h"


namespace mkc_timeseries
{
  using boost::gregorian::date;

  class BackTesterException : public std::runtime_error
  {
  public:
    BackTesterException(const std::string msg) 
      : std::runtime_error(msg)
    {}
    
    ~BackTesterException()
    {}
    
  };

  /**
   * @class BackTester
   * @brief Orchestrates the full backtesting loop by stepping through each trading day,
   *        triggering strategy logic, processing pending orders, and updating positions
   *        and order states.
   *
   * Responsibilities:
   * - Drive the simulation loop forward by day.
   * - Call eventEntryOrders and eventExitOrders on the strategy.
   * - Trigger execution of pending orders via TradingOrderManager.
   * - Maintain control flow and ensure correct sequencing of order processing.
   *
   * Observer Pattern Collaboration:
   * - BackTester does not directly observe order fills.
   * - Instead, it delegates order execution to StrategyBroker via BacktesterStrategy.
   * - StrategyBroker is registered as an observer with TradingOrderManager.
   * - When an order is executed, StrategyBroker is notified via OrderExecuted callbacks.
   *
   * Collaborators:
   * - BacktesterStrategy: defines trading logic for entry and exit conditions.
   * - StrategyBroker: handles order routing, position tracking, and fill notifications.
   *
   * Thread Safety:
   * - This class is **not thread-safe** and must not be shared across threads.
   * - Each `BackTester` instance must be used exclusively within the context of a single thread.
   * - All collaborating components (strategies, portfolios, security references, etc.) must be
   *independently owned per thread.
   * - Although safe usage is achieved in multithreaded environments via strict ownership isolation,
   *   the class itself performs no internal locking or concurrency protection.
   */
  using boost::gregorian::date;

  template <class Decimal>
  class BackTester
  {
  public:
    using StrategyPtr            = BacktesterStrategy<Decimal>*;
    using StrategyIterator       = typename std::list<std::shared_ptr<BacktesterStrategy<Decimal>>>::const_iterator;
    using StrategyRawIterator    = typename std::vector<StrategyPtr>::const_iterator;
    using BacktestDateRangeIterator = typename DateRangeContainer::ConstDateRangeIterator;

    /**
     * @brief Construct an empty BackTester with no strategies or dates.
     */
    explicit BackTester()
      : mStrategyList(),
	mStrategyRawList(),
	mBackTestDates()
    {}

    virtual ~BackTester()
    {}

    /**
     * @brief Copy constructor; clones strategy list and date ranges.
     * @param rhs Other BackTester to copy state from.
     */
    BackTester(const BackTester& rhs)
      : mStrategyList(rhs.mStrategyList),
	mBackTestDates(rhs.mBackTestDates)
    {
      rebuildStrategyRawList();
    }

    /**
     * @brief Assignment operator; copies strategies, dates, and bar series.
     * @param rhs Other BackTester to assign from.
     * @return Reference to this BackTester.
     */
    BackTester& operator=(const BackTester& rhs)
    {
      if (this != &rhs)
	{
	  mStrategyList = rhs.mStrategyList;
	  mBackTestDates = rhs.mBackTestDates;
	  rebuildStrategyRawList();
	}
      return *this;
    }

     /**
     * @brief Clone this BackTester, preserving configuration but not strategies.
     * @return Shared pointer to a new BackTester.
     * @note Must be implemented by derived classes (e.g., DailyBackTester).
     */
    virtual std::shared_ptr<BackTester<Decimal>> clone() const = 0;

    /**
     * @brief Add a strategy to be included in backtesting.
     * @param aStrategy Shared pointer to the strategy instance.
     */
    void addStrategy(const std::shared_ptr<BacktesterStrategy<Decimal>>& aStrategy)
    {
      mStrategyList.push_back(aStrategy);
      mStrategyRawList.push_back(aStrategy.get());
    }

    void setSingleStrategy(const std::shared_ptr<BacktesterStrategy<Decimal>>& s)
    {
      mStrategyList.clear();
      mStrategyRawList.clear();
      mStrategyList.push_back(s);
      mStrategyRawList.push_back(s.get());
    }

    /**
     * @brief Add a date-range over which to run the backtest.
     * @param range DateRange specifying start and end dates.
     */
    void addDateRange(const DateRange& range)
    {
      mBackTestDates.addDateRange(range);
    }

    /**
     * @brief Iterator to the first added strategy.
     * @return Const iterator to strategy list.
     */
    StrategyIterator beginStrategies() const
    {
      return mStrategyList.begin();
    }

    /**
     * @brief Iterator one past the last added strategy.
     * @return Const iterator to strategy list end.
     */
    StrategyIterator endStrategies() const
    {
      return mStrategyList.end();
    }

    /**
     * @brief Iterator to the first date-range used in backtesting.
     * @return Iterator to date-range container start.
     */
    BacktestDateRangeIterator beginBacktestDateRange() const
    {
      return mBackTestDates.beginDateRange();
    }

    /**
     * @brief Iterator one past the last date-range.
     * @return Iterator to date-range container end.
     */
    BacktestDateRangeIterator endBacktestDateRange() const
    {
      return mBackTestDates.endDateRange();
    }

    /**
     * @brief Number of distinct backtest date ranges configured.
     * @return Count of date ranges.
     */
    unsigned long numBackTestRanges() const
    {
      return mBackTestDates.getNumEntries();
    }

    /**
     * @brief Retrieve the closed-position history from the first strategy.
     * @return Reference to ClosedPositionHistory instance.
     * @throws BackTesterException if no strategies have been added.
     */
    const ClosedPositionHistory<Decimal>& getClosedPositionHistory() const
    {
      if (mStrategyList.empty())
	{
	  throw BackTesterException("getClosedPositionHistory: No strategies added");
	}
      return mStrategyList.front()->getStrategyBroker().getClosedPositionHistory();
    }

     /**
     * @brief Number of strategies currently registered.
     * @return Strategy count.
     */
    uint32_t getNumStrategies() const
    {
      return static_cast<uint32_t>(mStrategyList.size());
    }

    /**
     * @brief Extract a unified, high-resolution return series for one strategy.
     *
     * @details
     * This method walks every closed trade (via ClosedPositionHistory) and
     * every still-open position’s bar history to build a flat vector of
     * per-bar returns, computed as
     *   \f$\displaystyle r_t = \frac{close_t - close_{t-1}}{close_{t-1}}\f$.
     * It includes the very bar on which each trade exited, ensuring **no**
     * realized P&L is ever dropped.
     *
     * **Why bar-by-bar?**
     *  - **Large, homogeneous sample**:  Hundreds or thousands of bar returns
     *    give far more data points than a handful of trade P&Ls.  This
     *    drastically reduces estimator variance in resampling-based tests.
     *  - **Preserved time-series structure**:  Because each return is
     *    recorded at the native bar frequency—and trades are marked-to-market
     *    before exit—the resulting series captures autocorrelation and
     *    volatility clustering.  That lets you validly use block-bootstrap
     *    or block-permutation schemes when constructing null distributions.
     *  - **Sharper null distributions**:  In both permutation and bootstrap
     *    you’re effectively comparing observed statistics to an empirical
     *    sampling distribution.  Smoother, more finely grained nulls (from
     *    many bar returns) yield more precise p-values and confidence
     *    intervals than coarse, trade-level summaries.
     *  - **Strong FWE control with power**:  When plugged into a step-down
     *    permutation test (e.g. Masters’s algorithm), each permutation uses
     *    this rich bar-level statistic.  You maintain strong family-wise
     *    error control while maximizing power to detect “second-best,”
     *    “third-best,” etc., strategies.
     *  - **Robust out-of-sample inference**:  Bootstrapping OOS mean returns
     *    at the bar level (instead of per-trade) yields tighter, more
     *    realistic confidence bands—critical for spotting overfitting or
     *    regime shifts in live trading.
     *
     * @param strat  Pointer to the strategy whose history to extract.
     * @return A flat std::vector<Decimal> of all per-bar returns
     *         (closed and open) for that strategy.
     */
    virtual std::vector<Decimal> getAllHighResReturns(StrategyPtr strat) const
    {
      // 1) Get all high-resolution returns from closed trades.
      // This logic is now correctly encapsulated in ClosedPositionHistory.
      const auto& closedHist = strat->getStrategyBroker().getClosedPositionHistory();
      std::vector<Decimal> allReturns = closedHist.getHighResBarReturns();

      // 2) Append high-resolution returns from any currently open positions.
      for (auto it = strat->getPortfolio()->beginPortfolio();
	   it != strat->getPortfolio()->endPortfolio();
	   ++it)
	{
	  auto const& sec     = it->second;
	  const auto& instrPos = strat->getInstrumentPosition(sec->getSymbol());

	  for (uint32_t u = 1; u <= instrPos.getNumPositionUnits(); ++u)
	    {
	      auto posPtr = *instrPos.getInstrumentPosition(u);
	      auto begin  = posPtr->beginPositionBarHistory();
	      auto end    = posPtr->endPositionBarHistory();

	      if (begin == end)
		continue;

	      // For open positions, the first reference price is the entry price
	      Decimal prevReferencePrice = posPtr->getEntryPrice();

	      for (auto curr = begin; curr != end; ++curr)
		{
		  Decimal currentClose = curr->second.getCloseValue();
                  Decimal returnForBar;

                  if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                      returnForBar = (currentClose - prevReferencePrice) / prevReferencePrice;
                  else
                      returnForBar = DecimalConstants<Decimal>::DecimalZero;


                  if (posPtr->isShortPosition())
                  {
                    returnForBar *= -1;
                  }

		  allReturns.push_back(returnForBar);

                  // Update the reference price for the next bar's calculation
		  prevReferencePrice = currentClose;
		}
	    }
	}

      return allReturns;
    }

    virtual std::vector<std::pair<boost::posix_time::ptime, Decimal>>
    getAllHighResReturnsWithDates(StrategyPtr strat) const
    {
      using boost::posix_time::ptime;

      std::vector<std::pair<ptime, Decimal>> all;

      // 1) Closed trades: delegate to ClosedPositionHistory’s timestamped extractor
      const auto& closedHist = strat->getStrategyBroker().getClosedPositionHistory();
      {
        auto closed = closedHist.getHighResBarReturnsWithDates();
        all.insert(all.end(),
                   std::make_move_iterator(closed.begin()),
                   std::make_move_iterator(closed.end()));
      }

      // 2) Open positions: append bar-by-bar M2M returns with timestamps
      for (auto it = strat->getPortfolio()->beginPortfolio();
	   it != strat->getPortfolio()->endPortfolio();
	   ++it)
	{
	  const auto& sec     = it->second;
	  const auto& instr   = strat->getInstrumentPosition(sec->getSymbol());

	  for (uint32_t u = 1; u <= instr.getNumPositionUnits(); ++u)
	    {
	      auto posPtr = *instr.getInstrumentPosition(u);

	      auto begin = posPtr->beginPositionBarHistory();
	      auto end   = posPtr->endPositionBarHistory();
	      if (begin == end)
                continue;

	      // For open positions, start from the actual entry price
	      Decimal prevReferencePrice = posPtr->getEntryPrice();

	      for (auto curr = begin; curr != end; ++curr)
		{
		  const ptime ts          = curr->first;               // bar timestamp
		  const Decimal closeNow  = curr->second.getCloseValue();
		  Decimal rForBar;

		  if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                    rForBar = (closeNow - prevReferencePrice) / prevReferencePrice;
		  else
                    rForBar = DecimalConstants<Decimal>::DecimalZero;

		  if (posPtr->isShortPosition())
                    rForBar *= -1;

		  all.emplace_back(ts, rForBar);

		  prevReferencePrice = closeNow;
		}
	    }
	}

      return all;
    }
    
    std::vector<ExpandedBarMetrics<Decimal>> getExpandedHighResReturns(StrategyPtr strat) const
    {
      std::vector<ExpandedBarMetrics<Decimal>> allMetrics;
      
      // Closed trades
      const auto& closedHist = strat->getStrategyBroker().getClosedPositionHistory();
      auto closedMetrics = closedHist.getExpandedHighResBarReturns();
      allMetrics.insert(allMetrics.end(), closedMetrics.begin(), closedMetrics.end());
      
      // Open trades
      for (auto it = strat->getPortfolio()->beginPortfolio();
	   it != strat->getPortfolio()->endPortfolio();
	   ++it)
	{
	  const auto& sec = it->second;
	  const auto& instrPos = strat->getInstrumentPosition(sec->getSymbol());
	  
	  for (uint32_t u = 1; u <= instrPos.getNumPositionUnits(); ++u)
	    {
	      auto posPtr = *instrPos.getInstrumentPosition(u);
	      auto begin = posPtr->beginPositionBarHistory();
	      auto end = posPtr->endPositionBarHistory();
	      
	      if (std::distance(begin, end) < 2)
                continue;
	      
	      auto prev = begin;
	      for (auto curr = std::next(begin); curr != end; ++curr)
		{
		  const auto& prevBar = prev->second;
		  const auto& bar = curr->second;
		  
		  Decimal prevClose = prevBar.getCloseValue();
		  Decimal open = bar.getOpenValue();
		  Decimal high = bar.getHighValue();
		  Decimal low = bar.getLowValue();
		  Decimal close = bar.getCloseValue();
		  
		  if (prevClose == Decimal(0))
                    continue;

		  ExpandedBarMetrics<Decimal> metrics;
		  metrics.closeToClose = (close - prevClose) / prevClose;
		  metrics.openToClose = (close - open) / open;
		  metrics.highToOpen  = (high - open) / open;
		  metrics.lowToOpen   = (low - open) / open;

		  allMetrics.push_back(metrics);
		  prev = curr;
		}
	    }
	}

      return allMetrics;
    }

    /**
     * @brief Get the total number of trades (closed + open) for the first strategy
     * @return Total count of closed trades plus open position units
     * @throws BackTesterException if no strategies have been added
     */
    virtual uint32_t getNumTrades() const
    {
        if (mStrategyList.empty()) {
            throw BackTesterException("getNumTrades: No strategies added");
        }
        
        auto strategy = *(beginStrategies());
        
        // Get closed trades count
        uint32_t closedTrades = strategy->getStrategyBroker().getClosedTrades();
        
        // Get open trades count by iterating through instrument positions
        uint32_t openTrades = 0;
        for (auto it = strategy->getPortfolio()->beginPortfolio();
             it != strategy->getPortfolio()->endPortfolio();
             ++it) {
            
            const auto& security = it->second;
            const auto& instrPos = strategy->getInstrumentPosition(security->getSymbol());
            
            // Each position unit represents one trade
            openTrades += instrPos.getNumPositionUnits();
        }
        
        return closedTrades + openTrades;
    }

    /**
     * @brief Calculates the estimated annualized number of trades based on the backtest period.
     *
     * This method uses the total number of trades generated during the backtest and
     * normalizes it to a one-year period. This provides a data-driven estimate for
     * a strategy's trading frequency, which is essential for calculating annualized costs.
     *
     * @return The estimated number of trades per year as a double.
     * @throws BackTesterException if the backtest duration is zero or negative.
     */
    double getEstimatedAnnualizedTrades() const
    {
        uint32_t total_trades = getNumTrades();

        boost::gregorian::days duration_in_days = getEndDate() - getStartDate();
        
        if (duration_in_days.days() <= 0)
        {
            throw BackTesterException("getEstimatedAnnualizedTrades: Backtest duration must be positive.");
        }

        // Convert duration to years (using 365.25 to account for leap years)
        double duration_in_years = duration_in_days.days() / 365.25;

        return static_cast<double>(total_trades) / duration_in_years;
    }

    /**
     * @brief Get the total number of bars across all trades (closed + open) for the first strategy
     * @return Total count of bars in all closed and open trades
     * @throws BackTesterException if no strategies have been added
     */
    virtual uint32_t getNumBarsInTrades() const
    {
        if (mStrategyList.empty()) {
            throw BackTesterException("getNumBarsInTrades: No strategies added");
        }
        
        auto strategy = *(beginStrategies());
        
        // Get bars from closed trades
        const auto& closedHistory = strategy->getStrategyBroker().getClosedPositionHistory();
        uint32_t closedTradeBars = closedHistory.getNumBarsInMarket();
        
        // Get bars from open trades
        uint32_t openTradeBars = 0;
        for (auto it = strategy->getPortfolio()->beginPortfolio();
             it != strategy->getPortfolio()->endPortfolio();
             ++it) {
            
            const auto& security = it->second;
            const auto& instrPos = strategy->getInstrumentPosition(security->getSymbol());
            
            // Iterate through each open position unit
            for (uint32_t unitNum = 1; unitNum <= instrPos.getNumPositionUnits(); ++unitNum) {
                auto posPtr = *instrPos.getInstrumentPosition(unitNum);
                openTradeBars += posPtr->getNumBarsInPosition();
            }
        }
        
        return closedTradeBars + openTradeBars;
    }

    /**
     * @brief Compute the Profit Factor for the first strategy using high-resolution returns.
     * @details This method extracts all high-resolution bar returns from the first strategy
     * and computes the Profit Factor using StatUtils::computeProfitFactor.
     * @return The Profit Factor as a Decimal.
     * @throws BackTesterException if no strategies have been added.
     */
    Decimal getProfitFactor() const
    {
        if (mStrategyList.empty()) {
            throw BackTesterException("getProfitFactor: No strategies added");
        }
        
        auto strategy = *(beginStrategies());
        auto returns = getAllHighResReturns(strategy.get());
        
        return StatUtils<Decimal>::computeProfitFactor(returns);
    }

    /**
     * @brief Compute both the Profit Factor and Profitability for the first strategy.
     * @details This method extracts all high-resolution bar returns from the first strategy
     * and computes both the Profit Factor and required Win Rate (Profitability) using
     * StatUtils::computeProfitability.
     * @return A std::tuple<Decimal, Decimal> containing:
     * - get<0>: The Profit Factor
     * - get<1>: The required Win Rate (Profitability) as a percentage
     * @throws BackTesterException if no strategies have been added.
     */
    std::tuple<Decimal, Decimal> getProfitability() const
    {
        if (mStrategyList.empty()) {
            throw BackTesterException("getProfitability: No strategies added");
        }
        
        auto strategy = *(beginStrategies());
        auto returns = getAllHighResReturns(strategy.get());
        
        return StatUtils<Decimal>::computeProfitability(returns);
    }

    unsigned int getNumConsecutiveLosses() const
    {
      return (this->getClosedPositionHistory().getNumConsecutiveLosses());
    }
    
    /**
     * @brief Earliest date used across all backtest ranges.
     * @return Start date of backtest.
     */
    date getStartDate() const
    {
      return mBackTestDates.getFirstDateRange().getFirstDate();
    }

    /**
     * @brief Earliest date/time used across all backtest ranges.
     * @return Start date of backtest.
     */
    boost::posix_time::ptime getStartDateTime() const
    {
      return mBackTestDates.getFirstDateRange().getFirstDateTime();
    }

    /**
     * @brief Latest date used across all backtest ranges.
     * @return End date of backtest.
     */
    date getEndDate() const
    {
      return mBackTestDates.getFirstDateRange().getLastDate();
    }

        /**
     * @brief Latest date used across all backtest ranges.
     * @return End date of backtest.
     */
    boost::posix_time::ptime getEndDateTime() const
    {
      return mBackTestDates.getFirstDateRange().getLastDateTime();
    }

    /**
     * @brief Get the StrategyBroker for a strategy by name.
     * @param strategyName The name of the strategy to find
     * @return Reference to the StrategyBroker for the named strategy
     * @throws BackTesterException if the strategy is not found
     */
    auto& getStrategyBrokerForStrategy(const std::string& strategyName)
    {
      for (auto& strategy : mStrategyList)
      {
        if (strategy->getStrategyName() == strategyName)
        {
          return strategy->getStrategyBroker();
        }
      }
      throw BackTesterException("Strategy not found: " + strategyName);
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the daily time frame.
     * @return `true` if operating on the daily time frame `false` otherwise
     */
    virtual bool isDailyBackTester() const = 0;

    /**
     * @brief Determines whether this is a backtester that operates
     * on the weekly time frame.
     * @return `true` if operating on the weekl time frame `false` otherwise
     */
    virtual bool isWeeklyBackTester() const = 0;

    /**
     * @brief Determines whether this is a backtester that operates
     * on the monthly time frame.
     * @return `true` if operating on the monthly time frame `false` otherwise
     */
    virtual bool isMonthlyBackTester() const = 0;

    /**
     * @brief Determines whether this is a backtester that operates
     * on intraday time frames.
     * @return `true` if operating on intraday time frames `false` otherwise
     */
    virtual bool isIntradayBackTester() const = 0;
    
    /**
     * @brief Execute the full backtest across all configured date ranges.
     *
     * For each date range, saves bar dates, iterates through each bar (skipping the first),
     * processes entry/exit logic per strategy, records per-bar P&L via getLatestBarReturn(),
     * and handles multi-range rollovers by closing positions at range boundaries.
     *
     * @throws BackTesterException if no strategies are registered.
     */
    virtual void backtest()
    {
      typename BackTester<Decimal>::StrategyRawIterator itStrategy;
      typename BacktesterStrategy<Decimal>::PortfolioIterator iteratorPortfolio;

      if (mStrategyRawList.empty())
	{
	  throw BackTesterException("No strategies have been added to backtest");
	}

      bool multipleRanges = numBackTestRanges() > 1;
      unsigned int backtestNumber = 0;

      // ─── Outer loop over each DateRange ────────────────────────────────
      for (auto itRange = beginBacktestDateRange();
	   itRange != endBacktestDateRange();
	   ++itRange)
	{
	  // 1) Get the unified timestamp sequence from actual data
	  auto timestamps = getUnifiedTimestampSequence(itRange->second);

	  if (timestamps.empty()) {
	    continue; // Skip ranges with no data
	  }

	  // 2) Compute the "last bar" for this range
	  auto barBeforeBackTesterEndDateTime = timestamps.back();
	  ++backtestNumber;

	  // ─── Inner loop over actual timestamps (ptime-based) ───────────────────────────
	  for (size_t idx = 1; idx < timestamps.size(); ++idx)
	    {
	      const auto& currentTimestamp = timestamps[idx];
	      const auto& orderTimestamp = timestamps[idx - 1];

	      for (itStrategy = beginStrategiesRaw();
		   itStrategy != endStrategiesRaw();
		   ++itStrategy)
		{
		  StrategyPtr strat = *itStrategy;

		  for (iteratorPortfolio = strat->beginPortfolio();
		       iteratorPortfolio != strat->endPortfolio();
		       ++iteratorPortfolio)
		    {
		      const auto& secPtr = iteratorPortfolio->second;

		      if (multipleRanges
			  && currentTimestamp >= barBeforeBackTesterEndDateTime
			  && backtestNumber < numBackTestRanges())
			{
			  closeAllPositions(orderTimestamp);
			}
		      else
			{
			  processStrategyBar(secPtr.get(), strat, orderTimestamp);
			}

		      strat->eventProcessPendingOrders(currentTimestamp);
		    }
		}
	    }
	}
    }

  private:
    StrategyRawIterator beginStrategiesRaw() const
    {
      return mStrategyRawList.begin();
    }

    StrategyRawIterator endStrategiesRaw() const
    {
      return mStrategyRawList.end();
    }

    void rebuildStrategyRawList()
    {
      mStrategyRawList.clear();
      mStrategyRawList.reserve(mStrategyList.size());
      for (const auto& sp : mStrategyList)
	{
	  mStrategyRawList.push_back(sp.get());
	}
    }
    
    inline void processStrategyBar(Security<Decimal>* security,
			    StrategyPtr strategy,
			    const boost::posix_time::ptime& processingDateTime)
    {
      if (!strategy->doesSecurityHaveTradingData(*security, processingDateTime))
	{
	  return;
	}

      const auto symbol = security->getSymbol();
      strategy->eventUpdateSecurityBarNumber(symbol);

      if (!strategy->isFlatPosition(symbol))
	{
	  strategy->eventExitOrders(
				    security,
				    strategy->getInstrumentPosition(symbol),
				    processingDateTime);  // Pass ptime directly!
	}
      strategy->eventEntryOrders(
				 security,
				 strategy->getInstrumentPosition(symbol),
				 processingDateTime);  // Pass ptime directly!
    }

    void closeAllPositions(const boost::posix_time::ptime& orderDateTime)
    {
      for (auto itStrat = beginStrategiesRaw(); itStrat != endStrategiesRaw(); ++itStrat)
	{
	  StrategyPtr strategy = *itStrat;
	  for (auto itPort = strategy->beginPortfolio(); itPort != strategy->endPortfolio(); ++itPort)
	    {
	      const auto& securityPtr = itPort->second;
	      const auto symbol = securityPtr->getSymbol();
	      strategy->eventUpdateSecurityBarNumber(symbol);
	      strategy->ExitAllPositions(symbol, orderDateTime);  // Pass ptime directly!
	    }
	}
    }

  private:
    /**
     * @brief Get unified timestamp sequence from all securities in the portfolio
     * @param dateRange The date range to filter timestamps
     * @return Sorted vector of all unique timestamps across all securities
     */
    std::vector<boost::posix_time::ptime> getUnifiedTimestampSequence(const DateRange& dateRange)
    {
      std::set<boost::posix_time::ptime> allTimestamps;

      // Collect timestamps from all securities
      for (auto itStrategy = beginStrategiesRaw();
	   itStrategy != endStrategiesRaw();
	   ++itStrategy)
	{
	  StrategyPtr strat = *itStrategy;
	  for (auto itPortfolio = strat->beginPortfolio();
	       itPortfolio != strat->endPortfolio();
	       ++itPortfolio)
	    {
	      const auto& security = itPortfolio->second;
	      auto timeSeries = security->getTimeSeries();

	      // Get all timestamps from this security's time series
	      for (auto it = timeSeries->beginSortedAccess();
		   it != timeSeries->endSortedAccess();
		   ++it)
		{
		  auto timestamp = it->getDateTime();  // Already ptime!

		  // Filter by ptime range - maintains full precision!
		  if (timestamp >= dateRange.getFirstDateTime() &&
		      timestamp <= dateRange.getLastDateTime())
		    {
		      allTimestamps.insert(timestamp);
		    }
		}
	    }
	}

      // Convert to sorted vector
      return std::vector<boost::posix_time::ptime>(allTimestamps.begin(), allTimestamps.end());
    }

  private:
    std::list<std::shared_ptr<BacktesterStrategy<Decimal>>> mStrategyList;
    std::vector<StrategyPtr> mStrategyRawList;
    DateRangeContainer mBackTestDates;
  };

  //
  // class DailyBackTester
  //

  template <class Decimal> class DailyBackTester : public BackTester<Decimal>
  {
  public:
    explicit DailyBackTester(boost::gregorian::date startDate,
			     boost::gregorian::date endDate)
      : BackTester<Decimal>()
    {
      if (isWeekend (startDate))
	startDate = boost_next_weekday (startDate);

      if (isWeekend (endDate))
	endDate = boost_previous_weekday (endDate);

      DateRange r(startDate, endDate);
      this->addDateRange(r);
    }

    DailyBackTester() :
      BackTester<Decimal>()
    {}

    ~DailyBackTester()
    {}

    DailyBackTester(const DailyBackTester<Decimal> &rhs)
      :  BackTester<Decimal>(rhs)
    {}

    DailyBackTester<Decimal>&
    operator=(const DailyBackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Decimal>::operator=(rhs);
      return *this;
    }

    /**
     * @brief Clone the DailyBackTester with date ranges, but without strategies.
     *
     * Only the backtest date configuration is cloned.
     * The strategy list is left empty to allow caller-controlled population.
     *
     * This behavior is intentional to support multithreaded backtesting, where
     * each thread constructs and assigns strategy instances independently.
     */
    std::shared_ptr<BackTester<Decimal>> clone() const
    {
      auto back = std::make_shared<DailyBackTester<Decimal>>();
      auto it = this->beginBacktestDateRange();
      for (; it != this->endBacktestDateRange(); it++)
	back->addDateRange(it->second);

      return back;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the daily time frame.
     * @return `true`
     */
    bool isDailyBackTester() const
    {
      return true;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the weekly time frame.
     * @return `false`.
     */
    bool isWeeklyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the monthly time frame.
     * @return `false`.
     */
    bool isMonthlyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on intraday time frames.
     * @return `false`.
     */

    bool isIntradayBackTester() const
    {
      return false;
    }

  protected:
  };

  //
  // class MonthlyBackTester
  //

  template <class Decimal> class MonthlyBackTester : public BackTester<Decimal>
  {
  public:
    explicit MonthlyBackTester(boost::gregorian::date startDate,
			     boost::gregorian::date endDate)
      : BackTester<Decimal>()
    {
      DateRange r(first_of_month(startDate), first_of_month(endDate));
      this->addDateRange(r);
    }

    MonthlyBackTester() :
      BackTester<Decimal>()
    {}

    ~MonthlyBackTester()
    {}

    MonthlyBackTester(const MonthlyBackTester<Decimal> &rhs)
      :  BackTester<Decimal>(rhs)
    {}

    MonthlyBackTester<Decimal>&
    operator=(const MonthlyBackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Decimal>::operator=(rhs);

      return *this;
    }

    std::shared_ptr<BackTester<Decimal>> clone() const
    {
      auto back = std::make_shared<MonthlyBackTester<Decimal>>();
      auto it = this->beginBacktestDateRange();

      for (; it != this->endBacktestDateRange(); it++)
	back->addDateRange(it->second);

      return back;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the daily time frame.
     * @return `false`
     */
    bool isDailyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the weekly time frame.
     * @return `false`.
     */
    bool isWeeklyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the monthly time frame.
     * @return `true`.
     */
    bool isMonthlyBackTester() const
    {
      return true;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on intraday time frames.
     * @return `false`.
     */

    bool isIntradayBackTester() const
    {
      return false;
    }

  protected:
  };

  // Weekly

  //
  // class WeeklyBackTester
  //

  template <class Decimal> class WeeklyBackTester : public BackTester<Decimal>
  {
  public:
    explicit WeeklyBackTester(boost::gregorian::date startDate, 
			     boost::gregorian::date endDate)
      : BackTester<Decimal>()
    {
      DateRange r(first_of_week(startDate), first_of_week(endDate));
      this->addDateRange(r);
    }

    WeeklyBackTester() :
      BackTester<Decimal>()
    {}

    ~WeeklyBackTester()
    {}

    WeeklyBackTester(const WeeklyBackTester<Decimal> &rhs)
      :  BackTester<Decimal>(rhs)
    {}

    WeeklyBackTester<Decimal>& 
    operator=(const WeeklyBackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Decimal>::operator=(rhs);
      return *this;
    }

    std::shared_ptr<BackTester<Decimal>> clone() const
    {
      auto back = std::make_shared<WeeklyBackTester<Decimal>>();
      auto it = this->beginBacktestDateRange();

      for (; it != this->endBacktestDateRange(); it++)
	back->addDateRange(it->second);

      return back;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the daily time frame.
     * @return `false`
     */
    bool isDailyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the weekly time frame.
     * @return `true`.
     */
    bool isWeeklyBackTester() const
    {
      return true;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the monthly time frame.
     * @return `false`.
     */
    bool isMonthlyBackTester() const
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on intraday time frames.
     * @return `false`.
     */
    bool isIntradayBackTester() const
    {
      return false;
    }

  protected:
  };

  //
  // class IntradayBackTester - Full implementation for intraday backtesting
  //

  template <class Decimal>
  class IntradayBackTester : public BackTester<Decimal>
  {
  private:
    
  public:
    /**
     * @brief Primary Constructor: Creates a backtester for a precise ptime range.
     * @param startDateTime The exact start timestamp of the backtest range.
     * @param endDateTime The exact end timestamp of the backtest range.
     */
    IntradayBackTester(const boost::posix_time::ptime& startDateTime,
                       const boost::posix_time::ptime& endDateTime)
      : BackTester<Decimal>()
    {
      this->addDateRange(DateRange(startDateTime, endDateTime));
    }
    
    /**
     * @brief Default Constructor: Creates an empty intraday backtester.
     *        Date ranges must be added via addDateRange() before use.
     */
    IntradayBackTester()
      : BackTester<Decimal>()
    {}

    ~IntradayBackTester()
    {}

    IntradayBackTester(const IntradayBackTester<Decimal> &rhs)
      :  BackTester<Decimal>(rhs)
    {}

    IntradayBackTester<Decimal>&
    operator=(const IntradayBackTester<Decimal> &rhs)
    {
      if (this == &rhs)
        return *this;

      BackTester<Decimal>::operator=(rhs);
      
      return *this;
    }

    /**
     * @brief Clone the IntradayBackTester with configuration, but without strategies.
     */
    std::shared_ptr<BackTester<Decimal>> clone() const override
    {
      if (this->numBackTestRanges() == 0)
	throw BackTesterException("Cannot clone IntradayBackTester with no date ranges");

      auto back = std::make_shared<IntradayBackTester<Decimal>>();

      for (auto it = this->beginBacktestDateRange(); it != this->endBacktestDateRange(); ++it)
	back->addDateRange(it->second);

      return back;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the daily time frame.
     * @return `false`
     */
    bool isDailyBackTester() const override
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the weekly time frame.
     * @return `false`.
     */
    bool isWeeklyBackTester() const override
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on the monthly time frame.
     * @return `false`.
     */
    bool isMonthlyBackTester() const override
    {
      return false;
    }

    /**
     * @brief Determines whether this is a backtester that operates
     * on intraday time frames.
     * @return `true`.
     */
    bool isIntradayBackTester() const override
    {
      return true;
    }
  };

  template <class Decimal>
  class BackTesterFactory
  {
  public:
    /**
     * @brief Create a backtester of the specified timeframe over the given date range.
     *        Supports DAILY, WEEKLY, MONTHLY, and INTRADAY using a unified API.
     * @param theTimeFrame Timeframe duration enum.
     * @param backtestingDates DateRange defining start/end (date or ptime as needed).
     * @return Shared pointer to a BackTester specialized for the timeframe.
     * @throws BackTesterException if timeframe is unsupported.
     */
    static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
							      const DateRange& backtestingDates)
    {
      switch (theTimeFrame)
        {
        case TimeFrame::DAILY:
	  return std::make_shared<DailyBackTester<Decimal>>(backtestingDates.getFirstDate(),
							    backtestingDates.getLastDate());

        case TimeFrame::WEEKLY:
	  return std::make_shared<WeeklyBackTester<Decimal>>(backtestingDates.getFirstDate(),
							     backtestingDates.getLastDate());

        case TimeFrame::MONTHLY:
	  return std::make_shared<MonthlyBackTester<Decimal>>(backtestingDates.getFirstDate(),
							      backtestingDates.getLastDate());

        case TimeFrame::INTRADAY:
	  return std::make_shared<IntradayBackTester<Decimal>>(backtestingDates.getFirstDateTime(),
							       backtestingDates.getLastDateTime());

        default:
	  throw BackTesterException("BackTesterFactory::getBackTester - unsupported timeframe");
        }
    }

    /**
     * @brief Create a backtester using date-only bounds.
     *        Internally wraps dates into a DateRange and dispatches to getBackTester(..., DateRange).
     * @param theTimeFrame Timeframe duration enum.
     * @param startDate   Start date (midnight) for backtest.
     * @param endDate     End date (midnight) for backtest.
     * @return Shared pointer to a BackTester for DAILY, WEEKLY, or MONTHLY timeframe.
     * @throws BackTesterException if timeframe is INTRADAY or unsupported.
     */
    static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
							      boost::gregorian::date startDate,
							      boost::gregorian::date endDate)
    {
      if (theTimeFrame == TimeFrame::INTRADAY)
        {
	  throw BackTesterException(
				    "BackTesterFactory::getBackTester(date) - INTRADAY timeframe requires ptime bounds");
        }
      return getBackTester(theTimeFrame, DateRange(startDate, endDate));
    }

    /**
     * @brief Create an INTRADAY backtester using full ptime bounds.
     *        Only valid for INTRADAY timeframe; throws for non-INTRADAY.
     * @param theTimeFrame Timeframe duration enum; must be INTRADAY.
     * @param startDateTime Exact start timestamp for intraday backtest.
     * @param endDateTime   Exact end timestamp for intraday backtest.
     * @return Shared pointer to an IntradayBackTester.
     * @throws BackTesterException if timeframe is not INTRADAY.
     */
    static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
							      boost::posix_time::ptime startDateTime,
							      boost::posix_time::ptime endDateTime)
    {
      if (theTimeFrame != TimeFrame::INTRADAY)
        {
	  throw BackTesterException(
				    "BackTesterFactory::getBackTester(ptime) - non-INTRADAY timeframe cannot use ptime bounds");
        }
      return getBackTester(theTimeFrame, DateRange(startDateTime, endDateTime));
    }

    static std::shared_ptr<BackTester<Decimal>>
    backTestStrategy(const std::shared_ptr<BacktesterStrategy<Decimal>>& aStrategy,
		     TimeFrame::Duration theTimeFrame,
		     const DateRange& backtestingDates)
    {
      auto backtester = getBackTester(theTimeFrame, backtestingDates);
      backtester->addStrategy(aStrategy);
      backtester->backtest();
      return backtester;
    }
    
    /**
     * @brief Convenience: retrieve total closed trades from the first strategy.
     * @param aBackTester Shared pointer to an initialized BackTester.
     * @return Number of closed trades recorded.
     */
    static uint32_t getNumClosedTrades(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      auto strat = *(aBackTester->beginStrategies());
      return strat->getStrategyBroker().getClosedTrades();
    }
  };
}
#endif
