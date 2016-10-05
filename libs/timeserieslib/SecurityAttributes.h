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

  template <int Prec>
  class LeverageAttributes
  {
  public:
    LeverageAttributes (const decimal<Prec>& leverage)
      : mLeverage (leverage),
	mInverseLeverage (leverage <  DecimalConstants<Prec>::DecimalZero)
    {}

    const decimal<Prec>& getLeverage() const
    {
      return mLeverage;
    }

    bool isInverseLeverage() const
    {
      return mInverseLeverage;
    }

  private:
    decimal<Prec> mLeverage;
    bool mInverseLeverage;
  };

  template <int Prec>
  inline bool operator==(const LeverageAttributes<Prec>& lhs, const LeverageAttributes<Prec>& rhs)
  {
    return lhs.getLeverage() == rhs.getLeverage();
  }

  template <int Prec>
  inline bool operator!=(const LeverageAttributes<Prec>& lhs, const LeverageAttributes<Prec>& rhs){ return !(lhs == rhs); }

  template <int Prec>
  class FundAttributes
  {
  public:
    FundAttributes (const boost::gregorian::date& inceptionDate, 
		    const decimal<Prec>& expenseRatio,
		    const LeverageAttributes<Prec> leverageAttributes)
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

    const decimal<Prec>& getExpenseRatio() const
    {
      return mExpenseRatio;
    }

    const decimal<Prec>& getLeverage() const
    {
      return mLeverageAttributes.getLeverage();
    }

    bool isInverseFund() const
    {
      return mLeverageAttributes.isInverseLeverage();
    }

  private:
    boost::gregorian::date mInceptionDate;
    decimal<Prec> mExpenseRatio;
    LeverageAttributes<Prec> mLeverageAttributes;
  };

  template <int Prec>
  class SecurityAttributes
  {
  public:
    SecurityAttributes (const string& securitySymbol, const string& securityName,
			const decimal<Prec>& bigPointValue, const decimal<Prec>& securityTick)
      :mSecuritySymbol(securitySymbol),
       mSecurityName(securityName),
       mBigPointValue(bigPointValue),
       mTick(securityTick)
    {}

    SecurityAttributes (const SecurityAttributes<Prec> &rhs)
      : mSecuritySymbol(rhs.mSecuritySymbol),
	mSecurityName(rhs.mSecurityName),
	mBigPointValue(rhs.mBigPointValue),
	mTick(rhs.mTick)
    {}

    SecurityAttributes<Prec>& 
    operator=(const SecurityAttributes<Prec> &rhs)
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

    const decimal<Prec>& getBigPointValue() const
    {
      return mBigPointValue;
    }

    const decimal<Prec>& getTick() const
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
    decimal<Prec> mBigPointValue;
    decimal<Prec> mTick;
  };

  template <int Prec>
  class EquitySecurityAttributes : public SecurityAttributes<Prec>
  {
  public:
    EquitySecurityAttributes (const string& securitySymbol, const string& securityName) 
      : SecurityAttributes<Prec> (securitySymbol, securityName, DecimalConstants<Prec>::DecimalOne,
				  DecimalConstants<Prec>::EquityTick)
    {
    }

    EquitySecurityAttributes (const EquitySecurityAttributes<Prec> &rhs)
      : SecurityAttributes<Prec>(rhs)
    {}

    virtual ~EquitySecurityAttributes()
    {}

    EquitySecurityAttributes<Prec>& 
    operator=(const EquitySecurityAttributes<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      SecurityAttributes<Prec>::operator=(rhs);
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

  template <int Prec>
  class FundSecurityAttributes : public EquitySecurityAttributes<Prec>
  {
  public:
    FundSecurityAttributes (const string& securitySymbol, 
			    const string& securityName,
			    const FundAttributes<Prec>& attributes)
      : EquitySecurityAttributes<Prec> (securitySymbol, securityName),
	mAttributes(attributes)
      {}

    FundSecurityAttributes (const FundSecurityAttributes<Prec> &rhs)
      : EquitySecurityAttributes<Prec>(rhs),
	mAttributes(rhs.mAttributes)
    {}

    virtual ~FundSecurityAttributes()
    {}

    FundSecurityAttributes<Prec>& 
    operator=(const FundSecurityAttributes<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      EquitySecurityAttributes<Prec>::operator=(rhs);
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

    const decimal<Prec>& getExpenseRatio() const
    {
      return mAttributes.getExpenseRatio();
    }

    const decimal<Prec>& getLeverage() const
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
    FundAttributes<Prec> mAttributes;
  };

  template <int Prec>
  class ETFSecurityAttributes : public FundSecurityAttributes<Prec>
  {
  public:
    ETFSecurityAttributes (const string& securitySymbol, 
			    const string& securityName,
			   const FundAttributes<Prec>& attributes)
      : FundSecurityAttributes<Prec> (securitySymbol, securityName, attributes)
    {}

    ETFSecurityAttributes (const ETFSecurityAttributes<Prec> &rhs)
      : FundSecurityAttributes<Prec>(rhs)
    {}

    ~ETFSecurityAttributes()
    {}

    ETFSecurityAttributes<Prec>& 
    operator=(const ETFSecurityAttributes<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      FundSecurityAttributes<Prec>::operator=(rhs);
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

  template <int Prec>
  class CommonStockSecurityAttributes : public EquitySecurityAttributes<Prec>
  {
  public:
    CommonStockSecurityAttributes (const string& securitySymbol, 
				   const string& securityName)
      : EquitySecurityAttributes<Prec> (securitySymbol, securityName)
      {}

    ~CommonStockSecurityAttributes()
    {}

    CommonStockSecurityAttributes (const CommonStockSecurityAttributes<Prec> &rhs)
      : EquitySecurityAttributes<Prec>(rhs)
    {}

    CommonStockSecurityAttributes<Prec>& 
    operator=(const CommonStockSecurityAttributes<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      EquitySecurityAttributes<Prec>::operator=(rhs);
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

  template <int Prec>
  class FuturesSecurityAttributes : public SecurityAttributes<Prec>
  {
  public:
    FuturesSecurityAttributes (const string& securitySymbol, const string& securityName,
			       const decimal<Prec>& bigPointValue, const decimal<Prec>& securityTick)
      : SecurityAttributes<Prec> (securitySymbol, securityName, bigPointValue, securityTick)
    {}

    FuturesSecurityAttributes (const FuturesSecurityAttributes<Prec> &rhs)
      : SecurityAttributes<Prec>(rhs)
    {}

    ~FuturesSecurityAttributes()
    {}

    FuturesSecurityAttributes<Prec>& 
    operator=(const FuturesSecurityAttributes<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      SecurityAttributes<Prec>::operator=(rhs);
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
