#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Security.h"
#include "SecurityFactory.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::ptime;
using boost::posix_time::hours;

TEST_CASE ("Security operations", "[Security]")
{
  auto entry0 = createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry1 = createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry2 = createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry3 = createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry4 = createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry6 = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);

  spySeries->addEntry (*entry4);
  spySeries->addEntry (*entry6);
  spySeries->addEntry (*entry2);
  spySeries->addEntry (*entry3);
  spySeries->addEntry (*entry1);
  spySeries->addEntry (*entry5);
  spySeries->addEntry (*entry0);

  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  EquitySecurity<DecimalType> spy (equitySymbol, equityName, spySeries);

  REQUIRE (spy.getName() == equityName);
  REQUIRE (spy.getSymbol() == equitySymbol);
  REQUIRE (spy.getBigPointValue() == DecimalConstants<DecimalType>::DecimalOne);
  REQUIRE (spy.getTick() == DecimalConstants<DecimalType>::EquityTick);
  REQUIRE (spy.isEquitySecurity());
  REQUIRE_FALSE (spy.isFuturesSecurity());

  auto spyFromFactory (SecurityFactory<DecimalType>::createSecurity (equitySymbol, spySeries));
  REQUIRE (*(spy.getTimeSeries()) == *(spyFromFactory->getTimeSeries()));
  REQUIRE (spyFromFactory->getName() == spy.getName());
  REQUIRE (spyFromFactory->getSymbol() == spy.getSymbol());
  REQUIRE (spyFromFactory->getBigPointValue()  == spy.getBigPointValue());
  REQUIRE (spyFromFactory->getTick() == spy.getTick());
  REQUIRE (spyFromFactory->isEquitySecurity());
  REQUIRE_FALSE (spyFromFactory->isFuturesSecurity());

	   
  auto spy2 = spy.clone (spySeries);
  REQUIRE (spy2->getName() == spy.getName());
  REQUIRE (spy2->getSymbol() == spy.getSymbol());
  REQUIRE (spy2->getBigPointValue()  == spy.getBigPointValue());
  REQUIRE (spy2->getTick() == spy.getTick());
  REQUIRE (spy2->isEquitySecurity());
  REQUIRE_FALSE (spy2->isFuturesSecurity());

  Security<DecimalType>::ConstRandomAccessIterator itBegin = spy.getRandomAccessIteratorBegin();
  Security<DecimalType>::ConstRandomAccessIterator itEnd = spy.getRandomAccessIteratorEnd();

  itEnd--;

  REQUIRE ((*itBegin) == *entry6);
  REQUIRE ((*itEnd) == *entry0);

  REQUIRE (spy.getFirstDate() == entry6->getDateValue());
  REQUIRE (spy.getLastDate() == entry0->getDateValue());

  REQUIRE (spy2->getFirstDate() == entry6->getDateValue());
  REQUIRE (spy2->getLastDate() == entry0->getDateValue());

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

  REQUIRE (corn.getName() == futuresName);
  REQUIRE (corn.getSymbol() == futuresSymbol);
  REQUIRE (corn.getBigPointValue() == cornBigPointValue);
  REQUIRE (corn.getTick() == cornTickValue);
  REQUIRE_FALSE (corn.isEquitySecurity());
  REQUIRE (corn.isFuturesSecurity());

  auto corn2 = corn.clone (cornSeries);
  REQUIRE (corn2->getName() == corn.getName());
  REQUIRE (corn2->getSymbol() == corn.getSymbol());
  REQUIRE (corn2->getBigPointValue()  == corn.getBigPointValue());
  REQUIRE (corn2->getTick() == corn.getTick());
  REQUIRE_FALSE (corn2->isEquitySecurity());
  REQUIRE (corn2->isFuturesSecurity());

  Security<DecimalType>::ConstRandomAccessIterator itBeginCorn = corn.getRandomAccessIteratorBegin();
  Security<DecimalType>::ConstRandomAccessIterator itEndCorn = corn.getRandomAccessIteratorEnd();

  itEndCorn--;

  REQUIRE ((*itBeginCorn) == *futuresEntry0);
  REQUIRE ((*itEndCorn) == *futuresEntry11);

  REQUIRE (corn.getFirstDate() == futuresEntry0->getDateValue());
  REQUIRE (corn.getLastDate() == futuresEntry11->getDateValue());

  REQUIRE (corn2->getFirstDate() == futuresEntry0->getDateValue());
  REQUIRE (corn2->getLastDate() == futuresEntry11->getDateValue());

  SECTION ("Equity Security Time Series Access", "[TimeSeries Access]")
    {
      date randomDate1(2016, 1, 4);
      Security<DecimalType>::ConstRandomAccessIterator it = spy.getRandomAccessIterator(randomDate1);
      REQUIRE (it != spy.getRandomAccessIteratorEnd());
      REQUIRE ((*it) == *entry2);

      REQUIRE (spy.getTimeSeriesEntry(randomDate1) == *entry2);
    }

  SECTION ("Futures Security Time Series Access", "[TimeSeries Access]")
    {
      date randomDate1(1985, 11, 25);
      Security<DecimalType>::ConstRandomAccessIterator it = corn.getRandomAccessIterator(randomDate1);
      REQUIRE (it != corn.getRandomAccessIteratorEnd());
      REQUIRE ((*it) == *futuresEntry5);

      REQUIRE (corn.getTimeSeriesEntry(randomDate1) == *futuresEntry5);
    }

  SECTION ("Equity Security Time Series Access pt. 2", "[TimeSeries Access (2)]")
    {
      date randomDate1(2016, 1, 6);
      Security<DecimalType>::ConstRandomAccessIterator it = spy.getRandomAccessIterator(randomDate1);
      REQUIRE (it != spy.getRandomAccessIteratorEnd());
      REQUIRE (spy.getTimeSeriesEntry(it, 0) == *entry0);
      REQUIRE (spy.getTimeSeriesEntry(it, 1) == *entry1);
      REQUIRE (spy.getTimeSeriesEntry(it, 2) == *entry2);
      REQUIRE (spy.getTimeSeriesEntry(it, 3) == *entry3);
      REQUIRE (spy.getTimeSeriesEntry(it, 4) == *entry4);
      REQUIRE (spy.getTimeSeriesEntry(it, 5) == *entry5);
      REQUIRE (spy.getTimeSeriesEntry(it, 6) == *entry6);
    }

  SECTION ("Equity Security Time Series Access pt. 3", "[TimeSeries Access (3)]")
    {
      date randomDate1(2016, 1, 5);
      Security<DecimalType>::ConstRandomAccessIterator it = spy.getRandomAccessIterator(randomDate1);
      REQUIRE (it != spy.getRandomAccessIteratorEnd());
      REQUIRE (spy.getTimeSeriesEntry(it, 0) == *entry1);
      REQUIRE (spy.getTimeSeriesEntry(it, 1) == *entry2);
      REQUIRE (spy.getTimeSeriesEntry(it, 2) == *entry3);
      REQUIRE (spy.getTimeSeriesEntry(it, 3) == *entry4);
      REQUIRE (spy.getTimeSeriesEntry(it, 4) == *entry5);
      REQUIRE (spy.getTimeSeriesEntry(it, 5) == *entry6);
    }

  SECTION ("Equity Security Time Series Access pt. 4", "[TimeSeries Access (4)]")
    {
      date randomDate1(2016, 1, 4);
      Security<DecimalType>::ConstRandomAccessIterator it = spy.getRandomAccessIterator(randomDate1);

      date aDate = spy.getDateValue(it, 0);
      REQUIRE (aDate == entry2->getDateValue());
      REQUIRE (spy.getOpenValue(it, 1) == entry3->getOpenValue());
      REQUIRE (spy.getHighValue(it, 2) == entry4->getHighValue());
      REQUIRE (spy.getLowValue(it, 3) == entry5->getLowValue());
      REQUIRE (spy.getCloseValue(it, 4) == entry6->getCloseValue());
    }

  SECTION ("Equity Security Time Series Access pt. 5", "[TimeSeries Access (5)]")
    {
      date randomDate1(2016, 1, 6);
      Security<DecimalType>::ConstRandomAccessIterator it = spy.getRandomAccessIterator(randomDate1);

      REQUIRE_THROWS (spy.getTimeSeriesEntry (it, 7));
    }

  SECTION ("Equity Security Time Series Access pt. 6", "[TimeSeries Access (6)]")
    {
      Security<DecimalType>::ConstRandomAccessIterator it = spy.getRandomAccessIteratorEnd();

      REQUIRE_THROWS (spy.getTimeSeriesEntry (it, 1));
    }

  SECTION ("Equity Security Time Series Access pt. 7", "[TimeSeries Access (5)]")
    {
      date randomDate1(2016, 1, 15);
      REQUIRE_THROWS (spy.getRandomAccessIterator(randomDate1));
    }

// --- Intraday findTimeSeriesEntry (ptime) ---
  SECTION("Intraday findTimeSeriesEntry returns correct iterator or end", "[TimeSeries Access (ptime)]") {
    // pick a known entry
    ptime dt2 = entry2->getDateTime();
    auto it2 = spy.findTimeSeriesEntry(dt2);
    REQUIRE(it2 != spy.getRandomAccessIteratorEnd());
    REQUIRE(*it2 == *entry2);  // :contentReference[oaicite:0]{index=0}

    // miss a timestamp that isn't in the series
    ptime missingDt = dt2 + hours(3);
    REQUIRE(spy.findTimeSeriesEntry(missingDt) == spy.getRandomAccessIteratorEnd());
  }

  // --- Intraday getRandomAccessIterator (ptime) ---
  SECTION("Intraday getRandomAccessIterator throws on missing timestamp", "[TimeSeries Access (ptime)]") {
    ptime fakeDt(date(2020,1,1), hours(0));
    REQUIRE_THROWS_AS(spy.getRandomAccessIterator(fakeDt), SecurityException);  // :contentReference[oaicite:1]{index=1}
  }

  SECTION("Intraday getRandomAccessIterator returns iterator for existing timestamp", "[TimeSeries Access (ptime)]") {
    ptime dt4 = entry4->getDateTime();
    auto it4 = spy.getRandomAccessIterator(dt4);
    REQUIRE(*it4 == *entry4);  // :contentReference[oaicite:2]{index=2}
  }

  // --- Intraday getDateTimeValue (ptime) ---
  SECTION("Intraday getDateTimeValue returns correct datetime or throws on bad offset", "[TimeSeries Access (ptime)]") {
    // locate entry2
    ptime dt2 = entry2->getDateTime();
    auto it2 = spy.findTimeSeriesEntry(dt2);
    REQUIRE(it2 != spy.getRandomAccessIteratorEnd());

    // offset 0 → entry2
    REQUIRE(spy.getDateTimeValue(it2, 0) == dt2);
    // offset 1 → previous bar (entry3)
    REQUIRE(spy.getDateTimeValue(it2, 1) == entry3->getDateTime());  // :contentReference[oaicite:3]{index=3}

    // out-of-bounds offset should throw
    REQUIRE_THROWS(spy.getDateTimeValue(it2, 10));
  }
}
