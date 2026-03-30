/**
 * @file BoundFutureReturns.h
 * @brief Conservative future-return bounds via BCa confidence intervals on empirical quantiles.
 *
 * Builds monthly returns from a ClosedPositionHistory (or accepts pre-computed
 * returns) and computes BCa bootstrap confidence intervals around user-specified
 * lower and upper quantiles for operational monitoring.
 *
 * Copyright (C) MKC Associates, LLC — All Rights Reserved.
 */

#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include "randutils.hpp"
#include "ClosedPositionHistory.h"
#include "BiasCorrectedBootstrap.h"
#include "MonthlyReturnsBuilder.h"

namespace mkc_timeseries
{
  /**
   * @brief Compute an empirical quantile on a Decimal vector using an order statistic.
   *
   * Uses the common m = floor(p * (n + 1)) clamped to [1, n], and returns x[m-1]
   * after nth_element. No interpolation is performed (matches the original BND_RET logic).
   */
  template <class Decimal>
  Decimal empiricalQuantile(std::vector<Decimal> x, double p)
  {
    if (x.empty())
      {
	throw std::invalid_argument("empiricalQuantile: empty sample");
      }

    if (p <= 0.0)
      {
	return *std::min_element(x.begin(), x.end());
      }
    if (p >= 1.0)
      {
	return *std::max_element(x.begin(), x.end());
      }

    const std::size_t n = x.size();
    const std::size_t m =
      std::max<std::size_t>(1, static_cast<std::size_t>(std::floor(p * (n + 1)))) - 1;

    std::nth_element(x.begin(), x.begin() + static_cast<std::ptrdiff_t>(m), x.end());
    return x[m];
  }

  /**
   * @brief Container for a quantile’s point estimate and its BCa CI.
   */
  template <class Decimal>
  struct QuantileCI
  {
    Decimal point;  ///< Quantile on original data
    Decimal lo;     ///< Lower BCa endpoint
    Decimal hi;     ///< Upper BCa endpoint
  };

  /**
   * @brief End-to-end helper: builds monthly returns from ClosedPositionHistory and
   *        produces conservative future-return bounds via BCa CIs on target quantiles.
   *
   * Policy (default, configurable by client via getters):
   *   - Lower monitoring bound  = lower CI endpoint of lower quantile (conservative)
   *   - Upper monitoring bound  = upper CI endpoint of upper quantile (conservative)
   *
   * @tparam Decimal    Fixed-precision decimal type (e.g., dec::decimal<8>).
   * @tparam Resampler  Block resampler for BCa; defaults to StationaryBlockResampler<Decimal>,
   *                    which suits monthly return series.
   * @tparam Rng        Random-number generator consumed by the BCa bootstrap
   *                    (e.g., randutils::mt19937_rng).
   */
  template <class Decimal,
	    class Resampler = StationaryBlockResampler<Decimal>,
	    class Rng       = randutils::mt19937_rng>
  class BoundFutureReturns
  {
  public:
    /**
     * @brief Construct from a ClosedPositionHistory, building monthly returns internally.
     *
     * Delegates to the vector-based constructor after converting closed positions
     * into a monthly-return series via buildMonthlyReturnsFromClosedPositions().
     *
     * @param closedPositions  ClosedPositionHistory with realized trades
     * @param blockLen         Stationary block length (default 3 months)
     * @param lowerQuantileP   Lower quantile p in (0, 0.5) (default 0.10)
     * @param upperQuantileP   Upper quantile p in (0.5, 1)  (default 0.90)
     * @param numBootstraps    Number of bootstrap replicates B (default 5000)
     * @param confLevel        Confidence level in (0,1) for BCa CI (default 0.95)
     * @param intervalType     BCa interval type: TWO_SIDED or ONE_SIDED (default TWO_SIDED).
     *
     * @throws std::invalid_argument If parameters are out of range or data is insufficient.
     */
    BoundFutureReturns(const ClosedPositionHistory<Decimal> &closedPositions,
                       unsigned blockLen        = 3,
                       double   lowerQuantileP  = 0.10,
                       double   upperQuantileP  = 0.90,
                       unsigned numBootstraps   = 5000,
                       double   confLevel       = 0.95,
		       IntervalType intervalType = IntervalType::TWO_SIDED)
      : BoundFutureReturns(buildMonthlyReturnsFromClosedPositions<Decimal>(closedPositions),
                           blockLen, lowerQuantileP, upperQuantileP, numBootstraps, confLevel, intervalType)
    {
      // Delegating constructor: builds monthly returns and delegates to the main constructor
      // This eliminates code duplication between the two constructors
    }

    /**
     * @brief Constructor that takes pre-computed monthly returns directly
     * @param monthlyReturns  Pre-computed vector of monthly returns
     * @param blockLen         Stationary block length (default 3 months)
     * @param lowerQuantileP   Lower quantile p in (0, 0.5) (default 0.10)
     * @param upperQuantileP   Upper quantile p in (0.5, 1)  (default 0.90)
     * @param numBootstraps    Number of bootstrap replicates B (default 5000)
     * @param confLevel        Confidence level in (0,1) for BCa CI (default 0.95)
     * @param intervalType     BCa interval type: TWO_SIDED or ONE_SIDED (default TWO_SIDED).
     *
     * This constructor avoids rebuilding monthly returns when they are already available.
     *
     * @throws std::invalid_argument If parameters are out of range or fewer than 8 months of data.
     */
    BoundFutureReturns(const std::vector<Decimal> &monthlyReturns,
                       unsigned blockLen        = 3,
                       double   lowerQuantileP  = 0.10,
                       double   upperQuantileP  = 0.90,
                       unsigned numBootstraps   = 5000,
                       double   confLevel       = 0.95,
		       IntervalType intervalType = IntervalType::TWO_SIDED)
      : m_lowerP(lowerQuantileP),
	m_upperP(upperQuantileP),
	m_B(numBootstraps),
	m_conf(confLevel),
	m_monthly(monthlyReturns),
	m_intervalType(intervalType)
    {
      validateInputs();

      if (m_monthly.size() < 8)
        {
          throw std::invalid_argument(
				      "BoundFutureReturns: need at least ~8 months to estimate quantile bounds robustly.");
        }

      // Determine interval types for each quantile
      IntervalType lowerIntervalType;
      IntervalType upperIntervalType;
      
      if (m_intervalType == IntervalType::TWO_SIDED)
	{
	  // Backward compatible: both use TWO_SIDED
	  lowerIntervalType = IntervalType::TWO_SIDED;
	  upperIntervalType = IntervalType::TWO_SIDED;
      }
      else
	{
	  // One-sided requested: use appropriate one-sided interval for each tail
	  // Lower quantile cares about lower bound → ONE_SIDED_LOWER
	  // Upper quantile cares about upper bound → ONE_SIDED_UPPER
	  lowerIntervalType = IntervalType::ONE_SIDED_LOWER;
	  upperIntervalType = IntervalType::ONE_SIDED_UPPER;
	}

      
      // 2) Resampler for BCa (stationary block by default).
      //    Note: IIDResampler ignores blockLen (uses default constructor)
      Resampler sampler = createResampler(blockLen);

      // 3) Statistic functions for BCa (quantiles on resampled monthly returns).
      using Bca = BCaBootStrap<Decimal, Resampler, Rng>;

      typename Bca::StatFn statLower = [this](const std::vector<Decimal> &v)
      {
	return empiricalQuantile<Decimal>(v, m_lowerP);
      };

      typename Bca::StatFn statUpper = [this](const std::vector<Decimal> &v)
      {
	return empiricalQuantile<Decimal>(v, m_upperP);
      };

      // 4) Run BCa for lower quantile
      {
	Bca bca(m_monthly, m_B, m_conf, statLower, sampler, lowerIntervalType);
	m_lower.point = statLower(m_monthly);
	m_lower.lo    = bca.getLowerBound();
	m_lower.hi    = bca.getUpperBound();
      }

      // 5) Run BCa for upper quantile
      {
	Bca bca(m_monthly, m_B, m_conf, statUpper, sampler, upperIntervalType);
	m_upper.point = statUpper(m_monthly);
	m_upper.lo    = bca.getLowerBound();
	m_upper.hi    = bca.getUpperBound();
      }

      // 6) Operational bounds (conservative defaults)
      m_operationalLower = m_lower.lo;
      m_operationalUpper = m_upper.hi;
    }

    // ---- Monitoring getters (the ones most clients will call) ----

    /**
     * @brief Conservative lower monitoring bound
     *        (BCa lower endpoint of the lower quantile).
     */
    Decimal getLowerBound() const noexcept
    {
      return m_operationalLower;
    }

    /**
     * @brief Conservative upper monitoring bound
     *        (BCa upper endpoint of the upper quantile).
     */
    Decimal getUpperBound() const noexcept
    {
      return m_operationalUpper;
    }

    // ---- Optional diagnostics / flexibility ----

    /// Lower quantile probability p used for the lower tail.
    double getLowerQuantileP() const noexcept { return m_lowerP; }
    /// Upper quantile probability p used for the upper tail.
    double getUpperQuantileP() const noexcept { return m_upperP; }
    /// Number of bootstrap replicates B.
    unsigned getNumBootstraps() const noexcept { return m_B; }
    /// Confidence level used for BCa intervals.
    double getConfidenceLevel() const noexcept { return m_conf; }

    /// Pre-computed (or internally built) monthly return series.
    const std::vector<Decimal> &getMonthlyReturns() const noexcept { return m_monthly; }

    /// Full BCa confidence-interval result for the lower quantile.
    QuantileCI<Decimal> getLowerQuantileCI() const noexcept { return m_lower; }
    /// Full BCa confidence-interval result for the upper quantile.
    QuantileCI<Decimal> getUpperQuantileCI() const noexcept { return m_upper; }

    /// Point estimate of the lower quantile (no CI adjustment).
    Decimal getLowerPointQuantile() const noexcept { return m_lower.point; }
    /// Point estimate of the upper quantile (no CI adjustment).
    Decimal getUpperPointQuantile() const noexcept { return m_upper.point; }

    /**
     * @brief Switch operational bounds to conservative policy (CI endpoints).
     *
     * Sets the lower bound to the BCa lower endpoint of the lower quantile and
     * the upper bound to the BCa upper endpoint of the upper quantile.
     */
    void useConservativePolicy() noexcept
    {
      m_operationalLower = m_lower.lo;
      m_operationalUpper = m_upper.hi;
    }

    /**
     * @brief Switch operational bounds to point-estimate policy.
     *
     * Sets both monitoring bounds to the raw quantile point estimates, ignoring
     * the BCa confidence interval. Useful when a less conservative view is preferred.
     */
    void usePointPolicy() noexcept
    {
      m_operationalLower = m_lower.point;
      m_operationalUpper = m_upper.point;
    }

  private:
    /**
     * @brief Validate constructor parameters against allowed ranges.
     *
     * @throws std::invalid_argument If lowerQuantileP is not in (0, 0.5),
     *         upperQuantileP is not in (0.5, 1), numBootstraps < 1000,
     *         or confLevel is not in (0, 1).
     */
    void validateInputs() const
    {
      if (!(m_lowerP > 0.0 && m_lowerP < 0.5))
        {
	  throw std::invalid_argument("BoundFutureReturns: lowerQuantileP must be in (0, 0.5).");
        }
      if (!(m_upperP > 0.5 && m_upperP < 1.0))
        {
	  throw std::invalid_argument("BoundFutureReturns: upperQuantileP must be in (0.5, 1).");
        }
      if (m_B < 1000)
        {
	  throw std::invalid_argument("BoundFutureReturns: numBootstraps should be >= ~1000.");
        }
      if (!(m_conf > 0.0 && m_conf < 1.0))
        {
	  throw std::invalid_argument("BoundFutureReturns: confLevel must be in (0, 1).");
        }
    }

    /**
     * @brief Create a StationaryBlockResampler with the given block length.
     *
     * SFINAE overload selected when Resampler is StationaryBlockResampler.
     */
    template<typename R = Resampler>
    typename std::enable_if<
      std::is_same<R, StationaryBlockResampler<Decimal>>::value, R>::type
    createResampler(unsigned blockLen) const
    {
      return R(blockLen);
    }

    /**
     * @brief Create an IIDResampler (ignores blockLen).
     *
     * SFINAE overload selected when Resampler is IIDResampler.
     */
    template<typename R = Resampler>
    typename std::enable_if<
      std::is_same<R, IIDResampler<Decimal, Rng>>::value, R>::type
    createResampler(unsigned /*blockLen*/) const
    {
      return R();  // IIDResampler uses default constructor
    }

  private:
    // Settings
    double   m_lowerP;
    double   m_upperP;
    unsigned m_B;
    double   m_conf;

    // Data
    std::vector<Decimal> m_monthly;
    IntervalType m_intervalType;

    // Quantile results
    QuantileCI<Decimal> m_lower;
    QuantileCI<Decimal> m_upper;

    // Operational bounds exposed to clients
    Decimal m_operationalLower;
    Decimal m_operationalUpper;
  };
} // namespace mkc_timeseries
