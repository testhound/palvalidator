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
    REQUIRE(num::toString(d) == std::string("12.34500000"));
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

TEST_CASE("Round2Tick two-argument overload rounds to nearest tick correctly", "[number]") {
    DefaultNumber tick = createDecimal("0.05");

    SECTION("rounds down when remainder < half-tick") {
        DefaultNumber price = createDecimal("10.02");
        // 10.02 % 0.05 = 0.02 < 0.025 => 10.00
        REQUIRE(num::Round2Tick(price, tick) == createDecimal("10.00"));
    }

    SECTION("rounds up when remainder >= half-tick") {
        DefaultNumber price = createDecimal("10.03");
        // 0.03 >= 0.025 => 10.05
        REQUIRE(num::Round2Tick(price, tick) == createDecimal("10.05"));
    }

    SECTION("exact multiples remain unchanged") {
        DefaultNumber price = createDecimal("10.10");
        REQUIRE(num::Round2Tick(price, tick) == createDecimal("10.10"));
    }
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

TEST_CASE("RoundDownToTick floors price to the nearest lower tick multiple", "[number]") {
    DefaultNumber tick = createDecimal("0.05");

    SECTION("price already on a tick boundary is unchanged") {
        DefaultNumber price = createDecimal("10.10");
        REQUIRE(num::RoundDownToTick(price, tick) == createDecimal("10.10"));
    }

    SECTION("price just above a tick boundary rounds down") {
        DefaultNumber price = createDecimal("10.01");
        // 10.01 % 0.05 = 0.01 => floor to 10.00
        REQUIRE(num::RoundDownToTick(price, tick) == createDecimal("10.00"));
    }

    SECTION("price just below the next tick boundary rounds down, not up") {
        DefaultNumber price = createDecimal("10.04");
        // 0.04 < 0.05 => floor to 10.00, even though it is close to 10.05
        REQUIRE(num::RoundDownToTick(price, tick) == createDecimal("10.00"));
    }

    SECTION("price at exact half-tick rounds down, not up") {
        DefaultNumber price = createDecimal("10.025");
        // 0.025 is exactly half of 0.05 => floor to 10.00
        REQUIRE(num::RoundDownToTick(price, tick) == createDecimal("10.00"));
    }

    SECTION("remainder is always stripped regardless of how large the price is") {
        DefaultNumber price = createDecimal("1234.57");
        // 1234.57 % 0.05 = 0.02 => floor to 1234.55
        REQUIRE(num::RoundDownToTick(price, tick) == createDecimal("1234.55"));
    }

    SECTION("negative price floors away from zero (true floor semantics)") {
        DefaultNumber price = createDecimal("-10.03");
        // true floor: next lower multiple of 0.05 below -10.03 is -10.05
        REQUIRE(num::RoundDownToTick(price, tick) == createDecimal("-10.05"));
    }

    SECTION("negative price already on a tick boundary is unchanged") {
        DefaultNumber price = createDecimal("-10.05");
        REQUIRE(num::RoundDownToTick(price, tick) == createDecimal("-10.05"));
    }

    SECTION("works with a larger tick size") {
        DefaultNumber largeTick = createDecimal("0.25");
        DefaultNumber price     = createDecimal("10.37");
        // 10.37 % 0.25 = 0.12 => floor to 10.25
        REQUIRE(num::RoundDownToTick(price, largeTick) == createDecimal("10.25"));
    }

    SECTION("works with tick size of 1") {
        DefaultNumber wholeTick = createDecimal("1.00");
        DefaultNumber price     = createDecimal("7.99");
        REQUIRE(num::RoundDownToTick(price, wholeTick) == createDecimal("7.00"));
    }
}

TEST_CASE("RoundUpToTick ceils price to the nearest higher tick multiple", "[number]") {
    DefaultNumber tick = createDecimal("0.05");

    SECTION("price already on a tick boundary is unchanged") {
        DefaultNumber price = createDecimal("10.10");
        REQUIRE(num::RoundUpToTick(price, tick) == createDecimal("10.10"));
    }

    SECTION("price just above a tick boundary rounds up") {
        DefaultNumber price = createDecimal("10.01");
        // next multiple of 0.05 above 10.01 is 10.05
        REQUIRE(num::RoundUpToTick(price, tick) == createDecimal("10.05"));
    }

    SECTION("price just below the next tick boundary rounds up") {
        DefaultNumber price = createDecimal("10.04");
        // next multiple of 0.05 above 10.04 is 10.05
        REQUIRE(num::RoundUpToTick(price, tick) == createDecimal("10.05"));
    }

    SECTION("price at exact half-tick rounds up to the next boundary") {
        DefaultNumber price = createDecimal("10.025");
        // 10.025 is not on a tick boundary => ceil to 10.05
        REQUIRE(num::RoundUpToTick(price, tick) == createDecimal("10.05"));
    }

    SECTION("remainder is always rounded up regardless of how large the price is") {
        DefaultNumber price = createDecimal("1234.57");
        // 1234.57 % 0.05 = 0.02 => ceil to 1234.60
        REQUIRE(num::RoundUpToTick(price, tick) == createDecimal("1234.60"));
    }

    SECTION("negative price rounds up toward zero (true ceil semantics)") {
        DefaultNumber price = createDecimal("-10.03");
        // true ceil: next higher multiple of 0.05 above -10.03 is -10.00
        REQUIRE(num::RoundUpToTick(price, tick) == createDecimal("-10.00"));
    }

    SECTION("negative price already on a tick boundary is unchanged") {
        DefaultNumber price = createDecimal("-10.05");
        REQUIRE(num::RoundUpToTick(price, tick) == createDecimal("-10.05"));
    }

    SECTION("works with a larger tick size") {
        DefaultNumber largeTick = createDecimal("0.25");
        DefaultNumber price     = createDecimal("10.13");
        // next multiple of 0.25 above 10.13 is 10.25
        REQUIRE(num::RoundUpToTick(price, largeTick) == createDecimal("10.25"));
    }

    SECTION("works with tick size of 1") {
        DefaultNumber wholeTick = createDecimal("1.00");
        DefaultNumber price     = createDecimal("7.01");
        REQUIRE(num::RoundUpToTick(price, wholeTick) == createDecimal("8.00"));
    }
}

TEST_CASE("RoundDownToTick and RoundUpToTick are consistent with each other", "[number]") {
    DefaultNumber tick = createDecimal("0.05");

    SECTION("for a price on a boundary both functions agree") {
        DefaultNumber price = createDecimal("10.15");
        REQUIRE(num::RoundDownToTick(price, tick) == num::RoundUpToTick(price, tick));
    }

    SECTION("for an off-boundary price, RoundDown < price < RoundUp") {
        DefaultNumber price = createDecimal("10.03");
        DefaultNumber down  = num::RoundDownToTick(price, tick);
        DefaultNumber up    = num::RoundUpToTick(price, tick);

        REQUIRE(down < price);
        REQUIRE(price < up);

        // The gap between down and up must equal exactly one tick
        REQUIRE((up - down) == tick);
    }

    SECTION("RoundDown result is always a clean multiple of tick") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.03"),
            createDecimal("10.07"), createDecimal("99.99"),
        };
        for (auto& p : prices) {
            DefaultNumber down = num::RoundDownToTick(p, tick);
            REQUIRE(down % tick == createDecimal("0.00"));
        }
    }

    SECTION("RoundUp result is always a clean multiple of tick") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.03"),
            createDecimal("10.07"), createDecimal("99.99"),
        };
        for (auto& p : prices) {
            DefaultNumber up = num::RoundUpToTick(p, tick);
            REQUIRE(up % tick == createDecimal("0.00"));
        }
    }
}
