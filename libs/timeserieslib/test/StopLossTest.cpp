#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../StopLoss.h"

using namespace mkc_timeseries;

TEST_CASE ("ProfitTarget operations", "[ProfitTarget]")
{
  using namespace dec;

  NullStopLoss<7> noStopLoss;
  decimal<7> stop1(fromString<decimal<7>>("117.4165"));
  decimal<7> stop2(fromString<decimal<7>>("117.3659"));
  LongStopLoss<7> longStopLoss1(stop1);
  ShortStopLoss<7> shortStopLoss1(stop2);

  SECTION ("StopLoss constructor tests 1");
  {
    REQUIRE (longStopLoss1.getStopLoss() == stop1);
    REQUIRE (shortStopLoss1.getStopLoss() == stop2);
    REQUIRE_THROWS (noStopLoss.getStopLoss());
  }

  SECTION ("StopLoss constructor tests 2");
  {
    decimal<7> entry1(fromString<decimal<7>>("117.00"));
    decimal<7> stopReference(fromString<decimal<7>>("116.5203"));

    PercentNumber<7> percStop1 = PercentNumber<7>::createPercentNumber(fromString<decimal<7>>("0.41"));

    LongStopLoss<7> stopPrice2 (entry1, percStop1);

    REQUIRE (stopPrice2.getStopLoss() == stopReference);
  }

  SECTION ("StopLoss constructor tests 3");
  {
    decimal<7> entry1(fromString<decimal<7>>("117.00"));
    decimal<7> stopReference(fromString<decimal<7>>("117.4797"));

    PercentNumber<7> percStop1 = PercentNumber<7>::createPercentNumber(fromString<decimal<7>>("0.41"));

    ShortStopLoss<7> stopPrice2 (entry1, percStop1);
    REQUIRE (stopPrice2.getStopLoss() == stopReference);
  }

  SECTION ("NullStopLoss attributes");
  {
    REQUIRE (noStopLoss.isNullStopLoss() == true);
    REQUIRE (noStopLoss.isLongStopLoss() == false);
    REQUIRE (noStopLoss.isShortStopLoss() == false);
  }

  SECTION ("LongStopLoss attributes");
  {
    REQUIRE (longStopLoss1.isNullStopLoss() == false);
    REQUIRE (longStopLoss1.isLongStopLoss() == true);
    REQUIRE (longStopLoss1.isShortStopLoss() == false);
  }

  SECTION ("ShortStopLoss attributes");
  {
    REQUIRE (shortStopLoss1.isNullStopLoss() == false);
    REQUIRE (shortStopLoss1.isLongStopLoss() == false);
    REQUIRE (shortStopLoss1.isShortStopLoss() == true);
  }
}

