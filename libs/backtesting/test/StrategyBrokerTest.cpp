#include <memory> 
#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "TimeSeriesCsvReader.h"
#include "ClosedPositionHistory.h"
#include "StrategyBroker.h"
#include "BoostDateHelper.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::ptime;
using boost::posix_time::time_from_string;

const static std::string myCornSymbol("@C");

TEST_CASE ("StrategyBroker operations", "[StrategyBroker]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();

  std::string futuresSymbol("@C");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));

  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol, 
						   futuresName, 
						   cornBigPointValue,
						   cornTickValue, 
						   p);

  std::string portName("Corn Portfolio");
  auto aPortfolio = std::make_shared<Portfolio<DecimalType>>(portName);

  aPortfolio->addSecurity (corn);

  StrategyBroker<DecimalType> aBroker(aPortfolio);

  REQUIRE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());
  REQUIRE (aBroker.getTotalTrades() == 0);
  REQUIRE (aBroker.getOpenTrades() == 0);
  REQUIRE (aBroker.getClosedTrades() == 0);

  SECTION ("StrategyBroker test going long on open")

  {
    DecimalType stopLoss(createDecimal("250.20"));
    DecimalType profitTarget(createDecimal("255.40"));

    aBroker.EnterLongOnOpen (futuresSymbol,
			     TimeSeriesDate (1985, Nov, 14),
			     oneContract,
			     stopLoss,
			     profitTarget);
    REQUIRE_FALSE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());
    StrategyBroker<DecimalType>::PendingOrderIterator it = aBroker.beginPendingOrders();
    REQUIRE (it->second->isOrderPending());
    REQUIRE (it->second->isMarketOrder());
    REQUIRE (it->second->isLongOrder());

    // Check the stop loss and profit target on the pending order
    std::shared_ptr<MarketEntryOrder<DecimalType>> entryOrder = std::dynamic_pointer_cast<MarketEntryOrder<DecimalType>>(it->second);
    REQUIRE (entryOrder->getStopLoss() == stopLoss);
    REQUIRE (entryOrder->getProfitTarget() == profitTarget);

    aBroker.ProcessPendingOrders (TimeSeriesDate (1985, Nov, 15));
    REQUIRE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());

    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 1);
    REQUIRE (aBroker.getClosedTrades() == 0);
  }

  
  SECTION ("StrategyBroker test going short on open")
  {
    DecimalType stopLoss(createDecimal("255.40"));
    DecimalType profitTarget(createDecimal("250.20"));

    aBroker.EnterShortOnOpen (futuresSymbol,
			      TimeSeriesDate (1985, Nov, 14) ,
			      oneContract,
			      stopLoss,
			      profitTarget);
    REQUIRE_FALSE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());
    StrategyBroker<DecimalType>::PendingOrderIterator it = aBroker.beginPendingOrders();
    REQUIRE (it->second->isOrderPending());
    REQUIRE (it->second->isMarketOrder());
    REQUIRE (it->second->isShortOrder());

    // Check the stop loss and profit target on the pending order
    std::shared_ptr<MarketEntryOrder<DecimalType>> entryOrder = std::dynamic_pointer_cast<MarketEntryOrder<DecimalType>>(it->second);
    REQUIRE (entryOrder->getStopLoss() == stopLoss);
    REQUIRE (entryOrder->getProfitTarget() == profitTarget);

    aBroker.ProcessPendingOrders (TimeSeriesDate (1985, Nov, 15));
    REQUIRE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());

    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 1);
    REQUIRE (aBroker.getClosedTrades() == 0);
  }
  
  SECTION ("StrategyBroker test going long on open and finding open position")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Nov, 14));
    aBroker.EnterLongOnOpen (futuresSymbol,
			   orderDate,
			   oneContract);
    TimeSeriesDate executionDate(TimeSeriesDate (1985, Nov, 15));

    aBroker.ProcessPendingOrders (executionDate);
    StrategyBroker<DecimalType>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
    REQUIRE_FALSE (it == aBroker.endStrategyTransactions());
    auto transaction = it->second;
    REQUIRE (transaction->getEntryTradingOrder()->getFillDate() == executionDate);
    REQUIRE (transaction->getEntryTradingOrder()->getOrderDate() == orderDate);

    REQUIRE (transaction->getTradingPosition()->isLongPosition());
    REQUIRE (transaction->getTradingPosition()->isPositionOpen());
    REQUIRE (transaction->getTradingPosition()->getEntryDate() == executionDate);

  }

  SECTION ("StrategyBroker test going short on open and finding open position")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Nov, 14));
    aBroker.EnterShortOnOpen (futuresSymbol,
			   orderDate,
			   oneContract);
 
    TimeSeriesDate executionDate(TimeSeriesDate (1985, Nov, 15));

    aBroker.ProcessPendingOrders (executionDate);

    StrategyBroker<DecimalType>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
    REQUIRE_FALSE (it == aBroker.endStrategyTransactions());
    auto transaction = it->second;
    REQUIRE (transaction->getEntryTradingOrder()->getFillDate() == executionDate);
    REQUIRE (transaction->getEntryTradingOrder()->getOrderDate() == orderDate);

    REQUIRE (transaction->getTradingPosition()->isShortPosition());
    REQUIRE (transaction->getTradingPosition()->isPositionOpen());
    REQUIRE (transaction->getTradingPosition()->getEntryDate() == executionDate);
  }

  SECTION ("StrategyBroker test going long on open and add exit orders")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1985, Nov, 15));
    aBroker.EnterLongOnOpen (futuresSymbol,
			   orderDate,
			   oneContract);
    TimeSeriesDate executionDate(TimeSeriesDate (1985, Nov, 18));
    TimeSeriesDate lastOrderDate0(TimeSeriesDate (1985, Dec, 2));
    TimeSeriesDate lastOrderDate1(TimeSeriesDate (1985, Dec, 3));

    aBroker.ProcessPendingOrders (executionDate);

    auto longProfitTarget = createDecimal("3758.32172");

    PercentNumber<DecimalType> stopPercent(PercentNumber<DecimalType>::createPercentNumber (createDecimal("1.28")));

    InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator instrPosIterator =
      aBroker.getInstrumentPosition(futuresSymbol).getInstrumentPosition(1);

    InstrumentPosition<DecimalType> instrPosition(aBroker.getInstrumentPosition(futuresSymbol));

    DecimalType entryPrice = (*instrPosIterator)->getEntryPrice();
    std::cout << "Entry price = " << entryPrice << std::endl << std::endl;
    TimeSeriesDate aOrderDate = executionDate;
    TimeSeriesDate aOrderExecutionDate;

    for (; (aOrderDate <= lastOrderDate0) && (instrPosition.isLongPosition()); 
	 aOrderDate = boost_next_weekday (aOrderDate))
      {
	aBroker.ExitLongAllUnitsAtLimit (futuresSymbol,
					 aOrderDate,
					 longProfitTarget);


	aBroker.ExitLongAllUnitsAtStop (futuresSymbol,
					aOrderDate,
					entryPrice,
					stopPercent);

	aOrderExecutionDate = boost_next_weekday (aOrderDate);

	aBroker.ProcessPendingOrders (aOrderExecutionDate);
	instrPosition = aBroker.getInstrumentPosition(futuresSymbol);
	REQUIRE (instrPosition.isLongPosition());
      }

    REQUIRE (aOrderDate == lastOrderDate1);
    aBroker.ExitLongAllUnitsAtLimit (futuresSymbol,
				     aOrderDate,
				     longProfitTarget);


    aBroker.ExitLongAllUnitsAtStop (futuresSymbol,
				    aOrderDate,
				    entryPrice,
				    stopPercent);

    aOrderExecutionDate = boost_next_weekday (aOrderDate);

    aBroker.ProcessPendingOrders (aOrderExecutionDate);
    instrPosition = aBroker.getInstrumentPosition(futuresSymbol);
    REQUIRE (instrPosition.isFlatPosition());

    StrategyBroker<DecimalType>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
    REQUIRE_FALSE (it == aBroker.endStrategyTransactions());
    auto transaction = it->second;
    REQUIRE (transaction->isTransactionComplete());
    REQUIRE_FALSE (transaction->isTransactionOpen());
  }

  SECTION ("StrategyBroker test going short on open and add exit orders")
  {
    TimeSeriesDate orderDate(TimeSeriesDate (1986, May, 28));

    aBroker.EnterShortOnOpen (futuresSymbol, orderDate, oneContract);
    TimeSeriesDate executionDate(TimeSeriesDate (1986, May, 29));
    TimeSeriesDate lastOrderDate0(TimeSeriesDate (1986, Jun, 9));
    TimeSeriesDate lastOrderDate1(TimeSeriesDate (1986, Jun, 10));

    aBroker.ProcessPendingOrders (executionDate);

    PercentNumber<DecimalType> stopPercent(PercentNumber<DecimalType>::createPercentNumber (createDecimal("1.28")));
    PercentNumber<DecimalType> profitPercent(PercentNumber<DecimalType>::createPercentNumber (createDecimal("1.34")));

    InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator instrPosIterator =
      aBroker.getInstrumentPosition(futuresSymbol).getInstrumentPosition(1);

    InstrumentPosition<DecimalType> instrPosition(aBroker.getInstrumentPosition(futuresSymbol));

    DecimalType entryPrice = (*instrPosIterator)->getEntryPrice();
    std::cout << "Entry price = " << entryPrice << std::endl << std::endl;
    TimeSeriesDate aOrderDate = executionDate;
    TimeSeriesDate aOrderExecutionDate;

    for (; (aOrderDate <= lastOrderDate0) && (instrPosition.isShortPosition()); 
	 aOrderDate = boost_next_weekday (aOrderDate))
      {
	aBroker.ExitShortAllUnitsAtLimit (futuresSymbol,
					  aOrderDate,
					  entryPrice,
					  profitPercent);


	aBroker.ExitShortAllUnitsAtStop (futuresSymbol,
					aOrderDate,
					entryPrice,
					stopPercent);

	aOrderExecutionDate = boost_next_weekday (aOrderDate);

	aBroker.ProcessPendingOrders (aOrderExecutionDate);
	instrPosition = aBroker.getInstrumentPosition(futuresSymbol);
	REQUIRE (instrPosition.isShortPosition());
      }

    REQUIRE (aOrderDate == lastOrderDate1);

    aBroker.ExitShortAllUnitsAtLimit (futuresSymbol,
					  aOrderDate,
					  entryPrice,
					  profitPercent);


    aBroker.ExitShortAllUnitsAtStop (futuresSymbol,
					aOrderDate,
					entryPrice,
					stopPercent);

    aOrderExecutionDate = boost_next_weekday (aOrderDate);

    aBroker.ProcessPendingOrders (aOrderExecutionDate);
    instrPosition = aBroker.getInstrumentPosition(futuresSymbol);
    REQUIRE (instrPosition.isFlatPosition());

    StrategyBroker<DecimalType>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
    REQUIRE_FALSE (it == aBroker.endStrategyTransactions());
    auto transaction = it->second;
    REQUIRE (transaction->isTransactionComplete());
    REQUIRE_FALSE (transaction->isTransactionOpen());
  }

  SECTION ("StrategyBroker test going long and short on open and add exit orders")
  {
    TimeSeriesDate longOrderDate(TimeSeriesDate (1985, Nov, 15));
    aBroker.EnterLongOnOpen (futuresSymbol,
			     longOrderDate,
			     oneContract);
    TimeSeriesDate longExecutionDate(TimeSeriesDate (1985, Nov, 18));
    TimeSeriesDate lastLongOrderDate0(TimeSeriesDate (1985, Dec, 2));
    TimeSeriesDate lastLongOrderDate1(TimeSeriesDate (1985, Dec, 3));

    aBroker.ProcessPendingOrders (longExecutionDate);

    auto longProfitTarget = createDecimal("3758.32172");

    PercentNumber<DecimalType> stopPercent(PercentNumber<DecimalType>::createPercentNumber (createDecimal("1.28")));

    InstrumentPosition<DecimalType>::ConstInstrumentPositionIterator instrPosIterator =
      aBroker.getInstrumentPosition(futuresSymbol).getInstrumentPosition(1);

    InstrumentPosition<DecimalType> instrPosition(aBroker.getInstrumentPosition(futuresSymbol));

    DecimalType entryPrice = (*instrPosIterator)->getEntryPrice();
    std::cout << "Long Entry price = " << entryPrice << std::endl << std::endl;
    TimeSeriesDate aOrderDate = longExecutionDate;
    TimeSeriesDate aOrderExecutionDate;

    for (; (aOrderDate <= lastLongOrderDate1) && (instrPosition.isLongPosition()); 
	 aOrderDate = boost_next_weekday (aOrderDate))
      {
	aBroker.ExitLongAllUnitsAtLimit (futuresSymbol,
					 aOrderDate,
					 longProfitTarget);


	aBroker.ExitLongAllUnitsAtStop (futuresSymbol,
					aOrderDate,
					entryPrice,
					stopPercent);

	aOrderExecutionDate = boost_next_weekday (aOrderDate);

	aBroker.ProcessPendingOrders (aOrderExecutionDate);
	instrPosition = aBroker.getInstrumentPosition(futuresSymbol);
      }

    StrategyBroker<DecimalType>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
    REQUIRE_FALSE (it == aBroker.endStrategyTransactions());
    auto transaction = it->second;
    REQUIRE (transaction->isTransactionComplete());
    REQUIRE_FALSE (transaction->isTransactionOpen());

    REQUIRE (aBroker.getInstrumentPosition(futuresSymbol).isFlatPosition());
    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 1);

    TimeSeriesDate shortOrderDate(TimeSeriesDate (1986, May, 28));

    aBroker.EnterShortOnOpen (futuresSymbol, shortOrderDate, oneContract);
    TimeSeriesDate shortExecutionDate(TimeSeriesDate (1986, May, 29));
    TimeSeriesDate lastShortOrderDate0(TimeSeriesDate (1986, Jun, 9));
    TimeSeriesDate lastShortOrderDate1(TimeSeriesDate (1986, Jun, 10));

    aBroker.ProcessPendingOrders (shortExecutionDate);

    PercentNumber<DecimalType> shortStopPercent(PercentNumber<DecimalType>::createPercentNumber (createDecimal("1.28")));
    PercentNumber<DecimalType> shortProfitPercent(PercentNumber<DecimalType>::createPercentNumber (createDecimal("1.34")));

    instrPosIterator = aBroker.getInstrumentPosition(futuresSymbol).getInstrumentPosition(1);

    instrPosition = aBroker.getInstrumentPosition(futuresSymbol);

    DecimalType shortEntryPrice = (*instrPosIterator)->getEntryPrice();
    std::cout << "Short Entry price = " << shortEntryPrice << std::endl << std::endl;
    aOrderDate = shortExecutionDate;

    for (; (aOrderDate <= lastShortOrderDate1) && (instrPosition.isShortPosition()); 
	 aOrderDate = boost_next_weekday (aOrderDate))
      {
	aBroker.ExitShortAllUnitsAtLimit (futuresSymbol,
					  aOrderDate,
					  shortEntryPrice,
					  shortProfitPercent);


	aBroker.ExitShortAllUnitsAtStop (futuresSymbol,
					aOrderDate,
					shortEntryPrice,
					shortStopPercent);

	aOrderExecutionDate = boost_next_weekday (aOrderDate);

	aBroker.ProcessPendingOrders (aOrderExecutionDate);
	instrPosition = aBroker.getInstrumentPosition(futuresSymbol);
      }

    REQUIRE (aBroker.getInstrumentPosition(futuresSymbol).isFlatPosition());
    REQUIRE (aBroker.getTotalTrades() == 2);
    REQUIRE (aBroker.getOpenTrades() == 0);
    REQUIRE (aBroker.getClosedTrades() == 2);

    ClosedPositionHistory<DecimalType> positions (aBroker.getClosedPositionHistory());
    REQUIRE (positions.getNumPositions() == 2);
    REQUIRE (positions.getNumWinningPositions() == 2);
    REQUIRE (positions.getNumLosingPositions() == 0);
    REQUIRE (positions.getPercentWinners() == DecimalConstants<DecimalType>::DecimalOneHundred);
  }

  SECTION("StrategyBroker test ExitLongAllUnitsOnOpen")
    {
      // 1) Enter long on open and process
      aBroker.EnterLongOnOpen(futuresSymbol, {1985, Nov, 14}, oneContract);
      aBroker.ProcessPendingOrders({1985, Nov, 15});
      REQUIRE(aBroker.isLongPosition(futuresSymbol));

      // 2) Queue a market‐on‐open exit for all units
      aBroker.ExitLongAllUnitsOnOpen(futuresSymbol, {1985, Dec, 1});
      auto pitLong = aBroker.beginPendingOrders();
      REQUIRE(pitLong != aBroker.endPendingOrders());
      REQUIRE(pitLong->second->isMarketOrder());
      REQUIRE(pitLong->second->isExitOrder());

      // 3) Execute the exit and verify flat & closed‐trade count
      aBroker.ProcessPendingOrders({1985, Dec, 2});
      REQUIRE(aBroker.isFlatPosition(futuresSymbol));
      REQUIRE(aBroker.getClosedTrades() == 1);
    }

  SECTION("StrategyBroker test ExitShortAllUnitsOnOpen")
    {
      // 1) Enter short on open and process
      aBroker.EnterShortOnOpen(futuresSymbol, {1986, May, 28}, oneContract);
      aBroker.ProcessPendingOrders({1986, May, 29});
      REQUIRE(aBroker.isShortPosition(futuresSymbol));

      // 2) Queue a market‐on‐open exit for all units
      aBroker.ExitShortAllUnitsOnOpen(futuresSymbol, {1986, Jun, 15});
      auto pitShort = aBroker.beginPendingOrders();
      REQUIRE(pitShort != aBroker.endPendingOrders());
      REQUIRE(pitShort->second->isMarketOrder());
      REQUIRE(pitShort->second->isExitOrder());

      // 3) Execute the exit and verify flat & closed‐trade count
      aBroker.ProcessPendingOrders({1986, Jun, 16});
      REQUIRE(aBroker.isFlatPosition(futuresSymbol));
      REQUIRE(aBroker.getClosedTrades() == 1);
    }

  // -------------------- Exception Tests: Long Side --------------------

  SECTION("StrategyBroker throws on ExitLongAllUnitsOnOpen when flat")
    {
      REQUIRE_THROWS_AS(
			aBroker.ExitLongAllUnitsOnOpen(futuresSymbol, {1985, Nov, 14}),
			StrategyBrokerException);
    }

  SECTION("StrategyBroker throws on ExitLongAllUnitsAtLimit when flat")
    {
      REQUIRE_THROWS_AS(
			aBroker.ExitLongAllUnitsAtLimit(futuresSymbol, {1985, Nov, 14}, createDecimal("100.00")),
			StrategyBrokerException);
    }

  SECTION("StrategyBroker throws on ExitLongAllUnitsAtStop when flat")
    {
      REQUIRE_THROWS_AS(
			aBroker.ExitLongAllUnitsAtStop(futuresSymbol, {1985, Nov, 14}, createDecimal("100.00")),
			StrategyBrokerException);
    }

  // -------------------- Exception Tests: Short Side --------------------

  SECTION("StrategyBroker does nothing on ExitShortAllUnitsOnOpen when flat")
    {
      // Currently this method does not throw; ensure no orders are queued
      aBroker.ExitShortAllUnitsOnOpen(futuresSymbol, {1986, May, 28});
      REQUIRE(aBroker.beginPendingOrders() == aBroker.endPendingOrders());
    }

  SECTION("StrategyBroker throws on ExitShortAllUnitsAtLimit when flat")
    {
      REQUIRE_THROWS_AS(
			aBroker.ExitShortAllUnitsAtLimit(futuresSymbol, {1986, May, 28}, createDecimal("100.00")),
			StrategyBrokerException);
    }

  SECTION("StrategyBroker throws on ExitShortAllUnitsAtStop when flat")
    {
      REQUIRE_THROWS_AS(
			aBroker.ExitShortAllUnitsAtStop(futuresSymbol, {1986, May, 28}, createDecimal("100.00")),
			StrategyBrokerException);
    }

SECTION("StrategyBroker ptime overloads preserve exact datetime", "[StrategyBroker][ptime]") {
    // 1) ptime-based entry on open
    ptime entryDT = time_from_string("1985-11-14 08:45:30");
    DecimalType stopLoss  = createDecimal("250.20");
    DecimalType profitTgt = createDecimal("255.40");
    aBroker.EnterLongOnOpen(futuresSymbol, entryDT, oneContract, stopLoss, profitTgt);

    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
    auto itMkt = aBroker.beginPendingOrders();
    auto moOrder = std::dynamic_pointer_cast<MarketOnOpenLongOrder<DecimalType>>(itMkt->second);
    REQUIRE(moOrder);                                       // it's our new ptime ctor
    REQUIRE(moOrder->getOrderDateTime() == entryDT);        // exact match
    REQUIRE(moOrder->getOrderDate()     == entryDT.date()); // date part matches
    aBroker.ProcessPendingOrders(entryDT.date());           // clean up for next test

    // 2) ptime-based ExitLongAllUnitsOnOpen
    // first reopen same position
    aBroker.EnterLongOnOpen(futuresSymbol, entryDT, oneContract);
    aBroker.ProcessPendingOrders(entryDT.date());
    REQUIRE(aBroker.isLongPosition(futuresSymbol));

    ptime exitDT = time_from_string("1985-11-15 09:12:00");
    aBroker.ExitLongAllUnitsOnOpen(futuresSymbol, exitDT);  // :contentReference[oaicite:1]{index=1}

    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
    auto itExit = aBroker.beginPendingOrders();
    auto exOrder = std::dynamic_pointer_cast<MarketOnOpenSellOrder<DecimalType>>(itExit->second);
    REQUIRE(exOrder);
    REQUIRE(exOrder->getOrderDateTime() == exitDT);
    REQUIRE(exOrder->getOrderDate()     == exitDT.date());
 }

 SECTION("StrategyBroker legacy date overloads use default bar time", "[StrategyBroker][ptime]") {
   using boost::gregorian::days;
   using mkc_timeseries::getDefaultBarTime;

   // 1) Submit a date-only short entry on 1985-11-14
   TimeSeriesDate d1(1985, Nov, 14);
   ptime defaultDT1(d1, getDefaultBarTime());

   DecimalType stopLoss  = createDecimal("255.40");
   DecimalType profitTgt = createDecimal("250.20");

   aBroker.EnterShortOnOpen(futuresSymbol, d1, oneContract, stopLoss, profitTgt);
   REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
   {
     auto it = aBroker.beginPendingOrders();
     auto moShort = std::dynamic_pointer_cast<MarketOnOpenShortOrder<DecimalType>>(it->second);
     REQUIRE(moShort);
     REQUIRE(moShort->getOrderDateTime() == defaultDT1);
     REQUIRE(moShort->getOrderDate()     == d1);
   }

   // process the entry on the next day (1985-11-15)
   TimeSeriesDate d2 = d1 + days(1);
   aBroker.ProcessPendingOrders(d2);
   REQUIRE(aBroker.isShortPosition(futuresSymbol));

   // 2) Now submit the date-only cover on 1985-11-15
   ptime defaultDT2(d2, getDefaultBarTime());
   aBroker.ExitShortAllUnitsOnOpen(futuresSymbol, d2);
   REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
   {
     auto itCover = aBroker.beginPendingOrders();
     auto cvOrder = std::dynamic_pointer_cast<MarketOnOpenCoverOrder<DecimalType>>(itCover->second);
     REQUIRE(cvOrder);
     REQUIRE(cvOrder->getOrderDateTime() == defaultDT2);
     REQUIRE(cvOrder->getOrderDate()     == d2);
     REQUIRE(cvOrder->getUnitsInOrder().getTradingVolume() == oneContract.getTradingVolume());
   }
 }

 SECTION("StrategyBroker ptime overloads preserve exact datetime", "[StrategyBroker][ptime]") {
    using boost::gregorian::days;

    // --- 1) ptime-based entry on open ---
    ptime entryDT = time_from_string("1985-11-14 08:45:30");
    DecimalType stopLoss  = createDecimal("250.20");
    DecimalType profitTgt = createDecimal("255.40");

    // queue the entry
    aBroker.EnterLongOnOpen(futuresSymbol, entryDT, oneContract, stopLoss, profitTgt);
    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());

    // inspect the queued MarketOnOpenLongOrder
    {
      auto itMkt  = aBroker.beginPendingOrders();
      auto moOrder = std::dynamic_pointer_cast<MarketOnOpenLongOrder<DecimalType>>(itMkt->second);
      REQUIRE(moOrder);
      REQUIRE(moOrder->getOrderDateTime() == entryDT);
      REQUIRE(moOrder->getOrderDate()     == entryDT.date());
    }

    // fill the entry on the *next* bar (one day later)
    auto fillDate1 = entryDT.date() + days(1);
    aBroker.ProcessPendingOrders(fillDate1);
    REQUIRE(aBroker.isLongPosition(futuresSymbol));

    // --- 2) ptime-based ExitLongAllUnitsOnOpen ---
    ptime exitDT = time_from_string("1985-11-15 09:12:00");

    // queue the exit
    aBroker.ExitLongAllUnitsOnOpen(futuresSymbol, exitDT);
    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());

    // inspect the queued MarketOnOpenSellOrder
    {
      auto itExit  = aBroker.beginPendingOrders();
      auto exOrder = std::dynamic_pointer_cast<MarketOnOpenSellOrder<DecimalType>>(itExit->second);
      REQUIRE(exOrder);
      REQUIRE(exOrder->getOrderDateTime() == exitDT);
      REQUIRE(exOrder->getOrderDate()     == exitDT.date());
    }
 }

 // 1) DATE-based market entry & exit on open
 SECTION("StrategyBroker date overloads for MarketOnOpen preserve default bar time", "[StrategyBroker][date]")
   {
    using boost::posix_time::ptime;
    using boost::posix_time::time_from_string;

    // pick a known default bar time
    auto barTime = getDefaultBarTime();

    // 1a) Long entry
    date d1(1985, Nov, 14);
    aBroker.EnterLongOnOpen(futuresSymbol, d1, oneContract);
    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
    {
      auto it = aBroker.beginPendingOrders();
      auto mo = std::dynamic_pointer_cast<MarketOnOpenLongOrder<DecimalType>>(it->second);
      REQUIRE(mo);
      // check that the ctor forwarded date→ptime(date, defaultTime)
      ptime expected = ptime(d1, barTime);
      REQUIRE(mo->getOrderDateTime() == expected);
      REQUIRE(mo->getOrderDate()     == d1);
    }
    aBroker.ProcessPendingOrders(d1 + days(1));
    REQUIRE(aBroker.isLongPosition(futuresSymbol));

    // 1b) Exit on open
    date d2(1985, Nov, 15);
    aBroker.ExitLongAllUnitsOnOpen(futuresSymbol, d2);
    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
    {
      auto it = aBroker.beginPendingOrders();
      auto mo = std::dynamic_pointer_cast<MarketOnOpenSellOrder<DecimalType>>(it->second);
      REQUIRE(mo);
      ptime expected = ptime(d2, barTime);
      REQUIRE(mo->getOrderDateTime() == expected);
      REQUIRE(mo->getOrderDate() == d2);
    }
 }

 // ——————————————————————————————————————————————————————————————————————————————
 // 2) PTIME-based short entry & exit on open
 SECTION ("StrategyBroker ptime overloads for ShortOnOpen preserve exact datetime", "[StrategyBroker][ptime]") {
    using boost::posix_time::time_from_string;
    using boost::gregorian::days;

    // Use a date that your CSV actually contains (e.g. 1985-11-14/15)
    ptime ent = time_from_string("1985-11-14 08:15:00");
    ptime ext = time_from_string("1985-11-14 14:45:00");

    // short entry
    aBroker.EnterShortOnOpen(futuresSymbol, ent, oneContract);
    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
    {
      auto it = aBroker.beginPendingOrders();
      auto mo = std::dynamic_pointer_cast<MarketOnOpenShortOrder<DecimalType>>(it->second);
      REQUIRE(mo);
      REQUIRE(mo->getOrderDateTime() == ent);
      REQUIRE(mo->getOrderDate()     == ent.date());
    }
    // process against the next day’s bar (1985-11-15)
    aBroker.ProcessPendingOrders(ent.date() + days(1));
    REQUIRE(aBroker.isShortPosition(futuresSymbol));

    // short exit
    aBroker.ExitShortAllUnitsOnOpen(futuresSymbol, ext);
    REQUIRE(aBroker.beginPendingOrders() != aBroker.endPendingOrders());
    {
      auto it = aBroker.beginPendingOrders();
      auto mo = std::dynamic_pointer_cast<MarketOnOpenCoverOrder<DecimalType>>(it->second);
      REQUIRE(mo);
      REQUIRE(mo->getOrderDateTime() == ext);
      REQUIRE(mo->getOrderDate() == ext.date());
    }
 }

 // ——————————————————————————————————————————————————————————————————————————————
 // 3) DATE-based limit exits (using 1985-11-14…17)

 // ——————————————————————————————————————————————————————————————————————————————
// 3) DATE-based limit exits (no fill assert)
SECTION("StrategyBroker date overloads for Exit…AtLimit forward to ptime with default time", "[StrategyBroker][date]") {
    using boost::gregorian::date;
    using boost::gregorian::days;
    using boost::posix_time::ptime;

    auto barT = getDefaultBarTime();
    DecimalType limitPrice = createDecimal("150.00");
    auto pct = PercentNumber<DecimalType>::createPercentNumber(createDecimal("1.00"));

    // 1) Open & fill a long on 1985-11-14
    date od(1985, Nov, 14);
    aBroker.EnterLongOnOpen(futuresSymbol, od, oneContract);
    aBroker.ProcessPendingOrders(od + days(1));
    REQUIRE(aBroker.isLongPosition(futuresSymbol));

    // 2) simple-price overload on 1985-11-16
    date dlim1 = od + days(2);
    aBroker.ExitLongAllUnitsAtLimit(futuresSymbol, dlim1, limitPrice);
    {
      auto it = aBroker.beginPendingOrders();
      auto lo = std::dynamic_pointer_cast<SellAtLimitOrder<DecimalType>>(it->second);
      REQUIRE(lo);
      ptime expected1(dlim1, barT);
      REQUIRE(lo->getOrderDateTime() == expected1);
      REQUIRE(lo->getOrderDate()     == dlim1);
      REQUIRE(lo->getLimitPrice()    == limitPrice);
    }

    // 3) percent-overload on 1985-11-17
    date dlim2 = od + days(3);
    aBroker.ExitLongAllUnitsAtLimit(futuresSymbol, dlim2, limitPrice, pct);
    {
      auto it = aBroker.beginPendingOrders();

      // skip the first (simple-price) order, inspect the second
      ++it;
      auto lo = std::dynamic_pointer_cast<SellAtLimitOrder<DecimalType>>(it->second);
      REQUIRE(lo);
      ptime expected2(dlim2, barT);
      REQUIRE(lo->getOrderDateTime() == expected2);
      // recompute the concrete limit price exactly as StrategyBroker does:
      LongProfitTarget<DecimalType> target(limitPrice, pct);
      DecimalType expectedPrice = num::Round2Tick(
        target.getProfitTarget(),
        aBroker.getTick(futuresSymbol),
        aBroker.getTickDiv2(futuresSymbol)
      );
      REQUIRE(lo->getLimitPrice() == expectedPrice);
    }
 }

 SECTION("StrategyBroker ptime overloads for Exit…AtStop preserve exact datetime", "[StrategyBroker][ptime]")
   {
    using boost::gregorian::days;
    using boost::posix_time::time_from_string;

    // Use a percentage that will trigger based on the actual data
    // Entry: 3679.89135742188, Low on next day: 3645.2841796875 (0.94% drop)
    // So use 0.5% to ensure the stop triggers
    auto pct = PercentNumber<DecimalType>::createPercentNumber(createDecimal("0.50"));

    // Open & fill a long on 1985-11-14
    ptime odt = time_from_string("1985-11-14 09:00:00");
    aBroker.EnterLongOnOpen(futuresSymbol, odt, oneContract);
    aBroker.ProcessPendingOrders(odt.date() + days(1));
    REQUIRE(aBroker.isLongPosition(futuresSymbol));

    // Get the actual entry price from the position
    auto instrPos = aBroker.getInstrumentPosition(futuresSymbol);
    auto posIterator = instrPos.getInstrumentPosition(1);
    DecimalType entryPrice = (*posIterator)->getEntryPrice();
    std::cout << "Entry price = " << entryPrice << std::endl;
    
    // Calculate stop price as a percentage below entry price
    LongStopLoss<DecimalType> stopLossCalc(entryPrice, pct);
    DecimalType rawStopPrice = stopLossCalc.getStopLoss();
    DecimalType stopPrice = num::Round2Tick(rawStopPrice,
 				    aBroker.getTick(futuresSymbol),
 				    aBroker.getTickDiv2(futuresSymbol));
    std::cout << "Raw stop price = " << rawStopPrice << std::endl;
    std::cout << "Rounded stop price = " << stopPrice << std::endl;

    // simple-stop overload at 1985-11-15 10:30:00
    ptime sdt = time_from_string("1985-11-15 10:30:00");
    aBroker.ExitLongAllUnitsAtStop(futuresSymbol, sdt, stopPrice);
    {
      auto it = aBroker.beginPendingOrders();
      auto so = std::dynamic_pointer_cast<SellAtStopOrder<DecimalType>>(it->second);
      REQUIRE(so);
      REQUIRE(so->getOrderDateTime() == sdt);
      REQUIRE(so->getStopPrice()      == stopPrice);
    }

    // fire the stop on the next trading bar (skips weekends/holidays)
    aBroker.ProcessPendingOrders(boost_next_weekday(sdt.date()));
    REQUIRE(aBroker.isFlatPosition(futuresSymbol));

    // percent-stop overload at 1985-11-15 14:45:00
    ptime s2 = time_from_string("1985-11-15 14:45:00");
    aBroker.EnterLongOnOpen(futuresSymbol, odt, oneContract);
    aBroker.ProcessPendingOrders(odt.date() + days(1));
    REQUIRE(aBroker.isLongPosition(futuresSymbol));

    // Get the new entry price for the second position
    instrPos = aBroker.getInstrumentPosition(futuresSymbol);
    posIterator = instrPos.getInstrumentPosition(1);
    DecimalType entryPrice2 = (*posIterator)->getEntryPrice();

    aBroker.ExitLongAllUnitsAtStop(futuresSymbol, s2, entryPrice2, pct);
    {
      auto it = aBroker.beginPendingOrders();
      auto so = std::dynamic_pointer_cast<SellAtStopOrder<DecimalType>>(it->second);
      REQUIRE(so);
      REQUIRE(so->getOrderDateTime() == s2);

      LongStopLoss<DecimalType> stopTarget(entryPrice2, pct);
      DecimalType expectedSL = num::Round2Tick(
        stopTarget.getStopLoss(),
        aBroker.getTick(futuresSymbol),
        aBroker.getTickDiv2(futuresSymbol)
      );
      REQUIRE(so->getStopPrice() == expectedSL);
    }
 }
}

TEST_CASE("Execution Tick Policies", "[StrategyBroker][TickPolicies]")
{
    // Common setup for all policy tests
    auto& factory = SecurityAttributesFactory<DecimalType>::instance();

    // Note: The factory is a singleton and already contains pre-initialized securities
    // We'll use existing securities from the factory instead of trying to add new ones
    
    // Use existing equity security (AAPL is already in the factory)
    std::string equitySymbol("AAPL");
    auto equityIt = factory.getSecurityAttributes(equitySymbol);
    REQUIRE(equityIt != factory.endSecurityAttributes());
    auto equityAttrs = equityIt->second;

    // Use existing futures security (@ES is already in the factory)
    std::string futuresSymbol("@ES");
    auto futuresIt = factory.getSecurityAttributes(futuresSymbol);
    REQUIRE(futuresIt != factory.endSecurityAttributes());
    auto futuresAttrs = futuresIt->second;

    DecimalType baseTickEquity = createDecimal("0.01");
    DecimalType baseTickFutures = createDecimal("0.25");

    SECTION("NoFractions Policy")
    {
        date d(2023, 1, 1);
        DecimalType resultEquity = NoFractions<DecimalType>::apply(d, equityAttrs, baseTickEquity);
        REQUIRE(resultEquity == baseTickEquity);

        DecimalType resultFutures = NoFractions<DecimalType>::apply(d, futuresAttrs, baseTickFutures);
        REQUIRE(resultFutures == baseTickFutures);
    }

    SECTION("NysePre2001Fractions Policy")
    {
        const DecimalType eighth = createDecimal("0.125");
        const DecimalType sixteenth = createDecimal("0.0625");

        // 1. Before 1997-06-01: Should be 1/8th
        date d_pre_1997(1997, 5, 31);
        DecimalType result_pre_1997 = NysePre2001Fractions<DecimalType>::apply(d_pre_1997, equityAttrs, baseTickEquity);
        REQUIRE(result_pre_1997 == eighth);

        // 2. Between 1997-06-01 and 2001-04-09: Should be 1/16th
        date d_mid_2000(2000, 1, 1);
        DecimalType result_mid_2000 = NysePre2001Fractions<DecimalType>::apply(d_mid_2000, equityAttrs, baseTickEquity);
        REQUIRE(result_mid_2000 == sixteenth);

        // 3. On or after 2001-04-09: Should be decimal (0.01)
        date d_post_2001(2001, 4, 9);
        DecimalType result_post_2001 = NysePre2001Fractions<DecimalType>::apply(d_post_2001, equityAttrs, baseTickEquity);
        REQUIRE(result_post_2001 == baseTickEquity);

        // 4. Should not affect non-equity securities
        DecimalType result_futures = NysePre2001Fractions<DecimalType>::apply(d_mid_2000, futuresAttrs, baseTickFutures);
        REQUIRE(result_futures == baseTickFutures);
    }

    SECTION("Rule612SubPenny Policy (Split-Adjusted)")
    {
        const DecimalType cent = createDecimal("0.01");
        const DecimalType price_under_1 = createDecimal("0.50");
        const DecimalType price_over_1 = createDecimal("1.50");

        // For prices >= $1, tick must be at least 0.01
        DecimalType result_over_1 = Rule612SubPenny<DecimalType, true>::apply(price_over_1, equityAttrs, baseTickEquity);
        REQUIRE(result_over_1 == cent);

        // For prices < $1 (split-adjusted), sub-pennies are DISABLED. Tick remains 0.01.
        DecimalType result_under_1 = Rule612SubPenny<DecimalType, true>::apply(price_under_1, equityAttrs, baseTickEquity);
        REQUIRE(result_under_1 == cent);

        // Should not affect non-equity securities
        DecimalType result_futures = Rule612SubPenny<DecimalType, true>::apply(price_over_1, futuresAttrs, baseTickFutures);
        REQUIRE(result_futures == baseTickFutures);
    }

    SECTION("Rule612SubPenny Policy (Not Split-Adjusted)")
    {
        const DecimalType cent = createDecimal("0.01");
        const DecimalType sub_penny = createDecimal("0.0001");
        const DecimalType price_under_1 = createDecimal("0.50");
        const DecimalType price_over_1 = createDecimal("1.50");

        // For prices >= $1, tick must be at least 0.01
        DecimalType result_over_1 = Rule612SubPenny<DecimalType, false>::apply(price_over_1, equityAttrs, baseTickEquity);
        REQUIRE(result_over_1 == cent);

        // For prices < $1 (not split-adjusted), sub-pennies are ENABLED. Tick becomes 0.0001.
        DecimalType result_under_1 = Rule612SubPenny<DecimalType, false>::apply(price_under_1, equityAttrs, baseTickEquity);
        REQUIRE(result_under_1 == sub_penny);

        // Should not affect non-equity securities
        DecimalType result_futures = Rule612SubPenny<DecimalType, false>::apply(price_over_1, futuresAttrs, baseTickFutures);
        REQUIRE(result_futures == baseTickFutures);
    }
}
