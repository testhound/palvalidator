// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, November 2017
//

#ifndef __UNADJUSTED_PVALUE_STRATEGY_SELECTION_H
#define __UNADJUSTED_PVALUE_STRATEGY_SELECTION_H 1

#include <list>
#include "number.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include <boost/thread/mutex.hpp>

namespace mkc_timeseries
{
  template <class Decimal> class UnadjustedPValueStrategySelection
  {
  public:
    typedef typename std::list<std::shared_ptr<PalStrategy<Decimal>>> SurvivingStrategyContainer;
    typedef typename SurvivingStrategyContainer::const_iterator ConstSurvivingStrategiesIterator;
    
  public:
    UnadjustedPValueStrategySelection ()
      : mSurvivingStrategiesContainer(),
	mSurvivingStrategiesMutex()
    {}

    ~UnadjustedPValueStrategySelection ()
    {}

    UnadjustedPValueStrategySelection (const UnadjustedPValueStrategySelection& rhs)
      : mSurvivingStrategiesContainer (rhs.mSurvivingStrategiesContainer),
	mSurvivingStrategiesMutex(rhs.mSurvivingStrategiesMutex)
    {}

    UnadjustedPValueStrategySelection&
    operator=(const UnadjustedPValueStrategySelection& rhs)
      {
	if (this == &rhs)
	  return *this;

	mSurvivingStrategiesContainer = mSurvivingStrategiesContainer;
	mSurvivingStrategiesMutex = rhs.mSurvivingStrategiesMutex;
	
	return *this;
      }

    void addStrategy(const Decimal& pValue,
		     std::shared_ptr<PalStrategy<Decimal>> aStrategy)
    {
      if (pValue < DecimalConstants<Decimal>::SignificantPValue)
	{
	  boost::mutex::scoped_lock Lock(mSurvivingStrategiesMutex);
	  mSurvivingStrategiesContainer.push_back (aStrategy);
	  std::cout << "UnadjustedPValueSelection::Found surviving strategy with pValue = " << pValue  << std::endl;
	}
    }

    void selectSurvivingStrategies()
    {}
    
    size_t getNumSurvivingStrategies() const
    {
      return mSurvivingStrategiesContainer.size();
    }

    UnadjustedPValueStrategySelection::ConstSurvivingStrategiesIterator beginSurvivingStrategies() const
    {
      return mSurvivingStrategiesContainer.begin();
    }

    UnadjustedPValueStrategySelection::ConstSurvivingStrategiesIterator endSurvivingStrategies() const
    {
      return mSurvivingStrategiesContainer.end();
    }

  private:
    SurvivingStrategyContainer mSurvivingStrategiesContainer;
    boost::mutex mSurvivingStrategiesMutex;
  };
}
#endif
