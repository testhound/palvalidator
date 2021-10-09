#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "Portfolio.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;


std::shared_ptr<OHLCTimeSeriesEntry<DecimalType>>
    createEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    return createTimeSeriesEntry (dateString, openPrice, highPrice, lowPrice, closePrice, vol);
  }



TEST_CASE ("Security operations", "[Security]")
{
  auto entry0 = createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry1 = createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry2 = createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry3 = createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry4 = createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry6 = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);

  spySeries->addEntry (*entry4);
  spySeries->addEntry (*entry6);
  spySeries->addEntry (*entry2);
  spySeries->addEntry (*entry3);
  spySeries->addEntry (*entry1);
  spySeries->addEntry (*entry5);
  spySeries->addEntry (*entry0);

  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  EquitySecurity<DecimalType> spy (equitySymbol, equityName, spySeries);



  // Futures security

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));
  DecimalType cornTickValue(createDecimal("0.25"));

  auto futuresEntry0 = createTimeSeriesEntry ("19851118", "3664.51025", "3687.58178", "3656.81982","3672.20068",0);

  auto futuresEntry1 = createTimeSeriesEntry ("19851119", "3710.65307617188","3722.18872070313","3679.89135742188",
				       "3714.49829101563", 0);

  auto futuresEntry2 = createTimeSeriesEntry ("19851120", "3737.56982421875","3756.7958984375","3726.0341796875",
				       "3729.87939453125",0);

  auto futuresEntry3 = createTimeSeriesEntry ("19851121","3699.11743164063","3710.65307617188","3668.35546875",
				       "3683.73657226563",0);

  auto futuresEntry4 = createTimeSeriesEntry ("19851122","3664.43017578125","3668.23559570313","3653.0146484375",
				       "3656.81982421875", 0);

  auto futuresEntry5 = createTimeSeriesEntry ("19851125","3641.59887695313","3649.20947265625","3626.3779296875",
				       "3637.79370117188", 0);

  auto futuresEntry6 = createTimeSeriesEntry ("19851126","3656.81982421875","3675.84594726563","3653.0146484375",
				       "3660.625", 0);
  auto futuresEntry7 = createTimeSeriesEntry ("19851127", "3664.43017578125","3698.67724609375","3660.625",
				       "3691.06689453125", 0);
  auto futuresEntry8 = createTimeSeriesEntry ("19851129", "3717.70336914063","3729.119140625","3698.67724609375",
				       "3710.09301757813", 0);
  auto futuresEntry9 = createTimeSeriesEntry ("19851202", "3721.50854492188","3725.31372070313","3691.06689453125",
				       "3725.31372070313", 0);
  auto futuresEntry10 = createTimeSeriesEntry ("19851203", "3713.89819335938","3740.53466796875","3710.09301757813"
					,"3736.7294921875", 0);
  auto futuresEntry11 = createTimeSeriesEntry ("19851204","3744.33984375","3759.56079101563","3736.7294921875",
					"3740.53466796875",0);

  auto cornSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  cornSeries->addEntry(*futuresEntry0);
  cornSeries->addEntry(*futuresEntry1);
  cornSeries->addEntry(*futuresEntry2);
  cornSeries->addEntry(*futuresEntry3);
  cornSeries->addEntry(*futuresEntry4);
  cornSeries->addEntry(*futuresEntry5);
  cornSeries->addEntry(*futuresEntry6);
  cornSeries->addEntry(*futuresEntry7);
  cornSeries->addEntry(*futuresEntry8);
  cornSeries->addEntry(*futuresEntry9);
  cornSeries->addEntry(*futuresEntry10);
  cornSeries->addEntry(*futuresEntry11);

  FuturesSecurity<DecimalType> corn (futuresSymbol, futuresName, cornBigPointValue,
			   cornTickValue, cornSeries);

  std::string portName("Test Portfolio");

  Portfolio<DecimalType> aPortfolio(portName);
  auto cornPtr = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol, 
						     futuresName, 
						     cornBigPointValue,
						     cornTickValue, cornSeries);
  auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, equityName, spySeries);

  aPortfolio.addSecurity(cornPtr);
  aPortfolio.addSecurity(spyPtr);

  REQUIRE (aPortfolio.getNumSecurities() == 2);
  REQUIRE (aPortfolio.getPortfolioName() == portName);
  Portfolio<DecimalType>::ConstPortfolioIterator it = aPortfolio.beginPortfolio();
  REQUIRE (it->second->getSymbol() == futuresSymbol);
  it++;
  REQUIRE (it->second->getSymbol() == equitySymbol);
 
  
  SECTION ("Portfolio find test", "[Portfolio find]")
    {
      Portfolio<DecimalType>::ConstPortfolioIterator it = aPortfolio.findSecurity(equitySymbol);
      REQUIRE (it != aPortfolio.endPortfolio());
      REQUIRE (it->second->getSymbol() == equitySymbol);

      it = aPortfolio.findSecurity(futuresSymbol);
      REQUIRE (it != aPortfolio.endPortfolio());
      REQUIRE (it->second->getSymbol() == futuresSymbol);
    }
}
