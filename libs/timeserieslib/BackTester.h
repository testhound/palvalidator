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
    using BacktestDateRangeIterator = typename DateRangeContainer::DateRangeIterator;

    explicit BackTester()
      : mStrategyList(),
	mStrategyRawList(),
	mBackTestDates(),
	mDates()
    {}

    virtual ~BackTester()
    {}

    BackTester(const BackTester& rhs)
      : mStrategyList(rhs.mStrategyList),
	mBackTestDates(rhs.mBackTestDates),
	mDates(rhs.mDates)
    {
      rebuildStrategyRawList();
    }

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

    virtual std::shared_ptr<BackTester<Decimal>> clone() const = 0;

    void addStrategy(const std::shared_ptr<BacktesterStrategy<Decimal>>& aStrategy)
    {
      mStrategyList.push_back(aStrategy);
      mStrategyRawList.push_back(aStrategy.get());
    }

    void addDateRange(const DateRange& range)
    {
      mBackTestDates.addDateRange(range);
    }

    StrategyIterator beginStrategies() const
    {
      return mStrategyList.begin();
    }

    StrategyIterator endStrategies() const
    {
      return mStrategyList.end();
    }

    StrategyRawIterator beginStrategiesRaw() const
    {
      return mStrategyRawList.begin();
    }

    StrategyRawIterator endStrategiesRaw() const
    {
      return mStrategyRawList.end();
    }

    BacktestDateRangeIterator beginBacktestDateRange() const
    {
      return mBackTestDates.beginDateRange();
    }

    BacktestDateRangeIterator endBacktestDateRange() const
    {
      return mBackTestDates.endDateRange();
    }

    unsigned long numBackTestRanges() const
    {
      return mBackTestDates.getNumEntries();
    }

    const ClosedPositionHistory<Decimal>& getClosedPositionHistory() const
    {
      if (mStrategyList.empty())
	{
	  throw BackTesterException("getClosedPositionHistory: No strategies added");
	}
      return mStrategyList.front()->getStrategyBroker().getClosedPositionHistory();
    }

    uint32_t getNumStrategies() const
    {
      return static_cast<uint32_t>(mStrategyList.size());
    }

    date getStartDate() const
    {
      return mBackTestDates.getFirstDateRange().getFirstDate();
    }

    date getEndDate() const
    {
      return mBackTestDates.getFirstDateRange().getLastDate();
    }

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
			  processStrategyBar(secPtr, strat, orderDate);
			}

		      strat->eventProcessPendingOrders(current);
		    }
		}
	    }
	}
    }

  protected:
    virtual TimeSeriesDate previous_period(const TimeSeriesDate& d) const = 0;
    virtual TimeSeriesDate next_period(const TimeSeriesDate& d) const = 0;

  private:
    void rebuildStrategyRawList()
    {
      mStrategyRawList.clear();
      mStrategyRawList.reserve(mStrategyList.size());
      for (const auto& sp : mStrategyList)
	{
	  mStrategyRawList.push_back(sp.get());
	}
    }
    
    void processStrategyBar(
			    const std::shared_ptr<Security<Decimal>>& security,
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
