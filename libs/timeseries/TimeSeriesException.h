// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIMESERIES_EXCEPTION_H
#define __TIMESERIES_EXCEPTION_H 1

#include <stdexcept>
#include <string>

namespace mkc_timeseries
{
  // Original TimeSeriesException (can be used as a base or alongside new ones)
  class TimeSeriesException : public std::runtime_error
  {
  public:
    TimeSeriesException(const std::string msg)
      : std::runtime_error(msg)
    {}

    // Making destructor virtual as it's a base class for exceptions
    virtual ~TimeSeriesException() = default;
  };

  // New Exception Classes (as per PR document)
  class TimeSeriesDataAccessException : public TimeSeriesException
  {
  public:
      explicit TimeSeriesDataAccessException(const std::string& msg) 
        : TimeSeriesException(msg) {}
  };

  class TimeSeriesDataNotFoundException : public TimeSeriesDataAccessException
  {
  public:
      explicit TimeSeriesDataNotFoundException(const std::string& msg) 
        : TimeSeriesDataAccessException(msg) {}
  };

  class TimeSeriesOffsetOutOfRangeException : public TimeSeriesDataAccessException
  {
  public:
      explicit TimeSeriesOffsetOutOfRangeException(const std::string& msg) 
        : TimeSeriesDataAccessException(msg) {}
  };

} // namespace mkc_timeseries

#endif // __TIMESERIES_EXCEPTION_H