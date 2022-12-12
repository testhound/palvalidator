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
  public:
    typedef typename std::list<std::shared_ptr<BacktesterStrategy<Decimal>>>::const_iterator StrategyIterator;

    explicit BackTester()
      : mStrategyList()
    {
    }

    virtual ~BackTester()
    {}

    BackTester(const BackTester<Decimal> &rhs)
      : mStrategyList(rhs.mStrategyList)
    {}

    BackTester<Decimal>& 
    operator=(const BackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mStrategyList = rhs.mStrategyList;
      return *this;
    }

    virtual std::shared_ptr<BackTester<Decimal>> clone() const = 0;

    void addStrategy (std::shared_ptr<BacktesterStrategy<Decimal>> aStrategy)
    {
      mStrategyList.push_back(aStrategy);
    }

    StrategyIterator beginStrategies() const
    {
      return mStrategyList.begin();
    }

    StrategyIterator endStrategies() const
    {
      return mStrategyList.end();
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

    virtual const boost::gregorian::date getStartDate() const = 0;
    virtual const boost::gregorian::date getEndDate() const = 0;
    virtual void backtest() = 0;

  protected:
    virtual TimeSeriesDate previous_period(const TimeSeriesDate& d) const = 0;
    virtual TimeSeriesDate next_period(const TimeSeriesDate& d) const = 0;

  private:
    std::list<std::shared_ptr<BacktesterStrategy<Decimal>>> mStrategyList;
  };


  //
  // class DailyBackTester
  //

  template <class Decimal> class DailyBackTester : public BackTester<Decimal>
  {
  public:
    explicit DailyBackTester(boost::gregorian::date startDate, 
			     boost::gregorian::date endDate)
      : BackTester<Decimal>(),
      mBackTestDates()
    {
      if (isWeekend (startDate))
	startDate = boost_next_weekday (startDate);

      if (isWeekend (endDate))
	endDate = boost_previous_weekday (endDate);

      DateRange r(startDate, endDate);
      mBackTestDates.addDateRange(r);
    }

    DailyBackTester() :
      BackTester<Decimal>(),
      mBackTestDates()
    {}

    ~DailyBackTester()
    {}

    DailyBackTester(const DailyBackTester<Decimal> &rhs)
      :  BackTester<Decimal>(rhs),
      mBackTestDates(rhs.mBackTestDates)
    {}

    DailyBackTester<Decimal>& 
    operator=(const DailyBackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Decimal>::operator=(rhs);

      mBackTestDates = rhs.mBackTestDates;

      return *this;
    }

    void addDateRange(const DateRange& range)
    {
      mBackTestDates.addDateRange(range);
    }

    std::shared_ptr<BackTester<Decimal>> clone() const
    {
      auto back = std::make_shared<DailyBackTester<Decimal>>();
      auto it = mBackTestDates.beginDateRange();
      for (; it != mBackTestDates.endDateRange(); it++)
	back->addDateRange(it->second);

      return back;
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
      typename BacktesterStrategy<Decimal>::PortfolioIterator iteratorPortfolio;
      
      if (this->getNumStrategies() == 0)
	throw BackTesterException("No strategies have been added to backtest");

      boost::gregorian::date backTesterDate(next_period (getStartDate()));
      boost::gregorian::date orderDate;

      for (; backTesterDate <= getEndDate(); backTesterDate = next_period(backTesterDate))
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
		  
		  
		  processStrategyBar (aSecurity, aStrategy, orderDate);
		  aStrategy->eventProcessPendingOrders (backTesterDate);
		}
	    }
	}
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

  private:
    DateRangeContainer mBackTestDates;
  };

  //
  // class MonthlyBackTester
  //

  template <class Decimal> class MonthlyBackTester : public BackTester<Decimal>
  {
  public:
    explicit MonthlyBackTester(boost::gregorian::date startDate, 
			     boost::gregorian::date endDate)
      : BackTester<Decimal>(),
      mStartDate (first_of_month (startDate)),
      mEndDate (first_of_month (endDate))
    {}

    ~MonthlyBackTester()
    {}

    MonthlyBackTester(const MonthlyBackTester<Decimal> &rhs)
      :  BackTester<Decimal>(rhs),
	 mStartDate(rhs.mStartDate),
	 mEndDate (rhs.mEndDate)
    {}

    MonthlyBackTester<Decimal>& 
    operator=(const MonthlyBackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Decimal>::operator=(rhs);

      mStartDate = rhs.mStartDate;
      mEndDate = rhs.mEndDate;

      return *this;
    }

    std::shared_ptr<BackTester<Decimal>> clone() const
    {
      return std::make_shared<MonthlyBackTester<Decimal>>(getStartDate(),
						     getEndDate());
    }

    const boost::gregorian::date getStartDate() const
    {
      return mStartDate;
    }

    const boost::gregorian::date getEndDate() const
    {
      return mEndDate;
    }

    void backtest()
    {
      typename BackTester<Decimal>::StrategyIterator itStrategy;
      typename BacktesterStrategy<Decimal>::PortfolioIterator iteratorPortfolio;
      
      if (this->getNumStrategies() == 0)
	throw BackTesterException("No strategies have been added to backtest");

      boost::gregorian::date backTesterDate(boost_next_month (mStartDate));
      boost::gregorian::date orderDate;

      for (; backTesterDate <= mEndDate;  backTesterDate = boost_next_month(backTesterDate))
	{
	  orderDate = boost_previous_month (backTesterDate);

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
		  
		  
		  processStrategyBar (aSecurity, aStrategy, orderDate);
		  aStrategy->eventProcessPendingOrders (backTesterDate);
		}
	    }
	}
    }

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

  protected:
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const
      {
	return boost_previous_month(d);
      }

    TimeSeriesDate next_period(const TimeSeriesDate& d) const
      {
	return boost_next_month(d);
      }

  private:
    boost::gregorian::date mStartDate;
    boost::gregorian::date mEndDate;
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
      : BackTester<Decimal>(),
      mStartDate (first_of_week (startDate)),
      mEndDate (first_of_week (endDate))
    {}

    ~WeeklyBackTester()
    {}

    WeeklyBackTester(const WeeklyBackTester<Decimal> &rhs)
      :  BackTester<Decimal>(rhs),
	 mStartDate(rhs.mStartDate),
	 mEndDate (rhs.mEndDate)
    {}

    WeeklyBackTester<Decimal>& 
    operator=(const WeeklyBackTester<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Decimal>::operator=(rhs);

      mStartDate = rhs.mStartDate;
      mEndDate = rhs.mEndDate;

      return *this;
    }

    std::shared_ptr<BackTester<Decimal>> clone() const
    {
      return std::make_shared<WeeklyBackTester<Decimal>>(getStartDate(),
						     getEndDate());
    }

    const boost::gregorian::date getStartDate() const
    {
      return mStartDate;
    }

    const boost::gregorian::date getEndDate() const
    {
      return mEndDate;
    }

    void backtest()
    {
      typename BackTester<Decimal>::StrategyIterator itStrategy;
      typename BacktesterStrategy<Decimal>::PortfolioIterator iteratorPortfolio;
      
      if (this->getNumStrategies() == 0)
	throw BackTesterException("No strategies have been added to backtest");

      boost::gregorian::date backTesterDate(boost_next_week (mStartDate));
      boost::gregorian::date orderDate;

      for (; backTesterDate <= mEndDate;  backTesterDate = boost_next_week(backTesterDate))
	{
	  orderDate = boost_previous_week (backTesterDate);

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
		  
		  
		  processStrategyBar (aSecurity, aStrategy, orderDate);
		  aStrategy->eventProcessPendingOrders (backTesterDate);
		}
	    }
	}
    }

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

  protected:
    TimeSeriesDate previous_period(const TimeSeriesDate& d) const
      {
	return boost_previous_week(d);
      }

    TimeSeriesDate next_period(const TimeSeriesDate& d) const
      {
	return boost_next_week(d);
      }

  private:
    boost::gregorian::date mStartDate;
    boost::gregorian::date mEndDate;
  };
}

#endif
