#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TradingOrder.h"
#include "../TradingPosition.h"
#include "../StrategyTransaction.h"
#include "../InstrumentPosition.h"
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


template <class Decimal>
class TransactionObserver : public StrategyTransactionObserver<Decimal>
{
public:
  TransactionObserver() :
    StrategyTransactionObserver<Decimal>(),
    mNumClosedTransactions(0)
  {}

  ~TransactionObserver()
  {}

  int getNumClosedTransactions() const
  {
    return mNumClosedTransactions;
  }

  void TransactionComplete (StrategyTransaction<Decimal> *transaction)
  {
    mNumClosedTransactions++;
  }

private:
  int mNumClosedTransactions;
};

TEST_CASE ("TradingOrderManager Operations", "[TradingOrderManager]")
{
  std::string equitySymbol("SPY");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  auto longSpyEntryOrder1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(equitySymbol,
								       createShareVolume(1),
								       createDate("20151218"));
  longSpyEntryOrder1->MarkOrderExecuted (createDate("20151221"),
					 createDecimal("201.41"));

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
  auto longSpyPosition1 = std::make_shared<TradingPositionLong<DecimalType>>(equitySymbol,
								   createDecimal("201.41"),
								   *entry0,
								   oneShare);
  InstrumentPosition<DecimalType> instrumentPositionSpy (equitySymbol);
  instrumentPositionSpy.addPosition(longSpyPosition1);
  TransactionObserver<DecimalType> observer;

  auto strategyTrans = std::make_shared<StrategyTransaction<DecimalType>>(longSpyEntryOrder1,
								longSpyPosition1);
  REQUIRE (observer.getNumClosedTransactions() == 0);
  strategyTrans->addObserver (observer);
  REQUIRE (observer.getNumClosedTransactions() == 0);

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

  REQUIRE (strategyTrans->getEntryTradingOrder()->getFillPrice() == createDecimal("201.41"));
  REQUIRE (strategyTrans->getTradingPosition()->getEntryPrice() == createDecimal("201.41"));
  REQUIRE (strategyTrans->getTradingPosition()->getNumBarsInPosition() == 5);

  auto longSpyExitOrder1 = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(equitySymbol,
								       createShareVolume(1),
								       entry4->getDateValue());
  longSpyExitOrder1->MarkOrderExecuted (entry5->getDateValue(),
					 entry5->getOpenValue());
  instrumentPositionSpy.closeAllPositions (longSpyExitOrder1->getFillDate(),
					   longSpyExitOrder1->getFillPrice());
  strategyTrans->completeTransaction (longSpyExitOrder1);
  REQUIRE (observer.getNumClosedTransactions() == 1);
  REQUIRE (strategyTrans->getTradingPosition()->isPositionClosed());
  REQUIRE (strategyTrans->getExitTradingOrder()->getFillPrice() == entry5->getOpenValue());
  REQUIRE (strategyTrans->getExitTradingOrder()->getFillDate() == entry5->getDateValue());

  SECTION ("Test callback to observers")
    {

    }
}
