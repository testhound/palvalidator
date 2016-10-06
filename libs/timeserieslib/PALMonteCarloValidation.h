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
#include "decimal.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "MonteCarloPermutationTest.h"
#include "McptConfigurationFileReader.h"
#include "PalAst.h"

#include "runner.hpp"

namespace mkc_timeseries
{
  using dec::decimal;
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

  template <int Prec, typename McptType> class PALMonteCarloValidation
  {
  public:
    typedef typename list<shared_ptr<PalStrategy<Prec>>>::const_iterator SurvivingStrategiesIterator;

  public:
    PALMonteCarloValidation(std::shared_ptr<McptConfiguration<Prec>> configuration,
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
      std::shared_ptr<Security<Prec>> securityToTest = mMonteCarloConfiguration->getSecurity();

      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator = 
	patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Prec>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Prec>> longStrategy;
      std::shared_ptr<BackTester<Prec>> theBackTester;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;
      dec::decimal<Prec> pValue;

      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
	{
	  patternToTest = longPatternsIterator->second;
	  strategyName = longStrategyNameBase + std::to_string(strategyNumber);
	  longStrategy = std::make_shared<PalLongStrategy<Prec>>(strategyName, patternToTest, aPortfolio);
	  
	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());

	  theBackTester->addStrategy(longStrategy);

	  std::cout << "Running MCPT for strategy " << strategyNumber << std::endl;
	  // Run Monte Carlo Permutation Tests using the provided backtester
	  McptType mcpt(theBackTester, mNumPermutations);
	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Prec>::SignificantPValue)
	    {
	      mSurvivingStrategies.push_back (longStrategy);
	      std::cout << "Long Pattern found with p-Value < " << pValue << std::endl;
	    }

	  strategyNumber++;

	}

      std::cout << std::endl << "MCPT Processing short patterns" << std::endl << std::endl;
      
      std::shared_ptr<PalShortStrategy<Prec>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator = 
	patternsToTest->patternShortsBegin();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
	{
	  patternToTest = shortPatternsIterator->second;
	  strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
	  shortStrategy = std::make_shared<PalShortStrategy<Prec>>(strategyName, patternToTest, aPortfolio);
	  
	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());
	  theBackTester->addStrategy(shortStrategy);

	  std::cout << "Running MCPT for strategy " << strategyNumber << std::endl;
	  McptType mcpt(theBackTester, mNumPermutations);

	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Prec>::SignificantPValue)
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
      std::shared_ptr<Security<Prec>> securityToTest = mMonteCarloConfiguration->getSecurity();
      securityToTest->getTimeSeries()->syncronizeMapAndArray();
      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator =
        patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Prec>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Prec>> longStrategy;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;

      //build thread-pool-runner
      runner& Runner=getRunner();
      std::vector<std::future<void>> resultsOrErrorsVector;

      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
      {

        patternToTest = longPatternsIterator->second;
        strategyName = longStrategyNameBase + std::to_string(strategyNumber);
        longStrategy = std::make_shared<PalLongStrategy<Prec>>(strategyName, patternToTest, aPortfolio);

        auto theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                                oosDates.getFirstDate(),
                                oosDates.getLastDate());

        theBackTester->addStrategy(longStrategy);
        //start paralel part
        resultsOrErrorsVector.emplace_back(Runner.post([ this
                                                       , strategyNumber
                                                       , theBackTester = std::move(theBackTester)
                                                       , longStrategy]() -> void {
            dec::decimal<Prec> pValue;
            std::stringstream s;
            s << "Running MCPT for strategy " << strategyNumber <<' '<< std::endl;
            std::cout<<s.str();
            // Run Monte Carlo Permutation Tests using the provided backtester
            McptType mcpt(theBackTester, mNumPermutations);
            pValue = mcpt.runPermutationTest();

            if (pValue < DecimalConstants<Prec>::SignificantPValue)
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

      std::shared_ptr<PalShortStrategy<Prec>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator =
    patternsToTest->patternShortsBegin();
      resultsOrErrorsVector.clear();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
      {
        patternToTest = shortPatternsIterator->second;
        strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
        shortStrategy = std::make_shared<PalShortStrategy<Prec>>(strategyName, patternToTest, aPortfolio);

        auto theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                               oosDates.getFirstDate(),
                               oosDates.getLastDate());
        theBackTester->addStrategy(shortStrategy);

        //sends code to the runner
        resultsOrErrorsVector.emplace_back(Runner.post([strategyNumber,theBackTester,this,shortStrategy](){
            dec::decimal<Prec> pValue;
            std::stringstream s;
            s<<"Running MCPT for strategy " << strategyNumber << std::endl;
            std::cout<<s.str();
            //std::cout << "Running MCPT for strategy " << strategyNumber <<' '<< std::endl;
            McptType mcpt(theBackTester, mNumPermutations);

            pValue = mcpt.runPermutationTest();

            if (pValue < DecimalConstants<Prec>::SignificantPValue)
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
    std::shared_ptr<BackTester<Prec>> getBackTester(TimeFrame::Duration theTimeFrame,
						    boost::gregorian::date startDate, 
						    boost::gregorian::date endDate) const
    {
      if (theTimeFrame == TimeFrame::DAILY)
	return std::make_shared<DailyBackTester<Prec>>(startDate, endDate);
      else if (theTimeFrame == TimeFrame::WEEKLY)
	return std::make_shared<WeeklyBackTester<Prec>>(startDate, endDate);
      else if (theTimeFrame == TimeFrame::MONTHLY)
	return std::make_shared<MonthlyBackTester<Prec>>(startDate, endDate);
      else
	throw PALMonteCarloValidationException("PALMonteCarloValidation::getBackTester - Only daily and monthly time frame supported at present.");
    }

  private:
    std::shared_ptr<McptConfiguration<Prec>> mMonteCarloConfiguration;
    unsigned long mNumPermutations;
    boost::mutex  survivingStrategiesMutex;
    std::list<std::shared_ptr<PalStrategy<Prec>>> mSurvivingStrategies;
  };

  /////////////////////////
  // class PALMCPTtValidation
  //
  // Performs validaton using the original Monte Carlo Permutation Test
  // that shuffles the position vectors instead of using synthetic data
  //

  template <int Prec> class PALMCPTValidation
  {
  public:
    typedef typename list<shared_ptr<PalStrategy<Prec>>>::const_iterator SurvivingStrategiesIterator;

  public:
    PALMCPTValidation(std::shared_ptr<McptConfiguration<Prec>> configuration,
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
      std::shared_ptr<Security<Prec>> securityToTest = mMonteCarloConfiguration->getSecurity();

      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator = 
	patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Prec>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Prec>> longStrategy;
      std::shared_ptr<BackTester<Prec>> theBackTester;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;
      dec::decimal<Prec> pValue;


      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
	{
	  patternToTest = longPatternsIterator->second;
	  strategyName = longStrategyNameBase + std::to_string(strategyNumber);
	  longStrategy = std::make_shared<PalLongStrategy<Prec>>(strategyName, patternToTest, aPortfolio);

	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());

	  theBackTester->addStrategy(longStrategy);

	  // Run Monte Carlo Permutation Tests using the provided backtester
	  OriginalMCPT<Prec> mcpt(theBackTester, mNumPermutations);
	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Prec>::SignificantPValue)
	    {
	      mSurvivingStrategies.push_back (longStrategy);
	      std::cout << "Long Pattern found with p-Value < " << pValue << std::endl;
	    }

	  strategyNumber++;

	}

      std::cout << std::endl << "MCPT Processing short patterns" << std::endl << std::endl;
      
      std::shared_ptr<PalShortStrategy<Prec>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator = 
	patternsToTest->patternShortsBegin();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
	{
	  patternToTest = shortPatternsIterator->second;
	  strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
	  shortStrategy = std::make_shared<PalShortStrategy<Prec>>(strategyName, patternToTest, aPortfolio);

	  theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
				       oosDates.getFirstDate(),
				       oosDates.getLastDate());
	  theBackTester->addStrategy(shortStrategy);
	  OriginalMCPT<Prec> mcpt(theBackTester, mNumPermutations);
	  pValue = mcpt.runPermutationTest();

	  if (pValue < DecimalConstants<Prec>::SignificantPValue)
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
      std::shared_ptr<Security<Prec>> securityToTest = mMonteCarloConfiguration->getSecurity();

      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator =
    patternsToTest->patternLongsBegin();

      DateRange oosDates = mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Prec>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      std::shared_ptr<PriceActionLabPattern> patternToTest;
      std::shared_ptr<PalLongStrategy<Prec>> longStrategy;
      std::shared_ptr<BackTester<Prec>> theBackTester;

      std::string longStrategyNameBase("PAL Long Strategy ");

      std::string strategyName;
      unsigned long strategyNumber = 1;

      runner& Runner=getRunner();
      std::vector<std::future<void>> resultsOrErrorsVector;

      for (; longPatternsIterator != patternsToTest->patternLongsEnd(); longPatternsIterator++)
    {
      patternToTest = longPatternsIterator->second;
      strategyName = longStrategyNameBase + std::to_string(strategyNumber);
      longStrategy = std::make_shared<PalLongStrategy<Prec>>(strategyName, patternToTest, aPortfolio);

      theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                       oosDates.getFirstDate(),
                       oosDates.getLastDate());

      theBackTester->addStrategy(longStrategy);
      //sends code to be computed to the runner
      resultsOrErrorsVector.emplace_back(Runner.post([this,theBackTester,longStrategy,strategyNumber](){
          dec::decimal<Prec> pValue;
          // Run Monte Carlo Permutation Tests using the provided backtester
          OriginalMCPT<Prec> mcpt(theBackTester, mNumPermutations);
          pValue = mcpt.runPermutationTest();

          if (pValue < DecimalConstants<Prec>::SignificantPValue)
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

      std::shared_ptr<PalShortStrategy<Prec>> shortStrategy;
      std::string shortStrategyNameBase("PAL Short Strategy ");
      PriceActionLabSystem::ConstSortedPatternIterator shortPatternsIterator =
    patternsToTest->patternShortsBegin();

      for (; shortPatternsIterator != patternsToTest->patternShortsEnd(); shortPatternsIterator++)
    {
      patternToTest = shortPatternsIterator->second;
      strategyName = shortStrategyNameBase + std::to_string(strategyNumber);
      shortStrategy = std::make_shared<PalShortStrategy<Prec>>(strategyName, patternToTest, aPortfolio);

      theBackTester = getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                       oosDates.getFirstDate(),
                       oosDates.getLastDate());
      theBackTester->addStrategy(shortStrategy);

      //sends the code to the runner
      resultsOrErrorsVector.emplace_back(Runner.post([this,theBackTester,shortStrategy,strategyNumber](){
          dec::decimal<Prec> pValue;
          OriginalMCPT<Prec> mcpt(theBackTester, mNumPermutations);
          pValue = mcpt.runPermutationTest();

          if (pValue < DecimalConstants<Prec>::SignificantPValue)
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
    std::shared_ptr<BackTester<Prec>> getBackTester(TimeFrame::Duration theTimeFrame,
						    boost::gregorian::date startDate, 
						    boost::gregorian::date endDate) const
    {
      if (theTimeFrame == TimeFrame::DAILY)
	return std::make_shared<DailyBackTester<Prec>>(startDate, endDate);
      else
	throw PALMonteCarloValidationException("PALMCPTValidation::getBackTester - Only daily time frame supported at presetn.");
    }

  private:
    std::shared_ptr<McptConfiguration<Prec>> mMonteCarloConfiguration;
    unsigned long mNumPermutations;
    boost::mutex  survivingStrategiesMutex;
    std::list<std::shared_ptr<PalStrategy<Prec>>> mSurvivingStrategies;
  };
}

#endif
