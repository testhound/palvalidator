#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"
#include "../ClosedPositionHistory.h"
#include "../StrategyBroker.h"
#include "../BoostDateHelper.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef dec::decimal<7> DecimalType;
typedef OHLCTimeSeriesEntry<7> EntryType;

std::string myCornSymbol("C2");

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


std::shared_ptr<EntryType>
    createTimeSeriesEntry2 (const TimeSeriesDate& aDate,
		       const dec::decimal<7>& openPrice,
		       const dec::decimal<7>& highPrice,
		       const dec::decimal<7>& lowPrice,
		       const dec::decimal<7>& closePrice,
		       volume_t vol)
  {
    auto date1 = std::make_shared<date> (aDate);
    auto open1 = std::make_shared<DecimalType> (openPrice);
    auto high1 = std::make_shared<DecimalType> (highPrice);
    auto low1 = std::make_shared<DecimalType> (lowPrice);
    auto close1 = std::make_shared<DecimalType> (closePrice);
    return std::make_shared<EntryType>(date1, open1, high1, low1, 
						close1, vol, TimeFrame::DAILY);
  }


void addBarHistoryUntilDate (std::shared_ptr<TradingPosition<7>> openPosition,
			    const TimeSeriesDate& entryDate,
			    const TimeSeriesDate& exitDate,
			    const std::shared_ptr<OHLCTimeSeries<7>>& aTimeSeries )
{
  OHLCTimeSeries<7>::TimeSeriesIterator it = aTimeSeries->getTimeSeriesEntry(entryDate);
  it++;

  OHLCTimeSeries<7>::TimeSeriesIterator itEnd = aTimeSeries->getTimeSeriesEntry(exitDate);
  for (; it != itEnd; it++)
    {
      openPosition->addBar (it->second);
    }

  // Add exit bar since loop will not do it

  openPosition->addBar (it->second);
}

std::shared_ptr<TradingPositionLong<7>>
createClosedLongPosition (const std::shared_ptr<OHLCTimeSeries<7>>& aTimeSeries,
			  const TimeSeriesDate& entryDate,
			  const dec::decimal<7>& entryPrice,
			  const TimeSeriesDate& exitDate,
			  const dec::decimal<7>& exitPrice,
			  const TradingVolume& tVolume,
			  int dummy)
{

  auto entry = createTimeSeriesEntry2 (entryDate,
				      entryPrice,
				      entryPrice,
				      entryPrice,
				      entryPrice,
				      tVolume.getTradingVolume());

  auto aPos = std::make_shared<TradingPositionLong<7>>(myCornSymbol,
						       entryPrice,
						       entry,
						       tVolume);

  addBarHistoryUntilDate (aPos, entryDate, exitDate, aTimeSeries); 
  aPos->ClosePosition (exitDate,
		       exitPrice);
  return aPos;
}


std::shared_ptr<TradingPositionShort<7>>
createClosedShortPosition (const std::shared_ptr<OHLCTimeSeries<7>>& aTimeSeries,
			   const TimeSeriesDate& entryDate,
			  const dec::decimal<7>& entryPrice,
			  const TimeSeriesDate& exitDate,
			  const dec::decimal<7>& exitPrice,
			  const TradingVolume& tVolume,
			  int dummy)
{
  
  auto entry = createTimeSeriesEntry2 (entryDate,
				      entryPrice,
				      entryPrice,
				      entryPrice,
				      entryPrice,
				      tVolume.getTradingVolume());

  auto aPos = std::make_shared<TradingPositionShort<7>>(myCornSymbol,
						       entryPrice,
						       entry,
						       tVolume);
  addBarHistoryUntilDate (aPos, entryDate, exitDate, aTimeSeries);

  aPos->ClosePosition (exitDate,
		       exitPrice);
  return aPos;
}





TEST_CASE ("StrategyBroker operations", "[StrategyBroker]")
{
  PALFormatCsvReader<7> csvFile ("C2_122AR.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS);
  csvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<7>> p = csvFile.getTimeSeries();

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  decimal<7> cornBigPointValue(createDecimal("50.0"));
  decimal<7> cornTickValue(createDecimal("0.25"));
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  auto corn = std::make_shared<FuturesSecurity<7>>(futuresSymbol, 
						   futuresName, 
						   cornBigPointValue,
						   cornTickValue, 
						   p);

  std::string portName("Corn Portfolio");
  auto aPortfolio = std::make_shared<Portfolio<7>>(portName);

  aPortfolio->addSecurity (corn);

  StrategyBroker<7> aBroker(aPortfolio);

  REQUIRE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());
  REQUIRE (aBroker.getTotalTrades() == 0);
  REQUIRE (aBroker.getOpenTrades() == 0);
  REQUIRE (aBroker.getClosedTrades() == 0);

  SECTION ("StrategyBroker test going long on open")
  {
    aBroker.EnterLongOnOpen (futuresSymbol,
			   TimeSeriesDate (1985, Nov, 14) ,
			   oneContract);
    REQUIRE_FALSE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());
    StrategyBroker<7>::PendingOrderIterator it = aBroker.beginPendingOrders();
    REQUIRE (it->second->isOrderPending());
    REQUIRE (it->second->isMarketOrder());
    REQUIRE (it->second->isLongOrder());

    aBroker.ProcessPendingOrders (TimeSeriesDate (1985, Nov, 15));
    REQUIRE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());

    REQUIRE (aBroker.getTotalTrades() == 1);
    REQUIRE (aBroker.getOpenTrades() == 1);
    REQUIRE (aBroker.getClosedTrades() == 0);
  }

  
  SECTION ("StrategyBroker test going short on open")
  {
    aBroker.EnterShortOnOpen (futuresSymbol,
			   TimeSeriesDate (1985, Nov, 14) ,
			   oneContract);
    REQUIRE_FALSE (aBroker.beginPendingOrders() == aBroker.endPendingOrders());
    StrategyBroker<7>::PendingOrderIterator it = aBroker.beginPendingOrders();
    REQUIRE (it->second->isOrderPending());
    REQUIRE (it->second->isMarketOrder());
    REQUIRE (it->second->isShortOrder());

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
    StrategyBroker<7>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
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

    StrategyBroker<7>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
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

    PercentNumber<7> stopPercent(PercentNumber<7>::createPercentNumber (createDecimal("1.28")));

    InstrumentPosition<7>::ConstInstrumentPositionIterator instrPosIterator =
      aBroker.getInstrumentPosition(futuresSymbol).getInstrumentPosition(1);

    InstrumentPosition<7> instrPosition(aBroker.getInstrumentPosition(futuresSymbol));

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

    StrategyBroker<7>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
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

    PercentNumber<7> stopPercent(PercentNumber<7>::createPercentNumber (createDecimal("1.28")));
    PercentNumber<7> profitPercent(PercentNumber<7>::createPercentNumber (createDecimal("1.34")));

    InstrumentPosition<7>::ConstInstrumentPositionIterator instrPosIterator =
      aBroker.getInstrumentPosition(futuresSymbol).getInstrumentPosition(1);

    InstrumentPosition<7> instrPosition(aBroker.getInstrumentPosition(futuresSymbol));

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

    StrategyBroker<7>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
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

    PercentNumber<7> stopPercent(PercentNumber<7>::createPercentNumber (createDecimal("1.28")));

    InstrumentPosition<7>::ConstInstrumentPositionIterator instrPosIterator =
      aBroker.getInstrumentPosition(futuresSymbol).getInstrumentPosition(1);

    InstrumentPosition<7> instrPosition(aBroker.getInstrumentPosition(futuresSymbol));

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

    StrategyBroker<7>::StrategyTransactionIterator it = aBroker.beginStrategyTransactions();
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

    PercentNumber<7> shortStopPercent(PercentNumber<7>::createPercentNumber (createDecimal("1.28")));
    PercentNumber<7> shortProfitPercent(PercentNumber<7>::createPercentNumber (createDecimal("1.34")));

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

    ClosedPositionHistory<7> positions (aBroker.getClosedPositionHistory());
    REQUIRE (positions.getNumPositions() == 2);
    REQUIRE (positions.getNumWinningPositions() == 2);
    REQUIRE (positions.getNumLosingPositions() == 0);
    REQUIRE (positions.getPercentWinners() == DecimalConstants<7>::DecimalOneHundred);
  }

}

