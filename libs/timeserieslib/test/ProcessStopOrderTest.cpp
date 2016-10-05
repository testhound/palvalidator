#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../BoostDateHelper.h"
#include "../TimeSeriesEntry.h"
#include "../TradingOrderManager.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef dec::decimal<7> DecimalType;
typedef OHLCTimeSeriesEntry<7> EntryType;

std::shared_ptr<DecimalType>
createDecimalPtr(const std::string& valueString)
{
  return std::make_shared<DecimalType> (fromString<DecimalType>(valueString));
}

DecimalType
createDecimal(const std::string& valueString)
{
  return fromString<DecimalType>(valueString);
}

std::shared_ptr<EntryType>
    createTimeSeriesEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    auto date1 = std::make_shared<date> (from_undelimited_string(dateString));
    auto open1 = std::make_shared<DecimalType> (fromString<DecimalType>(openPrice));
    auto high1 = std::make_shared<DecimalType> (fromString<DecimalType>(highPrice));
    auto low1 = std::make_shared<DecimalType> (fromString<DecimalType>(lowPrice));
    auto close1 = std::make_shared<DecimalType> (fromString<DecimalType>(closePrice));
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol, TimeFrame::DAILY);
  }


TEST_CASE ("ProcessOrderVisitor StopOrder Operations", "[ProcessOrderVisitor]")
{
  auto badLongOrderDay = createTimeSeriesEntry ("19871204","2715.81884765625","2740.41235351563",
						"2677.17211914063","2680.685546875", 0);

  auto longDay1 = createTimeSeriesEntry ("19871207","2663.11865234375","2694.73876953125","2649.0654296875",
					   "2694.73876953125", 0);
  auto longDay2 = createTimeSeriesEntry ("19871208","2701.765625","2708.79223632813","2670.1455078125",
					 "2684.19873046875",0);

  auto longDay3 = createTimeSeriesEntry ("19871209","2687.71215820313","2712.3056640625","2677.17211914063",
					 "2712.3056640625", 0);

  auto longDay4 = createTimeSeriesEntry ("19871210","2712.3056640625","2722.845703125","2701.765625",
				       "2719.33227539063",0);

  auto longDay5 = createTimeSeriesEntry ("19871211","2712.3056640625","2722.845703125","2694.73876953125",
					 "2705.27880859375",0);

  auto longDay6 = createTimeSeriesEntry ("19871214","2708.79223632813","2712.3056640625","2684.19873046875",
				       "2691.2255859375", 0);

  auto longDay7 = createTimeSeriesEntry ("19871215","2680.685546875","2684.19873046875","2645.55200195313",
				       "2649.0654296875", 0);

  auto longDay8 = createTimeSeriesEntry ("19871216","2624.47192382813","2627.98510742188","2592.85180664063",
				       "2617.44506835938", 0);
  

  /*
19861107,3114.5595703125,3137.16162109375,3100.99853515625,3132.64135742188
19861110,3128.12084960938,3146.20263671875,3110.03930664063,3114.5595703125
19861111,3100.99853515625,3119.080078125,3078.396484375,3082.91674804688
19861112,3082.91674804688,3155.24340820313,3078.396484375,3132.64135742188

   */

  auto shortSignalDate = createTimeSeriesEntry ("19861110","3128.12084960938","3146.20263671875",
						"3110.03930664063","3114.5595703125", 0);

  auto shortDay1 = createTimeSeriesEntry ("19861111","3100.99853515625","3119.080078125","3078.396484375",
					  "3082.91674804688", 0);
  auto shortDay2 = createTimeSeriesEntry ("19861112","3082.91674804688","3155.24340820313","3078.396484375",
					  "3132.64135742188", 0);
	
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string tickerSymbol("C2");

  SellAtStopOrder<7> longOrder1(tickerSymbol, oneContract, 
				 longDay1->getDateValue(), createDecimal ("2629.03073"));

  CoverAtStopOrder<7> shortOrder1(tickerSymbol, oneContract, 
				  shortDay1->getDateValue(), createDecimal ("3140.69132"));

  ProcessOrderVisitor<7> longOrder1Processor (longDay2);
  ProcessOrderVisitor<7> shortOrder1Processor (shortDay2);  
  

  SECTION ("Verify long orders are executed")
  {
    REQUIRE (longOrder1.isOrderPending() == true);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (longDay3);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (longDay4);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (longDay5);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (longDay6);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (longDay7);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (longDay8);
    longOrder1.accept (longOrder1Processor);
     REQUIRE_FALSE (longOrder1.isOrderPending());
    REQUIRE (longOrder1.isOrderExecuted());
    REQUIRE (longOrder1.getFillDate() == longDay8->getDateValue());
    REQUIRE (longOrder1.getFillPrice() <= longOrder1.getStopPrice());
    std::cout << "long order fill price = " << longOrder1.getFillPrice() << std::endl;
  }

  SECTION ("Verify exception thrown on bad processing date")
  {
    ProcessOrderVisitor<7> longOrderProcessorBad (badLongOrderDay);

    REQUIRE (longOrder1.isOrderPending() == true);
    REQUIRE_THROWS (longOrder1.accept (longOrderProcessorBad));
  }

  SECTION ("Verify exception thrown on canceled order")
  {
    REQUIRE (longOrder1.isOrderPending() == true);
    longOrder1.MarkOrderCanceled();
    REQUIRE_THROWS (longOrder1.accept (longOrder1Processor));

  }

  SECTION ("Verify short orders are executed")
  {
    REQUIRE (shortOrder1.isOrderPending() == true);
    shortOrder1.accept (shortOrder1Processor);

    REQUIRE_FALSE (shortOrder1.isOrderPending());
    REQUIRE (shortOrder1.isOrderExecuted());

    REQUIRE (shortOrder1.getFillDate() == shortDay2->getDateValue());
    REQUIRE (shortOrder1.getFillPrice() >= shortOrder1.getStopPrice());
    std::cout << "short order fill price = " << shortOrder1.getFillPrice() << std::endl;
  }

  SECTION ("Verify short exception thrown on bad processing date")
  {
    ProcessOrderVisitor<7> shortOrderProcessorBad (shortSignalDate);

    REQUIRE (shortOrder1.isOrderPending() == true);
    REQUIRE_THROWS (shortOrder1.accept (shortOrderProcessorBad));
  }

  SECTION ("Verify short exception thrown on canceled order")
  {
    REQUIRE (shortOrder1.isOrderPending() == true);
    shortOrder1.MarkOrderCanceled();
    REQUIRE_THROWS (shortOrder1.accept (shortOrder1Processor));

  }
}
