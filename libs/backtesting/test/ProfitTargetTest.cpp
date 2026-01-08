#include <catch2/catch_test_macros.hpp>
#include "ProfitTarget.h"
#include "TestUtils.h"
#include <memory>

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

TEST_CASE ("LongProfitTarget copy constructor", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Copy constructor copies target value correctly")
  {
    DecimalType target(fromString<DecimalType>("125.50"));
    LongProfitTarget<DecimalType> original(target);
    LongProfitTarget<DecimalType> copy(original);

    REQUIRE (copy.getProfitTarget() == target);
    REQUIRE (copy.getProfitTarget() == original.getProfitTarget());
    REQUIRE (copy.isLongProfitTarget() == true);
    REQUIRE (copy.isNullProfitTarget() == false);
    REQUIRE (copy.isShortProfitTarget() == false);
  }

  SECTION ("Copy constructor with percent-based target")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> percent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("5.0"));
    
    LongProfitTarget<DecimalType> original(basePrice, percent);
    LongProfitTarget<DecimalType> copy(original);

    DecimalType expectedTarget(fromString<DecimalType>("105.00"));
    REQUIRE (copy.getProfitTarget() == expectedTarget);
    REQUIRE (copy.getProfitTarget() == original.getProfitTarget());
  }
}

TEST_CASE ("ShortProfitTarget copy constructor", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Copy constructor copies target value correctly")
  {
    DecimalType target(fromString<DecimalType>("95.75"));
    ShortProfitTarget<DecimalType> original(target);
    ShortProfitTarget<DecimalType> copy(original);

    REQUIRE (copy.getProfitTarget() == target);
    REQUIRE (copy.getProfitTarget() == original.getProfitTarget());
    REQUIRE (copy.isShortProfitTarget() == true);
    REQUIRE (copy.isNullProfitTarget() == false);
    REQUIRE (copy.isLongProfitTarget() == false);
  }

  SECTION ("Copy constructor with percent-based target")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> percent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("3.0"));
    
    ShortProfitTarget<DecimalType> original(basePrice, percent);
    ShortProfitTarget<DecimalType> copy(original);

    DecimalType expectedTarget(fromString<DecimalType>("97.00"));
    REQUIRE (copy.getProfitTarget() == expectedTarget);
    REQUIRE (copy.getProfitTarget() == original.getProfitTarget());
  }
}

TEST_CASE ("NullProfitTarget copy constructor", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Copy constructor maintains null behavior")
  {
    NullProfitTarget<DecimalType> original;
    NullProfitTarget<DecimalType> copy(original);

    REQUIRE (copy.isNullProfitTarget() == true);
    REQUIRE (copy.isLongProfitTarget() == false);
    REQUIRE (copy.isShortProfitTarget() == false);
  }
}

TEST_CASE ("LongProfitTarget assignment operator", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Assignment operator copies target value correctly")
  {
    DecimalType target1(fromString<DecimalType>("120.00"));
    DecimalType target2(fromString<DecimalType>("130.00"));
    
    LongProfitTarget<DecimalType> pt1(target1);
    LongProfitTarget<DecimalType> pt2(target2);

    pt2 = pt1;

    REQUIRE (pt2.getProfitTarget() == target1);
    REQUIRE (pt2.getProfitTarget() == pt1.getProfitTarget());
  }

  SECTION ("Self-assignment does nothing")
  {
    DecimalType target(fromString<DecimalType>("125.50"));
    LongProfitTarget<DecimalType> pt(target);

    pt = pt;

    REQUIRE (pt.getProfitTarget() == target);
    REQUIRE (pt.isLongProfitTarget() == true);
  }

  SECTION ("Chain assignment")
  {
    DecimalType target1(fromString<DecimalType>("100.00"));
    DecimalType target2(fromString<DecimalType>("110.00"));
    DecimalType target3(fromString<DecimalType>("120.00"));
    
    LongProfitTarget<DecimalType> pt1(target1);
    LongProfitTarget<DecimalType> pt2(target2);
    LongProfitTarget<DecimalType> pt3(target3);

    pt3 = pt2 = pt1;

    REQUIRE (pt1.getProfitTarget() == target1);
    REQUIRE (pt2.getProfitTarget() == target1);
    REQUIRE (pt3.getProfitTarget() == target1);
  }
}

TEST_CASE ("ShortProfitTarget assignment operator", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Assignment operator copies target value correctly")
  {
    DecimalType target1(fromString<DecimalType>("95.00"));
    DecimalType target2(fromString<DecimalType>("90.00"));
    
    ShortProfitTarget<DecimalType> pt1(target1);
    ShortProfitTarget<DecimalType> pt2(target2);

    pt2 = pt1;

    REQUIRE (pt2.getProfitTarget() == target1);
    REQUIRE (pt2.getProfitTarget() == pt1.getProfitTarget());
  }

  SECTION ("Self-assignment does nothing")
  {
    DecimalType target(fromString<DecimalType>("92.50"));
    ShortProfitTarget<DecimalType> pt(target);

    pt = pt;

    REQUIRE (pt.getProfitTarget() == target);
    REQUIRE (pt.isShortProfitTarget() == true);
  }

  SECTION ("Chain assignment")
  {
    DecimalType target1(fromString<DecimalType>("100.00"));
    DecimalType target2(fromString<DecimalType>("95.00"));
    DecimalType target3(fromString<DecimalType>("90.00"));
    
    ShortProfitTarget<DecimalType> pt1(target1);
    ShortProfitTarget<DecimalType> pt2(target2);
    ShortProfitTarget<DecimalType> pt3(target3);

    pt3 = pt2 = pt1;

    REQUIRE (pt1.getProfitTarget() == target1);
    REQUIRE (pt2.getProfitTarget() == target1);
    REQUIRE (pt3.getProfitTarget() == target1);
  }
}

TEST_CASE ("NullProfitTarget assignment operator", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Assignment operator maintains null behavior")
  {
    NullProfitTarget<DecimalType> pt1;
    NullProfitTarget<DecimalType> pt2;

    pt2 = pt1;

    REQUIRE (pt2.isNullProfitTarget() == true);
    REQUIRE (pt2.isLongProfitTarget() == false);
    REQUIRE (pt2.isShortProfitTarget() == false);
  }

  SECTION ("Self-assignment does nothing")
  {
    NullProfitTarget<DecimalType> pt;

    pt = pt;

    REQUIRE (pt.isNullProfitTarget() == true);
  }
}

TEST_CASE ("Percent-based calculation edge cases", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Long target with zero percent")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> zeroPercent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.0"));
    
    LongProfitTarget<DecimalType> pt(basePrice, zeroPercent);

    REQUIRE (pt.getProfitTarget() == basePrice);
  }

  SECTION ("Short target with zero percent")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> zeroPercent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.0"));
    
    ShortProfitTarget<DecimalType> pt(basePrice, zeroPercent);

    REQUIRE (pt.getProfitTarget() == basePrice);
  }

  SECTION ("Long target with large percent")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> largePercent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("50.0"));
    
    LongProfitTarget<DecimalType> pt(basePrice, largePercent);

    DecimalType expectedTarget(fromString<DecimalType>("150.00"));
    REQUIRE (pt.getProfitTarget() == expectedTarget);
  }

  SECTION ("Short target with large percent")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> largePercent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("25.0"));
    
    ShortProfitTarget<DecimalType> pt(basePrice, largePercent);

    DecimalType expectedTarget(fromString<DecimalType>("75.00"));
    REQUIRE (pt.getProfitTarget() == expectedTarget);
  }

  SECTION ("Long target with small fractional percent")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> smallPercent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.01"));
    
    LongProfitTarget<DecimalType> pt(basePrice, smallPercent);

    DecimalType expectedTarget(fromString<DecimalType>("100.01"));
    REQUIRE (pt.getProfitTarget() == expectedTarget);
  }

  SECTION ("Short target with small fractional percent")
  {
    DecimalType basePrice(fromString<DecimalType>("100.00"));
    PercentNumber<DecimalType> smallPercent = PercentNumber<DecimalType>::createPercentNumber(fromString<DecimalType>("0.01"));
    
    ShortProfitTarget<DecimalType> pt(basePrice, smallPercent);

    DecimalType expectedTarget(fromString<DecimalType>("99.99"));
    REQUIRE (pt.getProfitTarget() == expectedTarget);
  }
}

TEST_CASE ("Polymorphic behavior through base class pointer", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Long profit target through base pointer")
  {
    DecimalType target(fromString<DecimalType>("125.00"));
    std::unique_ptr<ProfitTarget<DecimalType>> ptr(new LongProfitTarget<DecimalType>(target));

    REQUIRE (ptr->getProfitTarget() == target);
    REQUIRE (ptr->isLongProfitTarget() == true);
    REQUIRE (ptr->isNullProfitTarget() == false);
    REQUIRE (ptr->isShortProfitTarget() == false);
  }

  SECTION ("Short profit target through base pointer")
  {
    DecimalType target(fromString<DecimalType>("95.00"));
    std::unique_ptr<ProfitTarget<DecimalType>> ptr(new ShortProfitTarget<DecimalType>(target));

    REQUIRE (ptr->getProfitTarget() == target);
    REQUIRE (ptr->isShortProfitTarget() == true);
    REQUIRE (ptr->isNullProfitTarget() == false);
    REQUIRE (ptr->isLongProfitTarget() == false);
  }

  SECTION ("Null profit target through base pointer")
  {
    std::unique_ptr<ProfitTarget<DecimalType>> ptr(new NullProfitTarget<DecimalType>());

    REQUIRE (ptr->isNullProfitTarget() == true);
    REQUIRE (ptr->isLongProfitTarget() == false);
    REQUIRE (ptr->isShortProfitTarget() == false);
  }
}

TEST_CASE ("Const correctness", "[ProfitTarget]")
{
  using namespace dec;

  SECTION ("Const LongProfitTarget")
  {
    DecimalType target(fromString<DecimalType>("120.00"));
    const LongProfitTarget<DecimalType> pt(target);

    REQUIRE (pt.getProfitTarget() == target);
    REQUIRE (pt.isLongProfitTarget() == true);
    REQUIRE (pt.isNullProfitTarget() == false);
    REQUIRE (pt.isShortProfitTarget() == false);
  }

  SECTION ("Const ShortProfitTarget")
  {
    DecimalType target(fromString<DecimalType>("95.00"));
    const ShortProfitTarget<DecimalType> pt(target);

    REQUIRE (pt.getProfitTarget() == target);
    REQUIRE (pt.isShortProfitTarget() == true);
    REQUIRE (pt.isNullProfitTarget() == false);
    REQUIRE (pt.isLongProfitTarget() == false);
  }

  SECTION ("Const NullProfitTarget")
  {
    const NullProfitTarget<DecimalType> pt;

    REQUIRE (pt.isNullProfitTarget() == true);
    REQUIRE (pt.isLongProfitTarget() == false);
    REQUIRE (pt.isShortProfitTarget() == false);
  }
}
