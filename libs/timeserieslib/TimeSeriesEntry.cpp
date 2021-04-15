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

  // The default bar start time is 4:00 PM EST since we  are primarily
  // working with equities
  // This should only be used for non-intraday times

  boost::posix_time::time_duration DefaultBarStartTime(boost::posix_time::hours(15));

  time_duration getDefaultBarTime()
  {
    return  DefaultBarStartTime;
  }
 

}
