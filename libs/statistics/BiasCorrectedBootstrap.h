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
#include "DecimalConstants.h"
#include "number.h"
#include "randutils.hpp" // For high-quality random number generation
#include "TimeFrame.h"

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
        
        // Don't call virtual function from constructor - use lazy initialization instead
    }
    
    virtual ~BCaBootStrap() = default;

    /**
     * @brief Gets the mean of the original sample of returns.
     * @return The calculated mean.
     */
    Decimal getMean() const {
        ensureCalculated();
        return m_mean;
    }

    /**
     * @brief Gets the lower bound of the BCaBootStrap confidence interval.
     * @return The lower confidence bound.
     */
    Decimal getLowerBound() const {
        ensureCalculated();
        return m_lower_bound;
    }

    /**
     * @brief Gets the upper bound of the BCaBootStrap confidence interval.
     * @return The upper confidence bound.
     */
    Decimal getUpperBound() const {
        ensureCalculated();
        return m_upper_bound;
    }

  protected:
    // Member variables
    const std::vector<Decimal>& m_returns;
    unsigned int m_num_resamples;
    double m_confidence_level;
    bool m_is_calculated;

    // Storage for results
    Decimal m_mean;
    Decimal m_lower_bound;
    Decimal m_upper_bound;

    // Setters for mock class usage in testing
    void setMean(const Decimal& mean) { m_mean = mean; }
    void setLowerBound(const Decimal& lower) { m_lower_bound = lower; }
    void setUpperBound(const Decimal& upper) { m_upper_bound = upper; }

    /**
     * @brief Ensures calculations are performed if not already done (lazy initialization).
     */
    void ensureCalculated() const {
        if (!m_is_calculated) {
            // Cast away const to allow lazy initialization
            const_cast<BCaBootStrap*>(this)->calculateBCaBounds();
        }
    }

    
    /**
 * @brief Performs the full Bias-Corrected and Accelerated (BCa) bootstrap procedure.
 *
 * This method is the core of the BCa algorithm. It:
 * 1. Computes the mean of the original data sample.
 * 2. Generates bootstrap replicates of the mean using resampling with replacement.
 * 3. Sorts the bootstrap replicates and computes the bias correction factor z0.
 * 4. Computes the acceleration factor a using jackknife resampling.
 * 5. Applies the BCa adjusted percentile transformation to determine the adjusted alpha levels.
 * 6. Selects the lower and upper confidence bounds using the Type 6 quantile estimator:
 *        index = floor(alpha * (B + 1)) - 1
 *    where alpha is the adjusted percentile and B is the number of bootstrap samples.
 *
 * The use of (B + 1) instead of B is based on the unbiased quantile estimator recommended by
 * Efron & Tibshirani (1993) and Hyndman & Fan (1996). This adjustment provides better
 * performance for small to moderate bootstrap sample sizes, particularly in the tails.
 *
 * Results are lazily computed and cached. This function is called on-demand by public getters.
 *
 * @throws std::invalid_argument if the sample size is less than 2.
 */
    virtual void calculateBCaBounds() {
        if (m_is_calculated) return;

        const size_t n = m_returns.size();

        // Add a guardrail: BCa is undefined for n < 2
        if (n < 2) {
            throw std::invalid_argument("BCa bootstrap requires at least 2 data points.");
        }

        m_mean = calculateMean(m_returns);

        // --- 1. Generate Bootstrap Replicates of the Mean ---
        std::vector<Decimal> bootstrap_means;
        bootstrap_means.reserve(m_num_resamples);
        
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
        // This measures the median bias of the bootstrap distribution. It is calculated as the
        // inverse normal CDF of the proportion of bootstrap means that are less than the original sample mean.
        // A value of zero indicates no median bias.
        long count_less = std::count_if(bootstrap_means.begin(), bootstrap_means.end(), 
                                      [this](const Decimal& val) { return val < this->m_mean; });
        
        double proportion_less = static_cast<double>(count_less) / m_num_resamples;
        double z0 = inverseNormalCdf(proportion_less);

        // --- 3. Calculate Acceleration Factor (a) using Jackknife ---
        // This measures the skewness of the bootstrap distribution by calculating how the
        // sample mean changes as each individual data point is omitted (jackknife resampling).
        std::vector<Decimal> jackknife_means;
        jackknife_means.reserve(n);
        Decimal jackknife_sum(0);

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
        
        Decimal a(0);
        if (denominator > Decimal(0)) {
            Decimal denom_pow = Decimal(std::pow(denominator.getAsDouble(), 1.5));
            if (denom_pow > Decimal(0)) {
                a = numerator / (Decimal(6) * denom_pow);
            }
        }
        
        // --- 4. Calculate Adjusted Alpha Levels ---
        // Using the bias (z0) and acceleration (a) factors, we adjust the standard normal
        // quantiles (e.g., -1.96 and +1.96 for a 95% CI) to find the corrected
        // percentile points (alpha1 and alpha2) in our bootstrap distribution.
        double alpha = (1.0 - m_confidence_level) / 2.0;
        double z_alpha1 = inverseNormalCdf(alpha);
        double z_alpha2 = inverseNormalCdf(1.0 - alpha);

        double term1 = z0 + z_alpha1;
        double alpha1_numerator = z0 + term1 / (1.0 - a.getAsDouble() * term1);
        double alpha1 = standardNormalCdf(alpha1_numerator);
        
        double term2 = z0 + z_alpha2;
        double alpha2_numerator = z0 + term2 / (1.0 - a.getAsDouble() * term2);
        double alpha2 = standardNormalCdf(alpha2_numerator);
        
        // --- 5. Determine Confidence Interval from Bootstrap Distribution ---
        // The final bounds are the values from the sorted list of bootstrap means
        // located at the adjusted percentiles (quantiles) alpha1 and alpha2.
        // The formula used here, p * (N+1), is a standard and well-regarded method
        // for calculating sample quantiles from a finite number of samples (N).
        // It provides better statistical properties than simpler methods like p*N.
        //
        // The calculation is as follows:
        //  - p: The adjusted percentile, e.g., alpha1.
        //  - N: The number of resamples, m_num_resamples.
        //  - index = floor(p * (N+1)) - 1
        // The subtraction of 1 adjusts the result to a 0-based array index.
        int lower_idx = static_cast<int>(std::floor(alpha1 * (m_num_resamples + 1))) - 1;
        int upper_idx = static_cast<int>(std::floor(alpha2 * (m_num_resamples + 1))) - 1;

        // Boundary checks
        lower_idx = std::max(0, lower_idx);
        upper_idx = std::min(static_cast<int>(m_num_resamples - 1), std::max(0, upper_idx));

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
     */
    double standardNormalCdf(double x) const {
        return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
    }

    /**
     * @brief Inverse of the Standard Normal CDF (Quantile function).
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
   */
  template <class Decimal>
  class BCaAnnualizer
  {
  public:
    BCaAnnualizer(const BCaBootStrap<Decimal>& bca_results, double annualization_factor)
    {
        if (annualization_factor <= 0) {
            throw std::invalid_argument("Annualization factor must be positive.");
        }

        // Correctly convert Decimal to double for std::pow, then convert result back to Decimal
        m_annualized_mean = Decimal(std::pow((Decimal("1.0") + bca_results.getMean()).getAsDouble(), annualization_factor)) - Decimal("1.0");
        m_annualized_lower_bound = Decimal(std::pow((Decimal("1.0") + bca_results.getLowerBound()).getAsDouble(), annualization_factor)) - Decimal("1.0");
        m_annualized_upper_bound = Decimal(std::pow((Decimal("1.0") + bca_results.getUpperBound()).getAsDouble(), annualization_factor)) - Decimal("1.0");
    }

    Decimal getAnnualizedMean() const { return m_annualized_mean; }
    Decimal getAnnualizedLowerBound() const { return m_annualized_lower_bound; }
    Decimal getAnnualizedUpperBound() const { return m_annualized_upper_bound; }

  private:
    Decimal m_annualized_mean;
    Decimal m_annualized_lower_bound;
    Decimal m_annualized_upper_bound;
  };
} // namespace mkc_timeseries

#endif // __BCA_BOOTSTRAP_H
