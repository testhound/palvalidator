// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __BACKTESTER_H
#define __BACKTESTER_H 1

#include <exception>
#include <list>
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

  template <class Decimal> class BackTester
  {
    using Map = map<boost::gregorian::date, DateRange>;
  public:
    typedef typename std::list<std::shared_ptr<BacktesterStrategy<Decimal>>>::const_iterator StrategyIterator;
    typedef typename DateRangeContainer::DateRangeIterator BacktestDateRangeIterator;

    explicit BackTester()
      : mStrategyList(),
      mBackTestDates()
    {
    }

    virtual ~BackTester()
    {}

    BackTester(const BackTester<Decimal> &rhs)
      : mStrategyList(rhs.mStrategyList),
      mBackTestDates(rhs.mBackTestDates)
    {}

    BackTester<Decimal>& 
    operator=(const BackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mStrategyList = rhs.mStrategyList;
      mBackTestDates = rhs.mBackTestDates;
      return *this;
    }

    virtual std::shared_ptr<BackTester<Decimal>> clone() const = 0;

    void addStrategy (std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy)
    {
      mStrategyList.push_back(aStrategy);
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

    const ClosedPositionHistory<Decimal>&
    getClosedPositionHistory() const
    {
      if (beginStrategies() == endStrategies())
	throw BackTesterException("BackTester::getClosedPositionHistory - No strategies have been added, so ClosedPositionHistory does not exist");

      return ((*beginStrategies())->getStrategyBroker().getClosedPositionHistory());
    }

    uint32_t getNumStrategies() const
    {
      return mStrategyList.size();
    }

    const boost::gregorian::date getStartDate() const
    {
      return mBackTestDates.getFirstDateRange().getFirstDate();
    }

    const boost::gregorian::date getEndDate() const
    {
      return mBackTestDates.getFirstDateRange().getLastDate();
    }

    void backtest()
    {
      typename BackTester<Decimal>::StrategyIterator itStrategy;
      typename BackTester<Decimal>::BacktestDateRangeIterator itDateRange;
      typename BacktesterStrategy<Decimal>::PortfolioIterator iteratorPortfolio;
 
      if (this->getNumStrategies() == 0)
	throw BackTesterException("No strategies have been added to backtest");

      boost::gregorian::date backTesterDate;
      boost::gregorian::date backTesterEndDate;
      boost::gregorian::date barBeforeBackTesterEndDate;
      boost::gregorian::date orderDate;
      bool multipleBacktestDates = this->numBackTestRanges() > 1;
      unsigned int backtestNumber = 0;

      for (itDateRange = this->beginBacktestDateRange(); itDateRange != endBacktestDateRange(); itDateRange++)
	{
	  backTesterDate = next_period (itDateRange->second.getFirstDate());
	  backTesterEndDate = itDateRange->second.getLastDate();
	  barBeforeBackTesterEndDate = previous_period(backTesterEndDate);

	  backtestNumber++;
	  for (; backTesterDate <= backTesterEndDate; backTesterDate = next_period(backTesterDate))
	    {
	      orderDate = previous_period (backTesterDate);

	      //std::cout << "Iterating over strategies" << std::endl;

	      for (itStrategy = this->beginStrategies(); itStrategy != this->endStrategies();
		   itStrategy++)
		{
		  auto aStrategy = (*itStrategy);
		  //std::cout << "Iterating over portfolio in strategy " << aStrategy->getStrategyName() << std::endl;

		  for (iteratorPortfolio = aStrategy->beginPortfolio();
		       iteratorPortfolio != aStrategy->endPortfolio();
		       iteratorPortfolio++)
		    {
		      auto aSecurity = iteratorPortfolio->second;
		  
		      // If there is more than one date range and this is not the last date range, close
		      // all positions.

		      if (multipleBacktestDates && (backTesterDate == barBeforeBackTesterEndDate) &&
			  (backtestNumber < this->numBackTestRanges()))
			  closeAllPositions(previous_period (backTesterDate));
		      else
			processStrategyBar (aSecurity, aStrategy, orderDate);

		      aStrategy->eventProcessPendingOrders (backTesterDate);
		    }
		}
	    }

	}
    }

      
  protected:
    virtual TimeSeriesDate previous_period(const TimeSeriesDate& d) const = 0;
    virtual TimeSeriesDate next_period(const TimeSeriesDate& d) const = 0;

private:
    void processStrategyBar (std::shared_ptr<Security<Decimal>> aSecurity,
			     std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy,
			     const date& processingDate)
    {
      if (aStrategy->doesSecurityHaveTradingData (*aSecurity, processingDate))
	  {
	    std::string theSymbol = aSecurity->getSymbol(); 
	    aStrategy->eventUpdateSecurityBarNumber(theSymbol);

	    if (!aStrategy->isFlatPosition (theSymbol))
	      aStrategy->eventExitOrders (aSecurity, 
					  aStrategy->getInstrumentPosition(theSymbol),
					  processingDate);
	    aStrategy->eventEntryOrders(aSecurity, 
					aStrategy->getInstrumentPosition(theSymbol),
					processingDate);
	    
	  }
    }

    void closeAllPositions(const TimeSeriesDate& orderDate)
      {
	//std::cout << "BackTester::closeAllPositions: close all positions as of " << orderDate << std::endl;
	typename BackTester<Decimal>::StrategyIterator itStrategy;
	typename BacktesterStrategy<Decimal>::PortfolioIterator iteratorPortfolio;

	for (itStrategy = this->beginStrategies(); itStrategy != this->endStrategies();
	     itStrategy++)
	  {
	    auto aStrategy = (*itStrategy);

	    for (iteratorPortfolio = aStrategy->beginPortfolio();
		 iteratorPortfolio != aStrategy->endPortfolio();
		 iteratorPortfolio++)
	      {
		auto aSecurity = iteratorPortfolio->second;
		std::string theSymbol = aSecurity->getSymbol();

		aStrategy->eventUpdateSecurityBarNumber(theSymbol);
		aStrategy->ExitAllPositions(theSymbol, orderDate);
	      }
	  }
      }

  private:
    std::list<std::shared_ptr<BacktesterStrategy<Decimal>>> mStrategyList;
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
}

#endif
