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
       * @brief Computes the standard normal cumulative distribution function.
       *
       * Calculates Φ(z) = P(Z ≤ z) where Z ~ N(0,1).
       *
       * Uses the error function (erf) for accurate computation:
       * Φ(z) = 0.5 * (1 + erf(z / √2))
       *
       * The error function provides excellent accuracy (typically ~1e-15)
       * across the full range of z values.
       *
       * @param z The value at which to evaluate the CDF.
       * @return The cumulative probability P(Z ≤ z), always in [0, 1].
       *
       * @note This function is noexcept and always returns a valid probability.
       *
       * @example
       * double p = compute_normal_cdf(1.96);   // Returns ~0.975
       * double p = compute_normal_cdf(-1.96);  // Returns ~0.025
       * double p = compute_normal_cdf(0.0);    // Returns exactly 0.5
       */
      inline double compute_normal_cdf(double z) noexcept
      {
        constexpr double INV_SQRT2 = 0.7071067811865475244; // 1/sqrt(2)
        return 0.5 * (1.0 + std::erf(z * INV_SQRT2));
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

      /**
       * @brief Computes the empirical cumulative distribution function (ECDF) at a point.
       *
       * The empirical CDF at value x is defined as:
       *   F_n(x) = (# of samples ≤ x) / n
       *
       * This is a non-parametric estimate of the cumulative distribution function
       * from sample data. It is commonly used in bootstrap methods, goodness-of-fit
       * tests (e.g., Kolmogorov-Smirnov), and quantile estimation.
       *
       * Properties:
       * - Returns values in [0, 1] of type ValueType
       * - Non-decreasing function of x
       * - Right-continuous (includes ties at x)
       * - Returns ValueType(0) for empty containers
       *
       * @tparam Container A container type supporting iteration (e.g., std::vector,
       *                   std::array, std::list). Elements must be comparable with
       *                   the query value using operator<=, support operator+ and
       *                   division by std::size_t.
       *
       * @param data The sample data container. Can be unsorted.
       * @param x The value at which to evaluate the ECDF. Must be comparable with
       *          container elements using operator<=.
       *
       * @return The proportion of values in data that are less than or equal to x,
       *         returned as the same type as the container elements.
       *         Returns ValueType(0) if data is empty.
       *
       * @note Time complexity: O(n) where n is the size of the data container.
       * @note This implementation does not require the data to be sorted.
       * @note Works with any numeric type that supports comparison, addition, and
       *       division by size_t, including double, float, int, and custom types
       *       like dec::decimal<N>.
       *
       * @example
       * // Basic usage with doubles
       * std::vector<double> samples = {1.5, 2.3, 1.8, 3.1, 2.0};
       * double F_2 = compute_empirical_cdf(samples, 2.0);  // Returns 0.6 (3 out of 5)
       *
       * @example
       * // Bootstrap bias correction (computing z0)
       * std::vector<double> bootstrap_stats = {...};
       * double theta_hat = 10.5;
       * double prop = compute_empirical_cdf(bootstrap_stats, theta_hat);
       * double z0 = compute_normal_quantile(prop);
       *
       * @example
       * // Works with integers
       * std::vector<int> int_data = {1, 5, 3, 8, 2, 5};
       * int F_5 = compute_empirical_cdf(int_data, 5);  // Returns 0 (integer division: 4/6)
       *
       * @example
       * // Works with floats
       * std::vector<float> float_data = {1.0f, 2.0f, 3.0f};
       * float F = compute_empirical_cdf(float_data, 2.5f);  // Returns 0.666...f (2 out of 3)
       *
       * @example
       * // Works with decimal types
       * using dec::decimal;
       * std::vector<decimal<2>> prices = {decimal<2>(100), decimal<2>(150), decimal<2>(200)};
       * decimal<2> F = compute_empirical_cdf(prices, decimal<2>(150)); // Returns 0.66 (2 out of 3)
       *
       * @see compute_normal_quantile for converting ECDF values to z-scores
       * @see https://en.wikipedia.org/wiki/Empirical_distribution_function
       */
      template <typename Container, typename QueryType>
      inline auto compute_empirical_cdf(const Container& data, const QueryType& x)
        -> typename Container::value_type
      {
        using ValueType = typename Container::value_type;
        
        if (data.empty())
        {
          return ValueType(0);
        }

        std::size_t count = 0;
        for (const auto& value : data)
        {
          if (value <= x)
          {
            ++count;
          }
        }

        // Create numerator and denominator as ValueType to maintain precision
        ValueType numerator(static_cast<int>(count));
        ValueType denominator(static_cast<int>(data.size()));
        
        return numerator / denominator;
      }
    } // namespace detail
  } // namespace analysis
} // namespace palvalidator
