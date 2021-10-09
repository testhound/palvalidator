#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "TradingVolume.h"

using namespace mkc_timeseries;

TEST_CASE ("TradingVolume operations", "[TradingVolume]")
{
  volume_t v1 = 100000;
  volume_t v2 = 500000;
  volume_t v3 = 8100000;
  volume_t v4 = 100000;
  volume_t v5 = 500000;

  TradingVolume vol1(v1, TradingVolume::SHARES);
  TradingVolume vol2(v1, TradingVolume::CONTRACTS);
  TradingVolume vol3(v2, TradingVolume::SHARES);
  TradingVolume vol4(v2, TradingVolume::CONTRACTS);
  TradingVolume vol5(v3, TradingVolume::SHARES);
  TradingVolume vol6(v4, TradingVolume::SHARES);
  TradingVolume vol7(v5, TradingVolume::CONTRACTS);
  TradingVolume vol8(v3, TradingVolume::CONTRACTS);

  REQUIRE (vol1.getTradingVolume() == v1);
  REQUIRE (vol1.getVolumeUnits() == TradingVolume::SHARES);
  REQUIRE (vol2.getTradingVolume() == v1);
  REQUIRE (vol2.getVolumeUnits() == TradingVolume::CONTRACTS);
  REQUIRE (vol3.getTradingVolume() == v2);
  REQUIRE (vol3.getVolumeUnits() == TradingVolume::SHARES);
  REQUIRE (vol4.getTradingVolume() == v2);
  REQUIRE (vol4.getVolumeUnits() == TradingVolume::CONTRACTS);

  REQUIRE (vol5.getTradingVolume() == v3);
  REQUIRE (vol5.getVolumeUnits() == TradingVolume::SHARES);

  SECTION ("performing less than operations")
    {
      REQUIRE (vol1 < vol3);
      REQUIRE (vol3 < vol5);
      REQUIRE (vol2 < vol4);
    }


  SECTION ("performing less than or equal operations")
    {
      REQUIRE (vol1 <= vol3);
      REQUIRE (vol3 <= vol5);
      REQUIRE (vol2 <= vol4);
      REQUIRE (vol1 <= vol6);
      REQUIRE (vol4 <= vol7);
    }

  SECTION ("performing greater than operations")
    {
      REQUIRE (vol3 > vol1);
      REQUIRE (vol5 > vol3);
      REQUIRE (vol4 > vol2);
    }

  SECTION ("performing greater than or equal operations")
    {
      REQUIRE (vol3 >= vol1);
      REQUIRE (vol5 >= vol3);
      REQUIRE (vol4 >= vol2);
      REQUIRE (vol4 >= vol7);
      REQUIRE (vol6 >= vol1);
    }

  SECTION ("performing equal operations")
    {
      REQUIRE (vol1 == vol6);
      REQUIRE (vol4 == vol7);
    }

  SECTION ("performing not equal operations")
    {
      REQUIRE (vol7 != vol8);
      REQUIRE (vol5 != vol6);
    }

  SECTION ("performing expected exception operations")
    {
      REQUIRE_THROWS (vol1 < vol2);
      REQUIRE_THROWS (vol1 <= vol2);
      REQUIRE_THROWS (vol1 > vol2);
      REQUIRE_THROWS (vol1 >= vol2);
    }
}

