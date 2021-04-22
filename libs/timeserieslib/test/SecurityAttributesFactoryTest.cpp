#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../SecurityAttributesFactory.h"
#include "TestUtils.h"

using namespace mkc_timeseries;


TEST_CASE ("Security operations", "[Security]")
{
  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  SecurityAttributesFactory<DecimalType> factory;
  SecurityAttributesFactory<DecimalType>::SecurityAttributesIterator it;

  it = factory.getSecurityAttributes(equitySymbol);
  REQUIRE_FALSE (it == factory.endSecurityAttributes());
  
  auto spy = it->second;

  REQUIRE (spy->getName() == equityName);
  REQUIRE (spy->getSymbol() == equitySymbol);
  REQUIRE (spy->getBigPointValue() == DecimalConstants<DecimalType>::DecimalOne);
  REQUIRE (spy->getTick() == DecimalConstants<DecimalType>::EquityTick);
  REQUIRE (spy->isEquitySecurity());
  REQUIRE_FALSE (spy->isFuturesSecurity());

  // Futures security

  std::string futuresSymbol("@C");
  std::string futuresName("Corn Futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));
  DecimalType cornTickValue(createDecimal("0.25"));

  it = factory.getSecurityAttributes(futuresSymbol);
  REQUIRE_FALSE (it == factory.endSecurityAttributes());
  auto corn = it->second;

  REQUIRE (corn->getName() == futuresName);
  REQUIRE (corn->getSymbol() == futuresSymbol);
  REQUIRE (corn->getBigPointValue() == cornBigPointValue);
  REQUIRE (corn->getTick() == cornTickValue);
  REQUIRE_FALSE (corn->isEquitySecurity());
  REQUIRE (corn->isFuturesSecurity());
}
