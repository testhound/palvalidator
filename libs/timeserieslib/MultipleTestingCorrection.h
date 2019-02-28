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
#include "PalStrategy.h"
#include <boost/thread/mutex.hpp>

namespace mkc_timeseries
{
  template <class Decimal> class MultipleTestingCorrection
  {
  public:
    typedef typename std::multimap<Decimal, std::shared_ptr<PalStrategy<Decimal>>> SortedStrategyContainer;
    typedef typename SortedStrategyContainer::const_iterator ConstSortedStrategyIterator;
    typedef typename SortedStrategyContainer::const_reverse_iterator ConstReverseSortedStrategyIterator;

    typedef typename std::list<std::shared_ptr<PalStrategy<Decimal>>> SurvivingStrategyContainer;
    typedef typename SurvivingStrategyContainer::const_iterator ConstSurvivingStrategiesIterator;
    
  public:
    virtual void correctForMultipleTests() = 0;
    virtual void selectSurvivingStrategies() = 0;
    
    void addStrategy(const Decimal& pValue,
		     std::shared_ptr<PalStrategy<Decimal>> aStrategy)
    {
      boost::mutex::scoped_lock Lock(mStrategiesMutex);
      mStrategyContainer.insert (std::make_pair (pValue, aStrategy));
    }

    
    MultipleTestingCorrection (const MultipleTestingCorrection& rhs)
      : mStrategyContainer (rhs.mStrategyContainer),
	mSurvivingStrategiesContainer (rhs.mSurvivingStrategiesContainer),
	mSurvivingStrategiesMutex(rhs.mSurvivingStrategiesMutex),
	mStrategiesMutex(rhs.mStrategiesMutex),
	mMultiTestingCorrectionPerformed(rhs.mMultiTestingCorrectionPerformed)
    {}
    
    MultipleTestingCorrection&
    operator=(const MultipleTestingCorrection& rhs)
      {
	if (this == &rhs)
	  return *this;

	mStrategyContainer = rhs.mStrategyContainer;
	mSurvivingStrategiesContainer = mSurvivingStrategiesContainer;
	mSurvivingStrategiesMutex = rhs.mSurvivingStrategiesMutex;
	mStrategiesMutex = rhs.mStrategiesMutex;
	mMultiTestingCorrectionPerformed = rhs.mMultiTestingCorrectionPerformed;
	
	return *this;
      }

    size_t getNumMultiComparisonStrategies() const
    {
      return mStrategyContainer.size();
    }

    size_t getNumSurvivingStrategies() const
    {
      return mSurvivingStrategiesContainer.size();
    }
    
    MultipleTestingCorrection::ConstSortedStrategyIterator beginSortedStrategy() const
    {
      return mStrategyContainer.begin();
    }

    MultipleTestingCorrection::ConstSortedStrategyIterator endSortedStrategy() const
    {
      return mStrategyContainer.end();
    }

    MultipleTestingCorrection::ConstReverseSortedStrategyIterator reverseBeginSortedStrategy() const
    {
      return mStrategyContainer.rbegin();
    }

    MultipleTestingCorrection::ConstReverseSortedStrategyIterator reverseEndSortedStrategy() const
    {
      return mStrategyContainer.rend();
    }

    MultipleTestingCorrection::ConstSurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mSurvivingStrategiesContainer.begin();
    }

    MultipleTestingCorrection::ConstSurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mSurvivingStrategiesContainer.end();
    }

  protected:
    MultipleTestingCorrection() :
      mStrategyContainer(),
      mSurvivingStrategiesContainer(),
      mSurvivingStrategiesMutex(),
      mStrategiesMutex(),
      mMultiTestingCorrectionPerformed (false)
    {}
    
    virtual ~MultipleTestingCorrection()
    {}

    void addSurvivingStrategy (std::shared_ptr<PalStrategy<Decimal>> strategy)
    {
      boost::mutex::scoped_lock Lock(mSurvivingStrategiesMutex);
      mSurvivingStrategiesContainer.push_back (strategy);
    }

    void multipleTestingCorrectionPerformed()
    {
      mMultiTestingCorrectionPerformed = true;
    }

  private:
    SortedStrategyContainer mStrategyContainer;
    SurvivingStrategyContainer mSurvivingStrategiesContainer;
    boost::mutex mSurvivingStrategiesMutex;
    boost::mutex mStrategiesMutex;
    bool mMultiTestingCorrectionPerformed;
  };

  //
  // Class that implements Banjamini & Hochberg False Discovery Rate Correction
  //
  
  template <class Decimal> class BenjaminiHochbergFdr :
    public MultipleTestingCorrection<Decimal>
  {
  public:
    BenjaminiHochbergFdr ()
      : MultipleTestingCorrection<Decimal>(),
	mFalseDiscoveryRate (DecimalConstants<Decimal>::DefaultFDR)
    {}

    ~BenjaminiHochbergFdr()
    {}
    
    BenjaminiHochbergFdr (const BenjaminiHochbergFdr<Decimal>& rhs)
      : MultipleTestingCorrection<Decimal> (rhs),
	mFalseDiscoveryRate (rhs.mFalseDiscoveryRate)
    {}

    BenjaminiHochbergFdr<Decimal>&
    operator=(const BenjaminiHochbergFdr<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	MultipleTestingCorrection<Decimal>::operator=(rhs);
	mFalseDiscoveryRate = rhs.mFalseDiscoveryRate;
	return *this;
      }

    void selectSurvivingStrategies()
    {
      correctForMultipleTests();
      this->multipleTestingCorrectionPerformed();
    }
    
    void correctForMultipleTests()
    {
      typename MultipleTestingCorrection<Decimal>::ConstReverseSortedStrategyIterator it = this->reverseBeginSortedStrategy();
      typename MultipleTestingCorrection<Decimal>::ConstReverseSortedStrategyIterator itEnd = this->reverseEndSortedStrategy();

      Decimal numTests ((int) this->getNumMultiComparisonStrategies());
      Decimal pValue (0);
      Decimal rank (numTests);
      Decimal criticalValue (0);

      // Starting searching at end of sorted map to find find largest pValue < (rank/numTests) * FDR
      
      for (; it != itEnd; it++)
	{
	  pValue = it->first;
	  criticalValue = (rank/numTests) * mFalseDiscoveryRate;

	  std::cout << "BenjaminiHochbergFdr:: pValue = " << pValue << ", Critical value = " << criticalValue << std::endl;
	  
	  if (pValue < criticalValue)
	    break;
	  else
	    rank = rank - DecimalConstants<Decimal>::DecimalOne;
	}

      // If we are not at the end add any remaining strategies to surviving strategies list
      while (it != itEnd)
	{
	  this->addSurvivingStrategy (it->second);
	  it++;
	}

      std::cout << "BenjaminiHochbergFdr::Number of multi comparison strategies = " << this->getNumMultiComparisonStrategies()  << std::endl;
      std::cout << "BenjaminiHochbergFdr::NUmber of surviving strategies after correction = " << this->getNumSurvivingStrategies() << std::endl;
	
    }
    
  private:
    Decimal mFalseDiscoveryRate;
  };


    //
  // Class that implements Banjamini & Hochberg False Discovery Rate Correction
  //
  
  template <class Decimal> class AdaptiveBenjaminiHochbergYr2000 :
    public MultipleTestingCorrection<Decimal>
  {
  public:
    AdaptiveBenjaminiHochbergYr2000 ()
      : MultipleTestingCorrection<Decimal>(),
      mFalseDiscoveryRate (DecimalConstants<Decimal>::DefaultFDR),
      mSlopes()
    {}

    ~AdaptiveBenjaminiHochbergYr2000()
    {}
    
    AdaptiveBenjaminiHochbergYr2000 (const AdaptiveBenjaminiHochbergYr2000<Decimal>& rhs)
      : MultipleTestingCorrection<Decimal> (rhs),
      mFalseDiscoveryRate (rhs.mFalseDiscoveryRate),
      mSlopes(rhs.mSlopes)
    {}

    AdaptiveBenjaminiHochbergYr2000<Decimal>&
    operator=(const AdaptiveBenjaminiHochbergYr2000<Decimal>& rhs)
      {
	if (this == &rhs)
	  return *this;

	MultipleTestingCorrection<Decimal>::operator=(rhs);
	mFalseDiscoveryRate = rhs.mFalseDiscoveryRate;
	mSlopes = rhs.mSlopes;
	return *this;
      }

    void selectSurvivingStrategies()
    {
      correctForMultipleTests();
      this->multipleTestingCorrectionPerformed();
    }

    void correctForMultipleTests()
    {
      typename MultipleTestingCorrection<Decimal>::ConstReverseSortedStrategyIterator it = this->reverseBeginSortedStrategy();
      typename MultipleTestingCorrection<Decimal>::ConstReverseSortedStrategyIterator itEnd = this->reverseEndSortedStrategy();
      Decimal pValue (0);
      Decimal rank ((int) this->getNumMultiComparisonStrategies());
      Decimal criticalValue (0);

      calculateSlopes();

      Decimal numTests (calculateMPrime());

      std::cout << "AdaptiveBenjaminiHochbergYr2000:: mPrime = " << numTests << ", m = " << this->getNumMultiComparisonStrategies() << std::endl;
      // Starting searching at end of sorted map to find find largest pValue < (rank/numTests) * FDR
      
      for (; it != itEnd; it++)
	{
	  pValue = it->first;
	  criticalValue = (rank/numTests) * mFalseDiscoveryRate;

	  std::cout << "AdaptiveBenjaminiHochbergYr2000:: pValue = " << pValue << ", Critical value = " << criticalValue << std::endl;
	  
	  if (pValue < criticalValue)
	    break;
	  else
	    rank = rank - DecimalConstants<Decimal>::DecimalOne;
	}

      // If we are not at the end add any remaining strategies to surviving strategies list
      while (it != itEnd)
	{
	  this->addSurvivingStrategy (it->second);
	  it++;
	}

      std::cout << "AdaptiveBenjaminiHochbergYr2000::Number of multi comparison strategies = " << this->getNumMultiComparisonStrategies()  << std::endl;
      std::cout << "AdaptiveBenjaminiHochbergYr2000::NUmber of surviving strategies after correction = " << this->getNumSurvivingStrategies() << std::endl;
	
    }
  private:
    void calculateSlopes()
    {
      if (this->getNumMultiComparisonStrategies() > 0)
	{
	  Decimal pValue (0);
	  Decimal m ((int) this->getNumMultiComparisonStrategies());
	  Decimal i(DecimalConstants<Decimal>::DecimalOne);
	  Decimal denom(DecimalConstants<Decimal>::DecimalOne);
	  Decimal num(DecimalConstants<Decimal>::DecimalOne);
	  Decimal slope(0);
	  
	  typename MultipleTestingCorrection<Decimal>::ConstSortedStrategyIterator it = this->beginSortedStrategy();
	  typename MultipleTestingCorrection<Decimal>::ConstSortedStrategyIterator itEnd = this->endSortedStrategy();

	  for (; it != itEnd; it++)
	    {
	      pValue = it->first;

	      num = (DecimalConstants<Decimal>::DecimalOne - pValue);
	      denom = (m + DecimalConstants<Decimal>::DecimalOne - i);
	      slope = num/denom;
	      mSlopes.push_back (slope);
	      std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateSlopes, i = " << i << ", slope = " << slope << std::endl;
	      i = i + DecimalConstants<Decimal>::DecimalOne;
	    }
	}
    }

    Decimal calculateMPrime()
    {
      Decimal m ((int) this->getNumMultiComparisonStrategies());
      
      for (unsigned int i = 1; i < mSlopes.size(); i++)
	{
	  if (mSlopes[i] < mSlopes[i - 1])
	    {
	      Decimal temp ((DecimalConstants<Decimal>::DecimalOne/mSlopes[i]) + DecimalConstants<Decimal>::DecimalOne);

	      std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime, i = " << i;
	      std::cout << ", mslopes[" << i << "] = " << mSlopes[i];
	      std::cout << ", mslopes[" << i - 1 << "] = " << mSlopes[i - 1] << std::endl;
	      std::cout << "AdaptiveBenjaminiHochbergYr2000::calculateMPrime = " << temp  << std::endl;
	      return std::min (temp, m);
	    }
	}

      return m;
    }
    
    
  private:
    Decimal mFalseDiscoveryRate;
    std::vector<Decimal> mSlopes;    // Slope of p-values
  };

}

#endif
