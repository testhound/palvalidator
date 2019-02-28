// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef DRAW_RANDOM_H_
#define DRAW_RANDOM_H_

#include "pcg_random.hpp"
#include "pcg_extras.hpp"
#include "randutils.hpp"
#include <ext/random>
#include <vector>
#include <iostream>

using uint32 = unsigned int;

// We typically use four years of daily OOS bars
#define TYPICAL_NUM_OOS_BARS 1040

template <typename RngType, typename RngType::result_type UpperBound = TYPICAL_NUM_OOS_BARS>
class BoundedRandThreshold
{
public:
  BoundedRandThreshold()
    : mThresholdContainer()
  {
    mThresholdContainer.reserve(UpperBound);
    typename RngType::result_type threshold;
    typedef typename RngType::result_type rtype;

    mThresholdContainer.push_back(0);
    
    for (typename RngType::result_type i = 1; i <= UpperBound + 1; i++)
      {
	threshold = (RngType::max() - RngType::min() + rtype(1) - i)
                    % i;
	std::cout << "Threshold = " << threshold << std::endl;
	mThresholdContainer.push_back(threshold);
      }
  }

  typename RngType::result_type getThreshold (typename RngType::result_type upper_bound)
  {
    return mThresholdContainer.at(upper_bound);
  }
  
private:
  std::vector<typename RngType::result_type> mThresholdContainer;
};

template <typename StdRandomEngine>
class DrawRandomNumber 
{
public:
  DrawRandomNumber()
    : mRandGen()
  {}

  DrawRandomNumber(const DrawRandomNumber<StdRandomEngine>& rhs)
    : mRandGen (rhs.mRandGen)
  {}

  DrawRandomNumber<StdRandomEngine>&
    operator=(const DrawRandomNumber<StdRandomEngine> &rhs)
    {
      if (this == &rhs)
	return *this;

      mRandGen = rhs.mRandGen;
      return *this;
    }

  typename StdRandomEngine::result_type DrawNumber(typename StdRandomEngine::result_type min,
						   typename StdRandomEngine::result_type max)
  {
    return mRandGen.uniform(min, max);
  }

  typename StdRandomEngine::result_type DrawNumber(typename StdRandomEngine::result_type maxNum)
  {
    typedef typename StdRandomEngine::result_type rtype;
    rtype threshold = mPreComputedThreshold.getThreshold(maxNum + 1);

    for (;;)
      {
        rtype r = mRandGen.engine()() - StdRandomEngine::min();
        if (r >= threshold)
	  return r % maxNum;
      }

  }
  
private:
  randutils::random_generator<StdRandomEngine> mRandGen; // Wrap in pcg randutils to get good seeding
  static BoundedRandThreshold<StdRandomEngine> mPreComputedThreshold;
};

template <typename StdRandomEngine> BoundedRandThreshold<StdRandomEngine> DrawRandomNumber<StdRandomEngine>::mPreComputedThreshold;
#endif
