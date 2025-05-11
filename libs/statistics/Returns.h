// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __RETURNS_H
#define __RETURNS_H 1

#include "number.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{
  template <class Decimal>
  Decimal calculateReturn (const Decimal& referencePrice, 
			   const Decimal& secondPrice)
  {
    return ((secondPrice - referencePrice) / referencePrice);
  }

  template <class Decimal>
  Decimal PercentReturn (const Decimal& referencePrice, 
				  const Decimal& secondPrice)
  {
    return (calculateReturn<Decimal>(referencePrice, secondPrice) *
	    DecimalConstants<Decimal>::DecimalOneHundred);
  }
}

#endif
