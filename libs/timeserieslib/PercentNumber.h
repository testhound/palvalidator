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
#include "decimal.h"
#include "DecimalConstants.h"

using dec::decimal;
using std::map;
using std::shared_ptr;


namespace mkc_timeseries
{
  template <int Prec> class PercentNumber
  {
  public:
    static const PercentNumber<Prec> createPercentNumber (const dec::decimal<Prec>& number)
    {
      boost::mutex::scoped_lock Lock(mNumberCacheMutex);

      typename map<decimal<Prec>, shared_ptr<PercentNumber>>::iterator it = mNumberCache.find (number);

      if (it != mNumberCache.end())
	return *(it->second);
      else
	{
	  std::shared_ptr<PercentNumber> p (new PercentNumber(number));
	  mNumberCache.insert(std::make_pair (number, p));
	  return *p;
	}
    }

    static const PercentNumber<Prec> createPercentNumber (const std::string& numberString)
    {
      dec::decimal<Prec> decNum(dec::fromString<dec::decimal<Prec>>(numberString));
      return PercentNumber<Prec>::createPercentNumber (decNum);
    }

    const dec::decimal<Prec>& getAsPercent() const
    {
      return mPercentNumber;
    }

    PercentNumber(const PercentNumber<Prec>& rhs)
      : mPercentNumber(rhs.mPercentNumber)
    {}

    PercentNumber& 
    operator=(const PercentNumber<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      mPercentNumber = rhs.mPercentNumber;
      return *this;
    }

    ~PercentNumber()
    {}

  private:
    PercentNumber (const dec::decimal<Prec>& number) 
      : mPercentNumber (number / DecimalConstants<Prec>::DecimalOneHundred)
    {}

  private:
    static boost::mutex  mNumberCacheMutex;
    static std::map<dec::decimal<Prec>, std::shared_ptr<PercentNumber>> mNumberCache;

    dec::decimal<Prec> mPercentNumber;
  };

  template <int Prec>
  bool operator< (const PercentNumber<Prec>& lhs, const PercentNumber<Prec>& rhs)
  {
    return lhs.getAsPercent() < rhs.getAsPercent();
   }

  template <int Prec>
  bool operator> (const PercentNumber<Prec>& lhs, const PercentNumber<Prec>& rhs){ return rhs < lhs; }

  template <int Prec>
  bool operator<=(const PercentNumber<Prec>& lhs, const PercentNumber<Prec>& rhs){ return !(lhs > rhs); }
  
  template <int Prec>
  bool operator>=(const PercentNumber<Prec>& lhs, const PercentNumber<Prec>& rhs){ return !(lhs < rhs); }

  template <int Prec>
  bool operator==(const PercentNumber<Prec>& lhs, const PercentNumber<Prec>& rhs)
  {
    return (lhs.getAsPercent() == rhs.getAsPercent());
  }

  template <int Prec>
  bool operator!=(const PercentNumber<Prec>& lhs, const PercentNumber<Prec>& rhs)
  { return !(lhs == rhs); }

  template <int Prec>
  boost::mutex PercentNumber<Prec>::mNumberCacheMutex;

  template <int Prec>
  std::map<dec::decimal<Prec>, std::shared_ptr<PercentNumber<Prec>>> PercentNumber<Prec>::mNumberCache;

  template <int Prec>
  inline PercentNumber<Prec>
  createAPercentNumber (const std::string& numStr)
  {
    return PercentNumber<Prec>::createPercentNumber (createADecimal<Prec>(numStr));
  }
}



#endif
