// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SECURITY_ATTRIBUTES_H
#define __SECURITY_ATTRIBUTES_H 1

#include <string>
#include <memory>
#include <boost/date_time.hpp>
#include "decimal.h"
#include "TradingVolume.h"
#include "DecimalConstants.h"

using dec::decimal;
using std::string;

namespace mkc_timeseries
{

  template <class Decimal>
  class LeverageAttributes
  {
  public:
    LeverageAttributes (const Decimal& leverage)
      : mLeverage (leverage),
	mInverseLeverage (leverage <  DecimalConstants<Decimal>::DecimalZero)
    {}

    const Decimal& getLeverage() const
    {
      return mLeverage;
    }

    bool isInverseLeverage() const
    {
      return mInverseLeverage;
    }

  private:
    Decimal mLeverage;
    bool mInverseLeverage;
  };

  template <class Decimal>
  inline bool operator==(const LeverageAttributes<Decimal>& lhs, const LeverageAttributes<Decimal>& rhs)
  {
    return lhs.getLeverage() == rhs.getLeverage();
  }

  template <class Decimal>
  inline bool operator!=(const LeverageAttributes<Decimal>& lhs, const LeverageAttributes<Decimal>& rhs){ return !(lhs == rhs); }

  template <class Decimal>
  class FundAttributes
  {
  public:
    FundAttributes (const boost::gregorian::date& inceptionDate, 
		    const Decimal& expenseRatio,
		    const LeverageAttributes<Decimal> leverageAttributes)
      : mInceptionDate (inceptionDate),
	mExpenseRatio(expenseRatio),
	mLeverageAttributes(leverageAttributes)
    {}

    ~FundAttributes()
    {}

    const boost::gregorian::date& getInceptionDate() const
    {
      return mInceptionDate;
    }

    const Decimal& getExpenseRatio() const
    {
      return mExpenseRatio;
    }

    const Decimal& getLeverage() const
    {
      return mLeverageAttributes.getLeverage();
    }

    bool isInverseFund() const
    {
      return mLeverageAttributes.isInverseLeverage();
    }

  private:
    boost::gregorian::date mInceptionDate;
    Decimal mExpenseRatio;
    LeverageAttributes<Decimal> mLeverageAttributes;
  };

  template <class Decimal>
  class SecurityAttributes
  {
  public:
    SecurityAttributes (const string& securitySymbol, const string& securityName,
			const Decimal& bigPointValue, const Decimal& securityTick)
      :mSecuritySymbol(securitySymbol),
       mSecurityName(securityName),
       mBigPointValue(bigPointValue),
       mTick(securityTick)
    {}

    SecurityAttributes (const SecurityAttributes<Decimal> &rhs)
      : mSecuritySymbol(rhs.mSecuritySymbol),
	mSecurityName(rhs.mSecurityName),
	mBigPointValue(rhs.mBigPointValue),
	mTick(rhs.mTick)
    {}

    SecurityAttributes<Decimal>& 
    operator=(const SecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mSecuritySymbol = rhs.mSecuritySymbol;
      mSecurityName = rhs.mSecurityName;
      mBigPointValue = rhs.mBigPointValue;
      mTick = rhs.mTick;
      return *this;
    }

    virtual ~SecurityAttributes()
    {}

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

    virtual bool isEquitySecurity() const = 0;
    virtual bool isFuturesSecurity() const = 0;
    virtual bool isCommonStock() const = 0;
    virtual bool isFund() const = 0;
    virtual TradingVolume::VolumeUnit getVolumeUnits() const = 0;
    
  private:
    std::string mSecuritySymbol;
    std::string mSecurityName;
    Decimal mBigPointValue;
    Decimal mTick;
  };

  template <class Decimal>
  class EquitySecurityAttributes : public SecurityAttributes<Decimal>
  {
  public:
    EquitySecurityAttributes (const string& securitySymbol, const string& securityName) 
      : SecurityAttributes<Decimal> (securitySymbol, securityName, DecimalConstants<Decimal>::DecimalOne,
				  DecimalConstants<Decimal>::EquityTick)
    {
    }

    EquitySecurityAttributes (const EquitySecurityAttributes<Decimal> &rhs)
      : SecurityAttributes<Decimal>(rhs)
    {}

    virtual ~EquitySecurityAttributes()
    {}

    EquitySecurityAttributes<Decimal>& 
    operator=(const EquitySecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      SecurityAttributes<Decimal>::operator=(rhs);
      return *this;
    }

    bool isEquitySecurity() const
    {
      return true;
    }

    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return TradingVolume::SHARES;
    }
    
    bool isFuturesSecurity() const
    {
      return false;
    }
  };

  template <class Decimal>
  class FundSecurityAttributes : public EquitySecurityAttributes<Decimal>
  {
  public:
    FundSecurityAttributes (const string& securitySymbol, 
			    const string& securityName,
			    const FundAttributes<Decimal>& attributes)
      : EquitySecurityAttributes<Decimal> (securitySymbol, securityName),
	mAttributes(attributes)
      {}

    FundSecurityAttributes (const FundSecurityAttributes<Decimal> &rhs)
      : EquitySecurityAttributes<Decimal>(rhs),
	mAttributes(rhs.mAttributes)
    {}

    virtual ~FundSecurityAttributes()
    {}

    FundSecurityAttributes<Decimal>& 
    operator=(const FundSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      EquitySecurityAttributes<Decimal>::operator=(rhs);
      mAttributes = rhs.mAttributes;
      return *this;
    }

    bool isCommonStock() const
    {
      return false;
    }

    bool isFund() const
    {
      return true;
    }

   const boost::gregorian::date& getInceptionDate() const
    {
      return mAttributes.getInceptionDate();
    }

    const Decimal& getExpenseRatio() const
    {
      return mAttributes.getExpenseRatio();
    }

    const Decimal& getLeverage() const
    {
      return mAttributes.getLeverage();
    }

    bool isInverseFund() const
    {
      return mAttributes.isInverseFund();
    }

    virtual bool isETF() const = 0;
    virtual bool isMutualFund() const = 0;

  private:
    FundAttributes<Decimal> mAttributes;
  };

  template <class Decimal>
  class ETFSecurityAttributes : public FundSecurityAttributes<Decimal>
  {
  public:
    ETFSecurityAttributes (const string& securitySymbol, 
			    const string& securityName,
			   const FundAttributes<Decimal>& attributes)
      : FundSecurityAttributes<Decimal> (securitySymbol, securityName, attributes)
    {}

    ETFSecurityAttributes (const ETFSecurityAttributes<Decimal> &rhs)
      : FundSecurityAttributes<Decimal>(rhs)
    {}

    ~ETFSecurityAttributes()
    {}

    ETFSecurityAttributes<Decimal>& 
    operator=(const ETFSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      FundSecurityAttributes<Decimal>::operator=(rhs);
      return *this;
    }

    bool isETF() const
    {
      return true;
    }

    bool isMutualFund() const
    {
      return false;
    }
  };

  template <class Decimal>
  class CommonStockSecurityAttributes : public EquitySecurityAttributes<Decimal>
  {
  public:
    CommonStockSecurityAttributes (const string& securitySymbol, 
				   const string& securityName)
      : EquitySecurityAttributes<Decimal> (securitySymbol, securityName)
      {}

    ~CommonStockSecurityAttributes()
    {}

    CommonStockSecurityAttributes (const CommonStockSecurityAttributes<Decimal> &rhs)
      : EquitySecurityAttributes<Decimal>(rhs)
    {}

    CommonStockSecurityAttributes<Decimal>& 
    operator=(const CommonStockSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      EquitySecurityAttributes<Decimal>::operator=(rhs);
      return *this;
    }

    bool isCommonStock() const
    {
      return true;
    }

    bool isFund() const
    {
      return false;
    }
  };

  template <class Decimal>
  class FuturesSecurityAttributes : public SecurityAttributes<Decimal>
  {
  public:
    FuturesSecurityAttributes (const string& securitySymbol, const string& securityName,
			       const Decimal& bigPointValue, const Decimal& securityTick)
      : SecurityAttributes<Decimal> (securitySymbol, securityName, bigPointValue, securityTick)
    {}

    FuturesSecurityAttributes (const FuturesSecurityAttributes<Decimal> &rhs)
      : SecurityAttributes<Decimal>(rhs)
    {}

    ~FuturesSecurityAttributes()
    {}

    FuturesSecurityAttributes<Decimal>& 
    operator=(const FuturesSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      SecurityAttributes<Decimal>::operator=(rhs);
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

    bool isCommonStock() const
    {
      return false;
    }

    bool isFund() const
    {
      return false;
    }

    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return TradingVolume::CONTRACTS;
    }
  };
}

#endif
