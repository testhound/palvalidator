// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __TRADING_POSITION_MANAGER_EXCEPTION_H
#define __TRADING_POSITION_MANAGER EXCEPTION_H 1

#include <exception>

namespace mkc_timeseries
{
  class InstrumentPositionManagerException : public std::runtime_error
  {
  public:
  InstrumentPositionManagerException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~InstrumentPositionManagerException()
      {}
  };
}
#endif
