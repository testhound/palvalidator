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
#include "number.h"
#include "TradingVolume.h"
#include "DecimalConstants.h"

using std::string;

namespace mkc_timeseries
{
  /**
   * @brief Represents leverage attributes for a financial instrument.
   * @tparam Decimal The numeric type used for calculations (e.g., double, a custom decimal class).
   *
   * This class stores the leverage factor and indicates if the leverage is inverse.
   */
  template <class Decimal>
  class LeverageAttributes
  {
  public:
    /**
     * @brief Constructs a LeverageAttributes object.
     * @param leverage The leverage factor. A negative value implies inverse leverage.
     */
    LeverageAttributes (const Decimal& leverage)
      : mLeverage (leverage),
	mInverseLeverage (leverage <  DecimalConstants<Decimal>::DecimalZero)
    {}

    /**
     * @brief Gets the leverage factor.
     * @return Const reference to the leverage factor.
     */
    const Decimal& getLeverage() const
    {
      return mLeverage;
    }

    /**
     * @brief Checks if the instrument has inverse leverage.
     * @return True if leverage is inverse (leverage value is negative), false otherwise.
     */
    bool isInverseLeverage() const
    {
      return mInverseLeverage;
    }

  private:
    Decimal mLeverage;
    bool mInverseLeverage;
  };

  /**
   * @brief Equality operator for LeverageAttributes.
   * @tparam Decimal The numeric type.
   * @param lhs Left-hand side LeverageAttributes object.
   * @param rhs Right-hand side LeverageAttributes object.
   * @return True if both objects have the same leverage value, false otherwise.
   */
  template <class Decimal>
  inline bool operator==(const LeverageAttributes<Decimal>& lhs, const LeverageAttributes<Decimal>& rhs)
  {
    return lhs.getLeverage() == rhs.getLeverage();
  }

   /**
   * @brief Inequality operator for LeverageAttributes.
   * @tparam Decimal The numeric type.
   * @param lhs Left-hand side LeverageAttributes object.
   * @param rhs Right-hand side LeverageAttributes object.
   * @return True if the objects have different leverage values, false otherwise.
   */
  template <class Decimal>
  inline bool operator!=(const LeverageAttributes<Decimal>& lhs, const LeverageAttributes<Decimal>& rhs){ return !(lhs == rhs); }

  /**
   * @brief Represents attributes specific to a fund.
   * @tparam Decimal The numeric type used for calculations.
   *
   * This class stores information like expense ratio and leverage attributes of a fund.
   * It interacts with LeverageAttributes to manage leverage details.
   */
  template <class Decimal>
  class FundAttributes
  {
  public:
    /**
     * @brief Constructs a FundAttributes object.
     * @param expenseRatio The expense ratio of the fund.
     * @param leverageAttributes A LeverageAttributes object describing the fund's leverage.
     */
    FundAttributes (const Decimal& expenseRatio,
		    const LeverageAttributes<Decimal> leverageAttributes)
      : mExpenseRatio(expenseRatio),
	mLeverageAttributes(leverageAttributes)
    {}

    /**
     * @brief Destructor for FundAttributes.
     */
    ~FundAttributes()
    {}

    /**
     * @brief Gets the expense ratio of the fund.
     * @return Const reference to the expense ratio.
     */
    const Decimal& getExpenseRatio() const
    {
      return mExpenseRatio;
    }

    /**
     * @brief Gets the leverage factor of the fund.
     * @return Const reference to the leverage factor, obtained from its LeverageAttributes member.
     */
    const Decimal& getLeverage() const
    {
      return mLeverageAttributes.getLeverage();
    }

    /**
     * @brief Checks if the fund is an inverse fund.
     * @return True if the fund has inverse leverage, false otherwise.
     */
    bool isInverseFund() const
    {
      return mLeverageAttributes.isInverseLeverage();
    }

  private:
    Decimal mExpenseRatio;
    LeverageAttributes<Decimal> mLeverageAttributes;
  };

   /**
   * @brief Abstract base class for security attributes.
   * @tparam Decimal The numeric type used for calculations.
   *
   * This class provides a common interface for various security attributes like symbol,
   * name, big point value, tick size, and inception date. Derived classes must
   * implement specific details related to the type of security.
   * Interacts with TradingVolume for volume units and DecimalConstants for default values.
   */
  template <class Decimal>
  class SecurityAttributes
  {
  public:
    /**
     * @brief Constructs a SecurityAttributes object.
     * @param securitySymbol The trading symbol of the security.
     * @param securityName The full name of the security.
     * @param bigPointValue The value of one full point move for the security.
     * @param securityTick The minimum price fluctuation (tick size).
     * @param inceptionDate The date the security was first listed or introduced.
     */
    SecurityAttributes (const string& securitySymbol, const string& securityName,
			const Decimal& bigPointValue, const Decimal& securityTick,
			const boost::gregorian::date& inceptionDate)
      :mSecuritySymbol(securitySymbol),
       mSecurityName(securityName),
       mBigPointValue(bigPointValue),
       mTick(securityTick),
       mInceptionDate (inceptionDate)
    {}

    /**
     * @brief Copy constructor for SecurityAttributes.
     * @param rhs The SecurityAttributes object to copy.
     */
    SecurityAttributes (const SecurityAttributes<Decimal> &rhs)
      : mSecuritySymbol(rhs.mSecuritySymbol),
	mSecurityName(rhs.mSecurityName),
	mBigPointValue(rhs.mBigPointValue),
	mTick(rhs.mTick),
	mInceptionDate(rhs.mInceptionDate)
    {}

    /**
     * @brief Assignment operator for SecurityAttributes.
     * @param rhs The SecurityAttributes object to assign from.
     * @return Reference to this SecurityAttributes object.
     */
    SecurityAttributes<Decimal>& 
    operator=(const SecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mSecuritySymbol = rhs.mSecuritySymbol;
      mSecurityName = rhs.mSecurityName;
      mBigPointValue = rhs.mBigPointValue;
      mTick = rhs.mTick;
      mInceptionDate = rhs.mInceptionDate;
      return *this;
    }

    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes.
     */
    virtual ~SecurityAttributes()
    {}

    /**
     * @brief Gets the name of the security.
     * @return Const reference to the security's name.
     */
    const std::string& getName() const
    {
      return mSecurityName;
    }

    /**
     * @brief Gets the symbol of the security.
     * @return Const reference to the security's symbol.
     */
    const std::string& getSymbol() const
    {
      return mSecuritySymbol;
    }

     /**
     * @brief Gets the big point value of the security.
     * @return Const reference to the big point value.
     */
    const Decimal& getBigPointValue() const
    {
      return mBigPointValue;
    }

    /**
     * @brief Gets the tick size of the security.
     * @return Const reference to the tick size.
     */
    const Decimal& getTick() const
    {
      return mTick;
    }

    /**
     * @brief Gets the inception date of the security.
     * @return Const reference to the inception date.
     */
    const boost::gregorian::date& getInceptionDate() const
    {
      return mInceptionDate;
    }

    /**
     * @brief Pure virtual method to check if the security is an equity.
     * @return Must be implemented by derived classes. True for equity, false otherwise.
     */
    virtual bool isEquitySecurity() const = 0;

     /**
     * @brief Pure virtual method to check if the security is a futures contract.
     * @return Must be implemented by derived classes. True for futures, false otherwise.
     */
    virtual bool isFuturesSecurity() const = 0;

    /**
     * @brief Pure virtual method to check if the security is common stock.
     * @return Must be implemented by derived classes. True for common stock, false otherwise.
     */
    virtual bool isCommonStock() const = 0;

    /**
     * @brief Pure virtual method to check if the security is a fund.
     * @return Must be implemented by derived classes. True for a fund, false otherwise.
     */
    virtual bool isFund() const = 0;

    /**
     * @brief Pure virtual method to get the volume units for the security.
     * @return Must be implemented by derived classes. Returns TradingVolume::VolumeUnit.
     */
    virtual TradingVolume::VolumeUnit getVolumeUnits() const = 0;
    
  private:
    std::string mSecuritySymbol;
    std::string mSecurityName;
    Decimal mBigPointValue;
    Decimal mTick;
    boost::gregorian::date mInceptionDate;
  };

  /**
   * @brief Represents attributes for equity securities.
   * @tparam Decimal The numeric type used for calculations.
   *
   * Inherits from SecurityAttributes and provides specific implementations for equities.
   * Default big point value is 1 and tick is defined by DecimalConstants::EquityTick.
   * Interacts with DecimalConstants and TradingVolume.
   */
  template <class Decimal>
  class EquitySecurityAttributes : public SecurityAttributes<Decimal>
  {
  public:
    /**
     * @brief Constructs an EquitySecurityAttributes object.
     * @param securitySymbol The trading symbol.
     * @param securityName The full name.
     * @param inceptionDate The inception date.
     */
    EquitySecurityAttributes (const string& securitySymbol, const string& securityName,
			      const boost::gregorian::date& inceptionDate)
      : SecurityAttributes<Decimal> (securitySymbol, securityName, DecimalConstants<Decimal>::DecimalOne,
				  DecimalConstants<Decimal>::EquityTick, inceptionDate)
    {
    }

    /**
     * @brief Copy constructor for EquitySecurityAttributes.
     * @param rhs The EquitySecurityAttributes object to copy.
     */
    EquitySecurityAttributes (const EquitySecurityAttributes<Decimal> &rhs)
      : SecurityAttributes<Decimal>(rhs)
    {}

    /**
     * @brief Virtual destructor.
     */
    virtual ~EquitySecurityAttributes()
    {}

    /**
     * @brief Assignment operator for EquitySecurityAttributes.
     * @param rhs The EquitySecurityAttributes object to assign from.
     * @return Reference to this EquitySecurityAttributes object.
     */
    EquitySecurityAttributes<Decimal>& 
    operator=(const EquitySecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      SecurityAttributes<Decimal>::operator=(rhs);
      return *this;
    }

    /**
     * @brief Checks if this is an equity security.
     * @return Always true for this class.
     */
    bool isEquitySecurity() const
    {
      return true;
    }

    /**
     * @brief Gets the volume units for equity securities.
     * @return TradingVolume::SHARES.
     */
    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return TradingVolume::SHARES;
    }

    /**
     * @brief Checks if this is a futures security.
     * @return Always false for this class.
     */
    bool isFuturesSecurity() const
    {
      return false;
    }
  };

  /**
   * @brief Abstract base class for fund security attributes (e.g., ETFs, Mutual Funds).
   * @tparam Decimal The numeric type used for calculations.
   *
   * Inherits from EquitySecurityAttributes and includes specific fund characteristics
   * via a FundAttributes member. Derived classes must specify if they are ETFs or Mutual Funds.
   * Contains a FundAttributes object.
   */
  template <class Decimal>
  class FundSecurityAttributes : public EquitySecurityAttributes<Decimal>
  {
  public:
     /**
     * @brief Constructs a FundSecurityAttributes object.
     * @param securitySymbol The trading symbol.
     * @param securityName The full name.
     * @param attributes A FundAttributes object containing details like expense ratio and leverage.
     * @param inceptionDate The inception date.
     */
    FundSecurityAttributes (const string& securitySymbol, 
			    const string& securityName,
			    const FundAttributes<Decimal>& attributes,
			    const boost::gregorian::date& inceptionDate)
      : EquitySecurityAttributes<Decimal> (securitySymbol, securityName, inceptionDate),
	mAttributes(attributes)
      {}

    /**
     * @brief Copy constructor for FundSecurityAttributes.
     * @param rhs The FundSecurityAttributes object to copy.
     */
    FundSecurityAttributes (const FundSecurityAttributes<Decimal> &rhs)
      : EquitySecurityAttributes<Decimal>(rhs),
	mAttributes(rhs.mAttributes)
    {}

    /**
     * @brief Virtual destructor.
     */
    virtual ~FundSecurityAttributes()
    {}

    /**
     * @brief Assignment operator for FundSecurityAttributes.
     * @param rhs The FundSecurityAttributes object to assign from.
     * @return Reference to this FundSecurityAttributes object.
     */
    FundSecurityAttributes<Decimal>& 
    operator=(const FundSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      EquitySecurityAttributes<Decimal>::operator=(rhs);
      mAttributes = rhs.mAttributes;
      return *this;
    }

    /**
     * @brief Checks if this is common stock.
     * @return Always false for funds.
     */
    bool isCommonStock() const
    {
      return false;
    }

    /**
     * @brief Checks if this is a fund.
     * @return Always true for this class.
     */
    bool isFund() const
    {
      return true;
    }

    /**
     * @brief Gets the expense ratio of the fund.
     * @return Const reference to the expense ratio from its FundAttributes member.
     */
    const Decimal& getExpenseRatio() const
    {
      return mAttributes.getExpenseRatio();
    }

    /**
     * @brief Gets the leverage factor of the fund.
     * @return Const reference to the leverage factor from its FundAttributes member.
     */
    const Decimal& getLeverage() const
    {
      return mAttributes.getLeverage();
    }

    /**
     * @brief Checks if the fund is an inverse fund.
     * @return True if the fund has inverse leverage, false otherwise, based on its FundAttributes.
     */
    bool isInverseFund() const
    {
      return mAttributes.isInverseFund();
    }

    /**
     * @brief Pure virtual method to check if the fund is an Exchange-Traded Fund (ETF).
     * @return Must be implemented by derived classes. True for ETF, false otherwise.
     */
    virtual bool isETF() const = 0;

    /**
     * @brief Pure virtual method to check if the fund is a Mutual Fund.
     * @return Must be implemented by derived classes. True for Mutual Fund, false otherwise.
     */
    virtual bool isMutualFund() const = 0;

  private:
    FundAttributes<Decimal> mAttributes;
  };

  /**
   * @brief Represents attributes for Exchange-Traded Funds or Exchange
   * trade notes (ETNs).
   * @tparam Decimal The numeric type used for calculations.
   *
   * Inherits from FundSecurityAttributes and provides concrete implementations for ETF checks.
   */
  template <class Decimal>
  class ETFSecurityAttributes : public FundSecurityAttributes<Decimal>
  {
  public:
    /**
     * @brief Constructs an ETFSecurityAttributes object.
     * @param securitySymbol The trading symbol.
     * @param securityName The full name.
     * @param attributes A FundAttributes object for the ETF.
     * @param inceptionDate The inception date.
     */
    ETFSecurityAttributes (const string& securitySymbol, 
			   const string& securityName,
			   const FundAttributes<Decimal>& attributes,
			   const boost::gregorian::date& inceptionDate)
      : FundSecurityAttributes<Decimal> (securitySymbol, securityName, attributes, inceptionDate)
    {}

    /**
     * @brief Copy constructor for ETFSecurityAttributes.
     * @param rhs The ETFSecurityAttributes object to copy.
     */
    ETFSecurityAttributes (const ETFSecurityAttributes<Decimal> &rhs)
      : FundSecurityAttributes<Decimal>(rhs)
    {}

    /**
     * @brief Destructor.
     */
    ~ETFSecurityAttributes()
    {}

    /**
     * @brief Assignment operator for ETFSecurityAttributes.
     * @param rhs The ETFSecurityAttributes object to assign from.
     * @return Reference to this ETFSecurityAttributes object.
     */
    ETFSecurityAttributes<Decimal>& 
    operator=(const ETFSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      FundSecurityAttributes<Decimal>::operator=(rhs);
      return *this;
    }

    /**
     * @brief Checks if this fund is an ETF.
     * @return Always true for this class.
     */
    bool isETF() const
    {
      return true;
    }

    /**
     * @brief Checks if this fund is a Mutual Fund.
     * @return Always false for ETFs.
     */
    bool isMutualFund() const
    {
      return false;
    }
  };

  /**
   * @brief Represents attributes for common stock securities.
   * @tparam Decimal The numeric type used for calculations.
   *
   * Inherits from EquitySecurityAttributes.
   */
  template <class Decimal>
  class CommonStockSecurityAttributes : public EquitySecurityAttributes<Decimal>
  {
  public:
    /**
     * @brief Constructs a CommonStockSecurityAttributes object.
     * @param securitySymbol The trading symbol.
     * @param securityName The full name.
     * @param inceptionDate The inception date.
     */
    CommonStockSecurityAttributes (const string& securitySymbol, 
				   const string& securityName,
				   const boost::gregorian::date& inceptionDate)
      : EquitySecurityAttributes<Decimal> (securitySymbol, securityName, inceptionDate)
      {}

    /**
     * @brief Destructor.
     */
    ~CommonStockSecurityAttributes()
    {}

    /**
     * @brief Copy constructor for CommonStockSecurityAttributes.
     * @param rhs The CommonStockSecurityAttributes object to copy.
     */
    CommonStockSecurityAttributes (const CommonStockSecurityAttributes<Decimal> &rhs)
      : EquitySecurityAttributes<Decimal>(rhs)
    {}

    /**
     * @brief Assignment operator for CommonStockSecurityAttributes.
     * @param rhs The CommonStockSecurityAttributes object to assign from.
     * @return Reference to this CommonStockSecurityAttributes object.
     */
    CommonStockSecurityAttributes<Decimal>& 
    operator=(const CommonStockSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      EquitySecurityAttributes<Decimal>::operator=(rhs);
      return *this;
    }

    /**
     * @brief Checks if this security is common stock.
     * @return Always true for this class.
     */
    bool isCommonStock() const
    {
      return true;
    }

    /**
     * @brief Checks if this security is a fund.
     * @return Always false for common stock.
     */
    bool isFund() const
    {
      return false;
    }
  };

  /**
   * @brief Represents attributes for futures contract securities.
   * @tparam Decimal The numeric type used for calculations.
   *
   * Inherits from SecurityAttributes and provides specific implementations for futures.
   * Interacts with TradingVolume for volume units (CONTRACTS).
   */
  template <class Decimal>
  class FuturesSecurityAttributes : public SecurityAttributes<Decimal>
  {
  public:
    /**
     * @brief Constructs a FuturesSecurityAttributes object.
     * @param securitySymbol The trading symbol.
     * @param securityName The full name.
     * @param bigPointValue The value of one full point move.
     * @param securityTick The minimum price fluctuation.
     * @param inceptionDate The inception date.
     */
    FuturesSecurityAttributes (const string& securitySymbol, const string& securityName,
			       const Decimal& bigPointValue, const Decimal& securityTick,
			       const boost::gregorian::date& inceptionDate)
      : SecurityAttributes<Decimal> (securitySymbol, securityName, bigPointValue, securityTick,
				     inceptionDate)
    {}

    /**
     * @brief Copy constructor for FuturesSecurityAttributes.
     * @param rhs The FuturesSecurityAttributes object to copy.
     */
    FuturesSecurityAttributes (const FuturesSecurityAttributes<Decimal> &rhs)
      : SecurityAttributes<Decimal>(rhs)
    {}

    /**
     * @brief Destructor.
     */
    ~FuturesSecurityAttributes()
    {}

     /**
     * @brief Assignment operator for FuturesSecurityAttributes.
     * @param rhs The FuturesSecurityAttributes object to assign from.
     * @return Reference to this FuturesSecurityAttributes object.
     */
    FuturesSecurityAttributes<Decimal>& 
    operator=(const FuturesSecurityAttributes<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      SecurityAttributes<Decimal>::operator=(rhs);
      return *this;
    }

    /**
     * @brief Checks if this is an equity security.
     * @return Always false for futures.
     */
    bool isEquitySecurity() const
    {
      return false;
    }

    /**
     * @brief Checks if this is a futures security.
     * @return Always true for this class.
     */
    bool isFuturesSecurity() const
    {
      return true;
    }

    /**
     * @brief Checks if this is common stock.
     * @return Always false for futures.
     */
    bool isCommonStock() const
    {
      return false;
    }

    /**
     * @brief Checks if this is a fund.
     * @return Always false for futures.
     */
    bool isFund() const
    {
      return false;
    }

    /**
     * @brief Gets the volume units for futures securities.
     * @return TradingVolume::CONTRACTS.
     */
    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return TradingVolume::CONTRACTS;
    }
  };
}

#endif
