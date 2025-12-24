#pragma once

#include <cmath>
#include <stdexcept>
#include <limits>

namespace palvalidator
{
  namespace analysis
  {
    namespace detail
    {
      /**
       * @brief Computes the quantile (inverse CDF) of the standard normal distribution.
       *
       * This function implements Peter Acklam's algorithm, which provides
       * excellent accuracy across the full range of probabilities. The algorithm
       * uses rational approximations with different coefficients for different regions.
       *
       * Accuracy:
       * - Relative error < 1.15e-9 for all p in (2.23e-308, 1 - 2.23e-308)
       * - Maximum absolute error < 1.15e-9
       *
       * Algorithm Details:
       * - Central region [0.02425, 0.97575]: One rational approximation
       * - Tail regions: Different rational approximation
       *
       * @param p Probability in (0, 1). The cumulative probability.
       * @return double The z-score such that Φ(z) = p, where Φ is the standard normal CDF
       *
       * @throws std::domain_error if p <= 0 or p >= 1
       *
       * @note For p = 0.5, returns exactly 0.0 (median)
       *
       * @see Acklam, P.J. (2010). "An algorithm for computing the inverse normal
       *      cumulative distribution function." Available at:
       *      https://web.archive.org/web/20151030215612/http://home.online.no/~pjacklam/notes/invnorm/
       *
       * @example
       * double z_975 = compute_normal_quantile(0.975);  // Returns ~1.96
       * double z_025 = compute_normal_quantile(0.025);  // Returns ~-1.96
       */
      inline double compute_normal_quantile(double p)
      {
        // Validate input
        if (p <= 0.0 || p >= 1.0)
        {
          throw std::domain_error(
            "compute_normal_quantile: probability p must be in (0, 1)");
        }

        // Handle median exactly
        if (p == 0.5)
        {
          return 0.0;
        }

        // Acklam's algorithm coefficients
        // Coefficients in rational approximations for central region
        static constexpr double a1 = -3.969683028665376e+01;
        static constexpr double a2 =  2.209460984245205e+02;
        static constexpr double a3 = -2.759285104469687e+02;
        static constexpr double a4 =  1.383577518672690e+02;
        static constexpr double a5 = -3.066479806614716e+01;
        static constexpr double a6 =  2.506628277459239e+00;

        static constexpr double b1 = -5.447609879822406e+01;
        static constexpr double b2 =  1.615858368580409e+02;
        static constexpr double b3 = -1.556989798598866e+02;
        static constexpr double b4 =  6.680131188771972e+01;
        static constexpr double b5 = -1.328068155288572e+01;

        // Coefficients in rational approximations for tail regions
        static constexpr double c1 = -7.784894002430226e-03;
        static constexpr double c2 = -3.223964580411365e-01;
        static constexpr double c3 = -2.400758277161838e+00;
        static constexpr double c4 = -2.549732539343734e+00;
        static constexpr double c5 =  4.374664141464968e+00;
        static constexpr double c6 =  2.938163982698783e+00;

        static constexpr double d1 =  7.784695709041462e-03;
        static constexpr double d2 =  3.224671290700398e-01;
        static constexpr double d3 =  2.445134137142996e+00;
        static constexpr double d4 =  3.754408661907416e+00;

        // Define break-points
        static constexpr double p_low  = 0.02425;
        static constexpr double p_high = 1.0 - p_low;

        double q, r, result;

        if (p < p_low)
        {
          // Rational approximation for lower tail
          q = std::sqrt(-2.0 * std::log(p));
          result = (((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6) /
                   ((((d1 * q + d2) * q + d3) * q + d4) * q + 1.0);
        }
        else if (p <= p_high)
        {
          // Rational approximation for central region
          q = p - 0.5;
          r = q * q;
          result = (((((a1 * r + a2) * r + a3) * r + a4) * r + a5) * r + a6) * q /
                   (((((b1 * r + b2) * r + b3) * r + b4) * r + b5) * r + 1.0);
        }
        else
        {
          // Rational approximation for upper tail
          q = std::sqrt(-2.0 * std::log(1.0 - p));
          result = -(((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6) /
                    ((((d1 * q + d2) * q + d3) * q + d4) * q + 1.0);
        }

        return result;
      }

      /**
       * @brief Computes the critical value for a two-tailed confidence interval.
       *
       * This is a convenience function that computes the z-value for a symmetric
       * confidence interval. For a confidence level CL, it returns the z such that
       * P(-z < Z < z) = CL, where Z ~ N(0,1).
       *
       * @param confidence_level The confidence level in (0, 1), e.g., 0.95 for 95% CI
       * @return double The critical z-value (always positive)
       *
       * @throws std::domain_error if confidence_level is not in (0, 1)
       *
       * @example
       * double z_95 = compute_normal_critical_value(0.95);  // Returns ~1.96
       * double z_99 = compute_normal_critical_value(0.99);  // Returns ~2.576
       */
      inline double compute_normal_critical_value(double confidence_level)
      {
        if (confidence_level <= 0.0 || confidence_level >= 1.0)
        {
          throw std::domain_error(
            "compute_normal_critical_value: confidence_level must be in (0, 1)");
        }

        const double alpha = 1.0 - confidence_level;
        return compute_normal_quantile(1.0 - alpha / 2.0);
      }

    } // namespace detail
  } // namespace analysis
} // namespace palvalidator
