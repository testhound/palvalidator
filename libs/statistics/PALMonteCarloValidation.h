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

  // Base class: parameter-driven Monte Carlo validation
  /*!
   * @class PALMonteCarloValidationBase
   * @brief Base class for parameter-driven Monte Carlo validation of trading strategies.
   *
   * This class provides the fundamental interface and common functionalities for
   * performing Monte Carlo permutation tests to assess the validity of trading strategies.
   * It is designed to be subclassed by concrete implementations that define the
   * specific testing procedures.
   *
   * @tparam Decimal The numeric type used for calculations (e.g., double, mpfr).
   * @tparam McptType The type of Monte Carlo Permutation Test to be used (e.g., OriginalMCPT).
   * @tparam _StrategySelection A template template parameter for the strategy selection policy
   * class, which manages and filters strategies based on test results.
   */
  template <class Decimal,
            typename McptType,
            template <typename> class _StrategySelection>
  class PALMonteCarloValidationBase
  {
  public:
     /*! @brief Iterator type for accessing surviving strategies. */
    using SurvivingStrategiesIterator =
      typename _StrategySelection<Decimal>::ConstSurvivingStrategiesIterator;

    /*!
     * @brief Constructs the PALMonteCarloValidationBase.
     * @param numPermutations The number of permutations to be performed in the Monte Carlo tests.
     * Must be a positive value.
     * @throw std::invalid_argument if numPermutations is zero.
     */
    explicit PALMonteCarloValidationBase(unsigned long numPermutations)
      : mNumPermutations(numPermutations),
        mStrategySelectionPolicy()
    {
      if (mNumPermutations == 0)
        throw std::invalid_argument("Number of permutations must be positive");
    }

    PALMonteCarloValidationBase(const PALMonteCarloValidationBase&) = default;
    PALMonteCarloValidationBase& operator=(const PALMonteCarloValidationBase&) = default;
    virtual ~PALMonteCarloValidationBase() = default;

    /*!
     * @brief Pure virtual method to run permutation tests.
     *
     * Concrete derived classes must implement this method to perform the actual
     * permutation tests on the given security data, set of trading patterns (strategies),
     * and over the specified date range.
     *
     * @param baseSecurity A shared pointer to the security on which strategies will be tested.
     * @param patterns A pointer to the PriceActionLabSystem containing the trading patterns to validate.
     * @param dateRange The date range for which the permutation tests should be run.
     */
    virtual void runPermutationTests(shared_ptr<Security<Decimal>> baseSecurity,
				     PriceActionLabSystem* patterns,
				     const DateRange& dateRange,
				     const Decimal& pValueSignificanceLevel =
				     DecimalConstants<Decimal>::SignificantPValue) = 0;

    /*!
     * @brief Gets an iterator to the beginning of the list of surviving strategies.
     *
     * Surviving strategies are those that pass the Monte Carlo validation criteria.
     * This method should be called after runPermutationTests().
     *
     * @return A const iterator pointing to the first surviving strategy.
     */
    SurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.beginSurvivingStrategies();
    }

    /*!
     * @brief Gets an iterator to the end of the list of surviving strategies.
     *
     * This method should be called after runPermutationTests().
     *
     * @return A const iterator pointing past the last surviving strategy.
     */
    SurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mStrategySelectionPolicy.endSurvivingStrategies();
    }

    /*!
     * @brief Gets the number of strategies that survived the validation process.
     *
     * This method should be called after runPermutationTests().
     *
     * @return The total count of surviving strategies.
     */
    unsigned long getNumSurvivingStrategies() const
    {
      return static_cast<unsigned long>(mStrategySelectionPolicy.getNumSurvivingStrategies());
    }

  protected:
    unsigned long                 mNumPermutations;
    _StrategySelection<Decimal>   mStrategySelectionPolicy;
  };

  /*!
   * @class PALMonteCarloValidation
   * @brief A concrete implementation of Monte Carlo validation for trading strategies.
   *
   * This class extends PALMonteCarloValidationBase to provide a specific methodology
   * for running permutation tests. It handles data preparation, executes backtests
   * for each strategy, performs Monte Carlo Permutation Tests (MCPT) potentially in parallel,
   * and applies multiple testing corrections.
   *
   * @tparam Decimal The numeric type used for calculations (e.g., double, mpfr).
   * @tparam McptType The type of Monte Carlo Permutation Test to be used (e.g., OriginalMCPT).
   * It must have a `ResultType` typedef and a `runPermutationTest()` method.
   * @tparam _StrategySelection A template template parameter for the strategy selection policy
   * class (e.g., UnadjustedPValueStrategySelection).
   * @tparam Executor The type of parallel executor to use for running tests.
   * Defaults to concurrency::StdAsyncExecutor.
   */
  template <class Decimal,
            typename McptType,
            template <typename> class _StrategySelection,
            typename Executor = concurrency::StdAsyncExecutor>
  class PALMonteCarloValidation
    : public PALMonteCarloValidationBase<Decimal, McptType, _StrategySelection>
  {
    using Base = PALMonteCarloValidationBase<Decimal, McptType, _StrategySelection>;
    using StrategyPtr = std::shared_ptr<PalStrategy<Decimal>>;
    using ResultType  = typename McptType::ResultType;

  public:
    static_assert(std::is_same<ResultType,
                  decltype(std::declval<McptType>().runPermutationTest())>::value,
                  "McptType::ResultType must match the return type of runPermutationTest()");

    /*!
     * @brief Constructs the PALMonteCarloValidation object.
     * @param numPermutations The number of permutations to be performed in the Monte Carlo tests.
     * Must be a positive value.
     * @throw std::invalid_argument if numPermutations is zero (via base class constructor).
     */
    explicit PALMonteCarloValidation(unsigned long numPermutations)
      : Base(numPermutations)
    {}

    /*!
     * @brief Runs permutation tests for the given strategies.
     *
     * This implementation involves:
     * 1. Preparing the out-of-sample time series data for the base security.
     * 2. Creating a portfolio with the cloned security.
     * 3. Iterating through all patterns (strategies) provided:
     * a. Creating a PalStrategy (long or short) for each pattern.
     * b. Setting up a BackTester for the strategy over the specified date range.
     * c. Running the specified McptType (Monte Carlo Permutation Test) for the strategy.
     * d. Collecting the raw result (e.g., p-value) and the strategy.
     * 4. The tests for each strategy are executed in parallel using the specified Executor.
     * 5. After all individual tests are complete, a multiple testing correction (e.g., Bonferroni)
     * is applied to the collected results via the strategy selection policy.
     *
     * @param baseSecurity A shared pointer to the security on which strategies will be tested.
     * Must not be null.
     * @param patterns A pointer to the PriceActionLabSystem containing the trading patterns to validate.
     * Must not be null.
     * @param dateRange The date range for which the permutation tests should be run.
     * @param pValueSignificanceLevel The significance level (alpha) used for the final
     * multiple testing correction. Defaults to a predefined
     * significant p-value from DecimalConstants.
     * @throw std::invalid_argument if baseSecurity or patterns is null.
     * @override
     */
    void runPermutationTests(shared_ptr<Security<Decimal>> baseSecurity,
			     PriceActionLabSystem* patterns,
			     const DateRange& dateRange,
			     const Decimal& pValueSignificanceLevel =
			     DecimalConstants<Decimal>::SignificantPValue) override
    {
      if (!baseSecurity)
        throw std::invalid_argument("Base security must not be null");
      if (!patterns)
        throw std::invalid_argument("Pattern set must not be null");

      // 1) Prepare data
      auto oosTS     = FilterTimeSeries<Decimal>(*baseSecurity->getTimeSeries(), dateRange);
      auto tempOosTS = std::make_shared<OHLCTimeSeries<Decimal>>(oosTS);
      auto clonedSec = baseSecurity->clone(tempOosTS);

      // Build portfolio
      auto portfolio = std::make_shared<Portfolio<Decimal>>(clonedSec->getName() + " Portfolio");
      portfolio->addSecurity(clonedSec);

      // Naming prefixes
      const std::string longPrefix  = "PAL Long Strategy ";
      const std::string shortPrefix = "PAL Short Strategy ";

      // 2) Collect patterns
      std::vector<PALPatternPtr> vecPatterns;
      for (auto it = patterns->allPatternsBegin(); it != patterns->allPatternsEnd(); ++it)
        vecPatterns.push_back(*it);

      // 3) Parallel backtests and MCPT
      std::mutex strategyMutex;
      Executor executor;
      size_t total = vecPatterns.size();

      concurrency::parallel_for(
        total, executor,
        [&](size_t idx) {
          auto pattern = vecPatterns[idx];

	  // Build strategy via centralized factory
	  std::string name = (pattern->isLongPattern() ? longPrefix : shortPrefix)
	    + std::to_string(idx + 1);
	  auto strategy = makePalStrategy<Decimal>(name, pattern, portfolio);

          // Use factory to get a backtester for this date range
          auto bt = BackTesterFactory<Decimal>::getBackTester(
                      baseSecurity->getTimeSeries()->getTimeFrame(),
                      dateRange.getFirstDate(),
                      dateRange.getLastDate());
          bt->addStrategy(strategy);

          McptType mcpt(bt, this->mNumPermutations);
          ResultType res = mcpt.runPermutationTest();

          std::lock_guard<std::mutex> lock(strategyMutex);
          this->mStrategySelectionPolicy.addStrategy(res, strategy);
        }
      );

      // 4) Final family-wise error correction
      this->mStrategySelectionPolicy.correctForMultipleTests(pValueSignificanceLevel);
    }

  private:
    static std::shared_ptr<PalStrategy<Decimal>> makeStrategy(
      size_t                              idx,
      const PALPatternPtr&                pattern,
      const std::shared_ptr<Portfolio<Decimal>>& portfolio,
      const std::string&                  longPrefix,
      const std::string&                  shortPrefix)
    {
      bool isLong = pattern->isLongPattern();
      std::string name = (isLong ? longPrefix : shortPrefix) + std::to_string(idx);
      if (isLong)
        return std::make_shared<PalLongStrategy<Decimal>>(name, pattern, portfolio);
      else
        return std::make_shared<PalShortStrategy<Decimal>>(name, pattern, portfolio);
    }
  };

  /////////////////////////
  // class PALMCPTtValidation
  //
  // Performs validaton using the original Monte Carlo Permutation Test
  // that shuffles the position vectors instead of using synthetic data
  //

  template <class Decimal>
  class PALMCPTValidation
  {
  public:
    using SurvivingStrategiesIterator =
      typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator;

    // 1) Constructor now only needs numPermutations
    explicit PALMCPTValidation(unsigned long numPermutations)
      : mNumPermutations(numPermutations)
    {
      if (mNumPermutations == 0)
	throw std::invalid_argument("Number of permutations must be positive");
    }

    // 2) Accessors unchanged
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
      return static_cast<unsigned long>(mSurvivingStrategies.size());
    }

    // 3) Signature now parallels PALMonteCarloValidation
    void runPermutationTests(
			     std::shared_ptr<Security<Decimal>> baseSecurity,
			     PriceActionLabSystem*             patterns,
			     const DateRange&                  dateRange)
    {
      if (!baseSecurity)
	throw std::invalid_argument("Base security must not be null");
      if (!patterns)
	throw std::invalid_argument("Pattern set must not be null");

      // Prepare out-of-sample series
      auto oosTS     = FilterTimeSeries<Decimal>(*baseSecurity->getTimeSeries(), dateRange);
      auto tempOosTS = std::make_shared<OHLCTimeSeries<Decimal>>(oosTS);
      auto testSec   = baseSecurity->clone(tempOosTS);

      // Build a one-security portfolio
      auto portfolio = std::make_shared<Portfolio<Decimal>>(testSec->getName() + " Portfolio");
      portfolio->addSecurity(testSec);

      // Runner for async dispatch
      runner& Runner = runner::instance();
      std::vector<boost::unique_future<void>> futures;

      // Mutex to protect survivors list
      boost::mutex survivorsMu;

      unsigned long strategyNumber = 1;

      // 3a) Long patterns
      for (auto it = patterns->patternLongsBegin(); it != patterns->patternLongsEnd(); ++it, ++strategyNumber)
	{
	  auto pattern = it->second;
	  std::string name = "PAL Long Strategy " + std::to_string(strategyNumber);
	  auto strat = std::make_shared<PalLongStrategy<Decimal>>(name, pattern, portfolio);

	  auto bt = BackTesterFactory<Decimal>::getBackTester(
							      baseSecurity->getTimeSeries()->getTimeFrame(),
							      dateRange.getFirstDate(),
							      dateRange.getLastDate());
	  bt->addStrategy(strat);

	  futures.emplace_back(Runner.post([this, bt, strat, strategyNumber, &survivorsMu](){
	    OriginalMCPT<Decimal> mcpt(bt, mNumPermutations);
	    Decimal p = mcpt.runPermutationTest();
	    if (p < DecimalConstants<Decimal>::SignificantPValue)
	      {
		boost::mutex::scoped_lock lk(survivorsMu);
		mSurvivingStrategies.push_back(strat);
		std::cout << "Strategy: " << strategyNumber
			  << " Long Pattern found with p-value < " << p << std::endl;
	      }
	  }));
	}

      // Wait for long‐pattern tasks
      for (auto &f : futures) {
	try { f.get(); }
	catch (const std::exception &e) { std::cerr << "Error: " << e.what() << std::endl; }
      }
      futures.clear();

      // 3b) Short patterns
      for (auto it = patterns->patternShortsBegin(); it != patterns->patternShortsEnd(); ++it, ++strategyNumber)
	{
	  auto pattern = it->second;
	  std::string name = "PAL Short Strategy " + std::to_string(strategyNumber);
	  auto strat = std::make_shared<PalShortStrategy<Decimal>>(name, pattern, portfolio);

	  auto bt = BackTesterFactory<Decimal>::getBackTester(
							      baseSecurity->getTimeSeries()->getTimeFrame(),
							      dateRange.getFirstDate(),
							      dateRange.getLastDate());
	  bt->addStrategy(strat);

	  futures.emplace_back(Runner.post([this, bt, strat, strategyNumber, &survivorsMu](){
	    OriginalMCPT<Decimal> mcpt(bt, mNumPermutations);
	    Decimal p = mcpt.runPermutationTest();
	    if (p < DecimalConstants<Decimal>::SignificantPValue)
	      {
		boost::mutex::scoped_lock lk(survivorsMu);
		mSurvivingStrategies.push_back(strat);
		std::cout << "Strategy: " << strategyNumber
			  << " Short Pattern found with p-value < " << p << std::endl;
	      }
	  }));
	}

      // Wait for short‐pattern tasks
      for (auto &f : futures) {
	try { f.get(); }
	catch (const std::exception &e) { std::cerr << "Error: " << e.what() << std::endl; }
      }
    }

  private:
    unsigned long   mNumPermutations;
    std::list<std::shared_ptr<PalStrategy<Decimal>>> mSurvivingStrategies;
  };
  
}

#endif
