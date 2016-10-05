#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TradingOrder.h"
#include "../TradingPosition.h"
#include "../StrategyTransaction.h"
#include "../InstrumentPosition.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef dec::decimal<7> DecimalType;

TradingVolume
createShareVolume (volume_t vol)
{
  return TradingVolume (vol, TradingVolume::SHARES);
}

date createDate (const std::string& dateString)
{
  return from_undelimited_string(dateString);
}

TradingVolume
createContractVolume (volume_t vol)
{
  return TradingVolume (vol, TradingVolume::CONTRACTS);
}


DecimalType
createDecimal(const std::string& valueString)
{
  return fromString<DecimalType>(valueString);
}

std::shared_ptr<OHLCTimeSeriesEntry<7>>
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
    return std::make_shared<OHLCTimeSeriesEntry<7>>(date1, open1, high1, low1, 
						close1, vol, TimeFrame::DAILY);
  }

template <int Prec>
class TransactionObserver : public StrategyTransactionObserver<Prec>
{
public:
  TransactionObserver() :
    StrategyTransactionObserver<Prec>(),
    mNumClosedTransactions(0)
  {}

  ~TransactionObserver()
  {}

  int getNumClosedTransactions() const
  {
    return mNumClosedTransactions;
  }

  void TransactionComplete (StrategyTransaction<Prec> *transaction)
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
  auto longSpyEntryOrder1 = std::make_shared<MarketOnOpenLongOrder<7>>(equitySymbol,
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
  auto longSpyPosition1 = std::make_shared<TradingPositionLong<7>>(equitySymbol,
								   createDecimal("201.41"),
								   entry0,
								   oneShare);
  InstrumentPosition<7> instrumentPositionSpy (equitySymbol);
  instrumentPositionSpy.addPosition(longSpyPosition1);
  TransactionObserver<7> observer;

  auto strategyTrans = std::make_shared<StrategyTransaction<7>>(longSpyEntryOrder1,
								longSpyPosition1);
  REQUIRE (observer.getNumClosedTransactions() == 0);
  strategyTrans->addObserver (observer);
  REQUIRE (observer.getNumClosedTransactions() == 0);

  instrumentPositionSpy.addBar(entry1);
  instrumentPositionSpy.addBar(entry2);
  instrumentPositionSpy.addBar(entry3);
  instrumentPositionSpy.addBar(entry4);

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

  auto longSpyExitOrder1 = std::make_shared<MarketOnOpenSellOrder<7>>(equitySymbol,
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
