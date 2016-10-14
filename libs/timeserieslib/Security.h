// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SECURITY_H
#define __SECURITY_H 1

#include <string>
#include <memory>
#include "TimeSeries.h"
#include "DecimalConstants.h"

using dec::decimal;
using std::string;

namespace mkc_timeseries
{
  class SecurityException : public std::runtime_error
  {
  public:
  SecurityException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~SecurityException()
      {}

  };

  template <int Prec>
  class Security
    {
      using Decimal = decimal<Prec>;

    public:
      typedef typename OHLCTimeSeries<Prec>::ConstRandomAccessIterator ConstRandomAccessIterator;

      Security (const string& securitySymbol, const string& securityName,
		const decimal<Prec>& bigPointValue, const decimal<Prec>& securityTick,
		std::shared_ptr<OHLCTimeSeries<Prec>> securityTimeSeries) :
	mSecuritySymbol(securitySymbol),
	mSecurityName(securityName),
	mBigPointValue(bigPointValue),
	mTick(securityTick),
	mSecurityTimeSeries(securityTimeSeries),
	mFirstDate(securityTimeSeries->getFirstDate())
      {}
      
      Security (const Security<Prec> &rhs)
	: mSecuritySymbol(rhs.mSecuritySymbol),
	  mSecurityName(rhs.mSecurityName),
	  mBigPointValue(rhs.mBigPointValue),
	  mTick(rhs.mTick),
	  mSecurityTimeSeries(rhs.mSecurityTimeSeries),
	  mFirstDate(rhs.mFirstDate)
      {}

      Security<Prec>& 
      operator=(const Security<Prec> &rhs)
      {
	if (this == &rhs)
	  return *this;

	mSecuritySymbol = rhs.mSecuritySymbol;
	mSecurityName = rhs.mSecurityName;
	mBigPointValue = rhs.mBigPointValue;
	mTick = rhs.mTick;
	mSecurityTimeSeries = rhs.mSecurityTimeSeries;
	mFirstDate = rhs.mFirstDate;
	return *this;
      }

      virtual ~Security()
      {}

      virtual bool isEquitySecurity() const = 0;
      virtual bool isFuturesSecurity() const = 0;

      Security::ConstRandomAccessIterator findTimeSeriesEntry (const boost::gregorian::date& d) const
	{
	  return mSecurityTimeSeries->getRandomAccessIterator(d);
	}

      Security::ConstRandomAccessIterator getRandomAccessIterator (const boost::gregorian::date& d) const
	{
	  Security::ConstRandomAccessIterator it = mSecurityTimeSeries->getRandomAccessIterator(d);
	  if (it != getRandomAccessIteratorEnd())
	    return mSecurityTimeSeries->getRandomAccessIterator(d);
	  else
	    throw SecurityException ("No time series entry for date: " +boost::gregorian::to_simple_string (d));
	}

      const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const boost::gregorian::date& d) const
	{
	  Security::ConstRandomAccessIterator it = this->getRandomAccessIterator (d);
	  return *(*it);
	}

      Security::ConstRandomAccessIterator getRandomAccessIteratorEnd() const
	{
	  return  mSecurityTimeSeries->endRandomAccess();
	}

      const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const ConstRandomAccessIterator& it, 
						       unsigned long offset) const
      {
	return *mSecurityTimeSeries->getTimeSeriesEntry(it, offset); 
      }

      const boost::gregorian::date&
      getDateValue (const ConstRandomAccessIterator& it, unsigned long offset) const
      {
	return mSecurityTimeSeries->getDateValue(it, offset); 
      }

     const decimal<Prec>& getOpenValue (const ConstRandomAccessIterator& it, 
					unsigned long offset) const
      {
	return mSecurityTimeSeries->getOpenValue(it, offset); 
      }


      const decimal<Prec>& getHighValue (const ConstRandomAccessIterator& it, 
					 unsigned long offset) const
      {
	return mSecurityTimeSeries->getHighValue(it, offset); 
      }

      const decimal<Prec>& getLowValue (const ConstRandomAccessIterator& it, 
					unsigned long offset) const
      {
	return mSecurityTimeSeries->getLowValue(it, offset); 
      }

      const decimal<Prec>& getCloseValue (const ConstRandomAccessIterator& it, 
					  unsigned long offset) const
      {
	return mSecurityTimeSeries->getCloseValue(it, offset); 
      }

      const std::string& getName() const
      {
	return mSecurityName;
      }

      const std::string& getSymbol() const
      {
	return mSecuritySymbol;
      }

      const decimal<Prec>& getBigPointValue() const
      {
	return mBigPointValue;
      }

      const decimal<Prec>& getTick() const
      {
	return mTick;
      }

      const boost::gregorian::date& getFirstDate() const
      {
	return mFirstDate;
      }

      const boost::gregorian::date& getLastDate() const
      {
	return mSecurityTimeSeries->getLastDate();
      }

      std::shared_ptr<OHLCTimeSeries<Prec>> getTimeSeries() const
      {
	return mSecurityTimeSeries;
      }

      virtual std::shared_ptr<Security<Prec>> 
      clone(std::shared_ptr<OHLCTimeSeries<Prec>> securityTimeSeries) const = 0;

    private:
      std::string mSecuritySymbol;
      std::string mSecurityName;
      decimal<Prec> mBigPointValue;
      decimal<Prec> mTick;
      std::shared_ptr<OHLCTimeSeries<Prec>> mSecurityTimeSeries;
      boost::gregorian::date mFirstDate;
  };

  template <int Prec>
  class EquitySecurity : public Security<Prec>
  {
  public:
    EquitySecurity (const string& securitySymbol, const string& securityName,
		    std::shared_ptr<OHLCTimeSeries<Prec>> securityTimeSeries) 
      : Security<Prec> (securitySymbol, securityName, DecimalConstants<Prec>::DecimalOne,
			DecimalConstants<Prec>::EquityTick, securityTimeSeries)
    {
    }

    std::shared_ptr<Security<Prec>> clone(std::shared_ptr<OHLCTimeSeries<Prec>> securityTimeSeries) const
    {
      return std::make_shared<EquitySecurity<Prec>>(this->getSymbol(),
						    this->getName(),
						    securityTimeSeries);
    }

    EquitySecurity (const EquitySecurity<Prec> &rhs)
      : Security<Prec>(rhs)
    {}

    ~EquitySecurity()
    {}

    EquitySecurity<Prec>& 
    operator=(const EquitySecurity<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      Security<Prec>::operator=(rhs);
      return *this;
    }

    bool isEquitySecurity() const
    {
      return true;
    }

    bool isFuturesSecurity() const
    {
      return false;
    }
  };


  template <int Prec>
  class FuturesSecurity : public Security<Prec>
  {
  public:
    FuturesSecurity (const string& securitySymbol, const string& securityName,
		     const decimal<Prec>& bigPointValue, const decimal<Prec>& securityTick,
		     std::shared_ptr<OHLCTimeSeries<Prec>> securityTimeSeries)
      : Security<Prec> (securitySymbol, securityName, bigPointValue, securityTick,
		  securityTimeSeries)
    {}

    std::shared_ptr<Security<Prec>> clone(std::shared_ptr<OHLCTimeSeries<Prec>> securityTimeSeries) const
    {
      return std::make_shared<FuturesSecurity<Prec>>(this->getSymbol(),
						     this->getName(),
						     this->getBigPointValue(),
						     this->getTick(),
						     securityTimeSeries);
    }

    FuturesSecurity (const FuturesSecurity<Prec> &rhs)
      : Security<Prec>(rhs)
    {}

    ~FuturesSecurity()
    {}

    FuturesSecurity<Prec>& 
    operator=(const FuturesSecurity<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      Security<Prec>::operator=(rhs);
      return *this;
    }

    bool isEquitySecurity() const
    {
      return false;
    }

    bool isFuturesSecurity() const
    {
      return true;
    }
  };
}


#endif
