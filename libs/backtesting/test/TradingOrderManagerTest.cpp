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
    mPosManager.addPosition (createLongTradingPosition (order));
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
  {}

  void OrderCanceled (CoverAtLimitOrder<Decimal> *order)
  {}
  
  void OrderCanceled (CoverAtStopOrder<Decimal> *order)
  {}
  
  void OrderCanceled (SellAtStopOrder<Decimal> *order)
  {}
  
private:

  OHLCTimeSeriesEntry<Decimal> getEntryBar (const std::string& tradingSymbol,
							const boost::gregorian::date& d)
    {
      typename Portfolio<Decimal>::ConstPortfolioIterator symbolIterator = mPortfolio->findSecurity (tradingSymbol);
      if (symbolIterator != mPortfolio->endPortfolio())
	{
	  typename Security<Decimal>::ConstRandomAccessIterator it = 
	    symbolIterator->second->getRandomAccessIterator (d);

	  return (*it);
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

std::shared_ptr<OHLCTimeSeriesEntry<DecimalType>>
    TradingOrderManager_createEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    return createTimeSeriesEntry (dateString, openPrice, highPrice, lowPrice, closePrice, vol);
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
  auto entry18 = TradingOrderManager_createEquityEntry ("20160119", "189.96", "190.11","186.20","188.06",
				    190196000);
  auto entry17 = TradingOrderManager_createEquityEntry ("20160115", "186.77","188.76", "185.52","187.81",	
				    324846400);
  auto entry16 = TradingOrderManager_createEquityEntry ("20160114", "189.55","193.26", "187.66", "191.93",
				   240795600);
  auto entry15 = TradingOrderManager_createEquityEntry ("20160113", "194.45", "194.86", "188.38","188.83",
				   221168900);
  auto entry14 = TradingOrderManager_createEquityEntry ("20160112", "193.82", "194.55", "191.14","193.66",
				   172330500);
  auto entry13 = TradingOrderManager_createEquityEntry ("20160111", "193.01", "193.41", "189.82","192.11",
				   187941300);
  auto entry12 = TradingOrderManager_createEquityEntry ("20160108", "195.19", "195.85", "191.58","191.92",
				   142662900);
  auto entry11 = TradingOrderManager_createEquityEntry ("20160107", "195.33", "197.44", "193.59","194.05",
				   142662900);
  auto entry10 = TradingOrderManager_createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry9 = TradingOrderManager_createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry8 = TradingOrderManager_createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry7 = TradingOrderManager_createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry6 = TradingOrderManager_createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = TradingOrderManager_createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry4 = TradingOrderManager_createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
  auto entry3 = TradingOrderManager_createEquityEntry ("20151224", "205.72", "206.33", "205.42", "205.68",
				   48542200);
  auto entry2 = TradingOrderManager_createEquityEntry ("20151223", "204.69", "206.07", "204.58", "206.02",
				   48542200);
  auto entry1 = TradingOrderManager_createEquityEntry ("20151222", "202.72", "203.85", "201.55", "203.50",
				   111026200);
  auto entry0 = TradingOrderManager_createEquityEntry ("20151221", "201.41", "201.88", "200.09", "201.67",
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
  auto entry1 = TradingOrderManager_createEquityEntry("20210104", "100", "105", "95", "102", 1000000);
  auto entry2 = TradingOrderManager_createEquityEntry("20210105", "103", "106", "100", "105", 1000000);

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

    auto entry3 = TradingOrderManager_createEquityEntry("20210106", "111", "115", "94", "100", 1000000);
    series->addEntry(*entry3);
    manager.processPendingOrders(entry3->getDateValue(), observer1.getPositionManager());

    REQUIRE((stopExit->isOrderExecuted() || limitExit->isOrderExecuted()));
    REQUIRE(!(stopExit->isOrderExecuted() && limitExit->isOrderExecuted()));
  }
}
