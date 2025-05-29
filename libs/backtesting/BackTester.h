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
#include "number.h"
#include "BoostDateHelper.h"
#include "BacktesterStrategy.h"


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
	mBackTestDates(),
	mDates()
    {}

    virtual ~BackTester()
    {}

    /**
     * @brief Copy constructor; clones strategy list and date ranges.
     * @param rhs Other BackTester to copy state from.
     */
    BackTester(const BackTester& rhs)
      : mStrategyList(rhs.mStrategyList),
	mBackTestDates(rhs.mBackTestDates),
	mDates(rhs.mDates)
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
	  mDates = rhs.mDates;
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
    std::vector<Decimal> getAllHighResReturns(StrategyPtr strat) const
    {
      // 1) closed trades
      const auto& closedHist = strat->getStrategyBroker()
                                 .getClosedPositionHistory();
      std::vector<Decimal> allReturns = closedHist.getHighResBarReturns();

      // 2) any open positions
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
	      if (std::distance(begin, end) < 2) 
                continue;

	      auto prev = begin;
	      for (auto curr = std::next(begin); curr != end; ++curr)
		{
		  Decimal c0 = prev->second.getCloseValue();
		  Decimal c1 = curr->second.getCloseValue();
		  allReturns.push_back((c1 - c0) / c0);
		  prev = curr;
		}
	    }
	}

      return allReturns;
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
     * @brief Latest date used across all backtest ranges.
     * @return End date of backtest.
     */
    date getEndDate() const
    {
      return mBackTestDates.getFirstDateRange().getLastDate();
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

      bool multipleRanges   = numBackTestRanges() > 1;
      unsigned int backtestNumber = 0;

      // ─── Outer loop over each DateRange ────────────────────────────────
      for (auto itRange = beginBacktestDateRange();
	   itRange != endBacktestDateRange();
	   ++itRange)
	{
	  // 1) Build the per-range date vector
	  mDates.clear();
	  auto rangeStart = itRange->second.getFirstDate();
	  auto rangeEnd   = itRange->second.getLastDate();

	  // include the first date, then step via next_period()
	  for (auto d = rangeStart; ; d = next_period(d))
	    {
	      mDates.push_back(d);
	      if (d == rangeEnd) break;
	    }

	  // 2) Compute the “last bar” for this range
	  auto barBeforeBackTesterEndDate = previous_period(rangeEnd);
	  ++backtestNumber;

	  // ─── Inner loop over days via index ───────────────────────────
	  for (size_t idx = 1; idx < mDates.size(); ++idx)
	    {
	      const date& current   = mDates[idx];
	      const date& orderDate = mDates[idx - 1];

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
			  && current == barBeforeBackTesterEndDate
			  && backtestNumber < numBackTestRanges())
			{
			  closeAllPositions(orderDate);
			}
		      else
			{
			  processStrategyBar(secPtr.get(), strat, orderDate);
			}

		      strat->eventProcessPendingOrders(current);
		    }
		}
	    }
	}
    }

  protected:
    /**
     * @brief Return the previous time period (e.g., prior trading day).
     * @param d Current date.
     * @return Previous period date.
     * @note Implemented by derived classes (Daily/Weekly/Monthly).
     */
    virtual TimeSeriesDate previous_period(const TimeSeriesDate& d) const = 0;

    /**
     * @brief Return the next time period (e.g., next trading day).
     * @param d Current date.
     * @return Next period date.
     * @note Implemented by derived classes.
     */
    virtual TimeSeriesDate next_period(const TimeSeriesDate& d) const = 0;

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
			    const date& processingDate)
    {
      if (!strategy->doesSecurityHaveTradingData(*security, processingDate))
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
				    processingDate);
	}
      strategy->eventEntryOrders(
				 security,
				 strategy->getInstrumentPosition(symbol),
				 processingDate);
    }

    void closeAllPositions(const TimeSeriesDate& orderDate)
    {
      for (auto itStrat = beginStrategiesRaw(); itStrat != endStrategiesRaw(); ++itStrat)
	{
	  StrategyPtr strategy = *itStrat;
	  for (auto itPort = strategy->beginPortfolio(); itPort != strategy->endPortfolio(); ++itPort)
	    {
	      const auto& securityPtr = itPort->second;
	      const auto symbol = securityPtr->getSymbol();
	      strategy->eventUpdateSecurityBarNumber(symbol);
	      strategy->ExitAllPositions(symbol, orderDate);
	    }
	}
    }

  private:
    std::list<std::shared_ptr<BacktesterStrategy<Decimal>>> mStrategyList;
    std::vector<StrategyPtr> mStrategyRawList;
    DateRangeContainer mBackTestDates;
    std::vector<boost::gregorian::date> mDates;
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
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const
      {
	return boost_previous_weekday(d);
      }

    TimeSeriesDate next_period(const TimeSeriesDate& d) const
      {
	return boost_next_weekday(d);
      }
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
     * @return `true`.
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
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const
      {
	return boost_previous_month(d);
      }

    TimeSeriesDate next_period(const TimeSeriesDate& d) const
      {
	return boost_next_month(d);
      }
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
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const
      {
	return boost_previous_week(d);
      }

    TimeSeriesDate next_period(const TimeSeriesDate& d) const
      {
	return boost_next_week(d);
      }
  };

  template <class Decimal>
  class BackTesterFactory
    {
    public:
      static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
								const DateRange& backtestingDates)
      {
	if (theTimeFrame == TimeFrame::DAILY)
	  return std::make_shared<DailyBackTester<Decimal>>(backtestingDates.getFirstDate(),
						  backtestingDates.getLastDate());
	else if (theTimeFrame == TimeFrame::WEEKLY)
	  return std::make_shared<WeeklyBackTester<Decimal>>(backtestingDates.getFirstDate(),
						   backtestingDates.getLastDate());
	else if (theTimeFrame == TimeFrame::MONTHLY)
	  return std::make_shared<MonthlyBackTester<Decimal>>(backtestingDates.getFirstDate(),
						    backtestingDates.getLastDate());
	else
	  throw BackTesterException("BackTesterFactory::getBacktester - cannot create backtester for time frame other than daily, weekly or monthly");
      }

      static std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
								boost::gregorian::date startDate,
								boost::gregorian::date endDate)
      {
	return BackTesterFactory<Decimal>::getBackTester(theTimeFrame, DateRange(startDate, endDate));
      }

      static uint32_t
      getNumClosedTrades(std::shared_ptr<BackTester<Decimal>> aBackTester)
      {
	std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
	  (*(aBackTester->beginStrategies()));

	return backTesterStrategy->getStrategyBroker().getClosedTrades();
      }

    };
}




#endif
