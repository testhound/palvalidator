#include <catch2/catch_test_macros.hpp>
#include "TradingVolume.h"
#include <utility>

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

TEST_CASE ("TradingVolume copy constructor", "[TradingVolume][copy]")
{
  SECTION ("copying SHARES volume")
    {
      TradingVolume original(250000, TradingVolume::SHARES);
      TradingVolume copy(original);
      
      REQUIRE (copy.getTradingVolume() == 250000);
      REQUIRE (copy.getVolumeUnits() == TradingVolume::SHARES);
      REQUIRE (copy == original);
    }

  SECTION ("copying CONTRACTS volume")
    {
      TradingVolume original(150000, TradingVolume::CONTRACTS);
      TradingVolume copy(original);
      
      REQUIRE (copy.getTradingVolume() == 150000);
      REQUIRE (copy.getVolumeUnits() == TradingVolume::CONTRACTS);
      REQUIRE (copy == original);
    }
}

TEST_CASE ("TradingVolume copy assignment operator", "[TradingVolume][copy]")
{
  SECTION ("assigning SHARES volume")
    {
      TradingVolume original(350000, TradingVolume::SHARES);
      TradingVolume assigned(100000, TradingVolume::SHARES);
      
      assigned = original;
      
      REQUIRE (assigned.getTradingVolume() == 350000);
      REQUIRE (assigned.getVolumeUnits() == TradingVolume::SHARES);
      REQUIRE (assigned == original);
    }

  SECTION ("assigning CONTRACTS volume")
    {
      TradingVolume original(450000, TradingVolume::CONTRACTS);
      TradingVolume assigned(200000, TradingVolume::CONTRACTS);
      
      assigned = original;
      
      REQUIRE (assigned.getTradingVolume() == 450000);
      REQUIRE (assigned.getVolumeUnits() == TradingVolume::CONTRACTS);
      REQUIRE (assigned == original);
    }

  SECTION ("self assignment")
    {
      TradingVolume vol(300000, TradingVolume::SHARES);
      vol = vol;
      
      REQUIRE (vol.getTradingVolume() == 300000);
      REQUIRE (vol.getVolumeUnits() == TradingVolume::SHARES);
    }

  SECTION ("changing unit type through assignment")
    {
      TradingVolume shares(100000, TradingVolume::SHARES);
      TradingVolume contracts(200000, TradingVolume::CONTRACTS);
      
      shares = contracts;
      
      REQUIRE (shares.getTradingVolume() == 200000);
      REQUIRE (shares.getVolumeUnits() == TradingVolume::CONTRACTS);
    }
}

TEST_CASE ("TradingVolume move constructor", "[TradingVolume][move]")
{
  SECTION ("moving SHARES volume")
    {
      TradingVolume original(550000, TradingVolume::SHARES);
      TradingVolume moved(std::move(original));
      
      REQUIRE (moved.getTradingVolume() == 550000);
      REQUIRE (moved.getVolumeUnits() == TradingVolume::SHARES);
    }

  SECTION ("moving CONTRACTS volume")
    {
      TradingVolume original(750000, TradingVolume::CONTRACTS);
      TradingVolume moved(std::move(original));
      
      REQUIRE (moved.getTradingVolume() == 750000);
      REQUIRE (moved.getVolumeUnits() == TradingVolume::CONTRACTS);
    }
}

TEST_CASE ("TradingVolume move assignment operator", "[TradingVolume][move]")
{
  SECTION ("move assigning SHARES volume")
    {
      TradingVolume original(650000, TradingVolume::SHARES);
      TradingVolume assigned(100000, TradingVolume::SHARES);
      
      assigned = std::move(original);
      
      REQUIRE (assigned.getTradingVolume() == 650000);
      REQUIRE (assigned.getVolumeUnits() == TradingVolume::SHARES);
    }

  SECTION ("move assigning CONTRACTS volume")
    {
      TradingVolume original(850000, TradingVolume::CONTRACTS);
      TradingVolume assigned(200000, TradingVolume::CONTRACTS);
      
      assigned = std::move(original);
      
      REQUIRE (assigned.getTradingVolume() == 850000);
      REQUIRE (assigned.getVolumeUnits() == TradingVolume::CONTRACTS);
    }

  SECTION ("self move assignment")
    {
      TradingVolume vol(400000, TradingVolume::SHARES);
      vol = std::move(vol);
      
      REQUIRE (vol.getTradingVolume() == 400000);
      REQUIRE (vol.getVolumeUnits() == TradingVolume::SHARES);
    }
}

TEST_CASE ("TradingVolume edge cases", "[TradingVolume][edge]")
{
  SECTION ("zero volume SHARES")
    {
      TradingVolume zero(0, TradingVolume::SHARES);
      
      REQUIRE (zero.getTradingVolume() == 0);
      REQUIRE (zero.getVolumeUnits() == TradingVolume::SHARES);
    }

  SECTION ("zero volume CONTRACTS")
    {
      TradingVolume zero(0, TradingVolume::CONTRACTS);
      
      REQUIRE (zero.getTradingVolume() == 0);
      REQUIRE (zero.getVolumeUnits() == TradingVolume::CONTRACTS);
    }

  SECTION ("maximum volume value")
    {
      volume_t maxVol = 18446744073709551615ULL; // max unsigned long long
      TradingVolume maxVolume(maxVol, TradingVolume::SHARES);
      
      REQUIRE (maxVolume.getTradingVolume() == maxVol);
      REQUIRE (maxVolume.getVolumeUnits() == TradingVolume::SHARES);
    }

  SECTION ("comparing zero volumes")
    {
      TradingVolume zero1(0, TradingVolume::SHARES);
      TradingVolume zero2(0, TradingVolume::SHARES);
      
      REQUIRE (zero1 == zero2);
      REQUIRE (zero1 <= zero2);
      REQUIRE (zero1 >= zero2);
      REQUIRE_FALSE (zero1 < zero2);
      REQUIRE_FALSE (zero1 > zero2);
    }

  SECTION ("zero vs non-zero")
    {
      TradingVolume zero(0, TradingVolume::SHARES);
      TradingVolume nonZero(100, TradingVolume::SHARES);
      
      REQUIRE (zero < nonZero);
      REQUIRE (nonZero > zero);
      REQUIRE (zero != nonZero);
    }
}

TEST_CASE ("TradingVolume inequality with different units", "[TradingVolume][units]")
{
  SECTION ("equality returns false for different units even with same volume")
    {
      TradingVolume shares(100000, TradingVolume::SHARES);
      TradingVolume contracts(100000, TradingVolume::CONTRACTS);
      
      REQUIRE_FALSE (shares == contracts);
      REQUIRE (shares != contracts);
    }

  SECTION ("equality returns false for different units and different volumes")
    {
      TradingVolume shares(100000, TradingVolume::SHARES);
      TradingVolume contracts(200000, TradingVolume::CONTRACTS);
      
      REQUIRE_FALSE (shares == contracts);
      REQUIRE (shares != contracts);
    }
}

TEST_CASE ("TradingVolume comparison reflexivity and symmetry", "[TradingVolume][properties]")
{
  TradingVolume vol1(100000, TradingVolume::SHARES);
  TradingVolume vol2(200000, TradingVolume::SHARES);
  TradingVolume vol3(100000, TradingVolume::SHARES);
  
  SECTION ("reflexivity of equality")
    {
      REQUIRE (vol1 == vol1);
    }

  SECTION ("symmetry of equality")
    {
      REQUIRE (vol1 == vol3);
      REQUIRE (vol3 == vol1);
    }

  SECTION ("transitivity of less than")
    {
      TradingVolume vol4(300000, TradingVolume::SHARES);
      REQUIRE (vol1 < vol2);
      REQUIRE (vol2 < vol4);
      REQUIRE (vol1 < vol4);
    }

  SECTION ("antisymmetry of less than")
    {
      REQUIRE (vol1 < vol2);
      REQUIRE_FALSE (vol2 < vol1);
    }
}
