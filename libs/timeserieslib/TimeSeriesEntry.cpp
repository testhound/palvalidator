// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#include "TimeSeriesEntry.h"

namespace mkc_timeseries
{
  std::shared_ptr<TradingVolume> TradingVolume::ZeroShares(new TradingVolume(0, TradingVolume::SHARES));
  std::shared_ptr<TradingVolume> TradingVolume::ZeroContracts(new TradingVolume(0, TradingVolume::CONTRACTS));

  

}
