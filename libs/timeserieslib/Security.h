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

  template <class Decimal>
  class Security
    {
    public:
      typedef typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator ConstRandomAccessIterator;

      Security (const string& securitySymbol, const string& securityName,
		const Decimal& bigPointValue, const Decimal& securityTick,
		std::shared_ptr<OHLCTimeSeries<Decimal>> securityTimeSeries) :
	mSecuritySymbol(securitySymbol),
	mSecurityName(securityName),
	mBigPointValue(bigPointValue),
	mTick(securityTick),
	mSecurityTimeSeries(securityTimeSeries),
	mFirstDate(securityTimeSeries->getFirstDate()),
	mTickDiv2(securityTick/DecimalConstants<Decimal>::DecimalTwo)
      {}
      
      Security (const Security<Decimal> &rhs)
	: mSecuritySymbol(rhs.mSecuritySymbol),
	  mSecurityName(rhs.mSecurityName),
	  mBigPointValue(rhs.mBigPointValue),
	  mTick(rhs.mTick),
	  mSecurityTimeSeries(rhs.mSecurityTimeSeries),
	  mFirstDate(rhs.mFirstDate),
	  mTickDiv2(rhs.mTickDiv2)
      {}

      Security<Decimal>& 
      operator=(const Security<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;

	mSecuritySymbol = rhs.mSecuritySymbol;
	mSecurityName = rhs.mSecurityName;
	mBigPointValue = rhs.mBigPointValue;
	mTick = rhs.mTick;
	mSecurityTimeSeries = rhs.mSecurityTimeSeries;
	mFirstDate = rhs.mFirstDate;
	mTickDiv2 = rhs.mTickDiv2;
	
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

     const Decimal& getOpenValue (const ConstRandomAccessIterator& it, 
					unsigned long offset) const
      {
	return mSecurityTimeSeries->getOpenValue(it, offset); 
      }


      const Decimal& getHighValue (const ConstRandomAccessIterator& it, 
					 unsigned long offset) const
      {
	return mSecurityTimeSeries->getHighValue(it, offset); 
      }

      const Decimal& getLowValue (const ConstRandomAccessIterator& it, 
					unsigned long offset) const
      {
	return mSecurityTimeSeries->getLowValue(it, offset); 
      }

      const Decimal& getCloseValue (const ConstRandomAccessIterator& it, 
				    unsigned long offset) const
      {
	return mSecurityTimeSeries->getCloseValue(it, offset); 
      }

      const Decimal& getVolumeValue (const ConstRandomAccessIterator& it, 
					 unsigned long offset) const
      {
	return mSecurityTimeSeries->getVolumeValue(it, offset); 
      }

	    
      const std::string& getName() const
      {
	return mSecurityName;
      }

      const std::string& getSymbol() const
      {
	return mSecuritySymbol;
      }

      const Decimal& getBigPointValue() const
      {
	return mBigPointValue;
      }

      const Decimal& getTick() const
      {
	return mTick;
      }

      const Decimal& getTickDiv2() const
      {
	return mTickDiv2;
      }

      const boost::gregorian::date& getFirstDate() const
      {
	return mFirstDate;
      }

      const boost::gregorian::date& getLastDate() const
      {
	return mSecurityTimeSeries->getLastDate();
      }

      std::shared_ptr<OHLCTimeSeries<Decimal>> getTimeSeries() const
      {
	return mSecurityTimeSeries;
      }

      virtual std::shared_ptr<Security<Decimal>> 
      clone(std::shared_ptr<OHLCTimeSeries<Decimal>> securityTimeSeries) const = 0;

    private:
      std::string mSecuritySymbol;
      std::string mSecurityName;
      Decimal mBigPointValue;
      Decimal mTick;
      std::shared_ptr<OHLCTimeSeries<Decimal>> mSecurityTimeSeries;
      boost::gregorian::date mFirstDate;
      Decimal mTickDiv2;                    // Used to speedup compuation of Round@Tick
    };

  template <class Decimal>
  class EquitySecurity : public Security<Decimal>
  {
  public:
    EquitySecurity (const string& securitySymbol, const string& securityName,
		    std::shared_ptr<OHLCTimeSeries<Decimal>> securityTimeSeries) 
      : Security<Decimal> (securitySymbol, securityName, DecimalConstants<Decimal>::DecimalOne,
			DecimalConstants<Decimal>::EquityTick, securityTimeSeries)
    {
    }

    std::shared_ptr<Security<Decimal>> clone(std::shared_ptr<OHLCTimeSeries<Decimal>> securityTimeSeries) const
    {
      return std::make_shared<EquitySecurity<Decimal>>(this->getSymbol(),
						    this->getName(),
						    securityTimeSeries);
    }

    EquitySecurity (const EquitySecurity<Decimal> &rhs)
      : Security<Decimal>(rhs)
    {}

    ~EquitySecurity()
    {}

    EquitySecurity<Decimal>& 
    operator=(const EquitySecurity<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      Security<Decimal>::operator=(rhs);
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


  template <class Decimal>
  class FuturesSecurity : public Security<Decimal>
  {
  public:
    FuturesSecurity (const string& securitySymbol, const string& securityName,
		     const Decimal& bigPointValue, const Decimal& securityTick,
		     std::shared_ptr<OHLCTimeSeries<Decimal>> securityTimeSeries)
      : Security<Decimal> (securitySymbol, securityName, bigPointValue, securityTick,
		  securityTimeSeries)
    {}

    std::shared_ptr<Security<Decimal>> clone(std::shared_ptr<OHLCTimeSeries<Decimal>> securityTimeSeries) const
    {
      return std::make_shared<FuturesSecurity<Decimal>>(this->getSymbol(),
						     this->getName(),
						     this->getBigPointValue(),
						     this->getTick(),
						     securityTimeSeries);
    }

    FuturesSecurity (const FuturesSecurity<Decimal> &rhs)
      : Security<Decimal>(rhs)
    {}

    ~FuturesSecurity()
    {}

    FuturesSecurity<Decimal>& 
    operator=(const FuturesSecurity<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      Security<Decimal>::operator=(rhs);
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
