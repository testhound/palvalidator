#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "SecurityAttributes.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("SecurityAttributesTest-Security operations", "[Security]")
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

  std::cout << "Finished testing LeverageAttributes" << std::endl;

  REQUIRE (spyAttributes.getExpenseRatio() == spyExpense);

  std::cout << "Finished getting ExpenseRatio" << std::endl;

  REQUIRE (spyAttributes.getLeverage() == spyLeverage.getLeverage());
  std::cout << "Finished getting Leverage" << std::endl;

  REQUIRE_FALSE (spyAttributes.isInverseFund());

  std::cout << "Finished getting isInverseFund" << std::endl;

  std::cout << "Finished testing SPY FundAttributes" << std::endl;

  REQUIRE (shAttributes.getExpenseRatio() == shExpense);
  REQUIRE (shAttributes.getLeverage() == shLeverage.getLeverage());
  REQUIRE (shAttributes.isInverseFund());
  
  std::cout << "Finished testing SH FundAttributes" << std::endl;

  ETFSecurityAttributes<DecimalType> spy (equitySymbol, equityName, spyAttributes,
					  spyInception);

  std::cout << "Finished creating ETFSecurityAttributes for SPY" << std::endl;

  REQUIRE (spy.getName() == equityName);
  REQUIRE (spy.getSymbol() == equitySymbol);
  REQUIRE (spy.getBigPointValue() == DecimalConstants<DecimalType>::DecimalOne);
  REQUIRE (spy.getTick() == DecimalConstants<DecimalType>::EquityTick);
  REQUIRE (spy.isEquitySecurity());
  REQUIRE_FALSE (spy.isFuturesSecurity());
  REQUIRE(spy.getInceptionDate() == spyInception);
  REQUIRE (spy.getVolumeUnits() == TradingVolume::SHARES);
  
  std::cout << "Finished SPY legacy attribute information" << std::endl;

  // Futures security

  std::cout << "Testing FuturesSecurityAttributes" << std::endl;

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
