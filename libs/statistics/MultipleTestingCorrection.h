// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
// Revised for separation-of-concerns design by ChatGPT.
// Further revised based on unit test debugging.

#ifndef __MULTIPLE_TESTING_CORRECTION_H
#define __MULTIPLE_TESTING_CORRECTION_H 1

#include <list>
#include <map>
#include <vector>
#include <tuple>
#include <algorithm>
#include <iostream>
#include <memory>
#include <boost/thread/mutex.hpp>

#include "number.h"
#include "DecimalConstants.h"
#include "PermutationTestResultPolicy.h"
#include "PalStrategy.h"

namespace mkc_timeseries
{

  //===========================================================================
  // Internal helper functions in the detail namespace.
  //===========================================================================
  namespace detail {

    // Helper function to prepare the container of hypothesis test results and build a sorted empirical null distribution.
    //
    // This function performs two key tasks:
    //   1. It sorts the container of hypothesis tests in ascending order based on the original (raw) p-values.
    //      - Each entry in the container is expected to be a tuple where the element at index 0 is the raw p-value.
    //   2. It constructs the empirical null distribution which is used to compute empirical p-values later.
    //      - The empirical null distribution is built in one of two ways:
    //          a. If a synthetic null distribution is provided (container.hasSyntheticNull() is true),
    //             then that distribution is used directly.
    //          b. Otherwise, the function iterates through the container and extracts a test statistic
    //             (typically the maximum test statistic) from each hypothesis (stored at index 1 in the tuple).
    //      - The resulting null distribution is then sorted.
    //
    // Parameters:
    //   container:
    //     - A container object that encapsulates the hypothesis test results.
    //     - It must provide several member functions:
    //         • getNumStrategies(): Returns the count of hypothesis tests (strategies). If zero, no processing occurs.
    //         • getInternalContainer(): Returns a reference to the container holding the tuples of test results.
    //             Each tuple should at least contain:
    //               - Index 0: the raw (original) p-value.
    //               - Index 1: the test statistic (or maximum test statistic) used for the null distribution.
    //         • hasSyntheticNull(): Indicates whether a synthetic null distribution is available.
    //         • getSyntheticNullDistribution(): Retrieves the synthetic null distribution vector if available.
    //
    //   sortedEmpiricalNullDistribution:
    //     - A reference to a vector that will be populated with the empirical null distribution values.
    //     - These values are either taken directly from a synthetic null distribution or extracted (and then sorted)
    //       from the container's test statistics.
    //
    // Return Value:
    //   - Returns true if:
    //         • The container is non-empty (i.e., it contains at least one hypothesis test),
    //         • And a non-empty sorted empirical null distribution can be built.
    //   - Returns false if the container is empty or if the constructed null distribution is empty.
    //
    // Detailed Process:
    //   a. Check if the container has any hypothesis tests by calling getNumStrategies(). If no tests exist, return false.
    //   b. Obtain a reference to the internal container via getInternalContainer().
    //   c. Sort the internal container in ascending order based on the original p-values (tuples’ element at index 0).
    //      - This sorting ensures that subsequent multiple testing adjustments are performed over an ordered set.
    //   d. Determine how to build the null distribution:
    //         - If a synthetic null distribution is available (hasSyntheticNull() returns true), assign it directly.
    //         - Otherwise, loop over each tuple in the internal container and extract the test statistic
    //           (stored at index 1) to form the null distribution.
    //   e. Sort the resulting null distribution vector in ascending order.
    //   f. Return true if the sorted empirical null distribution is not empty, indicating valid data for subsequent tests.
    template <typename Decimal, typename Container>
    bool prepareContainerAndNull(Container& container, std::vector<Decimal>& sortedEmpiricalNullDistribution) {
      // Verify that the container holds at least one hypothesis test.
      if (container.getNumStrategies() == 0)
        return false;
      
      // Access the internal container holding the hypothesis test result tuples.
      auto& tsContainer = container.getInternalContainer();
      
      // Sort the container based on the raw p-values (the first element of each tuple) in ascending order.
      std::sort(tsContainer.begin(), tsContainer.end(),
                [](const auto& a, const auto& b) {
                  return std::get<0>(a) < std::get<0>(b);
                });
      
      // Build the empirical null distribution:
      // An "empirical null distribution" is a distribution of a test statistic that is generated from
      // data where the null hypothesis is assumed to be true. Rather than relying on
      // theoretical distributions
      // (like the normal or t-distribution), you build the distribution directly from the
      // data (usually via resampling or permutation methods). In the context of these
      // correction methods, the empirical null distribution is created by collecting the
      // test statistics (e.g.max test statistic) from each hypothesis
      // (or permutation), then sorting them.
      //
      // This provides a data‐driven reference against which the observed p-values can be
      // compared during the multiple testing correction process. The approach is often used
      // in permutation tests and has been discussed in detail in works like Romano and Wolf (2005).

      // Check if a synthetic null distribution is provided.
      if (container.hasSyntheticNull())
      {
          // If a synthetic null exists, use it directly.
          sortedEmpiricalNullDistribution = container.getSyntheticNullDistribution();
      }
      else
      {
          // Otherwise, iterate through the container and collect the test statistic
          // from each hypothesis (stored at index 1 in each tuple). This statistic is used for
          // constructing the null distribution.
          for (const auto& entry : tsContainer)
            sortedEmpiricalNullDistribution.push_back(std::get<1>(entry));
      }
      
      // Sort the collected empirical null distribution values in ascending order.
      std::sort(sortedEmpiricalNullDistribution.begin(), sortedEmpiricalNullDistribution.end());
 
      // Return true if the empirical null distribution is non-empty, indicating that we can proceed.
      return !sortedEmpiricalNullDistribution.empty();
    }

    // Helper function to adjust p-values using an empirical null distribution.
    //
    // This function implements a general algorithm for resampling-based p-value adjustment as described in
    // Romano and Wolf (2005, 2016). It supports both step-down and step-up adjustments by iterating over
    // a container of hypothesis test results in reverse or forward order, respectively.
    //
    // Parameters:
    //   container:
    //     A container (e.g., vector of tuples) where each element represents a hypothesis test result.
    //     Each tuple is assumed to have the following structure:
    //       - Index 0: The observed (raw) p-value (which will be replaced with the adjusted p-value).
    //       - Index 1: The observed test statistic or the maximum test statistic (from a permutation),
    //                  used to compute the empirical p-value.
    //       - (Additional indices may be used, e.g., for storing a strategy pointer.)
    //
    //   sortedEmpiricalNullDistribution:
    //     A sorted vector of test statistics generated under the null hypothesis (the empirical null).
    //     This distribution is used to compute empirical p-values by determining the proportion of null
    //     test statistics that are greater than or equal to an observed test statistic.
    //
    //   computeCandidate:
    //     A lambda (or function object) that calculates a candidate for the adjusted p-value. It is
    //     provided with the computed empirical p-value, the current index (or rank), and the total number
    //     of tests. For example, in the Romano-Wolf step-down procedure, the candidate might be computed as:
    //       candidate = empiricalP * (totalTests / (i + 1))
    //
    //   updateMono:
    //     A lambda (or function object) used to enforce monotonicity across the adjusted p-values.
    //     Monotonicity is a requirement such that the sequence of adjusted p-values does not decrease
    //     as you move from more extreme to less extreme test results.
    //     - For step-down corrections (reverse iteration), updateMono returns the minimum of the previous
    //       adjusted p-value and the current candidate (ensuring the new value does not exceed the previous one).
    //     - For step-up corrections (forward iteration), updateMono returns the maximum of the previous
    //       adjusted p-value and the candidate.
    //
    //   reverseOrder:
    //     A boolean flag indicating the direction of iteration:
    //       - true: iterate in reverse order (step-down procedure). This means starting with the most
    //               “extreme” (largest) test statistic and moving to the least extreme.
    //       - false: iterate in forward order (step-up procedure).
    //
    //   initialPreviousAdj:
    //     The starting value for the “previous adjusted p-value” used in the monotonicity enforcement.
    //     Typically, this is set to 1.0 for step-down corrections (to ensure that the first candidate is used
    //     directly) and 0.0 for step-up corrections.
    //
    // Algorithm Overview:
    //   1. Determine the total number of hypothesis tests.
    //   2. Initialize a variable (previousAdjusted) with the starting value (initialPreviousAdj).
    //   3. Depending on the reverseOrder flag, iterate through the container:
    //        - For reverse order (step-down):
    //            a. Loop from the last element (index = totalTests - 1) down to index 0.
    //        - For forward order (step-up):
    //            a. Loop from the first element (index = 0) to the last.
    //
    //   4. For each test in the container:
    //        a. Extract the observed test statistic from the current tuple.
    //        b. Calculate the empirical p-value:
    //             - Use lower_bound on the sorted empirical null distribution to locate the first element
    //               that is not less than the observed test statistic.
    //             - Count how many values in the empirical null are greater than or equal to the observed statistic.
    //             - Divide this count by the total number of values in the null distribution.
    //        c. Calculate a candidate adjusted p-value using the computeCandidate lambda.
    //        d. Enforce monotonicity:
    //             - If this is the first iteration (depending on the order), the candidate is used directly.
    //             - Otherwise, use the updateMono lambda to combine the candidate with the previously adjusted p-value.
    //        e. Update the previous adjusted value and store the adjusted p-value back into the container.
    //
    //   5. After processing all tests, the container’s raw p-values are replaced with adjusted p-values that
    //      incorporate the scaling and monotonicity requirements.
    //
    // Usage Examples:
    //   - In RomanoWolfStepdownCorrection:
    //         • reverseOrder is true (step-down iteration).
    //         • initialPreviousAdj is set to 1.0.
    //         • computeCandidate multiplies the empirical p-value by (totalStrategies / (i + 1)).
    //         • updateMono returns the minimum of the previous adjusted p-value and the candidate.
    //
    //   - In HolmRomanoWolfCorrection:
    //         • reverseOrder is false (step-up iteration).
    //         • initialPreviousAdj is set to 0.0.
    //         • computeCandidate multiplies the empirical p-value by (totalStrategies - i).
    //         • updateMono returns the maximum of the previous adjusted p-value and the candidate.
    //
    // This design, using lambda parameters for key computation steps, makes adjustPValues a flexible helper
    // that can serve various multiple testing correction procedures by simply changing the parameters.
    template <typename Decimal, typename Container, typename FactorFunc, typename MonoFunc>
    void adjustPValues(Container& container,
                       const std::vector<Decimal>& sortedEmpiricalNullDistribution,
                       FactorFunc computeCandidate,
                       MonoFunc updateMono,
                       bool reverseOrder,
                       Decimal initialPreviousAdj) {
      // Determine the total number of hypothesis tests.
      const unsigned int totalStrategies = static_cast<unsigned int>(container.size());

      // Initialize the previous adjusted p-value with the provided starting value.
      Decimal previousAdjusted = initialPreviousAdj;

      // For step-down correction, iterate in reverse (from most extreme to least).
      if (reverseOrder) {
        for (int i = totalStrategies - 1; i >= 0; i--) {
          // Retrieve the observed test statistic for the current hypothesis.
          Decimal observedTestStat = std::get<1>(container[i]);

          // Find the first element in the sorted null distribution that is not less than
	  // the observed statistic.
          // This identifies the number of null distribution values >= the observed test statistic.
          auto lb = std::lower_bound(sortedEmpiricalNullDistribution.begin(),
                                     sortedEmpiricalNullDistribution.end(), observedTestStat);
          Decimal countGreaterEqual = static_cast<Decimal>(std::distance(lb, sortedEmpiricalNullDistribution.end()));

          // Compute the empirical p-value as the proportion of null test statistics
	  // greater than or equal to the observed one.
          Decimal empiricalP = countGreaterEqual / static_cast<Decimal>(sortedEmpiricalNullDistribution.size());

          // Use the computeCandidate function to get a candidate adjusted p-value.
          // Typically, this scales the empirical p-value based on the rank (i.e., the index "i") and the total count.
          Decimal candidate = computeCandidate(empiricalP, i, totalStrategies);

          // Enforce monotonicity:
          //  - For the first iteration (i == totalStrategies - 1), simply assign the candidate.
          //  - For subsequent iterations, combine the candidate with the previous adjusted value using updateMono.
          Decimal adjusted = (i == static_cast<int>(totalStrategies - 1)) ?
                             candidate : updateMono(previousAdjusted, candidate);

          // Update the previous adjusted value.
          previousAdjusted = adjusted;

          // Write the newly computed adjusted p-value back into the container.
          std::get<0>(container[i]) = adjusted;
        }
      } else {
        // For step-up correction, iterate forward (from least extreme to most).
        for (unsigned int i = 0; i < totalStrategies; i++) {
          // Retrieve the observed test statistic for the current hypothesis.
          Decimal observedTestStat = std::get<1>(container[i]);

          // Find the lower bound in the sorted empirical null distribution.
          auto lb = std::lower_bound(sortedEmpiricalNullDistribution.begin(),
                                     sortedEmpiricalNullDistribution.end(), observedTestStat);
          Decimal countGreaterEqual = static_cast<Decimal>(std::distance(lb, sortedEmpiricalNullDistribution.end()));

          // Compute the empirical p-value by dividing by the total number of null samples.
          Decimal empiricalP = countGreaterEqual / static_cast<Decimal>(sortedEmpiricalNullDistribution.size());

          // Calculate a candidate adjusted p-value based on the scaling dictated by computeCandidate.
          Decimal candidate = computeCandidate(empiricalP, i, totalStrategies);

          // Enforce monotonicity:
          //  - For the very first iteration (i == 0), take the candidate as is.
          //  - For subsequent iterations, ensure monotonicity by combining with the previous value using updateMono.
          Decimal adjusted = (i == 0) ? candidate : updateMono(previousAdjusted, candidate);

          // Update the previous adjusted p-value.
          previousAdjusted = adjusted;

          // Save the computed adjusted p-value back to the container.
          std::get<0>(container[i]) = adjusted;
        }
      }
    }

  } // namespace detail

  //===========================================================================
  // Existing common container used by several policies.
  // [BaseStrategyContainer remains unchanged]
  //===========================================================================
  template <class Decimal>
  class BaseStrategyContainer {
  public:
    typedef std::multimap<Decimal, std::shared_ptr<PalStrategy<Decimal>>> SortedStrategyContainer;
    typedef typename SortedStrategyContainer::const_iterator const_iterator;
    typedef typename SortedStrategyContainer::const_reverse_iterator const_reverse_iterator;

    typedef std::list<std::shared_ptr<PalStrategy<Decimal>>> SurvivingStrategyContainer;
    typedef typename SurvivingStrategyContainer::const_iterator surviving_const_iterator;

    BaseStrategyContainer() {}

    void addStrategy(const Decimal& key, std::shared_ptr<PalStrategy<Decimal>> strategy) {
      boost::mutex::scoped_lock Lock(strategiesMutex_);
      sortedStrategies_.insert(std::make_pair(key, strategy));
    }

    size_t getNumStrategies() const {
      boost::mutex::scoped_lock Lock(strategiesMutex_);
      return sortedStrategies_.size();
    }

    void addSurvivingStrategy(std::shared_ptr<PalStrategy<Decimal>> strategy) {
      boost::mutex::scoped_lock Lock(survivingMutex_);
      survivingStrategies_.push_back(strategy);
    }

    size_t getNumSurvivingStrategies() const {
      boost::mutex::scoped_lock Lock(survivingMutex_);
      return survivingStrategies_.size();
    }

    surviving_const_iterator beginSurvivingStrategies() const { return survivingStrategies_.begin(); }
    surviving_const_iterator endSurvivingStrategies() const { return survivingStrategies_.end(); }

    const SortedStrategyContainer& getInternalContainer() const { return sortedStrategies_; }

    //---------------------------------------------------------------
  /// @brief  Clear both the sorted‐p‐value map and the survivors list.
  void clearForNewTest()
    {
      // clear sortedStrategies_
      {
	boost::mutex::scoped_lock lk1(strategiesMutex_);
	sortedStrategies_.clear();
      }
      // clear survivingStrategies_
      {
	boost::mutex::scoped_lock lk2(survivingMutex_);
	survivingStrategies_.clear();
      }
    }
  private:
    SortedStrategyContainer sortedStrategies_;
    SurvivingStrategyContainer survivingStrategies_;
    mutable boost::mutex strategiesMutex_;
    mutable boost::mutex survivingMutex_;
  };


  //===========================================================================
  // Policy: BenjaminiHochbergFdr
  // [Code for BenjaminiHochbergFdr remains unchanged]
  //===========================================================================
  template <class Decimal>
  class BenjaminiHochbergFdr {
  public:
    typedef typename PValueReturnPolicy<Decimal>::ReturnType ReturnType;
    using ConstSurvivingStrategiesIterator = typename BaseStrategyContainer<Decimal>::surviving_const_iterator;

    BenjaminiHochbergFdr()
      : mFalseDiscoveryRate(DecimalConstants<Decimal>::DefaultFDR)
    {}

    void addStrategy(const ReturnType& pValue, std::shared_ptr<PalStrategy<Decimal>> aStrategy) {
      container_.addStrategy(pValue, aStrategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }

    ConstSurvivingStrategiesIterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }

    ConstSurvivingStrategiesIterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests([[maybe_unused]] const Decimal& pValueSignificanceLevel =
				 DecimalConstants<Decimal>::SignificantPValue) {
      auto it = container_.getInternalContainer().rbegin();
      auto itEnd = container_.getInternalContainer().rend();

      Decimal numTests(static_cast<int>(getNumMultiComparisonStrategies()));
      Decimal rank = numTests;
      Decimal criticalValue = 0;

      for (; it != itEnd; ++it) {
        Decimal pValue = it->first;
        criticalValue = (rank / numTests) * mFalseDiscoveryRate;
        // std::cout << "BenjaminiHochbergFdr:: pValue = " << pValue
        //           << ", Critical value = " << criticalValue << std::endl;
        if (pValue < criticalValue)
          break;
        else
          rank = rank - DecimalConstants<Decimal>::DecimalOne;
      }
      for (; it != itEnd; ++it) {
        container_.addSurvivingStrategy(it->second);
      }
      // std::cout << "BenjaminiHochbergFdr::Number of multi comparison strategies = " << getNumMultiComparisonStrategies() << std::endl;
      // std::cout << "BenjaminiHochbergFdr::Number of surviving strategies after correction = " << getNumSurvivingStrategies() << std::endl;
    }

     // Added for monotonicity test helper compatibility
    const typename BaseStrategyContainer<Decimal>::SortedStrategyContainer& getInternalContainer() const {
        return container_.getInternalContainer();
    }

    /// @brief  Reset state in preparation for a fresh run.
    void clearForNewTest()
    {
      container_.clearForNewTest();
    }

  private:
    BaseStrategyContainer<Decimal> container_;
    Decimal mFalseDiscoveryRate;
  };

  //===========================================================================
  // Policy: AdaptiveBenjaminiHochbergYr2000
  // REVISED: Now supports Slope-based, Tail-based, and Storey's Smoothed
  //          estimation methods for m0, selectable via an enum.
  //===========================================================================
  //
  // based on the paper "On the Adaptive Control of the False Discovery Rate in
  // Multiple Testing with Independent Statistics" by Benjamini and Hochberg (2000)
  // with an added estimator inspired by Storey (2002).
  //
  template <class Decimal>
  class AdaptiveBenjaminiHochbergYr2000 {
  public:
    // Enum to select the estimation method for the number of true nulls (m0).
    enum class EstimationMethod {
      SlopeBased,      // Benjamini & Hochberg (2000) slope-based method
      TailBased,       // Benjamini & Hochberg (2000) simple tail-based method
      StoreySmoothed   // Storey (2002) inspired smoother using linear regression
    };

    typedef typename PValueReturnPolicy<Decimal>::ReturnType ReturnType;
    using ConstSurvivingStrategiesIterator = typename BaseStrategyContainer<Decimal>::surviving_const_iterator;

    // Constructor now accepts an enum to select the estimation method.
    // By default, it uses the robust slope-based method.
    explicit AdaptiveBenjaminiHochbergYr2000(EstimationMethod method = EstimationMethod::SlopeBased)
      : m_estimationMethod(method),
	mFalseDiscoveryRate(DecimalConstants<Decimal>::DefaultFDR)
    {}

    void addStrategy(const ReturnType& pValue, std::shared_ptr<PalStrategy<Decimal>> aStrategy) {
      container_.addStrategy(pValue, aStrategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }

    ConstSurvivingStrategiesIterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }

    ConstSurvivingStrategiesIterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests([[maybe_unused]] const Decimal& pValueSignificanceLevel =
				 DecimalConstants<Decimal>::SignificantPValue) {
      if (getNumMultiComparisonStrategies() == 0)
        return; // Nothing to do

      std::cout << "In method AdaptiveBenjaminiHochbergYr2000::correctForMultipleTests" << std::endl;
      Decimal m0_estimate;

      // Use a switch to select the estimation method for m0.
      // This provides a clean and extensible design.
      switch (m_estimationMethod)
	{
	case EstimationMethod::TailBased:
	  m0_estimate = estimateM0TailBased();
	  break;

	case EstimationMethod::StoreySmoothed:
	  m0_estimate = estimateM0StoreySmoothed();
	  break;

	case EstimationMethod::SlopeBased:
	default:
	  m0_estimate = estimateM0SlopeBased();
	  break;
	}

      auto it = container_.getInternalContainer().rbegin();
      auto itEnd = container_.getInternalContainer().rend();

      
      // Now, proceed with the BH procedure using the chosen m0 estimate
      Decimal rank(static_cast<int>(getNumMultiComparisonStrategies()));
      Decimal criticalValue = DecimalConstants<Decimal>::DecimalZero;

      Decimal mPrime = std::min(Decimal(rank), m0_estimate * rank);

      std::string estimatorLabel = (m_estimationMethod ==  EstimationMethod::StoreySmoothed)
	? "Storey Smoother"
	: "Tail Single Lambda";
      
      std::cout << "[" << estimatorLabel << "] pi0 = " << m0_estimate
		<< ", mPrime = " << mPrime << ", m = " << rank << "\n";
      std::cout << "AdaptiveBenjaminiHochbergYr2000::Rank = " << rank << std::endl;

      for (; it != itEnd; ++it)
	{
	  Decimal pValue = it->first;
	  criticalValue = (rank / m0_estimate) * mFalseDiscoveryRate;

	  std::cout << "pValue = " << pValue << ", criticalValue = " << criticalValue << std::endl;
	  if (pValue < criticalValue)
	    break;
	  else
	    rank = rank - DecimalConstants<Decimal>::DecimalOne;
	}
    
      // Add all strategies from the break-point onwards (i.e., those that passed the test)

      for (; it != itEnd; ++it)
	{
	  std::cout << "In method AdaptiveBenjaminiHochbergYr2000::correctForMultipleTests adding surviving strategies" << std::endl;
	  container_.addSurvivingStrategy(it->second);
	}
    }

    // (Other public methods like getInternalContainer, getAllTestedStrategies, etc. remain unchanged)
    const typename BaseStrategyContainer<Decimal>::SortedStrategyContainer& getInternalContainer() const {
      return container_.getInternalContainer();
    }
    std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> getAllTestedStrategies() const {
      std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> result;
      for (const auto& entry : container_.getInternalContainer()) {
	result.emplace_back(entry.second, entry.first); // strategy, p-value
      }
      return result;
    }
    Decimal getStrategyPValue(std::shared_ptr<PalStrategy<Decimal>> strategy) const {
      for (const auto& entry : container_.getInternalContainer()) {
	if (entry.second == strategy) {
	  return entry.first; // return p-value
	}
      }
      return Decimal(1.0); // Default high p-value if not found
    }
    void clearForNewTest()
    {
      container_.clearForNewTest();
    }

  private:
    // Method 1: B&H (2000) Simple Tail-Based Estimator
    Decimal estimateM0TailBased() const {
      std::cout << "In method AdaptiveBenjaminiHochbergYr2000::estimateM0TailBased" << std::endl;
      const auto& strategies = container_.getInternalContainer();
      const size_t m = strategies.size();
      if (m == 0) return Decimal(0);

      const Decimal lambda = DecimalConstants<Decimal>::createDecimal("0.5");
      size_t count = std::count_if(strategies.begin(), strategies.end(),
				   [lambda](const auto& pair) {
				     return pair.first > lambda;
				   });
    
      Decimal pi0_hat = static_cast<Decimal>(count) / ((DecimalConstants<Decimal>::DecimalOne - lambda) * static_cast<Decimal>(m));
      Decimal m0_hat = std::min(DecimalConstants<Decimal>::DecimalOne, pi0_hat) * static_cast<Decimal>(m);
      return std::max(DecimalConstants<Decimal>::DecimalOne, m0_hat);
    }

    // Method 2: B&H (2000) Slope-Based Estimator
    Decimal estimateM0SlopeBased() {
      calculateSlopes(); // Note: This has a side effect of populating mSlopes
      Decimal m(static_cast<int>(getNumMultiComparisonStrategies()));
      for (unsigned int i = 1; i < mSlopes.size(); i++) {
	if (mSlopes[i] < mSlopes[i - 1]) {
	  if (mSlopes[i] <= DecimalConstants<Decimal>::DecimalZero) continue; // Avoid division by zero/negative
	  Decimal temp = (DecimalConstants<Decimal>::DecimalOne / mSlopes[i]) + DecimalConstants<Decimal>::DecimalOne;
	  return std::min(temp, m);
	}
      }
      return m;
    }
  
    // Method 3: Storey (2002) Inspired Smoother with Linear Regression
    Decimal estimateM0StoreySmoothed() const {
      std::cout << "In method AdaptiveBenjaminiHochbergYr2000::estimateM0StoreySmoothed" << std::endl;
      const auto& strategies = container_.getInternalContainer();
      const size_t m = strategies.size();
      if (m < 2) return Decimal(m); // Need at least 2 points for regression

      std::vector<Decimal> lambdas;
      std::vector<Decimal> pi0s;

      // Use Decimal type throughout for consistency
      for (Decimal lambda = DecimalConstants<Decimal>::createDecimal("0.25");
           lambda < DecimalConstants<Decimal>::createDecimal("0.60");
           lambda += DecimalConstants<Decimal>::createDecimal("0.05")) {
        lambdas.push_back(lambda);
        size_t count = std::count_if(strategies.begin(), strategies.end(),
                                     [lambda](const auto& entry) {
				       return entry.first > lambda;
                                     });
        Decimal pi0 = static_cast<Decimal>(count) / ((DecimalConstants<Decimal>::DecimalOne - lambda) * static_cast<Decimal>(m));
        pi0s.push_back(std::min(DecimalConstants<Decimal>::DecimalOne, pi0));

	std::cout << "[StoreySmoother] lambda = " << lambda
                  << ", m0(lambda) = " << pi0s.back() << std::endl;
      }

      if (lambdas.empty()) return Decimal(m);

      // Linear regression: pi0 ≈ a + b * lambda
      Decimal sumX = DecimalConstants<Decimal>::DecimalZero,
              sumY = DecimalConstants<Decimal>::DecimalZero,
              sumXY = DecimalConstants<Decimal>::DecimalZero,
              sumXX = DecimalConstants<Decimal>::DecimalZero;
      size_t n = lambdas.size();
      for (size_t i = 0; i < n; ++i) {
        sumX += lambdas[i];
        sumY += pi0s[i];
        sumXY += lambdas[i] * pi0s[i];
        sumXX += lambdas[i] * lambdas[i];
      }

      Decimal denom = static_cast<Decimal>(n) * sumXX - sumX * sumX;
      if (denom == DecimalConstants<Decimal>::DecimalZero) return Decimal(m); // Avoid division by zero

      Decimal slope = (static_cast<Decimal>(n) * sumXY - sumX * sumY) / denom;
      Decimal intercept = (sumY - slope * sumX) / static_cast<Decimal>(n);

      // ==> LOGGING: Log the final regression parameters
      std::cout << "[StoreySmoother] Linear regression: slope = " << slope
                << ", intercept = " << intercept << std::endl;

      // ==> LOGGING: Log residuals for each lambda
      std::cout << "--- Residual Analysis ---" << std::endl;
      for (size_t i = 0; i < n; ++i) {
        Decimal fitted = slope * lambdas[i] + intercept;
        Decimal actual = pi0s[i];
        Decimal residual = actual - fitted;
        std::cout << "[StoreySmoother] lambda = " << lambdas[i]
                  << ", fitted = " << fitted
                  << ", actual = " << actual
                  << ", residual = " << residual << std::endl;
      }
      std::cout << "-------------------------" << std::endl;
      
      // Extrapolate pi0 at lambda = 1.0 and clamp between 0 and 1
      Decimal extrapolatedPi0 = std::max(DecimalConstants<Decimal>::DecimalZero, std::min(DecimalConstants<Decimal>::DecimalOne, intercept + slope));
    
      // Convert pi0 proportion estimate to m0 count estimate
      Decimal m0_hat = extrapolatedPi0 * static_cast<Decimal>(m);

      // ==> LOGGING: Log the final extrapolated results
      std::cout << "[StoreySmoother] Smoothed pi0 = " << extrapolatedPi0
                << ", mPrime = " << m0_hat
                << ", m = " << static_cast<Decimal>(m) << std::endl;
      return std::max(DecimalConstants<Decimal>::DecimalOne, m0_hat);
    }

    void calculateSlopes() {
      mSlopes.clear();
      // ... (implementation of calculateSlopes is unchanged)
      if (getNumMultiComparisonStrategies() > 0) {
	Decimal m(static_cast<int>(getNumMultiComparisonStrategies()));
	Decimal i = DecimalConstants<Decimal>::DecimalOne;
      
	for (auto it = container_.getInternalContainer().begin(); it != container_.getInternalContainer().end(); ++it) {
	  Decimal pValue = it->first;
	  Decimal num = (DecimalConstants<Decimal>::DecimalOne - pValue);
	  Decimal denom = (m + DecimalConstants<Decimal>::DecimalOne - i);
	  Decimal slope = (denom > 0) ? (num / denom) : DecimalConstants<Decimal>::DecimalZero; // Avoid division by zero
	  mSlopes.push_back(slope);
	  i = i + DecimalConstants<Decimal>::DecimalOne;
	}
      }
    }

    BaseStrategyContainer<Decimal> container_;
    std::vector<Decimal> mSlopes;

    // Configuration member
    EstimationMethod m_estimationMethod;
    Decimal mFalseDiscoveryRate;
  };

  //===========================================================================
  // Policy: UnadjustedPValueStrategySelection
  // [Code for UnadjustedPValueStrategySelection remains unchanged]
  //===========================================================================
  template <class Decimal>
  class UnadjustedPValueStrategySelection {
  public:
    typedef typename PValueReturnPolicy<Decimal>::ReturnType ReturnType;
    using ConstSurvivingStrategiesIterator = typename BaseStrategyContainer<Decimal>::surviving_const_iterator;

    UnadjustedPValueStrategySelection() {}

    void addStrategy(const ReturnType& pValue, std::shared_ptr<PalStrategy<Decimal>> aStrategy) {
      container_.addStrategy(pValue, aStrategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }

    ConstSurvivingStrategiesIterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }

    ConstSurvivingStrategiesIterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests(const Decimal& pValueSignificanceLevel =
				 DecimalConstants<Decimal>::SignificantPValue)
    {
      for (auto const& entry : container_.getInternalContainer())
	{
	  const auto& pValue = entry.first;
	  const auto& strategy = entry.second;

	  if (pValue <= pValueSignificanceLevel)
	    container_.addSurvivingStrategy(strategy);
	}
    }

    // Added for monotonicity test helper compatibility
    const typename BaseStrategyContainer<Decimal>::SortedStrategyContainer& getInternalContainer() const {
        return container_.getInternalContainer();
    }

    // New methods for accessing all tested strategies and their p-values
    std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> getAllTestedStrategies() const {
        std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> result;
        for (const auto& entry : container_.getInternalContainer()) {
            result.emplace_back(entry.second, entry.first); // strategy, p-value
        }
        return result;
    }

    Decimal getStrategyPValue(std::shared_ptr<PalStrategy<Decimal>> strategy) const {
        for (const auto& entry : container_.getInternalContainer()) {
            if (entry.second == strategy) {
                return entry.first; // return p-value
            }
        }
        return Decimal(1.0); // Default high p-value if not found
    }

    /// @brief  Reset state in preparation for a fresh run.
    void clearForNewTest()
    {
      container_.clearForNewTest();
    }

  private:
    BaseStrategyContainer<Decimal> container_;
  };


  //===========================================================================
  // New helper class for test-statistic–based corrections.
  // [TestStatisticStrategyImplementation remains unchanged]
  //===========================================================================
  template <class Decimal>
  class TestStatisticStrategyImplementation {
  public:
    typedef std::vector<std::tuple<Decimal, Decimal, std::shared_ptr<PalStrategy<Decimal>>>> TestStatisticContainer;
    typedef typename TestStatisticContainer::const_iterator const_iterator;
    typedef std::list<std::shared_ptr<PalStrategy<Decimal>>> SurvivingStrategyContainer;
    typedef typename SurvivingStrategyContainer::const_iterator surviving_const_iterator;

    TestStatisticStrategyImplementation() {}

    void addStrategy(const Decimal& pValue, const Decimal& maxTestStat, std::shared_ptr<PalStrategy<Decimal>> strategy) {
      boost::mutex::scoped_lock Lock(mutex_);
      testStatisticStrategies_.emplace_back(pValue, maxTestStat, strategy);
    }

    // New method to mark surviving strategies based on an adjusted p-value threshold.
    void markSurvivingStrategies(Decimal significanceThreshold = DecimalConstants<Decimal>::SignificantPValue)
    {
      boost::mutex::scoped_lock Lock(mutex_);
      for (const auto& tup : testStatisticStrategies_)
	{
	  // std::get<0> holds the adjusted p-value.
	  if (std::get<0>(tup) <= significanceThreshold)
	    survivingStrategies_.push_back(std::get<2>(tup));
	}
    }
    
    size_t getNumStrategies() const {
      boost::mutex::scoped_lock Lock(mutex_);
      return testStatisticStrategies_.size();
    }

    void addSurvivingStrategy(std::shared_ptr<PalStrategy<Decimal>> strategy) {
      boost::mutex::scoped_lock Lock(mutex_);
      survivingStrategies_.push_back(strategy);
    }

    size_t getNumSurvivingStrategies() const {
      boost::mutex::scoped_lock Lock(mutex_);
      return survivingStrategies_.size();
    }

    surviving_const_iterator beginSurvivingStrategies() const { return survivingStrategies_.begin(); }
    surviving_const_iterator endSurvivingStrategies() const { return survivingStrategies_.end(); }

    // Returns a reference to the internal container.
    TestStatisticContainer& getInternalContainer() { return testStatisticStrategies_; }
    const TestStatisticContainer& getInternalContainer() const { return testStatisticStrategies_; } // Added const version

    // Methods added for unit testing

    void setSyntheticNullDistribution(const std::vector<Decimal>& syntheticNull) {
      boost::mutex::scoped_lock Lock(mutex_);
      syntheticNullDistribution_ = syntheticNull;
      hasSyntheticNull_ = true;
    }

    bool hasSyntheticNull() const {
      return hasSyntheticNull_;
    }

    const std::vector<Decimal>& getSyntheticNullDistribution() const {
      // No lock needed if syntheticNullDistribution_ is only written in setSyntheticNullDistribution
      // and read here after potential write. Assume setSyntheticNullDistribution happens before read.
      return syntheticNullDistribution_;
    }

    //---------------------------------------------------------------
    /// @brief  Reset this container to its pristine state.
    ///         Clears any added strategies, surviving strategies, and synthetic nulls.
    void clearForNewTest()
    {
      // clear raw/test‐stat entries
      {
        boost::mutex::scoped_lock lk(mutex_);
        testStatisticStrategies_.clear();
      }
      // clear survivors
      {
        boost::mutex::scoped_lock lk(mutex_);
        survivingStrategies_.clear();
      }
      // reset any synthetic null
      {
        boost::mutex::scoped_lock lk(mutex_);
        syntheticNullDistribution_.clear();
        hasSyntheticNull_ = false;
      }
    }

  private:
    TestStatisticContainer testStatisticStrategies_;
    SurvivingStrategyContainer survivingStrategies_;
    mutable boost::mutex mutex_;
    std::vector<Decimal> syntheticNullDistribution_;
    bool hasSyntheticNull_ = false;
  };

  template <class Decimal>
  class StrategyBaselineResultContainer
  {
  public:
    // The tuple now holds: < baselineStat, shared_ptr_to_strategy >
    using InternalContainer = std::vector<std::tuple<Decimal, std::shared_ptr<PalStrategy<Decimal>>>>;
    using surviving_const_iterator = typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator;

    StrategyBaselineResultContainer() {}

    void addStrategy(const Decimal& baselineStat, std::shared_ptr<PalStrategy<Decimal>> strategy)
    {
      boost::mutex::scoped_lock Lock(mutex_);
      internal_container_.emplace_back(baselineStat, strategy);
    }

    void markSurvivingStrategies(const std::vector<std::tuple<Decimal, std::shared_ptr<PalStrategy<Decimal>>>>& adjusted_p_values,
                                 Decimal significanceThreshold)
    {
      boost::mutex::scoped_lock Lock(mutex_);
      survivingStrategies_.clear();
      for (const auto& tup : adjusted_p_values) {
        if (std::get<0>(tup) <= significanceThreshold) {
	  survivingStrategies_.push_back(std::get<1>(tup));
        }
      }
    }
    
    InternalContainer& getInternalContainer() { return internal_container_; }
    size_t getNumStrategies() const
    {
      boost::mutex::scoped_lock Lock(mutex_);
      return internal_container_.size();
    }

    surviving_const_iterator beginSurvivingStrategies() const { return survivingStrategies_.begin(); }
    surviving_const_iterator endSurvivingStrategies() const { return survivingStrategies_.end(); }
    
    size_t getNumSurvivingStrategies() const
    {
      boost::mutex::scoped_lock Lock(mutex_);
      return survivingStrategies_.size();
    }
    
    void clearForNewTest()
    {
      boost::mutex::scoped_lock lk(mutex_);
      internal_container_.clear();
      survivingStrategies_.clear();
    }

  private:
    InternalContainer internal_container_;
    std::list<std::shared_ptr<PalStrategy<Decimal>>> survivingStrategies_;
    mutable boost::mutex mutex_;
  };


  //
// In MultipleTestingCorrection.h

template <class Decimal>
class RomanoWolfStepdownCorrection {
public:
    using FullResultType = std::tuple<Decimal, Decimal, Decimal>; // pValue, baselineStat, maxPermutedStat
    using ConstSurvivingStrategiesIterator = typename StrategyBaselineResultContainer<Decimal>::surviving_const_iterator;
    using StrategyPair = std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>;

    RomanoWolfStepdownCorrection() : m_isSyntheticNull(false) {}

    // This method accepts the full result from each permutation test.
    void addStrategy(const FullResultType& result, std::shared_ptr<PalStrategy<Decimal>> strategy) 
    {
      const Decimal& maxPermutedStat = std::get<1>(result); // The statistic for the null distribution
      const Decimal& baselineStat    = std::get<2>(result); // The statistic to be tested

      container_.addStrategy(baselineStat, strategy);
      
      // Only add to the empirical null if a synthetic one hasn't been provided.
      if (!m_isSyntheticNull) {
          empiricalNullDistribution_.push_back(maxPermutedStat);
      }
    }

    // Method to allow injection of a pre-computed null distribution, e.g., for testing.
    void setSyntheticNullDistribution(const std::vector<Decimal>& syntheticNull)
    {
        empiricalNullDistribution_ = syntheticNull;
        m_isSyntheticNull = !syntheticNull.empty();
    }

    // Returns the total number of strategies being evaluated.
    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }
    
    // Returns a vector of pairs, each containing a strategy and its final adjusted p-value.
    std::vector<StrategyPair> getAllTestedStrategies() const {
        return m_final_p_values;
    }

    // Looks up a specific strategy and returns its final adjusted p-value.
    Decimal getStrategyPValue(std::shared_ptr<PalStrategy<Decimal>> strategy) const {
        for (const auto& entry : m_final_p_values) {
            if (entry.first == strategy) {
                return entry.second; // return adjusted p-value
            }
        }
        return Decimal(1.0); // Default: not found or did not pass
    }

    void correctForMultipleTests(const Decimal& pValueSignificanceLevel = DecimalConstants<Decimal>::SignificantPValue)
    {
        // Throw an exception if called with no data, to match the test's expectation.
        if (container_.getNumStrategies() == 0)
            throw std::runtime_error("RomanoWolfStepdownCorrection: No strategies added for multiple testing correction.");

        if (empiricalNullDistribution_.empty())
            throw std::runtime_error("RomanoWolfStepdownCorrection: Empirical null distribution is empty.");

        auto& strategyResults = container_.getInternalContainer();

        // Sort strategies by baseline stat (descending)
        std::sort(strategyResults.begin(), strategyResults.end(),
                  [](const auto& a, const auto& b) {
                      return std::get<0>(a) > std::get<0>(b); 
                  });

        std::sort(empiricalNullDistribution_.begin(), empiricalNullDistribution_.end());

        std::vector<StrategyPair> temp_p_values;
        Decimal last_p_adj = DecimalConstants<Decimal>::DecimalZero;

        for (const auto& result_tuple : strategyResults) {
            const Decimal& baselineStat = std::get<0>(result_tuple);
            const auto& strategy = std::get<1>(result_tuple);

            auto lb = std::lower_bound(empiricalNullDistribution_.begin(), empiricalNullDistribution_.end(), baselineStat);
            Decimal countGreaterEqual = static_cast<Decimal>(std::distance(lb, empiricalNullDistribution_.end()));
            Decimal p_raw = countGreaterEqual / static_cast<Decimal>(empiricalNullDistribution_.size());
            
            Decimal p_adj = std::max(last_p_adj, p_raw);
            
            temp_p_values.emplace_back(strategy, p_adj); // Storing as <strategy, p_value>
            last_p_adj = p_adj;
        }

        // Store the final adjusted p-values in the member variable
        m_final_p_values = temp_p_values;
        
        // Create a temporary structure for the container to mark survivors
        std::vector<std::tuple<Decimal, std::shared_ptr<PalStrategy<Decimal>>>> pvals_for_marking;
        for (const auto& p : m_final_p_values) {
            pvals_for_marking.emplace_back(p.second, p.first);
        }

        container_.markSurvivingStrategies(pvals_for_marking, pValueSignificanceLevel);
    }
    
    ConstSurvivingStrategiesIterator beginSurvivingStrategies() const { return container_.beginSurvivingStrategies(); }
    ConstSurvivingStrategiesIterator endSurvivingStrategies() const { return container_.endSurvivingStrategies(); }
    size_t getNumSurvivingStrategies() const { return container_.getNumSurvivingStrategies(); }
    
    void clearForNewTest()
    {
      container_.clearForNewTest();
      empiricalNullDistribution_.clear();
      m_isSyntheticNull = false;
      m_final_p_values.clear();
    }

private:
    StrategyBaselineResultContainer<Decimal> container_;
    std::vector<Decimal> empiricalNullDistribution_; 
    bool m_isSyntheticNull;
    // New member to store the final results for the getter methods
    std::vector<StrategyPair> m_final_p_values;
};
  //
  
  //===========================================================================
  // Policy: RomanoWolfStepdownCorrection
  // Revised empirical p-value calculation.
  //===========================================================================
  // Revised to compute the stepdown adjustment in reverse order as described by Romano and Wolf.
  // This policy now uses composition to delegate container management to
  // TestStatisticStrategyImplementation.
  //
  // References:
  //  Romano, J. P. & Wolf, M. (2005). Exact and approximate stepdown methods for multiple hypothesis testing.
  //   Journal of the American Statistical Association, 100(469), 94-108.
  //
  // Romano, J. P. & Wolf, M. (2016). Efficient computation of adjusted p-values for resampling-based
  // stepdown multiple testing.
  //
  // This paper presents efficient algorithms for computing adjusted p-values using resampling-based stepdown methods.
  // The implementation in the RomanoWolfStepdownCorrection class, particularly in the correctForMultipleTests() method,
  // is directly inspired by the techniques described in this paper.
  // - Additional literature on permutation-based adjustments.
  //
  //

  template <class Decimal>
  class RomanoWolfStepdownCorrection2 {
  public:
    typedef typename PValueAndTestStatisticReturnPolicy<Decimal>::ReturnType RomanoWolfReturnType;
    typedef typename TestStatisticStrategyImplementation<Decimal>::TestStatisticContainer TestStatisticContainer;
    using ConstSurvivingStrategiesIterator = typename TestStatisticStrategyImplementation<Decimal>::surviving_const_iterator;

    RomanoWolfStepdownCorrection2() {}

    void addStrategy(const RomanoWolfReturnType& result, std::shared_ptr<PalStrategy<Decimal>> strategy) {
      Decimal pValue = std::get<0>(result);
      Decimal maxTestStat = std::get<1>(result);

      std::cout << "Romanowolf:: Strategy being added with p-value = " << pValue << ", Max Test Stat = " << maxTestStat << std::endl;
      container_.addStrategy(pValue, maxTestStat, strategy);
    }

    void setSyntheticNullDistribution(const std::vector<Decimal>& syntheticNull)
    {
      container_.setSyntheticNullDistribution(syntheticNull);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }

    ConstSurvivingStrategiesIterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }

    ConstSurvivingStrategiesIterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }

    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    // Added for monotonicity test helper compatibility
    const TestStatisticContainer& getInternalContainer() const {
        return container_.getInternalContainer();
    }

    // New methods for accessing all tested strategies and their p-values
    std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> getAllTestedStrategies() const {
        std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> result;
        for (const auto& entry : container_.getInternalContainer()) {
            result.emplace_back(std::get<2>(entry), std::get<0>(entry)); // strategy, adjusted p-value
        }
        return result;
    }

    Decimal getStrategyPValue(std::shared_ptr<PalStrategy<Decimal>> strategy) const {
        for (const auto& entry : container_.getInternalContainer()) {
            if (std::get<2>(entry) == strategy) {
                return std::get<0>(entry); // return adjusted p-value
            }
        }
        return Decimal(1.0); // Default high p-value if not found
    }

    void correctForMultipleTests(const Decimal& pValueSignificanceLevel =
     DecimalConstants<Decimal>::SignificantPValue) {
      if (container_.getNumStrategies() == 0)
 throw std::runtime_error("RomanoWolfStepdownCorrection: No strategies added for multiple testing correction.");
      
      std::vector<Decimal> sortedEmpiricalNullDistribution;

      if (!detail::prepareContainerAndNull(container_, sortedEmpiricalNullDistribution)) {
        std::cerr << "Warning: RomanoWolfStepdownCorrection - Empty container or null distribution." << std::endl;
        return;
      }

      auto& tsContainer = container_.getInternalContainer();
      const unsigned int totalStrategies = static_cast<unsigned int>(tsContainer.size());

      // Use reverse iteration (step-down) with initial previous-adjusted value of 1.0.
      detail::adjustPValues<Decimal>(
          tsContainer,
          sortedEmpiricalNullDistribution,
          // Lambda: candidate = empiricalP * (totalStrategies/(i+1))
          [totalStrategies](const Decimal& empiricalP, int idx, unsigned int) -> Decimal {
             return empiricalP * (static_cast<Decimal>(totalStrategies) / static_cast<Decimal>(idx + 1));
          },
          // Lambda: enforce monotonicity by taking the minimum
          [](const Decimal& previous, const Decimal& candidate) -> Decimal {
             return std::min(previous, candidate);
          },
          true,      // reverse order iteration (step-down)
          Decimal(1) // initial previous-adjusted p-value is 1.0
      );

      // Mark surviving strategies (adjusted p-value below significance threshold)
      container_.markSurvivingStrategies(pValueSignificanceLevel);
    }

    /// @brief  Reset state in preparation for a fresh run.
    void clearForNewTest()
    {
      container_.clearForNewTest();
    }

  private:
    TestStatisticStrategyImplementation<Decimal> container_;
  };


  //===========================================================================
  // Policy: HolmRomanoWolfCorrection
  // Revised empirical p-value calculation.
  //===========================================================================
  // Implements the Holm-Romano-Wolf stepdown procedure.
  // This procedure first computes the Romano-Wolf empirical p-values,
  // then applies Holm’s sequential adjustment.
  // This policy now uses composition via TestStatisticStrategyImplementation.
  //
  // References:
  // - Holm, S. (1979). A simple sequentially rejective multiple test procedure.
  //   Scandinavian Journal of Statistics, 6(2), 65-70.
  // - Romano, J. P. & Wolf, M. (2005). Exact and approximate stepdown methods for multiple hypothesis testing.
  //   Journal of the American Statistical Association, 100(469), 94-108.
  template <class Decimal>
  class HolmRomanoWolfCorrection {
  public:
    typedef typename PValueAndTestStatisticReturnPolicy<Decimal>::ReturnType HolmRomanoWolfReturnType;
    typedef typename TestStatisticStrategyImplementation<Decimal>::TestStatisticContainer TestStatisticContainer;
    using ConstSurvivingStrategiesIterator = typename TestStatisticStrategyImplementation<Decimal>::surviving_const_iterator;

    HolmRomanoWolfCorrection() {}

    void addStrategy(const HolmRomanoWolfReturnType& result, std::shared_ptr<PalStrategy<Decimal>> strategy) {
      Decimal pValue = std::get<0>(result);
      Decimal maxTestStat = std::get<1>(result);
      container_.addStrategy(pValue, maxTestStat, strategy);
    }

    void setSyntheticNullDistribution(const std::vector<Decimal>& syntheticNull)
    {
      container_.setSyntheticNullDistribution(syntheticNull);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }

    ConstSurvivingStrategiesIterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }

    ConstSurvivingStrategiesIterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    // Added for monotonicity test helper compatibility
    const TestStatisticContainer& getInternalContainer() const {
        return container_.getInternalContainer();
    }

    void correctForMultipleTests(const Decimal& pValueSignificanceLevel =
				 DecimalConstants<Decimal>::SignificantPValue) {
      if (container_.getNumStrategies() == 0)
	throw std::runtime_error("HolmRomanoWolfCorrection: No strategies added for multiple testing correction.");

      std::vector<Decimal> sortedEmpiricalNullDistribution;
      if (!detail::prepareContainerAndNull(container_, sortedEmpiricalNullDistribution)) {
        std::cerr << "Warning: HolmRomanoWolfCorrection - Empty container or null distribution." << std::endl;
        return;
      }

      auto& tsContainer = container_.getInternalContainer();
      const unsigned int totalStrategies = static_cast<unsigned int>(tsContainer.size());

      // Use forward iteration (step-up) with initial previous-adjusted value of 0.0.
      detail::adjustPValues<Decimal>(
          tsContainer,
          sortedEmpiricalNullDistribution,
          // Lambda: candidate = empiricalP * (totalStrategies - i)
          [totalStrategies](const Decimal& empiricalP, int idx, unsigned int) -> Decimal {
             return empiricalP * static_cast<Decimal>(totalStrategies - idx);
          },
          // Lambda: enforce monotonicity by taking the maximum
          [](const Decimal& previous, const Decimal& candidate) -> Decimal {
             return std::max(previous, candidate);
          },
          false,     // forward order iteration (step-up)
          Decimal(0) // initial previous-adjusted p-value is 0.0
      );

      // Mark surviving strategies (adjusted p-value below significance threshold)
      container_.markSurvivingStrategies(pValueSignificanceLevel);
    }

    /// @brief  Reset state in preparation for a fresh run.
    void clearForNewTest()
    {
      container_.clearForNewTest();
    }

  private:
    TestStatisticStrategyImplementation<Decimal> container_;
  };

} // namespace mkc_timeseries

#endif // __MULTIPLE_TESTING_CORRECTION_H
