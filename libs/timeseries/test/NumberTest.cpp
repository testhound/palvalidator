#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestUtils.h"
#include "number.h"
#include "SecurityAttributesFactory.h"

using namespace num; 
using num::DefaultNumber;
using namespace Catch;

TEST_CASE("operator% returns correct remainder for positive and negative values", "[number]") {
    DefaultNumber a = createDecimal("10.07");
    DefaultNumber b = createDecimal("0.05");

    // Positive remainder
    DefaultNumber remPos = a % b;
    REQUIRE(remPos == createDecimal("0.02"));

    // Negative remainder
    DefaultNumber negA = createDecimal("-10.07");
    DefaultNumber remNeg = negA % b;
    REQUIRE(remNeg == createDecimal("-0.02"));
}

TEST_CASE("toString produces the expected string representation", "[number]") {
    DefaultNumber d = createDecimal("12.345");
    REQUIRE(num::toString(d) == std::string("12.3450000"));
}

TEST_CASE("abs returns the absolute value of a decimal", "[number]") {
    DefaultNumber neg = createDecimal("-5.5");
    DefaultNumber pos = createDecimal("5.5");
    REQUIRE(num::abs(neg) == pos);
    REQUIRE(num::abs(pos) == pos);
}

TEST_CASE("to_double converts decimal to double accurately", "[number]") {
    DefaultNumber d = createDecimal("1.234");
    REQUIRE(num::to_double(d) == Approx(1.234));
}

TEST_CASE("fromString template parses string into decimal", "[number]") {
    DefaultNumber d = num::fromString<DefaultNumber>("2.718");
    REQUIRE(d == createDecimal("2.718"));
}

TEST_CASE("Round2Tick two-argument overload returns price unchanged", "[number]") {
    DefaultNumber price = createDecimal("10.03");
    DefaultNumber tick  = createDecimal("0.05");
    REQUIRE(num::Round2Tick(price, tick) == price);
}

TEST_CASE("Round2Tick three-argument overload rounds to nearest tick correctly", "[number]") {
    DefaultNumber tick     = createDecimal("0.05");
    DefaultNumber tickDiv2 = createDecimal("0.025");

    SECTION("round down when decimalMod < tickDiv2") {
        DefaultNumber price = createDecimal("10.02");
        // 10.02 % 0.05 = 0.02 < 0.025 => round down to 10.00
        REQUIRE(num::Round2Tick(price, tick, tickDiv2) == createDecimal("10.00"));
    }

    SECTION("round up when decimalMod >= tickDiv2") {
        DefaultNumber price1 = createDecimal("10.03");
        // 10.03 % 0.05 = 0.03 >= 0.025 => round up to 10.05
        REQUIRE(num::Round2Tick(price1, tick, tickDiv2) == createDecimal("10.05"));

        DefaultNumber price2 = createDecimal("10.08");
        // 10.08 % 0.05 = 0.03 >= 0.025 => round up to 10.10
        REQUIRE(num::Round2Tick(price2, tick, tickDiv2) == createDecimal("10.10"));
    }

    SECTION("exact multiples of tick remain unchanged") {
        DefaultNumber price  = createDecimal("10.10");
        // 10.10 % 0.05 = 0.00 < 0.025 => remains 10.10
        REQUIRE(num::Round2Tick(price, tick, tickDiv2) == createDecimal("10.10"));
    }
}

TEST_CASE("Round2Tick aligns prices to each factory security's tick", "[number][integration]") {
    auto& factory = mkc_timeseries::SecurityAttributesFactory<DefaultNumber>::instance();

    // A small set of sample raw prices to drive the test
    std::vector<DefaultNumber> rawPrices = {
        num::fromString<DefaultNumber>("100.00"),
        num::fromString<DefaultNumber>("100.03"),
        num::fromString<DefaultNumber>("99.98"),
        num::fromString<DefaultNumber>("1234.567"),
    };

    for (auto it = factory.beginSecurityAttributes(); it != factory.endSecurityAttributes(); ++it) {
        const auto& attrs = it->second;
        DefaultNumber tick    = attrs->getTick();
        // skip any bad ticks (e.g. zero)
        if (tick == num::fromString<DefaultNumber>("0.0")) continue;

        DefaultNumber halfTick = tick / num::fromString<DefaultNumber>("2.0");

        for (auto price : rawPrices) {
            auto rounded = num::Round2Tick(price, tick, halfTick);

            // 1) Must be a clean multiple of the tick
            auto remainder = rounded % tick;
            REQUIRE(remainder == num::fromString<DefaultNumber>("0.0"));

            // 2) Never move more than halfTick from original
            DefaultNumber diff = price - rounded;
            if (diff < num::fromString<DefaultNumber>("0.0"))
                diff = num::abs(diff);
            REQUIRE(diff <= halfTick);
        }
    }
}
