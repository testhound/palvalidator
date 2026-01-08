#include <catch2/catch_test_macros.hpp>
#include "StopLoss.h"
#include "TestUtils.h"

using namespace mkc_timeseries;

TEST_CASE ("StopLoss operations", "[StopLoss]")
{
  using namespace dec;

  NullStopLoss<DecimalType> noStopLoss;
  DecimalType stop1(fromString<DecimalType>("117.4165"));
  DecimalType stop2(fromString<DecimalType>("117.3659"));
  LongStopLoss<DecimalType> longStopLoss1(stop1);
  ShortStopLoss<DecimalType> shortStopLoss1(stop2);

  SECTION ("StopLoss constructor tests 1")
  {
    REQUIRE (longStopLoss1.getStopLoss() == stop1);
    REQUIRE (shortStopLoss1.getStopLoss() == stop2);
  }

  SECTION ("StopLoss constructor tests 2")
  {
    DecimalType entry1(fromString<DecimalType>("117.00"));
    DecimalType stopReference(fromString<DecimalType>("116.5203"));

    PercentNumber<DecimalType> percStop1 = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.41"));

    LongStopLoss<DecimalType> stopPrice2 (entry1, percStop1);

    REQUIRE (stopPrice2.getStopLoss() == stopReference);
  }

  SECTION ("StopLoss constructor tests 3")
  {
    DecimalType entry1(fromString<DecimalType>("117.00"));
    DecimalType stopReference(fromString<DecimalType>("117.4797"));

    PercentNumber<DecimalType> percStop1 = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.41"));

    ShortStopLoss<DecimalType> stopPrice2 (entry1, percStop1);
    REQUIRE (stopPrice2.getStopLoss() == stopReference);
  }

  SECTION ("NullStopLoss attributes")
  {
    REQUIRE (noStopLoss.isNullStopLoss() == true);
    REQUIRE (noStopLoss.isLongStopLoss() == false);
    REQUIRE (noStopLoss.isShortStopLoss() == false);
  }

  SECTION ("LongStopLoss attributes")
  {
    REQUIRE (longStopLoss1.isNullStopLoss() == false);
    REQUIRE (longStopLoss1.isLongStopLoss() == true);
    REQUIRE (longStopLoss1.isShortStopLoss() == false);
  }

  SECTION ("ShortStopLoss attributes")
  {
    REQUIRE (shortStopLoss1.isNullStopLoss() == false);
    REQUIRE (shortStopLoss1.isLongStopLoss() == false);
    REQUIRE (shortStopLoss1.isShortStopLoss() == true);
  }

  SECTION ("LongStopLoss copy constructor")
  {
    LongStopLoss<DecimalType> original(stop1);
    LongStopLoss<DecimalType> copy(original);
    
    REQUIRE (copy.getStopLoss() == original.getStopLoss());
    REQUIRE (copy.getStopLoss() == stop1);
    REQUIRE (copy.isLongStopLoss() == true);
    REQUIRE (copy.isNullStopLoss() == false);
    REQUIRE (copy.isShortStopLoss() == false);
  }

  SECTION ("ShortStopLoss copy constructor")
  {
    ShortStopLoss<DecimalType> original(stop2);
    ShortStopLoss<DecimalType> copy(original);
    
    REQUIRE (copy.getStopLoss() == original.getStopLoss());
    REQUIRE (copy.getStopLoss() == stop2);
    REQUIRE (copy.isShortStopLoss() == true);
    REQUIRE (copy.isNullStopLoss() == false);
    REQUIRE (copy.isLongStopLoss() == false);
  }

  SECTION ("NullStopLoss copy constructor")
  {
    NullStopLoss<DecimalType> original;
    NullStopLoss<DecimalType> copy(original);
    
    REQUIRE (copy.isNullStopLoss() == true);
    REQUIRE (copy.isLongStopLoss() == false);
    REQUIRE (copy.isShortStopLoss() == false);
  }

  SECTION ("LongStopLoss assignment operator")
  {
    DecimalType stop3(fromString<DecimalType>("100.0"));
    DecimalType stop4(fromString<DecimalType>("200.0"));
    
    LongStopLoss<DecimalType> stopA(stop3);
    LongStopLoss<DecimalType> stopB(stop4);
    
    REQUIRE (stopB.getStopLoss() == stop4);
    
    stopB = stopA;
    
    REQUIRE (stopB.getStopLoss() == stopA.getStopLoss());
    REQUIRE (stopB.getStopLoss() == stop3);
    REQUIRE (stopB.isLongStopLoss() == true);
  }

  SECTION ("ShortStopLoss assignment operator")
  {
    DecimalType stop3(fromString<DecimalType>("100.0"));
    DecimalType stop4(fromString<DecimalType>("200.0"));
    
    ShortStopLoss<DecimalType> stopA(stop3);
    ShortStopLoss<DecimalType> stopB(stop4);
    
    REQUIRE (stopB.getStopLoss() == stop4);
    
    stopB = stopA;
    
    REQUIRE (stopB.getStopLoss() == stopA.getStopLoss());
    REQUIRE (stopB.getStopLoss() == stop3);
    REQUIRE (stopB.isShortStopLoss() == true);
  }

  SECTION ("NullStopLoss assignment operator")
  {
    NullStopLoss<DecimalType> stopA;
    NullStopLoss<DecimalType> stopB;
    
    stopB = stopA;
    
    REQUIRE (stopB.isNullStopLoss() == true);
    REQUIRE (stopB.isLongStopLoss() == false);
    REQUIRE (stopB.isShortStopLoss() == false);
  }

  SECTION ("LongStopLoss self-assignment")
  {
    LongStopLoss<DecimalType> stopA(stop1);
    stopA = stopA;
    
    REQUIRE (stopA.getStopLoss() == stop1);
    REQUIRE (stopA.isLongStopLoss() == true);
  }

  SECTION ("ShortStopLoss self-assignment")
  {
    ShortStopLoss<DecimalType> stopA(stop2);
    stopA = stopA;
    
    REQUIRE (stopA.getStopLoss() == stop2);
    REQUIRE (stopA.isShortStopLoss() == true);
  }

  SECTION ("NullStopLoss self-assignment")
  {
    NullStopLoss<DecimalType> stopA;
    stopA = stopA;
    
    REQUIRE (stopA.isNullStopLoss() == true);
  }

  SECTION ("Polymorphic behavior - LongStopLoss")
  {
    StopLoss<DecimalType>* pStop = &longStopLoss1;
    
    REQUIRE (pStop->getStopLoss() == stop1);
    REQUIRE (pStop->isLongStopLoss() == true);
    REQUIRE (pStop->isNullStopLoss() == false);
    REQUIRE (pStop->isShortStopLoss() == false);
  }

  SECTION ("Polymorphic behavior - ShortStopLoss")
  {
    StopLoss<DecimalType>* pStop = &shortStopLoss1;
    
    REQUIRE (pStop->getStopLoss() == stop2);
    REQUIRE (pStop->isShortStopLoss() == true);
    REQUIRE (pStop->isNullStopLoss() == false);
    REQUIRE (pStop->isLongStopLoss() == false);
  }

  SECTION ("Polymorphic behavior - NullStopLoss")
  {
    StopLoss<DecimalType>* pStop = &noStopLoss;
    
    REQUIRE (pStop->isNullStopLoss() == true);
    REQUIRE (pStop->isLongStopLoss() == false);
    REQUIRE (pStop->isShortStopLoss() == false);
  }

  SECTION ("Edge case: LongStopLoss with zero percent")
  {
    DecimalType entry(fromString<DecimalType>("100.0"));
    PercentNumber<DecimalType> zeroPercent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.0"));
    
    LongStopLoss<DecimalType> stopPrice(entry, zeroPercent);
    
    REQUIRE (stopPrice.getStopLoss() == entry);
  }

  SECTION ("Edge case: ShortStopLoss with zero percent")
  {
    DecimalType entry(fromString<DecimalType>("100.0"));
    PercentNumber<DecimalType> zeroPercent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.0"));
    
    ShortStopLoss<DecimalType> stopPrice(entry, zeroPercent);
    
    REQUIRE (stopPrice.getStopLoss() == entry);
  }

  SECTION ("Edge case: LongStopLoss with large percent")
  {
    DecimalType entry(fromString<DecimalType>("100.0"));
    DecimalType expectedStop(fromString<DecimalType>("1.0"));
    PercentNumber<DecimalType> largePercent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("99.0"));
    
    LongStopLoss<DecimalType> stopPrice(entry, largePercent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Edge case: ShortStopLoss with large percent")
  {
    DecimalType entry(fromString<DecimalType>("100.0"));
    DecimalType expectedStop(fromString<DecimalType>("199.0"));
    PercentNumber<DecimalType> largePercent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("99.0"));
    
    ShortStopLoss<DecimalType> stopPrice(entry, largePercent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Edge case: LongStopLoss with very small percent")
  {
    DecimalType entry(fromString<DecimalType>("100.0"));
    DecimalType expectedStop(fromString<DecimalType>("99.99"));
    PercentNumber<DecimalType> tinyPercent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.01"));
    
    LongStopLoss<DecimalType> stopPrice(entry, tinyPercent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Edge case: ShortStopLoss with very small percent")
  {
    DecimalType entry(fromString<DecimalType>("100.0"));
    DecimalType expectedStop(fromString<DecimalType>("100.01"));
    PercentNumber<DecimalType> tinyPercent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.01"));
    
    ShortStopLoss<DecimalType> stopPrice(entry, tinyPercent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Edge case: LongStopLoss with very large base price")
  {
    DecimalType entry(fromString<DecimalType>("10000.0"));
    DecimalType expectedStop(fromString<DecimalType>("9950.0"));
    PercentNumber<DecimalType> percent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.5"));
    
    LongStopLoss<DecimalType> stopPrice(entry, percent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Edge case: ShortStopLoss with very large base price")
  {
    DecimalType entry(fromString<DecimalType>("10000.0"));
    DecimalType expectedStop(fromString<DecimalType>("10050.0"));
    PercentNumber<DecimalType> percent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.5"));
    
    ShortStopLoss<DecimalType> stopPrice(entry, percent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Edge case: LongStopLoss with very small base price")
  {
    DecimalType entry(fromString<DecimalType>("1.0"));
    DecimalType expectedStop(fromString<DecimalType>("0.995"));
    PercentNumber<DecimalType> percent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.5"));
    
    LongStopLoss<DecimalType> stopPrice(entry, percent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Edge case: ShortStopLoss with very small base price")
  {
    DecimalType entry(fromString<DecimalType>("1.0"));
    DecimalType expectedStop(fromString<DecimalType>("1.005"));
    PercentNumber<DecimalType> percent = 
        PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.5"));
    
    ShortStopLoss<DecimalType> stopPrice(entry, percent);
    
    REQUIRE (stopPrice.getStopLoss() == expectedStop);
  }

  SECTION ("Multiple LongStopLoss instances independence")
  {
    DecimalType stop3(fromString<DecimalType>("50.0"));
    DecimalType stop4(fromString<DecimalType>("75.0"));
    
    LongStopLoss<DecimalType> stopA(stop3);
    LongStopLoss<DecimalType> stopB(stop4);
    
    REQUIRE (stopA.getStopLoss() == stop3);
    REQUIRE (stopB.getStopLoss() == stop4);
    REQUIRE (stopA.getStopLoss() != stopB.getStopLoss());
  }

  SECTION ("Multiple ShortStopLoss instances independence")
  {
    DecimalType stop3(fromString<DecimalType>("50.0"));
    DecimalType stop4(fromString<DecimalType>("75.0"));
    
    ShortStopLoss<DecimalType> stopA(stop3);
    ShortStopLoss<DecimalType> stopB(stop4);
    
    REQUIRE (stopA.getStopLoss() == stop3);
    REQUIRE (stopB.getStopLoss() == stop4);
    REQUIRE (stopA.getStopLoss() != stopB.getStopLoss());
  }
}
