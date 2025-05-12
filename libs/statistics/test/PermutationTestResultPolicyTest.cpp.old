TEST_CASE("PermutationTesting Policies", "[MultipleTestingCorrection]") {
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
}
