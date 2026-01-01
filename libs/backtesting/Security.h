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
#include "TimeSeries.h" // Assumed to have the new OHLCTimeSeries API
#include "DecimalConstants.h"

using std::string;

namespace mkc_timeseries
{
  using boost::posix_time::ptime;
  using boost::gregorian::date; 

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
   * - Providing iterators for direct, sorted access to the underlying time series entries.
   *
   * Collaborations:
   * - OHLCTimeSeries: Holds a `std::shared_ptr<const OHLCTimeSeries<Decimal>>` (`mSecurityTimeSeries`).
   * This is the primary collaboration for accessing price history. Many methods delegate directly to this object.
   * Access methods now return `OHLCTimeSeriesEntry<Decimal>` by copy or throw exceptions
   * (e.g., `TimeSeriesDataNotFoundException`, `TimeSeriesOffsetOutOfRangeException`).
   * It also provides `beginSortedAccess` and `endSortedAccess` for full iteration.
   *
   * - OHLCTimeSeriesEntry: Individual data points returned when accessing the time series or iterating.
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
      /**
       * @brief Iterator type for direct, sorted access to time series entries.
       * This is typically a `std::vector<OHLCTimeSeriesEntry<Decimal>>::const_iterator`.
       */
      typedef typename OHLCTimeSeries<Decimal>::ConstSortedIterator ConstSortedIterator;

      /**
     * @brief Constructs a Security object.
     * @param securitySymbol The ticker symbol (e.g., "MSFT", "ES").
     * @param securityName The full name of the security (e.g., "Microsoft Corp.", "E-mini S&P 500").
     * @param bigPointValue The currency value of a single full point move in the security's price.
     * (e.g., 1.0 for stocks, 50.0 for ES futures).
     * @param securityTick The minimum price fluctuation allowed for the security.
     * @param securityTimeSeries A shared pointer to the constant OHLC time series data for this security.
     * The Security object does not own this data directly but holds a reference.
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

      Security(Security<Decimal>&& rhs) noexcept
	: mSecuritySymbol(std::move(rhs.mSecuritySymbol)),
	  mSecurityName(std::move(rhs.mSecurityName)),
	  mBigPointValue(std::move(rhs.mBigPointValue)),
	  mTick(std::move(rhs.mTick)),
	  mSecurityTimeSeries(std::move(rhs.mSecurityTimeSeries)),
	  mTickDiv2(std::move(rhs.mTickDiv2))
      {}

      Security<Decimal>& operator=(Security<Decimal>&& rhs) noexcept
      {
	if (this != &rhs) {
	  mSecuritySymbol = std::move(rhs.mSecuritySymbol);
	  mSecurityName = std::move(rhs.mSecurityName);
	  mBigPointValue = std::move(rhs.mBigPointValue);
	  mTick = std::move(rhs.mTick);
	  mSecurityTimeSeries = std::move(rhs.mSecurityTimeSeries);
	  mTickDiv2 = std::move(rhs.mTickDiv2);
	}
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
       * @brief Gets the time series entry (OHLC + Volume) for a specific date.
       * @param d The date to retrieve the entry for.
       * @return A copy of the `OHLCTimeSeriesEntry` for the given date.
       * @throws TimeSeriesDataNotFoundException if the date `d` is not found in the time series.
       * @details Delegates to `OHLCTimeSeries::getTimeSeriesEntry`.
       */
      OHLCTimeSeriesEntry<Decimal> getTimeSeriesEntry (const date& d) const
	{
	  return mSecurityTimeSeries->getTimeSeriesEntry(d);
	}

      /**
       * @brief Gets the time series entry (OHLC + Volume) for a specific ptime.
       * @param dt The ptime to retrieve the entry for.
       * @return A copy of the `OHLCTimeSeriesEntry` for the given ptime.
       * @throws TimeSeriesDataNotFoundException if the ptime `dt` is not found in the time series.
       * @details Delegates to `OHLCTimeSeries::getTimeSeriesEntry`.
       */
      OHLCTimeSeriesEntry<Decimal> getTimeSeriesEntry (const ptime& dt) const
	{
	  return mSecurityTimeSeries->getTimeSeriesEntry(dt);
	}
      
      /**
       * @brief Retrieves a time series entry relative to a base date by a specific offset.
       * @param base_d The base date from which to offset.
       * @param offset_bars_ago The number of bars to offset from base_d.
       * 0 means the entry for base_d itself.
       * Positive values mean bars prior to base_d (earlier in time).
       * Negative values mean bars after base_d (later in time).
       * @return A copy of the target `OHLCTimeSeriesEntry`.
       * @throws TimeSeriesDataNotFoundException if base_d is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset leads to an out-of-bounds access.
       * @details Delegates to `OHLCTimeSeries::getTimeSeriesEntry`.
       */
      OHLCTimeSeriesEntry<Decimal> getTimeSeriesEntry (const date& base_d, long offset_bars_ago) const
      {
        return mSecurityTimeSeries->getTimeSeriesEntry(base_d, offset_bars_ago);
      }

      /**
       * @brief Retrieves a time series entry relative to a base ptime by a specific offset.
       * @param base_dt The base ptime from which to offset.
       * @param offset_bars_ago The number of bars to offset from base_dt.
       * 0 means the entry for base_dt itself.
       * Positive values mean bars prior to base_dt (earlier in time).
       * Negative values mean bars after base_dt (later in time).
       * @return A copy of the target `OHLCTimeSeriesEntry`.
       * @throws TimeSeriesDataNotFoundException if base_dt is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset leads to an out-of-bounds access.
       * @details Delegates to `OHLCTimeSeries::getTimeSeriesEntry`.
       */
      OHLCTimeSeriesEntry<Decimal> getTimeSeriesEntry (const ptime& base_dt, long offset_bars_ago) const
      {
        return mSecurityTimeSeries->getTimeSeriesEntry(base_dt, offset_bars_ago);
      }

      /**
       * @brief Gets an iterator pointing to the beginning of the underlying time series entries, sorted by time.
       * @return A `ConstSortedIterator`.
       * @warning The returned iterator is invalidated by any modification (e.g., addEntry, deleteEntryByDate)
       * to the underlying OHLCTimeSeries instance. Use with caution, especially in concurrent scenarios.
       * @details Delegates to `OHLCTimeSeries::beginSortedAccess`.
       */
      ConstSortedIterator beginSortedEntries() const
      {
        return mSecurityTimeSeries->beginSortedAccess();
      }

      /**
       * @brief Gets an iterator pointing past the end of the underlying time series entries.
       * @return A `ConstSortedIterator`.
       * @warning See warning for `beginSortedEntries()`. The returned iterator is invalidated by any modification
       * to the underlying OHLCTimeSeries.
       * @details Delegates to `OHLCTimeSeries::endSortedAccess`.
       */
      ConstSortedIterator endSortedEntries() const
      {
        return mSecurityTimeSeries->endSortedAccess();
      }

      /** @brief Gets the Open price for a bar specified by a base date and an offset.
       * @param base_d The base date.
       * @param offset_bars_ago Number of bars prior to base_d (0 for base_d's bar).
       * @return The Open price.
       * @throws TimeSeriesDataNotFoundException if the base date is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset is out of bounds.
       * @details Delegates to `OHLCTimeSeries::getOpenValue`.
       */
      Decimal getOpenValue (const date& base_d, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getOpenValue(base_d, offset_bars_ago); 
      }

      /** @brief Gets the Open price for a bar specified by a base ptime and an offset.
       * @param base_dt The base ptime.
       * @param offset_bars_ago Number of bars prior to base_dt (0 for base_dt's bar).
       * @return The Open price.
       * @throws TimeSeriesDataNotFoundException if the base ptime is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset is out of bounds.
       * @details Delegates to `OHLCTimeSeries::getOpenValue`.
       */
      Decimal getOpenValue (const ptime& base_dt, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getOpenValue(base_dt, offset_bars_ago); 
      }

      /** @brief Gets the High price for a bar specified by a base date and an offset. */
      Decimal getHighValue (const date& base_d, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getHighValue(base_d, offset_bars_ago); 
      }

      /** @brief Gets the High price for a bar specified by a base ptime and an offset. */
      Decimal getHighValue (const ptime& base_dt, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getHighValue(base_dt, offset_bars_ago); 
      }

      /** @brief Gets the Low price for a bar specified by a base date and an offset. */
      Decimal getLowValue (const date& base_d, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getLowValue(base_d, offset_bars_ago); 
      }

      /** @brief Gets the Low price for a bar specified by a base ptime and an offset. */
      Decimal getLowValue (const ptime& base_dt, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getLowValue(base_dt, offset_bars_ago); 
      }
      
      /** @brief Gets the Close price for a bar specified by a base date and an offset. */
      Decimal getCloseValue (const date& base_d, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getCloseValue(base_d, offset_bars_ago); 
      }

      /** @brief Gets the Close price for a bar specified by a base ptime and an offset. */
      Decimal getCloseValue (const ptime& base_dt, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getCloseValue(base_dt, offset_bars_ago); 
      }

      /** @brief Gets the Volume for a bar specified by a base date and an offset. */
      Decimal getVolumeValue (const date& base_d, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getVolumeValue(base_d, offset_bars_ago); 
      }

      /** @brief Gets the Volume for a bar specified by a base ptime and an offset. */
      Decimal getVolumeValue (const ptime& base_dt, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getVolumeValue(base_dt, offset_bars_ago); 
      }

      /** @brief Gets the date component for a bar specified by a base date and an offset.
       * @param base_d The base date.
       * @param offset_bars_ago Number of bars prior to base_d (0 for base_d's bar).
       * @return The `boost::gregorian::date` of the target bar.
       * @throws TimeSeriesDataNotFoundException if the base date is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset is out of bounds.
       * @details Delegates to `OHLCTimeSeries::getDateValue`.
       */
      date getDateValue (const date& base_d, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getDateValue(base_d, offset_bars_ago); 
      }

      /** @brief Gets the date component for a bar specified by a base ptime and an offset.
       * @param base_dt The base ptime.
       * @param offset_bars_ago Number of bars prior to base_dt (0 for base_dt's bar).
       * @return The `boost::gregorian::date` of the target bar.
       * @throws TimeSeriesDataNotFoundException if the base ptime is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset is out of bounds.
       * @details Delegates to `OHLCTimeSeries::getDateValue`.
       */
      date getDateValue (const ptime& base_dt, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getDateValue(base_dt, offset_bars_ago); 
      }

      /** @brief Gets the full ptime timestamp for a bar specified by a base date and an offset.
       * @param base_d The base date.
       * @param offset_bars_ago Number of bars prior to base_d (0 for base_d's bar).
       * @return The `boost::posix_time::ptime` of the target bar.
       * @throws TimeSeriesDataNotFoundException if the base date is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset is out of bounds.
       * @details Delegates to `OHLCTimeSeries::getDateTimeValue`.
       */
      ptime getDateTimeValue (const date& base_d, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getDateTimeValue(base_d, offset_bars_ago);
      }

      /** @brief Gets the full ptime timestamp for a bar specified by a base ptime and an offset.
       * @param base_dt The base ptime.
       * @param offset_bars_ago Number of bars prior to base_dt (0 for base_dt's bar).
       * @return The `boost::posix_time::ptime` of the target bar.
       * @throws TimeSeriesDataNotFoundException if the base ptime is not found.
       * @throws TimeSeriesOffsetOutOfRangeException if the offset is out of bounds.
       * @details Delegates to `OHLCTimeSeries::getDateTimeValue`.
       */
      ptime getDateTimeValue (const ptime& base_dt, unsigned long offset_bars_ago) const
      {
	return mSecurityTimeSeries->getDateTimeValue(base_dt, offset_bars_ago);
      }

      /**
       * @brief Check if a date exists in the time series.
       * @param d The date to check for.
       * @return `true` if the date exists, `false` otherwise.
       * @details Delegates to `OHLCTimeSeries::isDateFound`.
       */
      bool isDateFound(const date& d) const
      {
        return mSecurityTimeSeries->isDateFound(d);
      }

      /**
       * @brief Check if a ptime exists in the time series.
       * @param pt The ptime to check for.
       * @return `true` if the ptime exists, `false` otherwise.
       * @details Delegates to `OHLCTimeSeries::isDateFound`.
       */
      bool isDateFound(const ptime& pt) const
      {
        return mSecurityTimeSeries->isDateFound(pt);
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
       * @brief Replace the underlying time series pointer for this Security.
       *
       * Intended for synthetic/permutation workflows where the Security's identity
       * (symbol, tick size, BPV) stays the same but the price history changes.
       * Not thread-safe against concurrent readers; ensure per-thread usage.
       *
       * @param newTimeSeries Shared pointer to the new (const) OHLC time series.
       * @throws SecurityException if newTimeSeries is null.
       */
      void resetTimeSeries(std::shared_ptr<const OHLCTimeSeries<Decimal>> newTimeSeries)
      {
	if (!newTimeSeries)
	  throw SecurityException("Security::resetTimeSeries: null time series");

	mSecurityTimeSeries = std::move(newTimeSeries);
      }

      /**
       * @brief Gets the intraday time frame duration for this security's time series.
       * @return boost::posix_time::time_duration representing the most common interval between bars
       * @throws TimeSeriesException if the security's time frame is not INTRADAY or insufficient data
       * @details Delegates to the underlying OHLCTimeSeries::getIntradayTimeFrameDuration method.
       * This is a convenience method for backtesting clients who often work with Security pointers.
       */
      boost::posix_time::time_duration getIntradayTimeFrameDuration() const
      {
	return mSecurityTimeSeries->getIntradayTimeFrameDuration();
      }

      /**
       * @brief Gets the intraday time frame duration in minutes for this security's time series.
       * @return long representing the most common interval between bars in minutes
       * @throws TimeSeriesException if the security's time frame is not INTRADAY or insufficient data
       * @details Delegates to the underlying OHLCTimeSeries::getIntradayTimeFrameDurationInMinutes method.
       * This is a convenience method for backtesting clients who need the duration in minutes.
       */
      long getIntradayTimeFrameDurationInMinutes() const
      {
	return mSecurityTimeSeries->getIntradayTimeFrameDurationInMinutes();
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

    EquitySecurity(EquitySecurity<Decimal>&& rhs) noexcept
      : Security<Decimal>(std::move(rhs))
    {}

    // Move assignment operator
    EquitySecurity<Decimal>& operator=(EquitySecurity<Decimal>&& rhs) noexcept
    {
      if (this != &rhs)
	{
	  Security<Decimal>::operator=(std::move(rhs));
	}
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

    FuturesSecurity(FuturesSecurity<Decimal>&& rhs) noexcept
      : Security<Decimal>(std::move(rhs))
    {}
    
    // Move assignment operator
    FuturesSecurity<Decimal>& operator=(FuturesSecurity<Decimal>&& rhs) noexcept
    {
      if (this != &rhs)
	{
	  Security<Decimal>::operator=(std::move(rhs));
	}
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
} // namespace mkc_timeseries


#endif // __SECURITY_H
