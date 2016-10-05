// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIME_FRAME_UTILITY_H
#define __TIME_FRAME_UTILITY_H 1

#include <string>
#include "TimeFrame.h"

namespace mkc_timeseries
{
  extern TimeFrame::Duration getTimeFrameFromString(const std::string& timeFrameString);
}

#endif


