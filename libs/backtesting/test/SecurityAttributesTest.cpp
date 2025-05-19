#include <catch2/catch_test_macros.hpp>
#include "SecurityAttributes.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("SecurityAttributesTest-Security operations", "[SecurityAttributestTest]")
{
  LeverageAttributes<DecimalType> spyLeverage(createDecimal("1.0"));
  LeverageAttributes<DecimalType> shLeverage(createDecimal("-1.0"));
  date spyInception(createDate("19930122"));
  DecimalType spyExpense(createDecimal("0.09"));
  date shInception(createDate("20060619"));
  DecimalType shExpense(createDecimal("0.90"));

  FundAttributes<DecimalType> spyAttributes(spyExpense,
					    spyLeverage);

  FundAttributes<DecimalType> shAttributes(shExpense,
					   shLeverage);

  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  REQUIRE (spyLeverage.getLeverage() == createDecimal("1.0"));
  REQUIRE_FALSE (spyLeverage.isInverseLeverage());

  REQUIRE (shLeverage.getLeverage() == createDecimal("-1.0"));
  REQUIRE (shLeverage.isInverseLeverage());

  REQUIRE (spyAttributes.getExpenseRatio() == spyExpense);

  REQUIRE (spyAttributes.getLeverage() == spyLeverage.getLeverage());

  REQUIRE_FALSE (spyAttributes.isInverseFund());

  REQUIRE (shAttributes.getExpenseRatio() == shExpense);
  REQUIRE (shAttributes.getLeverage() == shLeverage.getLeverage());
  REQUIRE (shAttributes.isInverseFund());
  
  ETFSecurityAttributes<DecimalType> spy (equitySymbol, equityName, spyAttributes,
					  spyInception);

  REQUIRE (spy.getName() == equityName);
  REQUIRE (spy.getSymbol() == equitySymbol);
  REQUIRE (spy.getBigPointValue() == DecimalConstants<DecimalType>::DecimalOne);
  REQUIRE (spy.getTick() == DecimalConstants<DecimalType>::EquityTick);
  REQUIRE (spy.isEquitySecurity());
  REQUIRE_FALSE (spy.isFuturesSecurity());
  REQUIRE(spy.getInceptionDate() == spyInception);
  REQUIRE (spy.getVolumeUnits() == TradingVolume::SHARES);
  
  // Futures security

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));
  DecimalType cornTickValue(createDecimal("0.25"));
  date randomInception(createDate("20060619"));

  FuturesSecurityAttributes<DecimalType> corn (futuresSymbol,
					       futuresName,
					       cornBigPointValue,
					       cornTickValue,
					       randomInception);

  REQUIRE (corn.getName() == futuresName);
  REQUIRE (corn.getSymbol() == futuresSymbol);
  REQUIRE (corn.getBigPointValue() == cornBigPointValue);
  REQUIRE (corn.getTick() == cornTickValue);
  REQUIRE_FALSE (corn.isEquitySecurity());
  REQUIRE (corn.isFuturesSecurity());
  REQUIRE(corn.getInceptionDate() == randomInception);
  REQUIRE (corn.getVolumeUnits() == TradingVolume::CONTRACTS);
}

//-----------------------------------------------------------------------------
// Test comparison operators for LeverageAttributes
TEST_CASE("LeverageAttributes-ComparisonOperators", "[LeverageAttributes]") {
    LeverageAttributes<DecimalType> la1(createDecimal("2.5"));
    LeverageAttributes<DecimalType> la2(createDecimal("2.5"));
    LeverageAttributes<DecimalType> la3(createDecimal("-2.5"));

    REQUIRE(la1 == la2);
    REQUIRE_FALSE(la1 != la2);

    REQUIRE_FALSE(la1 == la3);
    REQUIRE(la1 != la3);
}

//-----------------------------------------------------------------------------
// Extended tests for ETFSecurityAttributes (ETF-specific behaviors)
TEST_CASE("ETFSecurityAttributes-IdentityAndFundChecks", "[ETFSecurityAttributes]") {
    // Setup a positive-leverage ETF
    LeverageAttributes<DecimalType> leveragePos(createDecimal("1.0"));
    date inceptionPos(createDate("20200101"));
    FundAttributes<DecimalType> fundPos(createDecimal("0.15"), leveragePos);
    ETFSecurityAttributes<DecimalType> etfPos(
        "IVV", "iShares Core S&P 500 ETF", fundPos, inceptionPos);

    // Basic identity
    REQUIRE(etfPos.getSymbol() == "IVV");
    REQUIRE(etfPos.getName()   == "iShares Core S&P 500 ETF");

    // Fund attributes
    REQUIRE(etfPos.getExpenseRatio() == createDecimal("0.15"));
    REQUIRE(etfPos.getLeverage()      == createDecimal("1.0"));
    REQUIRE_FALSE(etfPos.isInverseFund());

    // Classification
    REQUIRE(etfPos.isETF());
    REQUIRE_FALSE(etfPos.isMutualFund());
    REQUIRE(etfPos.isFund());
    REQUIRE_FALSE(etfPos.isCommonStock());
}

//-----------------------------------------------------------------------------
// Tests for CommonStockSecurityAttributes
TEST_CASE("CommonStockSecurityAttributes-Basics", "[CommonStockSecurityAttributes]") {
    date inception(createDate("19950115"));
    CommonStockSecurityAttributes<DecimalType> cs(
        "AAPL", "Apple Inc.", inception);

    REQUIRE(cs.getSymbol() == "AAPL");
    REQUIRE(cs.getName()   == "Apple Inc.");

    // Legacy equity attributes
    REQUIRE(cs.getBigPointValue() == DecimalConstants<DecimalType>::DecimalOne);
    REQUIRE(cs.getTick()         == DecimalConstants<DecimalType>::EquityTick);
    REQUIRE(cs.getInceptionDate() == inception);

    // Security classification
    REQUIRE(cs.isEquitySecurity());
    REQUIRE_FALSE(cs.isFuturesSecurity());
    REQUIRE(cs.isCommonStock());
    REQUIRE_FALSE(cs.isFund());

    // Volume units
    REQUIRE(cs.getVolumeUnits() == TradingVolume::SHARES);
}

//-----------------------------------------------------------------------------
// Additional tests for FuturesSecurityAttributes
TEST_CASE("FuturesSecurityAttributes-CommonChecks", "[FuturesSecurityAttributes]") {
    date inception(createDate("20150310"));
    FuturesSecurityAttributes<DecimalType> fut(
        "ES", "E-mini S&P 500", createDecimal("50.0"), createDecimal("0.25"), inception);

    REQUIRE(fut.getSymbol() == "ES");
    REQUIRE(fut.getName()   == "E-mini S&P 500");

    // Price unit tests
    REQUIRE(fut.getBigPointValue() == createDecimal("50.0"));
    REQUIRE(fut.getTick()          == createDecimal("0.25"));
    REQUIRE(fut.getInceptionDate() == inception);

    // Classification
    REQUIRE_FALSE(fut.isEquitySecurity());
    REQUIRE(fut.isFuturesSecurity());
    REQUIRE_FALSE(fut.isCommonStock());
    REQUIRE_FALSE(fut.isFund());

    // Volume units
    REQUIRE(fut.getVolumeUnits() == TradingVolume::CONTRACTS);
}
