#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../PercentNumber.h"

using namespace mkc_timeseries;

TEST_CASE ("PercentNumber operations", "[PercentNumber]")
{
  using namespace dec;
  typedef decimal<4> PercentType;

  PercentType profitTarget (fromString<decimal<4>>("0.41"));
  PercentType profitTargetAsPercent (fromString<decimal<4>>("0.0041"));
  PercentType stop (fromString<decimal<4>>("0.39"));
  PercentType stopAsPercent (fromString<decimal<4>>("0.0039"));

  PercentNumber<4> profitTargetPercent = PercentNumber<4>::createPercentNumber (profitTarget);
  PercentNumber<4> aPercentNumber = PercentNumber<4>::createPercentNumber (std::string("0.41"));

  PercentNumber<4> stopPercent = PercentNumber<4>::createPercentNumber(stop);
  PercentNumber<4> sqrtConstants[] = 
    {
      PercentNumber<4>::createPercentNumber (std::string("0.0")),
      PercentNumber<4>::createPercentNumber (std::string("0.0"))
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
