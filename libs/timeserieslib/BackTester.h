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
#include "decimal.h"
#include "BoostDateHelper.h"
#include "BacktesterStrategy.h"


namespace mkc_timeseries
{
  using dec::decimal;
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

  template <int Prec> class BackTester
  {
  public:
    typedef typename std::list<std::shared_ptr<BacktesterStrategy<Prec>>>::const_iterator StrategyIterator;

    explicit BackTester()
      : mStrategyList()
    {
    }

    virtual ~BackTester()
    {}

    BackTester(const BackTester<Prec> &rhs)
      : mStrategyList(rhs.mStrategyList)
    {}

    BackTester<Prec>& 
    operator=(const BackTester<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      mStrategyList = rhs.mStrategyList;
      return *this;
    }

    virtual std::shared_ptr<BackTester<Prec>> clone() const = 0;

    void addStrategy (std::shared_ptr<BacktesterStrategy<Prec>> aStrategy)
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

    const ClosedPositionHistory<Prec>&
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

    virtual const boost::gregorian::date& getStartDate() const = 0;
    virtual const boost::gregorian::date& getEndDate() const = 0;
    virtual void backtest() = 0;

  private:
    std::list<std::shared_ptr<BacktesterStrategy<Prec>>> mStrategyList;
  };


  //
  // class DailyBackTester
  //

  template <int Prec> class DailyBackTester : public BackTester<Prec>
  {
  public:
    explicit DailyBackTester(boost::gregorian::date startDate, 
			     boost::gregorian::date endDate)
      : BackTester<Prec>(),
	mStartDate (startDate),
	mEndDate (endDate)
    {
      if (isWeekend (startDate))
	startDate = boost_next_weekday (startDate);

      if (isWeekend (endDate))
	endDate = boost_previous_weekday (endDate);
    }

    ~DailyBackTester()
    {}

    DailyBackTester(const DailyBackTester<Prec> &rhs)
      :  BackTester<Prec>(rhs),
	 mStartDate(rhs.mStartDate),
	 mEndDate (rhs.mEndDate)
    {}

    DailyBackTester<Prec>& 
    operator=(const DailyBackTester<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Prec>::operator=(rhs);

      mStartDate = rhs.mStartDate;
      mEndDate = rhs.mEndDate;

      return *this;
    }

    std::shared_ptr<BackTester<Prec>> clone() const
    {
      return std::make_shared<DailyBackTester<Prec>>(getStartDate(),
						     getEndDate());
    }

    const boost::gregorian::date& getStartDate() const
    {
      return mStartDate;
    }

    const boost::gregorian::date& getEndDate() const
    {
      return mEndDate;
    }

    void backtest()
    {
      typename BackTester<Prec>::StrategyIterator itStrategy;
      typename BacktesterStrategy<Prec>::PortfolioIterator iteratorPortfolio;
      
      if (this->getNumStrategies() == 0)
	throw BackTesterException("No strategies have been added to backtest");

      boost::gregorian::date backTesterDate(boost_next_weekday (mStartDate));
      boost::gregorian::date orderDate;

      for (; backTesterDate <= mEndDate;  backTesterDate = boost_next_weekday(backTesterDate))
	{
	  orderDate = boost_previous_weekday (backTesterDate);

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
    void processStrategyBar (std::shared_ptr<Security<Prec>> aSecurity,
			     std::shared_ptr<BacktesterStrategy<Prec>> aStrategy,
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
    boost::gregorian::date mStartDate;
    boost::gregorian::date mEndDate;
  };

  //
  // class MonthlyBackTester
  //

  template <int Prec> class MonthlyBackTester : public BackTester<Prec>
  {
  public:
    explicit MonthlyBackTester(boost::gregorian::date startDate, 
			     boost::gregorian::date endDate)
      : BackTester<Prec>(),
      mStartDate (first_of_month (startDate)),
      mEndDate (first_of_month (endDate))
    {}

    ~MonthlyBackTester()
    {}

    MonthlyBackTester(const MonthlyBackTester<Prec> &rhs)
      :  BackTester<Prec>(rhs),
	 mStartDate(rhs.mStartDate),
	 mEndDate (rhs.mEndDate)
    {}

    MonthlyBackTester<Prec>& 
    operator=(const MonthlyBackTester<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Prec>::operator=(rhs);

      mStartDate = rhs.mStartDate;
      mEndDate = rhs.mEndDate;

      return *this;
    }

    std::shared_ptr<BackTester<Prec>> clone() const
    {
      return std::make_shared<MonthlyBackTester<Prec>>(getStartDate(),
						     getEndDate());
    }

    const boost::gregorian::date& getStartDate() const
    {
      return mStartDate;
    }

    const boost::gregorian::date& getEndDate() const
    {
      return mEndDate;
    }

    void backtest()
    {
      typename BackTester<Prec>::StrategyIterator itStrategy;
      typename BacktesterStrategy<Prec>::PortfolioIterator iteratorPortfolio;
      
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
    void processStrategyBar (std::shared_ptr<Security<Prec>> aSecurity,
			     std::shared_ptr<BacktesterStrategy<Prec>> aStrategy,
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
    boost::gregorian::date mStartDate;
    boost::gregorian::date mEndDate;
  };

  // Weekly

  //
  // class WeeklyBackTester
  //

  template <int Prec> class WeeklyBackTester : public BackTester<Prec>
  {
  public:
    explicit WeeklyBackTester(boost::gregorian::date startDate, 
			     boost::gregorian::date endDate)
      : BackTester<Prec>(),
      mStartDate (first_of_week (startDate)),
      mEndDate (first_of_week (endDate))
    {}

    ~WeeklyBackTester()
    {}

    WeeklyBackTester(const WeeklyBackTester<Prec> &rhs)
      :  BackTester<Prec>(rhs),
	 mStartDate(rhs.mStartDate),
	 mEndDate (rhs.mEndDate)
    {}

    WeeklyBackTester<Prec>& 
    operator=(const WeeklyBackTester<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      BackTester<Prec>::operator=(rhs);

      mStartDate = rhs.mStartDate;
      mEndDate = rhs.mEndDate;

      return *this;
    }

    std::shared_ptr<BackTester<Prec>> clone() const
    {
      return std::make_shared<WeeklyBackTester<Prec>>(getStartDate(),
						     getEndDate());
    }

    const boost::gregorian::date& getStartDate() const
    {
      return mStartDate;
    }

    const boost::gregorian::date& getEndDate() const
    {
      return mEndDate;
    }

    void backtest()
    {
      typename BackTester<Prec>::StrategyIterator itStrategy;
      typename BacktesterStrategy<Prec>::PortfolioIterator iteratorPortfolio;
      
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
    void processStrategyBar (std::shared_ptr<Security<Prec>> aSecurity,
			     std::shared_ptr<BacktesterStrategy<Prec>> aStrategy,
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
    boost::gregorian::date mStartDate;
    boost::gregorian::date mEndDate;
  };
}

#endif
