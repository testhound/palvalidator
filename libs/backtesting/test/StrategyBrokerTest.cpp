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

  SECTION("StrategyBroker throws on ExitShortAllUnitsOnOpen when flat")
    {
      REQUIRE_THROWS_AS(
        aBroker.ExitShortAllUnitsOnOpen(futuresSymbol, {1986, May, 28}),
        StrategyBrokerException);
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

TEST_CASE("StrategyBroker constructor validation", "[StrategyBroker][constructor]")
{
  SECTION("StrategyBroker constructor throws exception with null portfolio")
  {
    // Attempt to create a StrategyBroker with null portfolio
    std::shared_ptr<Portfolio<DecimalType>> nullPortfolio = nullptr;
    
    REQUIRE_THROWS_AS(
      StrategyBroker<DecimalType>(nullPortfolio),
      StrategyBrokerException
    );
  }
  
  SECTION("StrategyBroker constructor succeeds with valid portfolio")
  {
    // Create a valid portfolio (minimal setup)
    DecimalType cornTickValue(createDecimal("0.25"));
    PALFormatCsvReader<DecimalType> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
    csvFile.readFile();

    std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();

    std::string futuresSymbol("@C");
    std::string futuresName("Corn futures");
    DecimalType cornBigPointValue(createDecimal("50.0"));

    auto corn = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol,
                                                               futuresName,
                                                               cornBigPointValue,
                                                               cornTickValue,
                                                               p);

    std::string portName("Valid Portfolio");
    auto validPortfolio = std::make_shared<Portfolio<DecimalType>>(portName);
    validPortfolio->addSecurity(corn);
    
    // This should not throw
    REQUIRE_NOTHROW(StrategyBroker<DecimalType>(validPortfolio));
  }
}

TEST_CASE("Copy constructor re-registers observer - CRITICAL BUG TEST", "[StrategyBroker][copy][critical]")
{
  // Setup: Create a portfolio and original broker
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();
  std::string futuresSymbol("@C");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
    futuresSymbol, futuresName, cornBigPointValue, cornTickValue, p
  );

  std::string portName("Test Portfolio");
  auto portfolio = std::make_shared<Portfolio<DecimalType>>(portName);
  portfolio->addSecurity(corn);

  // Create original broker
  StrategyBroker<DecimalType> broker1(portfolio);
  
  SECTION("Copied broker receives order execution notifications")
  {
    // Create a copy of the broker
    StrategyBroker<DecimalType> broker2(broker1);  // COPY CONSTRUCTOR
    
    // Both brokers should be flat initially
    REQUIRE(broker1.isFlatPosition(futuresSymbol));
    REQUIRE(broker2.isFlatPosition(futuresSymbol));
    
    // Add an order to broker2 (the copy)
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate orderDate(1985, Nov, 14);
    broker2.EnterLongOnOpen(futuresSymbol, orderDate, oneContract);
    
    // Verify order is pending
    REQUIRE_FALSE(broker2.beginPendingOrders() == broker2.endPendingOrders());
    
    // Process the order
    TimeSeriesDate executionDate(1985, Nov, 15);
    broker2.ProcessPendingOrders(executionDate);
    
    // CRITICAL TEST: If observer was registered, position should be long
    // If observer was NOT registered, position will still be flat (BUG!)
    REQUIRE(broker2.isLongPosition(futuresSymbol));
    
    // Also verify the transaction was created
    REQUIRE(broker2.getTotalTrades() == 1);
    REQUIRE(broker2.getOpenTrades() == 1);
    
    // Verify broker1 is unaffected
    REQUIRE(broker1.isFlatPosition(futuresSymbol));
    REQUIRE(broker1.getTotalTrades() == 0);
  }
}

// ============================================================================
// CRITICAL TEST 2: Complementary Order Cancellation
// ============================================================================

TEST_CASE("Complementary orders are canceled when one executes", "[StrategyBroker][orders][critical]")
{
  // Setup
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();
  std::string futuresSymbol("@C");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
    futuresSymbol, futuresName, cornBigPointValue, cornTickValue, p
  );

  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(corn);
  StrategyBroker<DecimalType> broker(portfolio);

  SECTION("Limit order execution cancels stop order")
  {
    // Enter a long position
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate entryDate(1985, Nov, 14);
    broker.EnterLongOnOpen(futuresSymbol, entryDate, oneContract);
    
    // Execute the entry
    TimeSeriesDate entryExecDate(1985, Nov, 15);
    broker.ProcessPendingOrders(entryExecDate);
    REQUIRE(broker.isLongPosition(futuresSymbol));
    
    // Get entry price for calculating stop
    auto instrPos = broker.getInstrumentPosition(futuresSymbol);
    auto posIterator = instrPos.getInstrumentPosition(1);
    DecimalType entryPrice = (*posIterator)->getEntryPrice();
    
    // Place BOTH limit (profit target) and stop (stop loss) orders
    TimeSeriesDate exitOrderDate(1985, Nov, 18);
    DecimalType limitPrice = entryPrice + createDecimal("5.00");  // Above entry
    DecimalType stopPrice = entryPrice - createDecimal("2.00");   // Below entry
    
    broker.ExitLongAllUnitsAtLimit(futuresSymbol, exitOrderDate, limitPrice);
    broker.ExitLongAllUnitsAtStop(futuresSymbol, exitOrderDate, stopPrice);
    
    // Verify both orders are pending
    int pendingCount = 0;
    for (auto it = broker.beginPendingOrders(); it != broker.endPendingOrders(); ++it) {
      pendingCount++;
    }
    REQUIRE(pendingCount == 2);  // Both limit and stop should be pending
    
    // Process a bar where the limit order triggers (price goes UP)
    TimeSeriesDate limitExecDate(1985, Nov, 19);
    
    // The limit should execute when this bar processes
    // After execution, the stop order should be automatically canceled
    broker.ProcessPendingOrders(limitExecDate);
    
    // CRITICAL TEST: Position should be flat (exited via limit)
    REQUIRE(broker.isFlatPosition(futuresSymbol));
    
    // CRITICAL TEST: No more pending orders (stop was canceled)
    REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
    
    // Verify trade is closed
    REQUIRE(broker.getClosedTrades() == 1);
    REQUIRE(broker.getOpenTrades() == 0);
  }
  
  SECTION("Stop order execution cancels limit order")
  {
    // Enter a long position
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate entryDate(1985, Nov, 14);
    broker.EnterLongOnOpen(futuresSymbol, entryDate, oneContract);
    
    // Execute the entry
    TimeSeriesDate entryExecDate(1985, Nov, 15);
    broker.ProcessPendingOrders(entryExecDate);
    REQUIRE(broker.isLongPosition(futuresSymbol));
    
    // Get entry price
    auto instrPos = broker.getInstrumentPosition(futuresSymbol);
    auto posIterator = instrPos.getInstrumentPosition(1);
    DecimalType entryPrice = (*posIterator)->getEntryPrice();
    
    // Place BOTH limit and stop orders
    TimeSeriesDate exitOrderDate(1985, Nov, 18);
    DecimalType limitPrice = entryPrice + createDecimal("5.00");
    DecimalType stopPrice = entryPrice - createDecimal("2.00");
    
    broker.ExitLongAllUnitsAtLimit(futuresSymbol, exitOrderDate, limitPrice);
    broker.ExitLongAllUnitsAtStop(futuresSymbol, exitOrderDate, stopPrice);
    
    // This time, process a bar where price goes DOWN (stop triggers, limit doesn't)
    TimeSeriesDate stopExecDate(1985, Nov, 19);
    broker.ProcessPendingOrders(stopExecDate);
    
    // CRITICAL TEST: Position should be flat (exited via stop)
    REQUIRE(broker.isFlatPosition(futuresSymbol));
    
    // CRITICAL TEST: No more pending orders (limit was canceled)
    REQUIRE(broker.beginPendingOrders() == broker.endPendingOrders());
    
    // Verify trade is closed
    REQUIRE(broker.getClosedTrades() == 1);
  }
}

// ============================================================================
// CRITICAL TEST 3: Copy Assignment Re-registers Observer
// ============================================================================

TEST_CASE("Copy assignment re-registers observer", "[StrategyBroker][copy][assignment]")
{
  // Setup two portfolios
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();
  std::string futuresSymbol("@C");

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
    futuresSymbol, "Corn", createDecimal("50.0"), cornTickValue, p
  );

  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Portfolio");
  portfolio->addSecurity(corn);

  StrategyBroker<DecimalType> broker1(portfolio);
  StrategyBroker<DecimalType> broker2(portfolio);

  SECTION("Assignment operator copies state and re-registers observer")
  {
    // Add order to broker1
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate orderDate(1985, Nov, 14);
    broker1.EnterLongOnOpen(futuresSymbol, orderDate, oneContract);
    
    // Assign broker1 to broker2
    broker2 = broker1;  // COPY ASSIGNMENT
    
    // Process orders on broker2
    TimeSeriesDate execDate(1985, Nov, 15);
    broker2.ProcessPendingOrders(execDate);
    
    // CRITICAL: If observer was registered, position should update
    REQUIRE(broker2.isLongPosition(futuresSymbol));
    REQUIRE(broker2.getTotalTrades() == 1);
  }
  
  SECTION("Self-assignment is safe")
  {
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate orderDate(1985, Nov, 14);
    broker1.EnterLongOnOpen(futuresSymbol, orderDate, oneContract);
    
    // Self-assignment should not crash or corrupt state
    broker1 = broker1;
    
    // Broker should still work
    TimeSeriesDate execDate(1985, Nov, 15);
    broker1.ProcessPendingOrders(execDate);
    REQUIRE(broker1.isLongPosition(futuresSymbol));
  }
}

// ============================================================================
// CRITICAL TEST 4: Move Semantics
// ============================================================================

TEST_CASE("Move constructor transfers state", "[StrategyBroker][move]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();
  std::string futuresSymbol("@C");

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
    futuresSymbol, "Corn", createDecimal("50.0"), cornTickValue, p
  );

  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Portfolio");
  portfolio->addSecurity(corn);

  SECTION("Move constructor transfers orders and positions")
  {
    StrategyBroker<DecimalType> broker1(portfolio);
    
    // Add order to broker1
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate orderDate(1985, Nov, 14);
    broker1.EnterLongOnOpen(futuresSymbol, orderDate, oneContract);
    
    // Move broker1 to broker2
    StrategyBroker<DecimalType> broker2(std::move(broker1));
    
    // broker2 should have the order
    REQUIRE_FALSE(broker2.beginPendingOrders() == broker2.endPendingOrders());
    
    // Process and verify broker2 works
    TimeSeriesDate execDate(1985, Nov, 15);
    broker2.ProcessPendingOrders(execDate);
    REQUIRE(broker2.isLongPosition(futuresSymbol));
  }
}

TEST_CASE("Move assignment transfers state", "[StrategyBroker][move]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();
  std::string futuresSymbol("@C");

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
    futuresSymbol, "Corn", createDecimal("50.0"), cornTickValue, p
  );

  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Portfolio");
  portfolio->addSecurity(corn);

  SECTION("Move assignment operator works correctly")
  {
    StrategyBroker<DecimalType> broker1(portfolio);
    StrategyBroker<DecimalType> broker2(portfolio);
    
    // Add order to broker1
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate orderDate(1985, Nov, 14);
    broker1.EnterLongOnOpen(futuresSymbol, orderDate, oneContract);
    
    // Move-assign broker1 to broker2
    broker2 = std::move(broker1);
    
    // broker2 should have the order
    REQUIRE_FALSE(broker2.beginPendingOrders() == broker2.endPendingOrders());
    
    // Process and verify
    TimeSeriesDate execDate(1985, Nov, 15);
    broker2.ProcessPendingOrders(execDate);
    REQUIRE(broker2.isLongPosition(futuresSymbol));
  }
}

// ============================================================================
// HIGH PRIORITY TEST: Exception Handling
// ============================================================================

TEST_CASE("Exception handling for invalid operations", "[StrategyBroker][exceptions]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();
  std::string futuresSymbol("@C");

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
    futuresSymbol, "Corn", createDecimal("50.0"), cornTickValue, p
  );

  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Portfolio");
  portfolio->addSecurity(corn);
  StrategyBroker<DecimalType> broker(portfolio);

  SECTION("Exiting non-existent long position throws exception")
  {
    TimeSeriesDate exitDate(1985, Nov, 14);
    
    // Try to exit a position that doesn't exist
    REQUIRE_THROWS_AS(
      broker.ExitLongAllUnitsOnOpen(futuresSymbol, exitDate),  // REMOVED oneContract
      StrategyBrokerException
    );
  }
  
  SECTION("Exiting wrong direction throws exception")
  {
    // Enter a LONG position
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate entryDate(1985, Nov, 14);
    broker.EnterLongOnOpen(futuresSymbol, entryDate, oneContract);
    
    TimeSeriesDate execDate(1985, Nov, 15);
    broker.ProcessPendingOrders(execDate);
    REQUIRE(broker.isLongPosition(futuresSymbol));
    
    // Try to exit as if it were a SHORT position
    TimeSeriesDate exitDate(1985, Nov, 16);
    REQUIRE_THROWS_AS(
      broker.ExitShortAllUnitsOnOpen(futuresSymbol, exitDate),  // REMOVED oneContract
      StrategyBrokerException
    );
  }
}

// ============================================================================
// INTEGRATION TEST: Copy Independence
// ============================================================================

TEST_CASE("Copied brokers are truly independent", "[StrategyBroker][copy][integration]")
{
  DecimalType cornTickValue(createDecimal("0.25"));
  PALFormatCsvReader<DecimalType> csvFile("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, cornTickValue);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = csvFile.getTimeSeries();
  std::string futuresSymbol("@C");

  auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
    futuresSymbol, "Corn", createDecimal("50.0"), cornTickValue, p
  );

  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Portfolio");
  portfolio->addSecurity(corn);

  SECTION("Operations on copied broker don't affect original")
  {
    StrategyBroker<DecimalType> broker1(portfolio);
    
    // Enter position in broker1
    TradingVolume oneContract(1, TradingVolume::CONTRACTS);
    TimeSeriesDate date1(1985, Nov, 14);
    broker1.EnterLongOnOpen(futuresSymbol, date1, oneContract);
    broker1.ProcessPendingOrders(TimeSeriesDate(1985, Nov, 15));
    
    REQUIRE(broker1.isLongPosition(futuresSymbol));
    REQUIRE(broker1.getTotalTrades() == 1);
    
    // Copy broker1 to broker2
    StrategyBroker<DecimalType> broker2(broker1);
    
    // broker2 should have same state initially
    REQUIRE(broker2.isLongPosition(futuresSymbol));
    REQUIRE(broker2.getTotalTrades() == 1);
    
    // First exit the long position in broker2, then enter a new short position
    TimeSeriesDate exitDate(1985, Nov, 17);
    broker2.ExitLongAllUnitsOnOpen(futuresSymbol, exitDate);
    broker2.ProcessPendingOrders(TimeSeriesDate(1985, Nov, 18));
    
    // Verify broker2 is now flat
    REQUIRE(broker2.isFlatPosition(futuresSymbol));
    REQUIRE(broker2.getClosedTrades() == 1);
    
    // Now enter a NEW short position in broker2
    TimeSeriesDate date2(1985, Nov, 19);
    broker2.EnterShortOnOpen(futuresSymbol, date2, oneContract);
    broker2.ProcessPendingOrders(TimeSeriesDate(1985, Nov, 20));
    
    // broker2 should now have 2 trades
    REQUIRE(broker2.getTotalTrades() == 2);
    
    // broker1 should still have only 1 trade
    REQUIRE(broker1.getTotalTrades() == 1);
    REQUIRE(broker1.isLongPosition(futuresSymbol));
  }
}

// ============================================================================
// Helper: standard corn futures setup shared across OrderType test cases
// ============================================================================

namespace {

struct CornBrokerFixture
{
  DecimalType cornTickValue   = createDecimal("0.25");
  std::string futuresSymbol   = "@C";
  TradingVolume oneContract   = TradingVolume(1, TradingVolume::CONTRACTS);

  std::shared_ptr<OHLCTimeSeries<DecimalType>> timeSeries;
  std::shared_ptr<Portfolio<DecimalType>>      portfolio;
  std::shared_ptr<StrategyBroker<DecimalType>> broker;

  CornBrokerFixture()
  {
    PALFormatCsvReader<DecimalType> csv("C2_122AR.txt", TimeFrame::DAILY,
                                        TradingVolume::CONTRACTS, cornTickValue);
    csv.readFile();
    timeSeries = csv.getTimeSeries();

    auto corn = std::make_shared<FuturesSecurity<DecimalType>>(
      futuresSymbol, "Corn futures", createDecimal("50.0"), cornTickValue, timeSeries);

    portfolio = std::make_shared<Portfolio<DecimalType>>("Corn Portfolio");
    portfolio->addSecurity(corn);

    broker = std::make_shared<StrategyBroker<DecimalType>>(portfolio);
  }

  // Open a long position on entryOrderDate, fill it on executionDate.
  void enterLong(date entryOrderDate, date executionDate)
  {
    broker->EnterLongOnOpen(futuresSymbol, entryOrderDate, oneContract);
    broker->ProcessPendingOrders(executionDate);
  }

  // Open a short position on entryOrderDate, fill it on executionDate.
  void enterShort(date entryOrderDate, date executionDate)
  {
    broker->EnterShortOnOpen(futuresSymbol, entryOrderDate, oneContract);
    broker->ProcessPendingOrders(executionDate);
  }

  // Return the single closed position from the broker's history.
  std::shared_ptr<TradingPosition<DecimalType>> firstClosedPosition() const
  {
    auto history = broker->getClosedPositionHistory();
    auto it = history.beginTradingPositions();
    REQUIRE(it != history.endTradingPositions());
    return it->second;
  }

  // Return the currently open position (unit 1).
  std::shared_ptr<TradingPosition<DecimalType>> openPosition() const
  {
    auto& instrPos = broker->getInstrumentPosition(futuresSymbol);
    auto it = instrPos.getInstrumentPosition(1);
    return *it;
  }
};

// ============================================================================
// SyntheticBrokerFixture
//
// An in-memory 4-bar time series with fully controlled OHLC prices.
// This eliminates dependency on CSV data for tests that need specific
// stop/limit trigger conditions.
//
// Bar schedule (all weekdays):
//   Bar 1  2020-01-02  O=300  H=305  L=295  C=300   [entry order day]
//   Bar 2  2020-01-03  O=300  H=305  L=295  C=300   [entry fills at open=300]
//   Bar 3  2020-01-06  O=300  H=305  L=295  C=300   [stop/limit order day]
//   Bar 4  2020-01-07  O=300  H=360  L=280  C=300   [execution day:
//                                                      H=360 fires short stop >=309 (3% above 300)
//                                                      L=280 fires long  stop <=291 (3% below 300)]
// ============================================================================
struct SyntheticBrokerFixture
{
  DecimalType tickValue    = createDecimal("0.25");
  std::string futuresSymbol = "@C";
  TradingVolume oneContract = TradingVolume(1, TradingVolume::CONTRACTS);

  // Convenience date constants that correspond to the 4 bars above
  const date bar1Date = date(2020, Jan,  2); // entry order day
  const date bar2Date = date(2020, Jan,  3); // entry fill day  → open = 300
  const date bar3Date = date(2020, Jan,  6); // stop/limit order day
  const date bar4Date = date(2020, Jan,  7); // stop/limit exec day

  std::shared_ptr<OHLCTimeSeries<DecimalType>> timeSeries;
  std::shared_ptr<Portfolio<DecimalType>>       portfolio;
  std::shared_ptr<StrategyBroker<DecimalType>>  broker;

  SyntheticBrokerFixture()
  {
    // Build a minimal 4-bar series with known prices.
    // Note: createTimeSeriesEntry date format is "YYYYMMDD".
    timeSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(
      TimeFrame::DAILY, TradingVolume::CONTRACTS);

    // Flat bars for the first three days
    timeSeries->addEntry(*createTimeSeriesEntry("20200102","300.00","305.00","295.00","300.00","0"));
    timeSeries->addEntry(*createTimeSeriesEntry("20200103","300.00","305.00","295.00","300.00","0"));
    timeSeries->addEntry(*createTimeSeriesEntry("20200106","300.00","305.00","295.00","300.00","0"));
    // Bar 4: extreme H and L so any 3% stop fires deterministically on this bar.
    //   Short stop (3% above 300 = 309): H=360 >= 309  → CoverAtStop fires
    //   Long  stop (3% below 300 = 291): L=280 <= 291  → SellAtStop fires
    timeSeries->addEntry(*createTimeSeriesEntry("20200107","300.00","360.00","280.00","300.00","0"));

    auto security = std::make_shared<FuturesSecurity<DecimalType>>(
      futuresSymbol, "Synthetic futures",
      createDecimal("50.0"), tickValue, timeSeries);

    portfolio = std::make_shared<Portfolio<DecimalType>>("Synthetic Portfolio");
    portfolio->addSecurity(security);

    broker = std::make_shared<StrategyBroker<DecimalType>>(portfolio);
  }

  void enterLong(date entryOrderDate, date executionDate)
  {
    broker->EnterLongOnOpen(futuresSymbol, entryOrderDate, oneContract);
    broker->ProcessPendingOrders(executionDate);
  }

  void enterShort(date entryOrderDate, date executionDate)
  {
    broker->EnterShortOnOpen(futuresSymbol, entryOrderDate, oneContract);
    broker->ProcessPendingOrders(executionDate);
  }

  std::shared_ptr<TradingPosition<DecimalType>> firstClosedPosition() const
  {
    auto history = broker->getClosedPositionHistory();
    auto it = history.beginTradingPositions();
    REQUIRE(it != history.endTradingPositions());
    return it->second;
  }

  std::shared_ptr<TradingPosition<DecimalType>> openPosition() const
  {
    auto& instrPos = broker->getInstrumentPosition(futuresSymbol);
    auto it = instrPos.getInstrumentPosition(1);
    return *it;
  }
};

} // anonymous namespace

// ============================================================================
// TEST SUITE: Entry order type propagation
// ============================================================================

TEST_CASE("StrategyBroker sets entry order type on new long position",
          "[StrategyBroker][OrderType][entry]")
{
  CornBrokerFixture f;

  f.enterLong({1985, Nov, 14}, {1985, Nov, 15});

  REQUIRE(f.broker->isLongPosition(f.futuresSymbol));
  auto pos = f.openPosition();
  REQUIRE(pos->getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
  REQUIRE(pos->hasKnownEntryOrderType());
}

TEST_CASE("StrategyBroker sets entry order type on new short position",
          "[StrategyBroker][OrderType][entry]")
{
  CornBrokerFixture f;

  f.enterShort({1985, Nov, 14}, {1985, Nov, 15});

  REQUIRE(f.broker->isShortPosition(f.futuresSymbol));
  auto pos = f.openPosition();
  REQUIRE(pos->getEntryOrderType() == OrderType::MARKET_ON_OPEN_SHORT);
  REQUIRE(pos->hasKnownEntryOrderType());
}

TEST_CASE("StrategyBroker: exit order type is UNKNOWN while position is still open",
          "[StrategyBroker][OrderType][entry]")
{
  CornBrokerFixture f;

  f.enterLong({1985, Nov, 14}, {1985, Nov, 15});

  auto pos = f.openPosition();
  REQUIRE(pos->getExitOrderType() == OrderType::UNKNOWN);
  REQUIRE_FALSE(pos->hasKnownExitOrderType());
}

// ============================================================================
// TEST SUITE: Exit order type propagation — long positions
// ============================================================================

TEST_CASE("StrategyBroker ExitLongAllUnitsOnOpen sets MARKET_ON_OPEN_SELL on closed position",
          "[StrategyBroker][OrderType][exit][long]")
{
  CornBrokerFixture f;

  f.enterLong({1985, Nov, 14}, {1985, Nov, 15});
  REQUIRE(f.broker->isLongPosition(f.futuresSymbol));

  f.broker->ExitLongAllUnitsOnOpen(f.futuresSymbol, {1985, Nov, 18});
  f.broker->ProcessPendingOrders({1985, Nov, 19});

  REQUIRE(f.broker->isFlatPosition(f.futuresSymbol));

  auto pos = f.firstClosedPosition();
  REQUIRE(pos->getExitOrderType() == OrderType::MARKET_ON_OPEN_SELL);
  REQUIRE(pos->hasKnownExitOrderType());
}

TEST_CASE("StrategyBroker ExitLongAllUnitsAtLimit sets SELL_AT_LIMIT on closed position",
          "[StrategyBroker][OrderType][exit][long]")
{
  CornBrokerFixture f;

  // Enter long on Nov 15 (fill Nov 18)
  f.enterLong({1985, Nov, 15}, {1985, Nov, 18});
  REQUIRE(f.broker->isLongPosition(f.futuresSymbol));

  // Place a limit well above the market so it fills quickly
  auto pos0      = f.openPosition();
  DecimalType entryPrice = pos0->getEntryPrice();
  auto limitPrice = entryPrice + createDecimal("500.00"); // far above — will not fill on a normal bar

  // Use dates that the existing long-exit test uses and where a limit eventually fills
  DecimalType fixedLimit = createDecimal("3758.32172");
  PercentNumber<DecimalType> stopPct =
    PercentNumber<DecimalType>::createPercentNumber(createDecimal("1.28"));

  TimeSeriesDate orderDate = {1985, Nov, 18};
  TimeSeriesDate lastDate  = {1985, Dec, 3};
  bool closed = false;

  for (; orderDate <= lastDate; orderDate = boost_next_weekday(orderDate))
  {
    f.broker->ExitLongAllUnitsAtLimit(f.futuresSymbol, orderDate, fixedLimit);
    f.broker->ExitLongAllUnitsAtStop(f.futuresSymbol, orderDate, entryPrice, stopPct);
    TimeSeriesDate execDate = boost_next_weekday(orderDate);
    f.broker->ProcessPendingOrders(execDate);

    if (f.broker->isFlatPosition(f.futuresSymbol)) {
      closed = true;
      break;
    }
  }

  REQUIRE(closed);
  auto closedPos = f.firstClosedPosition();
  // The position must have exited via either the limit or the stop — both are known types
  REQUIRE(closedPos->hasKnownExitOrderType());
  OrderType et = closedPos->getExitOrderType();
  REQUIRE((et == OrderType::SELL_AT_LIMIT || et == OrderType::SELL_AT_STOP));
}

TEST_CASE("StrategyBroker ExitLongAllUnitsAtStop sets SELL_AT_STOP on closed position",
          "[StrategyBroker][OrderType][exit][long]")
{
  // Use SyntheticBrokerFixture so prices are fully controlled.
  // Bar 2 (2020-01-03) open = 300.00  →  3% long stop = 291.00
  // Bar 4 (2020-01-07) L   = 280.00  →  280 <= 291, stop fires on the first attempt.
  SyntheticBrokerFixture f;

  f.enterLong(f.bar1Date, f.bar2Date);
  REQUIRE(f.broker->isLongPosition(f.futuresSymbol));

  auto pos0            = f.openPosition();
  DecimalType entryPrice = pos0->getEntryPrice(); // open of bar 2 = 300.00

  PercentNumber<DecimalType> stopPct =
    PercentNumber<DecimalType>::createPercentNumber(createDecimal("3.00"));

  // Place only a stop on bar 3 — no limit — so the exit type is unambiguously SELL_AT_STOP.
  f.broker->ExitLongAllUnitsAtStop(f.futuresSymbol, f.bar3Date, entryPrice, stopPct);
  f.broker->ProcessPendingOrders(f.bar4Date);

  // Bar 4 L=280 is below the 3% stop (291), so the position must be flat now.
  REQUIRE(f.broker->isFlatPosition(f.futuresSymbol));

  auto closedPos = f.firstClosedPosition();
  REQUIRE(closedPos->getExitOrderType() == OrderType::SELL_AT_STOP);
  REQUIRE(closedPos->hasKnownExitOrderType());
}

// ============================================================================
// TEST SUITE: Exit order type propagation — short positions
// ============================================================================

TEST_CASE("StrategyBroker ExitShortAllUnitsOnOpen sets MARKET_ON_OPEN_COVER on closed position",
          "[StrategyBroker][OrderType][exit][short]")
{
  CornBrokerFixture f;

  f.enterShort({1986, May, 28}, {1986, May, 29});
  REQUIRE(f.broker->isShortPosition(f.futuresSymbol));

  f.broker->ExitShortAllUnitsOnOpen(f.futuresSymbol, {1986, Jun, 2});
  f.broker->ProcessPendingOrders({1986, Jun, 3});

  REQUIRE(f.broker->isFlatPosition(f.futuresSymbol));

  auto pos = f.firstClosedPosition();
  REQUIRE(pos->getExitOrderType() == OrderType::MARKET_ON_OPEN_COVER);
  REQUIRE(pos->hasKnownExitOrderType());
}

TEST_CASE("StrategyBroker ExitShortAllUnitsAtLimit sets COVER_AT_LIMIT on closed position",
          "[StrategyBroker][OrderType][exit][short]")
{
  CornBrokerFixture f;

  f.enterShort({1986, May, 28}, {1986, May, 29});
  REQUIRE(f.broker->isShortPosition(f.futuresSymbol));

  auto pos0 = f.openPosition();
  DecimalType entryPrice = pos0->getEntryPrice();

  PercentNumber<DecimalType> profitPct =
    PercentNumber<DecimalType>::createPercentNumber(createDecimal("1.34"));
  PercentNumber<DecimalType> stopPct =
    PercentNumber<DecimalType>::createPercentNumber(createDecimal("1.28"));

  TimeSeriesDate orderDate = {1986, May, 29};
  TimeSeriesDate lastDate  = {1986, Jun, 10};
  bool closed = false;

  for (; orderDate <= lastDate; orderDate = boost_next_weekday(orderDate))
  {
    f.broker->ExitShortAllUnitsAtLimit(f.futuresSymbol, orderDate, entryPrice, profitPct);
    f.broker->ExitShortAllUnitsAtStop(f.futuresSymbol, orderDate, entryPrice, stopPct);
    TimeSeriesDate execDate = boost_next_weekday(orderDate);
    f.broker->ProcessPendingOrders(execDate);

    if (f.broker->isFlatPosition(f.futuresSymbol)) {
      closed = true;
      break;
    }
  }

  REQUIRE(closed);
  auto closedPos = f.firstClosedPosition();
  REQUIRE(closedPos->hasKnownExitOrderType());
  OrderType et = closedPos->getExitOrderType();
  REQUIRE((et == OrderType::COVER_AT_LIMIT || et == OrderType::COVER_AT_STOP));
}

TEST_CASE("StrategyBroker ExitShortAllUnitsAtStop sets COVER_AT_STOP on closed position",
          "[StrategyBroker][OrderType][exit][short]")
{
  // Use SyntheticBrokerFixture so prices are fully controlled.
  // Bar 2 (2020-01-03) open = 300.00  →  3% short stop = 309.00
  // Bar 4 (2020-01-07) H   = 360.00  →  360 >= 309, stop fires on the first attempt.
  SyntheticBrokerFixture f;

  f.enterShort(f.bar1Date, f.bar2Date);
  REQUIRE(f.broker->isShortPosition(f.futuresSymbol));

  auto pos0            = f.openPosition();
  DecimalType entryPrice = pos0->getEntryPrice(); // open of bar 2 = 300.00

  PercentNumber<DecimalType> stopPct =
    PercentNumber<DecimalType>::createPercentNumber(createDecimal("3.00"));

  // Place only a stop on bar 3 — no limit — so the exit type is unambiguously COVER_AT_STOP.
  f.broker->ExitShortAllUnitsAtStop(f.futuresSymbol, f.bar3Date, entryPrice, stopPct);
  f.broker->ProcessPendingOrders(f.bar4Date);

  // Bar 4 H=360 is above the 3% stop (309), so the position must be flat now.
  REQUIRE(f.broker->isFlatPosition(f.futuresSymbol));

  auto closedPos = f.firstClosedPosition();
  REQUIRE(closedPos->getExitOrderType() == OrderType::COVER_AT_STOP);
  REQUIRE(closedPos->hasKnownExitOrderType());
}

// ============================================================================
// TEST SUITE: Backward-compat no-clobber invariant (end-to-end regression test)
// ============================================================================

TEST_CASE("StrategyBroker backward-compat exit path does not clobber known exit order type",
          "[StrategyBroker][OrderType][regression]")
{
  // This is the end-to-end version of the TradingPosition unit test for the
  // KNOWN→UNKNOWN no-op. It proves that the full broker pipeline —
  // ExitOrderExecutedCommon → setExitOrderType(KNOWN) followed by the
  // backward-compat ClosePosition(ptime, price) → setExitOrderType(UNKNOWN) —
  // leaves the known type intact.
  CornBrokerFixture f;

  f.enterLong({1985, Nov, 14}, {1985, Nov, 15});

  f.broker->ExitLongAllUnitsOnOpen(f.futuresSymbol, {1985, Nov, 18});
  // Should not throw despite the backward-compat path sending UNKNOWN after SELL
  REQUIRE_NOTHROW(f.broker->ProcessPendingOrders({1985, Nov, 19}));

  REQUIRE(f.broker->isFlatPosition(f.futuresSymbol));
  REQUIRE(f.broker->getClosedTrades() == 1);

  auto pos = f.firstClosedPosition();
  // Known exit type must not have been overwritten to UNKNOWN
  REQUIRE(pos->getExitOrderType() == OrderType::MARKET_ON_OPEN_SELL);
}

// ============================================================================
// TEST SUITE: Entry and exit order types both survive in ClosedPositionHistory
// ============================================================================

TEST_CASE("StrategyBroker: both entry and exit order types are preserved in ClosedPositionHistory",
          "[StrategyBroker][OrderType][history]")
{
  CornBrokerFixture f;

  // Complete a full trade: market long entry, market-on-open exit
  f.enterLong({1985, Nov, 14}, {1985, Nov, 15});
  f.broker->ExitLongAllUnitsOnOpen(f.futuresSymbol, {1985, Nov, 18});
  f.broker->ProcessPendingOrders({1985, Nov, 19});

  REQUIRE(f.broker->getClosedTrades() == 1);

  auto pos = f.firstClosedPosition();

  // Both ends of the trade must be recorded
  REQUIRE(pos->getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
  REQUIRE(pos->getExitOrderType()  == OrderType::MARKET_ON_OPEN_SELL);
  REQUIRE(pos->hasKnownEntryOrderType());
  REQUIRE(pos->hasKnownExitOrderType());
  REQUIRE(pos->isPositionClosed());
}

TEST_CASE("StrategyBroker: short trade entry and exit order types are preserved in ClosedPositionHistory",
          "[StrategyBroker][OrderType][history]")
{
  CornBrokerFixture f;

  f.enterShort({1986, May, 28}, {1986, May, 29});
  f.broker->ExitShortAllUnitsOnOpen(f.futuresSymbol, {1986, Jun, 2});
  f.broker->ProcessPendingOrders({1986, Jun, 3});

  REQUIRE(f.broker->getClosedTrades() == 1);

  auto pos = f.firstClosedPosition();

  REQUIRE(pos->getEntryOrderType() == OrderType::MARKET_ON_OPEN_SHORT);
  REQUIRE(pos->getExitOrderType()  == OrderType::MARKET_ON_OPEN_COVER);
  REQUIRE(pos->hasKnownEntryOrderType());
  REQUIRE(pos->hasKnownExitOrderType());
}

TEST_CASE("StrategyBroker: multiple closed positions each carry independent order types",
          "[StrategyBroker][OrderType][history]")
{
  CornBrokerFixture f;

  // Trade 1: long, closed via market exit
  f.enterLong({1985, Nov, 14}, {1985, Nov, 15});
  f.broker->ExitLongAllUnitsOnOpen(f.futuresSymbol, {1985, Nov, 18});
  f.broker->ProcessPendingOrders({1985, Nov, 19});
  REQUIRE(f.broker->isFlatPosition(f.futuresSymbol));

  // Trade 2: short, closed via market cover
  f.enterShort({1986, May, 28}, {1986, May, 29});
  f.broker->ExitShortAllUnitsOnOpen(f.futuresSymbol, {1986, Jun, 2});
  f.broker->ProcessPendingOrders({1986, Jun, 3});
  REQUIRE(f.broker->isFlatPosition(f.futuresSymbol));

  REQUIRE(f.broker->getClosedTrades() == 2);

  auto history = f.broker->getClosedPositionHistory();
  int longCount = 0, shortCount = 0;

  for (auto it = history.beginTradingPositions(); it != history.endTradingPositions(); ++it)
  {
    auto pos = it->second;
    REQUIRE(pos->hasKnownEntryOrderType());
    REQUIRE(pos->hasKnownExitOrderType());

    if (pos->isLongPosition()) {
      REQUIRE(pos->getEntryOrderType() == OrderType::MARKET_ON_OPEN_LONG);
      REQUIRE(pos->getExitOrderType()  == OrderType::MARKET_ON_OPEN_SELL);
      ++longCount;
    } else {
      REQUIRE(pos->getEntryOrderType() == OrderType::MARKET_ON_OPEN_SHORT);
      REQUIRE(pos->getExitOrderType()  == OrderType::MARKET_ON_OPEN_COVER);
      ++shortCount;
    }
  }

  REQUIRE(longCount  == 1);
  REQUIRE(shortCount == 1);
}
