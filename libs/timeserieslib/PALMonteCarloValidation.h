// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PAL_MONTE_CARLO_VALIDATION_H
#define __PAL_MONTE_CARLO_VALIDATION_H 1

#include <string>
#include <list>
#include <sstream>
#include <boost/date_time.hpp>
#include "number.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "MonteCarloPermutationTest.h"
#include "McptConfigurationFileReader.h"
#include "PalAst.h"

#include "runner.hpp"

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using std::list;
  using std::shared_ptr;

  class PALMonteCarloValidationException : public std::runtime_error
  {
  public:
    PALMonteCarloValidationException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~PALMonteCarloValidationException()
      {}
  };

  template <class Decimal, typename McptType> class PALMonteCarloValidation
  {
  public:
    typedef typename list<shared_ptr<PalStrategy<Decimal>>>::const_iterator SurvivingStrategiesIterator;

  public:
    PALMonteCarloValidation(std::shared_ptr<McptConfiguration<Decimal>> configuration,
			    unsigned long numPermutations)
      : mMonteCarloConfiguration(configuration),
	mNumPermutations(numPermutations),
	mSurvivingStrategies()
    {}

    ~PALMonteCarloValidation()
    {}

    SurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mSurvivingStrategies.begin();
    }

    SurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mSurvivingStrategies.end();
    }

    unsigned long getNumSurvivingStrategies() const
    {
      return mSurvivingStrategies.size();
    }

/*  //original code: here for reference
    void runPermutationTests()
    {
      std::shared_ptr<Security<Decimal>> securityToTest = mMonteCarloConfiguration->getSecurity();

      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator = 
	patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Decimal>> longStrategy;
      std::shared_ptr<BackTester<Decimal>> theBackTester;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;
      Decimal pValue;

      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
	{
	  patternToTest = longPatternsIterator->second;
	  strategyName = longStrategyNameBase + std::to_string(strategyNumber);
	  longStrategy = std::make_shared<PalLongStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);
	  
	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());

	  theBackTester->addStrategy(longStrategy);

	  std::cout << "Running MCPT for strategy " << strategyNumber << std::endl;
	  // Run Monte Carlo Permutation Tests using the provided backtester
	  McptType mcpt(theBackTester, mNumPermutations);
	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Decimal>::SignificantPValue)
	    {
	      mSurvivingStrategies.push_back (longStrategy);
	      std::cout << "Long Pattern found with p-Value < " << pValue << std::endl;
	    }

	  strategyNumber++;

	}

      std::cout << std::endl << "MCPT Processing short patterns" << std::endl << std::endl;
      
      std::shared_ptr<PalShortStrategy<Decimal>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator = 
	patternsToTest->patternShortsBegin();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
	{
	  patternToTest = shortPatternsIterator->second;
	  strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
	  shortStrategy = std::make_shared<PalShortStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);
	  
	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());
	  theBackTester->addStrategy(shortStrategy);

	  std::cout << "Running MCPT for strategy " << strategyNumber << std::endl;
	  McptType mcpt(theBackTester, mNumPermutations);

	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Decimal>::SignificantPValue)
	    {
	      mSurvivingStrategies.push_back (shortStrategy);
	      std::cout << "Short Pattern found with p-Value < " << pValue << std::endl;
	    }
	  strategyNumber++;

	}
    }
*/
    void runPermutationTests()
    {
      std::shared_ptr<Security<Decimal>> securityToTest = mMonteCarloConfiguration->getSecurity();
      securityToTest->getTimeSeries()->syncronizeMapAndArray();
      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator =
        patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Decimal>> longStrategy;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;

      //build thread-pool-runner
      runner& Runner=runner::instance();
      std::vector<boost::unique_future<void>> resultsOrErrorsVector;

      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
      {

        patternToTest = longPatternsIterator->second;
        strategyName = longStrategyNameBase + std::to_string(strategyNumber);
        longStrategy = std::make_shared<PalLongStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);

        auto theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                                oosDates.getFirstDate(),
                                oosDates.getLastDate());

        theBackTester->addStrategy(longStrategy);
        //start paralel part
        resultsOrErrorsVector.emplace_back(Runner.post([ this
                                                       , strategyNumber
                                                       , theBackTester = std::move(theBackTester)
                                                       , longStrategy]() -> void {
            using namespace std::chrono;
            using clock = steady_clock;
            auto start = clock::now();

            Decimal pValue;

            {
                  std::stringstream s;
                  s << "Running MCPT for strategy " << strategyNumber <<' '<< std::endl;
                  std::cout<<s.str();
            }

            // Run Monte Carlo Permutation Tests using the provided backtester
            McptType mcpt(theBackTester, mNumPermutations);

            pValue = mcpt.runPermutationTest();

            auto end = clock::now();
            auto duration_ms = duration_cast<milliseconds>(end - start).count();

            {
                  std::stringstream s;
                  s << "Strategy " << strategyNumber << " took " << duration_ms << " ms to run" << std::endl;
                  std::cout<<s.str();
            }

            if (pValue < DecimalConstants<Decimal>::SignificantPValue)
              {
                boost::mutex::scoped_lock Lock(survivingStrategiesMutex);
                mSurvivingStrategies.push_back (longStrategy);
                std::cout <<"Strategy: "<<strategyNumber<< " Long Pattern found with p-Value < " << pValue << std::endl;
              }
        }));
        strategyNumber++;

      }
      for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
      {
          try{
              resultsOrErrorsVector[i].wait();
              resultsOrErrorsVector[i].get();
          }
          catch(std::exception const& e)
          {
              std::cerr<<"Strategy: "<<i<<" error: "<<e.what()<<std::endl;
          }
      }
    //end parallel part
      std::cout << std::endl << "MCPT Processing short patterns" << std::endl << std::endl;

      std::shared_ptr<PalShortStrategy<Decimal>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator =
    patternsToTest->patternShortsBegin();
      resultsOrErrorsVector.clear();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
      {
        patternToTest = shortPatternsIterator->second;
        strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
        shortStrategy = std::make_shared<PalShortStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);

        auto theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                               oosDates.getFirstDate(),
                               oosDates.getLastDate());
        theBackTester->addStrategy(shortStrategy);

        //sends code to the runner
        resultsOrErrorsVector.emplace_back(Runner.post([strategyNumber,theBackTester,this,shortStrategy](){
            Decimal pValue;
            std::stringstream s;
            s<<"Running MCPT for strategy " << strategyNumber << std::endl;
            std::cout<<s.str();
            //std::cout << "Running MCPT for strategy " << strategyNumber <<' '<< std::endl;
            McptType mcpt(theBackTester, mNumPermutations);

            pValue = mcpt.runPermutationTest();

            if (pValue < DecimalConstants<Decimal>::SignificantPValue)
              {
                boost::mutex::scoped_lock Lock(survivingStrategiesMutex);
                mSurvivingStrategies.push_back (shortStrategy);
                std::cout <<"Strategy: "<<strategyNumber<< " Short Pattern found with p-Value < " << pValue << std::endl;
              }
        }));

        strategyNumber++;

      }
      //collects exceptions from the runner and waits for the computation end to be signalled
      for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
      {
          try{
              resultsOrErrorsVector[i].get();
          }
          catch(std::exception const& e)
          {
              std::cerr<<"Strategy: "<<i<<" error: "<<e.what()<<std::endl;
          }
      }

    }
  private:
    std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
						    boost::gregorian::date startDate, 
						    boost::gregorian::date endDate) const
    {
      if (theTimeFrame == TimeFrame::DAILY)
	return std::make_shared<DailyBackTester<Decimal>>(startDate, endDate);
      else if (theTimeFrame == TimeFrame::WEEKLY)
	return std::make_shared<WeeklyBackTester<Decimal>>(startDate, endDate);
      else if (theTimeFrame == TimeFrame::MONTHLY)
	return std::make_shared<MonthlyBackTester<Decimal>>(startDate, endDate);
      else
	throw PALMonteCarloValidationException("PALMonteCarloValidation::getBackTester - Only daily and monthly time frame supported at present.");
    }

  private:
    std::shared_ptr<McptConfiguration<Decimal>> mMonteCarloConfiguration;
    unsigned long mNumPermutations;
    boost::mutex  survivingStrategiesMutex;
    std::list<std::shared_ptr<PalStrategy<Decimal>>> mSurvivingStrategies;
  };

  /////////////////////////
  // class PALMCPTtValidation
  //
  // Performs validaton using the original Monte Carlo Permutation Test
  // that shuffles the position vectors instead of using synthetic data
  //

  template <class Decimal> class PALMCPTValidation
  {
  public:
    typedef typename list<shared_ptr<PalStrategy<Decimal>>>::const_iterator SurvivingStrategiesIterator;

  public:
    PALMCPTValidation(std::shared_ptr<McptConfiguration<Decimal>> configuration,
			    unsigned long numPermutations)
      : mMonteCarloConfiguration(configuration),
	mNumPermutations(numPermutations),
	mSurvivingStrategies()
    {}

    ~PALMCPTValidation()
    {}

    SurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mSurvivingStrategies.begin();
    }

    SurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mSurvivingStrategies.end();
    }

    unsigned long getNumSurvivingStrategies() const
    {
      return mSurvivingStrategies.size();
    }

  /*  // original code: here for reference. safe to delete
     void runPermutationTests()
    {
      std::shared_ptr<Security<Decimal>> securityToTest = mMonteCarloConfiguration->getSecurity();

      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator = 
	patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Decimal>> longStrategy;
      std::shared_ptr<BackTester<Decimal>> theBackTester;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;
      Decimal pValue;


      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
	{
	  patternToTest = longPatternsIterator->second;
	  strategyName = longStrategyNameBase + std::to_string(strategyNumber);
	  longStrategy = std::make_shared<PalLongStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);

	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());

	  theBackTester->addStrategy(longStrategy);

	  // Run Monte Carlo Permutation Tests using the provided backtester
	  OriginalMCPT<Decimal> mcpt(theBackTester, mNumPermutations);
	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Decimal>::SignificantPValue)
	    {
	      mSurvivingStrategies.push_back (longStrategy);
	      std::cout << "Long Pattern found with p-Value < " << pValue << std::endl;
	    }

	  strategyNumber++;

	}

      std::cout << std::endl << "MCPT Processing short patterns" << std::endl << std::endl;
      
      std::shared_ptr<PalShortStrategy<Decimal>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator = 
	patternsToTest->patternShortsBegin();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
	{
	  patternToTest = shortPatternsIterator->second;
	  strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
	  shortStrategy = std::make_shared<PalShortStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);

	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());
	  theBackTester->addStrategy(shortStrategy);
	  OriginalMCPT<Decimal> mcpt(theBackTester, mNumPermutations);
	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Decimal>::SignificantPValue)
	    {
	      mSurvivingStrategies.push_back (shortStrategy);
	      std::cout << "Short Pattern found with p-Value < " << pValue << std::endl;
	    }
	  strategyNumber++;

	}
    }
*/
    void runPermutationTests()
    {
      std::shared_ptr<Security<Decimal>> securityToTest = mMonteCarloConfiguration->getSecurity();

      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator =
    patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Decimal>> longStrategy;
      std::shared_ptr<BackTester<Decimal>> theBackTester;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;

      runner& Runner=runner::instance();
      std::vector<boost::unique_future<void>> resultsOrErrorsVector;

      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
    {
      patternToTest = longPatternsIterator->second;
      strategyName = longStrategyNameBase + std::to_string(strategyNumber);
      longStrategy = std::make_shared<PalLongStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);

      theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                       oosDates.getFirstDate(),
                       oosDates.getLastDate());

      theBackTester->addStrategy(longStrategy);
      //sends code to be computed to the runner
      resultsOrErrorsVector.emplace_back(Runner.post([this,theBackTester,longStrategy,strategyNumber](){
          Decimal pValue;
          // Run Monte Carlo Permutation Tests using the provided backtester
          OriginalMCPT<Decimal> mcpt(theBackTester, mNumPermutations);
          pValue = mcpt.runPermutationTest();

          if (pValue < DecimalConstants<Decimal>::SignificantPValue)
            {
              boost::mutex::scoped_lock Lock(survivingStrategiesMutex);
              mSurvivingStrategies.push_back (longStrategy);
              std::cout <<"Strategy: "<<strategyNumber<< " Long Pattern found with p-Value < " << pValue << std::endl;
            }
        }));
      strategyNumber++;

    }
      //collects the results from the runner
      for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
      {
          try{
              resultsOrErrorsVector[i].get();
          }
          catch(std::exception const& e)
          {
              std::cerr<<"Strategy: "<<i<<" error: "<<e.what()<<std::endl;
          }
      }

      //cleans up the results vector for reuse
      resultsOrErrorsVector.clear();

      std::cout << std::endl << "MCPT Processing short patterns" << std::endl << std::endl;

      std::shared_ptr<PalShortStrategy<Decimal>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator =
    patternsToTest->patternShortsBegin();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
    {
      patternToTest = shortPatternsIterator->second;
      strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
      shortStrategy = std::make_shared<PalShortStrategy<Decimal>>(strategyName, patternToTest, aPortfolio);

      theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                       oosDates.getFirstDate(),
                       oosDates.getLastDate());
      theBackTester->addStrategy(shortStrategy);

      //sends the code to the runner
      resultsOrErrorsVector.emplace_back(Runner.post([this,theBackTester,shortStrategy,strategyNumber](){
          Decimal pValue;
          OriginalMCPT<Decimal> mcpt(theBackTester, mNumPermutations);
          pValue = mcpt.runPermutationTest();

          if (pValue < DecimalConstants<Decimal>::SignificantPValue)
            {
              boost::mutex::scoped_lock Lock(survivingStrategiesMutex);
              mSurvivingStrategies.push_back (shortStrategy);
              std::cout <<"Strategy: "<<strategyNumber<< " Short Pattern found with p-Value < " << pValue << std::endl;
            }
      }));

      strategyNumber++;

    }
      //collects the results from the runner, signalling exceptions-if any in this thread
      for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
      {
          try{
              resultsOrErrorsVector[i].get();
          }
          catch(std::exception const& e)
          {
              std::cerr<<"Strategy: "<<i<<" error: "<<e.what()<<std::endl;
          }
      }
    }

  private:
    std::shared_ptr<BackTester<Decimal>> getBackTester(TimeFrame::Duration theTimeFrame,
						    boost::gregorian::date startDate, 
						    boost::gregorian::date endDate) const
    {
      if (theTimeFrame == TimeFrame::DAILY)
	return std::make_shared<DailyBackTester<Decimal>>(startDate, endDate);
      else
	throw PALMonteCarloValidationException("PALMCPTValidation::getBackTester - Only daily time frame supported at presetn.");
    }

  private:
    std::shared_ptr<McptConfiguration<Decimal>> mMonteCarloConfiguration;
    unsigned long mNumPermutations;
    boost::mutex  survivingStrategiesMutex;
    std::list<std::shared_ptr<PalStrategy<Decimal>>> mSurvivingStrategies;
  };
}

#endif
