// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SECURITY_H
#define __SECURITY_H 1

#include <string>
#include <memory>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TimeSeries.h"
#include "DecimalConstants.h"

using std::string;

namespace mkc_timeseries
{
  using boost::posix_time::ptime;

  class SecurityException : public std::runtime_error
  {
  public:
  SecurityException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~SecurityException()
      {}

  };

   /**
   * @class Security
   * @brief Abstract base class representing a financial instrument (e.g., stock, future) for backtesting.
   * @tparam Decimal The numeric type used for financial calculations (prices, P/L).
   *
   * @details This class serves as the core representation of a tradable asset within the backtesting framework.
   * It encapsulates essential metadata about the security, such as its ticker symbol, name, tick size,
   * and value per point (Big Point Value). Crucially, it holds a reference (via `shared_ptr`) to the
   * historical price data (`OHLCTimeSeries`) for that instrument, providing the basis for strategy evaluation.
   *
   * Being abstract, it defines a common interface but requires concrete derived classes
   * (like `EquitySecurity` or `FuturesSecurity`) to specify the asset type and potentially
   * provide default metadata values. Access to historical price data is primarily achieved by
   * delegating calls to the contained `OHLCTimeSeries` object.
   *
   * Key Responsibilities:
   * - Storing security identification (symbol, name).
   * - Storing critical financial metadata (big point value, tick size).
   * - Holding and providing access to the associated historical price data (`OHLCTimeSeries`).
   * - Defining an interface for asset type identification (`isEquitySecurity`, `isFuturesSecurity`).
   * - Defining an interface for cloning (`clone`).
   *
   * Collaborations:
   * - OHLCTimeSeries: Holds a `std::shared_ptr<const OHLCTimeSeries>` (`mSecurityTimeSeries`).
   *   This is the primary collaboration for accessing price history. Many methods delegate directly to this object.
   *
   * - OHLCTimeSeriesEntry: Individual data points returned when accessing the time series.
   *
   * - boost::gregorian::date: Used for indexing and retrieving data from the time series.
   *
   * - DecimalConstants: Used for numeric constants like `DecimalTwo` and potentially default tick sizes in derived classes.
   *
   * - Portfolio (Usage Context): Typically holds a collection of `Security` objects.
   *
   * - StrategyBroker (Usage Context): May query `Security` objects (often via `Portfolio` or `SecurityAttributesFactory`)
   * for metadata (like tick size) and historical prices.
   */
  template <class Decimal>
  class Security
    {
    public:
      typedef typename OHLCTimeSeries<Decimal>::ConstRandomAccessIterator ConstRandomAccessIterator;

      /**
     * @brief Constructs a Security object.
     * @param securitySymbol The ticker symbol (e.g., "MSFT", "ES").
     * @param securityName The full name of the security (e.g., "Microsoft Corp.", "E-mini S&P 500").
     * @param bigPointValue The currency value of a single full point move in the security's price.
     * 		(e.g., 1.0 for stocks, 50.0 for ES futures).
     * @param securityTick The minimum price fluctuation allowed for the security.
     * @param securityTimeSeries A shared pointer to the constant OHLC time series data for this security.
     * 		The Security object does not own this data directly but holds a reference.
     * @throws SecurityException if securityTimeSeries is null.
     */
      Security (const string& securitySymbol, const string& securityName,
		const Decimal& bigPointValue, const Decimal& securityTick,
		std::shared_ptr<const OHLCTimeSeries<Decimal>> securityTimeSeries) :
	mSecuritySymbol(securitySymbol),
	mSecurityName(securityName),
	mBigPointValue(bigPointValue),
	mTick(securityTick),
	mSecurityTimeSeries(securityTimeSeries),
	mTickDiv2(securityTick/DecimalConstants<Decimal>::DecimalTwo)
      {
	if (!securityTimeSeries)
	  throw SecurityException ("Class Security: time series object is null");
      }
      
      Security (const Security<Decimal> &rhs)
	: mSecuritySymbol(rhs.mSecuritySymbol),
	  mSecurityName(rhs.mSecurityName),
	  mBigPointValue(rhs.mBigPointValue),
	  mTick(rhs.mTick),
	  mSecurityTimeSeries(rhs.mSecurityTimeSeries),
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
	mTickDiv2 = rhs.mTickDiv2;
	
	return *this;
      }

      virtual ~Security()
      {}

      /**
       * @brief Pure virtual method to check if this security represents an equity.
       * @return `true` if it is an equity, `false` otherwise.
       */
      virtual bool isEquitySecurity() const = 0;

      /**
       * @brief Pure virtual method to check if this security represents a futures contract.
       * @return `true` if it is a futures contract, `false` otherwise.
       */
      virtual bool isFuturesSecurity() const = 0;

      virtual TradingVolume::VolumeUnit getTradingVolumeUnits() const = 0;

      /**
       * @brief Finds an iterator pointing to the time series entry for a specific date.
       * @param d The date to find.
       * @return A `ConstRandomAccessIterator` pointing to the entry if found,
       * or `getRandomAccessIteratorEnd()` if the date is not in the series.
       * @details Delegates to `OHLCTimeSeries::getRandomAccessIterator`.
       */

      Security::ConstRandomAccessIterator findTimeSeriesEntry (const ptime& d) const
      {
	return mSecurityTimeSeries->getRandomAccessIterator(d);
      }

      /**
       * @brief Finds an iterator pointing to the time series entry for a specific date.
       * @param d The date to find.
       * @return A `ConstRandomAccessIterator` pointing to the entry if found,
       * or `getRandomAccessIteratorEnd()` if the date is not in the series.
       */
      Security::ConstRandomAccessIterator findTimeSeriesEntry (const boost::gregorian::date& d) const
	{
	  return findTimeSeriesEntry(ptime(d, getDefaultBarTime()));
	}

      /*
       * @brief Gets a random access iterator pointing to the time series entry for a specific date.
       * @param d The date to retrieve the iterator for.
       * @return A `ConstRandomAccessIterator` pointing to the entry for the given date.
       * @throws SecurityException if the date `d` is not found in the time series.
       * @details Delegates to `OHLCTimeSeries::getRandomAccessIterator` and adds error checking.
       */
      Security::ConstRandomAccessIterator getRandomAccessIterator (const boost::posix_time::ptime& d) const
	{
	  Security::ConstRandomAccessIterator it = mSecurityTimeSeries->getRandomAccessIterator(d);
	  if (it != getRandomAccessIteratorEnd())
	    return mSecurityTimeSeries->getRandomAccessIterator(d);
	  else
	    throw SecurityException ("No time series entry for date: " +boost::posix_time::to_simple_string (d));
	}

      /**
       * @brief Gets a random access iterator pointing to the time series entry for a specific date.
       * @param d The date to retrieve the iterator for.
       * @return A `ConstRandomAccessIterator` pointing to the entry for the given date.
       * @throws SecurityException if the date `d` is not found in the time series.
       */
      Security::ConstRandomAccessIterator getRandomAccessIterator (const boost::gregorian::date& d) const
	{
	  return getRandomAccessIterator(ptime(d, getDefaultBarTime()));
	}

      /**
       * @brief Gets the time series entry (OHLC + Volume) for a specific date.
       * @param d The date to retrieve the entry for.
       * @return A constant reference to the `OHLCTimeSeriesEntry` for the given date.
       * @throws SecurityException if the date `d` is not found in the time series.
       * @details Uses `getRandomAccessIterator(d)` internally.
     */
      const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const boost::gregorian::date& d) const
	{
	  Security::ConstRandomAccessIterator it = this->getRandomAccessIterator (d);
	  return (*it);
	}

      /**
       * @brief Gets an iterator pointing to the beginning of the underlying time series.
       * @return A `ConstRandomAccessIterator`.
       * @details Delegates to `OHLCTimeSeries::beginRandomAccess`.
       */
      Security::ConstRandomAccessIterator getRandomAccessIteratorBegin() const
	{
	  return  mSecurityTimeSeries->beginRandomAccess();
	}

      /**
       * @brief Gets an iterator pointing past the end of the underlying time series.
       * @return A `ConstRandomAccessIterator`.
       * @details Delegates to `OHLCTimeSeries::endRandomAccess`.
       */
      Security::ConstRandomAccessIterator getRandomAccessIteratorEnd() const
	{
	  return  mSecurityTimeSeries->endRandomAccess();
	}

      /**
       * @brief Gets the time series entry at a specific offset relative to an iterator.
       * @param it A valid `ConstRandomAccessIterator` within the time series.
       * @param offset The offset (number of entries) from the iterator's position.
       * @return A constant reference to the `OHLCTimeSeriesEntry` at the specified offset.
       * @throws std::out_of_range if the resulting position is outside the time series bounds.
       * @details Delegates to `OHLCTimeSeries::getTimeSeriesEntry`.
       */
      const OHLCTimeSeriesEntry<Decimal>& getTimeSeriesEntry (const ConstRandomAccessIterator& it, 
							      unsigned long offset) const
      {
	return mSecurityTimeSeries->getTimeSeriesEntry(it, offset); 
      }

      const boost::posix_time::ptime&
      getDateTimeValue (const ConstRandomAccessIterator& it, unsigned long offset) const
      {
	return mSecurityTimeSeries->getDateTimeValue(it, offset);
      }

      /**
       * @brief Gets the date value at a specific offset relative to an iterator.
       * @param it A valid `ConstRandomAccessIterator` within the time series.
       * @param offset The offset (number of entries) from the iterator's position.
       * @return A constant reference to the `boost::gregorian::date` at the specified offset.
       * @throws std::out_of_range if the resulting position is outside the time series bounds.
       * @details Delegates to `OHLCTimeSeries::getDateValue`.
       */
      boost::gregorian::date
      getDateValue (const ConstRandomAccessIterator& it, unsigned long offset) const
      {
	return mSecurityTimeSeries->getDateValue(it, offset); 
      }

      /** @brief Gets the Open price at a specific offset relative to an iterator.  */
     const Decimal& getOpenValue (const ConstRandomAccessIterator& it, 
					unsigned long offset) const
      {
	return mSecurityTimeSeries->getOpenValue(it, offset); 
      }

      /** @brief Gets the high price at a specific offset relative to an iterator.  */
      const Decimal& getHighValue (const ConstRandomAccessIterator& it, 
					 unsigned long offset) const
      {
	return mSecurityTimeSeries->getHighValue(it, offset); 
      }

      /** @brief Gets the low price at a specific offset relative to an iterator.  */
      const Decimal& getLowValue (const ConstRandomAccessIterator& it, 
					unsigned long offset) const
      {
	return mSecurityTimeSeries->getLowValue(it, offset); 
      }

      /** @brief Gets the close price at a specific offset relative to an iterator.  */
      const Decimal& getCloseValue (const ConstRandomAccessIterator& it, 
				    unsigned long offset) const
      {
	return mSecurityTimeSeries->getCloseValue(it, offset); 
      }

      /** @brief Gets the volume at a specific offset relative to an iterator.  */
      const Decimal& getVolumeValue (const ConstRandomAccessIterator& it, 
					 unsigned long offset) const
      {
	return mSecurityTimeSeries->getVolumeValue(it, offset); 
      }

      /** @brief Gets the full name of the security. */
      const std::string& getName() const
      {
	return mSecurityName;
      }

       /** @brief Gets the ticker symbol of the security. */
      const std::string& getSymbol() const
      {
	return mSecuritySymbol;
      }

      /** @brief Gets the currency value of a single full point move for this security. */
      const Decimal& getBigPointValue() const
      {
	return mBigPointValue;
      }

      /** @brief Gets the minimum price fluctuation (tick size) for this security. */
      const Decimal& getTick() const
      {
	return mTick;
      }

      /** @brief Gets the pre-calculated value of Tick / 2, used for rounding calculations. */
      const Decimal& getTickDiv2() const
      {
	return mTickDiv2;
      }

      /** @brief Gets a shared pointer to the underlying constant time series data. */
      std::shared_ptr<const OHLCTimeSeries<Decimal>> getTimeSeries() const
      {
	return mSecurityTimeSeries;
      }

      /**
       * @brief Pure virtual method to create a clone of this Security object, potentially with a different time series.
       * @param securityTimeSeries A shared pointer to the constant OHLC time series data for the new cloned security.
       * This allows creating Securities representing the same instrument but over different date ranges or frequencies.
       * @return A `std::shared_ptr` to the newly created Security object (of the same derived type).
       */
      virtual std::shared_ptr<Security<Decimal>> 
      clone(std::shared_ptr<const OHLCTimeSeries<Decimal>> securityTimeSeries) const = 0;

    private:
      std::string mSecuritySymbol;
      std::string mSecurityName;
      Decimal mBigPointValue;
      Decimal mTick;
      std::shared_ptr<const OHLCTimeSeries<Decimal>> mSecurityTimeSeries;
      Decimal mTickDiv2;                    // Used to speedup compuation of Round@Tick
    };

  /**
   * @class EquitySecurity
   * @brief Concrete implementation of `Security` representing an equity (stock).
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Provides a concrete representation for stocks. It sets the Big Point Value to 1
   * and uses a default equity tick size of 0.01. It implements the
   * `isEquitySecurity`, `isFuturesSecurity`, and `clone` methods inherited from the `Security` base class.
   */
  template <class Decimal>
  class EquitySecurity : public Security<Decimal>
  {
  public:
    /**
     * @brief Constructs an EquitySecurity object.
     * @param securitySymbol The equity's ticker symbol (e.g., "AAPL").
     * @param securityName The equity's full name (e.g., "Apple Inc.").
     * @param securityTimeSeries A shared pointer to the constant OHLC time series data for this equity.
     * @details Initializes the base `Security` with a Big Point Value of 1 and the default equity tick size.
     */
    EquitySecurity (const string& securitySymbol, const string& securityName,
		    std::shared_ptr<const OHLCTimeSeries<Decimal>> securityTimeSeries) 
      : Security<Decimal> (securitySymbol, securityName, DecimalConstants<Decimal>::DecimalOne,
			   DecimalConstants<Decimal>::EquityTick, securityTimeSeries)
    {
    }

    /**
     * @brief Creates a clone of this EquitySecurity, potentially with a different time series.
     * @param securityTimeSeries The time series for the cloned security.
     * @return A `std::shared_ptr` to the new `EquitySecurity` object.
     */
    std::shared_ptr<Security<Decimal>> clone(std::shared_ptr<const OHLCTimeSeries<Decimal>> securityTimeSeries) const
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

    /**
     * @brief Identifies this security as an equity.
     * @return `true`.
     */
    bool isEquitySecurity() const
    {
      return true;
    }

    /**
     * @brief Identifies this security as not a future.
     * @return `false`.
     */
    bool isFuturesSecurity() const
    {
      return false;
    }

    TradingVolume::VolumeUnit getTradingVolumeUnits() const
    {
      return TradingVolume::SHARES;
    }
  };

  /**
   * @class FuturesSecurity
   * @brief Concrete implementation of `Security` representing a futures contract.
   * @tparam Decimal The numeric type used for financial calculations.
   *
   * @details Provides a concrete representation for futures contracts. Unlike equities,
   * the Big Point Value and Tick Size must be explicitly provided in the constructor
   * as they vary significantly between different futures contracts. It implements the
   * `isEquitySecurity`, `isFuturesSecurity`, and `clone` methods inherited from the `Security` base class.
   */
  template <class Decimal>
  class FuturesSecurity : public Security<Decimal>
  {
  public:
    /**
     * @brief Constructs a FuturesSecurity object.
     * @param securitySymbol The futures contract symbol (e.g., "ES").
     * @param securityName The futures contract name (e.g., "E-mini S&P 500").
     * @param bigPointValue The currency value of a single full point move for this contract (e.g., 50.0 for ES).
     * @param securityTick The minimum price fluctuation (tick size) for this contract (e.g., 0.25 for ES).
     * @param securityTimeSeries A shared pointer to the constant OHLC time series data for this futures contract.
     */
    FuturesSecurity (const string& securitySymbol, const string& securityName,
		     const Decimal& bigPointValue, const Decimal& securityTick,
		     std::shared_ptr<const OHLCTimeSeries<Decimal>> securityTimeSeries)
      : Security<Decimal> (securitySymbol, securityName, bigPointValue, securityTick,
		  securityTimeSeries)
    {}

    /**
     * @brief Creates a clone of this FuturesSecurity, potentially with a different time series.
     * @param securityTimeSeries The time series for the cloned security.
     * @return A `std::shared_ptr` to the new `FuturesSecurity` object.
     */
    std::shared_ptr<Security<Decimal>> clone(std::shared_ptr<const OHLCTimeSeries<Decimal>> securityTimeSeries) const
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

    /**
     * @brief Identifies this security as not an equity.
     * @return `false`.
     */
    bool isEquitySecurity() const
    {
      return false;
    }

    /**
     * @brief Identifies this security as a future.
     * @return `true`.
     */
    bool isFuturesSecurity() const
    {
      return true;
    }

    TradingVolume::VolumeUnit getTradingVolumeUnits() const
    {
      return TradingVolume::CONTRACTS;
    }
    
  };
}


#endif
