#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../SecurityAttributesFactory.h"

using namespace mkc_timeseries;
typedef decimal<7> EquityType;
typedef decimal<7> DecimalType;

DecimalType
createDecimal(const std::string& valueString)
{
  return dec::fromString<DecimalType>(valueString);
}


TEST_CASE ("Security operations", "[Security]")
{
  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  SecurityAttributesFactory<7> factory;
  SecurityAttributesFactory<7>::SecurityAttributesIterator it;

  it = factory.getSecurityAttributes(equitySymbol);
  REQUIRE_FALSE (it == factory.endSecurityAttributes());
  
  auto spy = it->second;

  REQUIRE (spy->getName() == equityName);
  REQUIRE (spy->getSymbol() == equitySymbol);
  REQUIRE (spy->getBigPointValue() == DecimalConstants<7>::DecimalOne);
  REQUIRE (spy->getTick() == DecimalConstants<7>::EquityTick);
  REQUIRE (spy->isEquitySecurity());
  REQUIRE_FALSE (spy->isFuturesSecurity());

  // Futures security

  std::string futuresSymbol("@C");
  std::string futuresName("Corn Futures");
  decimal<7> cornBigPointValue(createDecimal("50.0"));
  decimal<7> cornTickValue(createDecimal("0.25"));

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
