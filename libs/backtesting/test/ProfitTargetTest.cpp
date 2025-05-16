#include <catch2/catch_test_macros.hpp>
#include "ProfitTarget.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

TEST_CASE ("ProfitTarget operations", "[ProfitTarget]")
{
  using namespace dec;

  NullProfitTarget<DecimalType> noProfitTarget;
  DecimalType target1(fromString<DecimalType>("117.4165"));
  DecimalType target2(fromString<DecimalType>("117.3659"));
  LongProfitTarget<DecimalType> longProfitTarget1(target1);
  ShortProfitTarget<DecimalType> shortProfitTarget1(target2);

  SECTION ("ProfitTarget constructor tests 1");
  {
    REQUIRE (longProfitTarget1.getProfitTarget() == target1);
    REQUIRE (shortProfitTarget1.getProfitTarget() == target2);
    REQUIRE_THROWS (noProfitTarget.getProfitTarget());
  }

  SECTION ("ProfitTarget constructor tests 2");
  {
    DecimalType entry1(fromString<DecimalType>("117.00"));
    DecimalType targetReference(fromString<DecimalType>("117.4797"));

    PercentNumber<DecimalType> percTarget1 = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.41"));

    LongProfitTarget<DecimalType> targetPrice2 (entry1, percTarget1);

    REQUIRE (targetPrice2.getProfitTarget() == targetReference);
  }

  SECTION ("ProfitTarget constructor tests 3");
  {
    DecimalType entry1(fromString<DecimalType>("117.00"));
    DecimalType targetReference(fromString<DecimalType>("116.5203"));

    PercentNumber<DecimalType> percTarget1 = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.41"));

    ShortProfitTarget<DecimalType> targetPrice2 (entry1, percTarget1);
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

