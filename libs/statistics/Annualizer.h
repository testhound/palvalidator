#ifndef __ANNUALIZER_H
#define __ANNUALIZER_H

#include <cmath>
#include <limits>
#include <stdexcept>
#include <ostream>
#include "number.h"
#include "DecimalConstants.h"
#include "TimeFrame.h"

namespace mkc_timeseries
{
  /**
   * @brief Centralized annualization factor calculator.
   *
   * Mirrors the logic previously implemented in BiasCorrectedBootstrap.h
   * (calculateAnnualizationFactor), but lives here so it can be reused by
   * non-BCa modules.
   *
   * @param timeFrame The time frame of the data (e.g., DAILY, WEEKLY, INTRADAY).
   * @param intraday_minutes_per_bar Minutes per bar for INTRADAY data; must be > 0.
   * @param trading_days_per_year Number of trading days per year (default 252).
   * @param trading_hours_per_day Number of trading hours per day (default 6.5).
   */
  inline double computeAnnualizationFactor(TimeFrame::Duration timeFrame,
                                           int intraday_minutes_per_bar = 0,
                                           double trading_days_per_year = 252.0,
                                           double trading_hours_per_day = 6.5)
  {
    switch (timeFrame)
      {
      case TimeFrame::DAILY:
        return trading_days_per_year;

      case TimeFrame::WEEKLY:
        return 52.0;

      case TimeFrame::MONTHLY:
        return 12.0;

      case TimeFrame::INTRADAY:
        {
          if (intraday_minutes_per_bar == 0)
            {
              throw std::invalid_argument(
					  "computeAnnualizationFactor(INTRADAY): intraday_minutes_per_bar must be specified.");
            }

          const double bars_per_hour = 60.0 / static_cast<double>(intraday_minutes_per_bar);
          if (!(bars_per_hour > 0.0) ||
              !(trading_days_per_year > 0.0) ||
              !(trading_hours_per_day > 0.0))
            {
              throw std::invalid_argument("Annualization inputs must be positive finite values.");
            }

          return trading_hours_per_day * bars_per_hour * trading_days_per_year;
        }

      case TimeFrame::QUARTERLY:
        return 4.0;

      case TimeFrame::YEARLY:
        return 1.0;

      default:
        throw std::invalid_argument("Unsupported time frame for annualization.");
      }
  }

  /**
   * @brief Convenience helper: compute annualization factor given a time frame
   *        and an associated time series object.
   *
   * This removes the need for each caller to:
   *   - check for INTRADAY
   *   - query getIntradayTimeFrameDurationInMinutes()
   *   - call the intraday overload explicitly
   *
   * @tparam TimeSeriesPtr
   *   Pointer or smart pointer to a time series type that implements:
   *     int getIntradayTimeFrameDurationInMinutes() const;
   */
  template <typename TimeSeriesPtr>
  inline double computeAnnualizationFactorForSeries(TimeFrame::Duration timeFrame,
						    const TimeSeriesPtr& ts,
						    double trading_days_per_year = 252.0,
						    double trading_hours_per_day = 6.5)
  {
    if (timeFrame == TimeFrame::INTRADAY)
      {
        if (ts)
          {
            const int minutesPerBar =
              ts->getIntradayTimeFrameDurationInMinutes();

            return computeAnnualizationFactor(
					      timeFrame,
					      minutesPerBar,
					      trading_days_per_year,
					      trading_hours_per_day);
          }
      }

    // Non-intraday (or missing series): rely on the TimeFrame-only variant.
    // For non-INTRADAY, intraday_minutes_per_bar is ignored.
    return computeAnnualizationFactor(
				      timeFrame,
				      0,
				      trading_days_per_year,
				      trading_hours_per_day);
  }


  template <typename NumT>
  inline double computeEffectiveAnnualizationFactor(NumT annualizedTrades,
						    unsigned int medianHoldBars,
						    double baseAnnualizationFactor,
						    std::ostream* os = nullptr)
  {
    const double at   = num::to_double(annualizedTrades);
    const double Keff = std::max(1.0, at * static_cast<double>(medianHoldBars));  // clamp to >= 1
    if (os) {
      const double p = (baseAnnualizationFactor > 0.0) ? (Keff / baseAnnualizationFactor) : 1.0;
      (*os) << "      [Bootstrap] Annualization factor (base) = " << baseAnnualizationFactor
	    << ", effective (participation-weighted) = " << Keff
	    << "  (p=" << p << ")\n";
    }
    return Keff;
  }

  /**
   * Annualizer for per-period returns.
   *
   * Provides stable annualization via:  (1 + r)^K - 1
   * implemented as exp(K * log1p(r)) - 1 with guards around r <= -1.
   *
   * Use annualize_one() for a single value, or annualize_triplet() for (lower, mean, upper).
   */
  template <class Decimal>
  class Annualizer
  {
  public:
    struct Triplet
    {
      Decimal lower{};
      Decimal mean{};
      Decimal upper{};
    };

    /**
     * Annualize a single per-period return r to K periods.
     *
     * Guards:
     * - If r <= -1, clamp to (-1 + eps) to keep log1p defined.
     * - After transform, if numerical underflow produces exactly -1,
     *   bump to (-1 + bump) so the result remains > -1 in Decimal quantization.
     */
    static Decimal annualize_one(const Decimal& r, double K,
				 double eps = 1e-12,
				 long double bump = 1e-7L)
    {
      if (!(K > 0.0) || !std::isfinite(K))
	{
	  throw std::invalid_argument("Annualizer: K must be positive and finite.");
	}

      const Decimal neg1  = DecimalConstants<Decimal>::DecimalMinusOne;
      const Decimal epsD  = Decimal(eps);
      const Decimal r_clip = (r > neg1) ? r : (neg1 + epsD);

      const long double lr = std::log1p(static_cast<long double>(num::to_double(r_clip)));
      const long double KK = static_cast<long double>(K);
      long double y = std::exp(KK * lr) - 1.0L;

      if (y <= -1.0L)
	{
	  y = -1.0L + bump;
	}
      return Decimal(static_cast<double>(y));
    }

    /**
     * Annualize (lower, mean, upper) together with the same settings.
     */
    static Triplet annualize_triplet(const Decimal& lower,
				     const Decimal& mean,
				     const Decimal& upper,
				     double K,
				     double eps = 1e-12,
				     long double bump = 1e-7L)
    {
      Triplet t;
      t.lower = annualize_one(lower, K, eps, bump);
      t.mean  = annualize_one(mean,  K, eps, bump);
      t.upper = annualize_one(upper, K, eps, bump);
      return t;
    }

    /**
     * De-annualize a K-period compounded return R back to a single-period return r.
     *
     * Inverse of annualize_one:
     *   R = (1 + r)^K - 1   ⇒   r = exp( log1p(R) / K ) - 1
     *
     * Guards mirror annualize_one():
     * - If K <= 0 or not finite → throw std::invalid_argument.
     * - If R <= -1 → clamp to (-1 + eps) so log1p(R) stays defined.
     * - If exp(...) - 1 underflows to exactly -1 in Decimal quantization,
     *   bump slightly toward > -1 (same 'bump' convention as annualize_one).
     */
    static Decimal deannualize_one(const Decimal& R, double K,
				   double eps = 1e-12,
				   long double bump = 1e-7L)
    {
      if (!(K > 0.0) || !std::isfinite(K))
	throw std::invalid_argument("Annualizer::deannualize_one: invalid K");

      // Clamp near -1 to keep log1p safe
      Decimal Rclamped = R;
      if (Rclamped <= -Decimal(1)) {
	// move to (-1 + eps) in Decimal space
	Rclamped = Decimal(-1) + Decimal(eps);
      }

      // r = exp( log1p(R) / K ) - 1

      long double lp1 = std::log1p(static_cast<long double>(Rclamped.getAsDouble()));
      long double r_ld = std::exp(lp1 / static_cast<long double>(K)) - 1.0L;
      
      // Quantization safety: avoid landing exactly at -1
      if (r_ld <= -1.0L) {
	r_ld = -1.0L + bump;
      }

      return Decimal(static_cast<double>(r_ld));
    }

    /**
     * De-annualize a (lower, mean, upper) triplet to per-period.
     * Monotone transform preserves ordering.
     */
    static Triplet deannualize_triplet(const Triplet& t, double K,
				       double eps = 1e-12,
				       long double bump = 1e-7L)
    {
      Triplet out;
      out.lower = deannualize_one(t.lower, K, eps, bump);
      out.mean  = deannualize_one(t.mean,  K, eps, bump);
      out.upper = deannualize_one(t.upper, K, eps, bump);
      return out;
    }
  };
} // namespace mkc_timeseries

#endif // __ANNUALIZER_H
