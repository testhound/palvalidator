#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "InstrumentPosition.h"
#include "DecimalConstants.h"
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
  TradingVolume twoContracts(2, TradingVolume::CONTRACTS);
  std::string tickerSymbol("C2");
  InstrumentPosition<DecimalType> c2InstrumentPositionLong (tickerSymbol);

  auto longPosition1 = std::make_shared<TradingPositionLong<DecimalType>>(tickerSymbol, entry0->getOpenValue(),  
								*entry0, oneContract);
  auto longPosition2 = std::make_shared<TradingPositionLong<DecimalType>>(tickerSymbol, entry4->getOpenValue(),  
								*entry4, oneContract);

  REQUIRE (c2InstrumentPositionLong.isFlatPosition());
  REQUIRE_FALSE (c2InstrumentPositionLong.isLongPosition());
  REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
  REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 0);

  c2InstrumentPositionLong.addPosition(longPosition1);
  REQUIRE (c2InstrumentPositionLong.getVolumeInAllUnits() == oneContract);
  REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 1);
  REQUIRE (c2InstrumentPositionLong.getFillPrice() == entry0->getOpenValue());
  REQUIRE (c2InstrumentPositionLong.getFillPrice(1) == entry0->getOpenValue());

  REQUIRE_FALSE (c2InstrumentPositionLong.isFlatPosition());
  REQUIRE (c2InstrumentPositionLong.isLongPosition());
  REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());

  c2InstrumentPositionLong.addBar(*entry1);
  c2InstrumentPositionLong.addBar(*entry2);
  c2InstrumentPositionLong.addBar(*entry3);
  c2InstrumentPositionLong.addBar(*entry4);

  c2InstrumentPositionLong.addPosition(longPosition2);
  REQUIRE (c2InstrumentPositionLong.getVolumeInAllUnits() == twoContracts);
  REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 2);
  REQUIRE (c2InstrumentPositionLong.getFillPrice() == entry0->getOpenValue());
  REQUIRE (c2InstrumentPositionLong.getFillPrice(1) == entry0->getOpenValue());
  REQUIRE (c2InstrumentPositionLong.getFillPrice(2) == entry4->getOpenValue());
  c2InstrumentPositionLong.addBar(*entry5);
  c2InstrumentPositionLong.addBar(*entry6);
  c2InstrumentPositionLong.addBar(*entry7);
  c2InstrumentPositionLong.addBar(*entry8);
  c2InstrumentPositionLong.addBar(*entry9);
  c2InstrumentPositionLong.addBar(*entry10);
  c2InstrumentPositionLong.addBar(*entry11);

  REQUIRE (longPosition1->getNumBarsInPosition() == 12);
  REQUIRE (longPosition1->getLastClose() == entry11->getCloseValue());
  REQUIRE (longPosition2->getNumBarsInPosition() == 8);
  REQUIRE (longPosition2->getLastClose() == entry11->getCloseValue());
InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator it1 = 
    c2InstrumentPositionLong.getInstrumentPosition (1);
  REQUIRE (it1 != c2InstrumentPositionLong.endInstrumentPosition());

  InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator it2 = 
    c2InstrumentPositionLong.getInstrumentPosition (2);
  REQUIRE (it2 != c2InstrumentPositionLong.endInstrumentPosition());

  std::shared_ptr<TradingPosition<DecimalType>> longPos1 = *it1;
  std::shared_ptr<TradingPosition<DecimalType>> longPos2 = *it2;

  REQUIRE (longPos1->getEntryDate() == longPosition1->getEntryDate());
  REQUIRE (longPos1->getEntryPrice() == longPosition1->getEntryPrice());
  REQUIRE (longPos2->getEntryDate() == longPosition2->getEntryDate());
  REQUIRE (longPos2->getEntryPrice() == longPosition2->getEntryPrice());

  InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator it3 = 
    c2InstrumentPositionLong.beginInstrumentPosition();
  REQUIRE (it3 != c2InstrumentPositionLong.endInstrumentPosition());
  REQUIRE ((*it3)->getEntryDate() == longPosition1->getEntryDate());
  it3++;
  REQUIRE (it3 != c2InstrumentPositionLong.endInstrumentPosition());
  REQUIRE ((*it3)->getEntryDate() == longPosition2->getEntryDate());
  it3++;
  REQUIRE (it3 == c2InstrumentPositionLong.endInstrumentPosition());

  auto shortEntry0 = createTimeSeriesEntry ("19860529","3789.64575195313","3801.65112304688",
					    "3769.63720703125","3785.64404296875", 0);
  auto shortEntry1 = createTimeSeriesEntry ("19860530","3785.64404296875","3793.6474609375","3769.63720703125",
					    "3793.6474609375", 0);
  auto shortEntry2 = createTimeSeriesEntry ("19860602","3789.64575195313","3833.6650390625",
					    "3773.63891601563","3825.66137695313", 0);
  auto shortEntry3 = createTimeSeriesEntry ("19860603","3837.66674804688","3837.66674804688",
					    "3761.63354492188","3769.63720703125", 0);
  auto shortEntry4 = createTimeSeriesEntry ("19860604","3773.63891601563","3801.65112304688",
					    "3757.6318359375","3793.6474609375", 0);
  auto shortEntry5 = createTimeSeriesEntry ("19860605","3793.6474609375","3801.65112304688","3777.640625",
					    "3797.6494140625", 0);
  auto shortEntry6 = createTimeSeriesEntry ("19860606","3805.65283203125","3809.6545410156",
					    "3781.64233398438","3801.65112304688", 0);
  auto shortEntry7 = createTimeSeriesEntry ("19860609","3797.6494140625","3809.65454101563",
					    "3785.64404296875","3793.6474609375", 0);
  auto shortEntry8 = createTimeSeriesEntry ("19860610","3793.6474609375","3797.6494140625",
					    "3781.64233398438","3785.64404296875", 0);
  auto shortEntry9 = createTimeSeriesEntry ("19860611","3777.640625","3781.64233398438",
					    "3733.62158203125","3749.62841796875", 0);

  auto shortEntry10 = createTimeSeriesEntry ("19860612","3745.62670898438",
					     "3745.62670898438","3685.6005859375",
					     "3689.60229492188", 0); 
  auto shortEntry11 = createTimeSeriesEntry ("19860613","3693.60400390625","3705.609375",
					     "3669.59375","3685.6005859375", 0);


  InstrumentPosition<DecimalType> c2InstrumentPositionShort (tickerSymbol);
  auto shortPosition1 = std::make_shared<TradingPositionShort<DecimalType>>(tickerSymbol, 
								 shortEntry0->getOpenValue(),  
								 *shortEntry0, 
								 oneContract);
  auto shortPosition2 = std::make_shared<TradingPositionShort<DecimalType>>(tickerSymbol, 
								 shortEntry3->getOpenValue(),  
								 *shortEntry3, 
								 oneContract);

  REQUIRE (c2InstrumentPositionShort.isFlatPosition());
  REQUIRE_FALSE (c2InstrumentPositionShort.isLongPosition());
  REQUIRE_FALSE (c2InstrumentPositionShort.isShortPosition());
  REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 0);

  c2InstrumentPositionShort.addPosition(shortPosition1);
  REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 1);
  REQUIRE_FALSE (c2InstrumentPositionShort.isFlatPosition());
  REQUIRE_FALSE (c2InstrumentPositionShort.isLongPosition());
  REQUIRE (c2InstrumentPositionShort.isShortPosition());

  c2InstrumentPositionShort.addBar(*shortEntry1);
  c2InstrumentPositionShort.addBar(*shortEntry2);
  c2InstrumentPositionShort.addBar(*shortEntry3);

  c2InstrumentPositionShort.addPosition(shortPosition2);
  REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 2);

  c2InstrumentPositionShort.addBar(*shortEntry4);
  c2InstrumentPositionShort.addBar(*shortEntry5);
  c2InstrumentPositionShort.addBar(*shortEntry6);
  c2InstrumentPositionShort.addBar(*shortEntry7);
  c2InstrumentPositionShort.addBar(*shortEntry8);
  c2InstrumentPositionShort.addBar(*shortEntry9);
  c2InstrumentPositionShort.addBar(*shortEntry10);
  c2InstrumentPositionShort.addBar(*shortEntry11);

  REQUIRE (shortPosition1->getNumBarsInPosition() == 12);
  REQUIRE (shortPosition1->getLastClose() == shortEntry11->getCloseValue());
  REQUIRE (shortPosition2->getNumBarsInPosition() == 9);
  REQUIRE (shortPosition2->getLastClose() == shortEntry11->getCloseValue());
  InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator it1Short = 
    c2InstrumentPositionShort.getInstrumentPosition (1);
  REQUIRE (it1Short != c2InstrumentPositionShort.endInstrumentPosition());

  InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator it2Short = 
    c2InstrumentPositionShort.getInstrumentPosition (2);
  REQUIRE (it2Short != c2InstrumentPositionShort.endInstrumentPosition());  

  std::shared_ptr<TradingPosition<DecimalType>> shortPos1 = *it1Short;
  std::shared_ptr<TradingPosition<DecimalType>> shortPos2 = *it2Short;

  REQUIRE (shortPos1->getEntryDate() == shortPosition1->getEntryDate());
  REQUIRE (shortPos1->getEntryPrice() == shortPosition1->getEntryPrice());
  REQUIRE (shortPos2->getEntryDate() == shortPosition2->getEntryDate());
  REQUIRE (shortPos2->getEntryPrice() == shortPosition2->getEntryPrice());

  InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator it3Short = 
    c2InstrumentPositionShort.beginInstrumentPosition();
  REQUIRE (it3Short != c2InstrumentPositionShort.endInstrumentPosition());
  REQUIRE ((*it3Short)->getEntryDate() == shortPosition1->getEntryDate());
  it3Short++;
  REQUIRE (it3Short != c2InstrumentPositionShort.endInstrumentPosition());
  REQUIRE ((*it3Short)->getEntryDate() == shortPosition2->getEntryDate());
  it3Short++;
  REQUIRE (it3Short == c2InstrumentPositionShort.endInstrumentPosition());

  SECTION ("Test closing all long positions")
  {
    REQUIRE_FALSE (c2InstrumentPositionLong.isFlatPosition());
    REQUIRE (c2InstrumentPositionLong.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
    REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 2);

    c2InstrumentPositionLong.closeAllPositions(createDate ("19851205"), 
					       createDecimal("3725.313720"));

    REQUIRE (c2InstrumentPositionLong.isFlatPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
    REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 0);
  }

  SECTION ("Test closing one long position")
  {
    REQUIRE_FALSE (c2InstrumentPositionLong.isFlatPosition());
    REQUIRE (c2InstrumentPositionLong.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
    REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 2);

    c2InstrumentPositionLong.closeUnitPosition(createDate ("19851205"), 
					       createDecimal("3725.313720"),
					       1);

    REQUIRE_FALSE (c2InstrumentPositionLong.isFlatPosition());
    REQUIRE (c2InstrumentPositionLong.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
    REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 1);

    InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator itLocal1 = 
      c2InstrumentPositionLong.getInstrumentPosition (1);
    REQUIRE (itLocal1 != c2InstrumentPositionLong.endInstrumentPosition());
    REQUIRE ((*itLocal1)->getEntryDate() == longPosition2->getEntryDate());
  }

  SECTION ("Test closing all short positions")
  {
    REQUIRE_FALSE (c2InstrumentPositionShort.isFlatPosition());
    REQUIRE_FALSE (c2InstrumentPositionShort.isLongPosition());
    REQUIRE (c2InstrumentPositionShort.isShortPosition());
    REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 2);

    c2InstrumentPositionShort.closeAllPositions(createDate ("19860616"), 
						createDecimal("3705.609375"));

    REQUIRE (c2InstrumentPositionShort.isFlatPosition());
    REQUIRE_FALSE (c2InstrumentPositionShort.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionShort.isShortPosition());
    REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 0);
  }
  
  SECTION ("Test closing one short position")
  {
    REQUIRE_FALSE (c2InstrumentPositionShort.isFlatPosition());
    REQUIRE_FALSE (c2InstrumentPositionShort.isLongPosition());
    REQUIRE (c2InstrumentPositionShort.isShortPosition());
    REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 2);

    c2InstrumentPositionShort.closeUnitPosition(createDate ("19860616"), 
						createDecimal("3705.609375"),
					       1);

    REQUIRE_FALSE (c2InstrumentPositionShort.isFlatPosition());
    REQUIRE_FALSE (c2InstrumentPositionShort.isLongPosition());
    REQUIRE (c2InstrumentPositionShort.isShortPosition());
    REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 1);

    InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator itLocal2 = 
      c2InstrumentPositionShort.getInstrumentPosition (1);
    REQUIRE (itLocal2 != c2InstrumentPositionShort.endInstrumentPosition());
    REQUIRE ((*itLocal2)->getEntryDate() == shortPosition2->getEntryDate());
  }

  SECTION ("Test throwing exception trying to add bar in flat position")
  {
    std::string tickerqqq("QQQ");
    InstrumentPosition<DecimalType> c2qqqPositionLong (tickerqqq);

    REQUIRE (c2qqqPositionLong.isFlatPosition());
    REQUIRE_THROWS (c2qqqPositionLong.addBar (*entry0));
  }

  SECTION ("Test throwing exception trying to getInstrumentPosition in flat state")
  {
    std::string tickerspy("SPY");
    InstrumentPosition<DecimalType> c2spyPositionLong (tickerspy);

    REQUIRE (c2spyPositionLong.isFlatPosition());
    REQUIRE_THROWS (c2spyPositionLong.getInstrumentPosition (1));
  }

  SECTION ("Test throwing exception trying to get begin iterator in flat state")
  {
    std::string tickeruso("USO");
    InstrumentPosition<DecimalType> c2usoPositionLong (tickeruso);

    REQUIRE (c2usoPositionLong.isFlatPosition());
    REQUIRE_THROWS (c2usoPositionLong.beginInstrumentPosition());
  }

  SECTION ("Test throwing exception trying to get end iterator in flat state")
  {
    std::string tickerdia("DIA");
    InstrumentPosition<DecimalType> c2diaPositionLong (tickerdia);

    REQUIRE (c2diaPositionLong.isFlatPosition());
    REQUIRE_THROWS (c2diaPositionLong.endInstrumentPosition());
  }

  SECTION ("Test throwing exception trying to closeAllPositions in flat state")
  {
    std::string tickeriwm("IWM");
    InstrumentPosition<DecimalType> c2iwmPositionLong (tickeriwm);

    REQUIRE (c2iwmPositionLong.isFlatPosition());
    REQUIRE_THROWS (c2iwmPositionLong.closeAllPositions(longPosition1->getEntryDate(),
							entry0->getOpenValue()));
  }

  SECTION ("Test throwing exception trying to closeUnitPosition in flat state")
  {
    std::string tickeribm("IBM");
    InstrumentPosition<DecimalType> c2ibmPositionLong (tickeribm);

    REQUIRE (c2ibmPositionLong.isFlatPosition());
    REQUIRE_THROWS (c2ibmPositionLong.closeUnitPosition(longPosition1->getEntryDate(),
							 entry0->getOpenValue(),
							 1));
  }

  SECTION ("Test throwing exception if unit is out of range")
  {
    REQUIRE_FALSE (c2InstrumentPositionLong.isFlatPosition());
    REQUIRE (c2InstrumentPositionLong.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
    REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 2);
    
    
    REQUIRE_THROWS (c2InstrumentPositionLong.getInstrumentPosition (3));
  }

  SECTION ("Test throwing exception if trying to add closed position")
  {
    REQUIRE_FALSE (c2InstrumentPositionLong.isFlatPosition());
    REQUIRE (c2InstrumentPositionLong.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
    REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 2);

    std::string tickermchp("MCHP");

    auto longPosition3 = std::make_shared<TradingPositionLong<DecimalType>>(tickermchp, 
								  entry4->getOpenValue(),  
								  *entry4, 
								  oneContract);
    longPosition3->ClosePosition(entry5->getDateValue(), entry5->getOpenValue());
    REQUIRE_THROWS (c2InstrumentPositionLong.addPosition (longPosition3));
  }

  SECTION ("Test throwing exception if trying to add short position to long")
  {
    REQUIRE_FALSE (c2InstrumentPositionLong.isFlatPosition());
    REQUIRE (c2InstrumentPositionLong.isLongPosition());
    REQUIRE_FALSE (c2InstrumentPositionLong.isShortPosition());
    REQUIRE (c2InstrumentPositionLong.getNumPositionUnits() == 2);

    REQUIRE_THROWS (c2InstrumentPositionLong.addPosition(shortPosition1));
  }

  SECTION ("Test throwing exception if trying to add long position to short")
  {
    REQUIRE_FALSE (c2InstrumentPositionShort.isFlatPosition());
    REQUIRE_FALSE (c2InstrumentPositionShort.isLongPosition());
    REQUIRE (c2InstrumentPositionShort.isShortPosition());
    REQUIRE (c2InstrumentPositionShort.getNumPositionUnits() == 2);

    REQUIRE_THROWS (c2InstrumentPositionShort.addPosition(longPosition1));
  }
}

