// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PERCENT_NUMBER_H
#define __PERCENT_NUMBER_H 1

#include <exception>
#include <memory>
#include <map>
#include <boost/thread/mutex.hpp>
#include "number.h"
#include "DecimalConstants.h"

using std::map;
using std::shared_ptr;


namespace mkc_timeseries
{
  template <class Decimal> class PercentNumber
  {
  public:
    static const PercentNumber<Decimal> createPercentNumber (const Decimal& number)
    {
      boost::mutex::scoped_lock Lock(mNumberCacheMutex);

      typename map<Decimal, shared_ptr<PercentNumber>>::iterator it = mNumberCache.find (number);

      if (it != mNumberCache.end())
	return *(it->second);
      else
	{
	  std::shared_ptr<PercentNumber> p (new PercentNumber(number));
	  mNumberCache.insert(std::make_pair (number, p));
	  return *p;
	}
    }

    static const PercentNumber<Decimal> createPercentNumber (const std::string& numberString)
    {
      Decimal decNum(num::fromString<Decimal>(numberString));
      return PercentNumber<Decimal>::createPercentNumber (decNum);
    }

    const Decimal& getAsPercent() const
    {
      return mPercentNumber;
    }

    PercentNumber(const PercentNumber<Decimal>& rhs)
      : mPercentNumber(rhs.mPercentNumber)
    {}

    PercentNumber& 
    operator=(const PercentNumber<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mPercentNumber = rhs.mPercentNumber;
      return *this;
    }

    ~PercentNumber()
    {}

  private:
    PercentNumber (const Decimal& number) 
      : mPercentNumber (number / DecimalConstants<Decimal>::DecimalOneHundred)
    {}

  private:
    static boost::mutex  mNumberCacheMutex;
    static std::map<Decimal, std::shared_ptr<PercentNumber>> mNumberCache;

    Decimal mPercentNumber;
  };

  template <class Decimal>
  bool operator< (const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs)
  {
    return lhs.getAsPercent() < rhs.getAsPercent();
   }

  template <class Decimal>
  bool operator> (const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs){ return rhs < lhs; }

  template <class Decimal>
  bool operator<=(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs){ return !(lhs > rhs); }
  
  template <class Decimal>
  bool operator>=(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs){ return !(lhs < rhs); }

  template <class Decimal>
  bool operator==(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs)
  {
    return (lhs.getAsPercent() == rhs.getAsPercent());
  }

  template <class Decimal>
  bool operator!=(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs)
  { return !(lhs == rhs); }

  template <class Decimal>
  boost::mutex PercentNumber<Decimal>::mNumberCacheMutex;

  template <class Decimal>
  std::map<Decimal, std::shared_ptr<PercentNumber<Decimal>>> PercentNumber<Decimal>::mNumberCache;

  template <class Decimal>
  inline PercentNumber<Decimal>
  createAPercentNumber (const std::string& numStr)
  {
    return PercentNumber<Decimal>::createPercentNumber (createADecimal<Decimal>(numStr));
  }
}



#endif
