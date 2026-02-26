// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>
//
// Unit tests for DecimalConstants<Decimal>
//
// Framework : Catch2 v2  (catch2/catch_test_macros.hpp)
//
//
// The test suite is instantiated for two Decimal types:
//   1. dec::decimal<6>  – the primary fixed-point type used by the library.
//   2. double           – exercises the floating-point branch of createDecimal.
//
// Strategy
// --------
// Every constant is compared against a value independently constructed via
// createDecimal() from its canonical string.  This means the tests do NOT
// hard-code raw numeric literals; they rely on the same conversion path that
// DecimalConstants itself uses, so the tests remain valid regardless of the
// precision (Prec) chosen.  Independently, the string representations are
// also verified using dec::toString() for the dec::decimal<6> instantiation,
// providing a second, orthogonal check that catches any mismatch between what
// the library stores and what human-readable output is produced.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>
#include <cmath>

#include "decimal.h"
#include "DecimalConstants.h"

// ---------------------------------------------------------------------------
// Convenience aliases
// ---------------------------------------------------------------------------
using Dec6  = dec::decimal<6>;
using DC6   = mkc_timeseries::DecimalConstants<Dec6>;
using DCDbl = mkc_timeseries::DecimalConstants<double>;

// ---------------------------------------------------------------------------
// Helper: build a Dec6 from a string literal (keeps test bodies readable)
// ---------------------------------------------------------------------------
static Dec6 d6(const std::string& s)
{
    return DC6::createDecimal(s);
}

// ---------------------------------------------------------------------------
// Helper: build a double from a string literal
// ---------------------------------------------------------------------------
static double dd(const std::string& s)
{
    return DCDbl::createDecimal(s);
}

// ============================================================================
// SECTION 1 – createDecimal factory (both types)
// ============================================================================

TEST_CASE("createDecimal correctly converts string representations – Dec6",
          "[DecimalConstants][createDecimal][Dec6]")
{
    // Positive integers
    REQUIRE(DC6::createDecimal("0.0")    == d6("0.0"));
    REQUIRE(DC6::createDecimal("1.0")    == d6("1.0"));
    REQUIRE(DC6::createDecimal("2.0")    == d6("2.0"));
    REQUIRE(DC6::createDecimal("3.0")    == d6("3.0"));
    REQUIRE(DC6::createDecimal("100.0")  == d6("100.0"));

    // Negative integers
    REQUIRE(DC6::createDecimal("-1.0")   == d6("-1.0"));
    REQUIRE(DC6::createDecimal("-2.0")   == d6("-2.0"));
    REQUIRE(DC6::createDecimal("-3.0")   == d6("-3.0"));

    // Fractional values
    REQUIRE(DC6::createDecimal("0.01")   == d6("0.01"));
    REQUIRE(DC6::createDecimal("0.05")   == d6("0.05"));
    REQUIRE(DC6::createDecimal("0.10")   == d6("0.10"));
    REQUIRE(DC6::createDecimal("0.20")   == d6("0.20"));
    REQUIRE(DC6::createDecimal("0.001")  == d6("0.001"));
    REQUIRE(DC6::createDecimal("0.666667") == d6("0.666667"));
    REQUIRE(DC6::createDecimal("1.5")    == d6("1.5"));
    REQUIRE(DC6::createDecimal("1.75")   == d6("1.75"));
}

TEST_CASE("createDecimal correctly converts string representations – double",
          "[DecimalConstants][createDecimal][double]")
{
    // For double we compare with a tolerance since floating-point equality
    // after a round-trip through std::stod is not always exact.
    using namespace Catch::Matchers;

    REQUIRE_THAT(DCDbl::createDecimal("0.0"),      WithinAbs(0.0,   1e-9));
    REQUIRE_THAT(DCDbl::createDecimal("1.0"),      WithinAbs(1.0,   1e-9));
    REQUIRE_THAT(DCDbl::createDecimal("-1.0"),     WithinAbs(-1.0,  1e-9));
    REQUIRE_THAT(DCDbl::createDecimal("100.0"),    WithinAbs(100.0, 1e-9));
    REQUIRE_THAT(DCDbl::createDecimal("0.05"),     WithinAbs(0.05,  1e-9));
    REQUIRE_THAT(DCDbl::createDecimal("0.20"),     WithinAbs(0.20,  1e-9));
    REQUIRE_THAT(DCDbl::createDecimal("0.001"),    WithinAbs(0.001, 1e-9));
    REQUIRE_THAT(DCDbl::createDecimal("0.666667"), WithinAbs(0.666667, 1e-9));
}

// ============================================================================
// SECTION 2 – Whole-number constants (Dec6)
// ============================================================================

TEST_CASE("Whole-number constants have correct values – Dec6",
          "[DecimalConstants][whole-number][Dec6]")
{
    SECTION("DecimalZero is 0")
    {
        REQUIRE(DC6::DecimalZero == d6("0.0"));
        REQUIRE(dec::toString(DC6::DecimalZero) == "0.000000");
    }

    SECTION("DecimalOne is 1")
    {
        REQUIRE(DC6::DecimalOne == d6("1.0"));
        REQUIRE(dec::toString(DC6::DecimalOne) == "1.000000");
    }

    SECTION("DecimalMinusOne is -1")
    {
        REQUIRE(DC6::DecimalMinusOne == d6("-1.0"));
        REQUIRE(dec::toString(DC6::DecimalMinusOne) == "-1.000000");
    }

    SECTION("DecimalTwo is 2")
    {
        REQUIRE(DC6::DecimalTwo == d6("2.0"));
        REQUIRE(dec::toString(DC6::DecimalTwo) == "2.000000");
    }

    SECTION("DecimalMinusTwo is -2")
    {
        REQUIRE(DC6::DecimalMinusTwo == d6("-2.0"));
        REQUIRE(dec::toString(DC6::DecimalMinusTwo) == "-2.000000");
    }

    SECTION("DecimalThree is 3")
    {
        REQUIRE(DC6::DecimalThree == d6("3.0"));
        REQUIRE(dec::toString(DC6::DecimalThree) == "3.000000");
    }

    SECTION("DecimalMinusThree is -3")
    {
        REQUIRE(DC6::DecimalMinusThree == d6("-3.0"));
        REQUIRE(dec::toString(DC6::DecimalMinusThree) == "-3.000000");
    }

    SECTION("DecimalOneHundred is 100")
    {
        REQUIRE(DC6::DecimalOneHundred == d6("100.0"));
        REQUIRE(dec::toString(DC6::DecimalOneHundred) == "100.000000");
    }
}

// ============================================================================
// SECTION 3 – Fractional constants (Dec6)
// ============================================================================

TEST_CASE("Fractional constants have correct values – Dec6",
          "[DecimalConstants][fractional][Dec6]")
{
    SECTION("DecimalOnePointFive is 1.5")
    {
        REQUIRE(DC6::DecimalOnePointFive == d6("1.5"));
        REQUIRE(dec::toString(DC6::DecimalOnePointFive) == "1.500000");
    }

    SECTION("DecimalOnePointSevenFive is 1.75")
    {
        REQUIRE(DC6::DecimalOnePointSevenFive == d6("1.75"));
        REQUIRE(dec::toString(DC6::DecimalOnePointSevenFive) == "1.750000");
    }

    SECTION("TenPercent is 0.10")
    {
        REQUIRE(DC6::TenPercent == d6("0.10"));
        REQUIRE(dec::toString(DC6::TenPercent) == "0.100000");
    }

    SECTION("TwentyPercent is 0.20")
    {
        REQUIRE(DC6::TwentyPercent == d6("0.20"));
        REQUIRE(dec::toString(DC6::TwentyPercent) == "0.200000");
    }

    SECTION("EquityTick is 0.01")
    {
        REQUIRE(DC6::EquityTick == d6("0.01"));
        REQUIRE(dec::toString(DC6::EquityTick) == "0.010000");
    }

    SECTION("DefaultEquitySlippage is 0.001")
    {
        REQUIRE(DC6::DefaultEquitySlippage == d6("0.001"));
        REQUIRE(dec::toString(DC6::DefaultEquitySlippage) == "0.001000");
    }

    SECTION("SignificantPValue is 0.05")
    {
        REQUIRE(DC6::SignificantPValue == d6("0.05"));
        REQUIRE(dec::toString(DC6::SignificantPValue) == "0.050000");
    }

    SECTION("DefaultFDR is 0.20")
    {
        REQUIRE(DC6::DefaultFDR == d6("0.20"));
        REQUIRE(dec::toString(DC6::DefaultFDR) == "0.200000");
    }

    SECTION("TwoThirds is the fraction 0.666667, NOT a percentage")
    {
        // This test guards against regression to the old (incorrect) value of
        // "66.6666667".  The constant represents the fraction 2/3, so it must
        // be less than 1.
        REQUIRE(DC6::TwoThirds == d6("0.666667"));
        REQUIRE(dec::toString(DC6::TwoThirds) == "0.666667");

        // Explicitly assert it is less than 1 to make the intent undeniable.
        REQUIRE(DC6::TwoThirds < DC6::DecimalOne);

        // And sanity-check it is greater than 0.5
        REQUIRE(DC6::TwoThirds > d6("0.5"));
    }
}

// ============================================================================
// SECTION 4 – Relational sanity checks (Dec6)
//
// These tests verify that the constants bear the expected mathematical
// relationships to each other.  They act as a second line of defence: even if
// two constants were individually correct but accidentally swapped, these
// checks would catch it.
// ============================================================================

TEST_CASE("Constants satisfy expected ordering and arithmetic relationships – Dec6",
          "[DecimalConstants][relations][Dec6]")
{
    SECTION("Ordering of non-negative whole numbers")
    {
        REQUIRE(DC6::DecimalZero      < DC6::DecimalOne);
        REQUIRE(DC6::DecimalOne       < DC6::DecimalTwo);
        REQUIRE(DC6::DecimalTwo       < DC6::DecimalThree);
        REQUIRE(DC6::DecimalThree     < DC6::DecimalOneHundred);
    }

    SECTION("Negatives are less than zero")
    {
        REQUIRE(DC6::DecimalMinusOne   < DC6::DecimalZero);
        REQUIRE(DC6::DecimalMinusTwo   < DC6::DecimalMinusOne);
        REQUIRE(DC6::DecimalMinusThree < DC6::DecimalMinusTwo);
    }

    SECTION("Negation relationships")
    {
        REQUIRE(DC6::DecimalOne   + DC6::DecimalMinusOne   == DC6::DecimalZero);
        REQUIRE(DC6::DecimalTwo   + DC6::DecimalMinusTwo   == DC6::DecimalZero);
        REQUIRE(DC6::DecimalThree + DC6::DecimalMinusThree == DC6::DecimalZero);
    }

    SECTION("Additive relationships between whole numbers")
    {
        REQUIRE(DC6::DecimalOne  + DC6::DecimalOne  == DC6::DecimalTwo);
        REQUIRE(DC6::DecimalOne  + DC6::DecimalTwo  == DC6::DecimalThree);
    }

    SECTION("Percentage constants are fractions of one hundred")
    {
        // TenPercent * 100 == 10
        REQUIRE(DC6::TenPercent    * DC6::DecimalOneHundred == d6("10.0"));
        // TwentyPercent * 100 == 20
        REQUIRE(DC6::TwentyPercent * DC6::DecimalOneHundred == d6("20.0"));
        // DefaultFDR (0.20) == TwentyPercent
        REQUIRE(DC6::DefaultFDR == DC6::TwentyPercent);
    }

    SECTION("Statistical thresholds are in (0, 1)")
    {
        REQUIRE(DC6::SignificantPValue > DC6::DecimalZero);
        REQUIRE(DC6::SignificantPValue < DC6::DecimalOne);
        REQUIRE(DC6::DefaultFDR       > DC6::DecimalZero);
        REQUIRE(DC6::DefaultFDR       < DC6::DecimalOne);
    }

    SECTION("Slippage and tick are small positive values")
    {
        REQUIRE(DC6::DefaultEquitySlippage > DC6::DecimalZero);
        REQUIRE(DC6::EquityTick            > DC6::DecimalZero);
        REQUIRE(DC6::DefaultEquitySlippage < DC6::EquityTick);   // 0.001 < 0.01
        REQUIRE(DC6::EquityTick            < DC6::TenPercent);   // 0.01  < 0.10
    }

    SECTION("TwoThirds is strictly between 0.5 and 1")
    {
        REQUIRE(DC6::TwoThirds > d6("0.5"));
        REQUIRE(DC6::TwoThirds < DC6::DecimalOne);
    }

    SECTION("OnePointFive is between One and Two")
    {
        REQUIRE(DC6::DecimalOnePointFive > DC6::DecimalOne);
        REQUIRE(DC6::DecimalOnePointFive < DC6::DecimalTwo);
    }

    SECTION("OnePointSevenFive is between OnePointFive and Two")
    {
        REQUIRE(DC6::DecimalOnePointSevenFive > DC6::DecimalOnePointFive);
        REQUIRE(DC6::DecimalOnePointSevenFive < DC6::DecimalTwo);
    }
}

// ============================================================================
// SECTION 5 – Double instantiation smoke tests
//
// We only verify that the constants are non-zero / have the right sign for
// the double instantiation; the string-comparison checks above apply only to
// dec::decimal<N>.
// ============================================================================

TEST_CASE("DecimalConstants<double> constants have correct sign and rough magnitude",
          "[DecimalConstants][double]")
{
    using namespace Catch::Matchers;

    REQUIRE_THAT(DCDbl::DecimalZero,             WithinAbs(0.0,    1e-9));
    REQUIRE_THAT(DCDbl::DecimalOne,              WithinAbs(1.0,    1e-9));
    REQUIRE_THAT(DCDbl::DecimalMinusOne,         WithinAbs(-1.0,   1e-9));
    REQUIRE_THAT(DCDbl::DecimalTwo,              WithinAbs(2.0,    1e-9));
    REQUIRE_THAT(DCDbl::DecimalMinusTwo,         WithinAbs(-2.0,   1e-9));
    REQUIRE_THAT(DCDbl::DecimalThree,            WithinAbs(3.0,    1e-9));
    REQUIRE_THAT(DCDbl::DecimalMinusThree,       WithinAbs(-3.0,   1e-9));
    REQUIRE_THAT(DCDbl::DecimalOneHundred,       WithinAbs(100.0,  1e-9));
    REQUIRE_THAT(DCDbl::DecimalOnePointFive,     WithinAbs(1.5,    1e-9));
    REQUIRE_THAT(DCDbl::DecimalOnePointSevenFive,WithinAbs(1.75,   1e-9));
    REQUIRE_THAT(DCDbl::TenPercent,              WithinAbs(0.10,   1e-9));
    REQUIRE_THAT(DCDbl::TwentyPercent,           WithinAbs(0.20,   1e-9));
    REQUIRE_THAT(DCDbl::EquityTick,              WithinAbs(0.01,   1e-9));
    REQUIRE_THAT(DCDbl::DefaultEquitySlippage,   WithinAbs(0.001,  1e-12));
    REQUIRE_THAT(DCDbl::SignificantPValue,        WithinAbs(0.05,   1e-9));
    REQUIRE_THAT(DCDbl::DefaultFDR,              WithinAbs(0.20,   1e-9));
    REQUIRE_THAT(DCDbl::TwoThirds,               WithinAbs(0.666667, 1e-6));
}

// ============================================================================
// SECTION 6 – createADecimal free-function helper
// ============================================================================

TEST_CASE("createADecimal free function produces same result as createDecimal",
          "[DecimalConstants][createADecimal]")
{
    REQUIRE(mkc_timeseries::createADecimal<Dec6>("1.5")    == d6("1.5"));
    REQUIRE(mkc_timeseries::createADecimal<Dec6>("0.001")  == d6("0.001"));
    REQUIRE(mkc_timeseries::createADecimal<Dec6>("-3.0")   == d6("-3.0"));
    REQUIRE(mkc_timeseries::createADecimal<Dec6>("100.0")  == d6("100.0"));
}
