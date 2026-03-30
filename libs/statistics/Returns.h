// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

/**
 * @file Returns.h
 * @brief Simple return-calculation utilities for price series.
 *
 * Provides fractional and percentage return computations used throughout
 * the bootstrap and back-testing pipelines.
 */

#ifndef __RETURNS_H
#define __RETURNS_H 1

#include "number.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{
  /**
   * @brief Computes the fractional return between two prices.
   *
   * Calculates (secondPrice − referencePrice) / referencePrice.
   *
   * @tparam Decimal Numeric type (e.g., dec::decimal<8>).
   * @param  referencePrice The base price (denominator). Must be non-zero.
   * @param  secondPrice    The comparison price (numerator offset).
   * @return Fractional return as a Decimal value.
   */
  template <class Decimal>
  Decimal calculateReturn (const Decimal& referencePrice,
			   const Decimal& secondPrice)
  {
    return ((secondPrice - referencePrice) / referencePrice);
  }

  /**
   * @brief Computes the percentage return between two prices.
   *
   * Equivalent to calculateReturn() × 100.
   *
   * @tparam Decimal Numeric type (e.g., dec::decimal<8>).
   * @param  referencePrice The base price. Must be non-zero.
   * @param  secondPrice    The comparison price.
   * @return Percentage return as a Decimal value.
   */
  template <class Decimal>
  Decimal PercentReturn (const Decimal& referencePrice,
				  const Decimal& secondPrice)
  {
    return (calculateReturn<Decimal>(referencePrice, secondPrice) *
	    DecimalConstants<Decimal>::DecimalOneHundred);
  }
}

#endif
