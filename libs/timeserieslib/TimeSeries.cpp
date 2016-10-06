// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#include "TimeSeries.h"

namespace mkc_timeseries
{
  boost::mutex TimeSeriesOffset::mOffsetCacheMutex;
  boost::mutex ArrayTimeSeriesIndex::mIndexCacheMutex;
  std::map<unsigned long, std::shared_ptr<TimeSeriesOffset>> TimeSeriesOffset::mOffsetCache;
  std::map<unsigned long, std::shared_ptr<ArrayTimeSeriesIndex>> ArrayTimeSeriesIndex::mIndexCache; 

}

