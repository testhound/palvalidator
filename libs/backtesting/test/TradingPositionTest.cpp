#include <catch2/catch_test_macros.hpp>
#include "TradingPosition.h"
#include "PercentNumber.h"
#include "DecimalConstants.h"
#include "TestUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::posix_time::ptime;
using boost::posix_time::time_from_string;
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
    REQUIRE (it->first.date() ==  TimeSeriesDate (1985, Nov, 19));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry1);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first.date() ==  TimeSeriesDate (1985, Dec, 4));
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
    REQUIRE (it->first.date() ==  TimeSeriesDate (1985, Nov, 19));
    REQUIRE (it->second.getTimeSeriesEntry() == *entry1);

    it = longPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first.date() ==  TimeSeriesDate (1985, Dec, 4));
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
    REQUIRE (it->first.date() ==  TimeSeriesDate (1986, May, 30));
    REQUIRE (it->second.getTimeSeriesEntry() == *shortEntry1);

    it = shortPosition1.endPositionBarHistory();
    it--;

    REQUIRE (it->first.date() ==  TimeSeriesDate (1986, Jun, 11));
    REQUIRE (it->second.getTimeSeriesEntry() == *shortEntry9);
  }

  SECTION("Invalid entry price") {
    REQUIRE_THROWS_AS(
    TradingPositionLong<DecimalType>("SYM", DecimalConstants<DecimalType>::DecimalZero, *entry0, oneContract),
    TradingPositionException);
  }

  SECTION("Negative profit target / stop loss") {
    TradingPositionLong<DecimalType> pos(tickerSymbol,
					 entry0->getOpenValue(),
					 *entry0,
					 oneContract);
    REQUIRE_THROWS_AS(pos.setProfitTarget(createDecimal("-1.0")), TradingPositionException);
    REQUIRE_THROWS_AS(pos.setStopLoss(createDecimal("-0.5")), TradingPositionException);
  }

  SECTION("Invalid R‐multiple stop") {
    TradingPositionLong<DecimalType> pos(tickerSymbol,
					 entry0->getOpenValue(),
					 *entry0,
					 oneContract);
    REQUIRE_THROWS_AS(pos.setRMultipleStop(DecimalConstants<DecimalType>::DecimalZero), TradingPositionException);
  }

  SECTION("Closing a position with an exit date before its entry date throws") {
    // Arrange: open a fresh long position
    TradingPositionLong<DecimalType> pos(
      tickerSymbol,
      entry0->getOpenValue(),
      *entry0,
      oneContract
    );

    // Act: try to close it one day before entry
    date beforeEntry = pos.getEntryDate() - days(1);

    // Assert: should blow up with a domain_error
    REQUIRE_THROWS_AS(
      pos.ClosePosition(beforeEntry, entry0->getOpenValue()),
      std::domain_error
    );
  }

  SECTION("Adding the same bar twice to an open position throws") {
    // Arrange: open a fresh long position (initial history contains only entry0)
    TradingPositionLong<DecimalType> pos(
					 tickerSymbol,
					 entry0->getOpenValue(),
					 *entry0,
					 oneContract
					 );

    // First time we add entry1 (a new date), no exception:
    REQUIRE_NOTHROW(pos.addBar(*entry1));

    // Second time we add entry1 again, should hit the duplicate‐date guard:
    REQUIRE_THROWS_AS(pos.addBar(*entry1), std::domain_error);
  }

  SECTION("TradingPositionLong intraday ptime close and getters", "[TradingPosition][ptime]") {
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    // 1) Create an INTRADAY bar at 2025-05-26 09:30:00
    auto entry = createTimeSeriesEntry(
        "20250526", "09:30:00",
        "100.0","105.0"," 95.0","102.0","10"
    ); // Intraday entry

    TradingPositionLong<DecimalType> pos(
        "SYM", entry->getOpenValue(), *entry, oneContract
    );

    // entry datetime round-trip
    ptime entryDT = entry->getDateTime();
    REQUIRE(pos.getEntryDateTime() == entryDT);
    REQUIRE(pos.getEntryDate()     == entryDT.date());

    // 2) Advance the series by one bar (09:31)
    auto nextBar = createTimeSeriesEntry(
        "20250526", "09:31:00",
        "102.0","106.0"," 96.0","103.0","5"
    );
    pos.addBar(*nextBar);

    // 3) Close intraday at 09:35
    ptime exitDT = time_from_string("2025-05-26 09:35:00");
    DecimalType exitPrice = createDecimal("104.25");
    pos.ClosePosition(exitDT, exitPrice); 

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitDateTime() == exitDT);
    REQUIRE(pos.getExitDate()     == exitDT.date());
    REQUIRE(pos.getExitPrice()    == exitPrice);
  }

SECTION("TradingPositionShort intraday ptime close and getters", "[TradingPosition][ptime]") {
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    // 1) Intraday entry at 2025-05-27 14:00:00
    auto entry = createTimeSeriesEntry(
        "20250527", "14:00:00",
        "200.0","205.0","195.0","201.5","20"
    ); // Intraday entry

    TradingPositionShort<DecimalType> pos(
        "ABC", entry->getOpenValue(), *entry, oneContract
    );

    // entry datetime round-trip
    ptime entryDT = entry->getDateTime();
    REQUIRE(pos.getEntryDateTime() == entryDT);
    REQUIRE(pos.getEntryDate()     == entryDT.date());

    // 2) Add one bar at 14:05
    auto nextBar = createTimeSeriesEntry(
        "20250527", "14:05:00",
        "201.5","206.0","196.0","202.0","15"
    );
    pos.addBar(*nextBar);

    // 3) Close intraday at 14:10
    ptime exitDT = time_from_string("2025-05-27 14:10:00");
    DecimalType exitPrice = createDecimal("199.75");
    pos.ClosePosition(exitDT, exitPrice);

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitDateTime() == exitDT);
    REQUIRE(pos.getExitDate()     == exitDT.date());
    REQUIRE(pos.getExitPrice()    == exitPrice);
 }
}

// ============================================================================
// TEST SUITE 1: Log Return Calculations
// ============================================================================

TEST_CASE("Log return calculations", "[TradingPosition][LogReturn]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Long position log return with price increase")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    auto bar2 = createTimeSeriesEntry("20250102", "102.0", "120.0", "100.0", "120.0", "1000");
    pos.addBar(*bar2);
    
    // Log return = ln(120/100) = ln(1.2) ≈ 0.1823
    DecimalType expectedLogReturn = calculateLogTradeReturn(
      createDecimal("100.0"),
      createDecimal("120.0")
    );
    
    REQUIRE(pos.getLogTradeReturn() == expectedLogReturn);
  }

  SECTION("Long position log return with price decrease")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    auto bar2 = createTimeSeriesEntry("20250102", "102.0", "102.0", "80.0", "80.0", "1000");
    pos.addBar(*bar2);
    
    // Log return = ln(80/100) = ln(0.8) ≈ -0.2231
    DecimalType expectedLogReturn = calculateLogTradeReturn(
      createDecimal("100.0"),
      createDecimal("80.0")
    );
    
    REQUIRE(pos.getLogTradeReturn() == expectedLogReturn);
  }

  SECTION("Short position log return with price decrease (winning)")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    auto bar2 = createTimeSeriesEntry("20250102", "102.0", "102.0", "80.0", "80.0", "1000");
    pos.addBar(*bar2);
    
    // Log return for short = -ln(80/100) = -ln(0.8) ≈ 0.2231 (positive, winning)
    DecimalType expectedLogReturn = -calculateLogTradeReturn(
      createDecimal("100.0"),
      createDecimal("80.0")
    );
    
    REQUIRE(pos.getLogTradeReturn() == expectedLogReturn);
  }

  SECTION("Short position log return with price increase (losing)")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    auto bar2 = createTimeSeriesEntry("20250102", "102.0", "120.0", "100.0", "120.0", "1000");
    pos.addBar(*bar2);
    
    // Log return for short = -ln(120/100) = -ln(1.2) ≈ -0.1823 (negative, losing)
    DecimalType expectedLogReturn = -calculateLogTradeReturn(
      createDecimal("100.0"),
      createDecimal("120.0")
    );
    
    REQUIRE(pos.getLogTradeReturn() == expectedLogReturn);
  }

  SECTION("Closed long position log return")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    // Log return = ln(110/100) = ln(1.1)
    DecimalType expectedLogReturn = calculateLogTradeReturn(
      createDecimal("100.0"),
      createDecimal("110.0")
    );
    
    REQUIRE(pos.getLogTradeReturn() == expectedLogReturn);
  }

  SECTION("Closed short position log return")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("90.0"));
    
    // Log return for short = -ln(90/100) = -ln(0.9)
    DecimalType expectedLogReturn = -calculateLogTradeReturn(
      createDecimal("100.0"),
      createDecimal("90.0")
    );
    
    REQUIRE(pos.getLogTradeReturn() == expectedLogReturn);
  }
}

TEST_CASE("Log return error handling", "[TradingPosition][LogReturn][Error]")
{
  SECTION("calculateLogTradeReturn with zero reference price")
  {
    REQUIRE_THROWS_AS(
      calculateLogTradeReturn(
        DecimalConstants<DecimalType>::DecimalZero,
        createDecimal("100.0")
      ),
      std::domain_error
    );
  }

  SECTION("calculateLogTradeReturn with negative reference price")
  {
    REQUIRE_THROWS_AS(
      calculateLogTradeReturn(
        createDecimal("-50.0"),
        createDecimal("100.0")
      ),
      std::domain_error
    );
  }

  SECTION("calculateLogTradeReturn with zero second price")
  {
    REQUIRE_THROWS_AS(
      calculateLogTradeReturn(
        createDecimal("100.0"),
        DecimalConstants<DecimalType>::DecimalZero
      ),
      std::domain_error
    );
  }

  SECTION("calculateLogTradeReturn with negative second price")
  {
    REQUIRE_THROWS_AS(
      calculateLogTradeReturn(
        createDecimal("100.0"),
        createDecimal("-50.0")
      ),
      std::domain_error
    );
  }

  SECTION("calculateNaturalLog with zero")
  {
    REQUIRE_THROWS_AS(
      calculateNaturalLog(DecimalConstants<DecimalType>::DecimalZero),
      std::domain_error
    );
  }

  SECTION("calculateNaturalLog with negative number")
  {
    REQUIRE_THROWS_AS(
      calculateNaturalLog(createDecimal("-10.0")),
      std::domain_error
    );
  }
}

// ============================================================================
// TEST SUITE 2: R-Multiple Calculations (Extended)
// ============================================================================

TEST_CASE("R-multiple calculations - extensive", "[TradingPosition][RMultiple]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "100.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Long position R-multiple for winning trade")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("95.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    // R = (exit - entry) / (entry - stop) = (110 - 100) / (100 - 95) = 10 / 5 = 2.0
    DecimalType expectedR = createDecimal("2.0");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Long position R-multiple stopped out exactly at stop")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("95.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("95.0"));
    
    // When exited exactly at stop, R = -1.0
    // Formula: (95 - 100) / (100 - 95) = -5 / 5 = -1.0
    DecimalType expectedR = createDecimal("-1.0");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Long position R-multiple for small losing trade")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("95.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("97.0"));
    
    // R = (97 - 100) / (100 - 95) = -3 / 5 = -0.6
    DecimalType expectedR = createDecimal("-0.6");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Long position R-multiple for large losing trade past stop")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("95.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("90.0"));
    
    // R = (90 - 100) / (100 - 95) = -10 / 5 = -2.0
    DecimalType expectedR = createDecimal("-2.0");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Short position R-multiple for winning trade")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("105.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("90.0"));
    
    // R = (entry - exit) / (stop - entry) = (100 - 90) / (105 - 100) = 10 / 5 = 2.0
    DecimalType expectedR = createDecimal("2.0");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Short position R-multiple stopped out exactly")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("105.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("105.0"));
    
    // R = (100 - 105) / (105 - 100) = -5 / 5 = -1.0
    DecimalType expectedR = createDecimal("-1.0");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Short position R-multiple for small losing trade")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("105.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("103.0"));
    
    // R = (100 - 103) / (105 - 100) = -3 / 5 = -0.6
    DecimalType expectedR = createDecimal("-0.6");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Short position R-multiple for large losing trade")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("105.0"));
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    // R = (100 - 110) / (105 - 100) = -10 / 5 = -2.0
    DecimalType expectedR = createDecimal("-2.0");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("Cannot get R-multiple on open position")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("95.0"));
    
    REQUIRE_THROWS_AS(pos.getRMultiple(), TradingPositionException);
  }

  SECTION("Cannot get R-multiple when stop not set")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    REQUIRE_THROWS_AS(pos.getRMultiple(), TradingPositionException);
  }

  SECTION("R-multiple with very tight stop")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("99.5"));  // Very tight stop
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("105.0"));
    
    // R = (105 - 100) / (100 - 99.5) = 5 / 0.5 = 10.0
    DecimalType expectedR = createDecimal("10.0");
    REQUIRE(pos.getRMultiple() == expectedR);
  }

  SECTION("R-multiple with wide stop")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setRMultipleStop(createDecimal("50.0"));  // Wide stop (50% down)
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    // R = (110 - 100) / (100 - 50) = 10 / 50 = 0.2
    DecimalType expectedR = createDecimal("0.2");
    REQUIRE(pos.getRMultiple() == expectedR);
  }
}

// ============================================================================
// TEST SUITE 3: Double-Close and Invalid State Operations
// ============================================================================

TEST_CASE("Invalid operations on closed positions", "[TradingPosition][Error]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  auto bar2 = createTimeSeriesEntry("20250102", "102.0", "110.0", "100.0", "108.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Cannot close an already closed long position")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    date exitDate1(2025, Jan, 5);
    DecimalType exitPrice1(createDecimal("110.0"));
    pos.ClosePosition(exitDate1, exitPrice1);
    
    REQUIRE(pos.isPositionClosed());
    
    // Try to close again
    date exitDate2(2025, Jan, 6);
    DecimalType exitPrice2(createDecimal("115.0"));
    
    REQUIRE_THROWS_AS(
      pos.ClosePosition(exitDate2, exitPrice2),
      TradingPositionException
    );
  }

  SECTION("Cannot close an already closed short position")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    date exitDate1(2025, Jan, 5);
    DecimalType exitPrice1(createDecimal("90.0"));
    pos.ClosePosition(exitDate1, exitPrice1);
    
    REQUIRE(pos.isPositionClosed());
    
    // Try to close again with ptime overload
    ptime exitDateTime2 = time_from_string("2025-01-06 14:30:00");
    DecimalType exitPrice2(createDecimal("85.0"));
    
    REQUIRE_THROWS_AS(
      pos.ClosePosition(exitDateTime2, exitPrice2),
      TradingPositionException
    );
  }

  SECTION("Cannot add bar to closed long position")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    REQUIRE(pos.isPositionClosed());
    
    REQUIRE_THROWS_AS(
      pos.addBar(*bar2),
      TradingPositionException
    );
  }

  SECTION("Cannot add bar to closed short position")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("90.0"));
    
    REQUIRE(pos.isPositionClosed());
    
    REQUIRE_THROWS_AS(
      pos.addBar(*bar2),
      TradingPositionException
    );
  }
}

// ============================================================================
// TEST SUITE 4: Exit Price Validation
// ============================================================================

TEST_CASE("Exit price validation", "[TradingPosition][Validation]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Closing long position with zero exit price throws")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    REQUIRE_THROWS_AS(
      pos.ClosePosition(date(2025, Jan, 5), DecimalConstants<DecimalType>::DecimalZero),
      TradingPositionException
    );
  }

  SECTION("Closing long position with negative exit price throws")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    REQUIRE_THROWS_AS(
      pos.ClosePosition(date(2025, Jan, 5), createDecimal("-10.0")),
      TradingPositionException
    );
  }

  SECTION("Closing short position with zero exit price throws")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    REQUIRE_THROWS_AS(
      pos.ClosePosition(date(2025, Jan, 5), DecimalConstants<DecimalType>::DecimalZero),
      TradingPositionException
    );
  }

  SECTION("Closing short position with negative exit price throws")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    REQUIRE_THROWS_AS(
      pos.ClosePosition(date(2025, Jan, 5), createDecimal("-5.0")),
      TradingPositionException
    );
  }
}

// ============================================================================
// TEST SUITE 5: OpenPositionHistory Edge Cases
// ============================================================================

TEST_CASE("New OpenPositionHistory operations", "[OpenPositionHistory2]")
{
  auto entry1 = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  auto entry2 = createTimeSeriesEntry("20250102", "102.0", "108.0", "100.0", "106.0", "1000");
  auto entry3 = createTimeSeriesEntry("20250103", "106.0", "110.0", "104.0", "108.0", "1000");

  SECTION("Single bar position - first and last are same")
  {
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TradingPositionLong<DecimalType> pos("TEST", createDecimal("100.0"), *entry1, oneContract);
    
    REQUIRE(pos.getNumBarsInPosition() == 1);
    REQUIRE(pos.getEntryDate() == date(2025, Jan, 1));
    
    // First and last bar should be the same
    auto it = pos.beginPositionBarHistory();
    REQUIRE(it->first.date() == date(2025, Jan, 1));
  }

  SECTION("Bars stored in chronological order")
  {
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TradingPositionLong<DecimalType> pos("TEST", createDecimal("100.0"), *entry1, oneContract);
    
    // Add bars in order
    pos.addBar(*entry2);
    pos.addBar(*entry3);
    
    REQUIRE(pos.getNumBarsInPosition() == 3);
    
    // Verify chronological order
    auto it = pos.beginPositionBarHistory();
    REQUIRE(it->first.date() == date(2025, Jan, 1));
    ++it;
    REQUIRE(it->first.date() == date(2025, Jan, 2));
    ++it;
    REQUIRE(it->first.date() == date(2025, Jan, 3));
  }

  SECTION("numBarsSinceEntry increments correctly")
  {
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TradingPositionLong<DecimalType> pos("TEST", createDecimal("100.0"), *entry1, oneContract);
    
    REQUIRE(pos.getNumBarsSinceEntry() == 0);  // Entry bar
    
    pos.addBar(*entry2);
    REQUIRE(pos.getNumBarsSinceEntry() == 1);
    
    pos.addBar(*entry3);
    REQUIRE(pos.getNumBarsSinceEntry() == 2);
  }
}

// ============================================================================
// TEST SUITE 6: Copy Semantics
// ============================================================================

TEST_CASE("Position copy operations", "[TradingPosition][Copy]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  auto bar2 = createTimeSeriesEntry("20250102", "102.0", "108.0", "100.0", "106.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Long position copy constructor")
  {
    TradingPositionLong<DecimalType> original(symbol, createDecimal("100.0"), *entry, oneContract);
    original.addBar(*bar2);
    original.setProfitTarget(createDecimal("120.0"));
    original.setStopLoss(createDecimal("95.0"));
    
    TradingPositionLong<DecimalType> copy(original);
    
    REQUIRE(copy.getTradingSymbol() == original.getTradingSymbol());
    REQUIRE(copy.getEntryPrice() == original.getEntryPrice());
    REQUIRE(copy.getEntryDate() == original.getEntryDate());
    REQUIRE(copy.getNumBarsInPosition() == original.getNumBarsInPosition());
    REQUIRE(copy.getTradingUnits() == original.getTradingUnits());
    REQUIRE(copy.getProfitTarget() == original.getProfitTarget());
    REQUIRE(copy.getStopLoss() == original.getStopLoss());
    REQUIRE(copy.isPositionOpen() == original.isPositionOpen());
    
    // Position ID behavior - documents current behavior
    REQUIRE(copy.getPositionID() == original.getPositionID());
  }

  SECTION("Short position copy constructor")
  {
    TradingPositionShort<DecimalType> original(symbol, createDecimal("100.0"), *entry, oneContract);
    original.addBar(*bar2);
    
    TradingPositionShort<DecimalType> copy(original);
    
    REQUIRE(copy.getTradingSymbol() == original.getTradingSymbol());
    REQUIRE(copy.getEntryPrice() == original.getEntryPrice());
    REQUIRE(copy.getNumBarsInPosition() == original.getNumBarsInPosition());
  }

  SECTION("Position assignment operator")
  {
    TradingPositionLong<DecimalType> pos1(symbol, createDecimal("100.0"), *entry, oneContract);
    pos1.addBar(*bar2);
    
    auto entry2 = createTimeSeriesEntry("20250110", "200.0", "205.0", "195.0", "202.0", "1000");
    TradingPositionLong<DecimalType> pos2("OTHER", createDecimal("200.0"), *entry2, oneContract);
    
    pos2 = pos1;  // Assignment
    
    REQUIRE(pos2.getTradingSymbol() == pos1.getTradingSymbol());
    REQUIRE(pos2.getEntryPrice() == pos1.getEntryPrice());
    REQUIRE(pos2.getNumBarsInPosition() == pos1.getNumBarsInPosition());
  }

  SECTION("Self-assignment")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.addBar(*bar2);
    
    pos = pos;  // Self-assignment
    
    // Should still be valid
    REQUIRE(pos.getTradingSymbol() == symbol);
    REQUIRE(pos.getNumBarsInPosition() == 2);
  }

  SECTION("Copy of closed position")
  {
    TradingPositionLong<DecimalType> original(symbol, createDecimal("100.0"), *entry, oneContract);
    original.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    TradingPositionLong<DecimalType> copy(original);
    
    REQUIRE(copy.isPositionClosed());
    REQUIRE(copy.getExitPrice() == original.getExitPrice());
    REQUIRE(copy.getExitDate() == original.getExitDate());
    REQUIRE(copy.getPercentReturn() == original.getPercentReturn());
  }
}

// ============================================================================
// TEST SUITE 7: Multiple Observers
// ============================================================================

TEST_CASE("Multiple observers notification", "[TradingPosition][Observer]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Multiple observers all notified on long position close")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    TestTradingPositionObserver<DecimalType> obs1;
    TestTradingPositionObserver<DecimalType> obs2;
    TestTradingPositionObserver<DecimalType> obs3;
    
    pos.addObserver(obs1);
    pos.addObserver(obs2);
    pos.addObserver(obs3);
    
    REQUIRE_FALSE(obs1.isPositionClosed());
    REQUIRE_FALSE(obs2.isPositionClosed());
    REQUIRE_FALSE(obs3.isPositionClosed());
    
    date exitDate(2025, Jan, 5);
    DecimalType exitPrice(createDecimal("110.0"));
    pos.ClosePosition(exitDate, exitPrice);
    
    REQUIRE(obs1.isPositionClosed());
    REQUIRE(obs2.isPositionClosed());
    REQUIRE(obs3.isPositionClosed());
    
    REQUIRE(obs1.getExitPrice() == exitPrice);
    REQUIRE(obs2.getExitPrice() == exitPrice);
    REQUIRE(obs3.getExitPrice() == exitPrice);
    
    REQUIRE(obs1.getExitDate() == exitDate);
    REQUIRE(obs2.getExitDate() == exitDate);
    REQUIRE(obs3.getExitDate() == exitDate);
  }

  SECTION("Multiple observers all notified on short position close")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    TestTradingPositionObserver<DecimalType> obs1;
    TestTradingPositionObserver<DecimalType> obs2;
    
    pos.addObserver(obs1);
    pos.addObserver(obs2);
    
    date exitDate(2025, Jan, 5);
    DecimalType exitPrice(createDecimal("90.0"));
    pos.ClosePosition(exitDate, exitPrice);
    
    REQUIRE(obs1.isPositionClosed());
    REQUIRE(obs2.isPositionClosed());
  }
}

// ============================================================================
// TEST SUITE 8: Position ID Uniqueness
// ============================================================================

TEST_CASE("Position ID management", "[TradingPosition][ID]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Position IDs are unique across different positions")
  {
    TradingPositionLong<DecimalType> pos1(symbol, createDecimal("100.0"), *entry, oneContract);
    TradingPositionLong<DecimalType> pos2(symbol, createDecimal("100.0"), *entry, oneContract);
    TradingPositionShort<DecimalType> pos3(symbol, createDecimal("100.0"), *entry, oneContract);
    TradingPositionShort<DecimalType> pos4(symbol, createDecimal("100.0"), *entry, oneContract);
    
    REQUIRE(pos1.getPositionID() != pos2.getPositionID());
    REQUIRE(pos2.getPositionID() != pos3.getPositionID());
    REQUIRE(pos3.getPositionID() != pos4.getPositionID());
    REQUIRE(pos1.getPositionID() != pos3.getPositionID());
  }

  SECTION("Position IDs increment sequentially")
  {
    TradingPositionLong<DecimalType> pos1(symbol, createDecimal("100.0"), *entry, oneContract);
    uint32_t id1 = pos1.getPositionID();
    
    TradingPositionLong<DecimalType> pos2(symbol, createDecimal("100.0"), *entry, oneContract);
    uint32_t id2 = pos2.getPositionID();
    
    TradingPositionLong<DecimalType> pos3(symbol, createDecimal("100.0"), *entry, oneContract);
    uint32_t id3 = pos3.getPositionID();
    
    REQUIRE(id2 == id1 + 1);
    REQUIRE(id3 == id2 + 1);
  }

  SECTION("Copied position has same ID as original (documents current behavior)")
  {
    TradingPositionLong<DecimalType> original(symbol, createDecimal("100.0"), *entry, oneContract);
    uint32_t originalID = original.getPositionID();
    
    TradingPositionLong<DecimalType> copy(original);
    
    // Current behavior: copy has same ID
    // This may be a design decision or a bug
    REQUIRE(copy.getPositionID() == originalID);
  }
}

// ============================================================================
// TEST SUITE 9: OpenPositionBar Operations
// ============================================================================

TEST_CASE("New OpenPositionBar operations", "[OpenPositionBar2]")
{
  SECTION("OpenPositionBar equality with same data")
  {
    auto bar1 = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
    auto bar2 = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
    
    OpenPositionBar<DecimalType> opb1(*bar1);
    OpenPositionBar<DecimalType> opb2(*bar2);
    
    REQUIRE(opb1 == opb2);
    REQUIRE_FALSE(opb1 != opb2);
  }

  SECTION("OpenPositionBar inequality with different data")
  {
    auto bar1 = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
    auto bar2 = createTimeSeriesEntry("20250102", "102.0", "108.0", "100.0", "106.0", "1000");
    
    OpenPositionBar<DecimalType> opb1(*bar1);
    OpenPositionBar<DecimalType> opb2(*bar2);
    
    REQUIRE(opb1 != opb2);
    REQUIRE_FALSE(opb1 == opb2);
  }

  SECTION("OpenPositionBar copy constructor")
  {
    auto bar = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
    OpenPositionBar<DecimalType> original(*bar);
    
    OpenPositionBar<DecimalType> copy(original);
    
    REQUIRE(copy == original);
    REQUIRE(copy.getDate() == original.getDate());
    REQUIRE(copy.getOpenValue() == original.getOpenValue());
    REQUIRE(copy.getCloseValue() == original.getCloseValue());
  }

  SECTION("OpenPositionBar assignment operator")
  {
    auto bar1 = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
    auto bar2 = createTimeSeriesEntry("20250102", "102.0", "108.0", "100.0", "106.0", "1000");
    
    OpenPositionBar<DecimalType> opb1(*bar1);
    OpenPositionBar<DecimalType> opb2(*bar2);
    
    REQUIRE(opb1 != opb2);
    
    opb1 = opb2;
    
    REQUIRE(opb1 == opb2);
  }

  SECTION("OpenPositionBar accessor methods")
  {
    auto bar = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
    OpenPositionBar<DecimalType> opb(*bar);
    
    REQUIRE(opb.getDate() == date(2025, Jan, 1));
    REQUIRE(opb.getOpenValue() == createDecimal("100.0"));
    REQUIRE(opb.getHighValue() == createDecimal("105.0"));
    REQUIRE(opb.getLowValue() == createDecimal("95.0"));
    REQUIRE(opb.getCloseValue() == createDecimal("102.0"));
    REQUIRE(opb.getVolumeValue() == createDecimal("1000"));
  }
}

// ============================================================================
// TEST SUITE 10: Boundary Cases and Edge Conditions
// ============================================================================

TEST_CASE("Position calculations with extreme values", "[TradingPosition][Boundary]")
{
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Very small prices")
  {
    auto entry = createTimeSeriesEntry("20250101", "0.0001", "0.00012", "0.00009", "0.00011", "1000");
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("0.0001"), *entry, oneContract);
    
    auto bar2 = createTimeSeriesEntry("20250102", "0.00011", "0.00015", "0.0001", "0.00015", "1000");
    pos.addBar(*bar2);
    
    // Return = (0.00015 - 0.0001) / 0.0001 = 0.5 = 50%
    DecimalType expectedReturn = createDecimal("0.5");
    REQUIRE(pos.getTradeReturn() == expectedReturn);
  }

  SECTION("Very large prices")
  {
    auto entry = createTimeSeriesEntry("20250101", "1000000.0", "1050000.0", "950000.0", "1020000.0", "1000");
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("1000000.0"), *entry, oneContract);
    
    auto bar2 = createTimeSeriesEntry("20250102", "1020000.0", "1200000.0", "1000000.0", "1200000.0", "1000");
    pos.addBar(*bar2);
    
    // Return = (1200000 - 1000000) / 1000000 = 0.2 = 20%
    DecimalType expectedReturn = createDecimal("0.2");
    REQUIRE(pos.getTradeReturn() == expectedReturn);
  }

  SECTION("Very small price difference")
  {
    auto entry = createTimeSeriesEntry("20250101", "100.0", "100.05", "99.95", "100.0", "1000");
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    auto bar2 = createTimeSeriesEntry("20250102", "100.0", "100.01", "99.99", "100.001", "1000");
    pos.addBar(*bar2);
    
    // Very small positive return
    REQUIRE(pos.getTradeReturn() > DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(pos.getTradeReturn() < createDecimal("0.01"));
  }

  SECTION("Extreme percentage gain")
  {
    auto entry = createTimeSeriesEntry("20250101", "1.0", "1.2", "0.9", "1.0", "1000");
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("1.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("10.0"));
    
    // 900% return
    DecimalType expectedReturn = createDecimal("9.0");
    REQUIRE(pos.getTradeReturn() == expectedReturn);
    
    DecimalType expectedPercent = createDecimal("900.0");
    REQUIRE(pos.getPercentReturn() == expectedPercent);
  }

  SECTION("Extreme percentage loss")
  {
    auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "100.0", "1000");
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("1.0"));
    
    // 99% loss
    DecimalType expectedReturn = createDecimal("-0.99");
    REQUIRE(pos.getTradeReturn() == expectedReturn);
    
    DecimalType expectedPercent = createDecimal("-99.0");
    REQUIRE(pos.getPercentReturn() == expectedPercent);
  }
}

TEST_CASE("ClosedPosition equality operators", "[ClosedPosition][Equality]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Two identical closed long positions are equal")
  {
    TradingPositionLong<DecimalType> pos1(symbol, createDecimal("100.0"), *entry, oneContract);
    pos1.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    TradingPositionLong<DecimalType> pos2(symbol, createDecimal("100.0"), *entry, oneContract);
    pos2.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    // Note: This tests the state objects themselves via a workaround
    // Direct comparison of TradingPosition isn't possible without accessing state
    REQUIRE(pos1.getEntryPrice() == pos2.getEntryPrice());
    REQUIRE(pos1.getExitPrice() == pos2.getExitPrice());
    REQUIRE(pos1.getEntryDate() == pos2.getEntryDate());
    REQUIRE(pos1.getExitDate() == pos2.getExitDate());
  }

  SECTION("Closed long and short positions with same prices are not equal")
  {
    TradingPositionLong<DecimalType> longPos(symbol, createDecimal("100.0"), *entry, oneContract);
    longPos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    TradingPositionShort<DecimalType> shortPos(symbol, createDecimal("100.0"), *entry, oneContract);
    shortPos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    
    // Long and short are different types, so returns will differ
    REQUIRE(longPos.getPercentReturn() != shortPos.getPercentReturn());
  }
}

// ============================================================================
// TEST SUITE 11: Zero Profit Target and Stop Loss Edge Cases
// ============================================================================

TEST_CASE("Zero profit target and stop loss", "[TradingPosition][Validation]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Setting zero profit target")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    // Current implementation allows zero (>= comparison)
    REQUIRE_NOTHROW(pos.setProfitTarget(DecimalConstants<DecimalType>::DecimalZero));
    REQUIRE(pos.getProfitTarget() == DecimalConstants<DecimalType>::DecimalZero);
  }

  SECTION("Setting zero stop loss")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    // Current implementation allows zero (>= comparison)
    REQUIRE_NOTHROW(pos.setStopLoss(DecimalConstants<DecimalType>::DecimalZero));
    REQUIRE(pos.getStopLoss() == DecimalConstants<DecimalType>::DecimalZero);
  }
}

// ============================================================================
// TEST SUITE 12: Miscellaneous Edge Cases
// ============================================================================

TEST_CASE("Miscellaneous edge cases", "[TradingPosition][Edge]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Position with no price movement (entry == exit)")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("100.0"));
    
    REQUIRE(pos.getTradeReturn() == DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(pos.getPercentReturn() == DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE_FALSE(pos.isWinningPosition());
    REQUIRE(pos.isLosingPosition());  // Zero return is considered losing
  }

  SECTION("RMultipleStopSet flag behavior")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    REQUIRE_FALSE(pos.RMultipleStopSet());
    
    pos.setRMultipleStop(createDecimal("95.0"));
    REQUIRE(pos.RMultipleStopSet());
  }

  SECTION("Trading symbol stored correctly")
  {
    std::string longSymbol("AAPL");
    TradingPositionLong<DecimalType> longPos(longSymbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE(longPos.getTradingSymbol() == longSymbol);
    
    std::string shortSymbol("MSFT");
    TradingPositionShort<DecimalType> shortPos(shortSymbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE(shortPos.getTradingSymbol() == shortSymbol);
  }

  SECTION("Trading units preserved through position lifecycle")
  {
    TradingVolume fiveContracts(5, TradingVolume::CONTRACTS);
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    
    // Recreate with different units
    TradingPositionLong<DecimalType> pos2(symbol, createDecimal("100.0"), *entry, fiveContracts);
    REQUIRE(pos2.getTradingUnits() == fiveContracts);
    
    pos2.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));
    REQUIRE(pos2.getTradingUnits() == fiveContracts);  // Still the same after close
  }
}

// ============================================================================
// TEST SUITE 13: OrderType Utility Functions
// ============================================================================

TEST_CASE("OrderType utility functions", "[OrderType][Utility]")
{
  SECTION("orderTypeToString returns correct strings for all values")
  {
    REQUIRE(orderTypeToString(OrderType::MARKET_ON_OPEN_LONG)  == "MARKET_ON_OPEN_LONG");
    REQUIRE(orderTypeToString(OrderType::MARKET_ON_OPEN_SHORT) == "MARKET_ON_OPEN_SHORT");
    REQUIRE(orderTypeToString(OrderType::MARKET_ON_OPEN_SELL)  == "MARKET_ON_OPEN_SELL");
    REQUIRE(orderTypeToString(OrderType::MARKET_ON_OPEN_COVER) == "MARKET_ON_OPEN_COVER");
    REQUIRE(orderTypeToString(OrderType::SELL_AT_LIMIT)        == "SELL_AT_LIMIT");
    REQUIRE(orderTypeToString(OrderType::COVER_AT_LIMIT)       == "COVER_AT_LIMIT");
    REQUIRE(orderTypeToString(OrderType::SELL_AT_STOP)         == "SELL_AT_STOP");
    REQUIRE(orderTypeToString(OrderType::COVER_AT_STOP)        == "COVER_AT_STOP");
    REQUIRE(orderTypeToString(OrderType::UNKNOWN)              == "UNKNOWN");
  }

  SECTION("isEntryOrderType returns true only for entry types")
  {
    REQUIRE(isEntryOrderType(OrderType::MARKET_ON_OPEN_LONG));
    REQUIRE(isEntryOrderType(OrderType::MARKET_ON_OPEN_SHORT));

    REQUIRE_FALSE(isEntryOrderType(OrderType::MARKET_ON_OPEN_SELL));
    REQUIRE_FALSE(isEntryOrderType(OrderType::MARKET_ON_OPEN_COVER));
    REQUIRE_FALSE(isEntryOrderType(OrderType::SELL_AT_LIMIT));
    REQUIRE_FALSE(isEntryOrderType(OrderType::COVER_AT_LIMIT));
    REQUIRE_FALSE(isEntryOrderType(OrderType::SELL_AT_STOP));
    REQUIRE_FALSE(isEntryOrderType(OrderType::COVER_AT_STOP));
    REQUIRE_FALSE(isEntryOrderType(OrderType::UNKNOWN));
  }

  SECTION("isExitOrderType returns true only for exit types")
  {
    REQUIRE(isExitOrderType(OrderType::MARKET_ON_OPEN_SELL));
    REQUIRE(isExitOrderType(OrderType::MARKET_ON_OPEN_COVER));
    REQUIRE(isExitOrderType(OrderType::SELL_AT_LIMIT));
    REQUIRE(isExitOrderType(OrderType::COVER_AT_LIMIT));
    REQUIRE(isExitOrderType(OrderType::SELL_AT_STOP));
    REQUIRE(isExitOrderType(OrderType::COVER_AT_STOP));

    REQUIRE_FALSE(isExitOrderType(OrderType::MARKET_ON_OPEN_LONG));
    REQUIRE_FALSE(isExitOrderType(OrderType::MARKET_ON_OPEN_SHORT));
    REQUIRE_FALSE(isExitOrderType(OrderType::UNKNOWN));
  }
}

// ============================================================================
// TEST SUITE 14: OrderType Default State (Legacy Constructors)
// ============================================================================

TEST_CASE("OrderType default state with legacy constructors", "[TradingPosition][OrderType][Default]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Legacy long position constructor defaults both order types to UNKNOWN")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);

    REQUIRE(pos.getEntryOrderType() == OrderType::UNKNOWN);
    REQUIRE(pos.getExitOrderType()  == OrderType::UNKNOWN);
  }

  SECTION("Legacy short position constructor defaults both order types to UNKNOWN")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);

    REQUIRE(pos.getEntryOrderType() == OrderType::UNKNOWN);
    REQUIRE(pos.getExitOrderType()  == OrderType::UNKNOWN);
  }

  SECTION("hasKnownEntryOrderType is false for legacy long position")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_FALSE(pos.hasKnownEntryOrderType());
  }

  SECTION("hasKnownExitOrderType is false for newly opened legacy long position")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_FALSE(pos.hasKnownExitOrderType());
  }

  SECTION("hasKnownEntryOrderType is false for legacy short position")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_FALSE(pos.hasKnownEntryOrderType());
  }

  SECTION("Legacy ClosePosition(date, price) leaves exit order type as UNKNOWN")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.ClosePosition(date(2025, Jan, 5), createDecimal("110.0"));

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitOrderType() == OrderType::UNKNOWN);
    REQUIRE_FALSE(pos.hasKnownExitOrderType());
  }

  SECTION("Legacy ClosePosition(ptime, price) leaves exit order type as UNKNOWN")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("90.0"));

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitOrderType() == OrderType::UNKNOWN);
    REQUIRE_FALSE(pos.hasKnownExitOrderType());
  }
}

// ============================================================================
// TEST SUITE 15: OrderType Enhanced Constructors
// ============================================================================

TEST_CASE("OrderType enhanced constructors", "[TradingPosition][OrderType][Constructor]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Enhanced long constructor sets entry order type correctly")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);

    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
    REQUIRE(pos.hasKnownEntryOrderType());
  }

  SECTION("Enhanced long constructor leaves exit order type UNKNOWN until closed")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);

    REQUIRE(pos.getExitOrderType() == OrderType::UNKNOWN);
    REQUIRE_FALSE(pos.hasKnownExitOrderType());
  }

  SECTION("Enhanced short constructor sets entry order type correctly")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);

    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_SHORT);
    REQUIRE(pos.hasKnownEntryOrderType());
  }

  SECTION("Enhanced short constructor leaves exit order type UNKNOWN until closed")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);

    REQUIRE(pos.getExitOrderType() == OrderType::UNKNOWN);
    REQUIRE_FALSE(pos.hasKnownExitOrderType());
  }
}

// ============================================================================
// TEST SUITE 16: ClosePosition with OrderType
// ============================================================================

TEST_CASE("ClosePosition with OrderType parameter", "[TradingPosition][OrderType][ClosePosition]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Long position closed with SELL_AT_LIMIT records correct exit order type")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("110.0"), OrderType::SELL_AT_LIMIT);

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitOrderType() == OrderType::SELL_AT_LIMIT);
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Long position closed with SELL_AT_STOP records correct exit order type")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("95.0"), OrderType::SELL_AT_STOP);

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitOrderType() == OrderType::SELL_AT_STOP);
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Long position closed with MARKET_ON_OPEN_SELL records correct exit order type")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("101.0"), OrderType::MARKET_ON_OPEN_SELL);

    REQUIRE(pos.getExitOrderType() == OrderType::MARKET_ON_OPEN_SELL);
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Short position closed with COVER_AT_LIMIT records correct exit order type")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("90.0"), OrderType::COVER_AT_LIMIT);

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitOrderType() == OrderType::COVER_AT_LIMIT);
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Short position closed with COVER_AT_STOP records correct exit order type")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("105.0"), OrderType::COVER_AT_STOP);

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getExitOrderType() == OrderType::COVER_AT_STOP);
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Short position closed with MARKET_ON_OPEN_COVER records correct exit order type")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("99.0"), OrderType::MARKET_ON_OPEN_COVER);

    REQUIRE(pos.getExitOrderType() == OrderType::MARKET_ON_OPEN_COVER);
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Entry order type is not affected by ClosePosition with order type")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("110.0"), OrderType::SELL_AT_LIMIT);

    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
  }

  SECTION("Exit price and date are still correct when using enhanced ClosePosition")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    DecimalType exitPrice = createDecimal("112.50");
    pos.ClosePosition(exitDT, exitPrice, OrderType::SELL_AT_LIMIT);

    REQUIRE(pos.getExitPrice()    == exitPrice);
    REQUIRE(pos.getExitDateTime() == exitDT);
    REQUIRE(pos.getExitDate()     == exitDT.date());
  }

  SECTION("Observer still fires when using ClosePosition with order type")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    TestTradingPositionObserver<DecimalType> observer;
    pos.addObserver(observer);

    REQUIRE_FALSE(observer.isPositionClosed());

    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    DecimalType exitPrice = createDecimal("110.0");
    pos.ClosePosition(exitDT, exitPrice, OrderType::SELL_AT_LIMIT);

    REQUIRE(observer.isPositionClosed());
    REQUIRE(observer.getExitPrice() == exitPrice);
    REQUIRE(observer.getExitDate()  == exitDT.date());
  }
}

// ============================================================================
// TEST SUITE 17: setEntryOrderType and setExitOrderType direct setters
// ============================================================================

TEST_CASE("setEntryOrderType and setExitOrderType validation", "[TradingPosition][OrderType][Setters]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("setEntryOrderType accepts all valid entry types")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_NOTHROW(pos.setEntryOrderType(OrderType::MARKET_ON_OPEN_LONG));
    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
  }

  SECTION("setEntryOrderType accepts UNKNOWN as reset/default without throwing")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_NOTHROW(pos.setEntryOrderType(OrderType::UNKNOWN));
    REQUIRE(pos.getEntryOrderType() == OrderType::UNKNOWN);
  }

  SECTION("setEntryOrderType rejects exit-type values")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_THROWS(pos.setEntryOrderType(OrderType::SELL_AT_LIMIT));
    REQUIRE_THROWS(pos.setEntryOrderType(OrderType::SELL_AT_STOP));
    REQUIRE_THROWS(pos.setEntryOrderType(OrderType::MARKET_ON_OPEN_SELL));
    REQUIRE_THROWS(pos.setEntryOrderType(OrderType::COVER_AT_LIMIT));
    REQUIRE_THROWS(pos.setEntryOrderType(OrderType::COVER_AT_STOP));
    REQUIRE_THROWS(pos.setEntryOrderType(OrderType::MARKET_ON_OPEN_COVER));
  }

  SECTION("setExitOrderType accepts all valid exit types on open position")
  {
    {
      TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
      REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::SELL_AT_LIMIT));
      REQUIRE(pos.getExitOrderType() == OrderType::SELL_AT_LIMIT);
    }
    {
      TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
      REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::SELL_AT_STOP));
      REQUIRE(pos.getExitOrderType() == OrderType::SELL_AT_STOP);
    }
    {
      TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
      REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::MARKET_ON_OPEN_SELL));
      REQUIRE(pos.getExitOrderType() == OrderType::MARKET_ON_OPEN_SELL);
    }
    {
      TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
      REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::COVER_AT_LIMIT));
      REQUIRE(pos.getExitOrderType() == OrderType::COVER_AT_LIMIT);
    }
    {
      TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
      REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::COVER_AT_STOP));
      REQUIRE(pos.getExitOrderType() == OrderType::COVER_AT_STOP);
    }
    {
      TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
      REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::MARKET_ON_OPEN_COVER));
      REQUIRE(pos.getExitOrderType() == OrderType::MARKET_ON_OPEN_COVER);
    }
  }

  SECTION("setExitOrderType accepts UNKNOWN as no-op when already UNKNOWN")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::UNKNOWN));
    REQUIRE(pos.getExitOrderType() == OrderType::UNKNOWN);
  }

  SECTION("setExitOrderType rejects entry-type values")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_THROWS(pos.setExitOrderType(OrderType::MARKET_ON_OPEN_LONG));
    REQUIRE_THROWS(pos.setExitOrderType(OrderType::MARKET_ON_OPEN_SHORT));
  }

  SECTION("setExitOrderType UNKNOWN → UNKNOWN is always a no-op")
  {
    // Call multiple times with UNKNOWN — should never throw
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::UNKNOWN));
    REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::UNKNOWN));
    REQUIRE(pos.getExitOrderType() == OrderType::UNKNOWN);
  }

  SECTION("setExitOrderType KNOWN → UNKNOWN is a silent no-op (backward compat path)")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setExitOrderType(OrderType::SELL_AT_LIMIT);
    REQUIRE(pos.getExitOrderType() == OrderType::SELL_AT_LIMIT);

    // The backward-compat forwarding path always sends UNKNOWN — must not clobber
    REQUIRE_NOTHROW(pos.setExitOrderType(OrderType::UNKNOWN));
    REQUIRE(pos.getExitOrderType() == OrderType::SELL_AT_LIMIT); // unchanged
  }

  SECTION("setExitOrderType KNOWN → different KNOWN throws (genuine double-set)")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setExitOrderType(OrderType::SELL_AT_LIMIT);

    REQUIRE_THROWS_AS(pos.setExitOrderType(OrderType::SELL_AT_STOP), TradingPositionException);
  }

  SECTION("setExitOrderType KNOWN → same KNOWN throws (idempotent set is still rejected)")
  {
    // The immutability rule does not make an exception for the same value —
    // once set, it is frozen to prevent silent logic errors.
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    pos.setExitOrderType(OrderType::SELL_AT_LIMIT);

    REQUIRE_THROWS_AS(pos.setExitOrderType(OrderType::SELL_AT_LIMIT), TradingPositionException);
  }
}

// ============================================================================
// TEST SUITE 18: String representations
// ============================================================================

TEST_CASE("OrderType string representation methods", "[TradingPosition][OrderType][Strings]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("getEntryOrderTypeString returns UNKNOWN for legacy long position")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE(pos.getEntryOrderTypeString() == "UNKNOWN");
  }

  SECTION("getExitOrderTypeString returns UNKNOWN for open position")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract);
    REQUIRE(pos.getExitOrderTypeString() == "UNKNOWN");
  }

  SECTION("getEntryOrderTypeString reflects enhanced constructor value")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    REQUIRE(pos.getEntryOrderTypeString() == "MARKET_ON_OPEN_LONG");
  }

  SECTION("getExitOrderTypeString reflects exit type after enhanced ClosePosition")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("110.0"), OrderType::SELL_AT_LIMIT);

    REQUIRE(pos.getExitOrderTypeString() == "SELL_AT_LIMIT");
  }

  SECTION("getEntryOrderTypeString for short position")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);
    REQUIRE(pos.getEntryOrderTypeString() == "MARKET_ON_OPEN_SHORT");
  }

  SECTION("getExitOrderTypeString for short position closed with COVER_AT_LIMIT")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("90.0"), OrderType::COVER_AT_LIMIT);

    REQUIRE(pos.getExitOrderTypeString() == "COVER_AT_LIMIT");
  }
}

// ============================================================================
// TEST SUITE 19: Full position lifecycle with order type tracking
// ============================================================================

TEST_CASE("Full position lifecycle with order type tracking", "[TradingPosition][OrderType][Lifecycle]")
{
  auto entry = createTimeSeriesEntry("20250101", "100.0", "105.0", "95.0", "102.0", "1000");
  auto bar2   = createTimeSeriesEntry("20250102", "102.0", "112.0", "101.0", "111.0", "1000");
  auto bar3   = createTimeSeriesEntry("20250103", "111.0", "115.0", "110.0", "114.0", "1000");
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("TEST");

  SECTION("Multi-bar long position: entry and limit exit are both recorded correctly")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    pos.addBar(*bar2);
    pos.addBar(*bar3);

    REQUIRE(pos.isPositionOpen());
    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
    REQUIRE(pos.getExitOrderType()  == OrderType::UNKNOWN);

    ptime exitDT = time_from_string("2025-01-03 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("115.0"), OrderType::SELL_AT_LIMIT);

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
    REQUIRE(pos.getExitOrderType()  == OrderType::SELL_AT_LIMIT);
    REQUIRE(pos.hasKnownEntryOrderType());
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Multi-bar short position: entry and stop exit are both recorded correctly")
  {
    TradingPositionShort<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                          OrderType::MARKET_ON_OPEN_SHORT);
    pos.addBar(*bar2);

    REQUIRE(pos.isPositionOpen());
    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_SHORT);
    REQUIRE(pos.getExitOrderType()  == OrderType::UNKNOWN);

    ptime exitDT = time_from_string("2025-01-02 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("103.0"), OrderType::COVER_AT_STOP);

    REQUIRE(pos.isPositionClosed());
    REQUIRE(pos.getEntryOrderType() == OrderType::MARKET_ON_OPEN_SHORT);
    REQUIRE(pos.getExitOrderType()  == OrderType::COVER_AT_STOP);
    REQUIRE(pos.hasKnownEntryOrderType());
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Attempting to re-close an already closed position throws regardless of order type")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime firstExit = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(firstExit, createDecimal("110.0"), OrderType::SELL_AT_LIMIT);
    REQUIRE(pos.isPositionClosed());

    // Attempt to close again — should throw from the closed state's ClosePosition guard
    ptime secondExit = time_from_string("2025-01-06 16:00:00");
    REQUIRE_THROWS(pos.ClosePosition(secondExit, createDecimal("112.0"), OrderType::SELL_AT_STOP));
  }

  SECTION("hasKnownExitOrderType transitions correctly across position lifecycle")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);

    // Open — exit type unknown
    REQUIRE_FALSE(pos.hasKnownExitOrderType());

    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("110.0"), OrderType::SELL_AT_LIMIT);

    // Closed — exit type now known
    REQUIRE(pos.hasKnownExitOrderType());
  }

  SECTION("Trade return calculation is unaffected by order type tracking")
  {
    TradingPositionLong<DecimalType> pos(symbol, createDecimal("100.0"), *entry, oneContract,
                                         OrderType::MARKET_ON_OPEN_LONG);
    ptime exitDT = time_from_string("2025-01-05 16:00:00");
    pos.ClosePosition(exitDT, createDecimal("110.0"), OrderType::SELL_AT_LIMIT);

    DecimalType expectedReturn = createDecimal("0.1"); // (110 - 100) / 100
    REQUIRE(pos.getTradeReturn()    == expectedReturn);
    REQUIRE(pos.getExitOrderType()  == OrderType::SELL_AT_LIMIT);
  }
}
