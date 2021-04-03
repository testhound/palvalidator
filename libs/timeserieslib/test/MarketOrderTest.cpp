#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TradingOrder.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("Market Order Operations", "[TradingOrder]")
{
  date orderDate1(from_undelimited_string ("20151218"));
  date orderDate2(from_undelimited_string ("20150816"));
  date orderDate3(from_undelimited_string ("20150810"));
  date orderDate4(from_undelimited_string ("20160128"));
  date exitDate(from_undelimited_string ("20160210"));

  TradingVolume units(100, TradingVolume::SHARES);
  TradingVolume units2(1000, TradingVolume::SHARES);
  std::string symbol1("QQQ");
  std::string symbol2("SPY");
  std::string symbol3("NFLX");
  std::string symbol4("AAPL");

  DecimalType stopLoss1(dec::fromString<DecimalType>("0.5"));
  DecimalType profitTarget1(dec::fromString<DecimalType>("1.0"));

  DecimalType stopLoss2(dec::fromString<DecimalType>("1.10"));
  DecimalType profitTarget2(dec::fromString<DecimalType>("2.20"));

  MarketOnOpenLongOrder<DecimalType> longOrder1(symbol1, units, orderDate1);
  MarketOnOpenLongOrder<DecimalType> longOrder2(symbol2, units, orderDate2, stopLoss1, profitTarget1);
  MarketOnOpenLongOrder<DecimalType> longOrder3(symbol3, units2, orderDate3);
  MarketOnOpenLongOrder<DecimalType> longOrder4(symbol4, units2, orderDate4);

  MarketOnOpenSellOrder<DecimalType> longOrder1Exit(symbol1, units, exitDate);
  MarketOnOpenSellOrder<DecimalType> longOrder2Exit(symbol2, units, exitDate);
  MarketOnOpenSellOrder<DecimalType> longOrder3Exit(symbol3, units2, exitDate);
  MarketOnOpenSellOrder<DecimalType> longOrder4Exit(symbol4, units2, exitDate);

  MarketOnOpenShortOrder<DecimalType> shortOrder1(symbol1, units, orderDate1);
  MarketOnOpenShortOrder<DecimalType> shortOrder2(symbol2, units, orderDate2, stopLoss2, profitTarget2);
  MarketOnOpenShortOrder<DecimalType> shortOrder3(symbol3, units2, orderDate3);
  MarketOnOpenShortOrder<DecimalType> shortOrder4(symbol4, units2, orderDate4);

  MarketOnOpenCoverOrder<DecimalType> shortOrder1Exit(symbol1, units, exitDate);
  MarketOnOpenCoverOrder<DecimalType> shortOrder2Exit(symbol2, units, exitDate);
  MarketOnOpenCoverOrder<DecimalType> shortOrder3Exit(symbol3, units2, exitDate);
  MarketOnOpenCoverOrder<DecimalType> shortOrder4Exit(symbol4, units2, exitDate);
  
  REQUIRE (longOrder1.getTradingSymbol() == symbol1);
  REQUIRE (longOrder1.getUnitsInOrder() == units);
  REQUIRE (longOrder1.getOrderDate() == orderDate1);
  REQUIRE (longOrder1.getOrderPriority() == 1);
  REQUIRE (longOrder1.getStopLoss() == DecimalConstants<DecimalType>::DecimalZero);
  REQUIRE (longOrder1.getProfitTarget() == DecimalConstants<DecimalType>::DecimalZero);

  //  REQUIRE (longOrder1.getOrderID() == 1);

  REQUIRE (longOrder1Exit.getTradingSymbol() == symbol1);
  REQUIRE (longOrder1Exit.getUnitsInOrder() == units);
  REQUIRE (longOrder1Exit.getOrderDate() == exitDate);
  REQUIRE (longOrder1Exit.getOrderPriority() == 1);

  REQUIRE (longOrder2.getTradingSymbol() == symbol2);
  REQUIRE (longOrder2.getUnitsInOrder() == units);
  REQUIRE (longOrder2.getOrderDate() == orderDate2);
  REQUIRE (longOrder2.getStopLoss() == stopLoss1);
  REQUIRE (longOrder2.getProfitTarget() == profitTarget1);

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

  REQUIRE (longOrder1.isEntryOrder() == true);
  REQUIRE (longOrder2.isEntryOrder() == true);
  REQUIRE (longOrder3.isEntryOrder() == true);
  REQUIRE (longOrder4.isEntryOrder() == true);

  REQUIRE_FALSE (longOrder1Exit.isEntryOrder());
  REQUIRE_FALSE (longOrder2Exit.isEntryOrder());
  REQUIRE_FALSE (longOrder3Exit.isEntryOrder());
  REQUIRE_FALSE (longOrder4Exit.isEntryOrder());

  REQUIRE (longOrder1Exit.isExitOrder());
  REQUIRE (longOrder2Exit.isExitOrder());
  REQUIRE (longOrder3Exit.isExitOrder());
  REQUIRE (longOrder4Exit.isExitOrder());

  REQUIRE (longOrder1Exit.isLongOrder());
  REQUIRE (longOrder2Exit.isLongOrder());
  REQUIRE (longOrder3Exit.isLongOrder());
  REQUIRE (longOrder4Exit.isLongOrder());

  /////////////////////////////////////////////

  REQUIRE (shortOrder1.getTradingSymbol() == symbol1);
  REQUIRE (shortOrder1.getUnitsInOrder() == units);
  REQUIRE (shortOrder1.getOrderDate() == orderDate1);
  REQUIRE (shortOrder1.getOrderPriority() == 1);
  REQUIRE (shortOrder1.getStopLoss() == DecimalConstants<DecimalType>::DecimalZero);
  REQUIRE (shortOrder1.getProfitTarget() == DecimalConstants<DecimalType>::DecimalZero);

  REQUIRE (shortOrder1Exit.getTradingSymbol() == symbol1);
  REQUIRE (shortOrder1Exit.getUnitsInOrder() == units);
  REQUIRE (shortOrder1Exit.getOrderDate() == exitDate);
  REQUIRE (shortOrder1Exit.getOrderPriority() == 1);

  //REQUIRE (shortOrder1.getOrderID() == 5);

  REQUIRE (shortOrder2.getTradingSymbol() == symbol2);
  REQUIRE (shortOrder2.getUnitsInOrder() == units);
  REQUIRE (shortOrder2.getOrderDate() == orderDate2);
  REQUIRE (shortOrder2.getStopLoss() == stopLoss2);
  REQUIRE (shortOrder2.getProfitTarget() == profitTarget2);

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

  REQUIRE (shortOrder1.isEntryOrder() == true);
  REQUIRE (shortOrder2.isEntryOrder() == true);
  REQUIRE (shortOrder3.isEntryOrder() == true);
  REQUIRE (shortOrder4.isEntryOrder() == true);

  REQUIRE (shortOrder1.isExitOrder() == false);
  REQUIRE (shortOrder2.isExitOrder() == false);
  REQUIRE (shortOrder3.isExitOrder() == false);
  REQUIRE (shortOrder4.isExitOrder() == false);

  REQUIRE_FALSE (shortOrder1Exit.isEntryOrder());
  REQUIRE_FALSE (shortOrder2Exit.isEntryOrder());
  REQUIRE_FALSE (shortOrder3Exit.isEntryOrder());
  REQUIRE_FALSE (shortOrder4Exit.isEntryOrder());

  REQUIRE (shortOrder1Exit.isExitOrder());
  REQUIRE (shortOrder2Exit.isExitOrder());
  REQUIRE (shortOrder3Exit.isExitOrder());
  REQUIRE (shortOrder4Exit.isExitOrder());

  REQUIRE (shortOrder1Exit.isShortOrder());
  REQUIRE (shortOrder2Exit.isShortOrder());
  REQUIRE (shortOrder3Exit.isShortOrder());
  REQUIRE (shortOrder4Exit.isShortOrder());

  REQUIRE_FALSE (shortOrder1Exit.isLongOrder());
  REQUIRE_FALSE (shortOrder2Exit.isLongOrder());
  REQUIRE_FALSE (shortOrder3Exit.isLongOrder());
  REQUIRE_FALSE (shortOrder4Exit.isLongOrder());

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
    date fillDate(from_undelimited_string ("20151221"));
    DecimalType fillPrice(dec::fromString<DecimalType>("110.87"));

    REQUIRE (longOrder1.isOrderPending() == true);

    longOrder1.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE (longOrder1.isOrderExecuted() == true);
    REQUIRE (longOrder1.isOrderPending() == false);
    REQUIRE (longOrder1.isOrderCanceled() == false);
    REQUIRE (longOrder1.getFillPrice() == fillPrice);
    REQUIRE (longOrder1.getFillDate() == fillDate);
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
    date fillDate(from_undelimited_string ("20150817"));
    DecimalType fillPrice(dec::fromString<DecimalType>("115.03"));

    REQUIRE (longOrder2.isOrderPending() == true);
    longOrder2.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE_THROWS (longOrder2.MarkOrderCanceled());
  }

  SECTION ("Throw exception if attempt to execute canceled order (short side)")
  {
    date fillDate(from_undelimited_string ("20150817"));
    DecimalType fillPrice(dec::fromString<DecimalType>("115.03"));

    REQUIRE (shortOrder2.isOrderPending() == true);
    shortOrder2.MarkOrderExecuted (fillDate, fillPrice);
    REQUIRE_THROWS (shortOrder2.MarkOrderCanceled());
  }

 SECTION ("Throw exception if attempt to execute canceled order")
  {
    date fillDate(from_undelimited_string ("20150817"));
    DecimalType fillPrice(dec::fromString<DecimalType>("115.03"));

    longOrder2.MarkOrderCanceled();
    REQUIRE (longOrder2.isOrderCanceled() == true);
    REQUIRE_THROWS (longOrder2.MarkOrderExecuted (fillDate, fillPrice));
  }

 SECTION ("Throw exception if execution date is before order date")
  {
    date fillDate(from_undelimited_string ("20151210"));
    DecimalType fillPrice(dec::fromString<DecimalType>("110.87"));

    REQUIRE (longOrder1.isOrderPending() == true);

    REQUIRE_THROWS (longOrder1.MarkOrderExecuted (fillDate, fillPrice));
  }
}
