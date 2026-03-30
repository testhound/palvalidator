// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

/**
 * @file SummaryStats.h
 * @brief Lightweight accumulator for median, min, max, and robust Qn scale.
 */

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
  /**
   * @brief Accumulates numeric values and provides summary statistics.
   *
   * Stores all values in memory to support order-statistic queries
   * (median, min, max) and the robust Qn scale estimator.
   *
   * @tparam Decimal Numeric type (e.g., dec::decimal<8>).
   */
  template <class Decimal> class SummaryStats
  {
  public:
    SummaryStats ()
      : mNumValues ()
    {
    }

    ~SummaryStats()
    {}

    /// @brief Appends a value to the collection.
    void addValue (const Decimal& value)
    {
      mNumValues.push_back(value);
    }

    /// @brief Returns the median of all accumulated values.
    Decimal getMedian() const
    {
      return MedianOfVec<Decimal> (mNumValues);
    }

    /// @brief Returns the maximum value.
    Decimal getLargestValue() const
    {
      return *std::max_element (mNumValues.begin(), mNumValues.end());
    }

    /// @brief Returns the minimum value.
    Decimal getSmallestValue() const
    {
      return *std::min_element (mNumValues.begin(), mNumValues.end());
    }

    /// @brief Returns the robust Qn scale estimator of the accumulated values.
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
