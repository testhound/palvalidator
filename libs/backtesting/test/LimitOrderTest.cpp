#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TradingOrder.h"
#include "TestUtils.h"

using boost::posix_time::ptime;
using boost::posix_time::time_from_string;
using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("Market Order Operations", "[LimitOrder]")
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

  SellAtLimitOrder<DecimalType> longOrder1(symbol1, units, orderDate1, createDecimal("111.90"));   // hit on 12/22/2015
  SellAtLimitOrder<DecimalType> longOrder2(symbol2, units, orderDate2, createDecimal("210.00"));   // hot on 8/18/2015
  SellAtLimitOrder<DecimalType> longOrder3(symbol3, units2, orderDate3, createDecimal("126.76"));  // hit on 8/18/2015
  SellAtLimitOrder<DecimalType> longOrder4(symbol4, units2, orderDate4, createDecimal("96.50"));   // hit on 1/29/2016

  CoverAtLimitOrder<DecimalType> shortOrder1(symbol1, units, orderDate1,createDecimal("109.00")); // hit on 1/4/2016
  CoverAtLimitOrder<DecimalType> shortOrder2(symbol2, units, orderDate2,createDecimal("200.00")); // hit on 8/21/2015
  CoverAtLimitOrder<DecimalType> shortOrder3(symbol3, units2, orderDate3,createDecimal("119.90")); // hit on 8/12/2015
  CoverAtLimitOrder<DecimalType> shortOrder4(symbol4, units2, orderDate4, createDecimal("93.00")); // hit on  1/28/2016


  REQUIRE (longOrder1.getTradingSymbol() == symbol1);
  REQUIRE (longOrder1.getUnitsInOrder() == units);
  REQUIRE (longOrder1.getOrderDate() == orderDate1);
  REQUIRE (longOrder1.getOrderPriority() == 10);
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
  REQUIRE (shortOrder1.getOrderPriority() == 10);

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
    date fillDate(from_undelimited_string ("20151222"));
    DecimalType fillPrice(dec::fromString<DecimalType>("111.93"));

    REQUIRE (longOrder1.isOrderPending() == true);

    longOrder1.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE (longOrder1.isOrderExecuted() == true);
    REQUIRE (longOrder1.isOrderPending() == false);
    REQUIRE (longOrder1.isOrderCanceled() == false);
    REQUIRE (longOrder1.getFillPrice() == fillPrice);
    REQUIRE (longOrder1.getFillDate() == fillDate);
  }

  SECTION ("Throw exception if long fill price is less than limit price")
  {
    date fillDate(from_undelimited_string ("20151222"));
    DecimalType fillPrice(dec::fromString<DecimalType>("111.89"));

    REQUIRE (longOrder1.isOrderPending() == true);

    // fill price is less than long limit price
    REQUIRE_THROWS (longOrder1.MarkOrderExecuted (fillDate, fillPrice));
  }

  SECTION ("Throw exception if short fill price is greater than limit price")
  {
    date fillDate(from_undelimited_string ("20160104"));
    DecimalType fillPrice(dec::fromString<DecimalType>("109.03"));

    REQUIRE (shortOrder1.isOrderPending() == true);

    // fill price is less than long limit price
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
    DecimalType fillPrice(dec::fromString<DecimalType>("210.07"));

    REQUIRE (longOrder2.isOrderPending() == true);
    longOrder2.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE_THROWS (longOrder2.MarkOrderCanceled());
  }

  SECTION ("Throw exception if attempt to execute canceled order (short side)")
  {
    date fillDate(from_undelimited_string ("20150821"));
    DecimalType fillPrice(dec::fromString<DecimalType>("199.70"));

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

TEST_CASE("SellAtLimitOrder ptime ctor and getters", "[LimitOrder][ptime]") {
    // intraday timestamp
    ptime orderDt = time_from_string("2025-05-26 10:30:00");
    TradingVolume units(100, TradingVolume::SHARES);
    std::string symbol("AAPL");
    DecimalType limitPrice = createDecimal("150.00");

    SellAtLimitOrder<DecimalType> order(symbol, units, orderDt, limitPrice);
    // new ptime API
    REQUIRE(order.getOrderDateTime() == orderDt);
    // legacy date API still returns just the date
    REQUIRE(order.getOrderDate()     == orderDt.date());
    REQUIRE(order.getLimitPrice()    == limitPrice);
}

TEST_CASE("SellAtLimitOrder execute with ptime at or above limit", "[LimitOrder][ptime]") {
    ptime orderDt = time_from_string("2025-05-26 09:45:00");
    ptime fillDt  = time_from_string("2025-05-26 13:15:30");
    TradingVolume units(50, TradingVolume::SHARES);
    std::string symbol("MSFT");
    DecimalType limitPrice = createDecimal("120.50");
    DecimalType fillPrice  = createDecimal("121.00");

    SellAtLimitOrder<DecimalType> order(symbol, units, orderDt, limitPrice);
    order.MarkOrderExecuted(fillDt, fillPrice);
    REQUIRE(order.isOrderExecuted());
    REQUIRE(order.getFillDateTime() == fillDt);
    REQUIRE(order.getFillDate()     == fillDt.date());
    REQUIRE(order.getFillPrice()    == fillPrice);
}

TEST_CASE("SellAtLimitOrder execution below limit throws", "[LimitOrder][ptime]") {
    ptime orderDt = time_from_string("2025-05-26 14:00:00");
    ptime fillDt  = time_from_string("2025-05-26 14:05:00");
    TradingVolume units(200, TradingVolume::SHARES);
    std::string symbol("GOOG");
    DecimalType limitPrice = createDecimal("200.00");
    DecimalType badPrice   = createDecimal("199.99");

    SellAtLimitOrder<DecimalType> order(symbol, units, orderDt, limitPrice);
    REQUIRE_THROWS_AS(order.MarkOrderExecuted(fillDt, badPrice),
                      TradingOrderNotExecutedException);
    REQUIRE(order.isOrderPending());
}

// --- CoverAtLimitOrder intraday tests ---
TEST_CASE("CoverAtLimitOrder ptime ctor and getters", "[LimitOrder][ptime]") {
    ptime orderDt = time_from_string("2025-05-27 11:00:00");
    TradingVolume units(75, TradingVolume::SHARES);
    std::string symbol("SPY");
    DecimalType limitPrice = createDecimal("80.00");

    CoverAtLimitOrder<DecimalType> order(symbol, units, orderDt, limitPrice);
    REQUIRE(order.getOrderDateTime() == orderDt);
    REQUIRE(order.getOrderDate()     == orderDt.date());  // :contentReference[oaicite:2]{index=2}
    REQUIRE(order.getLimitPrice()    == limitPrice);
}

TEST_CASE("CoverAtLimitOrder execute with ptime at or below limit", "[LimitOrder][ptime]") {
    ptime orderDt = time_from_string("2025-05-27 09:15:00");
    ptime fillDt  = time_from_string("2025-05-27 16:00:00");
    TradingVolume units(150, TradingVolume::SHARES);
    std::string symbol("TSLA");
    DecimalType limitPrice = createDecimal("95.00");
    DecimalType fillPrice  = createDecimal("94.50");

    CoverAtLimitOrder<DecimalType> order(symbol, units, orderDt, limitPrice);
    order.MarkOrderExecuted(fillDt, fillPrice);
    REQUIRE(order.isOrderExecuted());
    REQUIRE(order.getFillDateTime() == fillDt);
    REQUIRE(order.getFillDate()     == fillDt.date());  // :contentReference[oaicite:3]{index=3}
    REQUIRE(order.getFillPrice()    == fillPrice);
}

TEST_CASE("CoverAtLimitOrder execution above limit throws", "[LimitOrder][ptime]") {
    ptime orderDt = time_from_string("2025-05-27 12:30:00");
    ptime fillDt  = time_from_string("2025-05-27 12:45:00");
    TradingVolume units(300, TradingVolume::SHARES);
    std::string symbol("AMZN");
    DecimalType limitPrice = createDecimal("50.00");
    DecimalType badPrice   = createDecimal("50.01");

    CoverAtLimitOrder<DecimalType> order(symbol, units, orderDt, limitPrice);
    REQUIRE_THROWS_AS(order.MarkOrderExecuted(fillDt, badPrice),
                      TradingOrderNotExecutedException);
    REQUIRE(order.isOrderPending());
}

