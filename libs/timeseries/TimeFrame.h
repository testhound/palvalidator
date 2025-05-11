// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIME_FRAME_H
#define __TIME_FRAME_H 1

#include <exception>

namespace mkc_timeseries
{
  //
  // class TimeFrameException
  //

  class TimeFrameException : public std::domain_error
  {
  public:
    TimeFrameException(const std::string msg) 
      : std::domain_error(msg)
    {}
    
    ~TimeFrameException()
    {}
    
  };

  class TimeFrame
  {
  public:
    enum Duration {INTRADAY, DAILY, WEEKLY, MONTHLY, QUARTERLY, YEARLY} ;
  };

}

#endif
