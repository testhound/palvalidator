#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestUtils.h"
#include "RoundingPolicies.h"

using num::DefaultNumber;
using mkc_timeseries::NoRounding;
using mkc_timeseries::TickRounding;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers shared across test cases
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    const DefaultNumber TICK      = createDecimal("0.05");
    const DefaultNumber TICK_DIV2 = createDecimal("0.025");
    const DefaultNumber ZERO      = createDecimal("0.0");
}

// ─────────────────────────────────────────────────────────────────────────────
// NoRounding
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("NoRounding::round always returns the price unchanged", "[rounding][NoRounding]") {
    SECTION("price already on a tick boundary is returned as-is") {
        DefaultNumber price = createDecimal("10.10");
        REQUIRE(NoRounding<DefaultNumber>::round(price, TICK, TICK_DIV2) == price);
    }

    SECTION("price with sub-tick remainder is returned as-is") {
        DefaultNumber price = createDecimal("10.03");
        REQUIRE(NoRounding<DefaultNumber>::round(price, TICK, TICK_DIV2) == price);
    }

    SECTION("negative price is returned as-is") {
        DefaultNumber price = createDecimal("-7.77");
        REQUIRE(NoRounding<DefaultNumber>::round(price, TICK, TICK_DIV2) == price);
    }

    SECTION("zero price is returned as-is") {
        REQUIRE(NoRounding<DefaultNumber>::round(ZERO, TICK, TICK_DIV2) == ZERO);
    }

    SECTION("tick and tickDiv2 arguments are ignored entirely") {
        DefaultNumber price      = createDecimal("99.99");
        DefaultNumber otherTick  = createDecimal("0.10");
        DefaultNumber otherHalf  = createDecimal("0.05");
        REQUIRE(NoRounding<DefaultNumber>::round(price, otherTick, otherHalf) == price);
    }
}

TEST_CASE("NoRounding::roundHigh always returns the price unchanged", "[rounding][NoRounding]") {
    SECTION("off-boundary price is returned as-is") {
        DefaultNumber price = createDecimal("10.03");
        REQUIRE(NoRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == price);
    }

    SECTION("on-boundary price is returned as-is") {
        DefaultNumber price = createDecimal("10.05");
        REQUIRE(NoRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == price);
    }

    SECTION("negative price is returned as-is") {
        DefaultNumber price = createDecimal("-5.03");
        REQUIRE(NoRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == price);
    }
}

TEST_CASE("NoRounding::roundLow always returns the price unchanged", "[rounding][NoRounding]") {
    SECTION("off-boundary price is returned as-is") {
        DefaultNumber price = createDecimal("10.03");
        REQUIRE(NoRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == price);
    }

    SECTION("on-boundary price is returned as-is") {
        DefaultNumber price = createDecimal("10.05");
        REQUIRE(NoRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == price);
    }

    SECTION("negative price is returned as-is") {
        DefaultNumber price = createDecimal("-5.03");
        REQUIRE(NoRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == price);
    }
}

TEST_CASE("NoRounding: all three methods return identical results for any input", "[rounding][NoRounding]") {
    std::vector<DefaultNumber> prices = {
        createDecimal("0.00"),  createDecimal("0.01"),
        createDecimal("10.03"), createDecimal("10.05"),
        createDecimal("-7.77"), createDecimal("9999.99"),
    };

    for (const auto& price : prices) {
        DefaultNumber r  = NoRounding<DefaultNumber>::round    (price, TICK, TICK_DIV2);
        DefaultNumber rh = NoRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2);
        DefaultNumber rl = NoRounding<DefaultNumber>::roundLow (price, TICK, TICK_DIV2);
        REQUIRE(r  == price);
        REQUIRE(rh == price);
        REQUIRE(rl == price);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TickRounding::round  (delegates to num::Round2Tick)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TickRounding::round rounds to the nearest tick", "[rounding][TickRounding]") {
    SECTION("rounds down when remainder < half-tick") {
        DefaultNumber price = createDecimal("10.02");
        // rem = 0.02 < 0.025 => 10.00
        REQUIRE(TickRounding<DefaultNumber>::round(price, TICK, TICK_DIV2) == createDecimal("10.00"));
    }

    SECTION("rounds up when remainder >= half-tick") {
        DefaultNumber price = createDecimal("10.03");
        // rem = 0.03 >= 0.025 => 10.05
        REQUIRE(TickRounding<DefaultNumber>::round(price, TICK, TICK_DIV2) == createDecimal("10.05"));
    }

    SECTION("price exactly at half-tick boundary rounds up") {
        DefaultNumber price = createDecimal("10.025");
        // rem = 0.025 == tickDiv2 => rounds up to 10.05
        REQUIRE(TickRounding<DefaultNumber>::round(price, TICK, TICK_DIV2) == createDecimal("10.05"));
    }

    SECTION("price already on a tick boundary is unchanged") {
        DefaultNumber price = createDecimal("10.10");
        REQUIRE(TickRounding<DefaultNumber>::round(price, TICK, TICK_DIV2) == createDecimal("10.10"));
    }

    SECTION("zero is returned as zero") {
        REQUIRE(TickRounding<DefaultNumber>::round(ZERO, TICK, TICK_DIV2) == ZERO);
    }

    SECTION("works with a larger tick size") {
        DefaultNumber largeTick  = createDecimal("0.25");
        DefaultNumber largeHalf  = createDecimal("0.125");

        DefaultNumber priceDown  = createDecimal("10.10");   // rem=0.10 < 0.125 => 10.00
        DefaultNumber priceUp    = createDecimal("10.15");   // rem=0.15 >= 0.125 => 10.25
        REQUIRE(TickRounding<DefaultNumber>::round(priceDown, largeTick, largeHalf) == createDecimal("10.00"));
        REQUIRE(TickRounding<DefaultNumber>::round(priceUp,   largeTick, largeHalf) == createDecimal("10.25"));
    }

    SECTION("result is always a clean multiple of tick") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.02"), createDecimal("10.03"),
            createDecimal("10.04"), createDecimal("10.06"), createDecimal("10.07"),
        };
        for (const auto& price : prices) {
            DefaultNumber rounded = TickRounding<DefaultNumber>::round(price, TICK, TICK_DIV2);
            REQUIRE(rounded % TICK == ZERO);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TickRounding::roundHigh  (delegates to num::RoundUpToTick)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TickRounding::roundHigh always rounds up to the next tick boundary", "[rounding][TickRounding]") {
    SECTION("price just above a boundary rounds up") {
        DefaultNumber price = createDecimal("10.01");
        REQUIRE(TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == createDecimal("10.05"));
    }

    SECTION("price just below the next boundary rounds up") {
        DefaultNumber price = createDecimal("10.04");
        REQUIRE(TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == createDecimal("10.05"));
    }

    SECTION("price exactly at half-tick rounds up, not to nearest") {
        DefaultNumber price = createDecimal("10.025");
        REQUIRE(TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == createDecimal("10.05"));
    }

    SECTION("price already on a tick boundary is unchanged") {
        DefaultNumber price = createDecimal("10.10");
        REQUIRE(TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == createDecimal("10.10"));
    }

    SECTION("tickDiv2 argument is ignored — only tick matters for ceil") {
        // Passing a nonsense tickDiv2 must not affect the result
        DefaultNumber price       = createDecimal("10.03");
        DefaultNumber bogusHalf   = createDecimal("0.00");
        REQUIRE(TickRounding<DefaultNumber>::roundHigh(price, TICK, bogusHalf) == createDecimal("10.05"));
    }

    SECTION("negative price rounds up toward zero") {
        DefaultNumber price = createDecimal("-10.03");
        // ceil of -10.03 with tick=0.05 is -10.00
        REQUIRE(TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2) == createDecimal("-10.00"));
    }

    SECTION("result is always a clean multiple of tick") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.03"),
            createDecimal("10.07"), createDecimal("99.99"),
        };
        for (const auto& price : prices) {
            DefaultNumber rounded = TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2);
            REQUIRE(rounded % TICK == ZERO);
        }
    }

    SECTION("rounded result is always >= original price") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.03"),
            createDecimal("10.05"), createDecimal("10.07"),
        };
        for (const auto& price : prices) {
            DefaultNumber rounded = TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2);
            REQUIRE(rounded >= price);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TickRounding::roundLow  (delegates to num::RoundDownToTick)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TickRounding::roundLow always rounds down to the lower tick boundary", "[rounding][TickRounding]") {
    SECTION("price just above a boundary rounds down") {
        DefaultNumber price = createDecimal("10.01");
        REQUIRE(TickRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == createDecimal("10.00"));
    }

    SECTION("price just below the next boundary rounds down") {
        DefaultNumber price = createDecimal("10.04");
        REQUIRE(TickRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == createDecimal("10.00"));
    }

    SECTION("price at exact half-tick rounds down, not up") {
        DefaultNumber price = createDecimal("10.025");
        REQUIRE(TickRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == createDecimal("10.00"));
    }

    SECTION("price already on a tick boundary is unchanged") {
        DefaultNumber price = createDecimal("10.10");
        REQUIRE(TickRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == createDecimal("10.10"));
    }

    SECTION("tickDiv2 argument is ignored — only tick matters for floor") {
        DefaultNumber price     = createDecimal("10.03");
        DefaultNumber bogusHalf = createDecimal("0.00");
        REQUIRE(TickRounding<DefaultNumber>::roundLow(price, TICK, bogusHalf) == createDecimal("10.00"));
    }

    SECTION("negative price floors away from zero") {
        DefaultNumber price = createDecimal("-10.03");
        // floor of -10.03 with tick=0.05 is -10.05
        REQUIRE(TickRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2) == createDecimal("-10.05"));
    }

    SECTION("result is always a clean multiple of tick") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.03"),
            createDecimal("10.07"), createDecimal("99.99"),
        };
        for (const auto& price : prices) {
            DefaultNumber rounded = TickRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2);
            REQUIRE(rounded % TICK == ZERO);
        }
    }

    SECTION("rounded result is always <= original price") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.03"),
            createDecimal("10.05"), createDecimal("10.07"),
        };
        for (const auto& price : prices) {
            DefaultNumber rounded = TickRounding<DefaultNumber>::roundLow(price, TICK, TICK_DIV2);
            REQUIRE(rounded <= price);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TickRounding: cross-method consistency
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TickRounding: roundLow and roundHigh bracket the original price", "[rounding][TickRounding]") {
    SECTION("for an off-boundary price: roundLow < price < roundHigh") {
        DefaultNumber price = createDecimal("10.03");
        DefaultNumber lo    = TickRounding<DefaultNumber>::roundLow (price, TICK, TICK_DIV2);
        DefaultNumber hi    = TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2);

        REQUIRE(lo < price);
        REQUIRE(price < hi);
        REQUIRE((hi - lo) == TICK);
    }

    SECTION("for a price on a boundary: all three methods agree") {
        DefaultNumber price  = createDecimal("10.15");
        DefaultNumber lo     = TickRounding<DefaultNumber>::roundLow (price, TICK, TICK_DIV2);
        DefaultNumber hi     = TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2);
        DefaultNumber mid    = TickRounding<DefaultNumber>::round    (price, TICK, TICK_DIV2);

        REQUIRE(lo  == price);
        REQUIRE(hi  == price);
        REQUIRE(mid == price);
    }

    SECTION("round result always falls within [roundLow, roundHigh]") {
        std::vector<DefaultNumber> prices = {
            createDecimal("10.01"), createDecimal("10.02"), createDecimal("10.03"),
            createDecimal("10.04"), createDecimal("10.06"), createDecimal("10.08"),
        };
        for (const auto& price : prices) {
            DefaultNumber lo  = TickRounding<DefaultNumber>::roundLow (price, TICK, TICK_DIV2);
            DefaultNumber hi  = TickRounding<DefaultNumber>::roundHigh(price, TICK, TICK_DIV2);
            DefaultNumber mid = TickRounding<DefaultNumber>::round    (price, TICK, TICK_DIV2);

            REQUIRE(mid >= lo);
            REQUIRE(mid <= hi);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Policy contrast: NoRounding vs TickRounding on the same inputs
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("NoRounding and TickRounding produce different results for off-boundary prices", "[rounding]") {
    DefaultNumber offBoundary = createDecimal("10.03");

    REQUIRE(NoRounding<DefaultNumber>::round    (offBoundary, TICK, TICK_DIV2) == offBoundary);
    REQUIRE(TickRounding<DefaultNumber>::round  (offBoundary, TICK, TICK_DIV2) != offBoundary);

    REQUIRE(NoRounding<DefaultNumber>::roundHigh(offBoundary, TICK, TICK_DIV2) == offBoundary);
    REQUIRE(TickRounding<DefaultNumber>::roundHigh(offBoundary, TICK, TICK_DIV2) != offBoundary);

    REQUIRE(NoRounding<DefaultNumber>::roundLow (offBoundary, TICK, TICK_DIV2) == offBoundary);
    REQUIRE(TickRounding<DefaultNumber>::roundLow(offBoundary, TICK, TICK_DIV2) != offBoundary);
}

TEST_CASE("NoRounding and TickRounding agree when the price is already on a tick boundary", "[rounding]") {
    DefaultNumber onBoundary = createDecimal("10.15");

    REQUIRE(NoRounding<DefaultNumber>::round    (onBoundary, TICK, TICK_DIV2)
         == TickRounding<DefaultNumber>::round  (onBoundary, TICK, TICK_DIV2));

    REQUIRE(NoRounding<DefaultNumber>::roundHigh(onBoundary, TICK, TICK_DIV2)
         == TickRounding<DefaultNumber>::roundHigh(onBoundary, TICK, TICK_DIV2));

    REQUIRE(NoRounding<DefaultNumber>::roundLow (onBoundary, TICK, TICK_DIV2)
         == TickRounding<DefaultNumber>::roundLow(onBoundary, TICK, TICK_DIV2));
}
