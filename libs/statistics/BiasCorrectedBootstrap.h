// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
// BCa (Bias-Corrected and Accelerated) Bootstrap Implementation
// with pluggable resampling policies
//
// Default policy: IIDResampler (classic i.i.d. bootstrap)
// Alternative policy: StationaryBlockResampler (mean block length L)
// Statistic: pluggable (default: arithmetic mean via StatUtils)
//
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
#include "randutils.hpp"  // high-quality RNG
#include "TimeFrame.h"
#include "StatUtils.h"    // StatUtils<Decimal>::computeMean by default

namespace mkc_timeseries
{

// --------------------------- Resampling Policies -----------------------------

// IID resampler: classic i.i.d. bootstrap (draw n items with replacement)
// IIID stands for independent and identically distributed. In statistics and
// probability, it refers to a sequence of random variables where each variable
//  is both independent of the others and follows the same probability distribution
template <class Decimal>
struct IIDResampler {
  std::vector<Decimal>
  operator()(const std::vector<Decimal>& x, size_t n, randutils::mt19937_rng& rng) const {
    if (x.empty()) throw std::invalid_argument("IIDResampler: empty sample.");
    std::vector<Decimal> y; y.reserve(n);
    for (size_t j = 0; j < n; ++j) {
      const size_t idx = rng.uniform(size_t(0), x.size() - 1);
      y.push_back(x[idx]);
    }
    return y;
  }
};

// Stationary block bootstrap (Politis & Romano):
// variable-length blocks with *mean* block length L (>=2).
// With probability p=1/L we start a new block at a random index; else we
// advance to the next index (circular).
template <class Decimal>
struct StationaryBlockResampler {
  explicit StationaryBlockResampler(size_t L = 3)
  : m_L(std::max<size_t>(2, L)) {}

  std::vector<Decimal>
  operator()(const std::vector<Decimal>& x, size_t n, randutils::mt19937_rng& rng) const {
    if (x.empty()) throw std::invalid_argument("StationaryBlockResampler: empty sample.");
    std::vector<Decimal> y; y.reserve(n);

    const double p = 1.0 / static_cast<double>(m_L);
    size_t idx = rng.uniform(size_t(0), x.size() - 1);

    while (y.size() < n) {
      y.push_back(x[idx]);
      if (rng.uniform(0.0, 1.0) < p) {
        idx = rng.uniform(size_t(0), x.size() - 1);    // start new block
      } else {
        idx = (idx + 1) % x.size();                    // continue block
      }
    }
    return y;
  }

  size_t meanBlockLen() const { return m_L; }

private:
  size_t m_L;
};

// ------------------------------ BCa Bootstrap --------------------------------

/**
 * @class BCaBootStrap
 * @brief Computes a statistic and BCa (Bias-Corrected and Accelerated) CI with pluggable resampling.
 *
 * Template parameters:
 *   - Decimal: your numeric type
 *   - Sampler: resampling policy (default: IIDResampler<Decimal>)
 *
 * The statistic itself is pluggable via a std::function<Decimal(const std::vector<Decimal>&)>.
 * By default, the arithmetic mean is used: StatUtils<Decimal>::computeMean.
 */
template <class Decimal, class Sampler = IIDResampler<Decimal>>
class BCaBootStrap {
public:
  using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

  // Default statistic = arithmetic mean; default sampler = Sampler()
  BCaBootStrap(const std::vector<Decimal>& returns,
               unsigned int num_resamples,
               double confidence_level = 0.95)
  : m_returns(returns),
    m_num_resamples(num_resamples),
    m_confidence_level(confidence_level),
    m_statistic(&mkc_timeseries::StatUtils<Decimal>::computeMean),
    m_sampler(Sampler{}),
    m_is_calculated(false)
  {
    validateConstructorArgs();
  }

  // Custom statistic; default-constructed sampler
  BCaBootStrap(const std::vector<Decimal>& returns,
               unsigned int num_resamples,
               double confidence_level,
               StatFn statistic)
  : m_returns(returns),
    m_num_resamples(num_resamples),
    m_confidence_level(confidence_level),
    m_statistic(std::move(statistic)),
    m_sampler(Sampler{}),
    m_is_calculated(false)
  {
    if (!m_statistic)
      throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
    validateConstructorArgs();
  }

  // Custom statistic + custom sampler instance (e.g., StationaryBlockResampler(L))
  BCaBootStrap(const std::vector<Decimal>& returns,
               unsigned int num_resamples,
               double confidence_level,
               StatFn statistic,
               Sampler sampler)
  : m_returns(returns),
    m_num_resamples(num_resamples),
    m_confidence_level(confidence_level),
    m_statistic(std::move(statistic)),
    m_sampler(std::move(sampler)),
    m_is_calculated(false)
  {
    if (!m_statistic)
      throw std::invalid_argument("BCaBootStrap: statistic function must be valid.");
    validateConstructorArgs();
  }

  virtual ~BCaBootStrap() = default;

  Decimal getMean()        const { ensureCalculated(); return m_theta_hat; }
  Decimal getStatistic()   const { return getMean(); } // alias for backward compat
  Decimal getLowerBound()  const { ensureCalculated(); return m_lower_bound; }
  Decimal getUpperBound()  const { ensureCalculated(); return m_upper_bound; }

protected:
  // Data & config
  const std::vector<Decimal>& m_returns;
  unsigned int m_num_resamples;
  double m_confidence_level;
  StatFn m_statistic;
  Sampler m_sampler;
  bool m_is_calculated;

  // Results
  Decimal m_theta_hat;
  Decimal m_lower_bound;
  Decimal m_upper_bound;

  // Test hooks (kept for mocks)
  void setStatistic(const Decimal& theta)   { m_theta_hat = theta; }
  void setMean(const Decimal& theta)        { m_theta_hat = theta; }
  void setLowerBound(const Decimal& lower)  { m_lower_bound = lower; }
  void setUpperBound(const Decimal& upper)  { m_upper_bound = upper; }

  void validateConstructorArgs() const {
    if (m_returns.empty())
      throw std::invalid_argument("BCaBootStrap: input returns vector cannot be empty.");
    if (m_num_resamples < 100)
      throw std::invalid_argument("BCaBootStrap: number of resamples should be at least 100.");
    if (m_confidence_level <= 0.0 || m_confidence_level >= 1.0)
      throw std::invalid_argument("BCaBootStrap: confidence level must be between 0 and 1.");
  }

  void ensureCalculated() const {
    if (!m_is_calculated)
      const_cast<BCaBootStrap*>(this)->calculateBCaBounds();
  }

  virtual void calculateBCaBounds() {
    if (m_is_calculated) return;

    const size_t n = m_returns.size();
    if (n < 2)
      throw std::invalid_argument("BCa bootstrap requires at least 2 data points.");

    // 1) θ̂ (original statistic)
    m_theta_hat = m_statistic(m_returns);

    // 2) Bootstrap replicates using the selected resampling policy
    std::vector<Decimal> boot_stats;
    boot_stats.reserve(m_num_resamples);

    thread_local static randutils::mt19937_rng rng;

    for (unsigned int b = 0; b < m_num_resamples; ++b) {
      // *** The only place resampling occurs: policy call ***
      std::vector<Decimal> resample = m_sampler(m_returns, n, rng);
      boot_stats.push_back(m_statistic(resample));
    }
    std::sort(boot_stats.begin(), boot_stats.end());

    // 3) Bias-correction z0
    const auto count_less = std::count_if(
      boot_stats.begin(), boot_stats.end(),
      [this](const Decimal& v){ return v < this->m_theta_hat; });
    const double prop_less = static_cast<double>(count_less) / static_cast<double>(m_num_resamples);
    const double z0 = inverseNormalCdf(prop_less);

    // 4) Acceleration a via jackknife (delete-1). For dependent data,
    // you may later switch to a block jackknife (delete-1-block).
    std::vector<Decimal> jk_stats; jk_stats.reserve(n);
    Decimal jk_sum(DecimalConstants<Decimal>::DecimalZero);
    std::vector<Decimal> loov; loov.reserve(n - 1);

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
      if (den15 > 0.0) a = num / (Decimal(6) * Decimal(den15));
    }

    // 5) Adjusted percentiles → bounds
    const double alpha = (1.0 - m_confidence_level) / 2.0;
    const double z_alpha_lo = inverseNormalCdf(alpha);
    const double z_alpha_hi = inverseNormalCdf(1.0 - alpha);

    const double a_d = a.getAsDouble();

    const double t1 = z0 + z_alpha_lo;
    const double alpha1 = standardNormalCdf(z0 + t1 / (1.0 - a_d * t1));

    const double t2 = z0 + z_alpha_hi;
    const double alpha2 = standardNormalCdf(z0 + t2 / (1.0 - a_d * t2));

    const int lower_idx = unbiasedIndex(alpha1, m_num_resamples);
    const int upper_idx = unbiasedIndex(alpha2, m_num_resamples);

    m_lower_bound = boot_stats[lower_idx];
    m_upper_bound = boot_stats[upper_idx];

    m_is_calculated = true;
  }

  // ---- Math helpers ----

  double standardNormalCdf(double x) const {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
  }

  double inverseNormalCdf(double p) const {
    if (p <= 0.0) return -std::numeric_limits<double>::infinity();
    if (p >= 1.0) return  std::numeric_limits<double>::infinity();
    return (p < 0.5) ? -inverseNormalCdfHelper(p) : inverseNormalCdfHelper(1.0 - p);
  }

  double inverseNormalCdfHelper(double p) const {
    // Abramowitz & Stegun 26.2.23 (sufficient for CI work)
    static const double c[] = {2.515517, 0.802853, 0.010328};
    static const double d[] = {1.432788, 0.189269, 0.001308};
    const double t = std::sqrt(-2.0 * std::log(p));
    const double num = (c[0] + c[1] * t + c[2] * t * t);
    const double den = (1.0 + d[0] * t + d[1] * t * t + d[2] * t * t * t);
    return t - num / den;
  }

  static int unbiasedIndex(double p, unsigned int B) {
    int idx = static_cast<int>(std::floor(p * (static_cast<double>(B) + 1.0))) - 1;
    if (idx < 0) idx = 0;
    const int maxIdx = static_cast<int>(B) - 1;
    if (idx > maxIdx) idx = maxIdx;
    return idx;
  }
};

// ------------------------------ Annualizer -----------------------------------

inline double calculateAnnualizationFactor(TimeFrame::Duration timeFrame,
                                           unsigned int intraday_minutes_per_bar = 0,
                                           double trading_days_per_year = 252.0,
                                           double trading_hours_per_day = 6.5)
{
  switch (timeFrame) {
    case TimeFrame::DAILY:    return trading_days_per_year;
    case TimeFrame::WEEKLY:   return 52.0;
    case TimeFrame::MONTHLY:  return 12.0;
    case TimeFrame::INTRADAY: {
      if (intraday_minutes_per_bar == 0)
        throw std::invalid_argument("For INTRADAY timeframe, intraday_minutes_per_bar must be specified.");
      double bars_per_hour = 60.0 / intraday_minutes_per_bar;
      return trading_hours_per_day * bars_per_hour * trading_days_per_year;
    }
    case TimeFrame::QUARTERLY: return 4.0;
    case TimeFrame::YEARLY:    return 1.0;
    default: throw std::invalid_argument("Unsupported time frame for annualization.");
  }
}

template <class Decimal>
class BCaAnnualizer {
public:
  BCaAnnualizer(const BCaBootStrap<Decimal>& bca_results, double annualization_factor) {
    if (annualization_factor <= 0.0)
      throw std::invalid_argument("Annualization factor must be positive.");
    const Decimal one = DecimalConstants<Decimal>::DecimalOne;
    const double mean_d  = (one + bca_results.getMean()).getAsDouble();
    const double lower_d = (one + bca_results.getLowerBound()).getAsDouble();
    const double upper_d = (one + bca_results.getUpperBound()).getAsDouble();

    m_annualized_mean        = Decimal(std::pow(mean_d,  annualization_factor)) - one;
    m_annualized_lower_bound = Decimal(std::pow(lower_d, annualization_factor)) - one;
    m_annualized_upper_bound = Decimal(std::pow(upper_d, annualization_factor)) - one;
  }

  Decimal getAnnualizedMean() const        { return m_annualized_mean; }
  Decimal getAnnualizedLowerBound() const  { return m_annualized_lower_bound; }
  Decimal getAnnualizedUpperBound() const  { return m_annualized_upper_bound; }

private:
  Decimal m_annualized_mean;
  Decimal m_annualized_lower_bound;
  Decimal m_annualized_upper_bound;
};

} // namespace mkc_timeseries

#endif // __BCA_BOOTSTRAP_H
