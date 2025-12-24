// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//
// Normal Distribution Utility Functions
// Provides standard normal CDF and inverse CDF (quantile) functions
//
// IMPLEMENTATION NOTE (Updated 2024):
// This implementation now delegates to the high-precision Acklam algorithm
// (NormalQuantile.h) for improved accuracy while maintaining the original
// interface for backward compatibility.

#pragma once

#include <cmath>
#include <limits>
#include "NormalQuantile.h"

namespace mkc_timeseries
{
  /**
   * @struct NormalDistribution
   * @brief Utility functions for the standard normal distribution.
   *
   * This struct provides static methods for computing the cumulative distribution
   * function (CDF) and its inverse (quantile function) for the standard normal
   * distribution N(0,1).
   *
   * These functions are used throughout the statistics library for bootstrap
   * confidence interval calculations and hypothesis testing.
   *
   * IMPLEMENTATION: These functions now wrap the high-precision Acklam algorithm
   * from NormalQuantile.h, providing ~1e-9 accuracy (100-1000× better than the
   * previous Abramowitz & Stegun implementation) while maintaining backward
   * compatibility with existing code.
   */
  struct NormalDistribution
  {
    /**
     * @brief Computes the standard normal cumulative distribution function.
     *
     * Calculates Φ(x) = P(Z ≤ x) where Z ~ N(0,1).
     *
     * Uses the error function (erf) for accurate computation:
     * Φ(x) = 0.5 * (1 + erf(x / √2))
     *
     * @param x The value at which to evaluate the CDF.
     * @return The cumulative probability P(Z ≤ x).
     *
     * @note This function uses std::erf which provides excellent accuracy
     *       (typically ~1e-15) across the full range of x values.
     */
    static inline double standardNormalCdf(double x) noexcept
    {
      return palvalidator::analysis::detail::compute_normal_cdf(x);
    }

    /**
     * @brief Computes the inverse of the standard normal CDF (quantile function).
     *
     * Calculates Φ⁻¹(p) = x such that P(Z ≤ x) = p where Z ~ N(0,1).
     *
     * This is also known as the probit function or normal quantile function.
     * It is used to find critical values for confidence intervals and hypothesis tests.
     *
     * IMPLEMENTATION: Uses Peter Acklam's algorithm (2010) which provides
     * excellent accuracy with relative error < 1.15e-9 across the full range
     * of probabilities. This is approximately 100-1000× more accurate than
     * the previous Abramowitz & Stegun implementation.
     *
     * The function is noexcept and returns ±∞ for boundary cases rather than
     * throwing exceptions, maintaining compatibility with legacy code that
     * expects this behavior.
     *
     * @param p The probability value, should be in (0, 1).
     * @return The quantile value x such that Φ(x) = p.
     *         Returns -∞ if p ≤ 0, +∞ if p ≥ 1.
     *
     * @see palvalidator::analysis::detail::compute_normal_quantile for the
     *      underlying high-precision implementation.
     */
    static inline double inverseNormalCdf(double p) noexcept
    {
      // Handle boundary cases that would cause exceptions in compute_normal_quantile
      if (p <= 0.0) return -std::numeric_limits<double>::infinity();
      if (p >= 1.0) return  std::numeric_limits<double>::infinity();

      // For valid probabilities, use the high-precision Acklam algorithm
      // wrapped in try-catch for safety (though it should never throw given
      // the boundary checks above)
      try {
        return palvalidator::analysis::detail::compute_normal_quantile(p);
      }
      catch (...) {
        // This should never happen, but provide safe fallback
        // Return extreme value in the appropriate direction
        return (p < 0.5) ? -std::numeric_limits<double>::infinity()
                         :  std::numeric_limits<double>::infinity();
      }
    }

    /**
     * @brief Computes the critical value for a two-tailed confidence interval.
     *
     * This is a convenience function that computes the z-value for a symmetric
     * confidence interval. For a confidence level CL, it returns the z such that
     * P(-z < Z < z) = CL, where Z ~ N(0,1).
     *
     * Equivalent to: inverseNormalCdf(1 - (1-CL)/2)
     *
     * @param confidence_level The confidence level in (0, 1), e.g., 0.95 for 95% CI
     * @return double The critical z-value (always positive).
     *         Returns +∞ if confidence_level is not in (0, 1).
     *
     * @note This function is noexcept and handles invalid inputs by returning
     *       infinity rather than throwing exceptions.
     *
     * @example
     * double z_95 = criticalValue(0.95);  // Returns ~1.96
     * double z_99 = criticalValue(0.99);  // Returns ~2.576
     */
    static inline double criticalValue(double confidence_level) noexcept
    {
      // Validate confidence level
      if (confidence_level <= 0.0 || confidence_level >= 1.0) {
        return std::numeric_limits<double>::infinity();
      }

      // Compute upper tail probability
      const double alpha = 1.0 - confidence_level;
      const double p_upper = 1.0 - alpha / 2.0;

      // Use inverseNormalCdf which is already noexcept
      return inverseNormalCdf(p_upper);
    }

  private:
    /**
     * @brief Legacy helper function - no longer used.
     *
     * This function previously implemented the Abramowitz & Stegun approximation
     * but is now deprecated. The implementation has been replaced with delegation
     * to the high-precision Acklam algorithm.
     *
     * Kept for potential compatibility with derived classes, but should not be
     * called directly.
     *
     * @deprecated Use inverseNormalCdf instead, which now uses Acklam's algorithm.
     */
    [[deprecated("Use inverseNormalCdf instead - now uses high-precision Acklam algorithm")]]
    static inline double inverseNormalCdfHelper(double p) noexcept
    {
      // Delegate to the new implementation
      return inverseNormalCdf(p);
    }
  };

} // namespace mkc_timeseries
