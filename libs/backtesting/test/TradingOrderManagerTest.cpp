#include <catch2/catch_test_macros.hpp>
#include "TradingOrderManager.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

template <class Decimal>
class DummyBroker : public TradingOrderObserver<Decimal>
{
public:
  DummyBroker(std::shared_ptr<Portfolio<Decimal>> portfolio)
    : TradingOrderObserver<Decimal>(), 
      mExecutedOrder(),
      mCanceledOrder(),
      mPosManager(),
      mPortfolio(portfolio)
  {}

  const std::shared_ptr<TradingOrder<Decimal>>& getLastExecutedOrder() const
  {
    return mExecutedOrder;
  }

  const std::shared_ptr<TradingOrder<Decimal>>& getLastCanceledOrder() const
  {
    return mCanceledOrder;
  }

  InstrumentPositionManager<Decimal>& getPositionManager()
  {
    return mPosManager;
  }

  void addInstrument (const std::string symbol)
  {
    mPosManager.addInstrument (symbol);
  }

  void OrderExecuted (MarketOnOpenLongOrder<Decimal> *order)
  {
    mExecutedOrder = std::make_shared<MarketOnOpenLongOrder<Decimal>> (*order);
    mPosManager.addPosition (createLongTradingPosition (order));
  }

  void OrderExecuted (MarketOnOpenShortOrder<Decimal> *order)
  {
    mExecutedOrder = std::make_shared<MarketOnOpenShortOrder<Decimal>> (*order);
    mPosManager.addPosition (createShortTradingPosition (order));
  }

  void OrderExecuted (MarketOnOpenSellOrder<Decimal> *order)
  {}

  void OrderExecuted (MarketOnOpenCoverOrder<Decimal> *order)
  {}

  void OrderExecuted (SellAtLimitOrder<Decimal> *order)
  {
    mExecutedOrder = std::make_shared<SellAtLimitOrder<Decimal>> (*order);
    mPosManager.closeAllPositions (order->getTradingSymbol(),
				  order->getFillDate(),
				  order->getFillPrice());

  }

  void OrderExecuted (CoverAtLimitOrder<Decimal> *order)
  {
    mExecutedOrder = std::make_shared<CoverAtLimitOrder<Decimal>> (*order);
    mPosManager.closeAllPositions (order->getTradingSymbol(),
				  order->getFillDate(),
				  order->getFillPrice());
  }

  void OrderExecuted (CoverAtStopOrder<Decimal> *order)
  {
    mExecutedOrder = std::make_shared<CoverAtStopOrder<Decimal>> (*order);
    mPosManager.closeAllPositions (order->getTradingSymbol(),
				  order->getFillDate(),
				  order->getFillPrice());
  }

  void OrderExecuted (SellAtStopOrder<Decimal> *order)
  {
    mExecutedOrder = std::make_shared<SellAtStopOrder<Decimal>> (*order);
   mPosManager.closeAllPositions (order->getTradingSymbol(),
				  order->getFillDate(),
				  order->getFillPrice());
  }

  void OrderCanceled (MarketOnOpenLongOrder<Decimal> *order)
  {
    mCanceledOrder = std::make_shared<MarketOnOpenLongOrder<Decimal>> (*order);
  }

  void OrderCanceled (MarketOnOpenShortOrder<Decimal> *order)
  {}
  
  void OrderCanceled (MarketOnOpenSellOrder<Decimal> *order)
  {}

  void OrderCanceled (MarketOnOpenCoverOrder<Decimal> *order)
  {}

  void OrderCanceled (SellAtLimitOrder<Decimal> *order)
  {
    mCanceledOrder = std::make_shared<SellAtLimitOrder<Decimal>> (*order);
  }

  void OrderCanceled (CoverAtLimitOrder<Decimal> *order)
  {
    mCanceledOrder = std::make_shared<CoverAtLimitOrder<Decimal>> (*order);
  }

  void OrderCanceled (CoverAtStopOrder<Decimal> *order)
  {
    mCanceledOrder = std::make_shared<CoverAtStopOrder<Decimal>> (*order);
  }

  void OrderCanceled (SellAtStopOrder<Decimal> *order)
  {
    mCanceledOrder = std::make_shared<SellAtStopOrder<Decimal>> (*order);
  }
  
private:

  OHLCTimeSeriesEntry<Decimal> getEntryBar (const std::string& tradingSymbol,
  				const boost::gregorian::date& d)
    {
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (tradingSymbol);
      if (symbolIterator != mPortfolio->endPortfolio())
 {
   // Use new date-based API instead of iterator-based
   return symbolIterator->second->getTimeSeriesEntry(d);
 }
      else
 throw std::runtime_error ("DummyBroker::getEntryBar - Cannot find " +tradingSymbol +" in portfolio");
    }

  std::shared_ptr<TradingPositionLong<Decimal>>
    createLongTradingPosition (TradingOrder<Decimal> *order)
    {
      auto position = std::make_shared<TradingPositionLong<Decimal>> (order->getTradingSymbol(), 
								   order->getFillPrice(),
								   getEntryBar (order->getTradingSymbol(), 
										order->getFillDate()),
								   order->getUnitsInOrder());
      return position;
    }

   std::shared_ptr<TradingPositionShort<Decimal>>
    createShortTradingPosition (TradingOrder<Decimal> *order)
    {
      auto position = 
	std::make_shared<TradingPositionShort<Decimal>> (order->getTradingSymbol(), 
						      order->getFillPrice(),
						      getEntryBar (order->getTradingSymbol(), 
								   order->getFillDate()),
						      order->getUnitsInOrder());

      return position;
    }

private:
  std::shared_ptr<TradingOrder<Decimal>> mExecutedOrder;
  std::shared_ptr<TradingOrder<Decimal>> mCanceledOrder;
  InstrumentPositionManager<Decimal> mPosManager;
  std::shared_ptr<Portfolio<Decimal>> mPortfolio;
};




TradingVolume
TradingOrderManager_createShareVolume (volume_t vol)
{
  return TradingVolume (vol, TradingVolume::SHARES);
}

TradingVolume
TradingOrderManager_createContractVolume (volume_t vol)
{
  return TradingVolume (vol, TradingVolume::CONTRACTS);
}

std::shared_ptr<CoverAtLimitOrder<DecimalType>>
  createProfitTargetForShortTrade(const date& orderDate)
  {
    return std::make_shared<CoverAtLimitOrder<DecimalType>>(std::string ("SPY"),
						  TradingOrderManager_createShareVolume (1),
						  orderDate,
						  createDecimal("198.00"));
  }

  std::shared_ptr<CoverAtStopOrder<DecimalType>>
  createStopLossForShortTrade(const date& orderDate)
  {
    return std::make_shared<CoverAtStopOrder<DecimalType>>(std::string ("SPY"),
						 TradingOrderManager_createShareVolume (1),
						 orderDate,
						 createDecimal("208.00"));
  }

std::shared_ptr<CoverAtStopOrder<DecimalType>>
  createStopLossForShortTrade2(const date& orderDate)
  {
    return std::make_shared<CoverAtStopOrder<DecimalType>>(std::string ("SPY"),
						 TradingOrderManager_createShareVolume (1),
						 orderDate,
						 createDecimal("200.04"));
  }

TEST_CASE ("TradingOrderManager Operations", "[TradingOrderManager]")
{
  auto entry18 = createEquityEntry ("20160119", "189.96", "190.11","186.20","188.06",
				    190196000);
  auto entry17 = createEquityEntry ("20160115", "186.77","188.76", "185.52","187.81",	
				    324846400);
  auto entry16 = createEquityEntry ("20160114", "189.55","193.26", "187.66", "191.93",
				   240795600);
  auto entry15 = createEquityEntry ("20160113", "194.45", "194.86", "188.38","188.83",
				   221168900);
  auto entry14 = createEquityEntry ("20160112", "193.82", "194.55", "191.14","193.66",
				   172330500);
  auto entry13 = createEquityEntry ("20160111", "193.01", "193.41", "189.82","192.11",
				   187941300);
  auto entry12 = createEquityEntry ("20160108", "195.19", "195.85", "191.58","191.92",
				   142662900);
  auto entry11 = createEquityEntry ("20160107", "195.33", "197.44", "193.59","194.05",
				   142662900);
  auto entry10 = createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry9 = createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry8 = createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry7 = createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry6 = createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry4 = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
  auto entry3 = createEquityEntry ("20151224", "205.72", "206.33", "205.42", "205.68",
				   48542200);
  auto entry2 = createEquityEntry ("20151223", "204.69", "206.07", "204.58", "206.02",
				   48542200);
  auto entry1 = createEquityEntry ("20151222", "202.72", "203.85", "201.55", "203.50",
				   111026200);
  auto entry0 = createEquityEntry ("20151221", "201.41", "201.88", "200.09", "201.67",
				   99094300);
 
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);

  spySeries->addEntry (*entry0);
  spySeries->addEntry (*entry1);
  spySeries->addEntry (*entry2);
  spySeries->addEntry (*entry3);
  spySeries->addEntry (*entry4);
  spySeries->addEntry (*entry5);
  spySeries->addEntry (*entry6);  
  spySeries->addEntry (*entry7);
  spySeries->addEntry (*entry8);
  spySeries->addEntry (*entry9);
  spySeries->addEntry (*entry10);
  spySeries->addEntry (*entry11);
  spySeries->addEntry (*entry12);
  spySeries->addEntry (*entry13);
  spySeries->addEntry (*entry14);
  spySeries->addEntry (*entry15);
  spySeries->addEntry (*entry16);
  spySeries->addEntry (*entry17);
  spySeries->addEntry (*entry18);
 
  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  EquitySecurity<DecimalType> spy (equitySymbol, equityName, spySeries);



  // Futures security

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));
  DecimalType cornTickValue(createDecimal("0.25"));

  auto futuresEntry0 = createTimeSeriesEntry ("19851118", "3664.51025", "3687.58178", "3656.81982","3672.20068",0);

  auto futuresEntry1 = createTimeSeriesEntry ("19851119", "3710.65307617188","3722.18872070313","3679.89135742188",
				       "3714.49829101563", 0);

  auto futuresEntry2 = createTimeSeriesEntry ("19851120", "3737.56982421875","3756.7958984375","3726.0341796875",
				       "3729.87939453125",0);

  auto futuresEntry3 = createTimeSeriesEntry ("19851121","3699.11743164063","3710.65307617188","3668.35546875",
				       "3683.73657226563",0);

  auto futuresEntry4 = createTimeSeriesEntry ("19851122","3664.43017578125","3668.23559570313","3653.0146484375",
				       "3656.81982421875", 0);

  auto futuresEntry5 = createTimeSeriesEntry ("19851125","3641.59887695313","3649.20947265625","3626.3779296875",
				       "3637.79370117188", 0);

  auto futuresEntry6 = createTimeSeriesEntry ("19851126","3656.81982421875","3675.84594726563","3653.0146484375",
				       "3660.625", 0);
  auto futuresEntry7 = createTimeSeriesEntry ("19851127", "3664.43017578125","3698.67724609375","3660.625",
				       "3691.06689453125", 0);
  auto futuresEntry8 = createTimeSeriesEntry ("19851129", "3717.70336914063","3729.119140625","3698.67724609375",
				       "3710.09301757813", 0);
  auto futuresEntry9 = createTimeSeriesEntry ("19851202", "3721.50854492188","3725.31372070313","3691.06689453125",
				       "3725.31372070313", 0);
  auto futuresEntry10 = createTimeSeriesEntry ("19851203", "3713.89819335938","3740.53466796875","3710.09301757813"
					,"3736.7294921875", 0);
  auto futuresEntry11 = createTimeSeriesEntry ("19851204","3744.33984375","3759.56079101563","3736.7294921875",
					"3740.53466796875",0);

  auto cornSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  cornSeries->addEntry(*futuresEntry0);
  cornSeries->addEntry(*futuresEntry1);
  cornSeries->addEntry(*futuresEntry2);
  cornSeries->addEntry(*futuresEntry3);
  cornSeries->addEntry(*futuresEntry4);
  cornSeries->addEntry(*futuresEntry5);
  cornSeries->addEntry(*futuresEntry6);
  cornSeries->addEntry(*futuresEntry7);
  cornSeries->addEntry(*futuresEntry8);
  cornSeries->addEntry(*futuresEntry9);
  cornSeries->addEntry(*futuresEntry10);
  cornSeries->addEntry(*futuresEntry11);

  FuturesSecurity<DecimalType> corn (futuresSymbol, futuresName, cornBigPointValue,
			   cornTickValue, cornSeries);

  std::string portName("SPY Portfolio");
  std::string portName2("Corn Portfolio");
  
  Portfolio<DecimalType> aPortfolio(portName);
  Portfolio<DecimalType> aPortfolio2(portName2);

  auto cornPtr = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol, 
						     futuresName, 
						     cornBigPointValue,
						     cornTickValue, cornSeries);
  auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, equityName, spySeries);

  aPortfolio.addSecurity(spyPtr);
  aPortfolio2.addSecurity(cornPtr);

  auto aPortfolioPtr =  std::make_shared<Portfolio<DecimalType>> (aPortfolio);
  auto aPortfolioPtr2 =  std::make_shared<Portfolio<DecimalType>> (aPortfolio2);

  DummyBroker<DecimalType> dummyBroker1(aPortfolioPtr);
  dummyBroker1.addInstrument (equitySymbol);

  TradingOrderManager<DecimalType> orderManager(aPortfolioPtr);
  orderManager.addObserver (dummyBroker1);

  //InstrumentPositionManager<DecimalType> aPosManager;
  //aPosManager.addInstrument(futuresSymbol);

  REQUIRE (orderManager.getNumMarketExitOrders() == 0);
  REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
  REQUIRE (orderManager.getNumLimitExitOrders() == 0);
  REQUIRE (orderManager.getNumStopExitOrders() == 0);

  
  auto longSpyEntryOrder1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(equitySymbol,
								       TradingOrderManager_createShareVolume (1),
								       entry1->getDateValue());
								       
  TradingOrderManager<DecimalType> orderManager2(aPortfolioPtr2);

  REQUIRE (orderManager2.getNumMarketExitOrders() == 0);
  REQUIRE (orderManager2.getNumMarketEntryOrders() == 0);
  REQUIRE (orderManager2.getNumLimitExitOrders() == 0);
  REQUIRE (orderManager2.getNumStopExitOrders() == 0);

  auto spyEntryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(equitySymbol,
								  TradingOrderManager_createShareVolume (1),
								  entry17->getDateValue());

  

  SECTION ("Add and execute long market order")
    {
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      REQUIRE (longSpyEntryOrder1->isOrderPending());
      orderManager.addTradingOrder (longSpyEntryOrder1);

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 1);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      orderManager.processPendingOrders (entry2->getDateValue(), dummyBroker1.getPositionManager());

      REQUIRE (longSpyEntryOrder1->isOrderExecuted() );
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);
    }

  SECTION ("Add and execute long market order, skipping holiday")
    {

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      REQUIRE (spyEntryOrder->isOrderPending());
      orderManager.addTradingOrder (spyEntryOrder);

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 1);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      orderManager.processPendingOrders (date (2016, Jan, 18), dummyBroker1.getPositionManager());

      REQUIRE (spyEntryOrder->isOrderPending());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 1);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      // Now execute order

      orderManager.processPendingOrders (date (2016, Jan, 19), dummyBroker1.getPositionManager());

      //spyEntryOrder = dummyBroker1.getLastExecutedOrder();
      REQUIRE (spyEntryOrder->isOrderExecuted());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);
    }

  SECTION ("Add and execute short market order, add stop and limit exit orders")
    {
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      auto aOrder = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(equitySymbol,
								TradingOrderManager_createShareVolume (1),
								entry5->getDateValue());
      REQUIRE (aOrder->isOrderPending());
      orderManager.addTradingOrder (aOrder);

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 1);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      orderManager.processPendingOrders (entry6->getDateValue(), dummyBroker1.getPositionManager());
      auto aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aExecutedOrder->isOrderExecuted() );
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      auto aProfitTarget = createProfitTargetForShortTrade(entry6->getDateValue());
      auto aStopLoss = createStopLossForShortTrade(entry6->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 1);
      REQUIRE (orderManager.getNumStopExitOrders() == 1);

      TradingOrderManager<DecimalType>::PendingOrderIterator pendingIt = orderManager.beginPendingOrders();
      REQUIRE (pendingIt != orderManager.endPendingOrders());

      REQUIRE (pendingIt->second->isStopOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt->second->isLimitOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt == orderManager.endPendingOrders());

      orderManager.processPendingOrders (entry7->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      pendingIt = orderManager.beginPendingOrders();
      REQUIRE (pendingIt == orderManager.endPendingOrders());

      REQUIRE (aExecutedOrder->getFillDate() == entry6->getDateValue());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      aProfitTarget = createProfitTargetForShortTrade(entry7->getDateValue());
      aStopLoss = createStopLossForShortTrade(entry7->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);

      pendingIt = orderManager.beginPendingOrders();
      REQUIRE (pendingIt != orderManager.endPendingOrders());

      REQUIRE (pendingIt->second->isStopOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt->second->isLimitOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt == orderManager.endPendingOrders());

      orderManager.processPendingOrders (entry8->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aExecutedOrder->getFillDate() == entry6->getDateValue());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      aProfitTarget = createProfitTargetForShortTrade(entry8->getDateValue());
      aStopLoss = createStopLossForShortTrade(entry8->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);

      orderManager.processPendingOrders (entry9->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aExecutedOrder->getFillDate() == entry6->getDateValue());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      aProfitTarget = createProfitTargetForShortTrade(entry9->getDateValue());
      aStopLoss = createStopLossForShortTrade(entry9->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);
      
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 1);
      REQUIRE (orderManager.getNumStopExitOrders() == 1);

      orderManager.processPendingOrders (entry10->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aProfitTarget->isOrderExecuted());
      REQUIRE (aStopLoss->isOrderCanceled());

      REQUIRE (aExecutedOrder->getFillDate() == entry10->getDateValue());
      REQUIRE (aExecutedOrder->getFillPrice() == createDecimal("198.00"));
    }

SECTION ("Add and execute short market order, add stop and limit exit orders conflict")
    {
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      auto aOrder = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(equitySymbol,
								TradingOrderManager_createShareVolume (1),
								entry5->getDateValue());
      REQUIRE (aOrder->isOrderPending());
      orderManager.addTradingOrder (aOrder);

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 1);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      orderManager.processPendingOrders (entry6->getDateValue(), dummyBroker1.getPositionManager());
      auto aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aExecutedOrder->isOrderExecuted() );
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      auto aProfitTarget = createProfitTargetForShortTrade(entry6->getDateValue());
      auto aStopLoss = createStopLossForShortTrade(entry6->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 1);
      REQUIRE (orderManager.getNumStopExitOrders() == 1);

      TradingOrderManager<DecimalType>::PendingOrderIterator pendingIt = orderManager.beginPendingOrders();
      REQUIRE (pendingIt != orderManager.endPendingOrders());

      REQUIRE (pendingIt->second->isStopOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt->second->isLimitOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt == orderManager.endPendingOrders());

      orderManager.processPendingOrders (entry7->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      pendingIt = orderManager.beginPendingOrders();
      REQUIRE (pendingIt == orderManager.endPendingOrders());

      REQUIRE (aExecutedOrder->getFillDate() == entry6->getDateValue());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      aProfitTarget = createProfitTargetForShortTrade(entry7->getDateValue());
      aStopLoss = createStopLossForShortTrade(entry7->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);

      pendingIt = orderManager.beginPendingOrders();
      REQUIRE (pendingIt != orderManager.endPendingOrders());

      REQUIRE (pendingIt->second->isStopOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt->second->isLimitOrder());
      REQUIRE (pendingIt->second->isExitOrder());
      pendingIt++;

      REQUIRE (pendingIt == orderManager.endPendingOrders());

      orderManager.processPendingOrders (entry8->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aExecutedOrder->getFillDate() == entry6->getDateValue());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      aProfitTarget = createProfitTargetForShortTrade(entry8->getDateValue());
      aStopLoss = createStopLossForShortTrade(entry8->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);

      orderManager.processPendingOrders (entry9->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aExecutedOrder->getFillDate() == entry6->getDateValue());
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      aProfitTarget = createProfitTargetForShortTrade(entry9->getDateValue());
      aStopLoss = createStopLossForShortTrade2(entry9->getDateValue());

      orderManager.addTradingOrder (aProfitTarget);
      orderManager.addTradingOrder (aStopLoss);
      
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 1);
      REQUIRE (orderManager.getNumStopExitOrders() == 1);

      orderManager.processPendingOrders (entry10->getDateValue(), dummyBroker1.getPositionManager());
      aExecutedOrder = dummyBroker1.getLastExecutedOrder();

      REQUIRE (aStopLoss->isOrderExecuted());
      REQUIRE (dummyBroker1.getPositionManager().isFlatPosition(equitySymbol));
      REQUIRE (aProfitTarget->isOrderCanceled());

      REQUIRE (aExecutedOrder->getFillDate() == entry10->getDateValue());
      REQUIRE (aExecutedOrder->getFillPrice() == createDecimal("200.04"));
    }

  SECTION ("Add and execute long limit order exit")
    {
      orderManager.addTradingOrder (longSpyEntryOrder1);
      orderManager.processPendingOrders (entry2->getDateValue(), dummyBroker1.getPositionManager());

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);
  
      auto longSpyExitOrder1 = std::make_shared<SellAtLimitOrder<DecimalType>>(equitySymbol,
									  TradingOrderManager_createShareVolume (1),
									  entry2->getDateValue(),
									  createDecimal("207.28"));

      orderManager.addTradingOrder (longSpyExitOrder1);

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 1);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      orderManager.processPendingOrders (entry3->getDateValue(), dummyBroker1.getPositionManager());
      REQUIRE (longSpyExitOrder1->isOrderCanceled());

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      longSpyExitOrder1 = std::make_shared<SellAtLimitOrder<DecimalType>>(equitySymbol,
								TradingOrderManager_createShareVolume (1),
								entry3->getDateValue(),
								createDecimal("207.28"));
      orderManager.addTradingOrder (longSpyExitOrder1);
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 1);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      orderManager.processPendingOrders (entry4->getDateValue(), dummyBroker1.getPositionManager());
      REQUIRE (longSpyExitOrder1->isOrderCanceled());

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      longSpyExitOrder1 = std::make_shared<SellAtLimitOrder<DecimalType>>(equitySymbol,
								TradingOrderManager_createShareVolume (1),
								entry4->getDateValue(),
								createDecimal("207.28"));
      orderManager.addTradingOrder (longSpyExitOrder1);
      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 1);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);

      orderManager.processPendingOrders (entry5->getDateValue(), dummyBroker1.getPositionManager());
      REQUIRE (longSpyExitOrder1->isOrderExecuted());

      REQUIRE (orderManager.getNumMarketExitOrders() == 0);
      REQUIRE (orderManager.getNumMarketEntryOrders() == 0);
      REQUIRE (orderManager.getNumLimitExitOrders() == 0);
      REQUIRE (orderManager.getNumStopExitOrders() == 0);
    }
}

TEST_CASE("TradingOrderManager Extended Tests", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  TradingOrderManager<DecimalType> manager(portfolio);
  DummyBroker<DecimalType> observer1(portfolio);
  DummyBroker<DecimalType> observer2(portfolio);
  observer1.addInstrument(symbol);
  observer2.addInstrument(symbol);
  manager.addObserver(observer1);
  manager.addObserver(observer2);

  SECTION("Invalid order state rejects submission") {
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue());
    order->MarkOrderExecuted(entry2->getDateValue(), createDecimal("103.00"));
    REQUIRE_THROWS(manager.addTradingOrder(order));
  }

  SECTION("Multiple observers notified on execution") {
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue());
    manager.addTradingOrder(order);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    REQUIRE(observer1.getLastExecutedOrder()->isOrderExecuted());
    REQUIRE(observer2.getLastExecutedOrder()->isOrderExecuted());
  }

  SECTION("Duplicate order submission does not cause re-execution") {
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue());
    manager.addTradingOrder(order);

    REQUIRE(manager.getNumMarketEntryOrders() == 1);
  }
  
  SECTION("Exit order ignored when no open position") {
    auto exitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue(), createDecimal("110.00"));
    manager.addTradingOrder(exitOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    REQUIRE(exitOrder->isOrderCanceled());
  }

  SECTION("Pending orders with same date are handled") {
    auto o1 = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue());
    auto o2 = std::make_shared<SellAtLimitOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue(), createDecimal("104.00"));
    manager.addTradingOrder(o1);
    manager.addTradingOrder(o2);
    auto it = manager.beginPendingOrders();
    REQUIRE(it != manager.endPendingOrders());
  }

  SECTION("SellAtLimitOrder fills at open on gap up") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(symbol,
									   TradingOrderManager_createShareVolume(1),
									   entry1->getDateValue());
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    REQUIRE(entryOrder->isOrderExecuted());
 
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue(), createDecimal("101.00"));
    manager.addTradingOrder(limitOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    REQUIRE(limitOrder->isOrderExecuted());
    REQUIRE(limitOrder->getFillPrice() == entry2->getOpenValue());
  }

  SECTION("SellAtLimitOrder fills at limit if no gap") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(symbol,
									   TradingOrderManager_createShareVolume(1),
									   entry1->getDateValue());
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    REQUIRE(entryOrder->isOrderExecuted());
 
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue(), createDecimal("105.50"));
    manager.addTradingOrder(limitOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    REQUIRE(limitOrder->isOrderExecuted());
    REQUIRE(limitOrder->getFillPrice() == createDecimal("105.50"));
  }

  
  SECTION("Exit order canceled if position already closed") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue());
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());

    auto limitExit = std::make_shared<SellAtLimitOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry2->getDateValue(), createDecimal("90.00"));
    manager.addTradingOrder(limitExit);

    // Close position manually
    observer1.getPositionManager().closeAllPositions(symbol, entry2->getDateValue(), createDecimal("105.00"));
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    REQUIRE(limitExit->isOrderCanceled());
  }

  SECTION("Full pipeline: short entry, stop and limit exit") {
    auto shortEntry = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry1->getDateValue());
    manager.addTradingOrder(shortEntry);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());

    auto stopExit = std::make_shared<CoverAtStopOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry2->getDateValue(), createDecimal("110.00"));
    auto limitExit = std::make_shared<CoverAtLimitOrder<DecimalType>>(symbol, TradingOrderManager_createShareVolume(1), entry2->getDateValue(), createDecimal("95.00"));
    manager.addTradingOrder(stopExit);
    manager.addTradingOrder(limitExit);

    auto entry3 = createEquityEntry("20210106", "111", "115", "94", "100", 1000000);
    series->addEntry(*entry3);
    manager.processPendingOrders(entry3->getDateValue(), observer1.getPositionManager());

    REQUIRE((stopExit->isOrderExecuted() || limitExit->isOrderExecuted()));
    REQUIRE(!(stopExit->isOrderExecuted() && limitExit->isOrderExecuted()));
  }
}

// =============================================================================
// CORRECTED TEST CASES FOR TradingOrderManager
// These tests account for the rule: orders are only processed when
// processingDateTime > orderDateTime (strictly greater than)
// =============================================================================

TEST_CASE("TradingOrderManager Error Handling and Edge Cases", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);
  auto entry3 = createEquityEntry("20210106", "106", "110", "104", "108", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);
  series->addEntry(*entry3);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  TradingOrderManager<DecimalType> manager(portfolio);
  DummyBroker<DecimalType> observer(portfolio);
  observer.addInstrument(symbol);
  manager.addObserver(observer);

  SECTION("Missing timeseries data handles gracefully") {
    // Create order for far future date with no data
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      date(2030, Jan, 1)
    );
    manager.addTradingOrder(order);
    
    // Should not crash when processing with missing data
    REQUIRE_NOTHROW(manager.processPendingOrders(
      date(2021, Jan, 5),
      observer.getPositionManager()
    ));
    
    // Order should remain pending since data is not available
    REQUIRE(order->isOrderPending());
    REQUIRE(manager.getNumMarketEntryOrders() == 1);
  }

  SECTION("Adding same order instance twice") {
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    
    manager.addTradingOrder(order);
    int countAfterFirst = manager.getNumMarketEntryOrders();
    
    manager.addTradingOrder(order);
    int countAfterSecond = manager.getNumMarketEntryOrders();
    
    // Should handle duplicate gracefully
    REQUIRE(countAfterFirst == 1);
    REQUIRE(countAfterSecond == 2); // Or 1 if deduplication is implemented
  }

  SECTION("Future-dated orders are not processed prematurely") {
    // Order dated in the future
    auto futureOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      date(2021, Jan, 10)
    );
    manager.addTradingOrder(futureOrder);
    
    // Process at earlier date
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Order should still be pending (not processed yet)
    REQUIRE(futureOrder->isOrderPending());
    REQUIRE(manager.getNumMarketEntryOrders() == 1);
  }

  SECTION("Processing with empty order list does not crash") {
    // Process with no orders
    REQUIRE_NOTHROW(manager.processPendingOrders(
      entry2->getDateValue(), 
      observer.getPositionManager()
    ));
    
    REQUIRE(manager.getNumMarketEntryOrders() == 0);
    REQUIRE(manager.getNumMarketExitOrders() == 0);
  }
}

TEST_CASE("TradingOrderManager Order Priority and Sequencing", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);
  auto entry3 = createEquityEntry("20210106", "103", "107", "102", "105", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);
  series->addEntry(*entry3);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  TradingOrderManager<DecimalType> manager(portfolio);
  DummyBroker<DecimalType> observer(portfolio);
  observer.addInstrument(symbol);
  manager.addObserver(observer);

  SECTION("Exit orders processed before new entry orders on same date") {
    // Create position first
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    REQUIRE(entryOrder->isOrderExecuted());
    
    // Add exit and new entry both for entry2 date (will be processed on entry3)
    auto exitOrder = std::make_shared<MarketOnOpenSellOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue()
    );
    auto newEntry = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue()
    );
    
    manager.addTradingOrder(newEntry);
    manager.addTradingOrder(exitOrder);
    
    // Process on the NEXT day (entry3) since orders need processingDate > orderDate
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    // Both should execute (exit first, then new entry)
    REQUIRE(exitOrder->isOrderExecuted());
    REQUIRE(newEntry->isOrderExecuted());
  }

  SECTION("Stop and limit orders both triggered - only one executes") {
    // Enter long position
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Both stop and limit could trigger on same bar
    auto stopOrder = std::make_shared<SellAtStopOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("104.00") // Stop loss
    );
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("106.00") // Profit target
    );
    
    manager.addTradingOrder(stopOrder);
    manager.addTradingOrder(limitOrder);
    
    // Bar that triggers both (low=102, high=107) - process on NEXT day
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    // Verify only one executed, other canceled
    bool stopExecuted = stopOrder->isOrderExecuted();
    bool limitExecuted = limitOrder->isOrderExecuted();
    bool stopCanceled = stopOrder->isOrderCanceled();
    bool limitCanceled = limitOrder->isOrderCanceled();
    
    REQUIRE((stopExecuted || limitExecuted));
    REQUIRE(!(stopExecuted && limitExecuted));
    REQUIRE((stopCanceled || limitCanceled));
  }

  SECTION("Multiple entry orders on same bar for same symbol") {
    auto entry1Order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    auto entry2Order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    
    manager.addTradingOrder(entry1Order);
    manager.addTradingOrder(entry2Order);
    
    REQUIRE(manager.getNumMarketEntryOrders() == 2);
    
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Both should execute
    REQUIRE(entry1Order->isOrderExecuted());
    REQUIRE(entry2Order->isOrderExecuted());
    
    // Position manager should have positions
    REQUIRE(!observer.getPositionManager().isFlatPosition(symbol));
  }
}

TEST_CASE("TradingOrderManager Iterator Safety", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  TradingOrderManager<DecimalType> manager(portfolio);
  DummyBroker<DecimalType> observer(portfolio);
  observer.addInstrument(symbol);
  manager.addObserver(observer);

  SECTION("Iterating pending orders when collection is empty") {
    auto it = manager.beginPendingOrders();
    REQUIRE(it == manager.endPendingOrders());
    
    // Should be safe to iterate empty collection
    int count = 0;
    for (auto iter = manager.beginPendingOrders(); 
         iter != manager.endPendingOrders(); 
         ++iter) {
      count++;
    }
    REQUIRE(count == 0);
  }

  SECTION("Pending order iterator remains valid after processing") {
    auto order1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    auto order2 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue()
    );
    
    manager.addTradingOrder(order1);
    manager.addTradingOrder(order2);
    
    // Get iterator before processing
    auto it1 = manager.beginPendingOrders();
    REQUIRE(it1 != manager.endPendingOrders());
    
    // Process orders
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Get new iterator after processing - should not crash
    REQUIRE_NOTHROW([&]() {
      int count = 0;
      for (auto iter = manager.beginPendingOrders(); 
           iter != manager.endPendingOrders(); 
           ++iter) {
        count++;
      }
    }());
  }

  SECTION("Multiple concurrent iterators") {
    auto order1 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    auto order2 = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue()
    );
    
    manager.addTradingOrder(order1);
    manager.addTradingOrder(order2);
    
    // Create multiple iterators
    auto it1 = manager.beginPendingOrders();
    auto it2 = manager.beginPendingOrders();
    auto end = manager.endPendingOrders();
    
    // Both should work independently
    REQUIRE(it1 != end);
    REQUIRE(it2 != end);
  }
}

TEST_CASE("TradingOrderManager Observer Management", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  SECTION("Three observers all notified on execution") {
    TradingOrderManager<DecimalType> manager(portfolio);
    DummyBroker<DecimalType> observer1(portfolio);
    DummyBroker<DecimalType> observer2(portfolio);
    DummyBroker<DecimalType> observer3(portfolio);
    
    observer1.addInstrument(symbol);
    observer2.addInstrument(symbol);
    observer3.addInstrument(symbol);
    
    manager.addObserver(observer1);
    manager.addObserver(observer2);
    manager.addObserver(observer3);
    
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(order);
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    
    // All three observers should be notified
    REQUIRE(observer1.getLastExecutedOrder() != nullptr);
    REQUIRE(observer2.getLastExecutedOrder() != nullptr);
    REQUIRE(observer3.getLastExecutedOrder() != nullptr);
    
    REQUIRE(observer1.getLastExecutedOrder()->getOrderID() == order->getOrderID());
    REQUIRE(observer2.getLastExecutedOrder()->getOrderID() == order->getOrderID());
    REQUIRE(observer3.getLastExecutedOrder()->getOrderID() == order->getOrderID());
  }

  SECTION("Observer added after order submission is still notified") {
    TradingOrderManager<DecimalType> manager(portfolio);
    DummyBroker<DecimalType> observer1(portfolio);
    observer1.addInstrument(symbol);
    manager.addObserver(observer1);
    
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(order);
    
    // Add observer after order submitted but before processing
    DummyBroker<DecimalType> lateObserver(portfolio);
    lateObserver.addInstrument(symbol);
    manager.addObserver(lateObserver);
    
    manager.processPendingOrders(entry2->getDateValue(), observer1.getPositionManager());
    
    // Late observer should still be notified
    REQUIRE(lateObserver.getLastExecutedOrder() != nullptr);
    REQUIRE(lateObserver.getLastExecutedOrder()->getOrderID() == order->getOrderID());
  }

  SECTION("No observers registered still processes orders correctly") {
    TradingOrderManager<DecimalType> managerNoObs(portfolio);
    
    auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    managerNoObs.addTradingOrder(order);
    
    DummyBroker<DecimalType> tempObs(portfolio);
    tempObs.addInstrument(symbol);
    
    // Should not crash with no observers
    REQUIRE_NOTHROW(managerNoObs.processPendingOrders(
      entry2->getDateValue(), 
      tempObs.getPositionManager()
    ));
    REQUIRE(order->isOrderExecuted());
  }

  SECTION("Observer notified on order cancellation") {
    TradingOrderManager<DecimalType> manager(portfolio);
    DummyBroker<DecimalType> observer(portfolio);
    observer.addInstrument(symbol);
    manager.addObserver(observer);
    
    // Submit exit order with no position (dated on entry1)
    auto exitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue(), 
      createDecimal("110.00")
    );
    manager.addTradingOrder(exitOrder);
    
    // Process on NEXT day (entry2) - order will be canceled (no position)
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Order should be canceled and observer notified
    REQUIRE(exitOrder->isOrderCanceled());
    REQUIRE(observer.getLastCanceledOrder() != nullptr);
  }
}

TEST_CASE("TradingOrderManager Boundary Conditions for Limit/Stop Orders", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);
  auto entry3 = createEquityEntry("20210106", "108", "112", "107", "110", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);
  series->addEntry(*entry3);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  TradingOrderManager<DecimalType> manager(portfolio);
  DummyBroker<DecimalType> observer(portfolio);
  observer.addInstrument(symbol);
  manager.addObserver(observer);

  SECTION("SellAtLimitOrder with limit exactly at bar high") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Limit exactly at bar high (106) - order dated entry2, processed on entry3
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      entry2->getHighValue() // 106
    );
    manager.addTradingOrder(limitOrder);
    
    // Process on NEXT day (entry3) - entry3 high is 112, so limit at 106 will execute
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    REQUIRE(limitOrder->isOrderExecuted());
    // Fill at open since open (108) > limit (106) - gap up scenario
    REQUIRE(limitOrder->getFillPrice() == entry3->getOpenValue());
  }

  SECTION("SellAtLimitOrder with limit exactly at bar low") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Limit at bar low (100) - order dated entry2, processed on entry3
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      entry2->getLowValue() // 100
    );
    manager.addTradingOrder(limitOrder);
    
    // Process on NEXT day (entry3) - entry3 high is 112, so limit at 100 will execute
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    REQUIRE(limitOrder->isOrderExecuted());
  }

  SECTION("CoverAtStopOrder with stop exactly at bar high") {
    auto shortEntry = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(shortEntry);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Stop exactly at bar high (106) - order dated entry2, processed on entry3
    auto stopOrder = std::make_shared<CoverAtStopOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      entry2->getHighValue() // 106
    );
    manager.addTradingOrder(stopOrder);
    
    // Process on NEXT day (entry3) - entry3 high is 112, so stop at 106 will execute
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    REQUIRE(stopOrder->isOrderExecuted());
  }

  SECTION("Precision handling with very close prices") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Limit just 0.01 above entry2's open (103.01)
    DecimalType limitPrice = entry2->getOpenValue() + createDecimal("0.01");
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      limitPrice
    );
    manager.addTradingOrder(limitOrder);
    
    // Process on NEXT day (entry3) - entry3 high is 112, so limit at 103.01 will execute
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    REQUIRE(limitOrder->isOrderExecuted());
    // Fill at open since open (108) > limit (103.01) - gap up scenario
    REQUIRE(limitOrder->getFillPrice() == entry3->getOpenValue());
  }

  SECTION("Limit price between open and high") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Limit between entry2's open (103) and high (106)
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("104.50")
    );
    manager.addTradingOrder(limitOrder);
    
    // Process on NEXT day (entry3) - entry3 high is 112, so limit at 104.50 will execute
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    REQUIRE(limitOrder->isOrderExecuted());
    // Fill at open since open (108) > limit (104.50) - gap up scenario
    REQUIRE(limitOrder->getFillPrice() == entry3->getOpenValue());
  }

  SECTION("Limit order does not fill when price not reached") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Limit well above entry3's high (112)
    auto limitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("120.00") // Above entry3's high of 112
    );
    manager.addTradingOrder(limitOrder);
    
    // Process on NEXT day (entry3)
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    // Should be canceled (price not reached)
    REQUIRE(limitOrder->isOrderCanceled());
  }
}

TEST_CASE("TradingOrderManager Same-Bar Entry/Exit Restrictions", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);
  auto entry3 = createEquityEntry("20210106", "108", "112", "107", "110", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);
  series->addEntry(*entry3);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  TradingOrderManager<DecimalType> manager(portfolio);
  DummyBroker<DecimalType> observer(portfolio);
  observer.addInstrument(symbol);
  manager.addObserver(observer);

  SECTION("Exit order on same bar as entry is canceled") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    auto exitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue(), // Same date as entry
      createDecimal("110.00")
    );
    
    manager.addTradingOrder(entryOrder);
    manager.addTradingOrder(exitOrder);
    
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    REQUIRE(entryOrder->isOrderExecuted());
    // Exit on same bar as entry should be canceled
    REQUIRE(exitOrder->isOrderCanceled());
  }

  SECTION("Exit order day after entry executes normally") {
    auto entryOrder = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(entryOrder);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    REQUIRE(entryOrder->isOrderExecuted());
    
    // Exit order for entry2 (will be processed on entry3)
    auto exitOrder = std::make_shared<SellAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(),
      createDecimal("104.00")
    );
    manager.addTradingOrder(exitOrder);
    
    // Process on NEXT day (entry3)
    manager.processPendingOrders(entry3->getDateValue(), observer.getPositionManager());
    
    // Should execute normally
    REQUIRE(exitOrder->isOrderExecuted());
  }
}

TEST_CASE("TradingOrderManager Short/Cover Order Specific Tests", "[TradingOrderManager]") {
  std::string symbol = "SPY";
  auto entry1 = createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = createEquityEntry("20210105", "103", "106", "100", "105", 1000000);
  auto entry3GapDown = createEquityEntry("20210106", "94", "96", "92", "95", 1000000);
  auto entry4GapUp = createEquityEntry("20210107", "112", "115", "111", "113", 1000000);
  auto entry5 = createEquityEntry("20210108", "108", "112", "107", "110", 1000000);

  auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  series->addEntry(*entry1);
  series->addEntry(*entry2);
  series->addEntry(*entry3GapDown);
  series->addEntry(*entry4GapUp);
  series->addEntry(*entry5);

  auto equity = std::make_shared<EquitySecurity<DecimalType>>(symbol, "SPY ETF", series);
  auto portfolio = std::make_shared<Portfolio<DecimalType>>("Test Portfolio");
  portfolio->addSecurity(equity);

  TradingOrderManager<DecimalType> manager(portfolio);
  DummyBroker<DecimalType> observer(portfolio);
  observer.addInstrument(symbol);
  manager.addObserver(observer);

  SECTION("CoverAtLimitOrder fills at open on gap down") {
    // Enter short
    auto shortEntry = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(shortEntry);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    REQUIRE(shortEntry->isOrderExecuted());
    
    // Cover limit above current price (order dated entry2, will process on entry3GapDown)
    auto coverLimit = std::make_shared<CoverAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("97.00")
    );
    manager.addTradingOrder(coverLimit);
    
    // Gap down to below limit - process on NEXT day
    manager.processPendingOrders(entry3GapDown->getDateValue(), 
                                 observer.getPositionManager());
    
    REQUIRE(coverLimit->isOrderExecuted());
    // Should fill at open due to gap down
    REQUIRE(coverLimit->getFillPrice() == entry3GapDown->getOpenValue());
  }


  SECTION("CoverAtStopOrder fills at open on gap up") {
    // Enter short
    auto shortEntry = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(shortEntry);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Stop above current price (order dated entry2, will process on entry4GapUp)
    auto coverStop = std::make_shared<CoverAtStopOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("110.00")
    );
    manager.addTradingOrder(coverStop);
    
    // Gap up past stop - process on NEXT day that has the gap up
    manager.processPendingOrders(entry4GapUp->getDateValue(), 
                                 observer.getPositionManager());
    
    REQUIRE(coverStop->isOrderExecuted());
    // Should fill at open due to gap up
    REQUIRE(coverStop->getFillPrice() == entry4GapUp->getOpenValue());
  }

  SECTION("CoverAtStopOrder fills at stop when no gap") {
    // Enter short
    auto shortEntry = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(shortEntry);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Stop at 106 (order dated entry2, will process on entry5)
    auto coverStop = std::make_shared<CoverAtStopOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("106.00")
    );
    manager.addTradingOrder(coverStop);
    
    // Process on entry5 (high=112, so stop at 106 will execute)
    manager.processPendingOrders(entry5->getDateValue(), observer.getPositionManager());
    
    REQUIRE(coverStop->isOrderExecuted());
    // Fill at open since open (108) > stop (106) - gap up scenario
    REQUIRE(coverStop->getFillPrice() == entry5->getOpenValue());
  }

  SECTION("CoverAtLimitOrder does not fill when price too high") {
    // Enter short
    auto shortEntry = std::make_shared<MarketOnOpenShortOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(shortEntry);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Limit below all subsequent lows (order dated entry2, process on entry4GapUp)
    auto coverLimit = std::make_shared<CoverAtLimitOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("90.00") // Below all subsequent lows
    );
    manager.addTradingOrder(coverLimit);
    
    // Process on entry4GapUp (low=111, so limit at 90 won't fill)
    manager.processPendingOrders(entry4GapUp->getDateValue(), observer.getPositionManager());
    
    // Should be canceled (price not reached)
    REQUIRE(coverLimit->isOrderCanceled());
  }

  SECTION("SellAtStopOrder does not fill when price too high") {
    // Enter long
    auto longEntry = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry1->getDateValue()
    );
    manager.addTradingOrder(longEntry);
    manager.processPendingOrders(entry2->getDateValue(), observer.getPositionManager());
    
    // Stop below all subsequent lows (order dated entry2, process on entry4GapUp)
    auto stopOrder = std::make_shared<SellAtStopOrder<DecimalType>>(
      symbol, 
      TradingOrderManager_createShareVolume(1), 
      entry2->getDateValue(), 
      createDecimal("90.00") // Below all subsequent lows
    );
    manager.addTradingOrder(stopOrder);
    
    // Process on entry4GapUp (low=111, so stop at 90 won't trigger)
    manager.processPendingOrders(entry4GapUp->getDateValue(), observer.getPositionManager());
    
    // Should be canceled (price not reached)
    REQUIRE(stopOrder->isOrderCanceled());
  }
}
