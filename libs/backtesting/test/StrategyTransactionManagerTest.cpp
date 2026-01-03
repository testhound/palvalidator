#include <catch2/catch_test_macros.hpp>
#include "TradingOrder.h"
#include "TradingPosition.h"
#include "StrategyTransactionManager.h"
#include "InstrumentPosition.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TradingVolume
createShareVolume (volume_t vol)
{
  return TradingVolume (vol, TradingVolume::SHARES);
}


TradingVolume
createContractVolume (volume_t vol)
{
  return TradingVolume (vol, TradingVolume::CONTRACTS);
}

TEST_CASE ("StrategyTransactionManager Operations", "[StrategyTransactionManager]")
{
  auto entry18 = createTimeSeriesEntry ("20160119", "189.96", "190.11","186.20","188.06",
				    190196000);
  auto entry17 = createTimeSeriesEntry ("20160115", "186.77","188.76", "185.52","187.81",
				    324846400);
  auto entry16 = createTimeSeriesEntry ("20160114", "189.55","193.26", "187.66", "191.93",
				   240795600);
  auto entry15 = createTimeSeriesEntry ("20160113", "194.45", "194.86", "188.38","188.83",
				   221168900);
  auto entry14 = createTimeSeriesEntry ("20160112", "193.82", "194.55", "191.14","193.66",
				   172330500);
  auto entry13 = createTimeSeriesEntry ("20160111", "193.01", "193.41", "189.82","192.11",
				   187941300);
  auto entry12 = createTimeSeriesEntry ("20160108", "195.19", "195.85", "191.58","191.92",
				   142662900);
  auto entry11 = createTimeSeriesEntry ("20160107", "195.33", "197.44", "193.59","194.05",
				   142662900);
  auto entry10 = createTimeSeriesEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry9 = createTimeSeriesEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry8 = createTimeSeriesEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry7 = createTimeSeriesEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry6 = createTimeSeriesEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createTimeSeriesEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);
  auto entry4 = createTimeSeriesEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
  auto entry3 = createTimeSeriesEntry ("20151224", "205.72", "206.33", "205.42", "205.68",
				   48542200);
  auto entry2 = createTimeSeriesEntry ("20151223", "204.69", "206.07", "204.58", "206.02",
				   48542200);
  auto entry1 = createTimeSeriesEntry ("20151222", "202.72", "203.85", "201.55", "203.50",
				   111026200);
  auto entry0 = createTimeSeriesEntry ("20151221", "201.41", "201.88", "200.09", "201.67",
				   99094300);

  std::string equitySymbol("SPY");
  TradingVolume oneShare (1, TradingVolume::SHARES);
  auto longSpyEntryOrder1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(equitySymbol,
								       createShareVolume(1),
								       createDate("20151218"));
  longSpyEntryOrder1->MarkOrderExecuted (entry0->getDateValue(),
					 entry0->getOpenValue());
  auto longSpyPosition1 = std::make_shared<TradingPositionLong<DecimalType>>(equitySymbol,
								   entry0->getOpenValue(),
								   *entry0,
								   oneShare);
  InstrumentPosition<DecimalType> instrumentPositionSpy (equitySymbol);
  instrumentPositionSpy.addPosition(longSpyPosition1);
  StrategyTransactionManager<DecimalType> transactionManager;

  REQUIRE (transactionManager.getTotalTrades() == 0);
  REQUIRE (transactionManager.getOpenTrades() == 0);
  REQUIRE (transactionManager.getClosedTrades() == 0);

  auto strategyTrans = std::make_shared<StrategyTransaction<DecimalType>>(longSpyEntryOrder1,
								longSpyPosition1);
  transactionManager.addStrategyTransaction (strategyTrans);

  REQUIRE (transactionManager.getTotalTrades() == 1);
  REQUIRE (transactionManager.getOpenTrades() == 1);
  REQUIRE (transactionManager.getClosedTrades() == 0);

  instrumentPositionSpy.addBar(*entry1);
  instrumentPositionSpy.addBar(*entry2);
  instrumentPositionSpy.addBar(*entry3);
  instrumentPositionSpy.addBar(*entry4);

  REQUIRE (longSpyPosition1->getNumBarsInPosition() == 5);
  REQUIRE (longSpyEntryOrder1->isOrderExecuted());
  REQUIRE (longSpyEntryOrder1->isLongOrder());
  REQUIRE (longSpyPosition1->isPositionOpen());
  REQUIRE (longSpyPosition1->isLongPosition());

  REQUIRE (strategyTrans->isTransactionOpen());
  REQUIRE_FALSE (strategyTrans->isTransactionComplete());

  REQUIRE (strategyTrans->getEntryTradingOrder()->getFillPrice() == entry0->getOpenValue());
  REQUIRE (strategyTrans->getTradingPosition()->getEntryPrice() == entry0->getOpenValue());
  REQUIRE (strategyTrans->getTradingPosition()->getNumBarsInPosition() == 5);

  auto longSpyExitOrder1 = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(equitySymbol,
								       createShareVolume(1),
								       entry4->getDateValue());
  longSpyExitOrder1->MarkOrderExecuted (entry5->getDateValue(),
					 entry5->getOpenValue());
  instrumentPositionSpy.closeAllPositions (longSpyExitOrder1->getFillDate(),
					   longSpyExitOrder1->getFillPrice());

  REQUIRE (transactionManager.getTotalTrades() == 1);
  REQUIRE (transactionManager.getOpenTrades() == 1);
  REQUIRE (transactionManager.getClosedTrades() == 0);

  strategyTrans->completeTransaction (longSpyExitOrder1);

  REQUIRE (transactionManager.getTotalTrades() == 1);
  REQUIRE (transactionManager.getOpenTrades() == 0);
  REQUIRE (transactionManager.getClosedTrades() == 1);

  REQUIRE (strategyTrans->getTradingPosition()->isPositionClosed());
  REQUIRE (strategyTrans->getExitTradingOrder()->getFillPrice() == entry5->getOpenValue());
  REQUIRE (strategyTrans->getExitTradingOrder()->getFillDate() == entry5->getDateValue());

  auto longSpyEntryOrder2 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(equitySymbol,
								       createShareVolume(1),
								       entry6->getDateValue());
  longSpyEntryOrder2->MarkOrderExecuted (entry7->getDateValue(),
					 entry7->getOpenValue());
  auto longSpyPosition2 = std::make_shared<TradingPositionLong<DecimalType>>(equitySymbol,
								   entry7->getOpenValue(),
								   *entry7,
								   oneShare);

  instrumentPositionSpy.addPosition(longSpyPosition2);



  auto strategyTrans2 = std::make_shared<StrategyTransaction<DecimalType>>(longSpyEntryOrder2,
								longSpyPosition2);
  transactionManager.addStrategyTransaction (strategyTrans2);

  REQUIRE (transactionManager.getTotalTrades() == 2);
  REQUIRE (transactionManager.getOpenTrades() == 1);
  REQUIRE (transactionManager.getClosedTrades() == 1);

  instrumentPositionSpy.addBar(*entry8);
  instrumentPositionSpy.addBar(*entry9);


  REQUIRE (longSpyPosition2->getNumBarsInPosition() == 3);
  REQUIRE (longSpyEntryOrder2->isOrderExecuted());
  REQUIRE (longSpyEntryOrder2->isLongOrder());
  REQUIRE (longSpyPosition2->isPositionOpen());
  REQUIRE (longSpyPosition2->isLongPosition());

  REQUIRE (strategyTrans2->isTransactionOpen());
  REQUIRE_FALSE (strategyTrans2->isTransactionComplete());

  auto strategyTransactionIterator = transactionManager.findStrategyTransaction (longSpyPosition2->getPositionID());
  REQUIRE (strategyTransactionIterator != transactionManager.endStrategyTransaction());

  auto aStrategyTrans = strategyTransactionIterator->second;

  REQUIRE (aStrategyTrans->getEntryTradingOrder()->getFillPrice() == entry7->getOpenValue());
  REQUIRE (aStrategyTrans->getTradingPosition()->getEntryPrice() == entry7->getOpenValue());
  REQUIRE (aStrategyTrans->getTradingPosition()->getNumBarsInPosition() == 3);

   auto longSpyExitOrder2 = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(equitySymbol,
								       createShareVolume(1),
								       entry9->getDateValue());
  longSpyExitOrder2->MarkOrderExecuted (entry10->getDateValue(),
					 entry10->getOpenValue());
  instrumentPositionSpy.closeAllPositions (longSpyExitOrder2->getFillDate(),
					   longSpyExitOrder2->getFillPrice());

  strategyTrans2->completeTransaction (longSpyExitOrder2);

  REQUIRE (transactionManager.getTotalTrades() == 2);
  REQUIRE (transactionManager.getOpenTrades() == 0);
  REQUIRE (transactionManager.getClosedTrades() == 2);

  strategyTransactionIterator = transactionManager.findStrategyTransaction (longSpyPosition2->getPositionID());
  REQUIRE (strategyTransactionIterator != transactionManager.endStrategyTransaction());

  aStrategyTrans = strategyTransactionIterator->second;

  REQUIRE (aStrategyTrans->getTradingPosition()->getExitPrice() == entry10->getOpenValue());
  REQUIRE (aStrategyTrans->getTradingPosition()->getExitDate() == entry10->getDateValue());
}

TEST_CASE("StrategyTransactionManager - Initial State", "[StrategyTransactionManager]")
{
  StrategyTransactionManager<DecimalType> manager;
  
  SECTION("Empty manager has correct initial state")
  {
    REQUIRE(manager.getTotalTrades() == 0);
    REQUIRE(manager.getOpenTrades() == 0);
    REQUIRE(manager.getClosedTrades() == 0);
    REQUIRE(manager.getTransactionCount() == 0);
    REQUIRE(manager.size() == 0);
    REQUIRE(manager.empty());
    REQUIRE(manager.beginStrategyTransaction() == manager.endStrategyTransaction());
    REQUIRE(manager.beginSortedStrategyTransaction() == manager.endSortedStrategyTransaction());
  }
}

TEST_CASE("StrategyTransactionManager - Exception Handling", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager;

  SECTION("Adding null transaction throws exception")
  {
    REQUIRE_THROWS_AS(
      manager.addStrategyTransaction(nullptr),
      StrategyTransactionManagerException
    );
  }

  SECTION("Adding duplicate position ID throws exception")
  {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      equitySymbol, createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
    
    auto position = std::make_shared<TradingPositionLong<DecimalType>>(
      equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
    
    auto transaction = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder, position);
    
    manager.addStrategyTransaction(transaction);
    
    // Try to add the same transaction again
    REQUIRE_THROWS_AS(
      manager.addStrategyTransaction(transaction),
      StrategyTransactionManagerException
    );
  }
}

TEST_CASE("StrategyTransactionManager - Copy Constructor", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  auto entry1 = createTimeSeriesEntry("20151222", "202.72", "203.85", "201.55", "203.50", 111026200);

  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);

  StrategyTransactionManager<DecimalType> manager1;

  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());

  auto position = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);

  auto transaction = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder, position);
  manager1.addStrategyTransaction(transaction);

  SECTION("Copy constructor creates independent copy")
  {
    StrategyTransactionManager<DecimalType> manager2(manager1);

    REQUIRE(manager2.getTotalTrades() == 1);
    REQUIRE(manager2.getOpenTrades() == 1);
    REQUIRE(manager2.getClosedTrades() == 0);
    REQUIRE(manager2.hasTransaction(position->getPositionID()));

    // Complete transaction in manager1
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      equitySymbol, createShareVolume(1), entry1->getDateValue());
    exitOrder->MarkOrderExecuted(entry1->getDateValue(), entry1->getOpenValue());
    transaction->completeTransaction(exitOrder);

    // Deep-copy semantics: manager1 should see completion; manager2 should not.
    REQUIRE(manager1.getClosedTrades() == 1);
    REQUIRE(manager1.getOpenTrades() == 0);

    REQUIRE(manager2.getClosedTrades() == 0);
    REQUIRE(manager2.getOpenTrades() == 1);

    // Now complete the corresponding transaction inside manager2 and verify it updates independently.
    auto it2 = manager2.findStrategyTransaction(position->getPositionID());
    REQUIRE(it2 != manager2.endStrategyTransaction());

    auto transaction2 = it2->second;
    REQUIRE(transaction2);

    // Prove the copy is independent (not the same transaction object)
    REQUIRE(transaction2.get() != transaction.get());

    auto exitOrder2 = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      equitySymbol, createShareVolume(1), entry1->getDateValue());
    exitOrder2->MarkOrderExecuted(entry1->getDateValue(), entry1->getOpenValue());
    transaction2->completeTransaction(exitOrder2);

    REQUIRE(manager2.getClosedTrades() == 1);
    REQUIRE(manager2.getOpenTrades() == 0);
  }
}

TEST_CASE("StrategyTransactionManager - Move Constructor", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager1;
  
  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  
  auto position = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
  
  auto transaction = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder, position);
  manager1.addStrategyTransaction(transaction);
  
  uint32_t posID = position->getPositionID();
  
  SECTION("Move constructor transfers ownership")
  {
    StrategyTransactionManager<DecimalType> manager2(std::move(manager1));
    
    // Verify destination has correct state
    REQUIRE(manager2.getTotalTrades() == 1);
    REQUIRE(manager2.getOpenTrades() == 1);
    REQUIRE(manager2.getClosedTrades() == 0);
    REQUIRE(manager2.hasTransaction(posID));
    
    // Source object is in valid but unspecified state
    // We can only safely test that it's still usable (destructible/assignable)
    // Don't test specific values like getTotalTrades() as they're unspecified
  }
}

TEST_CASE("StrategyTransactionManager - Copy Assignment", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager1;
  StrategyTransactionManager<DecimalType> manager2;
  
  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  
  auto position = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
  
  auto transaction = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder, position);
  manager1.addStrategyTransaction(transaction);
  
  SECTION("Copy assignment works correctly")
  {
    manager2 = manager1;
    
    REQUIRE(manager2.getTotalTrades() == 1);
    REQUIRE(manager2.getOpenTrades() == 1);
    REQUIRE(manager2.getClosedTrades() == 0);
    REQUIRE(manager2.hasTransaction(position->getPositionID()));
  }
  
  SECTION("Self-assignment is safe")
  {
    manager1 = manager1;
    
    REQUIRE(manager1.getTotalTrades() == 1);
    REQUIRE(manager1.getOpenTrades() == 1);
    REQUIRE(manager1.getClosedTrades() == 0);
  }
}

TEST_CASE("StrategyTransactionManager - Move Assignment", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager1;
  StrategyTransactionManager<DecimalType> manager2;
  
  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  
  auto position = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
  
  auto transaction = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder, position);
  manager1.addStrategyTransaction(transaction);
  
  uint32_t posID = position->getPositionID();
  
  SECTION("Move assignment transfers ownership")
  {
    manager2 = std::move(manager1);
    
    // Verify destination has correct state
    REQUIRE(manager2.getTotalTrades() == 1);
    REQUIRE(manager2.getOpenTrades() == 1);
    REQUIRE(manager2.getClosedTrades() == 0);
    REQUIRE(manager2.hasTransaction(posID));
    
    // Source object is in valid but unspecified state
    // We can only safely test that it's still usable (destructible/assignable)
    // Don't test specific values as they're unspecified per C++ standard
  }
}

TEST_CASE("StrategyTransactionManager - Clear Function", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  auto entry1 = createTimeSeriesEntry("20151222", "202.72", "203.85", "201.55", "203.50", 111026200);
  
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager;
  
  auto entryOrder1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder1->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  
  auto position1 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
  
  auto transaction1 = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder1, position1);
  manager.addStrategyTransaction(transaction1);
  
  auto entryOrder2 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151219"));
  entryOrder2->MarkOrderExecuted(entry1->getDateValue(), entry1->getOpenValue());
  
  auto position2 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry1->getOpenValue(), *entry1, oneShare);
  
  auto transaction2 = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder2, position2);
  manager.addStrategyTransaction(transaction2);
  
  REQUIRE(manager.getTotalTrades() == 2);
  REQUIRE(manager.getOpenTrades() == 2);
  
  SECTION("Clear removes all transactions")
  {
    manager.clear();
    
    REQUIRE(manager.getTotalTrades() == 0);
    REQUIRE(manager.getOpenTrades() == 0);
    REQUIRE(manager.getClosedTrades() == 0);
    REQUIRE(manager.empty());
    REQUIRE(manager.beginStrategyTransaction() == manager.endStrategyTransaction());
    REQUIRE(manager.beginSortedStrategyTransaction() == manager.endSortedStrategyTransaction());
  }
}

TEST_CASE("StrategyTransactionManager - hasTransaction", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager;
  
  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  
  auto position = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
  
  auto transaction = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder, position);
  
  uint32_t posID = position->getPositionID();
  
  SECTION("hasTransaction returns false for non-existent transaction")
  {
    REQUIRE_FALSE(manager.hasTransaction(posID));
    REQUIRE_FALSE(manager.hasTransaction(99999));
  }
  
  SECTION("hasTransaction returns true for existing transaction")
  {
    manager.addStrategyTransaction(transaction);
    REQUIRE(manager.hasTransaction(posID));
    REQUIRE_FALSE(manager.hasTransaction(99999));
  }
}

TEST_CASE("StrategyTransactionManager - Iterator Coverage", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  auto entry1 = createTimeSeriesEntry("20151222", "202.72", "203.85", "201.55", "203.50", 111026200);
  auto entry2 = createTimeSeriesEntry("20151223", "204.69", "206.07", "204.58", "206.02", 48542200);
  
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager;
  
  // Add three transactions
  auto entryOrder1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder1->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  auto position1 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
  auto transaction1 = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder1, position1);
  manager.addStrategyTransaction(transaction1);
  
  auto entryOrder2 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151219"));
  entryOrder2->MarkOrderExecuted(entry1->getDateValue(), entry1->getOpenValue());
  auto position2 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry1->getOpenValue(), *entry1, oneShare);
  auto transaction2 = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder2, position2);
  manager.addStrategyTransaction(transaction2);
  
  auto entryOrder3 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151220"));
  entryOrder3->MarkOrderExecuted(entry2->getDateValue(), entry2->getOpenValue());
  auto position3 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry2->getOpenValue(), *entry2, oneShare);
  auto transaction3 = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder3, position3);
  manager.addStrategyTransaction(transaction3);
  
  SECTION("Can iterate through all transactions by position ID")
  {
    int count = 0;
    for (auto it = manager.beginStrategyTransaction(); 
         it != manager.endStrategyTransaction(); ++it)
    {
      REQUIRE(it->second != nullptr);
      count++;
    }
    REQUIRE(count == 3);
  }
  
  SECTION("Can iterate through sorted transactions")
  {
    int count = 0;
    ptime prevDate;
    for (auto it = manager.beginSortedStrategyTransaction(); 
         it != manager.endSortedStrategyTransaction(); ++it)
    {
      REQUIRE(it->second != nullptr);
      if (count > 0)
      {
        // Dates should be in ascending order
        REQUIRE(it->first >= prevDate);
      }
      prevDate = it->first;
      count++;
    }
    REQUIRE(count == 3);
  }
}

TEST_CASE("StrategyTransactionManager - Adding Pre-Completed Transaction", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  auto entry1 = createTimeSeriesEntry("20151222", "202.72", "203.85", "201.55", "203.50", 111026200);
  
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager;
  
  // Create a transaction
  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  entryOrder->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  
  auto position = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, entry0->getOpenValue(), *entry0, oneShare);
  
  auto transaction = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder, position);
  
  // Complete the transaction before adding to manager
  auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), entry1->getDateValue());
  exitOrder->MarkOrderExecuted(entry1->getDateValue(), entry1->getOpenValue());
  transaction->completeTransaction(exitOrder);
  
  REQUIRE(transaction->isTransactionComplete());
  
  SECTION("Adding completed transaction counts it as closed")
  {
    manager.addStrategyTransaction(transaction);
    
    REQUIRE(manager.getTotalTrades() == 1);
    REQUIRE(manager.getOpenTrades() == 0);
    REQUIRE(manager.getClosedTrades() == 1);
  }
}

TEST_CASE("StrategyTransactionManager - Multiple Transactions Same Date", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  
  std::string equitySymbol1("SPY");
  std::string equitySymbol2("QQQ");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  StrategyTransactionManager<DecimalType> manager;
  
  // Add two transactions with same entry date but different symbols
  auto entryOrder1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol1, createShareVolume(1), createDate("20151218"));
  entryOrder1->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  auto position1 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol1, entry0->getOpenValue(), *entry0, oneShare);
  auto transaction1 = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder1, position1);
  
  auto entryOrder2 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol2, createShareVolume(1), createDate("20151218"));
  entryOrder2->MarkOrderExecuted(entry0->getDateValue(), entry0->getOpenValue());
  auto position2 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol2, entry0->getOpenValue(), *entry0, oneShare);
  auto transaction2 = std::make_shared<StrategyTransaction<DecimalType>>(entryOrder2, position2);
  
  SECTION("Multimap handles multiple transactions with same date")
  {
    manager.addStrategyTransaction(transaction1);
    manager.addStrategyTransaction(transaction2);
    
    REQUIRE(manager.getTotalTrades() == 2);
    REQUIRE(manager.getOpenTrades() == 2);
    
    // Both should be in sorted map
    int count = 0;
    for (auto it = manager.beginSortedStrategyTransaction(); 
         it != manager.endSortedStrategyTransaction(); ++it)
    {
      count++;
    }
    REQUIRE(count == 2);
  }
}

TEST_CASE("StrategyTransactionManager - Observer Pattern Edge Cases", "[StrategyTransactionManager]")
{
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  auto entry1 = createTimeSeriesEntry("20151222", "202.72", "203.85", "201.55", "203.50", 111026200);
  
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  SECTION("Manager correctly handles underflow protection")
  {
    StrategyTransactionManager<DecimalType> manager;
    
    // This shouldn't happen in practice, but the code has underflow protection
    REQUIRE(manager.getOpenTrades() == 0);
    
    // Manually call TransactionComplete (simulating unexpected behavior)
    manager.TransactionComplete(nullptr);
    
    // Should increment completed but not decrement open below zero
    REQUIRE(manager.getClosedTrades() == 1);
    REQUIRE(manager.getOpenTrades() == 0);
  }
}
