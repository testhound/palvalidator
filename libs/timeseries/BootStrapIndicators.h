// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
//
// New Bootstrapped Stop/Target functions

#ifndef __BOOTSTRAPPED_INDICATORS_H
#define __BOOTSTRAPPED_INDICATORS_H

#include <functional>
#include <cmath> 
#include <stdexcept>
#include <algorithm>

// Include your two existing headers
#include "TimeSeriesIndicators.h"
#include "BiasCorrectedBootstrap.h"

namespace mkc_timeseries
{
  // Internal namespace for helper functions
  namespace detail
  {
    /**
     * @brief Helper struct to hold the four critical bounds from bootstrapping.
     */
    template <class Decimal>
    struct BootstrappedWidthBounds
    {
      Decimal upside_lower_bound;
      Decimal upside_upper_bound;
      Decimal downside_lower_bound;
      Decimal downside_upper_bound;
    };

    /**
     * @brief Internal helper to run BCa bootstrap on upside and downside widths.
     *
     * This function runs two separate BCa bootstraps using a stationary block
     * resampler to find the confidence intervals for:
     * 1. Upside Width: (q90 - median)
     * 2. Downside Width: (median - q10)
     *
     * It returns the conservative bounds needed for robust stop/target setting.
     *
     * @tparam Decimal Numeric type.
     * @param rocVec Vector of Rate-of-Change values.
     * @return BootstrappedWidthBounds struct containing the four key bounds.
     */
    template <class Decimal>
    BootstrappedWidthBounds<Decimal>
    ComputeBootstrappedWidths(const std::vector<Decimal>& rocVec)
    {
      // --- Configuration ---
      // Minimum sample size for a stable bootstrap
      constexpr size_t kMinBootstrapSize = 30;

      // Number of resamples (2000 is a good minimum)
      constexpr unsigned int kNumResamples = 10000;
      
      // We use a 90% CI to get the 5th and 95th percentiles

      constexpr double kConfidenceLevel = 0.90;

      // Small epsilon for degenerate cases
      const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-8");

      if (rocVec.size() < kMinBootstrapSize)
      {
        // Not enough data, return degenerate epsilon bounds
        // std::cerr << "Warning: Not enough data for bootstrap (" << rocVec.size() << " < " << kMinBootstrapSize << "). Returning eps." << std::endl;
        return {eps, eps, eps, eps};
      }

      using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

      // 1. Statistic for Upside (Profit) Width (for Longs)
      StatFn calc_upside_width = [](const std::vector<Decimal>& v) -> Decimal {
        if (v.size() < 2) return DecimalConstants<Decimal>::DecimalZero;
        // LinearInterpolationQuantile is in TimeSeriesIndicators.h and makes its own copy
        Decimal med = LinearInterpolationQuantile(v, 0.50);
        Decimal q90 = LinearInterpolationQuantile(v, 0.90);
        Decimal width = q90 - med;
        return (width < DecimalConstants<Decimal>::DecimalZero) ?
               DecimalConstants<Decimal>::DecimalZero : width;
      };

      // 2. Statistic for Downside (Stop) Width (for Longs)
      StatFn calc_downside_width = [](const std::vector<Decimal>& v) -> Decimal {
        if (v.size() < 2) return DecimalConstants<Decimal>::DecimalZero;
        Decimal med = LinearInterpolationQuantile(v, 0.50);
        Decimal q10 = LinearInterpolationQuantile(v, 0.10);
        Decimal width = med - q10;
        return (width < DecimalConstants<Decimal>::DecimalZero) ?
               DecimalConstants<Decimal>::DecimalZero : width;
      };

      // 3. Configure Stationary Block Resampler
      // Heuristic for block length: n^(1/3)
      const size_t n = rocVec.size();
      const size_t L = std::max<size_t>(2, static_cast<size_t>(std::pow(static_cast<double>(n), 1.0/3.0)));
      StationaryBlockResampler<Decimal> blockSampler(L);

      try
      {
        // 4. Run Bootstrap for Upside Width
        BCaBootStrap<Decimal, StationaryBlockResampler<Decimal>> bca_up(
            rocVec,
            kNumResamples,
            kConfidenceLevel,
            calc_upside_width,
            blockSampler
        );

        // 5. Run Bootstrap for Downside Width
        BCaBootStrap<Decimal, StationaryBlockResampler<Decimal>> bca_down(
            rocVec,
            kNumResamples,
            kConfidenceLevel,
            calc_downside_width,
            blockSampler
        );

        // 6. Extract all four bounds
        return {
          bca_up.getLowerBound(),
          bca_up.getUpperBound(),
          bca_down.getLowerBound(),
          bca_down.getUpperBound()
        };
      }
      catch (const std::exception& e)
      {
        // In case of bootstrap failure (e.g., all samples are identical)
        // std::cerr << "Bootstrap failed: " << e.what() << ". Returning eps." << std::endl;
        return {eps, eps, eps, eps};
      }
    }
  } // namespace detail


  /**
   * @brief Computes robust LONG-side profit target and stop widths using BCa bootstrap.
   *
   * This function calculates the distribution of upside (q90-median) and
   * downside (median-q10) widths using the Bias-Corrected and Accelerated (BCa)
   * stationary block bootstrap.
   *
   * It returns conservative, robust widths for a LONG trade:
   * - Profit Target: The 5th percentile (Lower Bound) of the upside width.
   * - Stop Loss:     The 95th percentile (Upper Bound) of the downside width.
   *
   * @tparam Decimal Numeric type.
   * @param series The OHLC time series to analyze.
   * @param period The lookback period for ROC calculation.
   * @return std::pair<Decimal, Decimal> {profit_width_long, stop_width_long}
   */
  template <typename Decimal>
  std::pair<Decimal, Decimal>
  ComputeBootStrappedLongStopAndTarget(const OHLCTimeSeries<Decimal>& series,
                                     uint32_t period)
  {
    if (series.getNumEntries() < 3)
      throw std::domain_error("ComputeBootStrappedLongStopAndTarget: input series too small");

    // RocSeries is in TimeSeriesIndicators.h
    auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
    auto rocVec    = rocSeries.getTimeSeriesAsVector();
    if (rocVec.size() < 3)
      throw std::domain_error("ComputeBootStrappedLongStopAndTarget: ROC series too small");

    // Get all 4 bounds from the bootstrap helper
    auto bounds = detail::ComputeBootstrappedWidths<Decimal>(rocVec);

    // For LONG trades:
    // Profit = Conservative Upside = Lower Bound of Upside Width
    // Stop   = Conservative Downside = Upper Bound of Downside Width
    Decimal profitWidth = bounds.upside_lower_bound;
    Decimal stopWidth   = bounds.downside_upper_bound;

    // Ensure non-degenerate widths
    const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-8");
    if (profitWidth <= DecimalConstants<Decimal>::DecimalZero)
      profitWidth = eps;
    if (stopWidth   <= DecimalConstants<Decimal>::DecimalZero)
      stopWidth   = eps;

    return {profitWidth, stopWidth};
  }

  /**
   * @brief Computes robust SHORT-side profit target and stop widths using BCa bootstrap.
   *
   * This function calculates the distribution of upside (q90-median) and
   * downside (median-q10) widths using the Bias-Corrected and Accelerated (BCa)
   * stationary block bootstrap.
   *
   * It returns conservative, robust widths for a SHORT trade:
   * - Profit Target: The 5th percentile (Lower Bound) of the downside width.
   * - Stop Loss:     The 95th percentile (Upper Bound) of the upside width.
   *
   * @tparam Decimal Numeric type.
   * @param series The OHLC time series to analyze.
   * @param period The lookback period for ROC calculation.
   * @return std::pair<Decimal, Decimal> {profit_width_short, stop_width_short}
   */
  template <typename Decimal>
  std::pair<Decimal, Decimal>
  ComputeBootStrappedShortStopAndTarget(const OHLCTimeSeries<Decimal>& series,
                                      uint32_t period)
  {
    if (series.getNumEntries() < 3)
      throw std::domain_error("ComputeBootStrappedShortStopAndTarget: input series too small");

    // RocSeries is in TimeSeriesIndicators.h
    auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
    auto rocVec    = rocSeries.getTimeSeriesAsVector();
    if (rocVec.size() < 3)
      throw std::domain_error("ComputeBootStrappedShortStopAndTarget: ROC series too small");

    // Get all 4 bounds from the bootstrap helper
    auto bounds = detail::ComputeBootstrappedWidths<Decimal>(rocVec);

    // For SHORT trades:
    // Profit = Conservative Downside = Lower Bound of Downside Width
    // Stop   = Conservative Upside   = Upper Bound of Upside Width
    Decimal profitWidth = bounds.downside_lower_bound;
    Decimal stopWidth   = bounds.upside_upper_bound;

    // Ensure non-degenerate widths
    const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-8");
    if (profitWidth <= DecimalConstants<Decimal>::DecimalZero)
      profitWidth = eps;
    if (stopWidth   <= DecimalConstants<Decimal>::DecimalZero)
      stopWidth   = eps;

    return {profitWidth, stopWidth};
  }

} // namespace mkc_timeseries

#endif // __BOOTSTRAPPED_INDICATORS_H
