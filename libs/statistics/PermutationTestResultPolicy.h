// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, 2025
//

/**
 * @file PermutationTestResultPolicy.h
 * @brief Result and statistics-collection policy classes for Monte-Carlo
 *        permutation testing, plus compile-time trait detectors for policy
 *        concept enforcement.
 *
 * Provides three result policies that control the return type of
 * runPermutationTest, two statistics-collection policies for tracking summary
 * test statistics across permutations, and SFINAE-based trait helpers used by
 * DefaultPermuteMarketChangesPolicy to verify policy contracts at compile time.
 */

#ifndef __PERMUTATION_TEST_RESULT_POLICY_H
#define __PERMUTATION_TEST_RESULT_POLICY_H 1

#include <string>
#include <tuple>
#include <type_traits>     // for std::void_t, std::false_type, std::true_type
#include <utility>         // for std::declval
#include "number.h"
#include "DecimalConstants.h"


namespace mkc_timeseries
{
  /**
   * @class PValueReturnPolicy
   * @brief Result policy that returns only the p-value from permutation testing.
   *
   * This is the default result policy. The summary test statistic and baseline
   * statistic parameters are accepted for interface compatibility but discarded.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template <class Decimal>
  class PValueReturnPolicy
  {
  public:
    using DecimalType = Decimal;
    using ReturnType = Decimal;

    /**
     * @brief Creates a return value containing only the p-value (2-parameter overload).
     * @param pValue         The computed permutation test p-value.
     * @param testStatistic  Unused; accepted for interface compatibility.
     * @return The p-value.
     */
    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic)
    {
      return pValue;
    }

    /**
     * @brief Creates a return value containing only the p-value (3-parameter overload).
     * @param pValue         The computed permutation test p-value.
     * @param testStatistic  Unused; accepted for interface compatibility.
     * @param baselineStat   Unused; accepted for interface compatibility.
     * @return The p-value.
     */
    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic, Decimal baselineStat)
    {
      return pValue;
    }
  };

  /**
   * @class PValueAndTestStatisticReturnPolicy
   * @brief Result policy that returns both the p-value and a summary test statistic.
   *
   * The return type is a std::tuple<Decimal, Decimal> where the first element is
   * the p-value and the second is the summary test statistic collected across
   * permutations (e.g., the maximum observed Sharpe ratio).
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template <class Decimal>
  class PValueAndTestStatisticReturnPolicy
  {
  public:
    using DecimalType = Decimal;
    using ReturnType = std::tuple<Decimal, Decimal>;

    /**
     * @brief Creates a tuple of (p-value, summary test statistic).
     * @param pValue    The computed permutation test p-value.
     * @param testStat  The summary test statistic from permutations.
     * @return A tuple containing the p-value and the test statistic.
     */
    static ReturnType createReturnValue(Decimal pValue, Decimal testStat)
    {
      return std::make_tuple(pValue, testStat);
    }

    /**
     * @brief Creates a tuple of (p-value, summary test statistic), discarding the baseline.
     * @param pValue         The computed permutation test p-value.
     * @param testStatistic  The summary test statistic from permutations.
     * @param baselineStat   Unused; accepted for interface compatibility.
     * @return A tuple containing the p-value and the test statistic.
     */
    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic, Decimal baselineStat)
    {
      return std::make_tuple(pValue, testStatistic);
    }
  };


  /**
   * @class FullPermutationResultPolicy
   * @brief Result policy that returns the p-value, summary test statistic, and baseline statistic.
   *
   * The return type is a std::tuple<Decimal, Decimal, Decimal> holding the raw p-value,
   * the summary (e.g., max) permuted test statistic, and the original baseline statistic.
   * This policy provides the most complete result set for downstream analysis.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template <class Decimal>
  class FullPermutationResultPolicy
  {
  public:
    using DecimalType = Decimal;
    // The tuple now holds: <raw pValue, baselineStat, maxPermutedStat>
    using ReturnType = std::tuple<Decimal, Decimal, Decimal>;

    /**
     * @brief Creates a 2-element tuple for compatibility with the static assertion.
     * @param pValue         The computed permutation test p-value.
     * @param testStatistic  The summary test statistic from permutations.
     * @return A 2-element tuple of (p-value, test statistic).
     */
    static std::tuple<Decimal, Decimal> createReturnValue(Decimal pValue, Decimal testStatistic)
    {
      return std::make_tuple(pValue, testStatistic);
    }

    /**
     * @brief Creates the full 3-element result tuple.
     * @param pValue         The computed permutation test p-value.
     * @param testStatistic  The summary test statistic from permutations.
     * @param baselineStat   The original baseline test statistic.
     * @return A 3-element tuple of (p-value, test statistic, baseline statistic).
     */
    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic, Decimal baselineStat)
    {
      return std::make_tuple(pValue, testStatistic, baselineStat);
    }
  };

  // --------------------------- Statistics Collection Policies -----------------------------

  /**
   * @class PermutationTestingMaxTestStatisticPolicy
   * @brief Collects the maximum test statistic observed across all permutations.
   *
   * Tracks the running maximum of a test statistic (e.g., Sharpe ratio, profit
   * factor) as each permutation completes. Used by multiple-testing correction
   * algorithms that need the maximum permuted statistic across the null
   * distribution.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template <class Decimal> class PermutationTestingMaxTestStatisticPolicy
  {
  public:
    using DecimalType = Decimal;

    /// Initializes the maximum to zero.
    PermutationTestingMaxTestStatisticPolicy()
      : mMaxTestStatistic(DecimalConstants<Decimal>::DecimalZero)
    {}

    /// Copy constructor.
    PermutationTestingMaxTestStatisticPolicy (const PermutationTestingMaxTestStatisticPolicy& rhs)
      : mMaxTestStatistic(rhs.mMaxTestStatistic)
    {}

    /// Copy assignment operator.
    PermutationTestingMaxTestStatisticPolicy<Decimal>&
    operator=(const PermutationTestingMaxTestStatisticPolicy<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mMaxTestStatistic = rhs.mMaxTestStatistic;
      return *this;
    }

    /**
     * @brief Updates the running maximum if the new statistic exceeds it.
     * @param testStat The test statistic from the latest permutation.
     */
    void updateTestStatistic(const Decimal& testStat)
    {
      if (testStat > mMaxTestStatistic)
	mMaxTestStatistic = testStat;
    }

    /**
     * @brief Returns the maximum test statistic observed so far.
     * @return The running maximum, or zero if no permutations have been recorded.
     */
    Decimal getTestStat() const
    {
      return mMaxTestStatistic;
    }

  private:
    Decimal mMaxTestStatistic;
  };

  /**
   * @class PermutationTestingNullTestStatisticPolicy
   * @brief No-op statistics collection policy that discards all test statistics.
   *
   * Used when only the p-value is needed and no summary statistic tracking is
   * required. Both updateTestStatistic and getTestStat are intentionally empty
   * or return zero, allowing the compiler to eliminate them entirely.
   *
   * @tparam Decimal The numerical type used for calculations.
   */
  template <class Decimal> class PermutationTestingNullTestStatisticPolicy
  {
  public:
    using DecimalType = Decimal;
    PermutationTestingNullTestStatisticPolicy()
    {}

    /// No-op; the statistic is intentionally discarded.
    void updateTestStatistic(const Decimal& testStat)
    {}

    /// Always returns zero since no statistics are tracked.
    Decimal getTestStat() const
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };

#include <type_traits>

#if __cplusplus < 201703L
namespace std {
  // C++14 doesn't have void_t; this polyfill makes it available
  template<typename...> using void_t = void;
}
#endif

// --------------------------- Compile-Time Trait Detectors -----------------------------

/// SFINAE trait: true if T defines a nested ReturnType alias.
template<typename T, typename = void>
struct has_return_type : std::false_type {};

template<typename T>
struct has_return_type<
    T,
    std::void_t<typename T::ReturnType>
> : std::true_type {};


/// SFINAE trait: true if T has a static createReturnValue(DecimalType, DecimalType).
template<typename T, typename = void>
struct has_create_return_value : std::false_type {};

template<typename T>
struct has_create_return_value<
    T,
    std::void_t<
      decltype(
        T::createReturnValue(
          std::declval<typename T::DecimalType>(),
          std::declval<typename T::DecimalType>()
        )
      )
    >
> : std::true_type {};

/// SFINAE trait: true if T has a static createReturnValue(DecimalType, DecimalType, DecimalType).
template<typename T, typename = void>
struct has_create_return_value_3param : std::false_type {};

template<typename T>
struct has_create_return_value_3param<
    T,
    std::void_t<
      decltype(
        T::createReturnValue(
          std::declval<typename T::DecimalType>(),
          std::declval<typename T::DecimalType>(),
          std::declval<typename T::DecimalType>()
        )
      )
    >
> : std::true_type {};


/// SFINAE trait: true if T has a member updateTestStatistic(DecimalType).
template<typename T, typename = void>
struct has_update_test_statistic : std::false_type {};

template<typename T>
struct has_update_test_statistic<
    T,
    std::void_t<
      decltype(
        std::declval<T&>().updateTestStatistic(
          std::declval<typename T::DecimalType>()
        )
      )
    >
> : std::true_type {};


/// SFINAE trait: true if T has a member getTestStat().
template<typename T, typename = void>
struct has_get_test_stat : std::false_type {};

template<typename T>
struct has_get_test_stat<
    T,
    std::void_t<
      decltype(
        std::declval<T&>().getTestStat()
      )
    >
> : std::true_type {};
}
#endif
