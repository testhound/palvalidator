#ifndef __ROUNDING_POLICIES_H
#define __ROUNDING_POLICIES_H 1

// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016

#include "number.h"

namespace mkc_timeseries
{
  template <class Decimal>
  struct NoRounding
  {
    static Decimal round(const Decimal& price,
			 const Decimal& /*tick*/,
			 const Decimal& /*tickDiv2*/)
    {
      return price;
    }
  };

  template <class Decimal>
  struct TickRounding {
    static Decimal round(const Decimal& price,
			 const Decimal& tick,
			 const Decimal& tickDiv2)
    {
      return num::Round2Tick(price, tick, tickDiv2);
    }
  };

} // namespace mkc_timeseries

#endif // __ROUNDING_POLICIES_H