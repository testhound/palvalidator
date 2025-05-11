#include <catch2/catch_test_macros.hpp>
#include "TimeSeriesCsvReader.h"
#include "ClosedPositionHistory.h"
#include "StrategyBroker.h"
#include "BoostDateHelper.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

std::string myCornSymbol("@C");

template< class T, class U > 
std::shared_ptr<T> dynamic_pointer_cast( const std::shared_ptr<U>& r ) noexcept
{
    if (auto p = dynamic_cast<typename std::shared_ptr<T>::element_type*>(r.get())) {
        return std::shared_ptr<T>(r, p);
    } else {
        return std::shared_ptr<T>();
    }
}

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

}

