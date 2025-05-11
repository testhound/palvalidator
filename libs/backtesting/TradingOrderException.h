// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TRADING_ORDER_EXCEPTION_H
#define __TRADING_ORDER_EXCEPTION_H 1

#include <exception>

class TradingOrderException : public std::runtime_error
{
public:
TradingOrderException(const std::string msg) 
  : std::runtime_error(msg)
{}

~TradingOrderException()
{}

};

class TradingOrderNotExecutedException : public std::runtime_error
{
public:
TradingOrderNotExecutedException(const std::string msg) 
  : std::runtime_error(msg)
{}

~TradingOrderNotExecutedException()
{}

};

class TradingOrderExecutedException : public std::runtime_error
{
public:
TradingOrderExecutedException(const std::string msg) 
  : std::runtime_error(msg)
{}

~TradingOrderExecutedException()
{}

};


///////////////////////////////////////

class TradingOrderManagerException : public std::runtime_error
{
public:
TradingOrderManagerException(const std::string msg) 
  : std::runtime_error(msg)
{}

~TradingOrderManagerException()
{}

};
#endif
