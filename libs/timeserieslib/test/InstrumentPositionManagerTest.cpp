#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../InstrumentPositionManager.h"
#include "../DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;


TEST_CASE ("TradingPosition operations", "[TradingPosition]")
{
  auto entry0 = createTimeSeriesEntry ("19851118", "3664.51025", "3687.58178", "3656.81982","3672.20068",0);

  auto entry1 = createTimeSeriesEntry ("19851119", "3710.65307617188","3722.18872070313","3679.89135742188",
				       "3714.49829101563", 0);

  auto entry2 = createTimeSeriesEntry ("19851120", "3737.56982421875","3756.7958984375","3726.0341796875",
				       "3729.87939453125",0);

  auto entry3 = createTimeSeriesEntry ("19851121","3699.11743164063","3710.65307617188","3668.35546875",
				       "3683.73657226563",0);

  auto entry4 = createTimeSeriesEntry ("19851122","3664.43017578125","3668.23559570313","3653.0146484375",
				       "3656.81982421875", 0);

  auto entry5 = createTimeSeriesEntry ("19851125","3641.59887695313","3649.20947265625","3626.3779296875",
				       "3637.79370117188", 0);

  auto entry6 = createTimeSeriesEntry ("19851126","3656.81982421875","3675.84594726563","3653.0146484375",
				       "3660.625", 0);
  auto entry7 = createTimeSeriesEntry ("19851127", "3664.43017578125","3698.67724609375","3660.625",
				       "3691.06689453125", 0);
  auto entry8 = createTimeSeriesEntry ("19851129", "3717.70336914063","3729.119140625","3698.67724609375",
				       "3710.09301757813", 0);
  auto entry9 = createTimeSeriesEntry ("19851202", "3721.50854492188","3725.31372070313","3691.06689453125",
				       "3725.31372070313", 0);
  auto entry10 = createTimeSeriesEntry ("19851203", "3713.89819335938","3740.53466796875","3710.09301757813"
					,"3736.7294921875", 0);
  auto entry11 = createTimeSeriesEntry ("19851204","3744.33984375","3759.56079101563","3736.7294921875",
					"3740.53466796875",0);
	
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  std::string tickerSymbol("C2");
  InstrumentPosition<DecimalType> c2InstrumentPositionLong (tickerSymbol);

  auto longPosition1 = std::make_shared<TradingPositionLong<DecimalType>>(tickerSymbol, entry0->getOpenValue(),  
								*entry0, oneContract);
  auto longPosition2 = std::make_shared<TradingPositionLong<DecimalType>>(tickerSymbol, entry4->getOpenValue(),  
								*entry4, oneContract);

  
  // Time Series for short positions
  
  auto shortEntry0 = createTimeSeriesEntry ("20160211","95.46","97.32",
					    "95.19","96.55", 0);
  auto shortEntry1 = createTimeSeriesEntry ("20160210","97.50","98.69","96.62",
					    "96.69", 0);
  auto shortEntry2 = createTimeSeriesEntry ("20160209","95.33","97.78",
					    "95.18","96.32", 0);
  auto shortEntry3 = createTimeSeriesEntry ("20160208","96.29","97.05",
					    "94.84","96.62", 0);
  auto shortEntry4 = createTimeSeriesEntry ("20160205","101.29","101.33",
					    "97.72","98.12", 0);
  auto shortEntry5 = createTimeSeriesEntry ("20160204","101.39","102.46",
					    "100.44","101.65", 0);
  auto shortEntry6 = createTimeSeriesEntry ("20160203","102.83","102.83",
					    "99.88","101.66", 0);

  std::string qqqSymbol("QQQ");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  auto shortPosition1 = std::make_shared<TradingPositionShort<DecimalType>>(qqqSymbol, 
								 shortEntry6->getOpenValue(),  
								 *shortEntry6, 
								 oneShare);
  auto shortPosition2 = std::make_shared<TradingPositionShort<DecimalType>>(qqqSymbol, 
								 shortEntry4->getOpenValue(),  
								 *shortEntry4, 
								 oneShare);


  InstrumentPositionManager<DecimalType> posManager;
  REQUIRE (posManager.getNumInstruments() == 0);
  posManager.addInstrument(tickerSymbol);
  REQUIRE (posManager.getNumInstruments() == 1);
  posManager.addInstrument(qqqSymbol);
  REQUIRE (posManager.getNumInstruments() == 2);

  REQUIRE_FALSE (posManager.isLongPosition(tickerSymbol));
  REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
  REQUIRE (posManager.isFlatPosition(tickerSymbol));
  REQUIRE (posManager.getNumPositionUnits (tickerSymbol) == 0);

  REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
  REQUIRE_FALSE (posManager.isShortPosition(qqqSymbol));
  REQUIRE (posManager.isFlatPosition(qqqSymbol));
  REQUIRE (posManager.getNumPositionUnits (qqqSymbol) == 0);

  posManager.addPosition (shortPosition1);
  REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
  REQUIRE (posManager.isShortPosition(qqqSymbol));
  REQUIRE_FALSE (posManager.isFlatPosition(qqqSymbol));
  REQUIRE (posManager.getNumPositionUnits (qqqSymbol) == 1);

  posManager.addBar (qqqSymbol, *shortEntry5);
  posManager.addBar (qqqSymbol, *shortEntry4);
  posManager.addPosition (shortPosition2);
  REQUIRE (posManager.getNumPositionUnits (qqqSymbol) == 2);
  posManager.addBar (qqqSymbol, *shortEntry3);
  posManager.addBar (qqqSymbol, *shortEntry2);
  posManager.addBar (qqqSymbol, *shortEntry1);
  posManager.addBar (qqqSymbol, *shortEntry0);

  posManager.addPosition (longPosition1);
  REQUIRE (posManager.isLongPosition(tickerSymbol));
  REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
  REQUIRE_FALSE (posManager.isFlatPosition(tickerSymbol));
  REQUIRE (posManager.getNumPositionUnits (tickerSymbol) == 1);

  posManager.addBar(tickerSymbol, *entry1);
  posManager.addBar(tickerSymbol, *entry2);
  posManager.addBar(tickerSymbol, *entry3);
  posManager.addBar(tickerSymbol, *entry4);
  posManager.addPosition (longPosition2);
  REQUIRE (posManager.getNumPositionUnits (tickerSymbol) == 2);
  posManager.addBar(tickerSymbol, *entry5);
  posManager.addBar(tickerSymbol, *entry6);
  posManager.addBar(tickerSymbol, *entry7);
  posManager.addBar(tickerSymbol, *entry8);
  posManager.addBar(tickerSymbol, *entry9);
  posManager.addBar(tickerSymbol, *entry10);
  posManager.addBar(tickerSymbol, *entry11);

  SECTION ("Test InstrumentPosition iterators")
  {
    InstrumentPositionManager<DecimalType>::ConstInstrumentPositionIterator it = 
      posManager.beginInstrumentPositions();

    REQUIRE (it->second->getInstrumentSymbol() == tickerSymbol);
    it++;
    REQUIRE (it->second->getInstrumentSymbol() == qqqSymbol);
    it++;
    REQUIRE (it == posManager.endInstrumentPositions());
    
  }

  SECTION ("Test getInstrumentPosition")
    {
    InstrumentPosition<DecimalType> qqqInstrument = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument.getInstrumentSymbol() == qqqSymbol);
    REQUIRE (qqqInstrument.getNumPositionUnits() == 2);
    REQUIRE (qqqInstrument.getFillPrice() == shortEntry6->getOpenValue());
    REQUIRE (qqqInstrument.getFillPrice(1) == shortEntry6->getOpenValue());
    REQUIRE (qqqInstrument.getFillPrice(2) == shortEntry4->getOpenValue());
  }

  SECTION ("Test addBarForOpenPosition")
    {
      auto aSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
      aSeries->addEntry(*entry0);
      aSeries->addEntry(*entry1);
      aSeries->addEntry(*entry2);
      aSeries->addEntry(*entry3);
      aSeries->addEntry(*entry4);
      aSeries->addEntry(*entry5);
      aSeries->addEntry(*entry6);
      aSeries->addEntry(*entry7);
      aSeries->addEntry(*entry8);
      aSeries->addEntry(*entry9);
      aSeries->addEntry(*entry10);
      aSeries->addEntry(*entry11);
      
      std::string futuresSymbol("C2");
      std::string futuresName("Corn futures");
      DecimalType cornBigPointValue(createDecimal("50.0"));
      DecimalType cornTickValue(createDecimal("0.25"));
      auto corn = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol, 
						       futuresName, 
						       cornBigPointValue,
						       cornTickValue, 
						       aSeries);

      std::string portName("Test Portfolio");
      auto aPortfolio = std::make_shared<Portfolio<DecimalType>>(portName);

      aPortfolio->addSecurity (corn);
      InstrumentPositionManager<DecimalType> aPosManager;
      aPosManager.addInstrument(futuresSymbol);

      auto longPositionCorn = std::make_shared<TradingPositionLong<DecimalType>>(futuresSymbol, entry0->getOpenValue(),  
								*entry0, oneContract);

      REQUIRE (aPosManager.getNumPositionUnits (futuresSymbol) == 0);
      aPosManager.addPosition (longPositionCorn);

      REQUIRE (aPosManager.isLongPosition(futuresSymbol));
      REQUIRE_FALSE (aPosManager.isShortPosition(futuresSymbol));
      REQUIRE_FALSE (aPosManager.isFlatPosition(futuresSymbol));
      REQUIRE (aPosManager.getNumPositionUnits (futuresSymbol) == 1);

      aPosManager.addBarForOpenPosition (createDate("19851119"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851120"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851121"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851122"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851125"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851126"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851127"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851129"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851202"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851203"), aPortfolio);
      aPosManager.addBarForOpenPosition (createDate("19851204"), aPortfolio);
      
      auto cornPos = aPosManager.getTradingPosition (futuresSymbol, 1);
      REQUIRE (cornPos->getNumBarsInPosition() == 12);
      REQUIRE (cornPos->getNumBarsSinceEntry() == 11);
    }

  SECTION ("Test closeUnitPosition")
  {
    InstrumentPosition<DecimalType> qqqInstrument = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument.getNumPositionUnits() == 2);
    REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
    REQUIRE (posManager.isShortPosition(qqqSymbol));
    REQUIRE_FALSE (posManager.isFlatPosition(qqqSymbol));

    REQUIRE (shortPosition1->isPositionOpen());
    REQUIRE (shortPosition2->isPositionOpen());

    posManager.closeUnitPosition(qqqSymbol,
				 createDate ("20160212"),
				 createDecimal("98.02"),
				 2);

    REQUIRE (shortPosition1->isPositionOpen());
    REQUIRE (shortPosition2->isPositionClosed());
    REQUIRE (shortPosition2->getExitPrice() == createDecimal("98.02"));
    REQUIRE (shortPosition2->getExitDate() ==  createDate ("20160212"));

    InstrumentPosition<DecimalType> qqqInstrument2 = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument2.getNumPositionUnits() == 1);
    REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
    REQUIRE (posManager.isShortPosition(qqqSymbol));
    REQUIRE_FALSE (posManager.isFlatPosition(qqqSymbol));


    REQUIRE (qqqInstrument2.getInstrumentSymbol() == qqqSymbol);
    REQUIRE (qqqInstrument2.getFillPrice() == shortEntry6->getOpenValue());
    REQUIRE (qqqInstrument2.getFillPrice(1) == shortEntry6->getOpenValue());
    REQUIRE_THROWS (qqqInstrument2.getFillPrice(2));

    posManager.closeUnitPosition(qqqSymbol,
				 createDate ("20160213"),
				 createDecimal("99.02"),
				 1);

    REQUIRE (shortPosition1->isPositionClosed());
    REQUIRE (shortPosition2->isPositionClosed());
    REQUIRE (shortPosition1->getExitPrice() == createDecimal("99.02"));
    REQUIRE (shortPosition1->getExitDate() ==  createDate ("20160213"));

    qqqInstrument2 = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument2.getNumPositionUnits() == 0);
    REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
    REQUIRE_FALSE (posManager.isShortPosition(qqqSymbol));
    REQUIRE (posManager.isFlatPosition(qqqSymbol));
  }

  SECTION ("Test closeAllPositions")
    {
      InstrumentPosition<DecimalType> cornInstrument = posManager.getInstrumentPosition(tickerSymbol);
      REQUIRE (cornInstrument.getNumPositionUnits() == 2);
      REQUIRE (posManager.isLongPosition(tickerSymbol));
      REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
      REQUIRE_FALSE (posManager.isFlatPosition(tickerSymbol));
      REQUIRE (longPosition1->isPositionOpen());
      REQUIRE (longPosition2->isPositionOpen());
      posManager.closeAllPositions (tickerSymbol,  createDate("19851205"),
				    createDecimal("3725.3137207"));

      InstrumentPosition<DecimalType> cornInstrument2 = posManager.getInstrumentPosition(tickerSymbol);
      REQUIRE (cornInstrument.getNumPositionUnits() == 0);
      REQUIRE_FALSE (posManager.isLongPosition(tickerSymbol));
      REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
      REQUIRE (posManager.isFlatPosition(tickerSymbol));

      REQUIRE (longPosition1->isPositionClosed());
      REQUIRE (longPosition2->isPositionClosed());
      REQUIRE (longPosition1->getExitPrice() == createDecimal("3725.3137207"));
      REQUIRE (longPosition2->getExitPrice() == createDecimal("3725.3137207"));
      REQUIRE (longPosition1->getExitDate() == createDate("19851205"));
      REQUIRE (longPosition2->getExitDate() == createDate("19851205"));
    }

  SECTION ("Test exceptions pt. 1")
    {
      REQUIRE (posManager.getNumInstruments() == 2);
      REQUIRE_THROWS (posManager.addInstrument(tickerSymbol));
      REQUIRE_THROWS (posManager.addInstrument(qqqSymbol));
    }


}

