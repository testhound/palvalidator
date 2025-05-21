#include <catch2/catch_test_macros.hpp>
#include "PercentNumber.h" //
#include "TestUtils.h"        // Assuming this contains DecimalType, fromString
#include "DecimalConstants.h" // For DecimalOneHundred

using namespace mkc_timeseries;
using namespace dec; // Assuming this is where DecimalType and fromString are aliased or defined from TestUtils.h

// ======= DecimalApprox matcher for Catch2 =======
// A simple Catch2-style approximate matcher for decimal types

template<typename Decimal>
struct DecimalApproxMatcher { // Renamed to avoid conflict if DecimalApprox is also a type elsewhere
    Decimal expected;
    Decimal tolerance;
    DecimalApproxMatcher(const Decimal& e, const Decimal& t)
      : expected(e), tolerance(t) {}
};

// Overload operator== so REQUIRE(actual == decimalApprox(...)) works
template<typename Decimal>
bool operator==(const Decimal& actual, const DecimalApproxMatcher<Decimal>& approx) {
    // Use decimal's abs() to compute absolute difference
    // Assuming Decimal has an abs() method and supports subtraction and <=
    if (actual > approx.expected)
        return (actual - approx.expected) <= approx.tolerance;
    else
        return (approx.expected - actual) <= approx.tolerance;
}

// Factory function to create DecimalApproxMatcher, analogous to Catch::Approx
template<typename Decimal>
auto decimalApprox(const Decimal& expected, const Decimal& tolerance) {
    return DecimalApproxMatcher<Decimal>(expected, tolerance);
}
// ======= End DecimalApprox matcher =======

TEST_CASE ("PercentNumber operations", "[PercentNumber]")
{
  using namespace dec;
  typedef DecimalType PercentType;

  PercentType profitTarget (fromString<DecimalType>("0.41"));
  PercentType profitTargetAsPercent (fromString<DecimalType>("0.0041"));
  PercentType stop (fromString<DecimalType>("0.39"));
  PercentType stopAsPercent (fromString<DecimalType>("0.0039"));

  // Tolerance for decimal comparisons, as provided by user.
  const PercentType DEC_TOL = fromString<PercentType>("0.00001");


  PercentNumber<DecimalType> profitTargetPercent = PercentNumber<DecimalType>::createPercentNumber (profitTarget);
  PercentNumber<DecimalType> aPercentNumber = PercentNumber<DecimalType>::createPercentNumber (std::string("0.41"));

  PercentNumber<DecimalType> stopPercent = PercentNumber<DecimalType>::createPercentNumber(stop);

  SECTION ("PercentNumber existing inequality tests"); // Renamed slightly to differentiate
  {
    REQUIRE (profitTargetPercent.getAsPercent() == profitTargetAsPercent);
    REQUIRE (aPercentNumber.getAsPercent() == profitTargetAsPercent);
    REQUIRE (stopPercent.getAsPercent() == stopAsPercent);
    REQUIRE (profitTargetPercent.getAsPercent() != stopPercent.getAsPercent());
    REQUIRE (profitTargetPercent.getAsPercent() > stopPercent.getAsPercent());
    REQUIRE (profitTargetPercent.getAsPercent() >= stopPercent.getAsPercent());
    REQUIRE (stopPercent.getAsPercent() <= profitTargetPercent.getAsPercent());
  }
}


TEST_CASE ("PercentNumber extended operations with DecimalApprox", "[PercentNumber]")
{
    using PercentType = DecimalType;
    const PercentType decimalOneHundred = DecimalConstants<PercentType>::DecimalOneHundred; //

    // Tolerance for decimal comparisons, using the user-provided value
    const PercentType TEST_DEC_TOL = fromString<PercentType>("0.00001");

    SECTION("Cache Behavior")
    {
        PercentType val1_decimal = fromString<PercentType>("25.0");
        PercentType val2_decimal = fromString<PercentType>("50.0");

        PercentNumber<PercentType> pn1_a = PercentNumber<PercentType>::createPercentNumber(val1_decimal);
        PercentNumber<PercentType> pn1_b = PercentNumber<PercentType>::createPercentNumber(val1_decimal);
        PercentNumber<PercentType> pn1_c = PercentNumber<PercentType>::createPercentNumber(std::string("25.0"));
        
        PercentNumber<PercentType> pn2_a = PercentNumber<PercentType>::createPercentNumber(val2_decimal);
        PercentNumber<PercentType> pn2_b = PercentNumber<PercentType>::createPercentNumber(std::string("50.0"));

        REQUIRE(pn1_a == pn1_b);
        REQUIRE(pn1_a == pn1_c);
        REQUIRE(pn1_a.getAsPercent() == decimalApprox(fromString<PercentType>("0.25"), TEST_DEC_TOL));

        REQUIRE(pn1_a != pn2_a);
        REQUIRE(pn2_a.getAsPercent() == decimalApprox(fromString<PercentType>("0.50"), TEST_DEC_TOL));
        REQUIRE(pn2_a == pn2_b);
    }

    SECTION("Edge Cases for Creation")
    {
        PercentNumber<PercentType> pn_zero_decimal = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("0.0"));
        REQUIRE(pn_zero_decimal.getAsPercent() == decimalApprox(fromString<PercentType>("0.0"), TEST_DEC_TOL));

        PercentNumber<PercentType> pn_zero_string = PercentNumber<PercentType>::createPercentNumber(std::string("0"));
        REQUIRE(pn_zero_string.getAsPercent() == decimalApprox(fromString<PercentType>("0.0"), TEST_DEC_TOL));
        REQUIRE(pn_zero_decimal == pn_zero_string);

        PercentNumber<PercentType> pn_negative_decimal = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("-10.0"));
        REQUIRE(pn_negative_decimal.getAsPercent() == decimalApprox(fromString<PercentType>("-0.10"), TEST_DEC_TOL));

        PercentNumber<PercentType> pn_negative_string = PercentNumber<PercentType>::createPercentNumber(std::string("-10.0"));
        REQUIRE(pn_negative_string.getAsPercent() == decimalApprox(fromString<PercentType>("-0.10"), TEST_DEC_TOL));
        REQUIRE(pn_negative_decimal == pn_negative_string);

        PercentNumber<PercentType> pn_small_positive = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("0.0001"));
        REQUIRE(pn_small_positive.getAsPercent() == decimalApprox(fromString<PercentType>("0.000001"), TEST_DEC_TOL));
        
        PercentNumber<PercentType> pn_large_positive = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("1000000.0"));
        REQUIRE(pn_large_positive.getAsPercent() == decimalApprox(fromString<PercentType>("10000.0"), TEST_DEC_TOL));

        // Test with createAPercentNumber global helper
        PercentNumber<PercentType> pn_helper_zero = createAPercentNumber<PercentType>(std::string("0.0"));
        REQUIRE(pn_helper_zero.getAsPercent() == decimalApprox(fromString<PercentType>("0.0"), TEST_DEC_TOL));
        REQUIRE(pn_helper_zero == pn_zero_decimal);

        PercentNumber<PercentType> pn_helper_positive = createAPercentNumber<PercentType>(std::string("15.5"));
        REQUIRE(pn_helper_positive.getAsPercent() == decimalApprox(fromString<PercentType>("0.155"), TEST_DEC_TOL));

        PercentNumber<PercentType> pn_helper_negative = createAPercentNumber<PercentType>(std::string("-5.25"));
        REQUIRE(pn_helper_negative.getAsPercent() == decimalApprox(fromString<PercentType>("-0.0525"), TEST_DEC_TOL));
    }

    SECTION("Copy Constructor and Assignment Operator")
    {
        PercentNumber<PercentType> original = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("75.0"));
        REQUIRE(original.getAsPercent() == decimalApprox(fromString<PercentType>("0.75"), TEST_DEC_TOL));

        PercentNumber<PercentType> copy_constructed(original); //
        REQUIRE(copy_constructed.getAsPercent() == decimalApprox(fromString<PercentType>("0.75"), TEST_DEC_TOL));
        REQUIRE(original == copy_constructed); 

        PercentNumber<PercentType> assigned_val = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("10.0"));
        REQUIRE(assigned_val.getAsPercent() == decimalApprox(fromString<PercentType>("0.10"), TEST_DEC_TOL));
        
        assigned_val = original; //
        REQUIRE(assigned_val.getAsPercent() == decimalApprox(fromString<PercentType>("0.75"), TEST_DEC_TOL));
        REQUIRE(original == assigned_val);

        assigned_val = assigned_val; // Self-assignment
        REQUIRE(assigned_val.getAsPercent() == decimalApprox(fromString<PercentType>("0.75"), TEST_DEC_TOL));

        PercentNumber<PercentType> another_original = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("80.0"));
        REQUIRE(copy_constructed.getAsPercent() == decimalApprox(fromString<PercentType>("0.75"), TEST_DEC_TOL));
        REQUIRE(assigned_val.getAsPercent() == decimalApprox(fromString<PercentType>("0.75"), TEST_DEC_TOL));
        REQUIRE(another_original.getAsPercent() == decimalApprox(fromString<PercentType>("0.80"), TEST_DEC_TOL));
    }

    SECTION("Thorough Comparison Operators")
    {
        PercentNumber<PercentType> p10 = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("10.0")); 
        PercentNumber<PercentType> p20 = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("20.0")); 
        PercentNumber<PercentType> p10_again = PercentNumber<PercentType>::createPercentNumber(fromString<PercentType>("10.0"));

        PercentType val_p10_dec = fromString<PercentType>("0.10");
        PercentType val_p20_dec = fromString<PercentType>("0.20");

        REQUIRE(p10.getAsPercent() == decimalApprox(val_p10_dec, TEST_DEC_TOL));
        REQUIRE(p20.getAsPercent() == decimalApprox(val_p20_dec, TEST_DEC_TOL));
        REQUIRE(p10_again.getAsPercent() == decimalApprox(val_p10_dec, TEST_DEC_TOL));

        // == and !=
        REQUIRE(p10 == p10_again);
        REQUIRE_FALSE(p10 != p10_again);
        REQUIRE(p10 != p20);
        REQUIRE_FALSE(p10 == p20);

        // < and <=
        REQUIRE(p10 < p20);
        REQUIRE_FALSE(p20 < p10);
        REQUIRE_FALSE(p10 < p10_again); 

        REQUIRE(p10 <= p20);
        REQUIRE(p10 <= p10_again);
        REQUIRE_FALSE(p20 <= p10);

        // > and >=
        REQUIRE(p20 > p10);
        REQUIRE_FALSE(p10 > p20);
        REQUIRE_FALSE(p10_again > p10);
        
        REQUIRE(p20 >= p10);
        REQUIRE(p10_again >= p10);
        REQUIRE_FALSE(p10 >= p20);
    }
    
    SECTION("getAsPercent functionality") //
    {
        PercentType input_val_raw = fromString<PercentType>("5.75");
        PercentNumber<PercentType> pn = PercentNumber<PercentType>::createPercentNumber(input_val_raw);
        
        PercentType expected_as_percent_val = input_val_raw / decimalOneHundred;
        REQUIRE(pn.getAsPercent() == expected_as_percent_val); 
        REQUIRE(pn.getAsPercent() == decimalApprox(fromString<PercentType>("0.0575"), TEST_DEC_TOL));

        PercentNumber<PercentType> pn_str = PercentNumber<PercentType>::createPercentNumber(std::string("12.34"));
        PercentType expected_as_percent_str_val = fromString<PercentType>("12.34") / decimalOneHundred;
        REQUIRE(pn_str.getAsPercent() == expected_as_percent_str_val);
        REQUIRE(pn_str.getAsPercent() == decimalApprox(fromString<PercentType>("0.1234"), TEST_DEC_TOL));
    }
}
