// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, 2025
//

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
  // Policy class that represents just a p-value
  // being returned from monte-carlo permutation testing

  template <class Decimal>
  class PValueReturnPolicy
  {
  public:
    using DecimalType = Decimal;
    using ReturnType = Decimal;

    // Return a p-value from Monte-Carlo
    // permutation testing

    // the second parameter is unused and is by convention zero
    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic)
    {
      return pValue;
    }

    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic, Decimal baselineStat)
    {
      return pValue;
    }
  };

  // Return a p-value and a test statistic from Monte-Carlo
  // permutation testing
  template <class Decimal>
  class PValueAndTestStatisticReturnPolicy
  {
  public:
    using DecimalType = Decimal;
    using ReturnType = std::tuple<Decimal, Decimal>;

    static ReturnType createReturnValue(Decimal pValue, Decimal testStat)
    {
      return std::make_tuple(pValue, testStat);
    }

    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic, Decimal baselineStat)
    {
      return std::make_tuple(pValue, testStatistic);
    }
  };


  // Returns a complete result set from a permutation test
  template <class Decimal>
  class FullPermutationResultPolicy
  {
  public:
    using DecimalType = Decimal;
    // The tuple now holds: <raw pValue, baselineStat, maxPermutedStat>
    using ReturnType = std::tuple<Decimal, Decimal, Decimal>;

    // 2-parameter version for compatibility with static assertion
    static std::tuple<Decimal, Decimal> createReturnValue(Decimal pValue, Decimal testStatistic)
    {
      return std::make_tuple(pValue, testStatistic);
    }

    static ReturnType createReturnValue(Decimal pValue, Decimal testStatistic, Decimal baselineStat)
    {
      return std::make_tuple(pValue, testStatistic, baselineStat);
    }
  };
  
  //// Policy classes related to collection permutation test statistics

  // class PermutationTestingMaxTestStatisticPolicy represents collecting
  // the maximum value of a test statistic (e.g. sharpe ratio) observed
  // during permutation testing

  template <class Decimal> class PermutationTestingMaxTestStatisticPolicy
  {
  public:
    using DecimalType = Decimal;
    PermutationTestingMaxTestStatisticPolicy()
      : mMaxTestStatistic(DecimalConstants<Decimal>::DecimalZero)
    {}

    PermutationTestingMaxTestStatisticPolicy (const PermutationTestingMaxTestStatisticPolicy& rhs)
      : mMaxTestStatistic(rhs.mMaxTestStatistic)
    {}

    PermutationTestingMaxTestStatisticPolicy<Decimal>&
    operator=(const PermutationTestingMaxTestStatisticPolicy<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mMaxTestStatistic = rhs.mMaxTestStatistic;
      return *this;
    }

    void updateTestStatistic(const Decimal& testStat)
    {
      if (testStat > mMaxTestStatistic)
	mMaxTestStatistic = testStat;
    }

    Decimal getTestStat() const
    {
      return mMaxTestStatistic;
    }

  private:
    Decimal mMaxTestStatistic;
  };

  // Class PermutationTestingNullTestStatisticPolicy
  // represents a policy of collecting no summary test statistics
  // This policy class is used when we just want to return a
  // p-value from permutation testing

  template <class Decimal> class PermutationTestingNullTestStatisticPolicy
  {
  public:
    using DecimalType = Decimal;
    PermutationTestingNullTestStatisticPolicy()
    {}

    void updateTestStatistic(const Decimal& testStat)
    {}

    Decimal getTestStat() const
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };

#include <type_traits>

#if __cplusplus < 201703L
namespace std {
  // C++14 doesn’t have void_t; this polyfill makes it available
  template<typename...> using void_t = void;
}
#endif

// ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// helper: detect T::ReturnType
template<typename T, typename = void>
struct has_return_type : std::false_type {};

template<typename T>
struct has_return_type<
    T,
    std::void_t<typename T::ReturnType>
> : std::true_type {};


// ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// helper: detect static createReturnValue(DecimalType, DecimalType)
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

// ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// helper: detect static createReturnValue(DecimalType, DecimalType, DecimalType)
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


// ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// helper: detect member updateTestStatistic(DecimalType)
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


// ––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
// helper: detect member getTestStat()
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
