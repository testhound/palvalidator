#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../ProfitTarget.h"

using namespace mkc_timeseries;

TEST_CASE ("ProfitTarget operations", "[ProfitTarget]")
{
  using namespace dec;

  NullProfitTarget<7> noProfitTarget;
  decimal<7> target1(fromString<decimal<7>>("117.4165"));
  decimal<7> target2(fromString<decimal<7>>("117.3659"));
  LongProfitTarget<7> longProfitTarget1(target1);
  ShortProfitTarget<7> shortProfitTarget1(target2);

  SECTION ("ProfitTarget constructor tests 1");
  {
    REQUIRE (longProfitTarget1.getProfitTarget() == target1);
    REQUIRE (shortProfitTarget1.getProfitTarget() == target2);
    REQUIRE_THROWS (noProfitTarget.getProfitTarget());
  }

  SECTION ("ProfitTarget constructor tests 2");
  {
    decimal<7> entry1(fromString<decimal<7>>("117.00"));
    decimal<7> targetReference(fromString<decimal<7>>("117.4797"));

    PercentNumber<7> percTarget1 = PercentNumber<7>::createPercentNumber(fromString<decimal<7>>("0.41"));

    LongProfitTarget<7> targetPrice2 (entry1, percTarget1);

    REQUIRE (targetPrice2.getProfitTarget() == targetReference);
  }

  SECTION ("ProfitTarget constructor tests 3");
  {
    decimal<7> entry1(fromString<decimal<7>>("117.00"));
    decimal<7> targetReference(fromString<decimal<7>>("116.5203"));

    PercentNumber<7> percTarget1 = PercentNumber<7>::createPercentNumber(fromString<decimal<7>>("0.41"));

    ShortProfitTarget<7> targetPrice2 (entry1, percTarget1);
    REQUIRE (targetPrice2.getProfitTarget() == targetReference);
  }

  SECTION ("NullProfitTarget attributes");
  {
    REQUIRE (noProfitTarget.isNullProfitTarget() == true);
    REQUIRE (noProfitTarget.isLongProfitTarget() == false);
    REQUIRE (noProfitTarget.isShortProfitTarget() == false);
  }

  SECTION ("LongProfitTarget attributes");
  {
    REQUIRE (longProfitTarget1.isNullProfitTarget() == false);
    REQUIRE (longProfitTarget1.isLongProfitTarget() == true);
    REQUIRE (longProfitTarget1.isShortProfitTarget() == false);
  }

  SECTION ("ShortProfitTarget attributes");
  {
    REQUIRE (shortProfitTarget1.isNullProfitTarget() == false);
    REQUIRE (shortProfitTarget1.isLongProfitTarget() == false);
    REQUIRE (shortProfitTarget1.isShortProfitTarget() == true);
  }
}

