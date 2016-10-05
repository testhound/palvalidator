#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../SecurityAttributes.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

typedef decimal<7> EquityType;
typedef decimal<7> DecimalType;

date createDate (const std::string& dateString)
{
  return from_undelimited_string(dateString);
}

DecimalType
createDecimal(const std::string& valueString)
{
  return dec::fromString<DecimalType>(valueString);
}


TEST_CASE ("Security operations", "[Security]")
{
  LeverageAttributes<7> spyLeverage(createDecimal("1.0"));
  LeverageAttributes<7> shLeverage(createDecimal("-1.0"));
  date spyInception(createDate("19930122"));
  DecimalType spyExpense(createDecimal("0.09"));
  date shInception(createDate("20060619"));
  DecimalType shExpense(createDecimal("0.90"));

  FundAttributes<7> spyAttributes(spyInception,
				  spyExpense,
				  spyLeverage);

  FundAttributes<7> shAttributes(shInception,
				 shExpense,
				 shLeverage);

  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  REQUIRE (spyLeverage.getLeverage() == createDecimal("1.0"));
  REQUIRE_FALSE (spyLeverage.isInverseLeverage());

  REQUIRE (shLeverage.getLeverage() == createDecimal("-1.0"));
  REQUIRE (shLeverage.isInverseLeverage());

  std::cout << "Finished testing LeverageAttributes" << std::endl;

  std::cout << "Getting InceptionDate" << std::endl;
  REQUIRE (spyAttributes.getInceptionDate() == spyInception);

  std::cout << "Finished getting InceptionDate" << std::endl;

  REQUIRE (spyAttributes.getExpenseRatio() == spyExpense);

  std::cout << "Finished getting ExpenseRatio" << std::endl;

  REQUIRE (spyAttributes.getLeverage() == spyLeverage.getLeverage());
  std::cout << "Finished getting Leverage" << std::endl;

  REQUIRE_FALSE (spyAttributes.isInverseFund());

  std::cout << "Finished getting isInverseFund" << std::endl;

  std::cout << "Finished testing SPY FundAttributes" << std::endl;

  REQUIRE (shAttributes.getInceptionDate() == shInception);
  REQUIRE (shAttributes.getExpenseRatio() == shExpense);
  REQUIRE (shAttributes.getLeverage() == shLeverage.getLeverage());
  REQUIRE (shAttributes.isInverseFund());
  
  std::cout << "Finished testing SH FundAttributes" << std::endl;

  ETFSecurityAttributes<7> spy (equitySymbol, equityName, spyAttributes);

  std::cout << "Finished creating ETFSecurityAttributes for SPY" << std::endl;

  REQUIRE (spy.getName() == equityName);
  REQUIRE (spy.getSymbol() == equitySymbol);
  REQUIRE (spy.getBigPointValue() == DecimalConstants<7>::DecimalOne);
  REQUIRE (spy.getTick() == DecimalConstants<7>::EquityTick);
  REQUIRE (spy.isEquitySecurity());
  REQUIRE_FALSE (spy.isFuturesSecurity());
  REQUIRE (spy.getVolumeUnits() == TradingVolume::SHARES);
  
  std::cout << "Finished SPY legacy attribute information" << std::endl;

  // Futures security

  std::cout << "Testing FuturesSecurityAttributes" << std::endl;

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  decimal<7> cornBigPointValue(createDecimal("50.0"));
  decimal<7> cornTickValue(createDecimal("0.25"));


  FuturesSecurityAttributes<7> corn (futuresSymbol, futuresName, cornBigPointValue,
			   cornTickValue);

  REQUIRE (corn.getName() == futuresName);
  REQUIRE (corn.getSymbol() == futuresSymbol);
  REQUIRE (corn.getBigPointValue() == cornBigPointValue);
  REQUIRE (corn.getTick() == cornTickValue);
  REQUIRE_FALSE (corn.isEquitySecurity());
  REQUIRE (corn.isFuturesSecurity());
  REQUIRE (corn.getVolumeUnits() == TradingVolume::CONTRACTS);
}
