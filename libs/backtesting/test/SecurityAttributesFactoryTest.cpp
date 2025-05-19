#include <catch2/catch_test_macros.hpp>
#include "SecurityAttributesFactory.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("SecurityAttributesFactory operations", "[SecurityAttributesFactory]")
{
  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  auto& factory = SecurityAttributesFactory<DecimalType>::instance();

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

TEST_CASE("SecurityAttributesFactory-CommonStockSymbol", "[SecurityAttributesFactory]") {
    auto& factory = SecurityAttributesFactory<DecimalType>::instance();
    auto it = factory.getSecurityAttributes("AAPL");
    REQUIRE(it != factory.endSecurityAttributes());
    auto attrs = it->second;

    REQUIRE(attrs->getSymbol() == "AAPL");
    REQUIRE(attrs->getName()   == "Apple");
    REQUIRE(attrs->isEquitySecurity());
    REQUIRE_FALSE(attrs->isFuturesSecurity());
    REQUIRE(attrs->isCommonStock());
    REQUIRE_FALSE(attrs->isFund());
    REQUIRE(attrs->getVolumeUnits() == TradingVolume::SHARES);
    REQUIRE(attrs->getInceptionDate() == createDate("19801212"));
}

TEST_CASE("SecurityAttributesFactory-ETFClassification", "[SecurityAttributesFactory]") {
    auto& factory = SecurityAttributesFactory<DecimalType>::instance();
    auto it = factory.getSecurityAttributes("QQQ");
    REQUIRE(it != factory.endSecurityAttributes());
    auto attrs = it->second;

    REQUIRE(attrs->getSymbol() == "QQQ");
    REQUIRE(attrs->getName()   == "PowerShares QQQ ETF");
    REQUIRE(attrs->isEquitySecurity());
    REQUIRE_FALSE(attrs->isFuturesSecurity());
    REQUIRE_FALSE(attrs->isCommonStock());
    REQUIRE(attrs->isFund());
    REQUIRE(attrs->getVolumeUnits() == TradingVolume::SHARES);
    REQUIRE(attrs->getInceptionDate() == createDate("19990310"));
}

TEST_CASE("SecurityAttributesFactory-FuturesClassification", "[SecurityAttributesFactory]") {
    auto& factory = SecurityAttributesFactory<DecimalType>::instance();
    std::string symbol = "@CL";
    auto it = factory.getSecurityAttributes(symbol);
    REQUIRE(it != factory.endSecurityAttributes());
    auto attrs = it->second;

    REQUIRE(attrs->getSymbol() == symbol);
    REQUIRE(attrs->getName()   == "Crude Oil Futures");
    REQUIRE_FALSE(attrs->isEquitySecurity());
    REQUIRE(attrs->isFuturesSecurity());
    REQUIRE_FALSE(attrs->isCommonStock());
    REQUIRE_FALSE(attrs->isFund());
    REQUIRE(attrs->getVolumeUnits() == TradingVolume::CONTRACTS);
    REQUIRE(attrs->getBigPointValue() == createDecimal("1000.0"));
    REQUIRE(attrs->getTick()           == createDecimal("0.01"));
}

TEST_CASE("SecurityAttributesFactory-UnknownSymbolIterator", "[SecurityAttributesFactory]") {
    auto& factory = SecurityAttributesFactory<DecimalType>::instance();
    auto it = factory.getSecurityAttributes("UNKNOWN");
    REQUIRE(it == factory.endSecurityAttributes());
}

TEST_CASE("getSecurityAttributes-FreeFunctionThrows", "[SecurityAttributesFactory]") {
    REQUIRE_THROWS_AS(getSecurityAttributes<DecimalType>("UNKNOWN"), SecurtyAttributesFactoryException);
}

TEST_CASE("SecurityAttributesFactory-Singleton", "[SecurityAttributesFactory]") {
    auto* f1 = &SecurityAttributesFactory<DecimalType>::instance();
    auto* f2 = &SecurityAttributesFactory<DecimalType>::instance();
    REQUIRE(f1 == f2);
}

TEST_CASE("SecurityAttributesFactory-BeginEndIterators", "[SecurityAttributesFactory]")
{
    auto& factory = SecurityAttributesFactory<DecimalType>::instance();
    auto begin = factory.beginSecurityAttributes();
    auto end   = factory.endSecurityAttributes();

    // There should be at least one entry
    REQUIRE(begin != end);

    std::vector<std::string> symbols;
    for (auto it = begin; it != end; ++it) {
        symbols.push_back(it->first);
    }

    // Verify that a well-known symbol appears in the iteration
    REQUIRE(std::find(symbols.begin(), symbols.end(), std::string("SPY")) != symbols.end());
}
