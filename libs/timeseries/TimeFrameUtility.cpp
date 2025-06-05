// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#include <boost/algorithm/string.hpp>
#include "TimeFrameUtility.h"

namespace mkc_timeseries
{
  TimeFrame::Duration getTimeFrameFromString(const std::string& timeFrameString)
  {
    std::string upperCaseTimeFrameStr = boost::to_upper_copy(timeFrameString);
    
    if (upperCaseTimeFrameStr == std::string("DAILY"))
      return TimeFrame::DAILY;
    else if (upperCaseTimeFrameStr == std::string("INTRADAY"))
      return TimeFrame::INTRADAY;
    else if (upperCaseTimeFrameStr == std::string("WEEKLY"))
      return TimeFrame::WEEKLY;
    else if (upperCaseTimeFrameStr == std::string("MONTHLY"))
      return TimeFrame::MONTHLY;
    else if (upperCaseTimeFrameStr == std::string("QUARTERLY"))
      return TimeFrame::QUARTERLY;
    else
      throw TimeFrameException("getTimeFrame - timeframe string " +upperCaseTimeFrameStr +" not recognized");
  }
}

