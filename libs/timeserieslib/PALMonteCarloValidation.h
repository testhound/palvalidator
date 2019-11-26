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
#include "MultipleTestingCorrection.h"
#include "UnadjustedPValueStrategySelection.h"
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


  /////////////////////////
  //  The default functionality(non-specialization) for PALMonteCarloValidation
  //
  template <class Decimal, typename McptType,
            template <typename> class _StrategySelection> class PALMonteCarloValidation:
      public PALMonteCarloValidationBase<Decimal,McptType, _StrategySelection>
  {
  public:
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

    void runPermutationTests()
    {
      // Create a security with just the OOS time series
      std::shared_ptr<Security<Decimal>> tempSecurity = this->mMonteCarloConfiguration->getSecurity();
      auto oosTimeSeries (FilterTimeSeries<Decimal> (*tempSecurity->getTimeSeries(),
                                                     this->mMonteCarloConfiguration->getOosDateRange()));

      auto tempOosTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>> (oosTimeSeries);

      std::shared_ptr<Security<Decimal>> securityToTest = tempSecurity->clone (tempOosTimeSeries);
      // std::shared_ptr<Security<Decimal>> securityToTest = tempSecurity->clone (oosTimeSeries);

      //std::shared_ptr<Security<Decimal>> securityToTest = mMonteCarloConfiguration->getSecurity();
      securityToTest->getTimeSeries()->syncronizeMapAndArray();

      // This line gets the patterns that have been read from the IR file
      PriceActionLabSystem *patternsToTest = this->mMonteCarloConfiguration->getPricePatterns();

      PriceActionLabSystem::ConstSortedPatternIterator longPatternsIterator =
          patternsToTest->patternLongsBegin();

      DateRange oosDates = this->mMonteCarloConfiguration->getOosDateRange();

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

          auto theBackTester = this->getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                                                   oosDates.getFirstDate(),
                                                   oosDates.getLastDate());

          theBackTester->addStrategy(longStrategy);
          //start paralel part
          resultsOrErrorsVector.emplace_back(Runner.post([ this
                                                         , strategyNumber
                                                         , theBackTester = std::move(theBackTester)
                                                         , longStrategy]() -> void {
              Decimal pValue;
              {
                std::stringstream s;
                s << "Running MCPT for strategy " << strategyNumber <<' '<< std::endl;
                std::cout<<s.str();
              }

              // Run Monte Carlo Permutation Tests using the provided backtester
              McptType mcpt(theBackTester, this->mNumPermutations);

              pValue = mcpt.runPermutationTest();

              this->mStrategySelectionPolicy.addStrategy (pValue, longStrategy);

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

          auto theBackTester = this->getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                                                   oosDates.getFirstDate(),
                                                   oosDates.getLastDate());
          theBackTester->addStrategy(shortStrategy);

          //sends code to the runner
          resultsOrErrorsVector.emplace_back(Runner.post([strategyNumber,theBackTester,this,shortStrategy](){
              Decimal pValue;
              std::stringstream s;
              s<<"Running MCPT for strategy " << strategyNumber << std::endl;
              std::cout<<s.str();

              McptType mcpt(theBackTester, this->mNumPermutations);

              pValue = mcpt.runPermutationTest();
              this->mStrategySelectionPolicy.addStrategy (pValue, shortStrategy);

            }));

          strategyNumber++;

        }

      //collects exceptions from the runner and waits for the computation end to be signalled
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

      this->mStrategySelectionPolicy.selectSurvivingStrategies();
    }

  };

  /////////////////////////
  //  A specialization of PALMonteCarloValidation that operates on BestOf selections
  //
  template <class Decimal,
            template <typename> class _StrategySelection,
            template <typename> class _BackTestResultPolicy,
            typename _ComputationPolicy>
  class PALMonteCarloValidation<Decimal, BestOfMonteCarloPermuteMarketChanges<Decimal, _BackTestResultPolicy, _ComputationPolicy> ,_StrategySelection>:
      public PALMonteCarloValidationBase<Decimal, BestOfMonteCarloPermuteMarketChanges<Decimal, _BackTestResultPolicy, _ComputationPolicy>, _StrategySelection>
  {
  public:
    typedef BestOfMonteCarloPermuteMarketChanges<Decimal, _BackTestResultPolicy, _ComputationPolicy> BestOfMcptType;

  public:
    PALMonteCarloValidation<Decimal, BestOfMonteCarloPermuteMarketChanges<Decimal, _BackTestResultPolicy, _ComputationPolicy> ,_StrategySelection>(std::shared_ptr<McptConfiguration<Decimal>> configuration,
                                                                                                                                                   unsigned long numPermutations)
      : PALMonteCarloValidationBase<Decimal, BestOfMcptType, _StrategySelection>(configuration, numPermutations)
    {}

    PALMonteCarloValidation<Decimal, BestOfMonteCarloPermuteMarketChanges<Decimal, _BackTestResultPolicy, _ComputationPolicy> ,_StrategySelection> (const PALMonteCarloValidation<Decimal,
                                                                                                                                                    BestOfMonteCarloPermuteMarketChanges<Decimal, _BackTestResultPolicy, _ComputationPolicy>, _StrategySelection>& rhs)
      : PALMonteCarloValidationBase<Decimal, BestOfMcptType, _StrategySelection>(rhs)
    {}

    ~PALMonteCarloValidation()
    {}

    void runPermutationTests()
    {
      runPermutationTests(this->mMonteCarloConfiguration->getPricePatterns());
    }

    void runPermutationTests(PriceActionLabSystem* patternsToTest)
    {

      // Create a security with just the OOS time series
      std::shared_ptr<Security<Decimal>> tempSecurity = this->mMonteCarloConfiguration->getSecurity();
      auto oosTimeSeries (FilterTimeSeries<Decimal> (*tempSecurity->getTimeSeries(),
                                                     this->mMonteCarloConfiguration->getOosDateRange()));

      auto tempOosTimeSeries = std::make_shared<OHLCTimeSeries<Decimal>> (oosTimeSeries);

      std::shared_ptr<Security<Decimal>> securityToTest = tempSecurity->clone (tempOosTimeSeries);

      securityToTest->getTimeSeries()->syncronizeMapAndArray();

      // This line gets the patterns that have been read from the IR file
//      PriceActionLabSystem *patternsToTest = this->mMonteCarloConfiguration->getPricePatterns();

      std::cout << "Running Best of permutation test with approximate permutation count of " << (patternsToTest->getNumPatterns() *  this->mNumPermutations)
                << "\n (" << patternsToTest->getNumPatterns() << " #patterns X " << this->mNumPermutations << " #permutations)" << std::endl;

      DateRange oosDates = this->mMonteCarloConfiguration->getOosDateRange();

      std::string portfolioName(securityToTest->getName() + std::string(" Portfolio"));

      auto aPortfolio = std::make_shared<Portfolio<Decimal>>(portfolioName);
      aPortfolio->addSecurity(securityToTest);

      auto theBackTester = this->getBackTester(securityToTest->getTimeSeries()->getTimeFrame(),
                                               oosDates.getFirstDate(),
                                               oosDates.getLastDate());

      // Runs permutations and gets all the tests that can be deemed valid
      BestOfMcptType mcpt(theBackTester, this->mNumPermutations, patternsToTest, aPortfolio);
      Decimal numberOfValidTests = mcpt.runPermutationTest();

      // Get the Results from the combined permutations, mapped to each strategy
      typename BestOfMcptType::StrategyResultMapType resultsMap;
      resultsMap = mcpt.getStrategiesResultsMap();

      // Iterate results to retrieve #of times beaten, and calculate pValue
      for( auto const& [baselineStat, strategyContainer] : resultsMap )
        {
          std::shared_ptr<PalStrategy<Decimal>> strategy = std::get<0>(strategyContainer);
          uint32_t timesBeaten;
          timesBeaten = std::get<1>(strategyContainer);
          //std::cout << "Checking BestOfpValue for: " << strategy->getStrategyName() << "." << std::endl;
          //std::cout << timesBeaten << " of " << numberOfValidTests.getAsInteger() << std::endl;
          Decimal pValue;
          //pValue = Decimal(timesBeaten) / numberOfValidTests;
          // TODO: For reasons yet unknown this has undefined behavior with large numbers. Needs investigation.
          double pDouble = double(timesBeaten)/ numberOfValidTests.getAsDouble();
          pValue = Decimal(pDouble);
          this->mStrategySelectionPolicy.addStrategy (pValue, strategy);
        }

      this->mStrategySelectionPolicy.selectSurvivingStrategies();
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
