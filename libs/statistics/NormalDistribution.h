// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//
// Normal Distribution Utility Functions
// Provides standard normal CDF and inverse CDF (quantile) functions

#pragma once

#include <cmath>
#include <limits>

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
     */
    static inline double standardNormalCdf(double x) noexcept
    {
      constexpr double INV_SQRT2 = 1.0 / 1.4142135623730950488; // 1/sqrt(2)
      return 0.5 * (1.0 + std::erf(x * INV_SQRT2));
    }

    /**
     * @brief Computes the inverse of the standard normal CDF (quantile function).
     *
     * Calculates Φ⁻¹(p) = x such that P(Z ≤ x) = p where Z ~ N(0,1).
     *
     * This is also known as the probit function or normal quantile function.
     * It is used to find critical values for confidence intervals and hypothesis tests.
     *
     * Implementation uses the Abramowitz & Stegun approximation (26.2.23) with
     * coefficients optimized for double precision, providing high accuracy for
     * p in the range (0, 1).
     *
     * @param p The probability value, must be in (0, 1).
     * @return The quantile value x such that Φ(x) = p.
     *         Returns -∞ if p ≤ 0, +∞ if p ≥ 1.
     */
    static inline double inverseNormalCdf(double p) noexcept
    {
      if (p <= 0.0) return -std::numeric_limits<double>::infinity();
      if (p >= 1.0) return  std::numeric_limits<double>::infinity();
      return (p < 0.5) ? -inverseNormalCdfHelper(p) : inverseNormalCdfHelper(1.0 - p);
    }

  private:
    /**
     * @brief Helper function for inverse normal CDF computation.
     *
     * Implements the Abramowitz & Stegun approximation (26.2.23) for computing
     * the inverse normal CDF for probabilities in the range (0, 0.5].
     *
     * The approximation uses rational function coefficients optimized for
     * double precision accuracy:
     * - Numerator: c₀ + c₁t + c₂t²
     * - Denominator: 1 + d₀t + d₁t² + d₂t³
     * where t = √(-2 ln(p))
     *
     * Reference: Abramowitz, M., & Stegun, I. A. (1964). Handbook of
     * Mathematical Functions. Section 26.2.23.
     *
     * @param p The probability value, should be in (0, 0.5] for best accuracy.
     * @return The quantile value for the given probability.
     */
    static inline double inverseNormalCdfHelper(double p) noexcept
    {
      // W. J. Cody / Abramowitz & Stegun 26.2.23 (Coefficients optimized for double precision)
      // These coefficients provide high accuracy for p in (0, 0.5]
      constexpr double c0 = 2.5155173462;
      constexpr double c1 = 0.8028530777;
      constexpr double c2 = 0.0103284795;
      constexpr double d0 = 1.4327881989;
      constexpr double d1 = 0.1892692257;
      constexpr double d2 = 0.0013083321;

      const double t   = std::sqrt(-2.0 * std::log(p));
      const double num = (c0 + c1 * t + c2 * t * t);
      const double den = (1.0 + d0 * t + d1 * t * t + d2 * t * t * t);
      return t - num / den;
    }
  };

} // namespace mkc_timeseries
