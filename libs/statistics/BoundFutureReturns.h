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
   * Template params:
   *   Decimal   : your fixed-precision decimal type (e.g., dec::decimal<8>)
   *   Resampler : StationaryBlockResampler<Decimal> by default (good for monthly series)
   *   Rng       : RNG type used by your BCa bootstrap (e.g., randutils::mt19937_rng)
   */
  template <class Decimal,
	    class Resampler = StationaryBlockResampler<Decimal>,
	    class Rng       = randutils::mt19937_rng>
  class BoundFutureReturns
  {
  public:
    /**
     * @param closedPositions  ClosedPositionHistory with realized trades
     * @param blockLen         Stationary block length (default 3 months)
     * @param lowerQuantileP   Lower quantile p in (0, 0.5) (default 0.10)
     * @param upperQuantileP   Upper quantile p in (0.5, 1)  (default 0.90)
     * @param numBootstraps    Number of bootstrap replicates B (default 5000)
     * @param confLevel        Confidence level in (0,1) for BCa CI (default 0.95)
     *
     * Throws std::invalid_argument on invalid parameters or insufficient data.
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
     *
     * This constructor avoids rebuilding monthly returns when they are already available.
     * Throws std::invalid_argument on invalid parameters or insufficient data.
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

    double getLowerQuantileP() const noexcept { return m_lowerP; }
    double getUpperQuantileP() const noexcept { return m_upperP; }
    unsigned getNumBootstraps() const noexcept { return m_B; }
    double getConfidenceLevel() const noexcept { return m_conf; }

    const std::vector<Decimal> &getMonthlyReturns() const noexcept { return m_monthly; }

    QuantileCI<Decimal> getLowerQuantileCI() const noexcept { return m_lower; }
    QuantileCI<Decimal> getUpperQuantileCI() const noexcept { return m_upper; }

    // Point quantiles (if a client prefers central policy instead of conservative CI endpoints).
    Decimal getLowerPointQuantile() const noexcept { return m_lower.point; }
    Decimal getUpperPointQuantile() const noexcept { return m_upper.point; }

    // If you later want to switch policy at runtime:
    void useConservativePolicy() noexcept
    {
      m_operationalLower = m_lower.lo;
      m_operationalUpper = m_upper.hi;
    }

    void usePointPolicy() noexcept
    {
      m_operationalLower = m_lower.point;
      m_operationalUpper = m_upper.point;
    }

  private:
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

    // Helper to create resampler - handles IIDResampler (no-arg constructor)
    // vs StationaryBlockResampler (takes blockLen)
    template<typename R = Resampler>
    typename std::enable_if<
      std::is_same<R, StationaryBlockResampler<Decimal>>::value, R>::type
    createResampler(unsigned blockLen) const
    {
      return R(blockLen);
    }

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
