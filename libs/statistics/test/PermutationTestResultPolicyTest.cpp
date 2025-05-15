#include <catch2/catch_test_macros.hpp>
#include "PermutationTestResultPolicy.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

TEST_CASE("PermutationTesting Policies", "[MultipleTestingCorrection]")
{
    using PolicyMax = PermutationTestingMaxTestStatisticPolicy<DecimalType>;
    using PolicyNull = PermutationTestingNullTestStatisticPolicy<DecimalType>;

    SECTION("PermutationTestingNullTestStatisticPolicy always returns zero") {
        PolicyNull policy;
        REQUIRE(policy.getTestStat() == DecimalConstants<DecimalType>::DecimalZero);
        policy.updateTestStatistic(createDecimal("10.5"));
        REQUIRE(policy.getTestStat() == DecimalConstants<DecimalType>::DecimalZero);
        policy.updateTestStatistic(createDecimal("-5.0"));
        REQUIRE(policy.getTestStat() == DecimalConstants<DecimalType>::DecimalZero);
    }

    SECTION("PermutationTestingMaxTestStatisticPolicy updates correctly") {
        PolicyMax policy;
        REQUIRE(policy.getTestStat() == DecimalConstants<DecimalType>::DecimalZero);
        DecimalType d10_5 = createDecimal("10.5");
        policy.updateTestStatistic(d10_5);
        REQUIRE(policy.getTestStat() == d10_5);
        policy.updateTestStatistic(createDecimal("5.0"));
        REQUIRE(policy.getTestStat() == d10_5);
        DecimalType d12_3 = createDecimal("12.3");
        policy.updateTestStatistic(d12_3);
        REQUIRE(policy.getTestStat() == d12_3);
        policy.updateTestStatistic(createDecimal("-2.0"));
        REQUIRE(policy.getTestStat() == d12_3);
        PolicyMax policy2 = policy;
        REQUIRE(policy2.getTestStat() == d12_3);
        PolicyMax policy3;
        policy3.updateTestStatistic(DecimalConstants<DecimalType>::DecimalOne);
        policy3 = policy;
        REQUIRE(policy3.getTestStat() == d12_3);
    }

    SECTION("PValueReturnPolicy returns only the p-value") {
      using Policy = PValueReturnPolicy<DecimalType>;

      DecimalType pValue = createDecimal("0.045");
      DecimalType dummy = DecimalConstants<DecimalType>::DecimalZero;
      DecimalType result = Policy::createReturnValue(pValue, dummy);

      REQUIRE(result == pValue);
    }

    SECTION("PValueAndTestStatisticReturnPolicy returns a tuple of p-value and test statistic") {
      using Policy = PValueAndTestStatisticReturnPolicy<DecimalType>;

      DecimalType pValue = createDecimal("0.123");
      DecimalType testStat = createDecimal("3.1415");

      auto result = Policy::createReturnValue(pValue, testStat);

      REQUIRE(std::get<0>(result) == pValue);
      REQUIRE(std::get<1>(result) == testStat);
    }
}
