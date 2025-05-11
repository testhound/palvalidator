// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SUMMARY_STATS_H
#define __SUMMARY_STATS_H 1

#include <vector>
#include <algorithm>
#include "number.h"
#include "DecimalConstants.h"
#include "TimeSeries.h"
#include "RobustnessTest.h"
#include "TimeSeriesIndicators.h"


namespace mkc_timeseries
{
  template <class Decimal> class SummaryStats
  {
  public:
    SummaryStats ()
      : mNumValues ()
    {
    }

    ~SummaryStats()
    {}

    void addValue (const Decimal& value)
    {
      mNumValues.push_back(value);
    }
    
    Decimal getMedian() const
    {
      return MedianOfVec<Decimal> (mNumValues);
    }

    Decimal getLargestValue() const
    {
      return *std::max_element (mNumValues.begin(), mNumValues.end());
    }

    Decimal getSmallestValue() const
    {
      return *std::min_element (mNumValues.begin(), mNumValues.end());
    }

    Decimal getRobustQn()
    {
      RobustQn<Decimal> statQn;

      return statQn.getRobustQn (mNumValues);
    }
    
  private:

    std::vector<Decimal> mNumValues;
  };
}

#endif
