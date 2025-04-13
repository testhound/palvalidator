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

    // Helper function to prepare the container and build the sorted null distribution.
    // Returns true if the container is non-empty and a non-empty sorted null distribution can be built.
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
    template <typename Decimal, typename Container>
    bool prepareContainerAndNull(Container& container, std::vector<Decimal>& sortedEmpiricalNullDistribution) {
      if (container.getNumStrategies() == 0)
        return false;
      
      auto& tsContainer = container.getInternalContainer();
      
      // Sort the container in ascending order by the original p-value (stored in element index 0).
      std::sort(tsContainer.begin(), tsContainer.end(),
                [](const auto& a, const auto& b) {
                  return std::get<0>(a) < std::get<0>(b);
                });
      
      // Build the null distribution either from a synthetic null or by extracting the max test statistic.
      if (container.hasSyntheticNull())
	{
	  sortedEmpiricalNullDistribution = container.getSyntheticNullDistribution();
	}
      else
	{
	  for (const auto& entry : tsContainer)
	    sortedEmpiricalNullDistribution.push_back(std::get<1>(entry)); // Use maxTestStat stored at index 1.
	}
      
      std::sort(sortedEmpiricalNullDistribution.begin(), sortedEmpiricalNullDistribution.end());
      return !sortedEmpiricalNullDistribution.empty();
    }

    // Helper function to adjust p-values.
    // The function iterates over the container (reverse for step-down, forward for step-up),
    // computes the empirical p-value using the sorted null distribution,
    // computes a candidate adjusted p-value (via computeCandidate lambda),
    // enforces monotonicity (via updateMono lambda),
    // and writes the adjusted p-value back into the container (at element index 0).
    template <typename Decimal, typename Container, typename FactorFunc, typename MonoFunc>
    void adjustPValues(Container& container,
                       const std::vector<Decimal>& sortedEmpiricalNullDistribution,
                       FactorFunc computeCandidate,
                       MonoFunc updateMono,
                       bool reverseOrder,
                       Decimal initialPreviousAdj) {
      const unsigned int totalStrategies = static_cast<unsigned int>(container.size());
      Decimal previousAdjusted = initialPreviousAdj;

      if (reverseOrder) {
        // Reverse order iteration (step-down correction).
        for (int i = totalStrategies - 1; i >= 0; i--) {
          Decimal observedTestStat = std::get<1>(container[i]);
          auto lb = std::lower_bound(sortedEmpiricalNullDistribution.begin(), sortedEmpiricalNullDistribution.end(), observedTestStat);
          Decimal countGreaterEqual = static_cast<Decimal>(std::distance(lb, sortedEmpiricalNullDistribution.end()));
          Decimal empiricalP = countGreaterEqual / static_cast<Decimal>(sortedEmpiricalNullDistribution.size());
          Decimal candidate = computeCandidate(empiricalP, i, totalStrategies);
          Decimal adjusted = (i == static_cast<int>(totalStrategies - 1)) ? candidate : updateMono(previousAdjusted, candidate);
          previousAdjusted = adjusted;
          std::get<0>(container[i]) = adjusted;
        }
      } else {
        // Forward order iteration (step-up correction).
        for (unsigned int i = 0; i < totalStrategies; i++) {
          Decimal observedTestStat = std::get<1>(container[i]);
          auto lb = std::lower_bound(sortedEmpiricalNullDistribution.begin(), sortedEmpiricalNullDistribution.end(), observedTestStat);
          Decimal countGreaterEqual = static_cast<Decimal>(std::distance(lb, sortedEmpiricalNullDistribution.end()));
          Decimal empiricalP = countGreaterEqual / static_cast<Decimal>(sortedEmpiricalNullDistribution.size());
          Decimal candidate = computeCandidate(empiricalP, i, totalStrategies);
          Decimal adjusted = (i == 0) ? candidate : updateMono(previousAdjusted, candidate);
          previousAdjusted = adjusted;
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

    BenjaminiHochbergFdr()
      : mFalseDiscoveryRate(DecimalConstants<Decimal>::DefaultFDR)
    {}

    void addStrategy(const ReturnType& pValue, std::shared_ptr<PalStrategy<Decimal>> aStrategy) {
      container_.addStrategy(pValue, aStrategy);
    }

    size_t getNumMultiComparisonStrategies() const {
      return container_.getNumStrategies();
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

  private:
    BaseStrategyContainer<Decimal> container_;
    Decimal mFalseDiscoveryRate;
  };


  //===========================================================================
  // Policy: AdaptiveBenjaminiHochbergYr2000
  // [Code for AdaptiveBenjaminiHochbergYr2000 remains unchanged]
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

      // std::cout << "AdaptiveBenjaminiHochbergYr2000:: mPrime = " << numTests
      //           << ", m = " << getNumMultiComparisonStrategies() << std::endl;

      for (; it != itEnd; ++it) {
        Decimal pValue = it->first;
        criticalValue = (rank / numTests) * mFalseDiscoveryRate;
        // std::cout << "AdaptiveBenjaminiHochbergYr2000:: pValue = " << pValue
        //           << ", Critical value = " << criticalValue << std::endl;
        if (pValue < criticalValue)
          break;
        else
          rank = rank - DecimalConstants<Decimal>::DecimalOne;
      }
      for (; it != itEnd; ++it) {
        container_.addSurvivingStrategy(it->second);
      }
      // std::cout << "AdaptiveBenjaminiHochbergYr2000::Number of multi comparison strategies = " << getNumMultiComparisonStrategies() << std::endl;
      // std::cout << "AdaptiveBenjaminiHochbergYr2000::Number of surviving strategies after correction = " << getNumSurvivingStrategies() << std::endl;
    }

     // Added for monotonicity test helper compatibility
    const typename BaseStrategyContainer<Decimal>::SortedStrategyContainer& getInternalContainer() const {
        return container_.getInternalContainer();
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
          //   std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateSlopes, i = " << i
          //             << ", slope = " << slope << std::endl;
          i = i + DecimalConstants<Decimal>::DecimalOne;
        }
      }
    }

    Decimal calculateMPrime() {
      Decimal m(static_cast<int>(getNumMultiComparisonStrategies()));
      for (unsigned int i = 1; i < mSlopes.size(); i++) {
        if (mSlopes[i] < mSlopes[i - 1]) {
          Decimal temp = (DecimalConstants<Decimal>::DecimalOne / mSlopes[i]) + DecimalConstants<Decimal>::DecimalOne;
          //   std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime, i = " << i
          //             << ", mSlopes[" << i << "] = " << mSlopes[i]
          //             << ", mSlopes[" << i - 1 << "] = " << mSlopes[i - 1] << std::endl;
          //   std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime = " << temp << std::endl;
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
  // [Code for UnadjustedPValueStrategySelection remains unchanged]
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

      for (auto const& entry : container_.getInternalContainer()) {
        const auto& pValue = entry.first;
        const auto& strategy = entry.second;
        if (pValue < SIGNIFICANCE_THRESHOLD)
          container_.addSurvivingStrategy(strategy);
      }
    }

    // Added for monotonicity test helper compatibility
    const typename BaseStrategyContainer<Decimal>::SortedStrategyContainer& getInternalContainer() const {
        return container_.getInternalContainer();
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

  private:
    TestStatisticContainer testStatisticStrategies_;
    SurvivingStrategyContainer survivingStrategies_;
    mutable boost::mutex mutex_;
    std::vector<Decimal> syntheticNullDistribution_;
    bool hasSyntheticNull_ = false;
  };


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
  class RomanoWolfStepdownCorrection {
  public:
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

    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }
    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    // Added for monotonicity test helper compatibility
    const TestStatisticContainer& getInternalContainer() const {
        return container_.getInternalContainer();
    }

    void correctForMultipleTests() {
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
      const Decimal SIGNIFICANCE_THRESHOLD = DecimalConstants<Decimal>::SignificantPValue;
      for (const auto& tup : tsContainer) {
        Decimal adjustedPValue = std::get<0>(tup);
        if (adjustedPValue < SIGNIFICANCE_THRESHOLD)
          container_.addSurvivingStrategy(std::get<2>(tup));
      }
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

    // Note: begin/endSortedStrategy provide iterators to the container BEFORE correction/sorting by adjusted p-value.
    // Use getInternalContainer() if access to the modified tuples is needed after correctForMultipleTests().

    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator beginSurvivingStrategies() const {
      return container_.beginSurvivingStrategies();
    }
    typename std::list<std::shared_ptr<PalStrategy<Decimal>>>::const_iterator endSurvivingStrategies() const {
      return container_.endSurvivingStrategies();
    }
    size_t getNumSurvivingStrategies() const {
      return container_.getNumSurvivingStrategies();
    }

    // Added for monotonicity test helper compatibility
    const TestStatisticContainer& getInternalContainer() const {
        return container_.getInternalContainer();
    }

    void correctForMultipleTests() {
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
      const Decimal SIGNIFICANCE_THRESHOLD = DecimalConstants<Decimal>::SignificantPValue;
      for (const auto& tup : tsContainer) {
        Decimal adjustedPValue = std::get<0>(tup);
        if (adjustedPValue < SIGNIFICANCE_THRESHOLD)
          container_.addSurvivingStrategy(std::get<2>(tup));
      }
    }

  private:
    TestStatisticStrategyImplementation<Decimal> container_;
  };

} // namespace mkc_timeseries

#endif // __MULTIPLE_TESTING_CORRECTION_H
