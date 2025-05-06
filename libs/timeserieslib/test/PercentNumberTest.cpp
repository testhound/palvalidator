#include <catch2/catch_test_macros.hpp>
#include "PercentNumber.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

TEST_CASE ("PercentNumber operations", "[PercentNumber]")
{
  using namespace dec;
  typedef DecimalType PercentType;

  PercentType profitTarget (fromString<DecimalType>("0.41"));
  PercentType profitTargetAsPercent (fromString<DecimalType>("0.0041"));
  PercentType stop (fromString<DecimalType>("0.39"));
  PercentType stopAsPercent (fromString<DecimalType>("0.0039"));

  PercentNumber<DecimalType> profitTargetPercent = PercentNumber<DecimalType>::createPercentNumber (profitTarget);
  PercentNumber<DecimalType> aPercentNumber = PercentNumber<DecimalType>::createPercentNumber (std::string("0.41"));

  PercentNumber<DecimalType> stopPercent = PercentNumber<DecimalType>::createPercentNumber(stop);
  PercentNumber<DecimalType> sqrtConstants[] = 
    {
      PercentNumber<DecimalType>::createPercentNumber (std::string("0.0")),
      PercentNumber<DecimalType>::createPercentNumber (std::string("0.0"))
    };

  SECTION ("PercentNumber inequality tests");
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
