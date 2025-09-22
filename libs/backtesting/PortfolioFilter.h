// PortfolioFilter.h
#pragma once
#include "TimeSeries.h"
#include "TimeSeriesIndicators.h"
#include "BiasCorrectedBootstrap.h"   // calculateAnnualizationFactor
#include "number.h"
#include "TimeSeriesException.h"      // TimeSeriesDataNotFoundException

namespace mkc_timeseries
{

  template <class Decimal>
  class PortfolioFilter
  {
  public:
    PortfolioFilter() = default;
    virtual ~PortfolioFilter() = default;
    virtual bool areEntriesAllowed(const boost::posix_time::ptime& dt) const = 0;
  };

  template <class Decimal, class VolPolicy = CloseToCloseVolatilityPolicy>
  class AdaptiveVolatilityPortfolioFilter : public PortfolioFilter<Decimal>
  {
  public:
    // ctor: rsquaredPeriod fixed at 20 per spec; prPeriod provided by caller
    AdaptiveVolatilityPortfolioFilter(const OHLCTimeSeries<Decimal>& ohlc,
				      uint32_t percentRankPeriod)
      : m_filterSeries(buildFilterSeries(ohlc, percentRankPeriod)) {}

    ~AdaptiveVolatilityPortfolioFilter() override = default;

    bool areEntriesAllowed(const boost::posix_time::ptime& dt) const override
    {
      // Look up the value at dt (fall back to date-normalized key)
      try {
        auto entry = m_filterSeries.getTimeSeriesEntry(dt.date());  // get entry by date
        const Decimal& v = entry.getValue();                        // entry value at dt
        return v.getAsDouble() < 0.75;                              // allow if < 75th pct
      } catch (const TimeSeriesDataNotFoundException&) {
        // If not found, disallow by default (no signal yet)
        return false;
      }
    }

  private:
    static NumericTimeSeries<Decimal>
    buildFilterSeries(const OHLCTimeSeries<Decimal>& ohlc, uint32_t prPeriod)
    {
      // Compute annualization factor from the series' timeframe; for INTRADAY supply minutes/bar
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

      // Build the percent-ranked adaptive annualized volatility series
      return AdaptiveVolatilityPercentRankAnnualizedSeries<Decimal, VolPolicy>(
								    ohlc,
								    /*rSquaredPeriod=*/20,
								    /*percentRankPeriod=*/prPeriod,
								    /*annualizationFactor=*/annualization);
    }

    NumericTimeSeries<Decimal> m_filterSeries; // stored filter time series
  };

  template <class Decimal>
  class NoPortfolioFilter : public PortfolioFilter<Decimal>
  {
  public:
    // Constructor takes no arguments and has empty body
    NoPortfolioFilter() {}

    ~NoPortfolioFilter() override = default;

    // areEntriesAllowed method always returns true
    bool areEntriesAllowed(const boost::posix_time::ptime& dt) const override
    {
      return true;
    }
  };

} // namespace mkc_timeseries
