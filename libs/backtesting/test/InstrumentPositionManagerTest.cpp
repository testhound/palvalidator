#include <catch2/catch_test_macros.hpp>
#include "InstrumentPositionManager.h"
#include "DecimalConstants.h"
#include "TestUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::posix_time::ptime;
using boost::posix_time::time_from_string;
using namespace mkc_timeseries;
using namespace boost::gregorian;


TEST_CASE ("TradingPosition operations", "[TradingManagerPosition]")
{
  auto entry0 = createTimeSeriesEntry ("19851118", "3664.51025", "3687.58178", "3656.81982","3672.20068",0);

  auto entry1 = createTimeSeriesEntry ("19851119", "3710.65307617188","3722.18872070313","3679.89135742188",
				       "3714.49829101563", 0);

  auto entry2 = createTimeSeriesEntry ("19851120", "3737.56982421875","3756.7958984375","3726.0341796875",
				       "3729.87939453125",0);

  auto entry3 = createTimeSeriesEntry ("19851121","3699.11743164063","3710.65307617188","3668.35546875",
				       "3683.73657226563",0);

  auto entry4 = createTimeSeriesEntry ("19851122","3664.43017578125","3668.23559570313","3653.0146484375",
				       "3656.81982421875", 0);

  auto entry5 = createTimeSeriesEntry ("19851125","3641.59887695313","3649.20947265625","3626.3779296875",
				       "3637.79370117188", 0);

  auto entry6 = createTimeSeriesEntry ("19851126","3656.81982421875","3675.84594726563","3653.0146484375",
				       "3660.625", 0);
  auto entry7 = createTimeSeriesEntry ("19851127", "3664.43017578125","3698.67724609375","3660.625",
				       "3691.06689453125", 0);
  auto entry8 = createTimeSeriesEntry ("19851129", "3717.70336914063","3729.119140625","3698.67724609375",
				       "3710.09301757813", 0);
  auto entry9 = createTimeSeriesEntry ("19851202", "3721.50854492188","3725.31372070313","3691.06689453125",
				       "3725.31372070313", 0);
  auto entry10 = createTimeSeriesEntry ("19851203", "3713.89819335938","3740.53466796875","3710.09301757813"
					,"3736.7294921875", 0);
  auto entry11 = createTimeSeriesEntry ("19851204","3744.33984375","3759.56079101563","3736.7294921875",
					"3740.53466796875",0);

  TradingVolume oneContract(1, TradingVolume::CONTRACTS);

  std::string tickerSymbol("C2");
  InstrumentPosition<DecimalType> c2InstrumentPositionLong (tickerSymbol);

  auto longPosition1 = std::make_shared<TradingPositionLong<DecimalType>>(tickerSymbol, entry0->getOpenValue(),
								*entry0, oneContract);
  auto longPosition2 = std::make_shared<TradingPositionLong<DecimalType>>(tickerSymbol, entry4->getOpenValue(),
								*entry4, oneContract);


  // Time Series for short positions

  auto shortEntry0 = createTimeSeriesEntry ("20160211","95.46","97.32",
					    "95.19","96.55", 0);
  auto shortEntry1 = createTimeSeriesEntry ("20160210","97.50","98.69","96.62",
					    "96.69", 0);
  auto shortEntry2 = createTimeSeriesEntry ("20160209","95.33","97.78",
					    "95.18","96.32", 0);
  auto shortEntry3 = createTimeSeriesEntry ("20160208","96.29","97.05",
					    "94.84","96.62", 0);
  auto shortEntry4 = createTimeSeriesEntry ("20160205","101.29","101.33",
					    "97.72","98.12", 0);
  auto shortEntry5 = createTimeSeriesEntry ("20160204","101.39","102.46",
					    "100.44","101.65", 0);
  auto shortEntry6 = createTimeSeriesEntry ("20160203","102.83","102.83",
					    "99.88","101.66", 0);

  std::string qqqSymbol("QQQ");
  TradingVolume oneShare(1, TradingVolume::SHARES);
  auto shortPosition1 = std::make_shared<TradingPositionShort<DecimalType>>(qqqSymbol,
								 shortEntry6->getOpenValue(),
								 *shortEntry6,
								 oneShare);
  auto shortPosition2 = std::make_shared<TradingPositionShort<DecimalType>>(qqqSymbol,
								 shortEntry4->getOpenValue(),
								 *shortEntry4,
								 oneShare);


  InstrumentPositionManager<DecimalType> posManager;
  REQUIRE (posManager.getNumInstruments() == 0);
  posManager.addInstrument(tickerSymbol);
  REQUIRE (posManager.getNumInstruments() == 1);
  posManager.addInstrument(qqqSymbol);
  REQUIRE (posManager.getNumInstruments() == 2);

  REQUIRE_FALSE (posManager.isLongPosition(tickerSymbol));
  REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
  REQUIRE (posManager.isFlatPosition(tickerSymbol));
  REQUIRE (posManager.getNumPositionUnits (tickerSymbol) == 0);

  REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
  REQUIRE_FALSE (posManager.isShortPosition(qqqSymbol));
  REQUIRE (posManager.isFlatPosition(qqqSymbol));
  REQUIRE (posManager.getNumPositionUnits (qqqSymbol) == 0);

  posManager.addPosition (shortPosition1);
  REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
  REQUIRE (posManager.isShortPosition(qqqSymbol));
  REQUIRE_FALSE (posManager.isFlatPosition(qqqSymbol));
  REQUIRE (posManager.getNumPositionUnits (qqqSymbol) == 1);

  posManager.addBar (qqqSymbol, *shortEntry5);
  posManager.addBar (qqqSymbol, *shortEntry4);
  posManager.addPosition (shortPosition2);
  REQUIRE (posManager.getNumPositionUnits (qqqSymbol) == 2);
  posManager.addBar (qqqSymbol, *shortEntry3);
  posManager.addBar (qqqSymbol, *shortEntry2);
  posManager.addBar (qqqSymbol, *shortEntry1);
  posManager.addBar (qqqSymbol, *shortEntry0);

  posManager.addPosition (longPosition1);
  REQUIRE (posManager.isLongPosition(tickerSymbol));
  REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
  REQUIRE_FALSE (posManager.isFlatPosition(tickerSymbol));
  REQUIRE (posManager.getNumPositionUnits (tickerSymbol) == 1);

  posManager.addBar(tickerSymbol, *entry1);
  posManager.addBar(tickerSymbol, *entry2);
  posManager.addBar(tickerSymbol, *entry3);
  posManager.addBar(tickerSymbol, *entry4);
  posManager.addPosition (longPosition2);
  REQUIRE (posManager.getNumPositionUnits (tickerSymbol) == 2);
  posManager.addBar(tickerSymbol, *entry5);
  posManager.addBar(tickerSymbol, *entry6);
  posManager.addBar(tickerSymbol, *entry7);
  posManager.addBar(tickerSymbol, *entry8);
  posManager.addBar(tickerSymbol, *entry9);
  posManager.addBar(tickerSymbol, *entry10);
  posManager.addBar(tickerSymbol, *entry11);

  SECTION ("Test InstrumentPosition iterators")
  {
    InstrumentPositionManager<DecimalType>::ConstInstrumentPositionIterator it =
      posManager.beginInstrumentPositions();

    REQUIRE (it->second->getInstrumentSymbol() == tickerSymbol);
    it++;
    REQUIRE (it->second->getInstrumentSymbol() == qqqSymbol);
    it++;
    REQUIRE (it == posManager.endInstrumentPositions());

  }

  SECTION ("Test getInstrumentPosition")
    {
    InstrumentPosition<DecimalType> qqqInstrument = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument.getInstrumentSymbol() == qqqSymbol);
    REQUIRE (qqqInstrument.getNumPositionUnits() == 2);
    REQUIRE (qqqInstrument.getFillPrice() == shortEntry6->getOpenValue());
    REQUIRE (qqqInstrument.getFillPrice(1) == shortEntry6->getOpenValue());
    REQUIRE (qqqInstrument.getFillPrice(2) == shortEntry4->getOpenValue());
  }

  SECTION ("Test addBarForOpenPosition")
    {
      auto aSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
      aSeries->addEntry(*entry0);
      aSeries->addEntry(*entry1);
      aSeries->addEntry(*entry2);
      aSeries->addEntry(*entry3);
      aSeries->addEntry(*entry4);
      aSeries->addEntry(*entry5);
      aSeries->addEntry(*entry6);
      aSeries->addEntry(*entry7);
      aSeries->addEntry(*entry8);
      aSeries->addEntry(*entry9);
      aSeries->addEntry(*entry10);
      aSeries->addEntry(*entry11);

      std::string futuresSymbol("C2");
      std::string futuresName("Corn futures");
      DecimalType cornBigPointValue(createDecimal("50.0"));
      DecimalType cornTickValue(createDecimal("0.25"));
      auto corn = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol,
						       futuresName,
						       cornBigPointValue,
						       cornTickValue,
						       aSeries);

      std::string portName("Test Portfolio");
      auto aPortfolio = std::make_shared<Portfolio<DecimalType>>(portName);

      aPortfolio->addSecurity (corn);
      InstrumentPositionManager<DecimalType> aPosManager;
      aPosManager.addInstrument(futuresSymbol);

      auto longPositionCorn = std::make_shared<TradingPositionLong<DecimalType>>(futuresSymbol, entry0->getOpenValue(),
								*entry0, oneContract);

      REQUIRE (aPosManager.getNumPositionUnits (futuresSymbol) == 0);
      aPosManager.addPosition (longPositionCorn);

      REQUIRE (aPosManager.isLongPosition(futuresSymbol));
      REQUIRE_FALSE (aPosManager.isShortPosition(futuresSymbol));
      REQUIRE_FALSE (aPosManager.isFlatPosition(futuresSymbol));
      REQUIRE (aPosManager.getNumPositionUnits (futuresSymbol) == 1);

      aPosManager.addBarForOpenPosition (createDate("19851119"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851120"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851121"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851122"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851125"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851126"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851127"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851129"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851202"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851203"), aPortfolio.get());
      aPosManager.addBarForOpenPosition (createDate("19851204"), aPortfolio.get());

      auto cornPos = aPosManager.getTradingPosition (futuresSymbol, 1);
      REQUIRE (cornPos->getNumBarsInPosition() == 12);
      REQUIRE (cornPos->getNumBarsSinceEntry() == 11);
    }

  SECTION ("Test closeUnitPosition")
  {
    InstrumentPosition<DecimalType> qqqInstrument = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument.getNumPositionUnits() == 2);
    REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
    REQUIRE (posManager.isShortPosition(qqqSymbol));
    REQUIRE_FALSE (posManager.isFlatPosition(qqqSymbol));

    REQUIRE (shortPosition1->isPositionOpen());
    REQUIRE (shortPosition2->isPositionOpen());

    posManager.closeUnitPosition(qqqSymbol,
				 createDate ("20160212"),
				 createDecimal("98.02"),
				 2);

    REQUIRE (shortPosition1->isPositionOpen());
    REQUIRE (shortPosition2->isPositionClosed());
    REQUIRE (shortPosition2->getExitPrice() == createDecimal("98.02"));
    REQUIRE (shortPosition2->getExitDate() ==  createDate ("20160212"));

    InstrumentPosition<DecimalType> qqqInstrument2 = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument2.getNumPositionUnits() == 1);
    REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
    REQUIRE (posManager.isShortPosition(qqqSymbol));
    REQUIRE_FALSE (posManager.isFlatPosition(qqqSymbol));


    REQUIRE (qqqInstrument2.getInstrumentSymbol() == qqqSymbol);
    REQUIRE (qqqInstrument2.getFillPrice() == shortEntry6->getOpenValue());
    REQUIRE (qqqInstrument2.getFillPrice(1) == shortEntry6->getOpenValue());
    REQUIRE_THROWS (qqqInstrument2.getFillPrice(2));

    posManager.closeUnitPosition(qqqSymbol,
				 createDate ("20160213"),
				 createDecimal("99.02"),
				 1);

    REQUIRE (shortPosition1->isPositionClosed());
    REQUIRE (shortPosition2->isPositionClosed());
    REQUIRE (shortPosition1->getExitPrice() == createDecimal("99.02"));
    REQUIRE (shortPosition1->getExitDate() ==  createDate ("20160213"));

    qqqInstrument2 = posManager.getInstrumentPosition(qqqSymbol);
    REQUIRE (qqqInstrument2.getNumPositionUnits() == 0);
    REQUIRE_FALSE (posManager.isLongPosition(qqqSymbol));
    REQUIRE_FALSE (posManager.isShortPosition(qqqSymbol));
    REQUIRE (posManager.isFlatPosition(qqqSymbol));
  }

  SECTION ("Test closeAllPositions")
    {
      InstrumentPosition<DecimalType> cornInstrument = posManager.getInstrumentPosition(tickerSymbol);
      REQUIRE (cornInstrument.getNumPositionUnits() == 2);
      REQUIRE (posManager.isLongPosition(tickerSymbol));
      REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
      REQUIRE_FALSE (posManager.isFlatPosition(tickerSymbol));
      REQUIRE (longPosition1->isPositionOpen());
      REQUIRE (longPosition2->isPositionOpen());
      posManager.closeAllPositions (tickerSymbol,  createDate("19851205"),
				    createDecimal("3725.3137207"));

      InstrumentPosition<DecimalType> cornInstrument2 = posManager.getInstrumentPosition(tickerSymbol);
      REQUIRE (cornInstrument.getNumPositionUnits() == 0);
      REQUIRE_FALSE (posManager.isLongPosition(tickerSymbol));
      REQUIRE_FALSE (posManager.isShortPosition(tickerSymbol));
      REQUIRE (posManager.isFlatPosition(tickerSymbol));

      REQUIRE (longPosition1->isPositionClosed());
      REQUIRE (longPosition2->isPositionClosed());
      REQUIRE (longPosition1->getExitPrice() == createDecimal("3725.3137207"));
      REQUIRE (longPosition2->getExitPrice() == createDecimal("3725.3137207"));
      REQUIRE (longPosition1->getExitDate() == createDate("19851205"));
      REQUIRE (longPosition2->getExitDate() == createDate("19851205"));
    }

  SECTION ("Test exceptions pt. 1")
    {
      REQUIRE (posManager.getNumInstruments() == 2);
      REQUIRE_THROWS (posManager.addInstrument(tickerSymbol));
      REQUIRE_THROWS (posManager.addInstrument(qqqSymbol));
    }

  SECTION("Intraday addBarForOpenPosition with ptime", "[InstrumentPositionManager][ptime]") {
    // 1) Build two intraday bars at 09:30 and 10:30
    auto entry0 = createTimeSeriesEntry(
      "20250526", "09:30:00",
      "100.0","105.0","95.0","102.0","10"
    );
    auto entry1 = createTimeSeriesEntry(
      "20250526", "10:30:00",
      "102.0","107.0","97.0","104.0","15"
    );

    // 2) Create an intraday series and wrap in a Security/Portfolio
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series->addEntry(*entry0);
    series->addEntry(*entry1);
    auto eq = std::make_shared<EquitySecurity<DecimalType>>("SYM", "Test Equity", series);
    auto port = std::make_shared<Portfolio<DecimalType>>("P");
    port->addSecurity(eq);

    // 3) Set up the manager, register the instrument, and open one intraday long
    InstrumentPositionManager<DecimalType> mgr;
    mgr.addInstrument("SYM");
    TradingVolume one(1, TradingVolume::SHARES);
    auto pos = std::make_shared<TradingPositionLong<DecimalType>>(
      "SYM", entry0->getOpenValue(), *entry0, one
    );
    mgr.addPosition(pos);
    REQUIRE(pos->getNumBarsInPosition() == 1);

    // 4) Advance by the second bar using the ptime overload
    ptime dt1 = entry1->getDateTime();
    mgr.addBarForOpenPosition(dt1, port.get());  // ptime overload

    // 5) Verify the TradingPosition inside the manager saw two bars
    auto fetched = mgr.getTradingPosition("SYM", 1);
    REQUIRE(fetched->getNumBarsInPosition() == 2);
}

SECTION("Intraday closeAllPositions with ptime", "[InstrumentPositionManager][ptime]") {
    // 1) Rebuild two-bar intraday setup
    auto entry0 = createTimeSeriesEntry(
      "20250526", "09:30:00",
      "100.0","105.0","95.0","102.0","10"
    );
    auto entry1 = createTimeSeriesEntry(
      "20250526", "10:30:00",
      "102.0","107.0","97.0","104.0","15"
    );
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
    series->addEntry(*entry0);
    series->addEntry(*entry1);
    auto eq = std::make_shared<EquitySecurity<DecimalType>>("ABC","Test Equity", series);
    auto port = std::make_shared<Portfolio<DecimalType>>("P2");
    port->addSecurity(eq);

    // 2) Open an intraday position
    InstrumentPositionManager<DecimalType> mgr2;
    mgr2.addInstrument("ABC");
    TradingVolume one(1, TradingVolume::SHARES);
    auto pos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      "ABC", entry0->getOpenValue(), *entry0, one
    );
    mgr2.addPosition(pos2);
    REQUIRE(mgr2.isLongPosition("ABC"));
    REQUIRE(mgr2.getNumPositionUnits("ABC") == 1);

    // 3) Close it at 10:30 via the ptime overload
    ptime exitDT = entry1->getDateTime();
    DecimalType exitPrice = entry1->getCloseValue();
    mgr2.closeAllPositions("ABC", exitDT, exitPrice);  // ptime overload

    // 4) After closing, the instrument should be flat and have zero units
    REQUIRE(mgr2.isFlatPosition("ABC"));
    REQUIRE(mgr2.getNumPositionUnits("ABC") == 0);

    // 5) Attempting to fetch a unit now throws
    REQUIRE_THROWS_AS(mgr2.getTradingPosition("ABC", 1), InstrumentPositionException);

    // 6) The original position object was closed in-place—verify its timestamps
    REQUIRE(pos2->getExitDateTime() == exitDT);
    REQUIRE(pos2->getExitDate()     == exitDT.date());
    REQUIRE(pos2->getExitPrice()    == exitPrice);
 }
}

TEST_CASE("InstrumentPositionManager - Move Semantics", "[InstrumentPositionManager][MoveSematics]")
{
  auto entry0 = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178", "3656.81982", "3672.20068", 0);
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol1("C2");
  std::string symbol2("QQQ");

  SECTION("Move Constructor")
  {
    // Create and populate original manager
    InstrumentPositionManager<DecimalType> original;
    original.addInstrument(symbol1);
    original.addInstrument(symbol2);
    
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol1, entry0->getOpenValue(), *entry0, oneContract);
    original.addPosition(longPos);
    
    REQUIRE(original.getNumInstruments() == 2);
    REQUIRE(original.isLongPosition(symbol1));
    REQUIRE(original.getNumPositionUnits(symbol1) == 1);

    // Move construct
    InstrumentPositionManager<DecimalType> moved(std::move(original));
    
    // Verify moved-to object has correct state
    REQUIRE(moved.getNumInstruments() == 2);
    REQUIRE(moved.isLongPosition(symbol1));
    REQUIRE(moved.isFlatPosition(symbol2));
    REQUIRE(moved.getNumPositionUnits(symbol1) == 1);
    
    // Original should still be valid (though in unspecified state for members)
    // We can't make strong guarantees about original's state after move
  }

  SECTION("Move Assignment Operator")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument(symbol1);
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol1, entry0->getOpenValue(), *entry0, oneContract);
    manager1.addPosition(longPos);
    
    InstrumentPositionManager<DecimalType> manager2;
    manager2.addInstrument(symbol2);
    
    REQUIRE(manager1.getNumInstruments() == 1);
    REQUIRE(manager2.getNumInstruments() == 1);
    
    // Move assign
    manager2 = std::move(manager1);
    
    REQUIRE(manager2.getNumInstruments() == 1);
    REQUIRE(manager2.isLongPosition(symbol1));
    REQUIRE(manager2.getNumPositionUnits(symbol1) == 1);
  }


  SECTION("Move Assignment Self-Check (via reference)")
    {
      // This test verifies the self-assignment check in move assignment
      // We use a reference to avoid the compiler warning about obvious self-move
      InstrumentPositionManager<DecimalType> manager;
      manager.addInstrument(symbol1);
      auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(symbol1,
									entry0->getOpenValue(),
									*entry0,
									oneContract);
      manager.addPosition(longPos);
  
      // Use a reference to the same object to trigger the self-assignment check
      // without triggering compiler warnings
      InstrumentPositionManager<DecimalType>& managerRef = manager;
      
      // This tests the self-assignment check: if (this == &rhs)
      manager = std::move(managerRef);
      
      // Manager should still be in a valid state after self-move-assignment
      // The self-assignment check should have prevented any actual moving
      REQUIRE(manager.getNumInstruments() == 1);
      REQUIRE(manager.isLongPosition(symbol1));
      REQUIRE(manager.getNumPositionUnits(symbol1) == 1);
    }
  
  SECTION("Swap Function")
  {
    InstrumentPositionManager<DecimalType> manager1;
    InstrumentPositionManager<DecimalType> manager2;
    
    manager1.addInstrument(symbol1);
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol1, entry0->getOpenValue(), *entry0, oneContract);
    manager1.addPosition(longPos);
    
    manager2.addInstrument(symbol2);
    
    REQUIRE(manager1.getNumInstruments() == 1);
    REQUIRE(manager2.getNumInstruments() == 1);
    REQUIRE(manager1.isLongPosition(symbol1));
    REQUIRE_THROWS(manager1.isLongPosition(symbol2)); // symbol2 not in manager1
    
    // Swap
    manager1.swap(manager2);
    
    // After swap, manager1 should have symbol2, manager2 should have symbol1
    REQUIRE(manager1.getNumInstruments() == 1);
    REQUIRE(manager2.getNumInstruments() == 1);
    REQUIRE_THROWS(manager2.isLongPosition(symbol2)); // symbol2 now in manager1
    REQUIRE(manager2.isLongPosition(symbol1)); // symbol1 now in manager2
    
    // Test non-member swap
    swap(manager1, manager2);
    REQUIRE(manager1.isLongPosition(symbol1)); // Back to original
  }
}

TEST_CASE("InstrumentPositionManager - Copy Semantics", "[InstrumentPositionManager][CopySemantics]")
{
  auto entry0 = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178", "3656.81982", "3672.20068", 0);
  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string symbol("C2");

  SECTION("Copy Constructor")
  {
    InstrumentPositionManager<DecimalType> original;
    original.addInstrument(symbol);
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, entry0->getOpenValue(), *entry0, oneContract);
    original.addPosition(longPos);
    
    REQUIRE(original.getNumInstruments() == 1);
    REQUIRE(original.isLongPosition(symbol));
    
    // Copy construct
    InstrumentPositionManager<DecimalType> copied(original);
    
    // Both should have same state
    REQUIRE(copied.getNumInstruments() == 1);
    REQUIRE(copied.isLongPosition(symbol));
    REQUIRE(original.getNumInstruments() == 1);
    REQUIRE(original.isLongPosition(symbol));
    
    // They share the same InstrumentPosition objects (shared_ptr semantics)
    // So changes to position affect both
  }

  SECTION("Assignment Operator")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument(symbol);
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, entry0->getOpenValue(), *entry0, oneContract);
    manager1.addPosition(longPos);
    
    InstrumentPositionManager<DecimalType> manager2;
    REQUIRE(manager2.getNumInstruments() == 0);
    
    // Assign
    manager2 = manager1;
    
    REQUIRE(manager2.getNumInstruments() == 1);
    REQUIRE(manager2.isLongPosition(symbol));
    REQUIRE(manager1.getNumInstruments() == 1);
    REQUIRE(manager1.isLongPosition(symbol));
  }

  SECTION("Self Assignment")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument(symbol);
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, entry0->getOpenValue(), *entry0, oneContract);
    manager.addPosition(longPos);
    
    // Self assignment should be safe
    manager = manager;
    
    REQUIRE(manager.getNumInstruments() == 1);
    REQUIRE(manager.isLongPosition(symbol));
    REQUIRE(manager.getNumPositionUnits(symbol) == 1);
  }
}

TEST_CASE("InstrumentPositionManager - Exception Tests for Invalid Symbols", 
          "[InstrumentPositionManager][Exceptions]")
{
  InstrumentPositionManager<DecimalType> manager;
  std::string validSymbol("AAPL");
  std::string invalidSymbol("INVALID");
  
  manager.addInstrument(validSymbol);

  SECTION("getInstrumentPosition throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(manager.getInstrumentPosition(invalidSymbol), 
                     InstrumentPositionManagerException);
  }

  SECTION("isLongPosition throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(manager.isLongPosition(invalidSymbol), 
                     InstrumentPositionManagerException);
  }

  SECTION("isShortPosition throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(manager.isShortPosition(invalidSymbol), 
                     InstrumentPositionManagerException);
  }

  SECTION("isFlatPosition throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(manager.isFlatPosition(invalidSymbol), 
                     InstrumentPositionManagerException);
  }

  SECTION("getVolumeInAllUnits throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(manager.getVolumeInAllUnits(invalidSymbol), 
                     InstrumentPositionManagerException);
  }

  SECTION("getNumPositionUnits throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(manager.getNumPositionUnits(invalidSymbol), 
                     InstrumentPositionManagerException);
  }

  SECTION("getTradingPosition throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(manager.getTradingPosition(invalidSymbol, 1), 
                     InstrumentPositionManagerException);
  }

  SECTION("addBar throws for invalid symbol")
  {
    auto entry = createTimeSeriesEntry("19851118", "100", "105", "95", "102", 0);
    REQUIRE_THROWS_AS(manager.addBar(invalidSymbol, *entry), 
                     InstrumentPositionManagerException);
  }

  SECTION("closeAllPositions throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(
      manager.closeAllPositions(invalidSymbol, createDate("20240101"), createDecimal("100.0")), 
      InstrumentPositionManagerException);
  }

  SECTION("closeUnitPosition throws for invalid symbol")
  {
    REQUIRE_THROWS_AS(
      manager.closeUnitPosition(invalidSymbol, createDate("20240101"), createDecimal("100.0"), 1), 
      InstrumentPositionManagerException);
  }
}

TEST_CASE("InstrumentPositionManager - Operations on Flat Positions", 
          "[InstrumentPositionManager][FlatPosition]")
{
  InstrumentPositionManager<DecimalType> manager;
  std::string symbol("FLAT");
  manager.addInstrument(symbol);
  
  REQUIRE(manager.isFlatPosition(symbol));
  REQUIRE(manager.getNumPositionUnits(symbol) == 0);

  SECTION("getVolumeInAllUnits throws when flat")
  {
    REQUIRE_THROWS_AS(manager.getVolumeInAllUnits(symbol), 
                     InstrumentPositionException);
  }

  SECTION("closeAllPositions throws when already flat")
  {
    REQUIRE_THROWS_AS(
      manager.closeAllPositions(symbol, createDate("20240101"), createDecimal("100.0")), 
      InstrumentPositionException);
  }

  SECTION("getTradingPosition throws when flat")
  {
    REQUIRE_THROWS_AS(manager.getTradingPosition(symbol, 1), 
                     InstrumentPositionException);
  }

  SECTION("addBar throws when flat")
  {
    auto entry = createTimeSeriesEntry("19851118", "100", "105", "95", "102", 0);
    REQUIRE_THROWS_AS(manager.addBar(symbol, *entry), 
                     InstrumentPositionException);
  }
}

TEST_CASE("InstrumentPositionManager - Invalid Unit Numbers", 
          "[InstrumentPositionManager][InvalidUnits]")
{
  auto entry0 = createTimeSeriesEntry("19851118", "100", "105", "95", "102", 0);
  TradingVolume oneShare(1, TradingVolume::SHARES);
  std::string symbol("TEST");
  
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument(symbol);
  
  auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
    symbol, entry0->getOpenValue(), *entry0, oneShare);
  manager.addPosition(longPos);
  
  REQUIRE(manager.getNumPositionUnits(symbol) == 1);

  SECTION("getTradingPosition with unit number too high throws")
  {
    REQUIRE_THROWS_AS(manager.getTradingPosition(symbol, 2), 
                     InstrumentPositionException);
    REQUIRE_THROWS_AS(manager.getTradingPosition(symbol, 100), 
                     InstrumentPositionException);
  }

  SECTION("getTradingPosition with unit number zero throws")
  {
    REQUIRE_THROWS_AS(manager.getTradingPosition(symbol, 0), 
                     InstrumentPositionException);
  }

  SECTION("closeUnitPosition with invalid unit number throws")
  {
    REQUIRE_THROWS_AS(
      manager.closeUnitPosition(symbol, createDate("20240101"), createDecimal("100.0"), 2),
      InstrumentPositionException);
    
    REQUIRE_THROWS_AS(
      manager.closeUnitPosition(symbol, createDate("20240101"), createDecimal("100.0"), 0),
      InstrumentPositionException);
  }
}

TEST_CASE("InstrumentPositionManager - State Transitions", 
          "[InstrumentPositionManager][StateTransitions]")
{
  auto longEntry = createTimeSeriesEntry("20240101", "100", "105", "95", "102", 0);
  auto shortEntry = createTimeSeriesEntry("20240102", "102", "107", "97", "100", 0);
  TradingVolume oneShare(1, TradingVolume::SHARES);
  std::string symbol("TRANS");

  SECTION("Flat → Long → Flat")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument(symbol);
    
    REQUIRE(manager.isFlatPosition(symbol));
    REQUIRE_FALSE(manager.isLongPosition(symbol));
    REQUIRE_FALSE(manager.isShortPosition(symbol));
    
    // Add long position
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, longEntry->getOpenValue(), *longEntry, oneShare);
    manager.addPosition(longPos);
    
    REQUIRE_FALSE(manager.isFlatPosition(symbol));
    REQUIRE(manager.isLongPosition(symbol));
    REQUIRE_FALSE(manager.isShortPosition(symbol));
    
    // Close all positions
    manager.closeAllPositions(symbol, createDate("20240103"), createDecimal("105.0"));
    
    REQUIRE(manager.isFlatPosition(symbol));
    REQUIRE_FALSE(manager.isLongPosition(symbol));
    REQUIRE_FALSE(manager.isShortPosition(symbol));
  }

  SECTION("Flat → Short → Flat")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument(symbol);
    
    REQUIRE(manager.isFlatPosition(symbol));
    
    // Add short position
    auto shortPos = std::make_shared<TradingPositionShort<DecimalType>>(
      symbol, shortEntry->getOpenValue(), *shortEntry, oneShare);
    manager.addPosition(shortPos);
    
    REQUIRE_FALSE(manager.isFlatPosition(symbol));
    REQUIRE_FALSE(manager.isLongPosition(symbol));
    REQUIRE(manager.isShortPosition(symbol));
    
    // Close all positions
    manager.closeAllPositions(symbol, createDate("20240103"), createDecimal("95.0"));
    
    REQUIRE(manager.isFlatPosition(symbol));
    REQUIRE_FALSE(manager.isLongPosition(symbol));
    REQUIRE_FALSE(manager.isShortPosition(symbol));
  }

  SECTION("Flat → Long → Flat → Short → Flat")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument(symbol);
    
    // Start flat
    REQUIRE(manager.isFlatPosition(symbol));
    
    // Go long
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, longEntry->getOpenValue(), *longEntry, oneShare);
    manager.addPosition(longPos);
    REQUIRE(manager.isLongPosition(symbol));
    
    // Close long, back to flat
    manager.closeAllPositions(symbol, createDate("20240103"), createDecimal("105.0"));
    REQUIRE(manager.isFlatPosition(symbol));
    
    // Go short
    auto shortPos = std::make_shared<TradingPositionShort<DecimalType>>(
      symbol, shortEntry->getOpenValue(), *shortEntry, oneShare);
    manager.addPosition(shortPos);
    REQUIRE(manager.isShortPosition(symbol));
    
    // Close short, back to flat
    manager.closeAllPositions(symbol, createDate("20240104"), createDecimal("95.0"));
    REQUIRE(manager.isFlatPosition(symbol));
  }

  SECTION("Closing last unit transitions to flat")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument(symbol);
    
    auto entry1 = createTimeSeriesEntry("20240101", "100", "105", "95", "102", 0);
    auto entry2 = createTimeSeriesEntry("20240102", "102", "107", "97", "104", 0);
    
    // Add two positions (pyramiding)
    auto pos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, entry1->getOpenValue(), *entry1, oneShare);
    auto pos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, entry2->getOpenValue(), *entry2, oneShare);
    
    manager.addPosition(pos1);
    manager.addPosition(pos2);
    
    REQUIRE(manager.getNumPositionUnits(symbol) == 2);
    REQUIRE(manager.isLongPosition(symbol));
    
    // Close first unit
    manager.closeUnitPosition(symbol, createDate("20240103"), createDecimal("105.0"), 1);
    REQUIRE(manager.getNumPositionUnits(symbol) == 1);
    REQUIRE(manager.isLongPosition(symbol)); // Still long with 1 unit
    
    // Close second unit
    manager.closeUnitPosition(symbol, createDate("20240104"), createDecimal("106.0"), 1);
    REQUIRE(manager.getNumPositionUnits(symbol) == 0);
    REQUIRE(manager.isFlatPosition(symbol)); // Now flat
  }
}

TEST_CASE("InstrumentPositionManager - Edge Cases", 
          "[InstrumentPositionManager][EdgeCases]")
{
  SECTION("Empty manager operations")
  {
    InstrumentPositionManager<DecimalType> manager;
    
    REQUIRE(manager.getNumInstruments() == 0);
    REQUIRE(manager.beginInstrumentPositions() == manager.endInstrumentPositions());
    
    // Iterator on empty manager
    auto it = manager.beginInstrumentPositions();
    REQUIRE(it == manager.endInstrumentPositions());
  }

  SECTION("Adding duplicate instrument throws")
  {
    InstrumentPositionManager<DecimalType> manager;
    std::string symbol("DUP");
    
    manager.addInstrument(symbol);
    REQUIRE(manager.getNumInstruments() == 1);
    
    REQUIRE_THROWS_AS(manager.addInstrument(symbol), 
                     InstrumentPositionManagerException);
    REQUIRE(manager.getNumInstruments() == 1); // Count unchanged
  }

  SECTION("Iterator traversal with multiple instruments")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument("A");
    manager.addInstrument("B");
    manager.addInstrument("C");
    
    REQUIRE(manager.getNumInstruments() == 3);
    
    int count = 0;
    for (auto it = manager.beginInstrumentPositions(); 
         it != manager.endInstrumentPositions(); 
         ++it)
    {
      count++;
      REQUIRE_FALSE(it->second->getInstrumentSymbol().empty());
    }
    
    REQUIRE(count == 3);
  }

  SECTION("getVolumeInAllUnits with active positions")
  {
    auto entry = createTimeSeriesEntry("20240101", "100", "105", "95", "102", 0);
    TradingVolume twoShares(2, TradingVolume::SHARES);
    TradingVolume threeShares(3, TradingVolume::SHARES);
    std::string symbol("VOL");
    
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument(symbol);
    
    // Add first position with 2 shares
    auto pos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, entry->getOpenValue(), *entry, twoShares);
    manager.addPosition(pos1);
    
    TradingVolume vol1 = manager.getVolumeInAllUnits(symbol);
    REQUIRE(vol1.getTradingVolume() == 2);
    REQUIRE(vol1.getVolumeUnits() == TradingVolume::SHARES);
    
    // Add second position with 3 shares
    auto pos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      symbol, entry->getOpenValue(), *entry, threeShares);
    manager.addPosition(pos2);
    
    TradingVolume vol2 = manager.getVolumeInAllUnits(symbol);
    REQUIRE(vol2.getTradingVolume() == 5); // 2 + 3
    REQUIRE(vol2.getVolumeUnits() == TradingVolume::SHARES);
  }
}

TEST_CASE("InstrumentPositionManager - ptime Overload for closeUnitPosition", 
          "[InstrumentPositionManager][ptime]")
{
  auto entry0 = createTimeSeriesEntry("20250526", "09:30:00", "100.0", "105.0", "95.0", "102.0", "10");
  auto entry1 = createTimeSeriesEntry("20250526", "10:30:00", "102.0", "107.0", "97.0", "104.0", "15");
  
  TradingVolume oneShare(1, TradingVolume::SHARES);
  std::string symbol("INTRA");
  
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument(symbol);
  
  // Add two positions
  auto pos1 = std::make_shared<TradingPositionLong<DecimalType>>(
    symbol, entry0->getOpenValue(), *entry0, oneShare);
  auto pos2 = std::make_shared<TradingPositionLong<DecimalType>>(
    symbol, entry1->getOpenValue(), *entry1, oneShare);
  
  manager.addPosition(pos1);
  manager.addPosition(pos2);
  
  REQUIRE(manager.getNumPositionUnits(symbol) == 2);
  
  SECTION("Close unit position with ptime")
  {
    ptime exitTime = time_from_string("2025-05-26 11:30:00");
    DecimalType exitPrice = createDecimal("106.0");
    
    // Close first unit using ptime overload
    manager.closeUnitPosition(symbol, exitTime, exitPrice, 1);
    
    REQUIRE(manager.getNumPositionUnits(symbol) == 1);
    REQUIRE(pos1->isPositionClosed());
    REQUIRE(pos1->getExitDateTime() == exitTime);
    REQUIRE(pos1->getExitPrice() == exitPrice);
    REQUIRE(pos2->isPositionOpen());
  }
}

TEST_CASE("InstrumentPositionManager - Multiple Instruments Simultaneously", 
          "[InstrumentPositionManager][MultipleInstruments]")
{
  auto entry1 = createTimeSeriesEntry("20240101", "100", "105", "95", "102", 0);
  auto entry2 = createTimeSeriesEntry("20240101", "50", "55", "48", "52", 0);
  auto entry3 = createTimeSeriesEntry("20240101", "200", "210", "195", "205", 0);
  
  TradingVolume oneShare(1, TradingVolume::SHARES);
  
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument("AAPL");
  manager.addInstrument("GOOGL");
  manager.addInstrument("MSFT");
  
  REQUIRE(manager.getNumInstruments() == 3);
  
  // Add long position for AAPL
  auto aaplPos = std::make_shared<TradingPositionLong<DecimalType>>(
    "AAPL", entry1->getOpenValue(), *entry1, oneShare);
  manager.addPosition(aaplPos);
  
  // Add short position for GOOGL
  auto googlPos = std::make_shared<TradingPositionShort<DecimalType>>(
    "GOOGL", entry2->getOpenValue(), *entry2, oneShare);
  manager.addPosition(googlPos);
  
  // Leave MSFT flat
  
  SECTION("Query all instruments")
  {
    REQUIRE(manager.isLongPosition("AAPL"));
    REQUIRE_FALSE(manager.isShortPosition("AAPL"));
    REQUIRE_FALSE(manager.isFlatPosition("AAPL"));
    
    REQUIRE_FALSE(manager.isLongPosition("GOOGL"));
    REQUIRE(manager.isShortPosition("GOOGL"));
    REQUIRE_FALSE(manager.isFlatPosition("GOOGL"));
    
    REQUIRE_FALSE(manager.isLongPosition("MSFT"));
    REQUIRE_FALSE(manager.isShortPosition("MSFT"));
    REQUIRE(manager.isFlatPosition("MSFT"));
  }
  
  SECTION("Close positions independently")
  {
    manager.closeAllPositions("AAPL", createDate("20240102"), createDecimal("110.0"));
    
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE(manager.isShortPosition("GOOGL")); // GOOGL unaffected
    REQUIRE(manager.isFlatPosition("MSFT"));   // MSFT unaffected
    
    manager.closeAllPositions("GOOGL", createDate("20240102"), createDecimal("45.0"));
    
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE(manager.isFlatPosition("GOOGL"));
    REQUIRE(manager.isFlatPosition("MSFT"));
  }
  
  SECTION("Add bars to specific instruments")
  {
    auto bar = createTimeSeriesEntry("20240102", "103", "108", "98", "105", 0);
    
    // Should only affect AAPL (has open position)
    manager.addBar("AAPL", *bar);
    
    // Should only affect GOOGL (has open position)
    manager.addBar("GOOGL", *bar);
    
    // Should throw for MSFT (flat position)
    REQUIRE_THROWS_AS(manager.addBar("MSFT", *bar), InstrumentPositionException);
  }
}

TEST_CASE("InstrumentPositionManager - Basic Construction and State", "[InstrumentPositionManager]")
{
  SECTION("Default constructor creates empty manager")
  {
    InstrumentPositionManager<DecimalType> manager;
    
    REQUIRE(manager.getNumInstruments() == 0);
    REQUIRE(manager.beginInstrumentPositions() == manager.endInstrumentPositions());
  }
  
  SECTION("Copy constructor creates independent copy")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument("AAPL");
    manager1.addInstrument("MSFT");
    
    REQUIRE(manager1.getNumInstruments() == 2);
    
    InstrumentPositionManager<DecimalType> manager2(manager1);
    
    REQUIRE(manager2.getNumInstruments() == 2);
    REQUIRE(manager2.isFlatPosition("AAPL"));
    REQUIRE(manager2.isFlatPosition("MSFT"));
    
    // Verify independence - add to manager1
    manager1.addInstrument("GOOG");
    REQUIRE(manager1.getNumInstruments() == 3);
    REQUIRE(manager2.getNumInstruments() == 2); // manager2 should be unchanged
  }
  
  SECTION("Copy assignment creates independent copy")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument("AAPL");
    manager1.addInstrument("MSFT");
    
    InstrumentPositionManager<DecimalType> manager2;
    manager2.addInstrument("GOOG");
    
    REQUIRE(manager1.getNumInstruments() == 2);
    REQUIRE(manager2.getNumInstruments() == 1);
    
    manager2 = manager1; // Copy assignment
    
    REQUIRE(manager2.getNumInstruments() == 2);
    REQUIRE(manager2.isFlatPosition("AAPL"));
    REQUIRE(manager2.isFlatPosition("MSFT"));
    
    // Verify independence
    manager1.addInstrument("TSLA");
    REQUIRE(manager1.getNumInstruments() == 3);
    REQUIRE(manager2.getNumInstruments() == 2);
  }
  
  SECTION("Copy assignment self-assignment is safe")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument("AAPL");
    manager.addInstrument("MSFT");
    
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wself-assign-overloaded"
    manager = manager; // Self-assignment
    #pragma GCC diagnostic pop
    
    REQUIRE(manager.getNumInstruments() == 2);
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE(manager.isFlatPosition("MSFT"));
  }
  
  SECTION("Move constructor transfers ownership")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument("AAPL");
    manager1.addInstrument("MSFT");
    
    REQUIRE(manager1.getNumInstruments() == 2);
    
    InstrumentPositionManager<DecimalType> manager2(std::move(manager1));
    
    REQUIRE(manager2.getNumInstruments() == 2);
    REQUIRE(manager2.isFlatPosition("AAPL"));
    REQUIRE(manager2.isFlatPosition("MSFT"));
    
    // manager1 is in valid but unspecified state
    // We can't make strong guarantees about it, but it should be usable
    REQUIRE_NOTHROW(manager1.getNumInstruments());
  }
  
  SECTION("Move assignment transfers ownership")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument("AAPL");
    manager1.addInstrument("MSFT");
    
    InstrumentPositionManager<DecimalType> manager2;
    manager2.addInstrument("GOOG");
    
    REQUIRE(manager1.getNumInstruments() == 2);
    REQUIRE(manager2.getNumInstruments() == 1);
    
    manager2 = std::move(manager1); // Move assignment
    
    REQUIRE(manager2.getNumInstruments() == 2);
    REQUIRE(manager2.isFlatPosition("AAPL"));
    REQUIRE(manager2.isFlatPosition("MSFT"));
    
    // manager1 is in valid but unspecified state
    REQUIRE_NOTHROW(manager1.getNumInstruments());
  }
  
  SECTION("Move assignment self-move is safe")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument("AAPL");
    manager.addInstrument("MSFT");
    
    InstrumentPositionManager<DecimalType>* pManager = &manager;
    
    manager = std::move(*pManager); // Self-move
    
    // After self-move, object should still be in valid state
    REQUIRE_NOTHROW(manager.getNumInstruments());
  }
}

TEST_CASE("InstrumentPositionManager - Swap Operations", "[InstrumentPositionManager]")
{
  SECTION("Member swap exchanges contents")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument("AAPL");
    manager1.addInstrument("MSFT");
    
    InstrumentPositionManager<DecimalType> manager2;
    manager2.addInstrument("GOOG");
    
    REQUIRE(manager1.getNumInstruments() == 2);
    REQUIRE(manager2.getNumInstruments() == 1);
    
    manager1.swap(manager2);
    
    REQUIRE(manager1.getNumInstruments() == 1);
    REQUIRE(manager1.isFlatPosition("GOOG"));
    REQUIRE(manager2.getNumInstruments() == 2);
    REQUIRE(manager2.isFlatPosition("AAPL"));
    REQUIRE(manager2.isFlatPosition("MSFT"));
  }
  
  SECTION("Non-member swap exchanges contents")
  {
    InstrumentPositionManager<DecimalType> manager1;
    manager1.addInstrument("AAPL");
    manager1.addInstrument("MSFT");
    
    InstrumentPositionManager<DecimalType> manager2;
    manager2.addInstrument("GOOG");
    
    using std::swap;
    swap(manager1, manager2); // Uses ADL to find the right swap
    
    REQUIRE(manager1.getNumInstruments() == 1);
    REQUIRE(manager1.isFlatPosition("GOOG"));
    REQUIRE(manager2.getNumInstruments() == 2);
    REQUIRE(manager2.isFlatPosition("AAPL"));
    REQUIRE(manager2.isFlatPosition("MSFT"));
  }
}

TEST_CASE("InstrumentPositionManager - Add Instrument", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  
  SECTION("Adding new instrument succeeds")
  {
    REQUIRE_NOTHROW(manager.addInstrument("AAPL"));
    REQUIRE(manager.getNumInstruments() == 1);
    REQUIRE(manager.isFlatPosition("AAPL"));
  }
  
  SECTION("Adding multiple instruments succeeds")
  {
    manager.addInstrument("AAPL");
    manager.addInstrument("MSFT");
    manager.addInstrument("GOOG");
    
    REQUIRE(manager.getNumInstruments() == 3);
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE(manager.isFlatPosition("MSFT"));
    REQUIRE(manager.isFlatPosition("GOOG"));
  }
  
  SECTION("Adding duplicate instrument throws exception")
  {
    manager.addInstrument("AAPL");
    REQUIRE(manager.getNumInstruments() == 1);
    
    REQUIRE_THROWS_AS(manager.addInstrument("AAPL"), InstrumentPositionManagerException);
    REQUIRE(manager.getNumInstruments() == 1); // Should not change
  }
  
  SECTION("Instrument starts in flat state")
  {
    manager.addInstrument("AAPL");
    
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE_FALSE(manager.isLongPosition("AAPL"));
    REQUIRE_FALSE(manager.isShortPosition("AAPL"));
    REQUIRE(manager.getNumPositionUnits("AAPL") == 0);
  }
}

TEST_CASE("InstrumentPositionManager - Get Instrument Position", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  
  SECTION("Getting position for non-existent symbol throws")
  {
    REQUIRE_THROWS_AS(manager.getInstrumentPosition("INVALID"), 
                      InstrumentPositionManagerException);
  }
  
  SECTION("Getting position by symbol returns correct reference")
  {
    manager.addInstrument("AAPL");
    
    const InstrumentPosition<DecimalType>& pos = manager.getInstrumentPosition("AAPL");
    REQUIRE(pos.getInstrumentSymbol() == "AAPL");
    REQUIRE(pos.isFlatPosition());
  }
  
  SECTION("Getting position by iterator returns correct reference")
  {
    manager.addInstrument("AAPL");
    manager.addInstrument("MSFT");
    
    auto it = manager.beginInstrumentPositions();
    const InstrumentPosition<DecimalType>& pos = manager.getInstrumentPosition(it);
    
    // Iterator points to one of the instruments
    REQUIRE((pos.getInstrumentSymbol() == "AAPL" || 
             pos.getInstrumentSymbol() == "MSFT"));
  }
}

TEST_CASE("InstrumentPositionManager - Position State Queries", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument("AAPL");
  
  auto entry = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
  TradingVolume vol(100, TradingVolume::SHARES);
  
  SECTION("Query on non-existent symbol throws")
  {
    REQUIRE_THROWS_AS(manager.isLongPosition("INVALID"), InstrumentPositionManagerException);
    REQUIRE_THROWS_AS(manager.isShortPosition("INVALID"), InstrumentPositionManagerException);
    REQUIRE_THROWS_AS(manager.isFlatPosition("INVALID"), InstrumentPositionManagerException);
  }
  
  SECTION("Flat position queries work correctly")
  {
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE_FALSE(manager.isLongPosition("AAPL"));
    REQUIRE_FALSE(manager.isShortPosition("AAPL"));
  }
  
  SECTION("Long position queries work correctly")
  {
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    
    manager.addPosition(longPos);
    
    REQUIRE(manager.isLongPosition("AAPL"));
    REQUIRE_FALSE(manager.isShortPosition("AAPL"));
    REQUIRE_FALSE(manager.isFlatPosition("AAPL"));
  }
  
  SECTION("Short position queries work correctly")
  {
    auto shortPos = std::make_shared<TradingPositionShort<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    
    manager.addPosition(shortPos);
    
    REQUIRE(manager.isShortPosition("AAPL"));
    REQUIRE_FALSE(manager.isLongPosition("AAPL"));
    REQUIRE_FALSE(manager.isFlatPosition("AAPL"));
  }
}

TEST_CASE("InstrumentPositionManager - Add Position", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  
  auto entry = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
  TradingVolume vol(100, TradingVolume::SHARES);
  
  SECTION("Adding position to non-existent instrument throws")
  {
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    
    REQUIRE_THROWS_AS(manager.addPosition(longPos), InstrumentPositionManagerException);
  }
  
  SECTION("Adding long position changes state from flat to long")
  {
    manager.addInstrument("AAPL");
    REQUIRE(manager.isFlatPosition("AAPL"));
    
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    
    manager.addPosition(longPos);
    
    REQUIRE(manager.isLongPosition("AAPL"));
    REQUIRE(manager.getNumPositionUnits("AAPL") == 1);
  }
  
  SECTION("Adding multiple positions (pyramiding) works")
  {
    manager.addInstrument("AAPL");
    
    auto entry2 = createTimeSeriesEntry("20250102", "155.0", "157.0", "154.0", "156.0", 0);
    
    auto longPos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    auto longPos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry2->getOpenValue(), *entry2, vol);
    
    manager.addPosition(longPos1);
    REQUIRE(manager.getNumPositionUnits("AAPL") == 1);
    
    manager.addPosition(longPos2);
    REQUIRE(manager.getNumPositionUnits("AAPL") == 2);
  }
}

TEST_CASE("InstrumentPositionManager - Get Volume", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument("AAPL");
  
  auto entry = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
  
  SECTION("Get volume on non-existent symbol throws")
  {
    REQUIRE_THROWS_AS(manager.getVolumeInAllUnits("INVALID"), 
                      InstrumentPositionManagerException);
  }
  
  SECTION("Get volume on flat position throws")
  {
    REQUIRE_THROWS_AS(manager.getVolumeInAllUnits("AAPL"), InstrumentPositionException);
  }
  
  SECTION("Get volume on single position returns correct volume")
  {
    TradingVolume vol(100, TradingVolume::SHARES);
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    
    manager.addPosition(longPos);
    
    TradingVolume totalVol = manager.getVolumeInAllUnits("AAPL");
    REQUIRE(totalVol.getTradingVolume() == 100);
    REQUIRE(totalVol.getVolumeUnits() == TradingVolume::SHARES);
  }
  
  SECTION("Get volume on multiple positions returns sum")
  {
    auto entry2 = createTimeSeriesEntry("20250102", "155.0", "157.0", "154.0", "156.0", 0);
    
    TradingVolume vol1(100, TradingVolume::SHARES);
    TradingVolume vol2(150, TradingVolume::SHARES);
    
    auto longPos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol1);
    auto longPos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry2->getOpenValue(), *entry2, vol2);
    
    manager.addPosition(longPos1);
    manager.addPosition(longPos2);
    
    TradingVolume totalVol = manager.getVolumeInAllUnits("AAPL");
    REQUIRE(totalVol.getTradingVolume() == 250);
  }
}

TEST_CASE("InstrumentPositionManager - Get Trading Position", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument("AAPL");
  
  auto entry1 = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
  auto entry2 = createTimeSeriesEntry("20250102", "155.0", "157.0", "154.0", "156.0", 0);
  
  TradingVolume vol(100, TradingVolume::SHARES);
  
  SECTION("Get trading position on non-existent symbol throws")
  {
    REQUIRE_THROWS_AS(manager.getTradingPosition("INVALID", 1), 
                      InstrumentPositionManagerException);
  }
  
  SECTION("Get trading position with invalid unit number throws")
  {
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry1->getOpenValue(), *entry1, vol);
    manager.addPosition(longPos);
    
    REQUIRE_THROWS_AS(manager.getTradingPosition("AAPL", 0), InstrumentPositionException);
    REQUIRE_THROWS_AS(manager.getTradingPosition("AAPL", 2), InstrumentPositionException);
  }
  
  SECTION("Get trading position returns correct position")
  {
    auto longPos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry1->getOpenValue(), *entry1, vol);
    auto longPos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry2->getOpenValue(), *entry2, vol);
    
    manager.addPosition(longPos1);
    manager.addPosition(longPos2);
    
    auto pos1 = manager.getTradingPosition("AAPL", 1);
    auto pos2 = manager.getTradingPosition("AAPL", 2);
    
    REQUIRE(pos1->getEntryPrice() == entry1->getOpenValue());
    REQUIRE(pos2->getEntryPrice() == entry2->getOpenValue());
  }
}

TEST_CASE("InstrumentPositionManager - Close Positions", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument("AAPL");
  
  auto entry = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
  TradingVolume vol(100, TradingVolume::SHARES);
  
  SECTION("Close all positions on non-existent symbol throws")
  {
    date exitDate(2025, Jan, 2);
    DecimalType exitPrice = dec::fromString<DecimalType>("155.0");
    
    REQUIRE_THROWS_AS(manager.closeAllPositions("INVALID", exitDate, exitPrice), 
                      InstrumentPositionManagerException);
  }
  
  SECTION("Close all positions using date")
  {
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    manager.addPosition(longPos);
    
    REQUIRE(manager.isLongPosition("AAPL"));
    
    date exitDate(2025, Jan, 2);
    DecimalType exitPrice = dec::fromString<DecimalType>("155.0");
    
    manager.closeAllPositions("AAPL", exitDate, exitPrice);
    
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE(manager.getNumPositionUnits("AAPL") == 0);
  }
  
  SECTION("Close all positions using ptime")
  {
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    manager.addPosition(longPos);
    
    REQUIRE(manager.isLongPosition("AAPL"));
    
    ptime exitTime = time_from_string("2025-01-02 16:00:00");
    DecimalType exitPrice = dec::fromString<DecimalType>("155.0");
    
    manager.closeAllPositions("AAPL", exitTime, exitPrice);
    
    REQUIRE(manager.isFlatPosition("AAPL"));
  }
  
  SECTION("Close unit position using date")
  {
    auto entry2 = createTimeSeriesEntry("20250102", "155.0", "157.0", "154.0", "156.0", 0);
    
    auto longPos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    auto longPos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry2->getOpenValue(), *entry2, vol);
    
    manager.addPosition(longPos1);
    manager.addPosition(longPos2);
    REQUIRE(manager.getNumPositionUnits("AAPL") == 2);
    
    date exitDate(2025, Jan, 3);
    DecimalType exitPrice = dec::fromString<DecimalType>("160.0");
    
    manager.closeUnitPosition("AAPL", exitDate, exitPrice, 1);
    
    REQUIRE(manager.isLongPosition("AAPL")); // Still long with 1 position
    REQUIRE(manager.getNumPositionUnits("AAPL") == 1);
  }
  
  SECTION("Close unit position using ptime")
  {
    auto entry2 = createTimeSeriesEntry("20250102", "155.0", "157.0", "154.0", "156.0", 0);
    
    auto longPos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    auto longPos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry2->getOpenValue(), *entry2, vol);
    
    manager.addPosition(longPos1);
    manager.addPosition(longPos2);
    
    ptime exitTime = time_from_string("2025-01-03 16:00:00");
    DecimalType exitPrice = dec::fromString<DecimalType>("160.0");
    
    manager.closeUnitPosition("AAPL", exitTime, exitPrice, 2);
    
    REQUIRE(manager.getNumPositionUnits("AAPL") == 1);
  }
  
  SECTION("Close last unit makes position flat")
  {
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    manager.addPosition(longPos);
    
    date exitDate(2025, Jan, 2);
    DecimalType exitPrice = dec::fromString<DecimalType>("155.0");
    
    manager.closeUnitPosition("AAPL", exitDate, exitPrice, 1);
    
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE(manager.getNumPositionUnits("AAPL") == 0);
  }
}

TEST_CASE("InstrumentPositionManager - Add Bar", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  manager.addInstrument("AAPL");
  
  auto entry1 = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
  auto entry2 = createTimeSeriesEntry("20250102", "155.0", "157.0", "154.0", "156.0", 0);
  
  TradingVolume vol(100, TradingVolume::SHARES);
  
  SECTION("Add bar to non-existent symbol throws")
  {
    REQUIRE_THROWS_AS(manager.addBar("INVALID", *entry2), 
                      InstrumentPositionManagerException);
  }
  
  SECTION("Add bar updates open position")
  {
    auto longPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry1->getOpenValue(), *entry1, vol);
    manager.addPosition(longPos);
    
    REQUIRE(longPos->getNumBarsInPosition() == 1);
    
    manager.addBar("AAPL", *entry2);
    
    REQUIRE(longPos->getNumBarsInPosition() == 2);
    REQUIRE(longPos->getLastClose() == entry2->getCloseValue());
  }
}

TEST_CASE("InstrumentPositionManager - Multiple Instruments", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  
  // Setup multiple instruments
  manager.addInstrument("AAPL");
  manager.addInstrument("MSFT");
  manager.addInstrument("GOOG");
  
  auto aaplEntry = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
  auto msftEntry = createTimeSeriesEntry("20250101", "300.0", "305.0", "298.0", "303.0", 0);
  auto googEntry = createTimeSeriesEntry("20250101", "2800.0", "2850.0", "2780.0", "2830.0", 0);
  
  TradingVolume vol(100, TradingVolume::SHARES);
  
  SECTION("Can manage multiple instruments independently")
  {
    auto aaplPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", aaplEntry->getOpenValue(), *aaplEntry, vol);
    auto msftPos = std::make_shared<TradingPositionShort<DecimalType>>(
      "MSFT", msftEntry->getOpenValue(), *msftEntry, vol);
    
    manager.addPosition(aaplPos);
    manager.addPosition(msftPos);
    
    REQUIRE(manager.isLongPosition("AAPL"));
    REQUIRE(manager.isShortPosition("MSFT"));
    REQUIRE(manager.isFlatPosition("GOOG"));
    
    REQUIRE(manager.getNumPositionUnits("AAPL") == 1);
    REQUIRE(manager.getNumPositionUnits("MSFT") == 1);
    REQUIRE(manager.getNumPositionUnits("GOOG") == 0);
  }
  
  SECTION("Can iterate through all instruments")
  {
    size_t count = 0;
    for (auto it = manager.beginInstrumentPositions(); 
         it != manager.endInstrumentPositions(); 
         ++it)
    {
      const InstrumentPosition<DecimalType>& pos = manager.getInstrumentPosition(it);
      REQUIRE(pos.isFlatPosition()); // All start flat
      count++;
    }
    REQUIRE(count == 3);
  }
  
  SECTION("Closing one instrument doesn't affect others")
  {
    auto aaplPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", aaplEntry->getOpenValue(), *aaplEntry, vol);
    auto msftPos = std::make_shared<TradingPositionLong<DecimalType>>(
      "MSFT", msftEntry->getOpenValue(), *msftEntry, vol);
    
    manager.addPosition(aaplPos);
    manager.addPosition(msftPos);
    
    REQUIRE(manager.isLongPosition("AAPL"));
    REQUIRE(manager.isLongPosition("MSFT"));
    
    date exitDate(2025, Jan, 2);
    DecimalType exitPrice = dec::fromString<DecimalType>("155.0");
    
    manager.closeAllPositions("AAPL", exitDate, exitPrice);
    
    REQUIRE(manager.isFlatPosition("AAPL"));
    REQUIRE(manager.isLongPosition("MSFT")); // MSFT unaffected
  }
}

TEST_CASE("InstrumentPositionManager - Iterator Operations", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  
  SECTION("Empty manager has begin == end")
  {
    REQUIRE(manager.beginInstrumentPositions() == manager.endInstrumentPositions());
  }
  
  SECTION("Can iterate through instruments")
  {
    manager.addInstrument("AAPL");
    manager.addInstrument("MSFT");
    manager.addInstrument("GOOG");
    
    std::vector<std::string> symbols;
    for (auto it = manager.beginInstrumentPositions(); 
         it != manager.endInstrumentPositions(); 
         ++it)
    {
      symbols.push_back(it->first);
    }
    
    REQUIRE(symbols.size() == 3);
    // Map iteration order is sorted by key
    REQUIRE(std::find(symbols.begin(), symbols.end(), "AAPL") != symbols.end());
    REQUIRE(std::find(symbols.begin(), symbols.end(), "MSFT") != symbols.end());
    REQUIRE(std::find(symbols.begin(), symbols.end(), "GOOG") != symbols.end());
  }
}

TEST_CASE("InstrumentPositionManager - Edge Cases", "[InstrumentPositionManager]")
{
  SECTION("Get num position units on non-existent symbol throws")
  {
    InstrumentPositionManager<DecimalType> manager;
    
    REQUIRE_THROWS_AS(manager.getNumPositionUnits("INVALID"), 
                      InstrumentPositionManagerException);
  }
  
  SECTION("Operations on empty manager")
  {
    InstrumentPositionManager<DecimalType> manager;
    
    REQUIRE(manager.getNumInstruments() == 0);
    REQUIRE(manager.beginInstrumentPositions() == manager.endInstrumentPositions());
  }
  
  SECTION("Can reuse symbol after closing all positions")
  {
    InstrumentPositionManager<DecimalType> manager;
    manager.addInstrument("AAPL");
    
    auto entry = createTimeSeriesEntry("20250101", "150.0", "152.0", "149.0", "151.0", 0);
    TradingVolume vol(100, TradingVolume::SHARES);
    
    auto longPos1 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry->getOpenValue(), *entry, vol);
    manager.addPosition(longPos1);
    
    date exitDate(2025, Jan, 2);
    DecimalType exitPrice = dec::fromString<DecimalType>("155.0");
    manager.closeAllPositions("AAPL", exitDate, exitPrice);
    
    REQUIRE(manager.isFlatPosition("AAPL"));
    
    // Can add new position after closing
    auto entry2 = createTimeSeriesEntry("20250103", "160.0", "162.0", "159.0", "161.0", 0);
    auto longPos2 = std::make_shared<TradingPositionLong<DecimalType>>(
      "AAPL", entry2->getOpenValue(), *entry2, vol);
    
    REQUIRE_NOTHROW(manager.addPosition(longPos2));
    REQUIRE(manager.isLongPosition("AAPL"));
  }
}

TEST_CASE("InstrumentPositionManager - Exception Messages", "[InstrumentPositionManager]")
{
  InstrumentPositionManager<DecimalType> manager;
  
  SECTION("Non-existent symbol exception contains symbol name")
  {
    try {
      manager.getInstrumentPosition("NONEXISTENT");
      FAIL("Should have thrown exception");
    } catch (const InstrumentPositionManagerException& e) {
      std::string msg(e.what());
      REQUIRE(msg.find("NONEXISTENT") != std::string::npos);
    }
  }
  
  SECTION("Duplicate symbol exception is descriptive")
  {
    manager.addInstrument("AAPL");
    
    try {
      manager.addInstrument("AAPL");
      FAIL("Should have thrown exception");
    } catch (const InstrumentPositionManagerException& e) {
      std::string msg(e.what());
      REQUIRE(msg.find("already exists") != std::string::npos);
    }
  }
}
