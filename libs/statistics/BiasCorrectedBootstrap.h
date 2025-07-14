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
#include "DecimalConstants.h"
#include "number.h"
#include "randutils.hpp" // For high-quality random number generation
#include "TimeFrame.h"

namespace mkc_timeseries
{
  /**
   * @class BCaBootStrap
   * @brief Computes the mean and Bias-Corrected and Accelerated (BCa) bootstrap confidence intervals for a set of returns.
   *
   * This class implements the BCa bootstrap algorithm, which provides more accurate
   * confidence intervals than standard percentile methods by adjusting for both bias
   * and skewness in the bootstrap distribution.
   *
   * @tparam Decimal The numerical type used for returns (e.g., a high-precision decimal class).
   */
  template <class Decimal>
  class BCaBootStrap {
  public:
    /**
     * @brief Constructs the BCaBootStrap calculator.
     *
     * @param returns The original sample of returns.
     * @param num_resamples The number of bootstrap samples to generate (e.g., 2000 or more).
     * @param confidence_level The desired confidence level (e.g., 0.95 for a 95% CI).
     * @throws std::invalid_argument if returns are empty, resamples are too few, or confidence is invalid.
     */
    BCaBootStrap(const std::vector<Decimal>& returns,
                 unsigned int num_resamples,
                 double confidence_level = 0.95)
      : m_returns(returns),
        m_num_resamples(num_resamples),
        m_confidence_level(confidence_level),
        m_is_calculated(false)
    {
        if (m_returns.empty()) {
            throw std::invalid_argument("Input returns vector cannot be empty.");
        }

        if (m_num_resamples < 100) { // A reasonable minimum
            throw std::invalid_argument("Number of resamples should be at least 100.");
        }
        
        if (m_confidence_level <= 0.0 || m_confidence_level >= 1.0) {
            throw std::invalid_argument("Confidence level must be between 0.0 and 1.0.");
        }
        
        // Calculate the results upon construction
        calculateBCaBounds();
    }

    /**
     * @brief Gets the mean of the original sample of returns.
     * @return The calculated mean.
     */
    Decimal getMean() const {
        return m_mean;
    }

    /**
     * @brief Gets the lower bound of the BCaBootStrap confidence interval.
     * @return The lower confidence bound.
     */
    Decimal getLowerBound() const {
        return m_lower_bound;
    }

    /**
     * @brief Gets the upper bound of the BCaBootStrap confidence interval.
     * @return The upper confidence bound.
     */
    Decimal getUpperBound() const {
        return m_upper_bound;
    }

    /**
     * @brief Sets the mean (for testing purposes).
     * @param mean The mean value to set.
     */
    void setMean(const Decimal& mean) {
        m_mean = mean;
    }

    /**
     * @brief Sets the lower bound (for testing purposes).
     * @param lower_bound The lower bound value to set.
     */
    void setLowerBound(const Decimal& lower_bound) {
        m_lower_bound = lower_bound;
    }

    /**
     * @brief Sets the upper bound (for testing purposes).
     * @param upper_bound The upper bound value to set.
     */
    void setUpperBound(const Decimal& upper_bound) {
        m_upper_bound = upper_bound;
    }

  private:
    // Member variables
    const std::vector<Decimal>& m_returns;
    unsigned int m_num_resamples;
    double m_confidence_level;
    bool m_is_calculated;

    // Storage for results
    Decimal m_mean;
    Decimal m_lower_bound;
    Decimal m_upper_bound;

    /**
     * @brief Orchestrates the entire BCaBootStrap calculation.
     */
    virtual void calculateBCaBounds() {
        if (m_is_calculated) return;

        const size_t n = m_returns.size();
        m_mean = calculateMean(m_returns);

        // --- 1. Generate Bootstrap Replicates of the Mean ---
        std::vector<Decimal> bootstrap_means;
        bootstrap_means.reserve(m_num_resamples);
        
        // Use randutils for high-quality, auto-seeded random number generation.
        // Declared thread_local static for performance and to avoid re-seeding.
        thread_local static randutils::mt19937_rng rng;

        for (unsigned int i = 0; i < m_num_resamples; ++i) {
            std::vector<Decimal> resample;
            resample.reserve(n);
            for (size_t j = 0; j < n; ++j) {

                size_t index = rng.uniform(size_t(0), n - 1);
                resample.push_back(m_returns[index]);
            }
            bootstrap_means.push_back(calculateMean(resample));
        }
        std::sort(bootstrap_means.begin(), bootstrap_means.end());

        // --- 2. Calculate Bias-Correction Factor (z0) ---
        long count_less = 0;
        for(const auto& b_mean : bootstrap_means) {
            if (b_mean < m_mean) {
                count_less++;
            }
        }
        double proportion_less = static_cast<double>(count_less) / m_num_resamples;
        double z0 = inverseNormalCdf(proportion_less);

        // --- 3. Calculate Acceleration Factor (a) using Jackknife ---
        std::vector<Decimal> jackknife_means;
        jackknife_means.reserve(n);
        Decimal jackknife_sum = Decimal(0);

        for (size_t i = 0; i < n; ++i) {
            Decimal current_sum = m_mean * Decimal(n);
            Decimal jack_mean = (current_sum - m_returns[i]) / Decimal(n - 1);
            jackknife_means.push_back(jack_mean);
            jackknife_sum += jack_mean;
        }
        Decimal jackknife_mean_avg = jackknife_sum / Decimal(n);

        Decimal numerator(0), denominator(0);
        for (const auto& j_mean : jackknife_means) {
            Decimal diff = jackknife_mean_avg - j_mean;
            numerator += (diff * diff * diff);
            denominator += (diff * diff);
        }
        
        Decimal a = Decimal(0);
        if (denominator > Decimal(0)) {
            Decimal denom_pow = Decimal(std::pow(denominator.getAsDouble(), 1.5));
            if (denom_pow > Decimal(0)) {
                a = numerator / (Decimal(6) * denom_pow);
            }
        }
        
        // --- 4. Calculate Adjusted Alpha Levels ---
        double alpha = (1.0 - m_confidence_level) / 2.0;
        double z_alpha1 = inverseNormalCdf(alpha);
        double z_alpha2 = inverseNormalCdf(1.0 - alpha);

        // Formula for alpha1
        double term1 = z0 + z_alpha1;
        double alpha1_numerator = z0 + term1 / (1.0 - a.getAsDouble() * term1);
        double alpha1 = standardNormalCdf(alpha1_numerator);
        
        // Formula for alpha2
        double term2 = z0 + z_alpha2;
        double alpha2_numerator = z0 + term2 / (1.0 - a.getAsDouble() * term2);
        double alpha2 = standardNormalCdf(alpha2_numerator);
        
        // --- 5. Determine Confidence Interval from Bootstrap Distribution ---
        int lower_idx = static_cast<int>(std::floor(alpha1 * m_num_resamples));
        int upper_idx = static_cast<int>(std::ceil(alpha2 * m_num_resamples)) - 1; // -1 for 0-based index

        // Boundary checks
        lower_idx = std::max(0, lower_idx);
        upper_idx = std::min(static_cast<int>(m_num_resamples - 1), upper_idx);

        m_lower_bound = bootstrap_means[lower_idx];
        m_upper_bound = bootstrap_means[upper_idx];
        
        m_is_calculated = true;
    }
    
    /**
     * @brief Helper to calculate the mean of a vector of Decimals.
     */
    Decimal calculateMean(const std::vector<Decimal>& vec) const {
        if (vec.empty()) return Decimal(0);
        Decimal sum = std::accumulate(vec.begin(), vec.end(), Decimal(0));
        return sum / Decimal(vec.size());
    }

    /**
     * @brief Standard Normal Cumulative Distribution Function (CDF).
     * Uses the error function erf().
     */
    double standardNormalCdf(double x) const {
        return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
    }

    /**
     * @brief Inverse of the Standard Normal CDF (Quantile function).
     * Uses a rational approximation for high accuracy.
     */
    double inverseNormalCdf(double p) const {
        if (p <= 0.0) return -std::numeric_limits<double>::infinity();
        if (p >= 1.0) return std::numeric_limits<double>::infinity();

        if (p < 0.5) {
            return -inverseNormalCdfHelper(p);
        } else {
            return inverseNormalCdfHelper(1.0 - p);
        }
    }

    double inverseNormalCdfHelper(double p) const {
        // Abramowitz and Stegun formula 26.2.23
        double c[] = {2.515517, 0.802853, 0.010328};
        double d[] = {1.432788, 0.189269, 0.001308};
        double t = sqrt(log(1.0 / (p * p)));
        double numerator = c[0] + c[1] * t + c[2] * t * t;
        double denominator = 1.0 + d[0] * t + d[1] * t * t + d[2] * t * t * t;
        return t - numerator / denominator;
    }
  };

  /**
   * @brief Calculates the number of bars in a year for a given time frame.
   *
   * @param timeFrame The primary duration (e.g., DAILY, INTRADAY).
   * @param intraday_minutes_per_bar The number of minutes in each bar (only for INTRADAY).
   * @param trading_days_per_year The number of trading days in a year (typically 252).
   * @param trading_hours_per_day The number of trading hours in a day (e.g., 6.5 for US stocks, 24 for forex).
   * @return The annualization factor (N).
   */
  inline double calculateAnnualizationFactor(
      TimeFrame::Duration timeFrame,
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
   * @brief Takes BCa bootstrap results and annualizes them.
   * @tparam Decimal The high-precision decimal type.
   */
  template <class Decimal>
  class BCaAnnualizer
  {
  public:
    /**
     * @brief Constructs the annualizer and computes the annualized results.
     *
     * @param bca_results An instance of BCaBootStrap containing the per-bar results.
     * @param annualization_factor The number of bars in a year (N).
     */
    BCaAnnualizer(const BCaBootStrap<Decimal>& bca_results, double annualization_factor)
    {
        if (annualization_factor <= 0) {
            throw std::invalid_argument("Annualization factor must be positive.");
        }

        // Geometrically compound the mean, lower, and upper bounds.
        m_annualized_mean = Decimal(pow((Decimal("1.0") + bca_results.getMean()).getAsDouble(), annualization_factor)) - Decimal("1.0");
        m_annualized_lower_bound = Decimal(pow((Decimal("1.0") + bca_results.getLowerBound()).getAsDouble(), annualization_factor)) - Decimal("1.0");
        m_annualized_upper_bound = Decimal(pow((Decimal("1.0") + bca_results.getUpperBound()).getAsDouble(), annualization_factor)) - Decimal("1.0");
    }

    /**
     * @brief Gets the annualized mean return.
     */
    Decimal getAnnualizedMean() const
    {
        return m_annualized_mean;
    }

    /**
     * @brief Gets the annualized lower bound of the confidence interval.
     */
    Decimal getAnnualizedLowerBound() const
    {
        return m_annualized_lower_bound;
    }

    /**
     * @brief Gets the annualized upper bound of the confidence interval.
     */
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
