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
#include <thread>
#include <boost/date_time.hpp>
#include <tuple>
#include <type_traits>  // for std::is_same
#include <utility>      // for std::declval
#include "number.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "MonteCarloPermutationTest.h"
#include "McptConfigurationFileReader.h"
#include "PalAst.h"
#include "PermutationTestResultPolicy.h"
#include "MultipleTestingCorrection.h"
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

  /////////////////////////
  //  The base class for PALMonteCarloValidation
  //
  //  Comment: chosen this approach - specialization of dervied classes - to minimize codebase, repeating only key virtual function
  template <class Decimal, typename McptType,
            template <typename> class _StrategySelection> class PALMonteCarloValidationBase
  {
  public:
    typedef typename _StrategySelection<Decimal>::ConstSurvivingStrategiesIterator SurvivingStrategiesIterator;

  public:
    PALMonteCarloValidationBase(std::shared_ptr<McptConfiguration<Decimal>> configuration,
                                unsigned long numPermutations)
      : mMonteCarloConfiguration(configuration),
        mNumPermutations(numPermutations),
        mStrategySelectionPolicy()
    {}

    PALMonteCarloValidationBase (const PALMonteCarloValidationBase<Decimal,
                                 McptType, _StrategySelection>& rhs)
      : mMonteCarloConfiguration(rhs.mMonteCarloConfiguration),
        mNumPermutations(rhs.mNumPermutations),
        mStrategySelectionPolicy(rhs.mStrategySelectionPolicy)
    {}

    PALMonteCarloValidationBase<Decimal, McptType, _StrategySelection>&
    operator=(const PALMonteCarloValidationBase<Decimal, McptType, _StrategySelection> &rhs)
    {
      if (this == &rhs)
        return *this;

      mMonteCarloConfiguration = rhs.mMonteCarloConfiguration;
      mNumPermutations = rhs.mNumPermutations;
      mStrategySelectionPolicy(rhs.mStrategySelectionPolicy);

      return *this;
    }

    virtual ~PALMonteCarloValidationBase()
    {}

    SurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.beginSurvivingStrategies();
    }

    SurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.endSurvivingStrategies();;
    }

    unsigned long getNumSurvivingStrategies() const
    {
      return (unsigned long) mStrategySelectionPolicy.getNumSurvivingStrategies();
    }

    virtual void runPermutationTests() = 0;

  protected:
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

  protected:
    std::shared_ptr<McptConfiguration<Decimal>> mMonteCarloConfiguration;
    unsigned long mNumPermutations;
    _StrategySelection<Decimal> mStrategySelectionPolicy;
  };

  // Number of threads to use for the outer patterns loop
  static constexpr std::size_t kOuterThreads =
    std::min<std::size_t>(
			  4,
			  std::max<std::size_t>(2, std::thread::hardware_concurrency() / 2));
			  
  /////////////////////////
  //  The default functionality(non-specialization) for PALMonteCarloValidation
  //
  template <class Decimal,
	    typename McptType,
            template <typename> class _StrategySelection,
	    typename Executor = concurrency::ThreadPoolExecutor<kOuterThreads>>
  class PALMonteCarloValidation: public PALMonteCarloValidationBase<Decimal,
								    McptType,
								    _StrategySelection>
  {
  public:
    using ResultType = typename McptType::ResultType;

    // 2) Sanity check: ensure that alias matches the actual method signature
    static_assert(std::is_same<ResultType,
		  decltype(std::declval<McptType>().runPermutationTest())>::value,
		  "McptType::ResultType must match the return type of runPermutationTest()");

    PALMonteCarloValidation(std::shared_ptr<McptConfiguration<Decimal>> configuration,
                            unsigned long numPermutations)
      : PALMonteCarloValidationBase<Decimal,McptType, _StrategySelection>(configuration, numPermutations)
    {}

    PALMonteCarloValidation (const PALMonteCarloValidation<Decimal,
                             McptType, _StrategySelection>& rhs)
      : PALMonteCarloValidationBase<Decimal,McptType, _StrategySelection>(rhs)
    {}

    ~PALMonteCarloValidation()
    {}

    void runPermutationTests() override
    {
      // 1) Prepare data
      auto tempSecurity = this->mMonteCarloConfiguration->getSecurity();
      auto oosTS = FilterTimeSeries<Decimal>(*tempSecurity->getTimeSeries(),
                                             this->mMonteCarloConfiguration->getOosDateRange());
      auto tempOosTS = std::make_shared<OHLCTimeSeries<Decimal>>(oosTS);
      auto securityToTest = tempSecurity->clone(tempOosTS);
      auto patternsToTest = this->mMonteCarloConfiguration->getPricePatterns();
      auto oosDates = this->mMonteCarloConfiguration->getOosDateRange();
      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(securityToTest->getName() + " Portfolio");
      aPortfolio->addSecurity(securityToTest);

      const std::string longPrefix  = "PAL Long Strategy ";
      const std::string shortPrefix = "PAL Short Strategy ";

      // 2) Collect all patterns into a vector for indexing
      std::vector<PALPatternPtr> patterns;
      for (auto it = patternsToTest->allPatternsBegin();
           it != patternsToTest->allPatternsEnd();
           ++it)
      {
        patterns.push_back(*it);
      }

      const size_t numPatterns = patterns.size();

      // 3) Execute tests in parallel
      std::mutex               strategyMutex;
      Executor                 executor{};

      concurrency::parallel_for(
        numPatterns,
        executor,
        [=, &strategyMutex, this](size_t idx)
        {
          auto patternToTest = patterns[idx];
          size_t strategyNumber = idx + 1;

          // create concrete strategy
          auto strategy = makeStrategy(
            strategyNumber,
            patternToTest,
            aPortfolio,
            longPrefix,
            shortPrefix
          );

          // backtest
          auto bt = this->getBackTester(
            securityToTest->getTimeSeries()->getTimeFrame(),
            oosDates.getFirstDate(),
            oosDates.getLastDate()
          );
          bt->addStrategy(strategy);

          // run MCPT
          McptType mcpt(bt, this->mNumPermutations);
          ResultType result = mcpt.runPermutationTest();

          // record under lock
          std::lock_guard<std::mutex> lock(strategyMutex);
          this->mStrategySelectionPolicy.addStrategy(result, strategy);
        }
      );

      // 4) Final correction
      this->mStrategySelectionPolicy.correctForMultipleTests();
    }
 
  private:
     /**
      * Construct either a PalLongStrategy or PalShortStrategy based on
      * pattern->isLongPattern(), using the correct prefix + strategyNumber.
      */
    static std::shared_ptr<PalStrategy<Decimal>> makeStrategy(size_t strategyNumber,
							      const PALPatternPtr& pattern,
							      const std::shared_ptr<Portfolio<Decimal>>& aPortfolio,
							      const std::string& longPrefix,
							      const std::string& shortPrefix)
    {
      bool isLong = pattern->isLongPattern();
      // pick the right name base
      std::string name = (isLong ? longPrefix : shortPrefix)
	+ std::to_string(strategyNumber);

      if (isLong)
	return std::make_shared<PalLongStrategy<Decimal>>(name, pattern, aPortfolio);
      else
	return std::make_shared<PalShortStrategy<Decimal>>(name, pattern, aPortfolio);
    }
    
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
