// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
//

#ifndef __MULTIPLE_TESTING_CORRECTION_H
#define __MULTIPLE_TESTING_CORRECTION_H 1

#include <list>
#include <map>
#include <vector>
#include "number.h"
#include "DecimalConstants.h"
#include "PermutationTestResultPolicy.h"
#include "PalStrategy.h"
#include <boost/thread/mutex.hpp>

namespace mkc_timeseries
{
  //-----------------------------------------------------------------------------
  // Common data structure holding the containers used by multiple policies.
  //-----------------------------------------------------------------------------
  template <class Decimal>
  struct MultipleTestingData {
    // Container for strategies that use a Decimal-based key (e.g., p-values).
    typedef std::multimap<Decimal, std::shared_ptr<PalStrategy<Decimal>>> SortedStrategyContainer;
    SortedStrategyContainer strategyContainer;

    // Container for policies that require both a p-value and a max test statistic.
    // Each element is a tuple: {p-value, max test statistic, strategy}
    typedef std::vector<std::tuple<Decimal, Decimal, std::shared_ptr<PalStrategy<Decimal>>>> MaxTestStatContainer;
    MaxTestStatContainer maxTestStatStrategies;

    // Container for surviving strategies after correction.
    typedef std::list<std::shared_ptr<PalStrategy<Decimal>>> SurvivingStrategyContainer;
    SurvivingStrategyContainer survivingStrategiesContainer;

    // Mutexes to protect concurrent access.
    boost::mutex strategiesMutex;
    boost::mutex survivingStrategiesMutex;

    // Explicit default constructor.
    MultipleTestingData()
      : strategyContainer(),
	maxTestStatStrategies(),
	survivingStrategiesContainer(),
	strategiesMutex(),
	survivingStrategiesMutex()
    {
      // Optional: Any debug or custom initialization code here.
    }
  };

  //-----------------------------------------------------------------------------
  // Base interface for multiple testing correction policies.
  // Templated on Decimal and the derived class's unique PermutationTestResultType,
  // so that addStrategy's parameter is strongly typed.
  //-----------------------------------------------------------------------------
  template <class Decimal, class PermutationTestResultType>
  class MultipleTestingPolicy {
  public:
    // Default constructor: explicitly initialize the data member and correction flag.
    MultipleTestingPolicy()
      : data(),
	multiTestingCorrectionPerformed(false)
    {}

    // Copy constructor: note that we copy only the containers and flag, not the mutexes.
    MultipleTestingPolicy(const MultipleTestingPolicy& rhs)
      : multiTestingCorrectionPerformed(rhs.multiTestingCorrectionPerformed)
    {
      { // Lock the source mutex for strategies and copy the corresponding containers.
	boost::mutex::scoped_lock Lock(rhs.data.strategiesMutex);
	data.strategyContainer = rhs.data.strategyContainer;
	data.maxTestStatStrategies = rhs.data.maxTestStatStrategies;
      }
      {
	// Lock the source mutex for surviving strategies and copy the container.
	boost::mutex::scoped_lock Lock(rhs.data.survivingStrategiesMutex);
	data.survivingStrategiesContainer = rhs.data.survivingStrategiesContainer;
      }
      // The mutex members (data.strategiesMutex and data.survivingStrategiesMutex)
      // are not copied; they are default-constructed in 'data'.
    }

    // Assignment operator: again, copy over the containers and flag while ignoring the mutexes.
    MultipleTestingPolicy& operator=(const MultipleTestingPolicy& rhs)
    {
      if (this == &rhs)
	return *this;

      // Copy correction flag.
      multiTestingCorrectionPerformed = rhs.multiTestingCorrectionPerformed;

      { // Lock both our and the source's strategies mutex and copy the container data.
	boost::mutex::scoped_lock Lock1(this->data.strategiesMutex);
	boost::mutex::scoped_lock Lock2(rhs.data.strategiesMutex);
	data.strategyContainer = rhs.data.strategyContainer;
	data.maxTestStatStrategies = rhs.data.maxTestStatStrategies;
      }
      { // Lock both our and the source's surviving strategies mutex and copy the container data.
	boost::mutex::scoped_lock Lock1(this->data.survivingStrategiesMutex);
	boost::mutex::scoped_lock Lock2(rhs.data.survivingStrategiesMutex);
	data.survivingStrategiesContainer = rhs.data.survivingStrategiesContainer;
      }
      // Mutexes themselves remain as originally constructed.
      return *this;
    }
    
    virtual ~MultipleTestingPolicy() {}

    // Unified interface: first argument is of the type defined by the derived class.
    virtual void addStrategy(const PermutationTestResultType& permutationResult,
			     std::shared_ptr<PalStrategy<Decimal>> aStrategy) = 0;

    virtual void correctForMultipleTests() = 0;

    // Public getters that mirror the original MultipleTestingCorrection interface.
    size_t getNumMultiComparisonStrategies() const {
      return data.strategyContainer.size();
    }

    size_t getNumSurvivingStrategies() const {
      return data.survivingStrategiesContainer.size();
    }

    typename MultipleTestingData<Decimal>::SortedStrategyContainer::const_iterator beginSortedStrategy() const {
      return data.strategyContainer.begin();
    }
    typename MultipleTestingData<Decimal>::SortedStrategyContainer::const_iterator endSortedStrategy() const {
      return data.strategyContainer.end();
    }
    typename MultipleTestingData<Decimal>::SortedStrategyContainer::const_reverse_iterator reverseBeginSortedStrategy() const {
      return data.strategyContainer.rbegin();
    }
    typename MultipleTestingData<Decimal>::SortedStrategyContainer::const_reverse_iterator reverseEndSortedStrategy() const {
      return data.strategyContainer.rend();
    }
    typename MultipleTestingData<Decimal>::SurvivingStrategyContainer::const_iterator beginSurvivingStrategies() const {
      return data.survivingStrategiesContainer.begin();
    }
    typename MultipleTestingData<Decimal>::SurvivingStrategyContainer::const_iterator endSurvivingStrategies() const {
      return data.survivingStrategiesContainer.end();
    }

  protected:
    // Helper to add a surviving strategy.
    void addSurvivingStrategy(std::shared_ptr<PalStrategy<Decimal>> strategy) {
      boost::mutex::scoped_lock Lock(data.survivingStrategiesMutex);
      data.survivingStrategiesContainer.push_back(strategy);
    }

    void setMultipleTestingCorrectionPerformed() {
      multiTestingCorrectionPerformed = true;
    }
 

  protected:
    // The common data object that holds our containers.
    MultipleTestingData<Decimal> data;

    // Flag to indicate that multiple testing correction was performed.
    bool multiTestingCorrectionPerformed;
  };

  //-----------------------------------------------------------------------------
  // BenjaminiHochbergFdr policy.
  // Uses a Decimal as the PermutationTestResultType.
  //-----------------------------------------------------------------------------
  template <class Decimal>
  class BenjaminiHochbergFdr : public MultipleTestingPolicy<Decimal, typename PValueReturnPolicy<Decimal>::ReturnType>
  {
  public:
    using BenjaminiReturnType = typename PValueReturnPolicy<Decimal>::ReturnType;

    BenjaminiHochbergFdr()
      : MultipleTestingPolicy<Decimal, BenjaminiReturnType>(),
	mFalseDiscoveryRate(DecimalConstants<Decimal>::DefaultFDR)
    {}

    BenjaminiHochbergFdr (const BenjaminiHochbergFdr<Decimal>& rhs)
      : MultipleTestingPolicy<Decimal, BenjaminiReturnType> (rhs),
	mFalseDiscoveryRate (rhs.mFalseDiscoveryRate)
    {}

    BenjaminiHochbergFdr<Decimal>&
    operator=(const BenjaminiHochbergFdr<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	MultipleTestingPolicy<Decimal, BenjaminiReturnType>::operator=(rhs);
	mFalseDiscoveryRate = rhs.mFalseDiscoveryRate;
	return *this;
      }
    
    void addStrategy(const BenjaminiReturnType& pValue,
		     std::shared_ptr<PalStrategy<Decimal>> aStrategy) override
    {
      boost::mutex::scoped_lock Lock(this->data.strategiesMutex);
      this->data.strategyContainer.insert(std::make_pair(pValue, aStrategy));
    }

    void correctForMultipleTests() override {
      auto it = this->data.strategyContainer.rbegin();
      auto itEnd = this->data.strategyContainer.rend();

      Decimal numTests(static_cast<int>(this->getNumMultiComparisonStrategies()));
      Decimal rank = numTests;
      Decimal criticalValue = 0;

      // Iterate in reverse order to find the largest p-value meeting the criterion.
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
      // Add the remaining strategies to the surviving container.
      while (it != itEnd) {
	boost::mutex::scoped_lock Lock(this->data.survivingStrategiesMutex);
	this->data.survivingStrategiesContainer.push_back(it->second);
	++it;
      }
      std::cout << "BenjaminiHochbergFdr::Number of multi comparison strategies = " << this->getNumMultiComparisonStrategies() << std::endl;
      std::cout << "BenjaminiHochbergFdr::Number of surviving strategies after correction = " << this->getNumSurvivingStrategies() << std::endl;
      this->setMultipleTestingCorrectionPerformed();
    }

  private:
    Decimal mFalseDiscoveryRate;
  };

  //-----------------------------------------------------------------------------
  // AdaptiveBenjaminiHochbergYr2000 policy.
  // Uses a Decimal as the PermutationTestResultType.
  //-----------------------------------------------------------------------------
  template <class Decimal>
  class AdaptiveBenjaminiHochbergYr2000 : public MultipleTestingPolicy<Decimal,
								       typename PValueReturnPolicy<Decimal>::ReturnType>
  {
  public:
    using AdaptiveBenjaminiReturnType = typename PValueReturnPolicy<Decimal>::ReturnType;

    AdaptiveBenjaminiHochbergYr2000()
      : MultipleTestingPolicy<Decimal, AdaptiveBenjaminiReturnType>(),
	mFalseDiscoveryRate(DecimalConstants<Decimal>::DefaultFDR),
	mSlopes()	
    {}

    ~AdaptiveBenjaminiHochbergYr2000()
    {}
    
    AdaptiveBenjaminiHochbergYr2000 (const AdaptiveBenjaminiHochbergYr2000<Decimal>& rhs)
      : MultipleTestingPolicy<Decimal, AdaptiveBenjaminiReturnType> (rhs),
      mFalseDiscoveryRate (rhs.mFalseDiscoveryRate),
      mSlopes(rhs.mSlopes)
    {}

    AdaptiveBenjaminiHochbergYr2000<Decimal>&
    operator=(const AdaptiveBenjaminiHochbergYr2000<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;
      
      MultipleTestingPolicy<Decimal, AdaptiveBenjaminiReturnType>::operator=(rhs);
      mFalseDiscoveryRate = rhs.mFalseDiscoveryRate;
      mSlopes = rhs.mSlopes;
      return *this;
    }
    
    void addStrategy(const AdaptiveBenjaminiReturnType& pValue,
		     std::shared_ptr<PalStrategy<Decimal>> aStrategy) override
    {
      boost::mutex::scoped_lock Lock(this->data.strategiesMutex);
      this->data.strategyContainer.insert(std::make_pair(pValue, aStrategy));
    }

    void correctForMultipleTests() override {
      auto it = this->data.strategyContainer.rbegin();
      auto itEnd = this->data.strategyContainer.rend();
      Decimal rank(static_cast<int>(this->getNumMultiComparisonStrategies()));
      Decimal criticalValue = 0;

      calculateSlopes();
      Decimal numTests = calculateMPrime();

      std::cout << "AdaptiveBenjaminiHochbergYr2000:: mPrime = " << numTests
		<< ", m = " << this->getNumMultiComparisonStrategies() << std::endl;

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
      while (it != itEnd) {
	boost::mutex::scoped_lock Lock(this->data.survivingStrategiesMutex);
	this->data.survivingStrategiesContainer.push_back(it->second);
	++it;
      }
      std::cout << "AdaptiveBenjaminiHochbergYr2000::Number of multi comparison strategies = " << this->getNumMultiComparisonStrategies() << std::endl;
      std::cout << "AdaptiveBenjaminiHochbergYr2000::Number of surviving strategies after correction = " << this->getNumSurvivingStrategies() << std::endl;
      // Finally, mark that multiple testing correction was performed.
      this->setMultipleTestingCorrectionPerformed();
    }

  private:
    void calculateSlopes() {
      if (this->getNumMultiComparisonStrategies() > 0) {
	Decimal m(static_cast<int>(this->getNumMultiComparisonStrategies()));
	Decimal i = DecimalConstants<Decimal>::DecimalOne;
	for (auto it = this->data.strategyContainer.begin(); it != this->data.strategyContainer.end(); ++it) {
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
      Decimal m(static_cast<int>(this->getNumMultiComparisonStrategies()));
      for (unsigned int i = 1; i < mSlopes.size(); i++) {
	if (mSlopes[i] < mSlopes[i - 1]) {
	  Decimal temp = (DecimalConstants<Decimal>::DecimalOne / mSlopes[i]) + DecimalConstants<Decimal>::DecimalOne;
	  std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime, i = " << i
		    << ", mslopes[" << i << "] = " << mSlopes[i]
		    << ", mslopes[" << i - 1 << "] = " << mSlopes[i - 1] << std::endl;
	  std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime = " << temp << std::endl;
	  return std::min(temp, m);
	}
      }
      return m;
    }

    Decimal mFalseDiscoveryRate;
    std::vector<Decimal> mSlopes;
  };

  //-----------------------------------------------------------------------------
  // RomanoWolfStepdownCorrection policy.
  // Uses a tuple {pValue, maxTestStat} as its PermutationTestResultType.
  //-----------------------------------------------------------------------------
  template <class Decimal>
  class RomanoWolfStepdownCorrection : public MultipleTestingPolicy<Decimal,
								    typename PValueAndTestStatisticReturnPolicy<Decimal>::ReturnType>
  {
  public:
    using RomanoWolfReturnType = typename PValueAndTestStatisticReturnPolicy<Decimal>::ReturnType;
    
    RomanoWolfStepdownCorrection()
      : MultipleTestingPolicy<Decimal, RomanoWolfReturnType>()
    {}

    RomanoWolfStepdownCorrection (const RomanoWolfStepdownCorrection<Decimal>& rhs)
      : MultipleTestingPolicy<Decimal, RomanoWolfReturnType>(rhs)
    {}

    RomanoWolfStepdownCorrection<Decimal>&
    operator=(const RomanoWolfStepdownCorrection<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;

      MultipleTestingPolicy<Decimal, RomanoWolfReturnType>::operator=(rhs);
      return *this;
    }
    
    void addStrategy(const RomanoWolfReturnType& result,
		     std::shared_ptr<PalStrategy<Decimal>> aStrategy) override
    {
      Decimal pValue = std::get<0>(result);
      Decimal maxTestStat = std::get<1>(result);
      boost::mutex::scoped_lock Lock(this->data.strategiesMutex);
      
      // Store the tuple (pValue, maxTestStat, strategy) in the max test statistic container.
      this->data.maxTestStatStrategies.emplace_back(pValue, maxTestStat, aStrategy);
    }

    void correctForMultipleTests() override
    {
      // If there are no strategies stored for max test statistics, then just return.
      if (this->data.maxTestStatStrategies.empty())
	return;

      // Sort the max test statistic container by the stored p-value (ascending).
      auto& vec = this->data.maxTestStatStrategies;
      std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
	return std::get<0>(a) < std::get<0>(b);
      });

      // Extract empirical null distribution: vector of max test statistics.
      std::vector<Decimal> sortedNull;
      for (const auto& entry : vec)
	{
	  sortedNull.push_back(std::get<1>(entry));
	}
      
      std::sort(sortedNull.begin(), sortedNull.end());

      // Stepdown procedure with proper empirical p-value calculation.
      Decimal previousAdjustedP = Decimal(1.0);
      unsigned int totalStrategies = (unsigned int) vec.size();

      for (unsigned int i = 0; i < totalStrategies; ++i)
	{
	  Decimal originalPValue = std::get<0>(vec[i]);

	  // Compute empirical p-value using upper_bound.
	  auto ub = std::upper_bound(sortedNull.begin(), sortedNull.end(), originalPValue);
	  Decimal empiricalP = static_cast<Decimal>(ub - sortedNull.begin());
	  empiricalP = empiricalP / static_cast<Decimal>((unsigned int) sortedNull.size());

	  // Adjust empirical p-value using stepdown scaling and ensure monotonicity.
	  Decimal adjustedP = std::min(previousAdjustedP,
				       empiricalP * (static_cast<Decimal>(totalStrategies) / static_cast<Decimal>((i + 1))));
	  previousAdjustedP = adjustedP;

	  // Update the tuple with the adjusted p-value.
	  std::get<0>(vec[i]) = adjustedP;
	}

      // Once p-values are adjusted, move strategies that meet the criterion into surviving container.
      // Here we assume a strategy “survives” if its adjusted p-value is below the significance threshold.
      for (const auto& tup : vec)
	{
	  Decimal adjustedPValue = std::get<0>(tup);
	  if (adjustedPValue < DecimalConstants<Decimal>::SignificantPValue)
	    this->addSurvivingStrategy(std::get<2>(tup));
	}

      // Finally, mark that multiple testing correction was performed.
      this->setMultipleTestingCorrectionPerformed();
    }
  };

  //-----------------------------------------------------------------------------
  // UnadjustedPValueStrategySelection policy.
  // Uses a Decimal as the PermutationTestResultType and acts as a stand-in
  // when no multiple testing correction is performed.
  // Its correctForMultipleTests() is empty, and selectSurvivingStrategies()
  // copies all strategies into the surviving container.
  //-----------------------------------------------------------------------------
  template <class Decimal>
  class UnadjustedPValueStrategySelection : public MultipleTestingPolicy<Decimal,
									 typename PValueReturnPolicy<Decimal>::ReturnType>
  {
  public:
    using UnadjustedReturnType = typename PValueReturnPolicy<Decimal>::ReturnType;
    
    UnadjustedPValueStrategySelection()
      : MultipleTestingPolicy<Decimal, UnadjustedReturnType>()
    {}

    UnadjustedPValueStrategySelection (const UnadjustedPValueStrategySelection<Decimal>& rhs)
      : MultipleTestingPolicy<Decimal, UnadjustedReturnType>(rhs)
    {}

    UnadjustedPValueStrategySelection<Decimal>&
    operator=(const UnadjustedPValueStrategySelection<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;

      MultipleTestingPolicy<Decimal, UnadjustedReturnType>::operator=(rhs);
      return *this;
    }

    void addStrategy(const UnadjustedReturnType& pValue,
		     std::shared_ptr<PalStrategy<Decimal>> aStrategy) override
    {
      boost::mutex::scoped_lock Lock(this->data.strategiesMutex);
      this->data.strategyContainer.insert(std::make_pair(pValue, aStrategy));
    }

    void correctForMultipleTests() override {
      // No correction is performed.

      this->setMultipleTestingCorrectionPerformed();

      for (auto & entry : this->data.strategyContainer) {
	if (entry.first < DecimalConstants<Decimal>::SignificantPValue)
	  this->addSurvivingStrategy(entry.second);
      }
    }
  };
}

#endif
