#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "TradingPosition.h"
#include "PercentNumber.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

template <class Decimal>
class TestTradingPositionObserver : public TradingPositionObserver<Decimal>
{
public:
  TestTradingPositionObserver()
    : TradingPositionObserver<Decimal>(),
      mPositionClosed(false),
      mExitPrice(DecimalConstants<Decimal>::DecimalZero)
  {}

  const Decimal& getExitPrice() const
  {
    return mExitPrice;
  }

  const date& getExitDate() const
  {
    return mExitDate;
  }

  void PositionClosed (TradingPosition<Decimal> *aPosition)
  {
    mPositionClosed = true;
    mExitPrice = aPosition->getExitPrice();
    mExitDate = aPosition->getExitDate();
  }

  bool isPositionClosed() const
  {
    return mPositionClosed;
  }

private:
  bool mPositionClosed;
  Decimal mExitPrice;
  date mExitDate;
};


TEST_CASE ("TradingPosition operations", "[TradingPosition]")
{
  auto entry0 = createTimeSeriesEntry ("19851118", "3664.51025", "3687.58178", "3656.81982","3672.20068", "0");

  auto entry1 = createTimeSeriesEntry ("19851119", "3710.65307617188","3722.18872070313","3679.89135742188",
				       "3714.49829101563", "0");

  auto entry2 = createTimeSeriesEntry ("19851120", "3737.56982421875","3756.7958984375","3726.0341796875",
				       "3729.87939453125", "0");

  auto entry3 = createTimeSeriesEntry ("19851121","3699.11743164063","3710.65307617188","3668.35546875",
				       "3683.73657226563", "0");

  auto entry4 = createTimeSeriesEntry ("19851122","3664.43017578125","3668.23559570313","3653.0146484375",
				       "3656.81982421875", "0");

  auto entry5 = createTimeSeriesEntry ("19851125","3641.59887695313","3649.20947265625","3626.3779296875",
				       "3637.79370117188", "0");

  auto entry6 = createTimeSeriesEntry ("19851126","3656.81982421875","3675.84594726563","3653.0146484375",
				       "3660.625", "0");
  auto entry7 = createTimeSeriesEntry ("19851127", "3664.43017578125","3698.67724609375","3660.625",
				       "3691.06689453125", "0");
  auto entry8 = createTimeSeriesEntry ("19851129", "3717.70336914063","3729.119140625","3698.67724609375",
				       "3710.09301757813", "0");
  auto entry9 = createTimeSeriesEntry ("19851202", "3721.50854492188","3725.31372070313","3691.06689453125",
				       "3725.31372070313", "0");
  auto entry10 = createTimeSeriesEntry ("19851203", "3713.89819335938","3740.53466796875","3710.09301757813"
					,"3736.7294921875", "0");
  auto entry11 = createTimeSeriesEntry ("19851204","3744.33984375","3759.56079101563","3736.7294921875",
					"3740.53466796875", "0");
	
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string tickerSymbol("C2");
  TradingPositionLong<DecimalType> longPosition1(tickerSymbol, entry0->getOpenValue(),  *entry0, oneContract);
  longPosition1.addBar(*entry5);
  longPosition1.addBar(*entry1);
  longPosition1.addBar(*entry9);
  longPosition1.addBar(*entry2);
  longPosition1.addBar(*entry3);
  longPosition1.addBar(*entry6);
  longPosition1.addBar(*entry7);
  longPosition1.addBar(*entry8);
  longPosition1.addBar(*entry4);
  longPosition1.addBar(*entry10);
  longPosition1.addBar(*entry11);

  auto shortEntry0 = createTimeSeriesEntry ("19860529","3789.64575195313","3801.65112304688",
					    "3769.63720703125","3785.64404296875", "0");
  auto shortEntry1 = createTimeSeriesEntry ("19860530","3785.64404296875","3793.6474609375","3769.63720703125",
					    "3793.6474609375", "0");
  auto shortEntry2 = createTimeSeriesEntry ("19860602","3789.64575195313","3833.6650390625",
					    "3773.63891601563","3825.66137695313", "0");
  auto shortEntry3 = createTimeSeriesEntry ("19860603","3837.66674804688","3837.66674804688",
					    "3761.63354492188","3769.63720703125", "0");
  auto shortEntry4 = createTimeSeriesEntry ("19860604","3773.63891601563","3801.65112304688",
					    "3757.6318359375","3793.6474609375", "0");
  auto shortEntry5 = createTimeSeriesEntry ("19860605","3793.6474609375","3801.65112304688","3777.640625",
					    "3797.6494140625", "0");
  auto shortEntry6 = createTimeSeriesEntry ("19860606","3805.65283203125","3809.6545410156",
					    "3781.64233398438","3801.65112304688", "0");
  auto shortEntry7 = createTimeSeriesEntry ("19860609","3797.6494140625","3809.65454101563",
					    "3785.64404296875","3793.6474609375", "0");
  auto shortEntry8 = createTimeSeriesEntry ("19860610","3793.6474609375","3797.6494140625",
					    "3781.64233398438","3785.64404296875", "0");
  auto shortEntry9 = createTimeSeriesEntry ("19860611","3777.640625","3781.64233398438",
					    "3733.62158203125","3749.62841796875", "0");

  auto shortEntry10 = createTimeSeriesEntry ("19861111","3100.99853515625","3119.080078125","3078.396484375",
					     "3082.91674804688", "0"); 
  auto shortEntry11 = createTimeSeriesEntry ("19861112","3082.91674804688","3155.24340820313","3078.396484375",
					     "3132.64135742188", "0");

  REQUIRE (longPosition1.isPositionOpen());
  REQUIRE_FALSE (longPosition1.isPositionClosed());
  REQUIRE (longPosition1.isLongPosition());
  REQUIRE_FALSE (longPosition1.isShortPosition());
  REQUIRE (longPosition1.getTradingSymbol() == tickerSymbol); 
  REQUIRE (longPosition1.getEntryDate() == TimeSeriesDate (1985, Nov, 18));
  REQUIRE (longPosition1.getEntryPrice() ==  entry0->getOpenValue());
  REQUIRE (longPosition1.getTradingUnits() == oneContract);
  REQUIRE (longPosition1.isWinningPosition());
  REQUIRE_FALSE (longPosition1.isLosingPosition());
  REQUIRE (longPosition1.getNumBarsInPosition() == 12);
  REQUIRE (longPosition1.getNumBarsSinceEntry() == 11);
  REQUIRE (longPosition1.getLastClose() == entry11->getCloseValue());
  REQUIRE (longPosition1.getProfitTarget() == DecimalConstants<DecimalType>::DecimalZero);
  REQUIRE (longPosition1.getStopLoss() == DecimalConstants<DecimalType>::DecimalZero);

  DecimalType longEntry1(longPosition1.getEntryPrice());
  DecimalType lastClose1(longPosition1.getLastClose());
  DecimalType oneHundred(DecimalConstants<DecimalType>::DecimalOneHundred);

  DecimalType longRefReturn (((lastClose1 - longEntry1)/longEntry1));
  DecimalType longRefPercentReturn (longRefReturn * oneHundred);
  DecimalType longRefMultiplier(longRefReturn + DecimalConstants<DecimalType>::DecimalOne);

  REQUIRE (longPosition1.getTradeReturn() == longRefReturn);
  REQUIRE (longPosition1.getPercentReturn() == longRefPercentReturn);
  REQUIRE (longPosition1.getTradeReturnMultiplier() == longRefMultiplier);

  TradingPositionShort<DecimalType> shortPosition1(tickerSymbol, shortEntry0->getOpenValue(),  *shortEntry0, oneContract);
  shortPosition1.addBar(*shortEntry1);
  shortPosition1.addBar(*shortEntry2);
  shortPosition1.addBar(*shortEntry3);
  shortPosition1.addBar(*shortEntry4);
  shortPosition1.addBar(*shortEntry5);
  shortPosition1.addBar(*shortEntry6);
  shortPosition1.addBar(*shortEntry7);
  shortPosition1.addBar(*shortEntry8);
  shortPosition1.addBar(*shortEntry9);

  REQUIRE (shortPosition1.isPositionOpen());
  REQUIRE_FALSE (shortPosition1.isPositionClosed());
  REQUIRE_FALSE (shortPosition1.isLongPosition());
  REQUIRE(shortPosition1.isShortPosition());
  REQUIRE (shortPosition1.getTradingSymbol() == tickerSymbol); 
  REQUIRE (shortPosition1.getEntryDate() == TimeSeriesDate (1986, May, 29));
  REQUIRE (shortPosition1.getEntryPrice() ==  shortEntry0->getOpenValue());
  REQUIRE (shortPosition1.getTradingUnits() == oneContract);
  REQUIRE (shortPosition1.isWinningPosition());
  REQUIRE_FALSE (shortPosition1.isLosingPosition());
  REQUIRE (shortPosition1.getNumBarsInPosition() == 10);
  REQUIRE (shortPosition1.getNumBarsSinceEntry() == 9);
  REQUIRE (shortPosition1.getLastClose() == shortEntry9->getCloseValue());
  REQUIRE (shortPosition1.getProfitTarget() == DecimalConstants<DecimalType>::DecimalZero);
  REQUIRE (shortPosition1.getStopLoss() == DecimalConstants<DecimalType>::DecimalZero);

 

  DecimalType shortEntryPrice1(shortPosition1.getEntryPrice());
  DecimalType lastShortClose1(shortPosition1.getLastClose());
  

  DecimalType shortRefReturn (-((lastShortClose1 - shortEntryPrice1)/shortEntryPrice1));
  DecimalType shortRefPercentReturn (shortRefReturn * oneHundred);
  DecimalType shortRefMultiplier(shortRefReturn + DecimalConstants<DecimalType>::DecimalOne);

  REQUIRE (shortPosition1.getTradeReturn() == shortRefReturn);
  REQUIRE (shortPosition1.getPercentReturn() == shortRefPercentReturn);
  REQUIRE (shortPosition1.getTradeReturnMultiplier() == shortRefMultiplier);

  TradingPositionShort<DecimalType> shortPosition2(tickerSymbol, shortEntry10->getOpenValue(),  *shortEntry10, oneContract);
  shortPosition2.addBar(*shortEntry11);

  REQUIRE (shortPosition2.isPositionOpen());
  REQUIRE_FALSE (shortPosition2.isPositionClosed());
  REQUIRE_FALSE (shortPosition2.isLongPosition());
  REQUIRE(shortPosition2.isShortPosition());
  REQUIRE (shortPosition2.getTradingSymbol() == tickerSymbol); 
  REQUIRE (shortPosition2.getEntryDate() == TimeSeriesDate (1986, Nov, 11));
  REQUIRE (shortPosition2.getEntryPrice() ==  shortEntry10->getOpenValue());
  REQUIRE (shortPosition2.getTradingUnits() == oneContract);
  REQUIRE_FALSE (shortPosition2.isWinningPosition());
  REQUIRE (shortPosition2.isLosingPosition());
  REQUIRE (shortPosition2.getNumBarsInPosition() == 2);
  REQUIRE (shortPosition2.getNumBarsSinceEntry() == 1);
  REQUIRE (shortPosition2.getLastClose() == shortEntry11->getCloseValue());


  SECTION ("TradingPositionLong profit target stop test")
  {
    REQUIRE (longPosition1.isLongPosition());
    REQUIRE (longPosition1.isPositionOpen());

    DecimalType stopLoss(createDecimal("1.0"));
    DecimalType profitTarget(createDecimal("2.0"));

    longPosition1.setStopLoss(stopLoss);
    longPosition1.setProfitTarget(profitTarget);

    REQUIRE (longPosition1.getStopLoss() == stopLoss);
    REQUIRE (longPosition1.getProfitTarget() == profitTarget);



    TimeSeriesDate longExitDate(TimeSeriesDate (1988, Mar, 24));
    DecimalType longExitPrice(createDecimal("260.32"));

    longPosition1.ClosePosition (longExitDate, longExitPrice);

    REQUIRE_THROWS (longPosition1.setStopLoss(stopLoss));
    REQUIRE_THROWS (longPosition1.setProfitTarget(profitTarget));
  }

  SECTION ("TradingPositionShort profit target stop test")
  {
    REQUIRE (shortPosition1.isShortPosition());
    REQUIRE (shortPosition1.isPositionOpen());

    DecimalType stopLoss(createDecimal("0.75"));
    DecimalType profitTarget(createDecimal("1.5"));

    shortPosition1.setStopLoss(stopLoss);
    shortPosition1.setProfitTarget(profitTarget);

    REQUIRE (shortPosition1.getStopLoss() == stopLoss);
    REQUIRE (shortPosition1.getProfitTarget() == profitTarget);



    TimeSeriesDate shortExitDate(TimeSeriesDate (1988, Mar, 24));
    DecimalType shortExitPrice(createDecimal("260.32"));

    shortPosition1.ClosePosition (shortExitDate, shortExitPrice);

    REQUIRE_THROWS (shortPosition1.setStopLoss(stopLoss));
    REQUIRE_THROWS (shortPosition1.setProfitTarget(profitTarget));
  }

  SECTION ("TradingPositionLong close position test")
  {
    TimeSeriesDate longExitDate(TimeSeriesDate (1985, Dec, 4));
    DecimalType longExitPrice(createDecimal("3758.32172"));

    REQUIRE (longPosition1.isLongPosition());
    REQUIRE (longPosition1.isPositionOpen());
    longPosition1.ClosePosition (longExitDate, longExitPrice);
    REQUIRE_FALSE (longPosition1.isPositionOpen());
    REQUIRE (longPosition1.isPositionClosed());
    REQUIRE (longPosition1.getExitPrice() == longExitPrice);
    REQUIRE (longPosition1.getExitDate() == longExitDate);
    std::cout << "Long position 1 % return = " << longPosition1.getPercentReturn() << std::endl;
  }

  SECTION ("TradingPositionLong close position with R multiple test")
  {
    TimeSeriesDate longExitDate(TimeSeriesDate (1985, Dec, 4));
    DecimalType longExitPrice(createDecimal("3758.32172"));

    REQUIRE (longPosition1.isLongPosition());
    REQUIRE (longPosition1.isPositionOpen());

    longPosition1.setRMultipleStop (createDecimal("3617.60452"));

    DecimalType entry = longPosition1.getEntryPrice();
 
    longPosition1.ClosePosition (longExitDate, longExitPrice);
    DecimalType exit = longPosition1.getExitPrice();

    DecimalType rMultiple ((exit - entry)/(entry - createDecimal("3617.60452")));
   
    REQUIRE (longPosition1.getRMultiple() == rMultiple);
	     
    REQUIRE_FALSE (longPosition1.isPositionOpen());
    REQUIRE (longPosition1.isPositionClosed());
    REQUIRE (longPosition1.getExitPrice() == longExitPrice);
    REQUIRE (longPosition1.getExitDate() == longExitDate);
    std::cout << "Long position 1 % return = " << longPosition1.getPercentReturn() << std::endl;
  }

  SECTION ("TradingPositionLong close observer test")
  {
    TimeSeriesDate longExitDate(TimeSeriesDate (1985, Dec, 4));
    DecimalType longExitPrice(createDecimal("3758.32172"));

    TestTradingPositionObserver<DecimalType> aObserver;
    longPosition1.addObserver (aObserver);

    REQUIRE_FALSE (aObserver.isPositionClosed());

    REQUIRE (longPosition1.isLongPosition());
    REQUIRE (longPosition1.isPositionOpen());
    longPosition1.ClosePosition (longExitDate, longExitPrice);
    REQUIRE_FALSE (longPosition1.isPositionOpen());
    REQUIRE (longPosition1.isPositionClosed());

    REQUIRE (aObserver.isPositionClosed());
    REQUIRE (aObserver.getExitPrice() == longExitPrice);
    REQUIRE (aObserver.getExitDate() == longExitDate);
  }

  SECTION ("TradingPositionShort close position test")
  {
    TimeSeriesDate shortExitDate(TimeSeriesDate (1986, Jun, 11));
    DecimalType shortExitPrice(createDecimal("3738.86450"));

    DecimalType entry = shortPosition1.getEntryPrice();
    
    REQUIRE (shortPosition1.isShortPosition());
    REQUIRE (shortPosition1.isPositionOpen());
    shortPosition1.ClosePosition (shortExitDate, shortExitPrice);
    REQUIRE_FALSE (shortPosition1.isPositionOpen());
    REQUIRE (shortPosition1.isPositionClosed());
    REQUIRE (shortPosition1.getExitPrice() == shortExitPrice);
    REQUIRE (shortPosition1.getExitDate() == shortExitDate);
    std::cout << "Short position1 % return = " << shortPosition1.getPercentReturn() << std::endl;
  }

  SECTION ("TradingPositionShort close position test w R multiple")
  {
    TimeSeriesDate shortExitDate(TimeSeriesDate (1986, Jun, 11));
    DecimalType shortExitPrice(createDecimal("3738.86450"));

    DecimalType entry = shortPosition1.getEntryPrice();
    DecimalType stopInDecimal(createDecimal("1.28"));

    PercentNumber<DecimalType> stopInPercent (PercentNumber<DecimalType>::createPercentNumber(stopInDecimal));
    DecimalType stopValue (entry + (stopInPercent.getAsPercent() * entry));

    REQUIRE (shortPosition1.isShortPosition());
    REQUIRE (shortPosition1.isPositionOpen());
    shortPosition1.setRMultipleStop (stopValue);
    shortPosition1.ClosePosition (shortExitDate, shortExitPrice);

    DecimalType exit = shortPosition1.getExitPrice();

    DecimalType rMultiple ((entry - exit)/(stopValue - entry));
   
    REQUIRE (shortPosition1.getRMultiple() == rMultiple);
    std::cout << "Short position1 r multiple = " << rMultiple << std::endl;
    REQUIRE_FALSE (shortPosition1.isPositionOpen());
    REQUIRE (shortPosition1.isPositionClosed());
    REQUIRE (shortPosition1.getExitPrice() == shortExitPrice);
    REQUIRE (shortPosition1.getExitDate() == shortExitDate);

  }

SECTION ("TradingPositionShort close position test 2")
  {
    TimeSeriesDate shortExitDate2(TimeSeriesDate (1986, Nov, 12));
    DecimalType shortExitPrice2(createDecimal("3140.69132"));

    REQUIRE (shortPosition2.isShortPosition());
    REQUIRE (shortPosition2.isPositionOpen());
    shortPosition2.ClosePosition (shortExitDate2, shortExitPrice2);
    REQUIRE_FALSE (shortPosition2.isPositionOpen());
    REQUIRE (shortPosition2.isPositionClosed());
    REQUIRE (shortPosition2.getExitPrice() == shortExitPrice2);
    REQUIRE (shortPosition2.getExitDate() == shortExitDate2);
    std::cout << "Short position 2 % return = " << shortPosition2.getPercentReturn() << std::endl;
  }

SECTION ("TradingPositionShort close position test 2 with R multiple")
  {
    TimeSeriesDate shortExitDate2(TimeSeriesDate (1986, Nov, 12));
    DecimalType shortExitPrice2(createDecimal("3140.69132"));

    REQUIRE (shortPosition2.isShortPosition());
    REQUIRE (shortPosition2.isPositionOpen());

    shortPosition2.setRMultipleStop (shortExitPrice2);

    DecimalType entry = shortPosition2.getEntryPrice();


    shortPosition2.ClosePosition (shortExitDate2, shortExitPrice2);

    DecimalType exit = shortPosition2.getExitPrice();

    DecimalType rMultiple (-(exit/shortExitPrice2));
   
    REQUIRE (shortPosition2.getRMultiple() == rMultiple);

    REQUIRE_FALSE (shortPosition2.isPositionOpen());
    REQUIRE (shortPosition2.isPositionClosed());
    REQUIRE (shortPosition2.getExitPrice() == shortExitPrice2);
    REQUIRE (shortPosition2.getExitDate() == shortExitDate2);
    std::cout << "Short position 2 % return = " << shortPosition2.getPercentReturn() << std::endl;
  }

  SECTION ("TradingPositionLong getExitPrice Exception")
  {
    REQUIRE_THROWS (longPosition1.getExitPrice());
  }

  SECTION ("TradingPositionLong getExitDate Exception")
  {
    REQUIRE_THROWS (longPosition1.getExitDate());
  }

  SECTION ("TradingPositionShort getExitPrice Exception")
  {
    REQUIRE_THROWS (shortPosition1.getExitPrice());
  }

  SECTION ("TradingPositionShort getExitDate Exception")
  {
    REQUIRE_THROWS (shortPosition1.getExitDate());
  }

  SECTION ("TradingPositionLong ConstIterator tests")
  {
    TradingPosition<DecimalType>::ConstPositionBarIterator it = longPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first ==  TimeSeriesDate (1985, Nov, 19));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry1);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first ==  TimeSeriesDate (1985, Dec, 4));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry11);
  }

  SECTION ("TradingPositionLong ConstIterator after position closed tests")
  {
    TimeSeriesDate longExitDate2(TimeSeriesDate (1985, Dec, 4));
    DecimalType longExitPrice2(createDecimal("3758.32172"));

    REQUIRE (longPosition1.isLongPosition());
    REQUIRE (longPosition1.isPositionOpen());
    longPosition1.ClosePosition (longExitDate2, longExitPrice2);
    REQUIRE (longPosition1.isPositionClosed());

    TradingPosition<DecimalType>::ConstPositionBarIterator it = longPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first ==  TimeSeriesDate (1985, Nov, 19));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry1);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first ==  TimeSeriesDate (1985, Dec, 4));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry11);
  }

  SECTION ("TradingPositionShort ConstIterator after position closes tests")
  {
    TimeSeriesDate shortExitDate3(TimeSeriesDate (1986, Jun, 11));
    DecimalType shortExitPrice3(createDecimal("3738.86450"));

    REQUIRE (shortPosition1.isShortPosition());
    REQUIRE (shortPosition1.isPositionOpen());
    shortPosition1.ClosePosition (shortExitDate3, shortExitPrice3);
    REQUIRE (shortPosition1.isPositionClosed());

    TradingPosition<DecimalType>::ConstPositionBarIterator it = shortPosition1.beginPositionBarHistory();
    it++;
    REQUIRE (it->first ==  TimeSeriesDate (1986, May, 30));
    REQUIRE (it->second.getTimeSeriesEntry() == *shortEntry1);

    it = shortPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first ==  TimeSeriesDate (1986, Jun, 11));
    REQUIRE (it->second.getTimeSeriesEntry() == *shortEntry9);
  }
  
}

