// PortfolioFilter.h
#pragma once
#include "TimeSeries.h"
#include "TimeSeriesIndicators.h"
#include "BiasCorrectedBootstrap.h"   // calculateAnnualizationFactor
#include "number.h"
#include "TimeSeriesException.h"      // TimeSeriesDataNotFoundException

namespace mkc_timeseries
{
  /**
   * @brief Abstract base class for portfolio entry filtering strategies.
   * @details This class defines the interface for different filtering mechanisms that
   * determine whether a new position should be allowed to be opened at a given time.
   * Concrete implementations provide specific filtering logic.
   * @tparam Decimal The floating-point type used for calculations (e.g., float, double).
   */
  template <class Decimal>
  class PortfolioFilter
  {
  public:
    PortfolioFilter() = default;
    virtual ~PortfolioFilter() = default;

    /**
     * @brief Pure virtual function to determine if a new portfolio entry is permitted.
     * @param dt The date and time to check for entry allowance.
     * @return true if an entry is allowed at the specified time, false otherwise.
     */
    virtual bool areEntriesAllowed(const boost::posix_time::ptime& dt) const = 0;
  };

  /**
   * @brief A portfolio filter that restricts entries during periods of high volatility.
   * @details This filter calculates the percent rank of the annualized volatility over a
   * specified period. It allows new entries only when the current volatility is below a
   * certain threshold (e.g., the 75th percentile), effectively avoiding trades in
   * excessively volatile market conditions.
   * @tparam Decimal The floating-point type used for calculations.
   * @tparam VolPolicy A policy class that defines how to calculate volatility (e.g., CloseToCloseVolatilityPolicy).
   */
  template <class Decimal, class VolPolicy = CloseToCloseVolatilityPolicy>
  class AdaptiveVolatilityPortfolioFilter : public PortfolioFilter<Decimal>
  {
  public:
    /**
     * @brief Constructs the filter with a default percent rank period.
     * @details The percent rank period is determined by the time frame of the input series.
     * @param ohlc The underlying Open-High-Low-Close time series.
     */
    AdaptiveVolatilityPortfolioFilter(const OHLCTimeSeries<Decimal>& ohlc)
      : AdaptiveVolatilityPortfolioFilter(ohlc, StandardPercentRankPeriod(ohlc.getTimeFrame()))
    {}

    /**
     * @brief Constructs the filter with a specified percent rank period.
     * @param ohlc The underlying Open-High-Low-Close time series.
     * @param percentRankPeriod The lookback period for the percent rank calculation.
     */
    AdaptiveVolatilityPortfolioFilter(const OHLCTimeSeries<Decimal>& ohlc,
				      uint32_t percentRankPeriod)
      : m_filterSeries(buildFilterSeries(ohlc, percentRankPeriod))
    {}

    ~AdaptiveVolatilityPortfolioFilter() override = default;

    /**
     * @brief Checks if entries are allowed based on the volatility at a given time.
     * @details Returns true if the volatility percent rank at the specified datetime is
     * less than 0.75 (the 75th percentile). If data for the given date is not
     * found in the series, it returns false by default.
     * @param dt The date and time to check.
     * @return true if volatility is below the threshold, false otherwise.
     */
    bool areEntriesAllowed(const boost::posix_time::ptime& dt) const override
    {
      try
	{
	  // Get the pre-computed filter value by date
	  auto entry = m_filterSeries.getTimeSeriesEntry(dt.date());
	  const Decimal& v = entry.getValue();
	  // Allow entry if the volatility is below the 75th percentile
	  return v.getAsDouble() < 0.75;
	}
      catch (const TimeSeriesDataNotFoundException&)
	{
	  // If no data is found for the date, disallow entry by default.
	  return false;
	}
    }

  private:
    /**
     * @brief A static helper function to build the volatility filter series.
     * @details This function computes an annualization factor based on the series's
     * time frame and then constructs the annualized, percent-ranked adaptive
     * volatility time series used by the filter.
     * @param ohlc The source OHLC time series.
     * @param prPeriod The lookback period for the percent rank calculation.
     * @return A NumericTimeSeries containing the computed volatility filter values.
     */
    static NumericTimeSeries<Decimal>
    buildFilterSeries(const OHLCTimeSeries<Decimal>& ohlc, uint32_t prPeriod)
    {
      // Compute annualization factor from the series' timeframe.
      double annualization = 0.0;
      const auto tf = ohlc.getTimeFrame();
      if (tf == TimeFrame::INTRADAY)
	{
	  const unsigned int minutes = static_cast<unsigned int>(ohlc.getIntradayTimeFrameDurationInMinutes());
	  annualization = calculateAnnualizationFactor(tf, minutes);
	}
      else
	{
	  annualization = calculateAnnualizationFactor(tf);
	}

      // Build the percent-ranked adaptive annualized volatility series.
      return AdaptiveVolatilityPercentRankAnnualizedSeries<Decimal, VolPolicy>(
								    ohlc,
								    /*rSquaredPeriod=*/20,
								    /*percentRankPeriod=*/prPeriod,
								    /*annualizationFactor=*/annualization);
    }

    /// @brief The time series holding the computed volatility filter values.
    NumericTimeSeries<Decimal> m_filterSeries;
  };

  /**
   * @brief A "pass-through" filter that always allows portfolio entries.
   * @details This class serves as a null object implementation of the PortfolioFilter.
   * It can be used to disable entry filtering while maintaining a consistent
   * polymorphic interface in the trading system.
   * @tparam Decimal The floating-point type used for calculations.
   */
  template <class Decimal>
  class NoPortfolioFilter : public PortfolioFilter<Decimal>
  {
  public:
    /**
     * @brief Constructs the NoPortfolioFilter.
     * @details The OHLC series argument is accepted for polymorphic compatibility
     * with other filter types but is not used.
     * @param ohlc An unused OHLC time series.
     */
    NoPortfolioFilter(const OHLCTimeSeries<Decimal>& ohlc) { (void)ohlc; }

    ~NoPortfolioFilter() override = default;

    /**
     * @brief Always returns true, allowing all entries.
     * @param dt The date and time to check (unused).
     * @return Always true.
     */
    bool areEntriesAllowed(const boost::posix_time::ptime& dt) const override
    {
      return true;
    }
  };

} // namespace mkc_timeseries
