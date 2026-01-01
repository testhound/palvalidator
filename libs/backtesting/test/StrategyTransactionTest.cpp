#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include "TradingOrder.h"
#include "TradingPosition.h"
#include "StrategyTransaction.h"
#include "InstrumentPosition.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TradingVolume createShareVolume(volume_t vol);
TradingVolume createContractVolume(volume_t vol);

template <class Decimal>
class TransactionObserver : public StrategyTransactionObserver<Decimal>
{
public:
  TransactionObserver() :
    StrategyTransactionObserver<Decimal>(),
    mNumClosedTransactions(0),
    mLastCompletedTransaction(nullptr)
  {}

  ~TransactionObserver()
  {}

  int getNumClosedTransactions() const
  {
    return mNumClosedTransactions;
  }

  StrategyTransaction<Decimal>* getLastCompletedTransaction() const
  {
    return mLastCompletedTransaction;
  }

  void TransactionComplete(StrategyTransaction<Decimal> *transaction)
  {
    mNumClosedTransactions++;
    mLastCompletedTransaction = transaction;
  }

private:
  int mNumClosedTransactions;
  StrategyTransaction<Decimal>* mLastCompletedTransaction;
};

// Helper function to create a basic long transaction
template <class Decimal>
std::shared_ptr<StrategyTransaction<Decimal>> 
createBasicLongTransaction(const std::string& symbol, const std::string& dateStr, const std::string& priceStr)
{
  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<Decimal>>(
    symbol, createShareVolume(1), createDate(dateStr));
  
  entryOrder->MarkOrderExecuted(createDate(dateStr), createDecimal(priceStr));
  
  auto entry0 = createTimeSeriesEntry(dateStr, priceStr, priceStr, priceStr, priceStr, 100000);
  auto position = std::make_shared<TradingPositionLong<Decimal>>(
    symbol, createDecimal(priceStr), *entry0, createShareVolume(1));
  
  return std::make_shared<StrategyTransaction<Decimal>>(entryOrder, position);
}

// Helper function to create a basic short transaction
template <class Decimal>
std::shared_ptr<StrategyTransaction<Decimal>> 
createBasicShortTransaction(const std::string& symbol, const std::string& dateStr, const std::string& priceStr)
{
  auto entryOrder = std::make_shared<MarketOnOpenShortOrder<Decimal>>(
    symbol, createShareVolume(1), createDate(dateStr));
  
  entryOrder->MarkOrderExecuted(createDate(dateStr), createDecimal(priceStr));
  
  auto entry0 = createTimeSeriesEntry(dateStr, priceStr, priceStr, priceStr, priceStr, 100000);
  auto position = std::make_shared<TradingPositionShort<Decimal>>(
    symbol, createDecimal(priceStr), *entry0, createShareVolume(1));
  
  return std::make_shared<StrategyTransaction<Decimal>>(entryOrder, position);
}

TEST_CASE("StrategyTransaction Operations", "[StrategyTransaction]")
{
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  auto longSpyEntryOrder1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), createDate("20151218"));
  
  longSpyEntryOrder1->MarkOrderExecuted(createDate("20151221"), createDecimal("201.41"));

  auto entry5 = createTimeSeriesEntry("20151229", "206.51", "207.79", "206.47", "207.40", 92640700);
  auto entry4 = createTimeSeriesEntry("20151228", "204.86", "205.26", "203.94", "205.21", 65899900);
  auto entry3 = createTimeSeriesEntry("20151224", "205.72", "206.33", "205.42", "205.68", 48542200);
  auto entry2 = createTimeSeriesEntry("20151223", "204.69", "206.07", "204.58", "206.02", 48542200);
  auto entry1 = createTimeSeriesEntry("20151222", "202.72", "203.85", "201.55", "203.50", 111026200);
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  
  auto longSpyPosition1 = std::make_shared<TradingPositionLong<DecimalType>>(
    equitySymbol, createDecimal("201.41"), *entry0, oneShare);
  
  InstrumentPosition<DecimalType> instrumentPositionSpy(equitySymbol);
  instrumentPositionSpy.addPosition(longSpyPosition1);
  TransactionObserver<DecimalType> observer;

  auto strategyTrans = std::make_shared<StrategyTransaction<DecimalType>>(
    longSpyEntryOrder1, longSpyPosition1);
  
  REQUIRE(observer.getNumClosedTransactions() == 0);
  strategyTrans->addObserver(observer);
  REQUIRE(observer.getNumClosedTransactions() == 0);

  instrumentPositionSpy.addBar(*entry1);
  instrumentPositionSpy.addBar(*entry2);
  instrumentPositionSpy.addBar(*entry3);
  instrumentPositionSpy.addBar(*entry4);

  REQUIRE(longSpyPosition1->getNumBarsInPosition() == 5);
  REQUIRE(longSpyEntryOrder1->isOrderExecuted());
  REQUIRE(longSpyEntryOrder1->isLongOrder());
  REQUIRE(longSpyPosition1->isPositionOpen());
  REQUIRE(longSpyPosition1->isLongPosition());

  REQUIRE(strategyTrans->isTransactionOpen());
  REQUIRE_FALSE(strategyTrans->isTransactionComplete());

  REQUIRE(strategyTrans->getEntryTradingOrder()->getFillPrice() == createDecimal("201.41"));
  REQUIRE(strategyTrans->getTradingPosition()->getEntryPrice() == createDecimal("201.41"));
  REQUIRE(strategyTrans->getTradingPosition()->getNumBarsInPosition() == 5);

  auto longSpyExitOrder1 = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
    equitySymbol, createShareVolume(1), entry4->getDateValue());
  
  longSpyExitOrder1->MarkOrderExecuted(entry5->getDateValue(), entry5->getOpenValue());
  instrumentPositionSpy.closeAllPositions(
    longSpyExitOrder1->getFillDate(), longSpyExitOrder1->getFillPrice());
  
  strategyTrans->completeTransaction(longSpyExitOrder1);
  
  REQUIRE(observer.getNumClosedTransactions() == 1);
  REQUIRE(strategyTrans->getTradingPosition()->isPositionClosed());
  REQUIRE(strategyTrans->getExitTradingOrder()->getFillPrice() == entry5->getOpenValue());
  REQUIRE(strategyTrans->getExitTradingOrder()->getFillDate() == entry5->getDateValue());
  REQUIRE_FALSE(strategyTrans->isTransactionOpen());
  REQUIRE(strategyTrans->isTransactionComplete());
}

TEST_CASE("StrategyTransaction Constructor Validation", "[StrategyTransaction][validation]")
{
  std::string symbol1("SPY");
  std::string symbol2("QQQ");
  
  SECTION("Constructor throws when symbols don't match")
  {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol1, createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("201.41"));
    
    auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
    auto position = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol2, createDecimal("201.41"), *entry0, createShareVolume(1));
    
    REQUIRE_THROWS_AS(
      StrategyTransaction<DecimalType>(entryOrder, position),
      StrategyTransactionException
    );
  }
  
  SECTION("Constructor throws when long order paired with short position")
  {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol1, createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("201.41"));
    
    auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
    auto position = std::make_shared<TradingPositionShort<DecimalType>>(
      symbol1, createDecimal("201.41"), *entry0, createShareVolume(1));
    
    REQUIRE_THROWS_AS(
      StrategyTransaction<DecimalType>(entryOrder, position),
      StrategyTransactionException
    );
  }
  
  SECTION("Constructor throws when short order paired with long position")
  {
    auto entryOrder = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(
      symbol1, createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("201.41"));
    
    auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
    auto position = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol1, createDecimal("201.41"), *entry0, createShareVolume(1));
    
    REQUIRE_THROWS_AS(
      StrategyTransaction<DecimalType>(entryOrder, position),
      StrategyTransactionException
    );
  }
  
  SECTION("Constructor succeeds with matching long order and position")
  {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol1, createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("201.41"));
    
    auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
    auto position = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol1, createDecimal("201.41"), *entry0, createShareVolume(1));
    
    REQUIRE_NOTHROW(
      StrategyTransaction<DecimalType>(entryOrder, position)
    );
  }
  
  SECTION("Constructor succeeds with matching short order and position")
  {
    auto entryOrder = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(
      symbol1, createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("201.41"));
    
    auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
    auto position = std::make_shared<TradingPositionShort<DecimalType>>(
      symbol1, createDecimal("201.41"), *entry0, createShareVolume(1));
    
    REQUIRE_NOTHROW(
      StrategyTransaction<DecimalType>(entryOrder, position)
    );
  }
}

TEST_CASE("StrategyTransaction State Transitions", "[StrategyTransaction][state]")
{
  auto transaction = createBasicLongTransaction<DecimalType>("SPY", "20151221", "201.41");
  
  SECTION("Transaction starts in Open state")
  {
    REQUIRE(transaction->isTransactionOpen());
    REQUIRE_FALSE(transaction->isTransactionComplete());
  }
  
  SECTION("Cannot get exit order when transaction is open")
  {
    REQUIRE_THROWS_AS(
      transaction->getExitTradingOrder(),
      StrategyTransactionException
    );
  }
  
  SECTION("Transaction transitions to Complete state after completeTransaction")
  {
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder->MarkOrderExecuted(createDate("20151222"), createDecimal("205.00"));
    
    transaction->completeTransaction(exitOrder);
    
    REQUIRE_FALSE(transaction->isTransactionOpen());
    REQUIRE(transaction->isTransactionComplete());
    REQUIRE(transaction->getExitTradingOrder() == exitOrder);
  }
  
  SECTION("Cannot complete an already completed transaction")
  {
    auto exitOrder1 = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder1->MarkOrderExecuted(createDate("20151222"), createDecimal("205.00"));
    
    transaction->completeTransaction(exitOrder1);
    
    auto exitOrder2 = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151223"));
    exitOrder2->MarkOrderExecuted(createDate("20151223"), createDecimal("207.00"));
    
    REQUIRE_THROWS_AS(
      transaction->completeTransaction(exitOrder2),
      StrategyTransactionException
    );
  }
}

TEST_CASE("StrategyTransaction Observer Pattern", "[StrategyTransaction][observer]")
{
  auto transaction = createBasicLongTransaction<DecimalType>("SPY", "20151221", "201.41");
  
  SECTION("Observer is notified when transaction completes")
  {
    TransactionObserver<DecimalType> observer;
    transaction->addObserver(observer);
    
    REQUIRE(observer.getNumClosedTransactions() == 0);
    
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder->MarkOrderExecuted(createDate("20151222"), createDecimal("205.00"));
    
    transaction->completeTransaction(exitOrder);
    
    REQUIRE(observer.getNumClosedTransactions() == 1);
    REQUIRE(observer.getLastCompletedTransaction() == transaction.get());
  }
  
  SECTION("Multiple observers are all notified")
  {
    TransactionObserver<DecimalType> observer1;
    TransactionObserver<DecimalType> observer2;
    TransactionObserver<DecimalType> observer3;
    
    transaction->addObserver(observer1);
    transaction->addObserver(observer2);
    transaction->addObserver(observer3);
    
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder->MarkOrderExecuted(createDate("20151222"), createDecimal("205.00"));
    
    transaction->completeTransaction(exitOrder);
    
    REQUIRE(observer1.getNumClosedTransactions() == 1);
    REQUIRE(observer2.getNumClosedTransactions() == 1);
    REQUIRE(observer3.getNumClosedTransactions() == 1);
  }
  
  SECTION("Observer added after completion is not notified")
  {
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder->MarkOrderExecuted(createDate("20151222"), createDecimal("205.00"));
    
    transaction->completeTransaction(exitOrder);
    
    TransactionObserver<DecimalType> observer;
    transaction->addObserver(observer);
    
    REQUIRE(observer.getNumClosedTransactions() == 0);
  }
  
  SECTION("Same observer can be added multiple times")
  {
    TransactionObserver<DecimalType> observer;
    transaction->addObserver(observer);
    transaction->addObserver(observer);
    
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder->MarkOrderExecuted(createDate("20151222"), createDecimal("205.00"));
    
    transaction->completeTransaction(exitOrder);
    
    // Observer notified twice
    REQUIRE(observer.getNumClosedTransactions() == 2);
  }
}

TEST_CASE("StrategyTransaction Copy Constructor", "[StrategyTransaction][copy]")
{
  auto original = createBasicLongTransaction<DecimalType>("SPY", "20151221", "201.41");
  TransactionObserver<DecimalType> observer;
  original->addObserver(observer);
  
  SECTION("Copy constructor creates valid transaction")
  {
    StrategyTransaction<DecimalType> copy(*original);
    
    REQUIRE(copy.isTransactionOpen());
    REQUIRE_FALSE(copy.isTransactionComplete());
    REQUIRE(copy.getEntryTradingOrder() == original->getEntryTradingOrder());
    REQUIRE(copy.getTradingPosition() == original->getTradingPosition());
  }
  
  SECTION("Copy shares the same underlying orders and position")
  {
    StrategyTransaction<DecimalType> copy(*original);
    
    // Verify they point to the same objects
    REQUIRE(copy.getEntryTradingOrder().get() == original->getEntryTradingOrder().get());
    REQUIRE(copy.getTradingPosition().get() == original->getTradingPosition().get());
  }
  
  SECTION("Copied transaction maintains state independently")
  {
    StrategyTransaction<DecimalType> copy(*original);
    
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder->MarkOrderExecuted(createDate("20151222"), createDecimal("205.00"));
    
    original->completeTransaction(exitOrder);
    
    REQUIRE(original->isTransactionComplete());
    REQUIRE(copy.isTransactionOpen()); // Copy still open
  }
}


TEST_CASE("StrategyTransaction Copy Assignment", "[StrategyTransaction][copy]")
{
  auto source = createBasicLongTransaction<DecimalType>("SPY", "20151221", "201.41");
  auto target = createBasicLongTransaction<DecimalType>("QQQ", "20151221", "100.00");
  
  SECTION("Assignment replaces transaction data")
  {
    *target = *source;
    
    REQUIRE(target->getEntryTradingOrder() == source->getEntryTradingOrder());
    REQUIRE(target->getTradingPosition() == source->getTradingPosition());
  }
  
  SECTION("Self-assignment is safe")
  {
    auto original = createBasicLongTransaction<DecimalType>("SPY", "20151221", "201.41");
    auto* originalPtr = original.get();
    
    *original = *original;
    
    REQUIRE(original.get() == originalPtr);
    REQUIRE(original->isTransactionOpen());
  }
}

TEST_CASE("StrategyTransaction Getters", "[StrategyTransaction][accessors]")
{
  std::string symbol("SPY");
  auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
    symbol, createShareVolume(1), createDate("20151218"));
  entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("201.41"));
  
  auto entry0 = createTimeSeriesEntry("20151221", "201.41", "201.88", "200.09", "201.67", 99094300);
  auto position = std::make_shared<TradingPositionLong<DecimalType>>(
    symbol, createDecimal("201.41"), *entry0, createShareVolume(1));
  
  StrategyTransaction<DecimalType> transaction(entryOrder, position);
  
  SECTION("getEntryTradingOrder returns correct order")
  {
    REQUIRE(transaction.getEntryTradingOrder() == entryOrder);
    REQUIRE(transaction.getEntryTradingOrder()->getFillPrice() == createDecimal("201.41"));
  }
  
  SECTION("getTradingPosition returns correct position")
  {
    REQUIRE(transaction.getTradingPosition() == position);
    REQUIRE(transaction.getTradingPosition()->getEntryPrice() == createDecimal("201.41"));
  }
  
  SECTION("getTradingPositionPtr returns same as getTradingPosition")
  {
    REQUIRE(transaction.getTradingPositionPtr() == transaction.getTradingPosition());
  }
}

TEST_CASE("StrategyTransaction Short Position", "[StrategyTransaction][short]")
{
  auto shortTransaction = createBasicShortTransaction<DecimalType>("SPY", "20151221", "201.41");
  
  SECTION("Short transaction is created successfully")
  {
    REQUIRE(shortTransaction->isTransactionOpen());
    REQUIRE(shortTransaction->getEntryTradingOrder()->isShortOrder());
    REQUIRE(shortTransaction->getTradingPosition()->isShortPosition());
  }
  
  SECTION("Short transaction can be completed")
  {
    auto exitOrder = std::make_shared<MarketOnOpenCoverOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151222"));
    exitOrder->MarkOrderExecuted(createDate("20151222"), createDecimal("198.00"));
    
    shortTransaction->completeTransaction(exitOrder);
    
    REQUIRE(shortTransaction->isTransactionComplete());
    REQUIRE(shortTransaction->getExitTradingOrder()->getFillPrice() == createDecimal("198.00"));
  }
}

TEST_CASE("StrategyTransaction Edge Cases", "[StrategyTransaction][edge]")
{
  SECTION("Transaction with zero-priced entry")
  {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      "SPY", createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("0.01"));
    
    auto entry0 = createTimeSeriesEntry("20151221", "0.01", "0.02", "0.01", "0.01", 100000);
    auto position = std::make_shared<TradingPositionLong<DecimalType>>(
      "SPY", createDecimal("0.01"), *entry0, createShareVolume(1));
    
    REQUIRE_NOTHROW(
      StrategyTransaction<DecimalType>(entryOrder, position)
    );
  }
  
  SECTION("Transaction with high-priced entry")
  {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      "BRK.A", createShareVolume(1), createDate("20151218"));
    entryOrder->MarkOrderExecuted(createDate("20151221"), createDecimal("500000.00"));
    
    auto entry0 = createTimeSeriesEntry("20151221", "500000.00", "500100.00", 
                                        "499900.00", "500050.00", 100);
    auto position = std::make_shared<TradingPositionLong<DecimalType>>(
      "BRK.A", createDecimal("500000.00"), *entry0, createShareVolume(1));
    
    REQUIRE_NOTHROW(
      StrategyTransaction<DecimalType>(entryOrder, position)
    );
  }
}

TEST_CASE("StrategyTransaction Observer Removal", "[StrategyTransaction][observer][future]")
{
  // Note: Current implementation doesn't support observer removal
  // This test documents the desired functionality for future implementation
  
  auto transaction = createBasicLongTransaction<DecimalType>("SPY", "20151221", "201.41");
  TransactionObserver<DecimalType> observer1;
  TransactionObserver<DecimalType> observer2;
  
  transaction->addObserver(observer1);
  transaction->addObserver(observer2);
  
  // TODO: Implement removeObserver method
  // transaction->removeObserver(observer1);
  
  INFO("Current implementation lacks removeObserver functionality");
  INFO("Consider adding: void removeObserver(StrategyTransactionObserver<Decimal>& observer)");
}

TEST_CASE("StrategyTransaction Thread Safety", "[StrategyTransaction][threading][future]")
{
  // Note: Current implementation is not thread-safe
  // This test documents considerations for future thread-safe implementation
  
  INFO("Current implementation is not thread-safe");
  INFO("If concurrent access is required, consider:");
  INFO("1. Adding std::mutex for state transitions");
  INFO("2. Using std::atomic for state if applicable");
  INFO("3. Protecting observer list with mutex");
  INFO("4. Document thread-safety guarantees");
}
