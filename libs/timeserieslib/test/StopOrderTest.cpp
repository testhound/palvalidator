#include <catch2/catch_test_macros.hpp>
#include "TradingOrder.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;



TEST_CASE ("Market Order Operations", "[TradingOrder]")
{
  date orderDate1(from_undelimited_string ("20151218"));
  date orderDate2(from_undelimited_string ("20150817"));
  date orderDate3(from_undelimited_string ("20150810"));
  date orderDate4(from_undelimited_string ("20160127"));
  TradingVolume units(100, TradingVolume::SHARES);
  TradingVolume units2(1000, TradingVolume::SHARES);
  std::string symbol1("QQQ");
  std::string symbol2("SPY");
  std::string symbol3("NFLX");
  std::string symbol4("AAPL");

  SellAtStopOrder<DecimalType> longOrder1(symbol1, units, orderDate1, createDecimal("108.00"));   // hit on 1/6/2016
  SellAtStopOrder<DecimalType> longOrder2(symbol2, units, orderDate2, createDecimal("205.00"));   // hot on 8/21/2015
  SellAtStopOrder<DecimalType> longOrder3(symbol3, units2, orderDate3, createDecimal("126.76"));  // hit on 8/18/2015
  SellAtStopOrder<DecimalType> longOrder4(symbol4, units2, orderDate4, createDecimal("96.50"));   // hit on 1/29/2016

  CoverAtStopOrder<DecimalType> shortOrder1(symbol1, units, orderDate1,createDecimal("112.00")); // hit on 12/23/2015
  CoverAtStopOrder<DecimalType> shortOrder2(symbol2, units, orderDate2,createDecimal("210.25")); // hit on 8/18/2015
  CoverAtStopOrder<DecimalType> shortOrder3(symbol3, units2, orderDate3,createDecimal("119.90")); // hit on 8/12/2015
  CoverAtStopOrder<DecimalType> shortOrder4(symbol4, units2, orderDate4, createDecimal("93.00")); // hit on  1/28/2016

  
  REQUIRE (longOrder1.getTradingSymbol() == symbol1);
  REQUIRE (longOrder1.getUnitsInOrder() == units);
  REQUIRE (longOrder1.getOrderDate() == orderDate1);
  REQUIRE (longOrder1.getOrderPriority() == 5);
  //  REQUIRE (longOrder1.getOrderID() == 1);

  REQUIRE (longOrder2.getTradingSymbol() == symbol2);
  REQUIRE (longOrder2.getUnitsInOrder() == units);
  REQUIRE (longOrder2.getOrderDate() == orderDate2);
  // REQUIRE (longOrder2.getOrderID() == 2);

  REQUIRE (longOrder3.getTradingSymbol() == symbol3);
  REQUIRE (longOrder3.getUnitsInOrder() == units2);
  REQUIRE (longOrder3.getOrderDate() == orderDate3);
  // REQUIRE (longOrder3.getOrderID() == 3);

  REQUIRE (longOrder4.getTradingSymbol() == symbol4);
  REQUIRE (longOrder4.getUnitsInOrder() == units2);
  REQUIRE (longOrder4.getOrderDate() == orderDate4);
  // REQUIRE (longOrder4.getOrderID() == 4);

  REQUIRE (longOrder1.isOrderPending() == true);
  REQUIRE (longOrder2.isOrderPending() == true);
  REQUIRE (longOrder3.isOrderPending() == true);
  REQUIRE (longOrder4.isOrderPending() == true);

  REQUIRE (longOrder1.isOrderExecuted() == false);
  REQUIRE (longOrder2.isOrderExecuted() == false);
  REQUIRE (longOrder3.isOrderExecuted() == false);
  REQUIRE (longOrder4.isOrderExecuted() == false);

  REQUIRE (longOrder1.isOrderCanceled() == false);
  REQUIRE (longOrder2.isOrderCanceled() == false);
  REQUIRE (longOrder3.isOrderCanceled() == false);
  REQUIRE (longOrder4.isOrderCanceled() == false);

  REQUIRE (longOrder1.isLongOrder() == true);
  REQUIRE (longOrder2.isLongOrder() == true);
  REQUIRE (longOrder3.isLongOrder() == true);
  REQUIRE (longOrder4.isLongOrder() == true);

  REQUIRE (longOrder1.isShortOrder() == false);
  REQUIRE (longOrder2.isShortOrder() == false);
  REQUIRE (longOrder3.isShortOrder() == false);
  REQUIRE (longOrder4.isShortOrder() == false);

  REQUIRE_FALSE (longOrder1.isEntryOrder());
  REQUIRE_FALSE (longOrder2.isEntryOrder());
  REQUIRE_FALSE (longOrder3.isEntryOrder());
  REQUIRE_FALSE (longOrder4.isEntryOrder());

  REQUIRE (longOrder1.isExitOrder());
  REQUIRE (longOrder2.isExitOrder());
  REQUIRE (longOrder3.isExitOrder());
  REQUIRE (longOrder4.isExitOrder());

  /////////////////////////////////////////////

  REQUIRE (shortOrder1.getTradingSymbol() == symbol1);
  REQUIRE (shortOrder1.getUnitsInOrder() == units);
  REQUIRE (shortOrder1.getOrderDate() == orderDate1);
  REQUIRE (shortOrder1.getOrderPriority() == 5);

  //REQUIRE (shortOrder1.getOrderID() == 5);

  REQUIRE (shortOrder2.getTradingSymbol() == symbol2);
  REQUIRE (shortOrder2.getUnitsInOrder() == units);
  REQUIRE (shortOrder2.getOrderDate() == orderDate2);
  //REQUIRE (shortOrder2.getOrderID() == 6);

  REQUIRE (shortOrder3.getTradingSymbol() == symbol3);
  REQUIRE (shortOrder3.getUnitsInOrder() == units2);
  REQUIRE (shortOrder3.getOrderDate() == orderDate3);
  //REQUIRE (shortOrder3.getOrderID() == 7);

  REQUIRE (shortOrder4.getTradingSymbol() == symbol4);
  REQUIRE (shortOrder4.getUnitsInOrder() == units2);
  REQUIRE (shortOrder4.getOrderDate() == orderDate4);
  //REQUIRE (shortOrder4.getOrderID() == 8);

  REQUIRE (shortOrder1.isOrderPending() == true);
  REQUIRE (shortOrder2.isOrderPending() == true);
  REQUIRE (shortOrder3.isOrderPending() == true);
  REQUIRE (shortOrder4.isOrderPending() == true);

  REQUIRE (shortOrder1.isOrderExecuted() == false);
  REQUIRE (shortOrder2.isOrderExecuted() == false);
  REQUIRE (shortOrder3.isOrderExecuted() == false);
  REQUIRE (shortOrder4.isOrderExecuted() == false);

  REQUIRE (shortOrder1.isOrderCanceled() == false);
  REQUIRE (shortOrder2.isOrderCanceled() == false);
  REQUIRE (shortOrder3.isOrderCanceled() == false);
  REQUIRE (shortOrder4.isOrderCanceled() == false);

  REQUIRE (shortOrder1.isLongOrder() == false);
  REQUIRE (shortOrder2.isLongOrder() == false);
  REQUIRE (shortOrder3.isLongOrder() == false);
  REQUIRE (shortOrder4.isLongOrder() == false);

  REQUIRE (shortOrder1.isShortOrder() == true);
  REQUIRE (shortOrder2.isShortOrder() == true);
  REQUIRE (shortOrder3.isShortOrder() == true);
  REQUIRE (shortOrder4.isShortOrder() == true);

  REQUIRE_FALSE (shortOrder1.isEntryOrder());
  REQUIRE_FALSE (shortOrder2.isEntryOrder());
  REQUIRE_FALSE (shortOrder3.isEntryOrder());
  REQUIRE_FALSE (shortOrder4.isEntryOrder());

  REQUIRE (shortOrder1.isExitOrder());
  REQUIRE (shortOrder2.isExitOrder());
  REQUIRE (shortOrder3.isExitOrder());
  REQUIRE (shortOrder4.isExitOrder());

  SECTION ("Verify orders are canceled")
  {
    longOrder1.MarkOrderCanceled();
    REQUIRE (longOrder1.isOrderPending() == false);
    REQUIRE (longOrder1.isOrderExecuted() == false);
    REQUIRE (longOrder1.isOrderCanceled() == true);

    shortOrder1.MarkOrderCanceled();
    REQUIRE (shortOrder1.isOrderPending() == false);
    REQUIRE (shortOrder1.isOrderExecuted() == false);
    REQUIRE (shortOrder1.isOrderCanceled() == true);
  }

  SECTION ("Verify orders are executed")
  {
    date fillDate(from_undelimited_string ("20160106"));
    DecimalType fillPrice(dec::fromString<DecimalType>("108.00"));

    REQUIRE (longOrder1.isOrderPending() == true);

    longOrder1.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE (longOrder1.isOrderExecuted() == true);
    REQUIRE (longOrder1.isOrderPending() == false);
    REQUIRE (longOrder1.isOrderCanceled() == false);
    REQUIRE (longOrder1.getFillPrice() == fillPrice);
    REQUIRE (longOrder1.getFillDate() == fillDate);
  }

  SECTION ("Throw exception if long stop price fill is greater than stop price")
  {
    date fillDate(from_undelimited_string ("20160106"));
    DecimalType fillPrice(dec::fromString<DecimalType>("108.52"));

    REQUIRE (longOrder1.isOrderPending() == true);

    // fill price is greater than long stop price
    REQUIRE_THROWS (longOrder1.MarkOrderExecuted (fillDate, fillPrice));
  }

  SECTION ("Throw exception if short stop fill price is less than stop price")
  {
    date fillDate(from_undelimited_string ("20151223"));
    DecimalType fillPrice(dec::fromString<DecimalType>("111.14"));

    REQUIRE (shortOrder1.isOrderPending() == true);

    // fill price is less than short stop price
    REQUIRE_THROWS (shortOrder1.MarkOrderExecuted (fillDate, fillPrice));
  }

  SECTION ("Throw exception if attempt to get fill price on pending order")
  {
    REQUIRE (shortOrder3.isOrderPending() == true);
    REQUIRE_THROWS (shortOrder3.getFillPrice());
  }

  SECTION ("Throw exception if attempt to get fill date on pending order")
  {
    REQUIRE (longOrder3.isOrderPending() == true);
    REQUIRE_THROWS (longOrder3.getFillDate());
  }

  SECTION ("Throw exception if attempt to get fill price on canceled order")
  {
    REQUIRE (longOrder3.isOrderPending() == true);
    longOrder3.MarkOrderCanceled();
    REQUIRE (longOrder3.isOrderCanceled());
    REQUIRE_THROWS (longOrder3.getFillPrice());

  }

  SECTION ("Throw exception if attempt to get fill date on canceled order")
  {
    REQUIRE (longOrder3.isOrderPending() == true);
    longOrder3.MarkOrderCanceled();
    REQUIRE (longOrder3.isOrderCanceled());
    REQUIRE_THROWS (longOrder3.getFillDate());

  }

  SECTION ("Throw exception if attempt to cancel executed order (long side)")
  {
    date fillDate(from_undelimited_string ("20150818"));
    DecimalType fillPrice(dec::fromString<DecimalType>("204.07"));

    REQUIRE (longOrder2.isOrderPending() == true);
    longOrder2.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE_THROWS (longOrder2.MarkOrderCanceled());
  }

  SECTION ("Throw exception if attempt to execute canceled order (short side)")
  {
    date fillDate(from_undelimited_string ("20150821"));
    DecimalType fillPrice(dec::fromString<DecimalType>("210.25"));

    REQUIRE (shortOrder2.isOrderPending() == true);
    shortOrder2.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE_THROWS (shortOrder2.MarkOrderCanceled());
  }

 SECTION ("Throw exception if attempt to execute canceled order")
  {
    date fillDate(from_undelimited_string ("20150818"));
    DecimalType fillPrice(dec::fromString<DecimalType>("210.00"));

    longOrder2.MarkOrderCanceled();
    REQUIRE (longOrder2.isOrderCanceled() == true);
    REQUIRE_THROWS (longOrder2.MarkOrderExecuted (fillDate, fillPrice));
  }

 SECTION ("Throw exception if execution date is before order date")
  {
    date fillDate(from_undelimited_string ("20151207"));
    DecimalType fillPrice(dec::fromString<DecimalType>("110.87"));

    REQUIRE (longOrder1.isOrderPending() == true);

    REQUIRE_THROWS (longOrder1.MarkOrderExecuted (fillDate, fillPrice));
  }
}
