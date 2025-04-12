// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
// Revised for separation-of-concerns design by ChatGPT.

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
  // Existing common container used by several policies.
  // This encapsulates the container for strategies keyed by a Decimal
  // (typically representing a p-value) and the container for surviving strategies.
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

    const_iterator beginSortedStrategy() const { return sortedStrategies_.begin(); }
    const_iterator endSortedStrategy() const { return sortedStrategies_.end(); }
    const_reverse_iterator rbeginSortedStrategy() const { return sortedStrategies_.rbegin(); }
    const_reverse_iterator rendSortedStrategy() const { return sortedStrategies_.rend(); }

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

  private:
    SortedStrategyContainer sortedStrategies_;
    SurvivingStrategyContainer survivingStrategies_;
    mutable boost::mutex strategiesMutex_;
    mutable boost::mutex survivingMutex_;
  };


  //===========================================================================
  // Policy: BenjaminiHochbergFdr
  // [Code for BenjaminiHochbergFdr remains unchanged and reuses BaseStrategyContainer]
  //===========================================================================
  template <class Decimal>
  class BenjaminiHochbergFdr {
  public:
    typedef typename PValueReturnPolicy<Decimal>::ReturnType ReturnType;

    BenjaminiHochbergFdr()
      : mFalseDiscoveryRate(DecimalConstants<Decimal>::DefaultFDR)
    {}

    void addStrategy(const ReturnType& pValue, std::shared_ptr<PalStrategy<Decimal>> aStrategy) {
      container_.addStrategy(pValue, aStrategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }

    typename BaseStrategyContainer<Decimal>::const_iterator beginSortedStrategy() const {
      return container_.beginSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::const_iterator endSortedStrategy() const {
      return container_.endSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::const_reverse_iterator rbeginSortedStrategy() const {
      return container_.rbeginSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::const_reverse_iterator rendSortedStrategy() const {
      return container_.rendSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::surviving_const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }
    typename BaseStrategyContainer<Decimal>::surviving_const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests() {
      auto it = container_.getInternalContainer().rbegin();
      auto itEnd = container_.getInternalContainer().rend();

      Decimal numTests(static_cast<int>(getNumMultiComparisonStrategies()));
      Decimal rank = numTests;
      Decimal criticalValue = 0;

      // Iterate in reverse order:
      // Step 1: Iterate from the largest to the smallest p-value.
      // Step 2: For each p-value compute criticalValue = (rank / m) * FDR, where m = total number of tests.
      // Step 3: Stop when the p-value falls below criticalValue.
      // (This approach is inspired by the original Benjamini-Hochberg (1995) procedure.)
      for (; it != itEnd; ++it) {
        Decimal pValue = it->first;
        criticalValue = (rank / numTests) * mFalseDiscoveryRate;
        std::cout << "BenjaminiHochbergFdr:: pValue = " << pValue
                  << ", Critical value = " << criticalValue << std::endl;
        if (pValue < criticalValue)
          break;
        else
          rank = rank - DecimalConstants<Decimal>::DecimalOne;
      }
      // Strategies that have not been rejected are added to the surviving set.
      for (; it != itEnd; ++it) {
        container_.addSurvivingStrategy(it->second);
      }
      std::cout << "BenjaminiHochbergFdr::Number of multi comparison strategies = " << getNumMultiComparisonStrategies() << std::endl;
      std::cout << "BenjaminiHochbergFdr::Number of surviving strategies after correction = " << getNumSurvivingStrategies() << std::endl;
    }

  private:
    BaseStrategyContainer<Decimal> container_;
    Decimal mFalseDiscoveryRate;
  };


  //===========================================================================
  // Policy: AdaptiveBenjaminiHochbergYr2000
  // [Code for AdaptiveBenjaminiHochbergYr2000 remains unchanged and reuses BaseStrategyContainer]
  //===========================================================================
  template <class Decimal>
  class AdaptiveBenjaminiHochbergYr2000 {
  public:
    typedef typename PValueReturnPolicy<Decimal>::ReturnType ReturnType;

    AdaptiveBenjaminiHochbergYr2000()
      : mFalseDiscoveryRate(DecimalConstants<Decimal>::DefaultFDR)
    {}

    void addStrategy(const ReturnType& pValue, std::shared_ptr<PalStrategy<Decimal>> aStrategy) {
      container_.addStrategy(pValue, aStrategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }
    typename BaseStrategyContainer<Decimal>::const_iterator beginSortedStrategy() const {
      return container_.beginSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::const_iterator endSortedStrategy() const {
      return container_.endSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::surviving_const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }
    typename BaseStrategyContainer<Decimal>::surviving_const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests() {
      auto it = container_.getInternalContainer().rbegin();
      auto itEnd = container_.getInternalContainer().rend();
      Decimal rank(static_cast<int>(getNumMultiComparisonStrategies()));
      Decimal criticalValue = 0;

      calculateSlopes();
      Decimal numTests = calculateMPrime();

      std::cout << "AdaptiveBenjaminiHochbergYr2000:: mPrime = " << numTests
                << ", m = " << getNumMultiComparisonStrategies() << std::endl;

      // Steps for Adaptive Procedure:
      // 1. Calculate slopes for each hypothesis based on the difference from 1.
      // 2. Compute m' (mPrime) as the minimum adjusted slope factor.
      // 3. Iterate in reverse order to compute adjusted critical values using mPrime.
      // 4. Strategies meeting the criterion are added as surviving.
      for (; it != itEnd; ++it) {
        Decimal pValue = it->first;
        criticalValue = (rank / numTests) * mFalseDiscoveryRate;
        std::cout << "AdaptiveBenjaminiHochbergYr2000:: pValue = " << pValue
                  << ", Critical value = " << criticalValue << std::endl;
        if (pValue < criticalValue)
          break;
        else
          rank = rank - DecimalConstants<Decimal>::DecimalOne;
      }
      for (; it != itEnd; ++it) {
        container_.addSurvivingStrategy(it->second);
      }
      std::cout << "AdaptiveBenjaminiHochbergYr2000::Number of multi comparison strategies = " << getNumMultiComparisonStrategies() << std::endl;
      std::cout << "AdaptiveBenjaminiHochbergYr2000::Number of surviving strategies after correction = " << getNumSurvivingStrategies() << std::endl;
    }

  private:
    void calculateSlopes() {
      if (getNumMultiComparisonStrategies() > 0) {
        Decimal m(static_cast<int>(getNumMultiComparisonStrategies()));
        Decimal i = DecimalConstants<Decimal>::DecimalOne;
        mSlopes.clear();
        for (auto it = container_.getInternalContainer().begin(); it != container_.getInternalContainer().end(); ++it) {
          Decimal pValue = it->first;
          Decimal num = (DecimalConstants<Decimal>::DecimalOne - pValue);
          Decimal denom = (m + DecimalConstants<Decimal>::DecimalOne - i);
          Decimal slope = num / denom;
          mSlopes.push_back(slope);
          std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateSlopes, i = " << i
                    << ", slope = " << slope << std::endl;
          i = i + DecimalConstants<Decimal>::DecimalOne;
        }
      }
    }

    Decimal calculateMPrime() {
      Decimal m(static_cast<int>(getNumMultiComparisonStrategies()));
      for (unsigned int i = 1; i < mSlopes.size(); i++) {
        if (mSlopes[i] < mSlopes[i - 1]) {
          Decimal temp = (DecimalConstants<Decimal>::DecimalOne / mSlopes[i]) + DecimalConstants<Decimal>::DecimalOne;
          std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime, i = " << i
                    << ", mSlopes[" << i << "] = " << mSlopes[i]
                    << ", mSlopes[" << i - 1 << "] = " << mSlopes[i - 1] << std::endl;
          std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime = " << temp << std::endl;
          return std::min(temp, m);
        }
      }
      return m;
    }

    BaseStrategyContainer<Decimal> container_;
    Decimal mFalseDiscoveryRate;
    std::vector<Decimal> mSlopes;
  };


  //===========================================================================
  // Policy: UnadjustedPValueStrategySelection
  // [Code for UnadjustedPValueStrategySelection remains unchanged and reuses BaseStrategyContainer]
  //===========================================================================
  template <class Decimal>
  class UnadjustedPValueStrategySelection {
  public:
    typedef typename PValueReturnPolicy<Decimal>::ReturnType ReturnType;

    UnadjustedPValueStrategySelection() {}

    void addStrategy(const ReturnType& pValue, std::shared_ptr<PalStrategy<Decimal>> aStrategy) {
      container_.addStrategy(pValue, aStrategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }
    typename BaseStrategyContainer<Decimal>::const_iterator beginSortedStrategy() const {
      return container_.beginSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::const_iterator endSortedStrategy() const {
      return container_.endSortedStrategy();
    }
    typename BaseStrategyContainer<Decimal>::surviving_const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }
    typename BaseStrategyContainer<Decimal>::surviving_const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests() {
      const Decimal SIGNIFICANCE_THRESHOLD = DecimalConstants<Decimal>::SignificantPValue;
      for (auto & entry : container_.getInternalContainer()) {
        if (entry.first < SIGNIFICANCE_THRESHOLD)
          container_.addSurvivingStrategy(entry.second);
      }
    }

  private:
    BaseStrategyContainer<Decimal> container_;
  };


  //===========================================================================
  // New helper class for test-statistic–based corrections.
  // This class encapsulates a container for strategies together with their
  // associated p-values and test statistics.
  // The vector type is now called TestStatisticContainer and the member variable
  // is named testStatisticStrategies_.
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

    size_t getNumStrategies() const {
      boost::mutex::scoped_lock Lock(mutex_);
      return testStatisticStrategies_.size();
    }

    const_iterator beginSortedStrategy() const { return testStatisticStrategies_.begin(); }
    const_iterator endSortedStrategy() const { return testStatisticStrategies_.end(); }

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
    const TestStatisticContainer& getInternalContainer() const { return testStatisticStrategies_; }

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
      return syntheticNullDistribution_;
    }

  private:
    TestStatisticContainer testStatisticStrategies_;
    SurvivingStrategyContainer survivingStrategies_;
    mutable boost::mutex mutex_;
    std::vector<Decimal> syntheticNullDistribution_;
    bool hasSyntheticNull_ = false;    
  };


  //===========================================================================
  // Policy: RomanoWolfStepdownCorrection
  // Revised to compute the stepdown adjustment in reverse order as described by Romano and Wolf.
  // This policy now uses composition to delegate container management to
  // TestStatisticStrategyImplementation.
  //
  // References:
  // - Romano, J. P. & Wolf, M. (2005). Exact and approximate stepdown methods for multiple hypothesis testing.
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
  //===========================================================================
  template <class Decimal>
  class RomanoWolfStepdownCorrection {
  public:
    // The return type is defined as a tuple {p-value, maxTestStat}.
    typedef typename PValueAndTestStatisticReturnPolicy<Decimal>::ReturnType RomanoWolfReturnType;
    typedef typename TestStatisticStrategyImplementation<Decimal>::TestStatisticContainer TestStatisticContainer;

    RomanoWolfStepdownCorrection() {}

    void addStrategy(const RomanoWolfReturnType& result, std::shared_ptr<PalStrategy<Decimal>> strategy) {
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

    typename TestStatisticContainer::const_iterator beginSortedStrategy() const {
      return container_.beginSortedStrategy();
    }
    typename TestStatisticContainer::const_iterator endSortedStrategy() const {
      return container_.endSortedStrategy();
    }
    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }
    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    // The following comments describe the algorithm implemented in correctForMultipleTests():
    // 1. If no strategies exist, exit.
    // 2. Obtain the internal container (a vector of {p-value, maxTestStat, strategy} tuples).
    // 3. **Sort the container in ascending order by the original (empirical) p-value.**
    // 4. **Build an empirical null distribution:** This is done by extracting the max test statistic
    //    from each tuple and sorting these values. This distribution represents the null hypothesis
    //    as estimated from the data (i.e., without relying on a theoretical distribution).
    // 5. **Iterate in reverse order (from the highest to lowest p-value):**
    //    a. For each hypothesis, use std::upper_bound on the sorted empirical null distribution to count
    //       how many null test statistics are less than or equal to the original p-value.
    //       - The code `auto ub = std::upper_bound(sortedNull.begin(), sortedNull.end(), originalPValue);`
    //         finds the position of the first element in sortedNull that is greater than originalPValue.
    //       - Then, `Decimal empiricalP = static_cast<Decimal>(ub - sortedNull.begin());`
    //         computes the number of such elements, yielding a count that—when divided by the total
    //         number of nulls—provides an empirical p-value.
    //    b. Compute a candidate adjusted p-value:
    //         candidate = empiricalP * (totalStrategies / (i+1)),
    //       where i is the index in reverse order.
    //    c. For the hypothesis with the highest p-value, the candidate becomes the adjusted p-value;
    //       for others, choose the minimum of the candidate and the previously computed adjusted p-value.
    // 6. Mark strategies with adjusted p-values below the significance threshold as surviving.
    void correctForMultipleTests() {
      if (container_.getNumStrategies() == 0)
        return;

      // Step 1: Obtain a reference to the internal container for sorting.
      auto& tsContainer = container_.getInternalContainer();

      // Step 2: Sort the container in ascending order by the original empirical p-value.
      std::sort(tsContainer.begin(), tsContainer.end(),
                [](const auto& a, const auto& b) {
                  return std::get<0>(a) < std::get<0>(b);
                });

      // An "empirical null distribution" is a distribution of a test statistic that is generated from
      // data where the null hypothesis is assumed to be true. Rather than relying on theoretical distributions
      // (like the normal or t-distribution), you build the distribution directly from the
      // data (usually via resampling or permutation methods). In the context of these correction methods,
      // the empirical null distribution is created by collecting the "max test statistics" from each hypothesis
      // (or permutation), then sorting them.
      //
      // This provides a data‐driven reference against which the observed p-values can be compared during the
      // multiple testing correction process. The approach is often used in permutation tests and has been discussed
      // in detail in works like Romano and Wolf (2005).
      
      // Step 3: Build the empirical null distribution by extracting the max test statistics from each tuple.
      // This distribution represents our empirical null hypothesis based on the observed data.
      std::vector<Decimal> sortedNull;

      if (container_.hasSyntheticNull())
	{
	  sortedNull = container_.getSyntheticNullDistribution();
	}
      else
	{
	  for (const auto& entry : tsContainer)
	    {
	      sortedNull.push_back(std::get<1>(entry));
	    }
	}

      std::sort(sortedNull.begin(), sortedNull.end());

      unsigned int totalStrategies = static_cast<unsigned int>(tsContainer.size());
      Decimal previousAdjustedP = Decimal(1.0);
      Decimal sortedNullSize(static_cast<Decimal>(static_cast<unsigned int>(sortedNull.size())));      

      // Step 4: Iterate in reverse order (from largest to smallest original p-value):
      //   a. For each hypothesis, count the number of null test statistics (from sortedNull)
      //      that are less than or equal to the observed p-value.
      //      This is achieved using std::upper_bound.
      //   b. Compute the empirical p-value = (count from upper_bound) / (total number of nulls).
      //   c. Calculate candidate adjusted p-value = empiricalP * (totalStrategies / (i+1)).
      //   d. For the least significant hypothesis, the candidate is the adjusted p-value.
      //      For the others, take the minimum of candidate and the previously computed adjusted p-value.
      for (int i = totalStrategies - 1; i >= 0; --i) {
        Decimal originalPValue = std::get<0>(tsContainer[i]);
	Decimal observedTestStat = std::get<1>(tsContainer[i]);
	
	// This function searches through the sorted vector sortedNull
	// (the empirical null distribution built from the maximum test statistics) for
	// the first element that is greater than originalPValue.

	// Because the vector is sorted, this gives us the position (iterator) corresponding
	// to the number of elements in the null distribution that are less than or equal to
	// the observed (original) p-value.
	auto ub = std::upper_bound(sortedNull.begin(), sortedNull.end(), observedTestStat);

	// Subtracting the beginning iterator from ub computes the number of elements in
	// sortedNull that are less than or equal to originalPValue.
	//
	// Essentially, this difference yields a count of how many test statistics in the null
	// distribution do not exceed the observed statistic.
	
        Decimal empiricalP = static_cast<Decimal>(ub - sortedNull.begin());

	// Compute "empirical p-value" by dividing it by the total number of elements in the null distribution.
        empiricalP = empiricalP / sortedNullSize;
 
        Decimal candidate = empiricalP * (static_cast<Decimal>(totalStrategies) / static_cast<Decimal>(i + 1));
        Decimal adjustedP = (i == static_cast<int>(totalStrategies - 1)) ? candidate :
	  std::min(previousAdjustedP, candidate);
	
        previousAdjustedP = adjustedP;
        std::get<0>(tsContainer[i]) = adjustedP;
      }

      // Step 5: Mark strategies with adjusted p-values below the significance threshold as surviving.
      for (const auto& tup : tsContainer) {
        Decimal adjustedPValue = std::get<0>(tup);
        if (adjustedPValue < DecimalConstants<Decimal>::SignificantPValue)
          container_.addSurvivingStrategy(std::get<2>(tup));
      }
    }

    
  private:
    TestStatisticStrategyImplementation<Decimal> container_;
  };


  //===========================================================================
  // Policy: HolmRomanoWolfCorrection
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
  //
  // The following comments detail the algorithm in correctForMultipleTests():
  // 1. If no strategies exist, exit.
  // 2. Obtain the internal container (vector of {p-value, maxTestStat, strategy} tuples) and sort it
  //    in ascending order by the original (empirical) p-value.
  // 3. Build the empirical null distribution from the max test statistics, which represents the
  //    estimated distribution of the test statistic under the null hypothesis.
  // 4. Iterate in forward order (from smallest to largest observed p-value):
  //    a. For each hypothesis, determine the number of null test statistics that are less than or equal
  //       to the observed p-value using std::upper_bound.
  //       - The operation `ub - sortedNull.begin()` computes the count.
  //    b. The empirical p-value is then the ratio of this count to the total number of nulls.
  //    c. Compute a candidate adjusted p-value using Holm’s scaling: candidate = empiricalP * ((totalStrategies - i) / totalStrategies).
  //    d. For the first hypothesis, the candidate is directly the adjusted p-value.
  //       For subsequent hypotheses, choose the maximum of the candidate and the previously computed adjusted p-value.
  // 5. Mark strategies with adjusted p-values below the significance threshold as surviving.
  //===========================================================================
  template <class Decimal>
  class HolmRomanoWolfCorrection {
  public:
    // The return type is defined as a tuple {p-value, maxTestStat}.
    typedef typename PValueAndTestStatisticReturnPolicy<Decimal>::ReturnType HolmRomanoWolfReturnType;
    typedef typename TestStatisticStrategyImplementation<Decimal>::TestStatisticContainer TestStatisticContainer;

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

    typename TestStatisticContainer::const_iterator beginSortedStrategy() const {
      return container_.beginSortedStrategy();
    }
    typename TestStatisticContainer::const_iterator endSortedStrategy() const {
      return container_.endSortedStrategy();
    }
    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }
    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests() {
      if (container_.getNumStrategies() == 0)
        return;

      // Step 1: Obtain a reference to the internal container containing (p-value, max test statistic, strategy) tuples.
      auto& tsContainer = container_.getInternalContainer();

      // Step 2: Sort the container in ascending order by the original empirical p-value.
      std::sort(tsContainer.begin(), tsContainer.end(),
                [](const auto& a, const auto& b) {
                  return std::get<0>(a) < std::get<0>(b);
                });

      // Step 3: Build the empirical null distribution by extracting all max test statistics.
      // This sorted vector is our empirical null distribution.
      std::vector<Decimal> sortedNull;

      if (container_.hasSyntheticNull())
	{
	  sortedNull = container_.getSyntheticNullDistribution();
	}
      else
	{
	  for (const auto& entry : tsContainer)
	    {
	      sortedNull.push_back(std::get<1>(entry));
	    }
	}
      
      std::sort(sortedNull.begin(), sortedNull.end());

      unsigned int totalStrategies = static_cast<unsigned int>(tsContainer.size());
      Decimal previousAdjustedP = Decimal(0);  // Holm adjustment starts at 0.
      Decimal sortedNullSize(static_cast<Decimal>(static_cast<unsigned int>(sortedNull.size())));      

      // Step 4: Iterate in forward order (from smallest to largest observed p-value):
      //   For each hypothesis:
      //     a. Use std::upper_bound to find the count of null test statistics that are less than or equal
      //        to the original p-value.
      //        - The code: auto ub = std::upper_bound(sortedNull.begin(), sortedNull.end(), originalPValue);
      //          returns an iterator to the first element greater than originalPValue.
      //        - The difference (ub - sortedNull.begin()) is the count.
      //     b. Compute the empirical p-value as: (count) / (total number of nulls).
      //     c. Calculate candidate adjusted p-value = empiricalP * ((totalStrategies - i) / totalStrategies).
      //     d. For the first hypothesis, set the adjusted p-value to candidate;
      //        for later hypotheses, set it to the maximum of candidate and the previous adjusted value.
      for (unsigned int i = 0; i < totalStrategies; ++i) {
        Decimal originalPValue = std::get<0>(tsContainer[i]);
	Decimal observedTestStat = std::get<1>(tsContainer[i]);
	
	auto ub = std::upper_bound(sortedNull.begin(), sortedNull.end(), observedTestStat);
        //auto ub = std::upper_bound(sortedNull.begin(), sortedNull.end(), originalPValue);

	Decimal empiricalP = static_cast<Decimal>(ub - sortedNull.begin());
        empiricalP = empiricalP / sortedNullSize;
        Decimal candidate = empiricalP * (static_cast<Decimal>(totalStrategies - i) / static_cast<Decimal>(totalStrategies));
        Decimal adjustedP = (i == 0) ? candidate : std::max(previousAdjustedP, candidate);
        previousAdjustedP = adjustedP;
        std::get<0>(tsContainer[i]) = adjustedP;
      }

      // Step 5: Mark strategies with adjusted p-values below the significance threshold as surviving.
      for (const auto& tup : tsContainer) {
        Decimal adjustedPValue = std::get<0>(tup);
        if (adjustedPValue < DecimalConstants<Decimal>::SignificantPValue)
          container_.addSurvivingStrategy(std::get<2>(tup));
      }
    }

  private:
    TestStatisticStrategyImplementation<Decimal> container_;
  };

  //=========================================================================
  // Policy: MastersRomanoWolfCorrection
  // Implements the stepwise sequential testing approach inspired by Timothy Masters.
  //=========================================================================
  template <class Decimal>
  class MastersRomanoWolfCorrection {
  public:
    typedef typename PValueAndTestStatisticReturnPolicy<Decimal>::ReturnType ReturnType;
    typedef typename TestStatisticStrategyImplementation<Decimal>::TestStatisticContainer TestStatisticContainer;

    MastersRomanoWolfCorrection() {}

    void addStrategy(const ReturnType& result, std::shared_ptr<PalStrategy<Decimal>> strategy) {
      Decimal pValue = std::get<0>(result);
      Decimal maxTestStat = std::get<1>(result);
      container_.addStrategy(pValue, maxTestStat, strategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
    }

    void setSyntheticNullDistribution(const std::vector<Decimal>& syntheticNull)
    {
      container_.setSyntheticNullDistribution(syntheticNull);
    }

    typename TestStatisticContainer::const_iterator beginSortedStrategy() const {
      return container_.beginSortedStrategy();
    }

    typename TestStatisticContainer::const_iterator endSortedStrategy() const {
      return container_.endSortedStrategy();
    }

    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }

    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }

    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    void correctForMultipleTests() {
      if (container_.getNumStrategies() == 0)
	return;

      auto& tsContainer = container_.getInternalContainer();

      // Step 1: Sort strategies by observed test statistic (descending for stepwise testing).
      std::sort(tsContainer.begin(), tsContainer.end(), [](const auto& a, const auto& b) {
	return std::get<1>(a) > std::get<1>(b); // descending order
      });

      const unsigned int totalStrategies = static_cast<unsigned int>(tsContainer.size());
      const Decimal totalStrategiesDecimal(static_cast<Decimal>(totalStrategies));
      Decimal previousPval(0);

      // Step 2: Build initial null distribution (copy of max test stats).
      std::vector<Decimal> nullDistribution;

      if (container_.hasSyntheticNull())
	{
	  nullDistribution = container_.getSyntheticNullDistribution();
	}
      else
	{
	  for (const auto& entry : tsContainer)
	    {
	      nullDistribution.push_back(std::get<1>(entry));
	    }
	}

      std::sort(nullDistribution.begin(), nullDistribution.end()); // ascending for upper_bound

      Decimal nullDistributionSize(static_cast<Decimal>(static_cast<unsigned int>(nullDistribution.size())));      
      // Step 3: Stepwise sequential testing
      for (unsigned int i = 0; i < totalStrategies; ++i) {
	const Decimal observedStat = std::get<1>(tsContainer[i]);

	// Count exceedances in current null distribution
	auto ub = std::upper_bound(nullDistribution.begin(), nullDistribution.end(), observedStat);
	Decimal exceedanceCount = static_cast<Decimal>(nullDistribution.end() - ub);

	// Compute p-value: (exceedances + 1) / (null size + 1) to avoid zero p-values
	Decimal pval = (exceedanceCount + DecimalConstants<Decimal>::DecimalOne)
	  / (nullDistributionSize + DecimalConstants<Decimal>::DecimalOne);

	// Enforce monotonicity
	pval = std::max(pval, previousPval);
	previousPval = pval;

	std::get<0>(tsContainer[i]) = pval; // Save adjusted p-value

	// Step 4: Check significance
	if (pval <= DecimalConstants<Decimal>::SignificantPValue) {
	  container_.addSurvivingStrategy(std::get<2>(tsContainer[i]));

	  // Step 5: Shrink the null distribution by removing this strategy's stat from null
	  auto removeIt = std::lower_bound(nullDistribution.begin(), nullDistribution.end(), observedStat);
	  if (removeIt != nullDistribution.end() && *removeIt == observedStat)
	    nullDistribution.erase(removeIt);
	} else {
	  // Early stop if p-value exceeds significance level
	  break;
	}
      }
    }

  private:
    TestStatisticStrategyImplementation<Decimal> container_;
  };
} // namespace mkc_timeseries

#endif
