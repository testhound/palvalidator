// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
// BCa (Bias-Corrected and Accelerated) Bootstrap Implementation
// Developed with assistance from Gemini

#ifndef __BCA_BOOTSTRAP_H
#define __BCA_BOOTSTRAP_H

#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <limits>

#include "DecimalConstants.h"
#include "number.h"
#include "randutils.hpp"
#include "TimeFrame.h"
#include "StatUtils.h"
namespace mkc_timeseries
{
  /**
   * @class BCaBootStrap
   * @brief Computes the mean and Bias-Corrected and Accelerated (BCa) bootstrap confidence intervals for a sample.
   *
   * This class implements the BCa bootstrap algorithm as described in:
   * - Efron & Tibshirani (1993). *An Introduction to the Bootstrap*
   * - Hyndman & Fan (1996). "Sample Quantiles in Statistical Packages," *The American Statistician*
   *
   * The BCa algorithm corrects percentile-based confidence intervals for both bias and skewness
   * in the bootstrap distribution by calculating two parameters:
   *  - z0 (bias-correction)
   *  - a  (acceleration, derived from the jackknife)
   *
   * After transforming the desired alpha level (e.g., 2.5%) into adjusted percentiles using these
   * parameters, the method computes the lower and upper bounds of the confidence interval
   * by selecting elements from the sorted bootstrap distribution.
   *
   * The quantile selection uses the "unbiased quantile estimator" method based on order statistics.
   * Instead of simply indexing the sorted bootstrap distribution at B*alpha (which is biased for small B),
   * we follow the approach:
   *
   *     index = floor(alpha * (B + 1)) - 1;
   *
   * where B is the number of bootstrap replicates. This method avoids interpolation and is
   * statistically justified in the BCa literature (Efron & Tibshirani) and aligns with
   * Hyndman & Fan's Type 6 quantile estimator.
   *
   * The lower and upper bounds are clipped to ensure they fall within the range [0, B - 1].
   * This method improves accuracy, especially for confidence bounds near the tails.
   *
   * Usage example:
   *
   *     std::vector<Decimal> returns = {...};
   *     BCaBootStrap<Decimal> bca(returns, 2000, 0.95);
   *     Decimal lower = bca.getLowerBound();
   *     Decimal upper = bca.getUpperBound();
   *
   * @tparam Decimal A high-precision decimal type with arithmetic support and getAsDouble().
   */
  template <class Decimal>
  class BCaBootStrap {
  public:
    using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

    /**
     * @brief Constructs a BCaBootStrap that uses the arithmetic mean as the statistic (backward compatible).
     *
     * @param returns            Original sample.
     * @param num_resamples      Number of bootstrap samples to generate (e.g., 2000).
     * @param confidence_level   Desired confidence level (e.g., 0.95).
     */
    BCaBootStrap(const std::vector<Decimal>& returns,
                 unsigned int num_resamples,
                 double confidence_level = 0.95)
      : m_returns(returns),
        m_num_resamples(num_resamples),
        m_confidence_level(confidence_level),
        m_statistic(&mkc_timeseries::StatUtils<Decimal>::computeMean),
        m_is_calculated(false)
    {
        validateConstructorArgs();
    }

    /**
     * @brief Constructs a BCaBootStrap with a custom statistic function.
     *
     * @param returns            Original sample.
     * @param num_resamples      Number of bootstrap samples to generate (e.g., 2000).
     * @param confidence_level   Desired confidence level (e.g., 0.95).
     * @param statistic          A callable mapping vector<Decimal> -> Decimal.
     */
    BCaBootStrap(const std::vector<Decimal>& returns,
                 unsigned int num_resamples,
                 double confidence_level,
                 StatFn statistic)
      : m_returns(returns),
        m_num_resamples(num_resamples),
        m_confidence_level(confidence_level),
        m_statistic(std::move(statistic)),
        m_is_calculated(false)
    {
        if (!m_statistic)
	  throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");

        validateConstructorArgs();
    }

    virtual ~BCaBootStrap() = default;

    /**
     * @brief Gets the statistic computed on the original sample (θ̂). Kept as 'getMean' for backward compatibility.
     */
    Decimal getMean() const
    {
      ensureCalculated();
      return m_theta_hat;
    }

    /**
     * @brief Alias for getMean() for semantic clarity with generic statistic.
     */
    Decimal getStatistic() const
    {
      return getMean();
    }

    /**
     * @brief Gets the lower bound of the BCa confidence interval (in statistic units).
     */
    Decimal getLowerBound() const
    {
      ensureCalculated();
      return m_lower_bound;
    }

    /**
     * @brief Gets the upper bound of the BCa confidence interval (in statistic units).
     */
    Decimal getUpperBound() const
    {
      ensureCalculated();
      return m_upper_bound;
    }

  protected:
    // Member variables
    const std::vector<Decimal>& m_returns;
    unsigned int m_num_resamples;
    double m_confidence_level;
    StatFn m_statistic;
    bool m_is_calculated;

    // Storage for results
    Decimal m_theta_hat;     // θ̂ on original sample
    Decimal m_lower_bound;   // lower BCa bound of θ
    Decimal m_upper_bound;   // upper BCa bound of θ

    // Setters for mock class usage in testing
    void setStatistic(const Decimal& theta) { m_theta_hat = theta; }
    void setMean(const Decimal& theta)      { m_theta_hat = theta; } // backward-compat setter
    void setLowerBound(const Decimal& lower) { m_lower_bound = lower; }
    void setUpperBound(const Decimal& upper) { m_upper_bound = upper; }

    void validateConstructorArgs() const
    {
      if (m_returns.empty())
	throw std::invalid_argument("BCaBootStrap: input returns vector cannot be empty.");

      if (m_num_resamples < 100)
            throw std::invalid_argument("BCaBootStrap: number of resamples should be at least 100.");

      if (m_confidence_level <= 0.0 || m_confidence_level >= 1.0)
	throw std::invalid_argument("BCaBootStrap: confidence level must be between 0 and 1.");
    }

    /**
     * @brief Ensures calculations are performed if not already done (lazy initialization).
     */
    void ensureCalculated() const
    {
      if (!m_is_calculated)
	// Cast away const to allow lazy initialization
	const_cast<BCaBootStrap*>(this)->calculateBCaBounds();
    }

    /**
     * @brief Core BCa algorithm with generic statistic.
     *
     * Steps:
     *  1) θ̂ = statistic(returns)
     *  2) Bootstrap replicates θ̂*
     *  3) z0 from proportion of θ̂* < θ̂
     *  4) a from jackknife leave-one-out recomputation of statistic
     *  5) Adjusted percentiles → indices → bounds
     */
    virtual void calculateBCaBounds() {
        if (m_is_calculated) return;

        const size_t n = m_returns.size();
        if (n < 2) {
            throw std::invalid_argument("BCa bootstrap requires at least 2 data points.");
        }

        // 1) Original statistic
        m_theta_hat = m_statistic(m_returns);

        // 2) Bootstrap replicates of the statistic
        std::vector<Decimal> boot_stats;
        boot_stats.reserve(m_num_resamples);

        thread_local static randutils::mt19937_rng rng;
        for (unsigned int b = 0; b < m_num_resamples; ++b) {
            std::vector<Decimal> resample;
            resample.reserve(n);
            for (size_t j = 0; j < n; ++j) {
                size_t idx = rng.uniform(size_t(0), n - 1);
                resample.push_back(m_returns[idx]);
            }
            boot_stats.push_back(m_statistic(resample));
        }
        std::sort(boot_stats.begin(), boot_stats.end());

        // 3) Bias-correction z0
        const auto count_less = std::count_if(
            boot_stats.begin(), boot_stats.end(),
            [this](const Decimal& v){ return v < this->m_theta_hat; });
        const double prop_less = static_cast<double>(count_less) / static_cast<double>(m_num_resamples);
        const double z0 = inverseNormalCdf(prop_less);

        // 4) Acceleration a via jackknife (recompute statistic on leave-one-out)
        std::vector<Decimal> jk_stats;
        jk_stats.reserve(n);
        Decimal jk_sum(DecimalConstants<Decimal>::DecimalZero);

        // Pre-allocate and reuse a buffer to avoid O(n^2) allocations (still O(n^2) computes)
        std::vector<Decimal> loov;
        loov.reserve(n - 1);

        for (size_t i = 0; i < n; ++i) {
            loov.clear();
            loov.insert(loov.end(), m_returns.begin(), m_returns.begin() + i);
            loov.insert(loov.end(), m_returns.begin() + i + 1, m_returns.end());
            const Decimal th = m_statistic(loov);
            jk_stats.push_back(th);
            jk_sum += th;
        }

        const Decimal jk_avg = jk_sum / Decimal(n);
        Decimal num(DecimalConstants<Decimal>::DecimalZero);
        Decimal den(DecimalConstants<Decimal>::DecimalZero);
        for (const auto& th : jk_stats) {
            const Decimal d = jk_avg - th;
            num += d * d * d;
            den += d * d;
        }

        Decimal a(DecimalConstants<Decimal>::DecimalZero);
        if (den > DecimalConstants<Decimal>::DecimalZero) {
            const double den15 = std::pow(num::to_double(den), 1.5);
            if (den15 > 0.0) {
                a = num / (Decimal(6) * Decimal(den15));
            }
        }

        // 5) Adjusted alpha levels and bounds
        const double alpha = (1.0 - m_confidence_level) / 2.0;
        const double z_alpha_lo = inverseNormalCdf(alpha);
        const double z_alpha_hi = inverseNormalCdf(1.0 - alpha);

        // BCa transform: alpha* = Phi( z0 + (z + z0) / (1 - a (z + z0)) )
        const double a_d = a.getAsDouble();

        const double t1 = z0 + z_alpha_lo;
        const double alpha1 = standardNormalCdf(z0 + t1 / (1.0 - a_d * t1));

        const double t2 = z0 + z_alpha_hi;
        const double alpha2 = standardNormalCdf(z0 + t2 / (1.0 - a_d * t2));

        // Unbiased order-statistic index: floor(p * (B + 1)) - 1, clipped to [0, B-1]
        const int lower_idx = unbiasedIndex(alpha1, m_num_resamples);
        const int upper_idx = unbiasedIndex(alpha2, m_num_resamples);

        m_lower_bound = boot_stats[lower_idx];
        m_upper_bound = boot_stats[upper_idx];

        m_is_calculated = true;
    }

    /**
     * @brief Standard Normal Cumulative Distribution Function (CDF).
     */
    double standardNormalCdf(double x) const
    {
      return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
    }

    /**
     * @brief Inverse of the Standard Normal CDF (Quantile function).
     */
    double inverseNormalCdf(double p) const
    {
      if (p <= 0.0)
	return -std::numeric_limits<double>::infinity();
      if (p >= 1.0)
	return  std::numeric_limits<double>::infinity();

      // use a helper on (0, 0.5], then reflect
      return (p < 0.5) ? -inverseNormalCdfHelper(p) : inverseNormalCdfHelper(1.0 - p);
    }

    double inverseNormalCdfHelper(double p) const
    {
      // Abramowitz and Stegun formula 26.2.23 (approximation)
      static const double c[] = {2.515517, 0.802853, 0.010328};
      static const double d[] = {1.432788, 0.189269, 0.001308};

      const double t = std::sqrt(-2.0 * std::log(p));
      const double num = (c[0] + c[1] * t + c[2] * t * t);
      const double den = (1.0 + d[0] * t + d[1] * t * t + d[2] * t * t * t);
      return t - num / den;
    }

    static int unbiasedIndex(double p, unsigned int B)
    {
      // floor(p * (B + 1)) - 1, then clip to [0, B-1]
      int idx = static_cast<int>(std::floor(p * (static_cast<double>(B) + 1.0))) - 1;
      if (idx < 0)
	idx = 0;

      const int maxIdx = static_cast<int>(B) - 1;
      if (idx > maxIdx)
	idx = maxIdx;

      return idx;
    }
  };

  /**
   * @brief Calculates the number of bars in a year for a given time frame.
   */
  inline double calculateAnnualizationFactor(TimeFrame::Duration timeFrame,
					     unsigned int intraday_minutes_per_bar = 0,
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
	  if (intraday_minutes_per_bar == 0) {
	    throw std::invalid_argument("For INTRADAY timeframe, intraday_minutes_per_bar must be specified.");
	  }
	  double bars_per_hour = 60.0 / intraday_minutes_per_bar;
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
   * @class BCaAnnualizer
   * @brief Takes BCa bootstrap results and annualizes them by geometric compounding.
   *
   * Note: This assumes the statistic is a per-period return compatible with compounding:
   * Annualized = (1 + θ)^{k} - 1, where k is the annualization factor.
   */
  template <class Decimal>
  class BCaAnnualizer
  {
  public:
    BCaAnnualizer(const BCaBootStrap<Decimal>& bca_results, double annualization_factor)
    {
      if (annualization_factor <= 0.0)
	throw std::invalid_argument("Annualization factor must be positive.");

      const Decimal one = DecimalConstants<Decimal>::DecimalOne;
      const double mean_d   = (one + bca_results.getMean()).getAsDouble();
      const double lower_d  = (one + bca_results.getLowerBound() ).getAsDouble();
      const double upper_d  = (one + bca_results.getUpperBound() ).getAsDouble();
      
      m_annualized_mean        = Decimal(std::pow(mean_d,  annualization_factor)) - one;
      m_annualized_lower_bound = Decimal(std::pow(lower_d, annualization_factor)) - one;
      m_annualized_upper_bound = Decimal(std::pow(upper_d, annualization_factor)) - one;
    }

    Decimal getAnnualizedMean() const
    {
      return m_annualized_mean;
    }
    
    Decimal getAnnualizedLowerBound() const
    {
      return m_annualized_lower_bound;
    }

    Decimal getAnnualizedUpperBound() const
    {
      return m_annualized_upper_bound;
    }

  private:
    Decimal m_annualized_mean;
    Decimal m_annualized_lower_bound;
    Decimal m_annualized_upper_bound;
  };
  
} // namespace mkc_timeseries

#endif // __BCA_BOOTSTRAP_H
