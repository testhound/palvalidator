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
#include <numeric>
#include <cmath>
#include <limits>
#include <iomanip>
#include <optional>
#include <boost/thread/mutex.hpp>
#include "libalglib/interpolation.h"
#include "number.h"
#include "DecimalConstants.h"
#include "PermutationTestResultPolicy.h"
#include "PalStrategy.h"
#include "TimeSeriesIndicators.h"

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
     DecimalConstants<Decimal>::SignificantPValue,
     [[maybe_unused]] bool partitionByFamily = false) {
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
  //
  // based on the paper "On the Adaptive Control of the False Discovery Rate in
  // Multiple Testing with Independent Statistics" by Benjamini and Hochberg (2000)
  // with an added estimator inspired by
  // "A direct approach to false discovery rates" Storey (2002).
  //
  template <class Decimal>
  class AdaptiveBenjaminiHochbergYr2000 {
  public:
    typedef typename PValueReturnPolicy<Decimal>::ReturnType ReturnType;
    using ConstSurvivingStrategiesIterator = typename BaseStrategyContainer<Decimal>::surviving_const_iterator;

    // Constructor now accepts an enum to select the estimation method.
    // By default, it uses the robust slope-based method.
    explicit AdaptiveBenjaminiHochbergYr2000(const Decimal& falseDiscoveryRate = DecimalConstants<Decimal>::DefaultFDR)
      : mFalseDiscoveryRate(falseDiscoveryRate)
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
     DecimalConstants<Decimal>::SignificantPValue,
     bool partitionByFamily = false)
    {
      if (getNumMultiComparisonStrategies() == 0)
        return;

      std::cout << "In method AdaptiveBenjaminiHochbergYr2000::correctForMultipleTests" << std::endl;
      std::cout << "pValueSignificanceLevel = " << pValueSignificanceLevel << std::endl;
      std::cout << "mFalseDiscoveryRate = " << mFalseDiscoveryRate << std::endl;
      std::cout << "partitionByFamily parameter = " << (partitionByFamily ? "true" : "false") << std::endl;
      if (partitionByFamily) {
        std::cout << "Family partitioning enabled: Long and Short strategies will be corrected separately" << std::endl;
      }

      if (partitionByFamily) {
        // Family partitioning path: partition strategies and apply corrections separately
        const auto& allStrategies = container_.getInternalContainer();
        
        // Partition strategies by family (Long vs Short)
        std::vector<std::pair<Decimal, std::shared_ptr<PalStrategy<Decimal>>>> longStrategies, shortStrategies;
        
        for (const auto& entry : allStrategies) {
          const auto& strategy = entry.second;
          
          // Determine family based on strategy type using PalStrategy methods
          if (strategy->isLongStrategy()) {
            longStrategies.emplace_back(entry.first, entry.second);
          } else if (strategy->isShortStrategy()) {
            shortStrategies.emplace_back(entry.first, entry.second);
          }
        }
        
        std::cout << "Family partitioning: " << longStrategies.size()
                  << " Long strategies, " << shortStrategies.size()
                  << " Short strategies" << std::endl;
        
        // Apply corrections separately to each family
        if (!longStrategies.empty()) {
          std::cout << "Processing Long family..." << std::endl;
          processFamily(longStrategies, pValueSignificanceLevel, "Long");
        }
        
        if (!shortStrategies.empty()) {
          std::cout << "Processing Short family..." << std::endl;
          processFamily(shortStrategies, pValueSignificanceLevel, "Short");
        }
      } else {
        // Unified correction path: treat all strategies as one family
        const auto& allStrategies = container_.getInternalContainer();
        std::vector<std::pair<Decimal, std::shared_ptr<PalStrategy<Decimal>>>> unifiedFamily;
        
        // Convert container format to family format
        for (const auto& entry : allStrategies) {
          unifiedFamily.emplace_back(entry.first, entry.second);
        }
        
        std::cout << "Unified correction: processing all " << unifiedFamily.size() << " strategies together" << std::endl;
        processFamily(unifiedFamily, pValueSignificanceLevel, "Unified");
      }
    }

    const typename BaseStrategyContainer<Decimal>::SortedStrategyContainer& getInternalContainer() const {
      return container_.getInternalContainer();
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>>
    getAllTestedStrategies() const
    {
      std::vector<std::pair<std::shared_ptr<PalStrategy<Decimal>>, Decimal>> result;
      for (const auto& entry : container_.getInternalContainer()) {
	result.emplace_back(entry.second, entry.first); // strategy, p-value
      }
      return result;
    }
    
    Decimal getStrategyPValue(std::shared_ptr<PalStrategy<Decimal>> strategy) const
    {
      for (const auto& entry : container_.getInternalContainer())
	{
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
    // Helper method to process a single family (Long or Short strategies)
    void processFamily(const std::vector<std::pair<Decimal, std::shared_ptr<PalStrategy<Decimal>>>>& familyStrategies,
                       const Decimal& pValueSignificanceLevel,
                       const std::string& familyName)
    {
      if (familyStrategies.empty()) return;
      
      std::cout << "Processing " << familyName << " family with " << familyStrategies.size() << " strategies" << std::endl;
      
      // Estimate m0 for this family
      Decimal m0_estimate;
      if (m_test_m0_override.has_value()) {
        m0_estimate = m_test_m0_override.value();
      } else {
        const size_t m = familyStrategies.size();
        
        // Define a threshold for what constitutes a "high" p-value for our dynamic check.
        const Decimal high_p_value_threshold = DecimalConstants<Decimal>::createDecimal("0.8");
        size_t high_p_value_count = std::count_if(familyStrategies.begin(), familyStrategies.end(),
                                                  [high_p_value_threshold](const auto& pair) {
                                                    return pair.first > high_p_value_threshold;
                                                  });
        
        // Dynamic Trigger: If the proportion of very high p-values is low (e.g., < 10%),
        // the spline estimator may be unstable. In this case, we choose the robust tail-based estimator.
        const bool use_fallback_estimator = (m > 0) && ((static_cast<double>(high_p_value_count) / m) < 0.10);

        if (use_fallback_estimator) {
          std::cout << "[" << familyName << " Family] P-value distribution has a sparse tail ("
                    << high_p_value_count << "/" << m << " p-values > " << high_p_value_threshold
                    << "). Using robust tail-based estimator." << std::endl;
          m0_estimate = estimateM0TailBasedForFamily(familyStrategies);
        } else {
          std::cout << "[" << familyName << " Family] P-value distribution appears stable. Attempting spline-based estimator." << std::endl;
          try {
            m0_estimate = estimateM0StoreySmoothedForFamily(familyStrategies);
            if (m0_estimate <= DecimalConstants<Decimal>::DecimalZero)
              throw std::runtime_error("Spline-based m0 estimate was zero or negative.");
          } catch (const std::exception& e) {
            std::cerr << "[Warning] " << familyName << " Family: Spline estimator failed ('" << e.what()
                      << "'). Falling back to robust tail-based estimator." << std::endl;
            m0_estimate = estimateM0TailBasedForFamily(familyStrategies);
          }
        }
      }
      
      m0_estimate = std::max(DecimalConstants<Decimal>::DecimalOne, m0_estimate);
      
      // Calculate the dynamic FDR using the provided p-value cutoff.
      Decimal dynamic_fdr = estimateFDRForPValueForFamily(familyStrategies, pValueSignificanceLevel, m0_estimate);
      
      const Decimal fdr_ceiling = DecimalConstants<Decimal>::createDecimal("0.20");
      Decimal final_fdr = std::min(fdr_ceiling, dynamic_fdr);
      
      // Create a sorted container for this family (sorted by p-value in ascending order)
      auto sortedFamily = familyStrategies;
      std::sort(sortedFamily.begin(), sortedFamily.end(),
                [](const auto& a, const auto& b) {
                  return a.first < b.first;
                });
      
      Decimal rank(static_cast<int>(sortedFamily.size()));
      bool cutoff_found = false;
      
      std::cout << "--- " << familyName << " Family Benjamini-Hochberg Adaptive Procedure ---" << std::endl;
      std::cout << "Total Tests (m): " << sortedFamily.size()
                << ", Estimated True Nulls (m0): " << m0_estimate
                << ", Final Adaptive FDR (q): " << final_fdr << std::endl;
      std::cout << "-----------------------------------------------" << std::endl;
      
      // Iterate through family strategies from highest p-value to lowest
      for (auto it = sortedFamily.rbegin(); it != sortedFamily.rend(); ++it) {
        Decimal pValue = it->first;
        
        // Calculate the critical value for the current rank
        Decimal criticalValue = (rank / m0_estimate) * final_fdr;
        
        // Print the comparison for this step
        std::cout << "[" << familyName << "] Checking p-value: " << pValue
                  << " (rank " << rank << ")"
                  << " vs. Critical Value: " << criticalValue;
        
        // If we haven't found the cutoff yet, check if this p-value meets the criterion.
        // Once the condition is met, all subsequent (smaller) p-values are also significant.
        if (!cutoff_found && pValue < criticalValue)
          cutoff_found = true;
        
        if (cutoff_found) {
          std::cout << "  ==> SIGNIFICANT" << std::endl;
          container_.addSurvivingStrategy(it->second);
        } else {
          std::cout << "  ==> Not Significant" << std::endl;
        }
        
        // Decrement rank for the next (smaller) p-value
        rank = rank - DecimalConstants<Decimal>::DecimalOne;
      }
      
      std::cout << "--- " << familyName << " Family Procedure Complete ---" << std::endl;
    }
    
    // Helper method to estimate m0 using tail-based method for a specific family
    Decimal estimateM0TailBasedForFamily(const std::vector<std::pair<Decimal,
					 std::shared_ptr<PalStrategy<Decimal>>>>& familyStrategies) const
    {
      const size_t m = familyStrategies.size();
      if (m == 0) return Decimal(0);
      
      const Decimal lambda = DecimalConstants<Decimal>::createDecimal("0.5");
      size_t count = std::count_if(familyStrategies.begin(), familyStrategies.end(),
                                   [lambda](const auto& pair) {
                                     return pair.first > lambda;
                                   });
      
      Decimal pi0_hat = static_cast<Decimal>(count) / ((DecimalConstants<Decimal>::DecimalOne - lambda) * static_cast<Decimal>(m));
      Decimal m0_hat = std::min(DecimalConstants<Decimal>::DecimalOne, pi0_hat) * static_cast<Decimal>(m);
      return std::max(DecimalConstants<Decimal>::DecimalOne, m0_hat);
    }
    
    // Helper method to estimate m0 using spline-based method for a specific family
    Decimal estimateM0StoreySmoothedForFamily(const std::vector<std::pair<Decimal,
					      std::shared_ptr<PalStrategy<Decimal>>>>& familyStrategies) const
    {
      const size_t m = familyStrategies.size();
      if (m < 2) return Decimal(m);
      
      // Generate the (lambda, pi0) data points for this family
      std::vector<double> lambdas;
      std::vector<double> pi0s;
      
      for (Decimal lambda = DecimalConstants<Decimal>::createDecimal("0.05");
           lambda <= DecimalConstants<Decimal>::createDecimal("0.95");
           lambda += DecimalConstants<Decimal>::createDecimal("0.01")) {
        
        size_t count = std::count_if(familyStrategies.begin(), familyStrategies.end(),
                                     [lambda](const auto& entry) {
                                       return entry.first > lambda;
                                     });
        
        // Avoid division by zero if lambda is close to 1
        if (DecimalConstants<Decimal>::DecimalOne - lambda <= DecimalConstants<Decimal>::DecimalZero) continue;
        
        Decimal pi0 = static_cast<Decimal>(count) / ((DecimalConstants<Decimal>::DecimalOne - lambda) * static_cast<Decimal>(m));
        
        double lambda_double = static_cast<double>(lambda.getAsDouble());
        double pi0_double = static_cast<double>(std::min(DecimalConstants<Decimal>::DecimalOne, pi0).getAsDouble());
        
        lambdas.push_back(lambda_double);
        pi0s.push_back(pi0_double);
      }
      
      if (lambdas.empty())
        return Decimal(m);
      
      // Find the optimal smoothing parameter using cross-validation
      const double optimal_lambdans = findOptimalLambdansViaCrossValidation(lambdas, pi0s);
      
      // Convert std::vector to ALGLIB's real_1d_array format.
      alglib::real_1d_array x;
      x.setcontent(lambdas.size(), lambdas.data());
      
      alglib::real_1d_array y;
      y.setcontent(pi0s.size(), pi0s.data());
      
      // Fit the smoothing spline using the MODERN spline1dfit function.
      alglib::spline1dinterpolant s;
      alglib::spline1dfitreport rep;
      
      const alglib::ae_int_t n_points = static_cast<alglib::ae_int_t>(lambdas.size());
      const alglib::ae_int_t m_basis = std::clamp(static_cast<alglib::ae_int_t>(std::sqrt(n_points)),
                                                  static_cast<alglib::ae_int_t>(10),
                                                  static_cast<alglib::ae_int_t>(100));
      
      // Use the optimal lambdans found via cross-validation.
      try {
        alglib::spline1dfit(x, y, n_points, m_basis, optimal_lambdans, s, rep);
      } catch(const alglib::ap_error& e) {
        // Fitting failed, fall back to the total number of tests.
        std::cerr << "ALGLIB spline fitting failed for family: " << e.msg << std::endl;
        return Decimal(m);
      }
      
      // Evaluate the spline at lambda = 1.0 to get the pi0 estimate.
      double extrapolated_pi0_double = alglib::spline1dcalc(s, 1.0);
      
      // Convert back to Decimal type
      Decimal extrapolatedPi0 = static_cast<Decimal>(extrapolated_pi0_double);
      
      // Clamp the result and compute the final m0 estimate.
      extrapolatedPi0 = std::max(DecimalConstants<Decimal>::DecimalZero,
                                 std::min(DecimalConstants<Decimal>::DecimalOne, extrapolatedPi0));
      Decimal m0_hat = extrapolatedPi0 * static_cast<Decimal>(m);
      
      return m0_hat;
    }
    
    // Helper method to estimate FDR for a specific family
    Decimal estimateFDRForPValueForFamily(const std::vector<std::pair<Decimal, std::shared_ptr<PalStrategy<Decimal>>>>& familyStrategies,
                                          const Decimal& pValueCutoff,
                                          const Decimal& m0_estimate) const
    {
      if (familyStrategies.empty())
        return Decimal(0);
      
      Decimal pi0_estimate = m0_estimate / familyStrategies.size();
      
      // Count the number of rejected hypotheses for the given cutoff.
      Decimal num_rejections(DecimalConstants<Decimal>::DecimalZero);
      for (const auto& entry : familyStrategies) {
        if (entry.first <= pValueCutoff) {
          num_rejections += DecimalConstants<Decimal>::DecimalOne;
        }
      }
      
      if (num_rejections == DecimalConstants<Decimal>::DecimalZero) {
        return DecimalConstants<Decimal>::DecimalZero;
      }
      
      // Apply the formula to estimate the FDR.
      Decimal m = static_cast<Decimal>(familyStrategies.size());
      Decimal estimated_fdr = (pi0_estimate * pValueCutoff * m) / num_rejections;
      
      return std::min(Decimal(1.0), estimated_fdr); // FDR cannot be > 1
    }


  /**
   * @brief Finds the optimal smoothing parameter ('lambdans') for spline fitting using k-fold cross-validation.
   * @param lambdas The vector of lambda values (x-coordinates).
   * @param pi0s The vector of pi0 estimates (y-coordinates).
   * @param k_folds The number of folds to use for cross-validation.
   * @return The 'lambdans' value that resulted in the lowest average cross-validated error.
   */
  double findOptimalLambdansViaCrossValidation(const std::vector<double>& lambdas,
                                               const std::vector<double>& pi0s,
                                               int k_folds = 10) const
  {
      // A pre-defined grid of lambdans values to test on a log scale.
      const std::vector<double> lambdans_to_test =
	{
          1.0e-7, 1.0e-6, 1.0e-5, 1.0e-4, 1.0e-3, 1.0e-2, 1.0e-1, 1.0
	};

      const size_t n_data_points = lambdas.size();
      if (n_data_points < static_cast<size_t>(k_folds))
	{
          std::cerr << "[CrossValidation] Warning: Not enough data points (" << n_data_points 
                    << ") for " << k_folds << "-fold cross-validation. Returning default lambdans." << std::endl;
          return 1.0e-5; // Return a sensible default if CV is not possible.
	}

      double best_lambdans = lambdans_to_test[0];
      double min_avg_rmse = std::numeric_limits<double>::max();

      std::cout << "[CrossValidation] Starting " << k_folds << "-fold cross-validation to find optimal lambdans..." << std::endl;

      // Iterate over each candidate lambdans value to test its performance.
      for (double current_lambdans : lambdans_to_test) {
          std::vector<double> fold_rmses;
          size_t fold_size = n_data_points / k_folds;

          // Perform k-fold validation for the current_lambdans.
          for (int k = 0; k < k_folds; ++k) {
              size_t start_index = k * fold_size;
              // Ensure the last fold includes all remaining points.
              size_t end_index = (k == k_folds - 1) ? n_data_points : start_index + fold_size;

              std::vector<double> train_x, train_y, val_x, val_y;
              train_x.reserve(n_data_points - (end_index - start_index));
              train_y.reserve(n_data_points - (end_index - start_index));

              // Partition data into the current training and validation sets.
              for (size_t i = 0; i < n_data_points; ++i) {
                  if (i >= start_index && i < end_index) {
                      val_x.push_back(lambdas[i]);
                      val_y.push_back(pi0s[i]);
                  } else {
                      train_x.push_back(lambdas[i]);
                      train_y.push_back(pi0s[i]);
                  }
              }
              
              if (train_x.empty() || val_x.empty()) continue;

              // Fit the spline on the training data for this fold.
              alglib::real_1d_array x_train_alg, y_train_alg;
              x_train_alg.setcontent(train_x.size(), train_x.data());
              y_train_alg.setcontent(train_y.size(), train_y.data());

              alglib::spline1dinterpolant s;
              alglib::spline1dfitreport rep;
              try {
                  alglib::spline1dfit(x_train_alg, y_train_alg, train_x.size(), n_data_points, current_lambdans, s, rep);
              } catch(const alglib::ap_error& e) {
                  std::cerr << "ALGLIB fitting failed during CV for lambdans=" << current_lambdans << ". Msg: " << e.msg << std::endl;
                  continue; 
              }

              // Calculate the sum of squared errors on the validation set.
              double fold_sse = 0.0;
              for (size_t i = 0; i < val_x.size(); ++i) {
                  double predicted_y = alglib::spline1dcalc(s, val_x[i]);
                  fold_sse += std::pow(predicted_y - val_y[i], 2);
              }
              fold_rmses.push_back(std::sqrt(fold_sse / val_x.size()));
          }

          if (fold_rmses.empty()) continue;

          // Average the RMSE across all k folds.
          double avg_rmse = std::accumulate(fold_rmses.begin(), fold_rmses.end(), 0.0) / fold_rmses.size();
          std::cout << "[CrossValidation]   lambdans = " << std::scientific << current_lambdans
                    << ", Avg. RMSE = " << std::fixed << avg_rmse << std::endl;

          // Update the best lambdans if the current one yields a lower average error.
          if (avg_rmse < min_avg_rmse) {
              min_avg_rmse = avg_rmse;
              best_lambdans = current_lambdans;
          }
      }

      std::cout << "[CrossValidation] Search complete. Optimal lambdans found: " << std::scientific << best_lambdans
                << " (with min avg. RMSE = " << std::fixed << min_avg_rmse << ")" << std::endl;

      return best_lambdans;
  }

  public:
    Decimal estimateFDRForPValue(const Decimal& pValueCutoff)
    {
      // Collect unified family like lines 528-535
      const auto& allStrategies = container_.getInternalContainer();
      std::vector<std::pair<Decimal, std::shared_ptr<PalStrategy<Decimal>>>> unifiedFamily;
      
      // Convert container format to family format
      for (const auto& entry : allStrategies) {
        unifiedFamily.emplace_back(entry.first, entry.second);
      }
      
      // Estimate m0 for the unified family using the family-based method
      Decimal m0_estimate = estimateM0StoreySmoothedForFamily(unifiedFamily);
      
      // Call the family-based method
      return estimateFDRForPValueForFamily(unifiedFamily, pValueCutoff, m0_estimate);
    }

    /**
     * @brief [For Unit Testing Only] Sets a fixed m0 value, bypassing the spline estimator.
     */
    void setM0ForTesting(const Decimal& m0) {
        m_test_m0_override = m0;
    }
    
    BaseStrategyContainer<Decimal> container_;
    Decimal mFalseDiscoveryRate;
    std::optional<Decimal> m_test_m0_override;
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
     DecimalConstants<Decimal>::SignificantPValue,
     [[maybe_unused]] bool partitionByFamily = false)
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

    void correctForMultipleTests(const Decimal& pValueSignificanceLevel = DecimalConstants<Decimal>::SignificantPValue,
                                 [[maybe_unused]] bool partitionByFamily = false)
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
     DecimalConstants<Decimal>::SignificantPValue,
     [[maybe_unused]] bool partitionByFamily = false) {
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
     DecimalConstants<Decimal>::SignificantPValue,
     [[maybe_unused]] bool partitionByFamily = false) {
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
