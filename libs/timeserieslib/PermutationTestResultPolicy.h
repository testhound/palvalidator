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

namespace mkc_timeseries
{
  // Policy class that represents just a p-value
  // being returned from monte-carlo permutation testing

  template <class Decimal>
  class PValueReturnPolicy
  {
  public:
    using ReturnType = Decimal;

    // Return a p-value from Monte-Carlo
    // permutation testing

    // the second parameter is unused and is by convention zero
    static ReturnType createReturnValue(Decimal pValue, Decimal unused)
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
    using ReturnType = std::tuple<Decimal, Decimal>;

    static ReturnType createReturnValue(Decimal pValue, Decimal testStat)
    {
      return std::make_tuple(pValue, testStat);
    }
  };


  //// Policy classes related to collection permutation test statistics

  // class PermutationTestingMaxTestStatisticPolicy represents collecting
  // the maximum value of a test statistic (e.g. sharpe ratio) observed
  // during permutation testing

  template <class Decimal> class PermutationTestingMaxTestStatisticPolicy
  {
  public:
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
    PermutationTestingNullTestStatisticPolicy()
    {}

    void updateTestStatistic(const Decimal& testStat)
    {}

    Decimal getTestStat() const
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };

  // 1) Helpers to detect ReturnType and createReturnValue(...)
  template<typename T, typename = void>
  struct has_return_type : std::false_type
  {};

  template<typename T>
  struct has_return_type<T, std::void_t<typename T::ReturnType>> : std::true_type
  {};

  template<typename T, typename = void>
  struct has_create_return_value : std::false_type
  {};

  template<typename T>
  struct has_create_return_value<T, std::void_t<
				      decltype(T::createReturnValue(std::declval<Decimal>(), std::declval<Decimal>()))>> : std::true_type
  {};

  // 2) Helpers to detect updateTestStatistic(...) and getTestStat()
  template<typename T, typename = void>
  struct has_update_test_statistic : std::false_type
  {};

  template<typename T>
  struct has_update_test_statistic<T, std::void_t<
					decltype(std::declval<T&>().updateTestStatistic(std::declval<Decimal>()))
					>> : std::true_type
  {};

  template<typename T, typename = void>
  struct has_get_test_stat : std::false_type
  {};

  template<typename T>
  struct has_get_test_stat<T, std::void_t<
				decltype(std::declval<T&>().getTestStat())>> : std::true_type
  {};
}
#endif
