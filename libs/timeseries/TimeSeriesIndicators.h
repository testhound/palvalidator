// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIME_SERIES_INDICATORS_H
#define __TIME_SERIES_INDICATORS_H 1

#include <type_traits>
#include <cmath>
#include <utility>
#include "TimeSeries.h"
#include "DecimalConstants.h"
#include <algorithm>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include "ThrowAssert.hpp"

namespace mkc_timeseries
{
  using namespace boost::accumulators;

    /**
   * @brief Divides each element of series1 by its corresponding element in series2.
   *
   * Creates a new time series where each entry is the result of dividing the
   * value from series1 by the value from series2 at the same date.
   * The operation proceeds from the most recent date backwards.
   * The resulting series will have the length of the shorter of the two input series.
   * If a denominator value in series2 is zero, the resulting value for that date is zero.
   *
   * @tparam Decimal The numeric type used in the time series (e.g., double, a custom decimal class).
   * @param series1 The time series representing the numerators.
   * @param series2 The time series representing the denominators.
   * @return A new NumericTimeSeries containing the element-wise division results.
   * @throws std::domain_error if the time frames of the two series are different.
   * @throws std::domain_error if the end dates of the two series are different.
   * @throws Assertion failure (via throw_assert) if dates do not match during iteration.
   * @note Handles division by zero by setting the result to DecimalConstants<Decimal>::DecimalZero.
   */
  template <class Decimal>
  NumericTimeSeries<Decimal> DivideSeries (const NumericTimeSeries<Decimal>& series1,
					   const NumericTimeSeries<Decimal>& series2)
  {
    // if either input is empty, just return an empty series
    if (series1.getNumEntries() == 0 || series2.getNumEntries() == 0)
      return NumericTimeSeries<Decimal>(series1.getTimeFrame());

    if (series1.getTimeFrame() != series2.getTimeFrame())
      throw std::domain_error (std::string("DivideSeries:: time frame of two series must be the same"));

    // strict: only equal-length series are allowed
    if (series1.getNumEntries() != series2.getNumEntries())
        throw std::domain_error("DivideSeries:: series lengths must be the same");

    if (series1.getLastDate() != series2.getLastDate())
      throw std::domain_error (std::string ("DivideSeries:: end date of two series must be the same"));

    unsigned long seriesMin = std::min (series1.getNumEntries(), series2.getNumEntries());
    unsigned long initialEntries = std::max (seriesMin, (unsigned long) 1);

    NumericTimeSeries<Decimal> resultSeries(series1.getTimeFrame(), initialEntries);
    TimeFrame::Duration resultTimeFrame = series1.getTimeFrame();

    typename NumericTimeSeries<Decimal>::ConstReverseTimeSeriesIterator it1 = series1.beginReverseSortedAccess();
    typename NumericTimeSeries<Decimal>::ConstReverseTimeSeriesIterator it2 = series2.beginReverseSortedAccess();
    Decimal temp;

    for (; ((it1 != series1.endReverseSortedAccess()) && (it2 != series2.endReverseSortedAccess())); it1++, it2++)
      {
	throw_assert (it1->first == it2->first, "DivideSeries - date1: " +boost::posix_time::to_simple_string (it1->first) +" and date2: " +boost::posix_time::to_simple_string(it2->first) +" are not equal");
	if (it2->second->getValue() == DecimalConstants<Decimal>::DecimalZero)
	  temp = DecimalConstants<Decimal>::DecimalZero;
	else
	  temp = it1->second->getValue() / it2->second->getValue();

	resultSeries.addEntry (NumericTimeSeriesEntry<Decimal> (it1->first,
								temp,
								resultTimeFrame));
      }

    return resultSeries;
  }

  /**
   * @brief Calculates the Rate of Change (ROC) for a time series over a specified period.
   *
   * The ROC is calculated as: ((currentValue / value_period_ago) - 1) * 100.
   * The resulting series starts at the index 'period' of the original series.
   * If the input series has fewer than (period + 1) entries, an empty series is returned.
   *
   * @tparam Decimal The numeric type used in the time series.
   * @param series The input time series.
   * @param period The lookback period for the ROC calculation.
   * @return A new NumericTimeSeries containing the ROC values.
   * @note The first ROC value corresponds to the date at index 'period' in the input series.
   */
  template <class Decimal>
  NumericTimeSeries<Decimal> RocSeries (const NumericTimeSeries<Decimal>& series, uint32_t period)
  {
    size_t n = series.getNumEntries();
    size_t cap = (n > period ? n - period : 0);
    unsigned long initialEntries = cap;

    NumericTimeSeries<Decimal> resultSeries(series.getTimeFrame(), initialEntries);
    typename NumericTimeSeries<Decimal>::ConstRandomAccessIterator it = series.beginRandomAccess();

    if (series.getNumEntries() < (period + 1))
      return resultSeries;

    // Start at second element so we begin the rate of change calculatiom
    //it++;

    it = it + period;
    Decimal currentValue, prevValue, rocValue;
    std::shared_ptr<NumericTimeSeriesEntry<Decimal>> p;

    for (; it != series.endRandomAccess(); it++)
      {
	p = series.getTimeSeriesEntry (it, 0);
	currentValue = p->getValue();

	prevValue = series.getValue(it, period);
	if (prevValue == DecimalConstants<Decimal>::DecimalZero)
	  throw std::domain_error("RocSeries: division by zero in look-back value");

	rocValue = ((currentValue / prevValue) - DecimalConstants<Decimal>::DecimalOne) *
	  DecimalConstants<Decimal>::DecimalOneHundred;
	resultSeries.addEntry(NumericTimeSeriesEntry<Decimal> (p->getDateTime(),
							       rocValue,
							       series.getTimeFrame()));
      }

    return resultSeries;
  }

  /**
   * @brief Calculates the median value of all entries in a NumericTimeSeries.
   *
   * Extracts all values, sorts them, and computes the median.
   *
   * @tparam Decimal The numeric type used in the time series.
   * @param series The input time series.
   * @return The median value of the series.
   * @throws std::domain_error if the input time series is empty.
   */
  template <class Decimal>
  Decimal Median(const NumericTimeSeries<Decimal>& series)
  {
    typedef typename std::vector<Decimal>::size_type vec_size_type;

    std::vector<Decimal> sortedVector (series.getTimeSeriesAsVector());
    std::sort (sortedVector.begin(), sortedVector.end());

    vec_size_type size = sortedVector.size();
    if (size == 0)
      throw std::domain_error ("Cannot take median of empty time series");

    vec_size_type mid = size / 2;

    if ((size % 2) == 0)
      return (sortedVector[mid] + sortedVector[mid - 1])/DecimalConstants<Decimal>::DecimalTwo;
    else
      return sortedVector[mid];
  }

  /**
   * @brief Calculates the median value of elements in a vector of Decimal.
   *
   * Makes a copy of the input vector, sorts it, and computes the median.
   *
   * @tparam Decimal The numeric type used in the vector.
   * @param series The input vector of Decimal values.
   * @return The median value of the vector.
   * @throws std::domain_error if the input vector is empty.
   * @note This function operates on a copy of the input vector.
   */
  template <class Decimal>
  Decimal MedianOfVec(const std::vector<Decimal>& series)
  {
    typedef typename std::vector<Decimal>::size_type vec_size_type;

    std::vector<Decimal> sortedVector (series);
    std::sort (sortedVector.begin(), sortedVector.end());

    vec_size_type size = sortedVector.size();
    if (size == 0)
      throw std::domain_error ("Cannot take median of empty time series");

    vec_size_type mid = size / 2;

    if ((size % 2) == 0)
      return ((sortedVector[mid] + sortedVector[mid - 1])/DecimalConstants<Decimal>::DecimalTwo);
    else
      return sortedVector[mid];
  }

  /**
   * @brief Calculates the median value of elements in a generic vector.
   *
   * Makes a copy of the input vector, sorts it, and computes the median.
   * Requires type T to support copy construction, comparison (<), addition (+),
   * and division by T(2.0).
   *
   * @tparam T The data type of the elements in the vector. Must support necessary operations.
   * @param series The input vector.
   * @return The median value of the vector.
   * @throws std::domain_error if the input vector is empty.
   * @note This function operates on a copy of the input vector.
   */
  template <typename T>
  T Median(const std::vector<T>& series) {
    typedef typename std::vector<T>::size_type vec_size_type;

    std::vector<T> sortedVector(series);
    std::sort(sortedVector.begin(), sortedVector.end());

    vec_size_type size = sortedVector.size();
    if (size == 0) {
      throw std::domain_error("Cannot take median of empty time series");
    }

    vec_size_type mid = size / 2;

    if ((size % 2) == 0) {
      return (sortedVector[mid] + sortedVector[mid - 1]) / T(2.0);
    } else {
      return sortedVector[mid];
    }
  }

   /**
   * @brief Calculates the population standard deviation for a vector of arithmetic types.
   *
   * This overload is enabled only for standard arithmetic types (int, float, double, etc.).
   * Calculates the mean, variance (using N in the denominator), and then the square root.
   *
   * @tparam T An arithmetic type (e.g., int, float, double). Determined via std::is_arithmetic.
   * @param series The input vector of arithmetic values.
   * @return The population standard deviation as a double. Returns 0.0 if the vector is empty.
   * @note Calculates population standard deviation (divides variance by N).
   */
  template <typename T>
  typename std::enable_if<std::is_arithmetic<T>::value, double>::type
  StandardDeviation(const std::vector<T>& series)
  {
    if (series.empty())
      return 0.0;
    double sum = 0.0;
    for (T v : series) sum += v;
    double mean = sum / series.size();
    double var = 0.0;
    for (T v : series) {
      double d = v - mean;
      var += d * d;
    }
    var /= series.size();
    return std::sqrt(var);
  }

  /**
   * @brief Calculates the population standard deviation for a vector of non-arithmetic types.
   *
   * This overload is enabled for types that are not standard arithmetic types
   * (e.g., custom Decimal classes). It assumes the type T has a `getAsDouble()` method.
   * Uses boost::accumulators to compute the variance.
   *
   * @tparam T A non-arithmetic type, expected to have a `getAsDouble()` method and be constructible from double.
   * @param series The input vector of values.
   * @return The population standard deviation as type T. Returns DecimalConstants<T>::DecimalZero if the vector is empty.
   * @note Calculates population standard deviation (boost::accumulators::variance defaults to N).
   * @note Requires T to have `getAsDouble()` and be constructible from double.
   */
  template <typename T>
  typename std::enable_if<!std::is_arithmetic<T>::value, T>::type
  StandardDeviation(const std::vector<T>& series)
  {
    if (series.empty())
      return DecimalConstants<T>::DecimalZero;
    std::vector<double> vals;
    vals.reserve(series.size());
    for (const T& v : series)
      vals.push_back(v.getAsDouble());
    accumulator_set<double, features<tag::variance>> stats;
    stats = for_each(vals.begin(), vals.end(), stats);
    double var = variance(stats);
    return T(std::sqrt(var));
  }
  
  /**
   * @brief Calculates the Median Absolute Deviation (MAD), scaled for normality, for arithmetic types.
   *
   * Calculates the median of the input data, then the median of the absolute deviations
   * from that median. The result is scaled by ~1.4826 to make it a consistent
   * estimator of the standard deviation for normally distributed data.
   * This overload is enabled only for standard arithmetic types (int, float, double, etc.).
   *
   * @tparam T An arithmetic type (e.g., int, float, double). Determined via std::is_arithmetic.
   * @param series The input vector of arithmetic values.
   * @return The scaled MAD as a double. Returns 0.0 if the vector is empty.
   * @note The scaling factor 1.4826 makes the MAD comparable to the standard deviation for normal distributions.
   */
  template <typename T>
  typename std::enable_if<std::is_arithmetic<T>::value, double>::type
  MedianAbsoluteDeviation(const std::vector<T>& series)
  {
    if (series.empty())
      return 0.0;
    std::vector<T> sorted(series);
    std::sort(sorted.begin(), sorted.end());
    size_t n = sorted.size();
    double med = (n % 2 == 0)
      ? (static_cast<double>(sorted[n/2 - 1]) + sorted[n/2]) / 2.0
      : static_cast<double>(sorted[n/2]);
    std::vector<double> devs;
    devs.reserve(n);
    for (T v : series) devs.push_back(std::abs(static_cast<double>(v) - med));
    std::sort(devs.begin(), devs.end());
    if (n % 2 == 0)
      return ((devs[n/2 - 1] + devs[n/2]) / 2.0) * 1.4826;
    else
      return devs[n/2] * 1.4826;
  }

  /**
   * @brief Calculates the Median Absolute Deviation (MAD), scaled for normality, for non-arithmetic types.
   *
   * Calculates the median of the input data, then the median of the absolute deviations
   * from that median. The result is scaled by T(1.4826) to make it a consistent
   * estimator of the standard deviation for normally distributed data.
   * This overload is enabled for types that are not standard arithmetic types
   * (e.g., custom Decimal classes). It assumes the type T supports median calculation
   * (via MedianOfVec), subtraction, absolute value (.abs()), and construction from double.
   *
   * @tparam T A non-arithmetic type. Must support operations required by MedianOfVec,
   * subtraction (-), absolute value (.abs()), and construction from double (e.g., T(1.4826)).
   * @param series The input vector of values.
   * @return The scaled MAD as type T. Returns DecimalConstants<T>::DecimalZero if the vector is empty.
   * @note The scaling factor 1.4826 makes the MAD comparable to the standard deviation for normal distributions.
   * @note Relies on MedianOfVec for median calculations and assumes T has necessary operators/methods.
   */
  template <typename T>
  typename std::enable_if<!std::is_arithmetic<T>::value, T>::type
  MedianAbsoluteDeviation(const std::vector<T>& series)
  {
    if (series.empty())
      return DecimalConstants<T>::DecimalZero;
    // compute median of series
    T firstMed = MedianOfVec(series);
    std::vector<T> deviations;
    deviations.reserve(series.size());
    for (const T& v : series)
      deviations.push_back((v - firstMed).abs());
    T medDev = MedianOfVec(deviations);
    return medDev * T(1.4826);
  }
  
  /**
   * @brief High-level documentation of the Qₙ robust scale estimator algorithm.
   *
   * Qₙ is a 50%–breakdown‐point estimator with ~82% efficiency under Gaussian
   * assumptions.  It improves on the Median Absolute Deviation (MAD) by using
   * pairwise distances and an order‐statistic selection.
   *
   * Algorithm Overview:
   * -------------------
   * 1. Let n = number of observations. If n < 2, return zero.
   * 2. Compute:
   *      h = floor(n/2) + 1
   *      k = h * (h - 1) / 2
   * 3. Build a list `diffs` of all |x[j] - x[i]| for 0 ≤ i < j < n.
   * 4. Use `std::nth_element(diffs.begin(), diffs.begin() + (k-1), diffs.end())`
   *    to partition `diffs` so that the (k−1)th element is the k-th smallest.
   *    Let med = diffs[k-1].
   * 5. Compute the finite–sample correction factor cₙ:
   *      - For n ≤  9: use tabulated constants for unbiasedness under normality.
   *      - For n >  9 and n odd:  cₙ = (n / (n + 1.4)) * 2.2219
   *      - For n >  9 and n even: cₙ = (n / (n + 3.8)) * 2.2219
   *    Multiply: Qₙ = cₙ * med.
   *
   * Complexity and Robustness:
   * --------------------------
   * - Time:    O(n²) to generate diffs, plus O(n²) average for nth_element.
   * - Space:   O(n²) for the `diffs` vector.
   * - Breakdown point: 50% (resistant to up to half the data being outliers).
   * - Efficiency: ~82% under Gaussian models (far above ~37% for MAD).
   *
   * Pseudocode:
   * ~~~~~~~~~~
   *   function Qn(x[1..n]):
   *       if n < 2: return 0
   *       h = floor(n/2) + 1
   *       k = h*(h-1)/2
   *       diffs = []
   *       for i in 1..n-1:
   *           for j in i+1..n:
   *               diffs.push_back(abs(x[j] - x[i]))
   *       nth_element(diffs, k-1)
   *       med = diffs[k-1]
   *       return c_n(n) * med
   *
   * @see Rousseeuw, P.J. and Croux, C. (1993), “Alternatives to the Median Absolute
   *      Deviation”, Journal of the American Statistical Association.
   */

  template <class Decimal>
  class RobustQn
  {
  public:
     /**
     * @brief Construct a Q_n estimator from a time series.
     *
     * Copies all values from the given NumericTimeSeries into an internal
     * vector for computation.
     *
     * @param series  Source time series of Decimal values.
     */
    explicit RobustQn(const NumericTimeSeries<Decimal>& series)
      : mData(series.getTimeSeriesAsVector())
    {}

    /**
     * @brief Default construct; use getRobustQn(inputVec) instead.
     */
    RobustQn() = default;

    /**
     * @brief Compute Q_n for the stored time series.
     *
     * @return  The Q_n robust scale estimate.
     */
    Decimal getRobustQn() const
    {
      return computeQn(mData);
    }

     /**
     * @brief Compute Q_n for an arbitrary vector of values.
     *
     * Does not modify the input vector.
     *
     * @param inputVec  Vector of Decimal values.
     * @return          The Q_n robust scale estimate.
     */
    Decimal getRobustQn(const std::vector<Decimal>& inputVec) const
    {
      return computeQn(inputVec);
    }

  private:
    std::vector<Decimal> mData;

    /**
     * @brief Core Q_n computation on a value vector.
     *
     * Steps:
     *   1. Let n = values.size(). If n < 2, return zero.
     *   2. Compute h = floor(n/2) + 1 and k = h*(h-1)/2.
     *   3. Build all pairwise absolute differences |x_j - x_i| for i<j.
     *   4. Use std::nth_element to find the k-th smallest difference.
     *   5. Multiply the selected difference by the correction factor c_n.
     *
     * @param values  Input values for Q_n calculation.
     * @return        The scaled Q_n estimate.
     */
    static Decimal computeQn(const std::vector<Decimal>& values)
    {
      const size_t n = values.size();
      if (n < 2)
	return DecimalConstants<Decimal>::DecimalZero;

      // h = floor(n/2) + 1, k = h*(h-1)/2
      const size_t h = n/2 + 1;
      const size_t k = h*(h - 1)/2;

      // collect all pairwise absolute differences
      std::vector<Decimal> diffs;
      diffs.reserve((n*(n-1))/2);
      for (size_t i = 0; i + 1 < n; ++i) {
	for (size_t j = i + 1; j < n; ++j) {
	  Decimal d = values[j] - values[i];
	  if (d < DecimalConstants<Decimal>::DecimalZero)
	    d = -d;
	  diffs.push_back(d);
	}
      }
        
      // Select the k-th smallest difference (1-based index)
      std::nth_element(diffs.begin(), diffs.begin() + k - 1, diffs.end());
      Decimal med = diffs[k - 1];

      // apply finite-sample correction
      return computeCorrectionFactor(n) * med;
    }

    /**
     * @brief Compute the finite-sample correction factor c_n.
     *
     * For n <= 9: uses precomputed constants for unbiasedness under normality.
     * For n > 9: uses asymptotic approximations:
     *   - n odd:  c_n = (n / (n + 1.4)) * 2.2219
     *   - n even: c_n = (n / (n + 3.8)) * 2.2219
     *
     * @param n  Sample size.
     * @return   Scaling factor for Q_n estimator.
     */
    static Decimal computeCorrectionFactor(size_t n)
    {
      static constexpr double smallC[10] =
	{
	  0.0, 0.0, 0.399, 0.994, 0.512,
	  0.844, 0.611, 0.857, 0.669, 0.872
	};

      if (n <= 9)
	{
	  // use the exact small-sample constant
	  return Decimal(smallC[n]);
	}

      // otherwise use the asymptotic formula
      static constexpr double asymp = 2.2219;
      double dn = (n % 2 == 1)
                  ? (static_cast<double>(n) / (n + 1.4)) * asymp
                  : (static_cast<double>(n) / (n + 3.8)) * asymp;
      return Decimal(dn);
    }
  };
  /**
   * @brief Calculates the Internal Bar Strength (IBS) for a time series.
   *
   * IBS (Internal Bar Strength) is a mean reversion indicator that measures where
   * the closing price of a security falls within its daily price range.
   * IBS is calculated as: (Close - Low) / (High - Low).
   * It ranges from 0 to 1.
   * A low IBS (close near the low of the day) suggests potential buying opportunities,
   * while a high IBS (close near the high of the day) suggests potential selling opportunities.
   *
   * The resulting series has the same length as the input series.
   * If the denominator (High - Low) is zero, the resulting value for that date is zero.
   *
   * @tparam Decimal The numeric type used in the time series (e.g., double, a custom decimal class).
   * @param series The input OHLC time series.
   * @return A new NumericTimeSeries containing the IBS values.
   * @note Handles division by zero by setting the result to DecimalConstants<Decimal>::DecimalZero.
   */
  template <class Decimal>
  NumericTimeSeries<Decimal> IBS1Series (const OHLCTimeSeries<Decimal>& series)
  {
    // if input is empty, just return an empty series
    if (series.getNumEntries() == 0)
      return NumericTimeSeries<Decimal>(series.getTimeFrame());

    unsigned long initialEntries = series.getNumEntries();
    NumericTimeSeries<Decimal> resultSeries(series.getTimeFrame(), initialEntries);
    
    // Iterate through all entries in the OHLC series
    auto entries = series.getEntriesCopy();
    for (const auto& entry : entries)
    {
      Decimal high = entry.getHighValue();
      Decimal low = entry.getLowValue();
      Decimal close = entry.getCloseValue();
      Decimal ibsValue;
      
      // Calculate IBS: (Close - Low) / (High - Low)
      // Handle division by zero case (when High == Low)
      Decimal denominator = high - low;
      if (denominator == DecimalConstants<Decimal>::DecimalZero)
      {
        ibsValue = DecimalConstants<Decimal>::DecimalZero;
      }
      else
      {
        ibsValue = (close - low) / denominator;
      }
      
      resultSeries.addEntry(NumericTimeSeriesEntry<Decimal>(entry.getDateTime(),
                                                           ibsValue,
                                                           series.getTimeFrame()));
    }

    return resultSeries;
  }

  /**
   * @brief Computes the robust skewness statistic using the Medcouple estimator.
   *
   * The Medcouple (MC) is a robust, nonparametric measure of skewness defined as:
   *
   *    MC = median { [(x_j - m) - (m - x_i)] / (x_j - x_i) | x_i < m < x_j }
   *
   * Where:
   *   - m is the median of the data,
   *   - x_i are values below the median,
   *   - x_j are values above the median,
   *   - The kernel function is only evaluated where the denominator is non-zero.
   *
   * The medcouple takes values in the range [-1, 1]:
   *   - MC > 0: indicates right (positive) skew.
   *   - MC < 0: indicates left (negative) skew.
   *   - MC ≈ 0: indicates symmetric distribution.
   *
   * Robust to outliers with a 25% breakdown point, the medcouple is more appropriate than
   * classical skewness measures for financial data, which is often heavy-tailed and non-Gaussian.
   *
   * Reference:
   *   - Brys, G., Hubert, M., & Struyf, A. (2004). A Robust Measure of Skewness. *Journal of Computational and Graphical Statistics*, 13(4), 996–1017.
   *     https://doi.org/10.1198/106186004X12632
   *
   * @tparam Decimal A numeric type, such as double or a fixed-point Decimal class.
   * @param series A NumericTimeSeries containing the values to be analyzed.
   * @return The medcouple statistic, in the range [-1, 1], as a robust estimate of skew.
   * @throws std::domain_error if the series contains fewer than 3 values.
   */
  template <typename Decimal>
  Decimal RobustSkewMedcouple(const NumericTimeSeries<Decimal>& series) {
    std::vector<Decimal> data = series.getTimeSeriesAsVector();
    if (data.size() < 3)
      throw std::domain_error("RobustSkewMedcouple: Series too small");

    std::sort(data.begin(), data.end());
    const Decimal med = MedianOfVec(data);

    // Partition into lower and upper halves relative to the median.
    std::vector<Decimal> lower, upper;
    for (const auto& val : data) {
      if (val < med)
	lower.push_back(val);
      else if (val > med)
	upper.push_back(val);
      // Skip values equal to median — not used in kernel.
    }

    std::vector<Decimal> kernel;
    kernel.reserve(lower.size() * upper.size());

    for (size_t i = 0; i < lower.size(); ++i) {
      for (size_t j = 0; j < upper.size(); ++j) {
	Decimal denom = upper[j] - lower[i];
	if (denom == DecimalConstants<Decimal>::DecimalZero)
	  continue; // Avoid division by zero
	Decimal h = ((upper[j] - med) - (med - lower[i])) / denom;
	kernel.push_back(h);
      }
    }

    if (kernel.empty())
      return DecimalConstants<Decimal>::DecimalZero;

    std::sort(kernel.begin(), kernel.end());
    return MedianOfVec(kernel);
  }

  // ============================================
// Core: compute asymmetric raw return levels
// ============================================
/**
 * @brief Computes asymmetric profit target and stop loss levels using robust statistics.
 *
 * Skew is applied exactly once and scaled by Qn so its effect is in the same units
 * as the base dispersion. The stop-side sign is chosen so that a negative skew
 * (fatter left tail) widens the stop (i.e., makes the raw stop return more negative).
 *
 * Formulas:
 *   Profit Target = median + (k_qn * Qn) + (k_skew_target * Qn * skew)
 *   Stop Loss     = median - (k_qn * Qn) + (k_skew_stop   * Qn * skew)
 *
 * @tparam Decimal Numeric type.
 * @param median Median of the return series.
 * @param qn     Robust Qₙ scale estimator (non-negative).
 * @param skew   Robust medcouple skew in [-1,1].
 * @param k_qn   Multiplier for Qₙ (e.g., 1.0).
 * @param k_skew_target Multiplier for the skew term on the target side.
 * @param k_skew_stop   Multiplier for the skew term on the stop side.
 * @param[out] profitTarget Raw profit-target return (can be positive/negative).
 * @param[out] stopLoss     Raw stop-loss return (typically negative for longs).
 *
 * @throws std::domain_error if Qn is negative.
 */
template <typename Decimal>
void ComputeAsymmetricStopAndTarget(
    const Decimal& median,
    const Decimal& qn,
    const Decimal& skew,
    const Decimal& k_qn,
    const Decimal& k_skew_target,
    const Decimal& k_skew_stop,
    Decimal& profitTarget,
    Decimal& stopLoss)
{
  if (qn < DecimalConstants<Decimal>::DecimalZero)
    throw std::domain_error("Qn must be non-negative");

  // Profit side: push outward with dispersion and (signed) skew adjustment
  profitTarget = median + (k_qn * qn) + (k_skew_target * qn * skew);

  // Stop side: negative skew (left tail) should WIDEN the stop (more negative)
  // so its contribution must be + (const * qn * skew), which is negative when skew<0.
  stopLoss     = median - (k_qn * qn) + (k_skew_stop   * qn * skew);
}

// ============================================
// Helper functions for quantile-based analysis
// ============================================

/**
 * @brief Computes a sample quantile from a vector of values.
 *
 * Uses the nth_element algorithm to find the quantile without fully sorting the vector.
 * The input vector is modified during computation.
 *
 * @tparam Decimal Numeric type for calculations.
 * @param values Vector of values (will be modified during computation).
 * @param p Quantile probability in [0, 1].
 * @return The computed quantile value.
 */
template <typename Decimal>
Decimal SampleQuantile(std::vector<Decimal> values, double p)
{
  if (values.empty()) return DecimalConstants<Decimal>::DecimalZero;
  p = std::clamp(p, 0.0, 1.0);
  const size_t n = values.size();
  const size_t k = static_cast<size_t>(std::floor(p * (n - 1)));
  std::nth_element(values.begin(), values.begin() + k, values.end());
  return values[k];
}

/**
 * @brief Winsorizes a vector in-place by capping extreme values at specified quantiles.
 *
 * Replaces values below the tau-quantile with the tau-quantile value,
 * and values above the (1-tau)-quantile with the (1-tau)-quantile value.
 *
 * @tparam Decimal Numeric type for calculations.
 * @param values Vector to winsorize (modified in-place).
 * @param tau Tail probability for winsorization (e.g., 0.01 for 1% per tail).
 */

 template <typename Decimal>
void WinsorizeInPlace(std::vector<Decimal>& values, double tau)
{
  if (values.empty()) return;

  
  if (tau < 0.0) tau = 0.0;
  if (tau > 0.25) tau = 0.25;
  if (tau == 0.0) return;

  const size_t n = values.size();

  // Nearest-rank on (n-1)*p to pick tail cutpoints.
  auto kth_value = [&](double p) -> Decimal {
    if (p <= 0.0) {
      return *std::min_element(values.begin(), values.end());
    }
    if (p >= 1.0) {
      return *std::max_element(values.begin(), values.end());
    }
    const double r = p * static_cast<double>(n - 1);
    size_t k = static_cast<size_t>(std::llround(r));
    if (k >= n) k = n - 1;

    std::vector<Decimal> tmp(values);
    std::nth_element(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(k), tmp.end());
    return tmp[k];
  };

  const Decimal lo = kth_value(tau);
  const Decimal hi = kth_value(1.0 - tau);

  for (auto& x : values) {
    if (x < lo) x = lo;
    else if (x > hi) x = hi;
  }
}

/**
 * @brief Computes stop and target levels using winsorized quantiles method.
 *
 * This method uses empirical quantiles from winsorized return data to determine
 * typical upside and downside movements. It provides a "typical day" approach
 * based on historical return distribution.
 *
 * @tparam Decimal Numeric type for calculations.
 * @param series The OHLC time series to analyze.
 * @param period The lookback period for ROC calculation (default: 1).
 * @return A pair of {profitWidth, stopWidth} representing positive distances
 *         from the median for target and stop levels respectively.
 * @throws std::domain_error if series has fewer than 3 bars or ROC series is too small.
 */
template <typename Decimal>
std::pair<Decimal, Decimal>
ComputeQuantileStopAndTargetFromSeries(const OHLCTimeSeries<Decimal>& series,
                                       uint32_t period = 1)
{
  using namespace mkc_timeseries;

  // Fixed, minimal knobs (no curve fitting)
  constexpr double kWinsorTail       = 0.01;  // 1% per tail
  constexpr double kAlphaLower       = 0.10;  // lower quantile  (stop)
  constexpr double kAlphaUpper       = 0.10;  // upper quantile  (target)
  constexpr size_t kMinSample        = 20;    // min size for stable tails

  if (series.getNumEntries() < 3)
    throw std::domain_error("Input series must contain at least 3 bars");

  // Build ROC% series from in-sample closes
  auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
  auto rocVec    = rocSeries.getTimeSeriesAsVector();
  if (rocVec.size() < 3)
    throw std::domain_error("ROC series too small for estimation");

  const Decimal zero = DecimalConstants<Decimal>::DecimalZero;

  // 1) Center: median of raw ROC%
  const Decimal median = MedianOfVec(rocVec);

  // 2) Winsorize a copy lightly
  std::vector<Decimal> wv = rocVec;
  if (wv.size() >= kMinSample)
    WinsorizeInPlace(wv, kWinsorTail);

  // 3) One-sided quantiles around the median
  const Decimal q_lo = (wv.size() >= kMinSample) ? SampleQuantile(wv, kAlphaLower)
                                                 : median;
  const Decimal q_hi = (wv.size() >= kMinSample) ? SampleQuantile(wv, 1.0 - kAlphaUpper)
                                                 : median;

  // 4) Positive widths (typical downside/upside)
  Decimal profitWidth = q_hi - median;
  Decimal stopWidth   = median - q_lo;

  if (profitWidth < zero) profitWidth = zero;
  if (stopWidth   < zero) stopWidth   = zero;

  // Degenerate fallback
  if (profitWidth == zero && stopWidth == zero) {
    const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-6");
    return {eps, eps};
  }

  return {profitWidth, stopWidth};
}

/**
 * @brief Computes stop and target levels using robust Qn + Medcouple skew method.
 *
 * This method uses robust statistical measures (Qn scale estimator and Medcouple skew)
 * to determine asymmetric stop and target levels that account for return distribution
 * characteristics.
 *
 * @tparam Decimal Numeric type for calculations.
 * @param series The OHLC time series to analyze.
 * @param period The lookback period for ROC calculation.
 * @param useAnchors Whether to cap/floor the computed widths by empirical tails.
 * @return A pair of {profitWidth, stopWidth} representing positive distances
 *         from the median for target and stop levels respectively.
 * @throws std::domain_error if series has fewer than 3 bars or ROC series is too small.
 */
template <typename Decimal>
std::pair<Decimal, Decimal>
ComputeRobustStopAndTargetFromSeries(const OHLCTimeSeries<Decimal>& series,
                                     uint32_t period,
                                     bool useAnchors)
{
  using namespace mkc_timeseries;

  // Fixed, minimal knobs (no curve fitting)
  constexpr double kWinsorTail       = 0.01;  // 1% per tail
  constexpr double kAlphaLower       = 0.10;  // lower quantile  (stop)
  constexpr double kAlphaUpper       = 0.10;  // upper quantile  (target)
  constexpr size_t kMinSample        = 20;    // min size for stable tails

  if (series.getNumEntries() < 3)
    throw std::domain_error("Input series must contain at least 3 bars");

  // Build ROC% series from in-sample closes
  auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
  auto rocVec    = rocSeries.getTimeSeriesAsVector();
  if (rocVec.size() < 3)
    throw std::domain_error("ROC series too small for estimation");

  // Helper for finite value checking
  auto is_finite = [](const Decimal& x) {
    return std::isfinite(x.getAsDouble());
  };

  const Decimal zero = DecimalConstants<Decimal>::DecimalZero;

  // 1) Robust stats
  const Decimal median = MedianOfVec(rocVec);
  const Decimal qn     = RobustQn<Decimal>(rocSeries).getRobustQn();
  Decimal skew         = RobustSkewMedcouple(rocSeries);

  // 2) Clamp skew mildly
  const Decimal half = DecimalConstants<Decimal>::createDecimal("0.5");
  if (skew >  half) skew = half;
  if (skew < -half) skew = -half;

  // 3) Multipliers (symmetric base, unit skew weights)
  const Decimal k_qn          = DecimalConstants<Decimal>::DecimalOne;
  const Decimal k_skew_target = DecimalConstants<Decimal>::DecimalOne;
  const Decimal k_skew_stop   = DecimalConstants<Decimal>::DecimalOne;

  // 4) Core compute (raw levels)
  Decimal rawTarget, rawStop;
  ComputeAsymmetricStopAndTarget(
      median, qn, skew,
      k_qn, k_skew_target, k_skew_stop,
      rawTarget, rawStop);

  // 5) Convert to positive widths + symmetric fallbacks
  Decimal profitWidth = rawTarget;   // expect ≥ 0
  Decimal stopWidth   = -rawStop;    // expect ≥ 0

  if (profitWidth <= zero || !is_finite(profitWidth))
    profitWidth = median + qn;
  if (stopWidth   <= zero || !is_finite(stopWidth))
    stopWidth   = median + qn;

  // 6) Optional empirical anchors (cap target, floor stop by tails)
  if (useAnchors && rocVec.size() >= kMinSample)
  {
    std::vector<Decimal> wv = rocVec;
    WinsorizeInPlace(wv, kWinsorTail);

    const Decimal q_lo = SampleQuantile(wv, kAlphaLower);
    const Decimal q_hi = SampleQuantile(wv, 1.0 - kAlphaUpper);

    Decimal targetCap = q_hi - median;   // cap overly optimistic targets
    Decimal stopFloor = median - q_lo;   // floor overly tight stops

    if (targetCap < zero) targetCap = zero;
    if (stopFloor < zero) stopFloor = zero;

    if (targetCap > zero && profitWidth > targetCap)
      profitWidth = targetCap;
    if (stopWidth < stopFloor)
      stopWidth = stopFloor;
  }

  return {profitWidth, stopWidth};
}

template <typename Decimal>
inline std::pair<Decimal, Decimal>
ComputeRobustStopAndTargetFromSeries(const OHLCTimeSeries<Decimal>& series,
                                     uint32_t period = 1)
{
  constexpr bool kDefaultUseAnchors        = false; // ignored in quantile mode

  return ComputeRobustStopAndTargetFromSeries<Decimal>(
      series, period, kDefaultUseAnchors);
}

/**
 * @brief Computes asymmetric stop and target for a LONG position.
 *
 * This method implements a hybrid approach based on partitioned return distributions.
 * 1.  It separates the n-period Rate of Change (ROC) series into positive and negative returns.
 * 2.  The PROFIT TARGET is calculated from the POSITIVE returns distribution using
 * robust statistics (Median + Qn) to identify a "typical" profitable move.
 * 3.  The STOP LOSS is calculated from the NEGATIVE returns distribution using an
 * empirical quantile (e.g., 15th percentile) to identify a "typical worst-case" loss.
 *
 * This approach respects the inherent asymmetry of financial returns, modeling gains and
 * losses as distinct processes.
 *
 * @tparam Decimal Numeric type for calculations.
 * @param series The OHLC time series to analyze.
 * @param period The lookback period for ROC calculation (should match median hold time).
 * @return A pair of {profitWidth, stopWidth}, both representing positive distances.
 * @throws std::domain_error if the series has too few entries for meaningful analysis.
 */
template <typename Decimal>
std::pair<Decimal, Decimal>
ComputeLongStopAndTargetFromSeries(const OHLCTimeSeries<Decimal>& series,
                                     uint32_t period = 1)
{
    using namespace mkc_timeseries;

    // --- Configuration Constants ---
    constexpr size_t kMinTotalSamples = 25;     // Minimum total ROC values needed.
    constexpr size_t kMinPartitionSamples = 10; // Minimum values in pos/neg partitions.
    constexpr double kStopQuantile = 0.15;      // e.g., 15th percentile of losses for the stop.

    if (series.getNumEntries() < kMinTotalSamples)
        throw std::domain_error("ComputeLongStopAndTargetFromSeries: Input series too small for robust analysis.");

    // --- Data Preparation ---
    auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
    auto rocVec = rocSeries.getTimeSeriesAsVector();

    if (rocVec.size() < kMinTotalSamples)
        throw std::domain_error("ComputeLongStopAndTargetFromSeries: ROC series too small after calculation.");

    const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
    const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-6");
    
    std::vector<Decimal> positive_rocs, negative_rocs;
    for (const auto& roc : rocVec) {
        if (roc > zero)
            positive_rocs.push_back(roc);
        else if (roc < zero)
            negative_rocs.push_back(roc);
    }

    Decimal profitWidth = zero;
    Decimal stopWidth = zero;

    // --- Profit Target Calculation (from Positive ROCs) ---
    if (positive_rocs.size() >= kMinPartitionSamples)
    {
        RobustQn<Decimal> qn_estimator;
        Decimal median_pos = MedianOfVec(positive_rocs);
        Decimal qn_pos = qn_estimator.getRobustQn(positive_rocs);
        profitWidth = median_pos + qn_pos;
    }
    else
    {
        // Fallback: use a simple quantile of the entire distribution if not enough positive samples.
        profitWidth = SampleQuantile(rocVec, 0.75);
    }
    
    // --- Stop Loss Calculation (from Negative ROCs) ---
    if (negative_rocs.size() >= kMinPartitionSamples)
    {
        // SampleQuantile needs a copy as it modifies the vector.
        Decimal stop_level = SampleQuantile(std::vector<Decimal>(negative_rocs), kStopQuantile);
        stopWidth = -stop_level; // stop_level is negative, so this makes it positive.
    }
    else
    {
        // Fallback: use a simple quantile of the entire distribution.
        Decimal stop_level = SampleQuantile(rocVec, kStopQuantile);
        stopWidth = -stop_level;
    }
    
    // --- Sanity Checks and Finalization ---
    if (profitWidth <= zero)
        profitWidth = eps;
    if (stopWidth <= zero)
        stopWidth = eps;

    return {profitWidth, stopWidth};
}


/**
 * @brief Computes asymmetric stop and target for a SHORT position.
 *
 * This method mirrors the logic of the long-side calculation but for short selling.
 * 1.  It separates the n-period Rate of Change (ROC) series into positive and negative returns.
 * 2.  The PROFIT TARGET is calculated from the NEGATIVE returns distribution using
 * robust statistics (Median - Qn) to identify a "typical" profitable drop.
 * 3.  The STOP LOSS is calculated from the POSITIVE returns distribution using an
 * empirical quantile (e.g., 85th percentile) to identify an "unusually strong" rally.
 *
 * This approach respects the inherent asymmetry of financial returns, modeling gains and
 * losses as distinct processes.
 *
 * @tparam Decimal Numeric type for calculations.
 * @param series The OHLC time series to analyze.
 * @param period The lookback period for ROC calculation (should match median hold time).
 * @return A pair of {profitWidth, stopWidth}, both representing positive distances.
 * @throws std::domain_error if the series has too few entries for meaningful analysis.
 */
template <typename Decimal>
std::pair<Decimal, Decimal>
ComputeShortStopAndTargetFromSeries(const OHLCTimeSeries<Decimal>& series,
                                      uint32_t period = 1)
{
    using namespace mkc_timeseries;

    // --- Configuration Constants ---
    constexpr size_t kMinTotalSamples = 25;
    constexpr size_t kMinPartitionSamples = 10;
    constexpr double kStopQuantile = 0.85; // e.g., 85th percentile of gains for the stop.

    if (series.getNumEntries() < kMinTotalSamples)
        throw std::domain_error("ComputeShortStopAndTargetFromSeries: Input series too small for robust analysis.");

    // --- Data Preparation ---
    auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
    auto rocVec = rocSeries.getTimeSeriesAsVector();

    if (rocVec.size() < kMinTotalSamples)
        throw std::domain_error("ComputeShortStopAndTargetFromSeries: ROC series too small after calculation.");
    
    const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
    const Decimal eps = DecimalConstants<Decimal>::createDecimal("1e-6");

    std::vector<Decimal> positive_rocs, negative_rocs;
    for (const auto& roc : rocVec) {
        if (roc > zero)
            positive_rocs.push_back(roc);
        else if (roc < zero)
            negative_rocs.push_back(roc);
    }
    
    Decimal profitWidth = zero;
    Decimal stopWidth = zero;
    
    // --- Profit Target Calculation (from Negative ROCs) ---
    if (negative_rocs.size() >= kMinPartitionSamples)
    {
        RobustQn<Decimal> qn_estimator;
        Decimal median_neg = MedianOfVec(negative_rocs);
        Decimal qn_neg = qn_estimator.getRobustQn(negative_rocs);
        profitWidth = -(median_neg - qn_neg); // Result should be positive width.
    }
    else
    {
        // Fallback: use a simple quantile of the entire distribution.
        profitWidth = -(SampleQuantile(rocVec, 0.25));
    }
    
    // --- Stop Loss Calculation (from Positive ROCs) ---
    if (positive_rocs.size() >= kMinPartitionSamples)
    {
        // SampleQuantile needs a copy as it modifies the vector.
        stopWidth = SampleQuantile(std::vector<Decimal>(positive_rocs), kStopQuantile);
    }
    else
    {
        // Fallback: use a simple quantile of the entire distribution.
        stopWidth = SampleQuantile(rocVec, kStopQuantile);
    }

    // --- Sanity Checks and Finalization ---
    if (profitWidth <= zero)
        profitWidth = eps;
    if (stopWidth <= zero)
        stopWidth = eps;

    return {profitWidth, stopWidth};
}

}

#endif

