#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Security.h"
#include "SecurityFactory.h"
#include "TestUtils.h"
#include "number.h"
#include "decimal_math.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::ptime;
using boost::posix_time::hours;
using boost::posix_time::minutes;

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

  Security<DecimalType>::ConstSortedIterator itBegin = spy.beginSortedEntries();
  Security<DecimalType>::ConstSortedIterator itEnd = spy.endSortedEntries();

  itEnd--;

  REQUIRE ((*itBegin) == *entry6);
  REQUIRE ((*itEnd) == *entry0);

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

  Security<DecimalType>::ConstSortedIterator itBeginCorn = corn.beginSortedEntries();
  Security<DecimalType>::ConstSortedIterator itEndCorn = corn.endSortedEntries();

  itEndCorn--;

  REQUIRE ((*itBeginCorn) == *futuresEntry0);
  REQUIRE ((*itEndCorn) == *futuresEntry11);

  SECTION ("Equity Security Time Series Access", "[TimeSeries Access]")
    {
      date randomDate1(2016, 1, 4);
      // Use new API to check entry exists and retrieve it
      REQUIRE (spy.isDateFound(randomDate1));
      auto entry = spy.getTimeSeriesEntry(randomDate1);
      REQUIRE (entry == *entry2);

      REQUIRE (spy.getTimeSeriesEntry(randomDate1) == *entry2);
    }

  SECTION ("Futures Security Time Series Access", "[TimeSeries Access]")
    {
      date randomDate1(1985, 11, 25);
      // Use new API to check entry exists and retrieve it
      REQUIRE (corn.isDateFound(randomDate1));
      auto entry = corn.getTimeSeriesEntry(randomDate1);
      REQUIRE (entry == *futuresEntry5);

      REQUIRE (corn.getTimeSeriesEntry(randomDate1) == *futuresEntry5);
    }

  SECTION ("Equity Security Time Series Access pt. 2", "[TimeSeries Access (2)]")
    {
      date randomDate1(2016, 1, 6);
      // Use date-based access instead of iterator-based
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 0) == *entry0);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 1) == *entry1);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 2) == *entry2);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 3) == *entry3);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 4) == *entry4);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 5) == *entry5);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 6) == *entry6);
    }

  SECTION ("Equity Security Time Series Access pt. 3", "[TimeSeries Access (3)]")
    {
      date randomDate1(2016, 1, 5);
      // Use date-based access instead of iterator-based
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 0) == *entry1);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 1) == *entry2);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 2) == *entry3);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 3) == *entry4);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 4) == *entry5);
      REQUIRE (spy.getTimeSeriesEntry(randomDate1, 5) == *entry6);
    }

  SECTION ("Equity Security Time Series Access pt. 4", "[TimeSeries Access (4)]")
    {
      date randomDate1(2016, 1, 4);
      // Use date-based access instead of iterator-based
      date aDate = spy.getDateValue(randomDate1, 0);
      REQUIRE (aDate == entry2->getDateValue());
      REQUIRE (spy.getOpenValue(randomDate1, 1) == entry3->getOpenValue());
      REQUIRE (spy.getHighValue(randomDate1, 2) == entry4->getHighValue());
      REQUIRE (spy.getLowValue(randomDate1, 3) == entry5->getLowValue());
      REQUIRE (spy.getCloseValue(randomDate1, 4) == entry6->getCloseValue());
    }

  SECTION ("Equity Security Time Series Access pt. 5", "[TimeSeries Access (5)]")
    {
      date randomDate1(2016, 1, 6);
      // Use date-based access instead of iterator-based
      REQUIRE_THROWS (spy.getTimeSeriesEntry(randomDate1, 7));
    }

  SECTION ("Equity Security Time Series Access pt. 6", "[TimeSeries Access (6)]")
    {
      // Test with a date that doesn't exist - should throw
      date nonExistentDate(2020, 1, 1);
      REQUIRE_THROWS (spy.getTimeSeriesEntry(nonExistentDate, 1));
    }

  SECTION ("Equity Security Time Series Access pt. 7", "[TimeSeries Access (7)]")
    {
      date randomDate1(2016, 1, 15);
      // Test that accessing a non-existent date throws
      REQUIRE_THROWS (spy.getTimeSeriesEntry(randomDate1));
    }

  SECTION("Trading volume units for equity and futures", "[Security][VolumeUnits]")
    {
      REQUIRE(spy.getTradingVolumeUnits() == TradingVolume::SHARES);
      REQUIRE(corn.getTradingVolumeUnits() == TradingVolume::CONTRACTS);
    }

  SECTION("TickDiv2 values are half of tick size", "[Security][TickDiv2]")
    {
      // Equity tick is 0.01 → tickDiv2 should be 0.005
      DecimalType expectedEquityHalf = DecimalConstants<DecimalType>::EquityTick / createDecimal("2");
      REQUIRE(spy.getTickDiv2() == expectedEquityHalf);
      
      // Corn futures tick is 0.25 → tickDiv2 should be 0.125
      DecimalType expectedCornHalf = createDecimal("0.25") / createDecimal("2");
      REQUIRE(corn.getTickDiv2() == expectedCornHalf);
    }

  SECTION("isDateFound(date) finds or returns false", "[Security][FindByDate]")
    {
      boost::gregorian::date hitDate(2016, 1, 4);
      REQUIRE(spy.isDateFound(hitDate));
      auto entry = spy.getTimeSeriesEntry(hitDate);
      REQUIRE(entry == *entry2);
      
      // miss
      boost::gregorian::date missDate(1990, 1, 1);
      REQUIRE_FALSE(spy.isDateFound(missDate));
    }

  SECTION("Volume access by iterator offset", "[Security][VolumeOffset]")
    {
      boost::gregorian::date testDate(2016, 1, 4);
      // Use date-based access instead of iterator-based
      REQUIRE(spy.getVolumeValue(testDate, 0) == entry2->getVolumeValue());
    }

  SECTION("Copy and assignment for EquitySecurity and FuturesSecurity", "[Security][Copy]")
    {
      // Equity
      EquitySecurity<DecimalType> copySpy(spy);
      REQUIRE(copySpy.getSymbol() == spy.getSymbol());
      REQUIRE(copySpy.getName()   == spy.getName());
      REQUIRE(*(copySpy.getTimeSeries()) == *(spy.getTimeSeries()));

      EquitySecurity<DecimalType> assignSpy = spy;
      REQUIRE(assignSpy.getSymbol() == spy.getSymbol());
      REQUIRE(assignSpy.getName()   == spy.getName());
      REQUIRE(*(assignSpy.getTimeSeries()) == *(spy.getTimeSeries()));

      // Futures
      FuturesSecurity<DecimalType> copyCorn(corn);
      REQUIRE(copyCorn.getSymbol() == corn.getSymbol());
      REQUIRE(copyCorn.getName()   == corn.getName());
      REQUIRE(*(copyCorn.getTimeSeries()) == *(corn.getTimeSeries()));

      FuturesSecurity<DecimalType> assignCorn = corn;
      REQUIRE(assignCorn.getSymbol() == corn.getSymbol());
      REQUIRE(assignCorn.getName()   == corn.getName());
      REQUIRE(*(assignCorn.getTimeSeries()) == *(corn.getTimeSeries()));
    }

  SECTION("resetTimeSeries replaces the underlying series", "[Security][resetTimeSeries]") {
    // Build two distinct mini series
    auto s1 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    auto s2 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);

    auto e1 = createEquityEntry("20200102", "100", "105", "99",  "102",  1000000);
    auto e2 = createEquityEntry("20200103", "102", "106", "101", "104",  1100000);
    s1->addEntry(*e1);
    s1->addEntry(*e2);

    auto e3 = createEquityEntry("20200102", "200", "205", "198", "204",  900000);
    auto e4 = createEquityEntry("20200103", "204", "208", "203", "207",  950000);
    s2->addEntry(*e3);
    s2->addEntry(*e4);

    // Start with s1
    EquitySecurity<DecimalType> sec("TST", "Test Security", s1);

    // Sanity: values come from s1
    boost::gregorian::date d1(2020, 1, 2);
    boost::gregorian::date d2(2020, 1, 3);
    REQUIRE(sec.getCloseValue(d1, 0) == e1->getCloseValue());
    REQUIRE(sec.getCloseValue(d2, 0) == e2->getCloseValue());

    // Swap to s2 and verify values changed
    sec.resetTimeSeries(s2);
    REQUIRE(*(sec.getTimeSeries()) == *s2);
    REQUIRE(sec.getCloseValue(d1, 0) == e3->getCloseValue());
    REQUIRE(sec.getCloseValue(d2, 0) == e4->getCloseValue());
  }

  SECTION("resetTimeSeries throws on null pointer", "[Security][resetTimeSeries][throws]") {
    auto s1 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    auto e1 = createEquityEntry("20200102", "100", "105", "99", "102", 1000000);
    s1->addEntry(*e1);

    EquitySecurity<DecimalType> sec("TST", "Test Security", s1);

    std::shared_ptr<const OHLCTimeSeries<DecimalType>> nullTs;
    REQUIRE_THROWS_AS(sec.resetTimeSeries(nullTs), SecurityException);
  }

  // --- Intraday isDateFound (ptime) ---
  SECTION("Intraday isDateFound returns correct result", "[TimeSeries Access (ptime)]") {
    // pick a known entry
    ptime dt2 = entry2->getDateTime();
    REQUIRE(spy.isDateFound(dt2));
    auto entry = spy.getTimeSeriesEntry(dt2);
    REQUIRE(entry == *entry2);

    // miss a timestamp that isn't in the series
    ptime missingDt = dt2 + hours(3);
    REQUIRE_FALSE(spy.isDateFound(missingDt));
  }

  // --- Intraday getTimeSeriesEntry (ptime) ---
  SECTION("Intraday getTimeSeriesEntry throws on missing timestamp", "[TimeSeries Access (ptime)]") {
    ptime fakeDt(date(2020,1,1), hours(0));
    REQUIRE_THROWS_AS(spy.getTimeSeriesEntry(fakeDt), TimeSeriesDataNotFoundException);
  }

  SECTION("Intraday getTimeSeriesEntry returns entry for existing timestamp", "[TimeSeries Access (ptime)]") {
    ptime dt4 = entry4->getDateTime();
    // Use new API to check entry exists and retrieve it
    REQUIRE(spy.isDateFound(dt4));
    auto retrieved_entry = spy.getTimeSeriesEntry(dt4);
    REQUIRE(retrieved_entry == *entry4);
  }

  // --- Intraday getDateTimeValue (ptime) ---
  SECTION("Intraday getDateTimeValue returns correct datetime or throws on bad offset", "[TimeSeries Access (ptime)]") {
    // locate entry2
    ptime dt2 = entry2->getDateTime();
    
    // Use date-based access instead of iterator-based
    // offset 0 → entry2
    REQUIRE(spy.getDateTimeValue(dt2, 0) == dt2);
    // offset 1 → previous bar (entry3)
    REQUIRE(spy.getDateTimeValue(dt2, 1) == entry3->getDateTime());

    // out-of-bounds offset should throw
    REQUIRE_THROWS(spy.getDateTimeValue(dt2, 10));
  }

  SECTION("Security getIntradayTimeFrameDuration - Intraday series", "[Security][IntradayDuration]")
  {
    // Create intraday time series with 30-minute intervals
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);

    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");
    auto entry4 = createTimeSeriesEntry("20210405", "10:30", "102.0", "104.0", "101.5", "103.0", "2500");

    intradaySeries->addEntry(*entry1);
    intradaySeries->addEntry(*entry2);
    intradaySeries->addEntry(*entry3);
    intradaySeries->addEntry(*entry4);

    EquitySecurity<DecimalType> intradayEquity("SPY", "SPDR S&P 500 ETF", intradaySeries);

    auto duration = intradayEquity.getIntradayTimeFrameDuration();
    REQUIRE(duration == minutes(30));
    REQUIRE(duration.total_seconds() / 60 == 30);
  }

  SECTION("Security getIntradayTimeFrameDuration - Hourly intervals", "[Security][IntradayDuration]")
  {
    // Create intraday time series with 60-minute intervals
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::CONTRACTS);

    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "3700.0", "3710.0", "3690.0", "3705.0", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "10:00", "3705.0", "3720.0", "3700.0", "3715.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "11:00", "3715.0", "3730.0", "3710.0", "3725.0", "2000");

    intradaySeries->addEntry(*entry1);
    intradaySeries->addEntry(*entry2);
    intradaySeries->addEntry(*entry3);

    DecimalType esBigPointValue(createDecimal("50.0"));
    DecimalType esTickValue(createDecimal("0.25"));
    FuturesSecurity<DecimalType> intradayFutures("ES", "E-mini S&P 500", esBigPointValue, esTickValue, intradaySeries);

    auto duration = intradayFutures.getIntradayTimeFrameDuration();
    REQUIRE(duration == hours(1));
    REQUIRE(duration.total_seconds() / 60 == 60);
  }

  SECTION("Security getIntradayTimeFrameDuration - Exception for non-intraday", "[Security][IntradayDuration]")
  {
    // Use existing daily spy and corn series from the test
    REQUIRE_THROWS_AS(spy.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);
    REQUIRE_THROWS_AS(corn.getIntradayTimeFrameDuration(), mkc_timeseries::TimeSeriesException);
  }

  SECTION("Security getIntradayTimeFrameDuration - Various intervals", "[Security][IntradayDuration]")
  {
    // Test 5-minute intervals
    {
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "09:05", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "09:10", "101.0", "103.0", "100.5", "102.0", "2000");

      series->addEntry(*entry1);
      series->addEntry(*entry2);
      series->addEntry(*entry3);

      EquitySecurity<DecimalType> security("TEST", "Test Security", series);
      auto duration = security.getIntradayTimeFrameDuration();
      REQUIRE(duration == minutes(5));
      REQUIRE(duration.total_seconds() / 60 == 5);
    }

    // Test 15-minute intervals
    {
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "09:15", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "09:30", "101.0", "103.0", "100.5", "102.0", "2000");

      series->addEntry(*entry1);
      series->addEntry(*entry2);
      series->addEntry(*entry3);

      EquitySecurity<DecimalType> security("TEST2", "Test Security 2", series);
      auto duration = security.getIntradayTimeFrameDuration();
      REQUIRE(duration == minutes(15));
      REQUIRE(duration.total_seconds() / 60 == 15);
    }

    // Test 90-minute intervals
    {
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::CONTRACTS);
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "3700.0", "3710.0", "3690.0", "3705.0", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "10:30", "3705.0", "3720.0", "3700.0", "3715.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "12:00", "3715.0", "3730.0", "3710.0", "3725.0", "2000");

      series->addEntry(*entry1);
      series->addEntry(*entry2);
      series->addEntry(*entry3);

      DecimalType bigPointValue(createDecimal("50.0"));
      DecimalType tickValue(createDecimal("0.25"));
      FuturesSecurity<DecimalType> security("TEST3", "Test Futures", bigPointValue, tickValue, series);
      auto duration = security.getIntradayTimeFrameDuration();
      REQUIRE(duration == minutes(90));
      REQUIRE(duration.total_seconds() / 60 == 90);
    }
  }

  SECTION("Security getIntradayTimeFrameDuration - Irregular intervals", "[Security][IntradayDuration]")
  {
    // Create intraday time series with mostly 30-minute intervals but one 60-minute gap
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);

    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");
    auto entry4 = createTimeSeriesEntry("20210405", "10:30", "102.0", "104.0", "101.5", "103.0", "2500");
    // Missing 11:00 bar - holiday early close
    auto entry5 = createTimeSeriesEntry("20210405", "12:00", "103.0", "105.0", "102.5", "104.0", "3000");
    auto entry6 = createTimeSeriesEntry("20210405", "12:30", "104.0", "106.0", "103.5", "105.0", "3500");

    series->addEntry(*entry1);
    series->addEntry(*entry2);
    series->addEntry(*entry3);
    series->addEntry(*entry4);
    series->addEntry(*entry5);
    series->addEntry(*entry6);

    EquitySecurity<DecimalType> security("IRREG", "Irregular Security", series);

    // Should return 30 minutes as it's the most common interval (4 occurrences vs 1 occurrence of 90 minutes)
    auto duration = security.getIntradayTimeFrameDuration();
    REQUIRE(duration == minutes(30));
    REQUIRE(duration.total_seconds() / 60 == 30);
  }

  SECTION("Security getIntradayTimeFrameDurationInMinutes - Intraday series", "[Security][IntradayDurationMinutes]")
  {
    // Create intraday time series with 30-minute intervals
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);

    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");
    auto entry4 = createTimeSeriesEntry("20210405", "10:30", "102.0", "104.0", "101.5", "103.0", "2500");

    intradaySeries->addEntry(*entry1);
    intradaySeries->addEntry(*entry2);
    intradaySeries->addEntry(*entry3);
    intradaySeries->addEntry(*entry4);

    EquitySecurity<DecimalType> intradayEquity("SPY", "SPDR S&P 500 ETF", intradaySeries);

    auto durationMinutes = intradayEquity.getIntradayTimeFrameDurationInMinutes();
    REQUIRE(durationMinutes == 30);

    // Verify consistency with time_duration method
    auto duration = intradayEquity.getIntradayTimeFrameDuration();
    REQUIRE(durationMinutes == duration.total_seconds() / 60);
  }

  SECTION("Security getIntradayTimeFrameDurationInMinutes - Hourly intervals", "[Security][IntradayDurationMinutes]")
  {
    // Create intraday time series with 60-minute intervals
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::CONTRACTS);

    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "3700.0", "3710.0", "3690.0", "3705.0", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "10:00", "3705.0", "3720.0", "3700.0", "3715.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "11:00", "3715.0", "3730.0", "3710.0", "3725.0", "2000");

    intradaySeries->addEntry(*entry1);
    intradaySeries->addEntry(*entry2);
    intradaySeries->addEntry(*entry3);

    DecimalType esBigPointValue(createDecimal("50.0"));
    DecimalType esTickValue(createDecimal("0.25"));
    FuturesSecurity<DecimalType> intradayFutures("ES", "E-mini S&P 500", esBigPointValue, esTickValue, intradaySeries);

    auto durationMinutes = intradayFutures.getIntradayTimeFrameDurationInMinutes();
    REQUIRE(durationMinutes == 60);

    // Verify consistency with time_duration method
    auto duration = intradayFutures.getIntradayTimeFrameDuration();
    REQUIRE(durationMinutes == duration.total_seconds() / 60);
  }

  SECTION("Security getIntradayTimeFrameDurationInMinutes - Exception for non-intraday", "[Security][IntradayDurationMinutes]")
  {
    // Use existing daily spy and corn series from the test
    REQUIRE_THROWS_AS(spy.getIntradayTimeFrameDurationInMinutes(), mkc_timeseries::TimeSeriesException);
    REQUIRE_THROWS_AS(corn.getIntradayTimeFrameDurationInMinutes(), mkc_timeseries::TimeSeriesException);
  }

  SECTION("Security getIntradayTimeFrameDurationInMinutes - Various intervals", "[Security][IntradayDurationMinutes]")
  {
    // Test 5-minute intervals
    {
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "09:05", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "09:10", "101.0", "103.0", "100.5", "102.0", "2000");

      series->addEntry(*entry1);
      series->addEntry(*entry2);
      series->addEntry(*entry3);

      EquitySecurity<DecimalType> security("TEST", "Test Security", series);
      REQUIRE(security.getIntradayTimeFrameDurationInMinutes() == 5);
    }

    // Test 15-minute intervals
    {
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "09:15", "100.5", "102.0", "100.0", "101.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "09:30", "101.0", "103.0", "100.5", "102.0", "2000");

      series->addEntry(*entry1);
      series->addEntry(*entry2);
      series->addEntry(*entry3);

      EquitySecurity<DecimalType> security("TEST2", "Test Security 2", series);
      REQUIRE(security.getIntradayTimeFrameDurationInMinutes() == 15);
    }

    // Test 90-minute intervals
    {
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::CONTRACTS);
      auto entry1 = createTimeSeriesEntry("20210405", "09:00", "3700.0", "3710.0", "3690.0", "3705.0", "1000");
      auto entry2 = createTimeSeriesEntry("20210405", "10:30", "3705.0", "3720.0", "3700.0", "3715.0", "1500");
      auto entry3 = createTimeSeriesEntry("20210405", "12:00", "3715.0", "3730.0", "3710.0", "3725.0", "2000");

      series->addEntry(*entry1);
      series->addEntry(*entry2);
      series->addEntry(*entry3);

      DecimalType bigPointValue(createDecimal("50.0"));
      DecimalType tickValue(createDecimal("0.25"));
      FuturesSecurity<DecimalType> security("TEST3", "Test Futures", bigPointValue, tickValue, series);
      REQUIRE(security.getIntradayTimeFrameDurationInMinutes() == 90);
    }
  }

  SECTION("Security getIntradayTimeFrameDurationInMinutes - Irregular intervals", "[Security][IntradayDurationMinutes]")
  {
    // Create intraday time series with mostly 30-minute intervals but one 90-minute gap
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);

    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");
    auto entry4 = createTimeSeriesEntry("20210405", "10:30", "102.0", "104.0", "101.5", "103.0", "2500");
    // Missing 11:00 bar - holiday early close
    auto entry5 = createTimeSeriesEntry("20210405", "12:00", "103.0", "105.0", "102.5", "104.0", "3000");
    auto entry6 = createTimeSeriesEntry("20210405", "12:30", "104.0", "106.0", "103.5", "105.0", "3500");

    series->addEntry(*entry1);
    series->addEntry(*entry2);
    series->addEntry(*entry3);
    series->addEntry(*entry4);
    series->addEntry(*entry5);
    series->addEntry(*entry6);

    EquitySecurity<DecimalType> security("IRREG", "Irregular Security", series);

    // Should return 30 minutes as it's the most common interval (4 occurrences vs 1 occurrence of 90 minutes)
    auto durationMinutes = security.getIntradayTimeFrameDurationInMinutes();
    REQUIRE(durationMinutes == 30);

    // Verify consistency with time_duration method
    auto duration = security.getIntradayTimeFrameDuration();
    REQUIRE(durationMinutes == duration.total_seconds() / 60);
  }

  SECTION("Security getIntradayTimeFrameDurationInMinutes - Consistency tests", "[Security][IntradayDurationMinutes]")
  {
    // Create intraday time series with 15-minute intervals
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);

    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "09:15", "100.5", "102.0", "100.0", "101.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "09:30", "101.0", "103.0", "100.5", "102.0", "2000");
    auto entry4 = createTimeSeriesEntry("20210405", "09:45", "102.0", "104.0", "101.5", "103.0", "2500");

    series->addEntry(*entry1);
    series->addEntry(*entry2);
    series->addEntry(*entry3);
    series->addEntry(*entry4);

    EquitySecurity<DecimalType> security("CONS", "Consistency Test Security", series);

    // Test multiple calls return same result (caching)
    auto duration1 = security.getIntradayTimeFrameDurationInMinutes();
    auto duration2 = security.getIntradayTimeFrameDurationInMinutes();
    auto duration3 = security.getIntradayTimeFrameDurationInMinutes();

    REQUIRE(duration1 == 15);
    REQUIRE(duration1 == duration2);
    REQUIRE(duration2 == duration3);

    // Test consistency with time_duration method
    auto timeDuration = security.getIntradayTimeFrameDuration();
    REQUIRE(duration1 == timeDuration.total_seconds() / 60);
  }
}

// ============================================================================
// ADDITIONAL TESTS TO FILL TESTING GAPS
// ============================================================================

TEST_CASE("Security Copy Constructor and Assignment Operator", "[Security][CopySemantics]")
{
  // Setup test data
  auto entry0 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto entry1 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
  
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry0);
  spySeries->addEntry(*entry1);

  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");
  
  SECTION("EquitySecurity Copy Constructor")
  {
    EquitySecurity<DecimalType> original(equitySymbol, equityName, spySeries);
    EquitySecurity<DecimalType> copied(original);
    
    // Verify all properties are copied correctly
    REQUIRE(copied.getName() == original.getName());
    REQUIRE(copied.getSymbol() == original.getSymbol());
    REQUIRE(copied.getBigPointValue() == original.getBigPointValue());
    REQUIRE(copied.getTick() == original.getTick());
    REQUIRE(copied.isEquitySecurity() == original.isEquitySecurity());
    REQUIRE(copied.isFuturesSecurity() == original.isFuturesSecurity());
    
    // Verify the time series pointer is shared (shallow copy of shared_ptr)
    REQUIRE(copied.getTimeSeries() == original.getTimeSeries());
    REQUIRE(copied.getTimeSeries()->getNumEntries() == original.getTimeSeries()->getNumEntries());
  }
  
  SECTION("EquitySecurity Copy Assignment Operator")
  {
    EquitySecurity<DecimalType> original(equitySymbol, equityName, spySeries);
    
    // Create a different security to assign to
    auto otherEntry = createEquityEntry("20160107", "199.00", "200.00", "198.00", "199.50", 1000000);
    auto otherSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    otherSeries->addEntry(*otherEntry);
    
    EquitySecurity<DecimalType> assigned("OTHER", "Other Security", otherSeries);
    
    // Perform assignment
    assigned = original;
    
    // Verify all properties are assigned correctly
    REQUIRE(assigned.getName() == original.getName());
    REQUIRE(assigned.getSymbol() == original.getSymbol());
    REQUIRE(assigned.getBigPointValue() == original.getBigPointValue());
    REQUIRE(assigned.getTick() == original.getTick());
    REQUIRE(assigned.isEquitySecurity() == original.isEquitySecurity());
    REQUIRE(assigned.isFuturesSecurity() == original.isFuturesSecurity());
    REQUIRE(assigned.getTimeSeries() == original.getTimeSeries());
  }
  
  SECTION("EquitySecurity Self-Assignment")
  {
    EquitySecurity<DecimalType> security(equitySymbol, equityName, spySeries);
    
    // Self-assignment should be safe
    security = security;
    
    // Verify object is still valid
    REQUIRE(security.getName() == equityName);
    REQUIRE(security.getSymbol() == equitySymbol);
    REQUIRE(security.getTimeSeries()->getNumEntries() == 2);
  }
  
  SECTION("FuturesSecurity Copy Constructor")
  {
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178", 
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    FuturesSecurity<DecimalType> original("ES", "E-mini S&P 500", bigPointValue, 
                                          tickValue, futuresSeries);
    FuturesSecurity<DecimalType> copied(original);
    
    // Verify all properties are copied correctly
    REQUIRE(copied.getName() == original.getName());
    REQUIRE(copied.getSymbol() == original.getSymbol());
    REQUIRE(copied.getBigPointValue() == original.getBigPointValue());
    REQUIRE(copied.getTick() == original.getTick());
    REQUIRE(copied.isEquitySecurity() == original.isEquitySecurity());
    REQUIRE(copied.isFuturesSecurity() == original.isFuturesSecurity());
    REQUIRE(copied.getTimeSeries() == original.getTimeSeries());
  }
  
  SECTION("FuturesSecurity Copy Assignment Operator")
  {
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    FuturesSecurity<DecimalType> original("ES", "E-mini S&P 500", bigPointValue,
                                          tickValue, futuresSeries);
    
    // Create a different security
    DecimalType otherBPV(createDecimal("100.0"));
    DecimalType otherTick(createDecimal("0.10"));
    auto otherEntry = createTimeSeriesEntry("19851119", "3710.65307617188", "3722.18872070313",
                                            "3679.89135742188", "3714.49829101563", 0);
    auto otherSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    otherSeries->addEntry(*otherEntry);
    
    FuturesSecurity<DecimalType> assigned("NQ", "E-mini Nasdaq", otherBPV, otherTick, otherSeries);
    
    // Perform assignment
    assigned = original;
    
    // Verify all properties are assigned correctly
    REQUIRE(assigned.getName() == original.getName());
    REQUIRE(assigned.getSymbol() == original.getSymbol());
    REQUIRE(assigned.getBigPointValue() == original.getBigPointValue());
    REQUIRE(assigned.getTick() == original.getTick());
    REQUIRE(assigned.getTimeSeries() == original.getTimeSeries());
  }
  
  SECTION("FuturesSecurity Self-Assignment")
  {
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    FuturesSecurity<DecimalType> security("ES", "E-mini S&P 500", bigPointValue,
                                         tickValue, futuresSeries);
    
    // Self-assignment should be safe
    security = security;
    
    // Verify object is still valid
    REQUIRE(security.getName() == "E-mini S&P 500");
    REQUIRE(security.getSymbol() == "ES");
    REQUIRE(security.getTimeSeries()->getNumEntries() == 1);
  }
}

TEST_CASE("Security Constructor Exception Handling", "[Security][Exceptions]")
{
  std::string symbol("TEST");
  std::string name("Test Security");
  
  SECTION("EquitySecurity throws exception for null time series")
  {
    std::shared_ptr<const OHLCTimeSeries<DecimalType>> nullSeries;
    
    REQUIRE_THROWS_AS(
      EquitySecurity<DecimalType>(symbol, name, nullSeries),
      SecurityException
    );
    
    try {
      EquitySecurity<DecimalType> security(symbol, name, nullSeries);
      FAIL("Expected SecurityException to be thrown");
    } catch (const SecurityException& e) {
      std::string errorMsg(e.what());
      REQUIRE(errorMsg.find("time series object is null") != std::string::npos);
    }
  }
  
  SECTION("FuturesSecurity throws exception for null time series")
  {
    std::shared_ptr<const OHLCTimeSeries<DecimalType>> nullSeries;
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    REQUIRE_THROWS_AS(
      FuturesSecurity<DecimalType>(symbol, name, bigPointValue, tickValue, nullSeries),
      SecurityException
    );
    
    try {
      FuturesSecurity<DecimalType> security(symbol, name, bigPointValue, tickValue, nullSeries);
      FAIL("Expected SecurityException to be thrown");
    } catch (const SecurityException& e) {
      std::string errorMsg(e.what());
      REQUIRE(errorMsg.find("time series object is null") != std::string::npos);
    }
  }
}

TEST_CASE("Security getTradingVolumeUnits", "[Security][VolumeUnits]")
{
  auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  
  SECTION("EquitySecurity returns SHARES")
  {
    auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    spySeries->addEntry(*entry);
    
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
    
    REQUIRE(spy.getTradingVolumeUnits() == TradingVolume::SHARES);
  }
  
  SECTION("FuturesSecurity returns CONTRACTS")
  {
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    FuturesSecurity<DecimalType> es("ES", "E-mini S&P 500", bigPointValue, tickValue, futuresSeries);
    
    REQUIRE(es.getTradingVolumeUnits() == TradingVolume::CONTRACTS);
  }
}

TEST_CASE("Security Round Functions", "[Security][Rounding]")
{
  auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry);
  
  SECTION("roundToTick for EquitySecurity (tick = 0.01)")
  {
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
    
    // Test rounding to nearest tick
    REQUIRE(num::Round2Tick(createDecimal("100.004"), spy.getTick()) == createDecimal("100.00"));
    REQUIRE(num::Round2Tick(createDecimal("100.005"), spy.getTick()) == createDecimal("100.01"));
    REQUIRE(num::Round2Tick(createDecimal("100.014"), spy.getTick()) == createDecimal("100.01"));
    REQUIRE(num::Round2Tick(createDecimal("100.015"), spy.getTick()) == createDecimal("100.02"));
    
    // Edge cases
    REQUIRE(num::Round2Tick(createDecimal("100.00"), spy.getTick()) == createDecimal("100.00"));
    REQUIRE(num::Round2Tick(createDecimal("0.004"), spy.getTick()) == createDecimal("0.00"));
    REQUIRE(num::Round2Tick(createDecimal("0.005"), spy.getTick()) == createDecimal("0.01"));
  }
  
  SECTION("roundDownToTick for EquitySecurity")
  {
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
    
    REQUIRE(std::floor(createDecimal("100.019") / spy.getTick()) * spy.getTick() == createDecimal("100.01"));
    REQUIRE(std::floor(createDecimal("100.011") / spy.getTick()) * spy.getTick() == createDecimal("100.01"));
    REQUIRE(std::floor(createDecimal("100.00") / spy.getTick()) * spy.getTick() == createDecimal("100.00"));
    REQUIRE(std::floor(createDecimal("99.999") / spy.getTick()) * spy.getTick() == createDecimal("99.99"));
  }
  
  SECTION("roundUpToTick for EquitySecurity")
  {
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
    
    REQUIRE(std::ceil(createDecimal("100.001") / spy.getTick()) * spy.getTick() == createDecimal("100.01"));
    REQUIRE(std::ceil(createDecimal("100.011") / spy.getTick()) * spy.getTick() == createDecimal("100.02"));
    REQUIRE(std::ceil(createDecimal("100.00") / spy.getTick()) * spy.getTick() == createDecimal("100.00"));
    REQUIRE(std::ceil(createDecimal("99.991") / spy.getTick()) * spy.getTick() == createDecimal("100.00"));
  }
  
  SECTION("roundToTick for FuturesSecurity (tick = 0.25)")
  {
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    FuturesSecurity<DecimalType> es("ES", "E-mini S&P 500", bigPointValue, tickValue, futuresSeries);
    
    // Test rounding to nearest 0.25 tick
    REQUIRE(num::Round2Tick(createDecimal("100.00"), es.getTick()) == createDecimal("100.00"));
    REQUIRE(num::Round2Tick(createDecimal("100.10"), es.getTick()) == createDecimal("100.00"));
    REQUIRE(num::Round2Tick(createDecimal("100.13"), es.getTick()) == createDecimal("100.25"));
    REQUIRE(num::Round2Tick(createDecimal("100.25"), es.getTick()) == createDecimal("100.25"));
    REQUIRE(num::Round2Tick(createDecimal("100.37"), es.getTick()) == createDecimal("100.25"));
    REQUIRE(num::Round2Tick(createDecimal("100.38"), es.getTick()) == createDecimal("100.50"));
  }
  
  SECTION("roundDownToTick for FuturesSecurity (tick = 0.25)")
  {
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    FuturesSecurity<DecimalType> es("ES", "E-mini S&P 500", bigPointValue, tickValue, futuresSeries);
    
    REQUIRE(std::floor(createDecimal("100.24") / es.getTick()) * es.getTick() == createDecimal("100.00"));
    REQUIRE(std::floor(createDecimal("100.49") / es.getTick()) * es.getTick() == createDecimal("100.25"));
    REQUIRE(std::floor(createDecimal("100.74") / es.getTick()) * es.getTick() == createDecimal("100.50"));
    REQUIRE(std::floor(createDecimal("100.99") / es.getTick()) * es.getTick() == createDecimal("100.75"));
  }
  
  SECTION("roundUpToTick for FuturesSecurity (tick = 0.25)")
  {
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    FuturesSecurity<DecimalType> es("ES", "E-mini S&P 500", bigPointValue, tickValue, futuresSeries);
    
    REQUIRE(std::ceil(createDecimal("100.01") / es.getTick()) * es.getTick() == createDecimal("100.25"));
    REQUIRE(std::ceil(createDecimal("100.26") / es.getTick()) * es.getTick() == createDecimal("100.50"));
    REQUIRE(std::ceil(createDecimal("100.51") / es.getTick()) * es.getTick() == createDecimal("100.75"));
    REQUIRE(std::ceil(createDecimal("100.76") / es.getTick()) * es.getTick() == createDecimal("101.00"));
  }
  
  SECTION("Rounding with custom tick size")
  {
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    // Test with a 0.05 tick size
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.05"));
    FuturesSecurity<DecimalType> custom("TEST", "Test", bigPointValue, tickValue, futuresSeries);
    
    REQUIRE(num::Round2Tick(createDecimal("100.02"), custom.getTick()) == createDecimal("100.00"));
    REQUIRE(num::Round2Tick(createDecimal("100.03"), custom.getTick()) == createDecimal("100.05"));
    REQUIRE(num::Round2Tick(createDecimal("100.07"), custom.getTick()) == createDecimal("100.05"));
    REQUIRE(num::Round2Tick(createDecimal("100.08"), custom.getTick()) == createDecimal("100.10"));
  }
}

TEST_CASE("Security Volume Access Methods", "[Security][Volume]")
{
  SECTION("Equity volume methods")
  {
    auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto entry2 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
    
    auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    spySeries->addEntry(*entry1);
    spySeries->addEntry(*entry2);
    
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
    
    date d1(2016, 1, 6);
    date d2(2016, 1, 5);
    
    // Test getVolume by date
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(spy.getVolumeValue(d1, 0))), spy.getTradingVolumeUnits()) == TradingVolume(142662900, TradingVolume::SHARES));
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(spy.getVolumeValue(d2, 0))), spy.getTradingVolumeUnits()) == TradingVolume(105999900, TradingVolume::SHARES));
    
    // Test volume through time series entry
    auto entry = spy.getTimeSeriesEntry(d1);
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(entry.getVolumeValue())), TradingVolume::SHARES) == TradingVolume(142662900, TradingVolume::SHARES));
  }
  
  SECTION("Futures volume methods")
  {
    auto entry1 = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                        "3656.81982", "3672.20068", 50000);
    auto entry2 = createTimeSeriesEntry("19851119", "3710.65307617188", "3722.18872070313",
                                        "3679.89135742188", "3714.49829101563", 75000);
    
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*entry1);
    futuresSeries->addEntry(*entry2);
    
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    FuturesSecurity<DecimalType> es("ES", "E-mini S&P 500", bigPointValue, tickValue, futuresSeries);
    
    date d1(1985, 11, 18);
    date d2(1985, 11, 19);
    
    // Test getVolume by date
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(es.getVolumeValue(d1, 0))), es.getTradingVolumeUnits()) == TradingVolume(50000, TradingVolume::CONTRACTS));
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(es.getVolumeValue(d2, 0))), es.getTradingVolumeUnits()) == TradingVolume(75000, TradingVolume::CONTRACTS));
  }
  
  SECTION("Volume access with offset")
  {
    auto entry1 = createEquityEntry("20160104", "200.49", "201.03", "198.59", "201.02", 222353400);
    auto entry2 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
    auto entry3 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    
    auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    spySeries->addEntry(*entry1);
    spySeries->addEntry(*entry2);
    spySeries->addEntry(*entry3);
    
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
    
    date baseDate(2016, 1, 6);
    
    // Test volume access with offset
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(spy.getVolumeValue(baseDate, 0))), spy.getTradingVolumeUnits()) == TradingVolume(142662900, TradingVolume::SHARES));
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(spy.getVolumeValue(baseDate, 1))), spy.getTradingVolumeUnits()) == TradingVolume(105999900, TradingVolume::SHARES));
    REQUIRE(TradingVolume(static_cast<volume_t>(num::to_double(spy.getVolumeValue(baseDate, 2))), spy.getTradingVolumeUnits()) == TradingVolume(222353400, TradingVolume::SHARES));
  }
}

TEST_CASE("Security Multiple Instances", "[Security][MultipleInstances]")
{
  SECTION("Multiple securities sharing same time series")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto sharedSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    sharedSeries->addEntry(*entry);
    
    EquitySecurity<DecimalType> spy1("SPY", "SPDR S&P 500 ETF", sharedSeries);
    EquitySecurity<DecimalType> spy2("SPY", "SPDR S&P 500 ETF", sharedSeries);
    
    // Both should share the same time series (shared_ptr semantics)
    REQUIRE(spy1.getTimeSeries() == spy2.getTimeSeries());
    REQUIRE(spy1.getTimeSeries()->getNumEntries() == spy2.getTimeSeries()->getNumEntries());
    
    // Verify shared_ptr reference count behavior - store references to avoid temporary copies
    auto ts1 = spy1.getTimeSeries();
    auto ts2 = spy2.getTimeSeries();
    REQUIRE(ts1.use_count() == ts2.use_count());
  }
  
  SECTION("Multiple securities with different time series")
  {
    auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series1 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series1->addEntry(*entry1);
    
    auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
    auto series2 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series2->addEntry(*entry2);
    
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", series1);
    EquitySecurity<DecimalType> aapl("AAPL", "Apple Inc.", series2);
    
    // Different securities should have different time series
    REQUIRE(spy.getTimeSeries() != aapl.getTimeSeries());
    REQUIRE(spy.getSymbol() != aapl.getSymbol());
    
    // But both should work independently
    date d(2016, 1, 6);
    REQUIRE(spy.getCloseValue(d, 0) == createDecimal("198.82"));
    REQUIRE(aapl.getCloseValue(d, 0) == createDecimal("90.75"));
  }
  
  SECTION("Copy creates independent object with shared time series")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series->addEntry(*entry);
    
    EquitySecurity<DecimalType> original("SPY", "SPDR S&P 500 ETF", series);
    EquitySecurity<DecimalType> copy(original);
    
    // Should share time series
    REQUIRE(original.getTimeSeries() == copy.getTimeSeries());
    
    // But are independent objects
    REQUIRE(&original != &copy);
  }
}

TEST_CASE("Security Iterator Const-Correctness", "[Security][Iterators]")
{
  auto entry1 = createEquityEntry("20160104", "200.49", "201.03", "198.59", "201.02", 222353400);
  auto entry2 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
  auto entry3 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  spySeries->addEntry(*entry2);
  spySeries->addEntry(*entry3);
  
  const EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
  
  SECTION("Const iterators can be obtained from const object")
  {
    Security<DecimalType>::ConstSortedIterator it = spy.beginSortedEntries();
    Security<DecimalType>::ConstSortedIterator end = spy.endSortedEntries();
    
    // Should be able to iterate
    int count = 0;
    for (; it != end; ++it) {
      count++;
      // Should be able to read but not modify
      auto entry = *it;
      REQUIRE(entry.getOpenValue() > DecimalConstants<DecimalType>::DecimalZero);
    }
    REQUIRE(count == 3);
  }
  
  SECTION("Iterator traversal")
  {
    auto it = spy.beginSortedEntries();
    auto end = spy.endSortedEntries();
    
    // First entry should be earliest date
    REQUIRE((*it) == *entry1);
    
    ++it;
    REQUIRE((*it) == *entry2);
    
    ++it;
    REQUIRE((*it) == *entry3);
    
    ++it;
    REQUIRE(it == end);
  }
}

TEST_CASE("Security Time Series Offset Edge Cases", "[Security][OffsetEdgeCases]")
{
  auto entry1 = createEquityEntry("20160104", "200.49", "201.03", "198.59", "201.02", 222353400);
  auto entry2 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
  auto entry3 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  spySeries->addEntry(*entry2);
  spySeries->addEntry(*entry3);
  
  EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
  
  SECTION("Negative offset (future dates)")
  {
    date baseDate(2016, 1, 5); // Middle date
    
    // Offset -1 should give next date (later in time)
    auto entry = spy.getTimeSeriesEntry(baseDate, -1);
    REQUIRE(entry == *entry3);
    
    // Offset 0 should give same date
    entry = spy.getTimeSeriesEntry(baseDate, 0);
    REQUIRE(entry == *entry2);
  }
  
  SECTION("Maximum positive offset")
  {
    date latestDate(2016, 1, 6);
    
    // Maximum offset should be 2 (to reach first entry)
    auto entry = spy.getTimeSeriesEntry(latestDate, 2);
    REQUIRE(entry == *entry1);
    
    // Offset beyond range should throw
    REQUIRE_THROWS_AS(
      spy.getTimeSeriesEntry(latestDate, 3),
      TimeSeriesOffsetOutOfRangeException
    );
  }
  
  SECTION("Maximum negative offset")
  {
    date earliestDate(2016, 1, 4);
    
    // Maximum negative offset should be -2 (to reach last entry)
    auto entry = spy.getTimeSeriesEntry(earliestDate, -2);
    REQUIRE(entry == *entry3);
    
    // Offset beyond range should throw
    REQUIRE_THROWS_AS(
      spy.getTimeSeriesEntry(earliestDate, -3),
      TimeSeriesOffsetOutOfRangeException
    );
  }
  
  SECTION("Zero offset at boundaries")
  {
    date earliestDate(2016, 1, 4);
    date latestDate(2016, 1, 6);
    
    // Zero offset should work at boundaries
    REQUIRE(spy.getTimeSeriesEntry(earliestDate, 0) == *entry1);
    REQUIRE(spy.getTimeSeriesEntry(latestDate, 0) == *entry3);
  }
}

TEST_CASE("Security getOpen/High/Low/Close with Offset", "[Security][PriceAccessWithOffset]")
{
  auto entry1 = createEquityEntry("20160104", "200.49", "201.03", "198.59", "201.02", 222353400);
  auto entry2 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
  auto entry3 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  spySeries->addEntry(*entry2);
  spySeries->addEntry(*entry3);
  
  EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
  
  date baseDate(2016, 1, 6); // Latest date
  
  SECTION("Price access with positive offset (earlier dates)")
  {
    // Offset 0 - current date
    REQUIRE(spy.getOpenValue(baseDate, 0) == createDecimal("198.34"));
    REQUIRE(spy.getHighValue(baseDate, 0) == createDecimal("200.06"));
    REQUIRE(spy.getLowValue(baseDate, 0) == createDecimal("197.60"));
    REQUIRE(spy.getCloseValue(baseDate, 0) == createDecimal("198.82"));
    
    // Offset 1 - one day earlier
    REQUIRE(spy.getOpenValue(baseDate, 1) == createDecimal("201.40"));
    REQUIRE(spy.getHighValue(baseDate, 1) == createDecimal("201.90"));
    REQUIRE(spy.getLowValue(baseDate, 1) == createDecimal("200.05"));
    REQUIRE(spy.getCloseValue(baseDate, 1) == createDecimal("201.36"));
    
    // Offset 2 - two days earlier
    REQUIRE(spy.getOpenValue(baseDate, 2) == createDecimal("200.49"));
    REQUIRE(spy.getHighValue(baseDate, 2) == createDecimal("201.03"));
    REQUIRE(spy.getLowValue(baseDate, 2) == createDecimal("198.59"));
    REQUIRE(spy.getCloseValue(baseDate, 2) == createDecimal("201.02"));
  }
  
  SECTION("Price access from middle date with both directions")
  {
    date middleDate(2016, 1, 5);
    
    // Offset -1 (next day, later in time)
    REQUIRE(spy.getCloseValue(middleDate, -1) == createDecimal("198.82"));
    
    // Offset 0 (same day)
    REQUIRE(spy.getCloseValue(middleDate, 0) == createDecimal("201.36"));
    
    // Offset 1 (previous day, earlier in time)
    REQUIRE(spy.getCloseValue(middleDate, 1) == createDecimal("201.02"));
  }
}

TEST_CASE("Security Intraday Time Series Methods", "[Security][IntradayMethods]")
{
  SECTION("isPtimeFound for intraday series")
  {
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
    
    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
    
    intradaySeries->addEntry(*entry1);
    intradaySeries->addEntry(*entry2);
    
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", intradaySeries);
    
    ptime pt1(date(2021, 4, 5), hours(9) + minutes(0));
    ptime pt2(date(2021, 4, 5), hours(9) + minutes(30));
    ptime ptNotFound(date(2021, 4, 5), hours(10) + minutes(0));
    
    REQUIRE(spy.isDateFound(pt1));
    REQUIRE(spy.isDateFound(pt2));
    REQUIRE_FALSE(spy.isDateFound(ptNotFound));
  }
  
  SECTION("Price access by ptime")
  {
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
    
    auto entry = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
    intradaySeries->addEntry(*entry);
    
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", intradaySeries);
    
    ptime pt(date(2021, 4, 5), hours(9) + minutes(30));
    
    REQUIRE(spy.getOpenValue(pt, 0) == createDecimal("100.5"));
    REQUIRE(spy.getHighValue(pt, 0) == createDecimal("102.0"));
    REQUIRE(spy.getLowValue(pt, 0) == createDecimal("100.0"));
    REQUIRE(spy.getCloseValue(pt, 0) == createDecimal("101.0"));
  }
  
  SECTION("getTimeSeriesEntry by ptime with offset")
  {
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::SHARES);
    
    auto entry1 = createTimeSeriesEntry("20210405", "09:00", "100.0", "101.0", "99.0", "100.5", "1000");
    auto entry2 = createTimeSeriesEntry("20210405", "09:30", "100.5", "102.0", "100.0", "101.0", "1500");
    auto entry3 = createTimeSeriesEntry("20210405", "10:00", "101.0", "103.0", "100.5", "102.0", "2000");
    
    intradaySeries->addEntry(*entry1);
    intradaySeries->addEntry(*entry2);
    intradaySeries->addEntry(*entry3);
    
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", intradaySeries);
    
    ptime basePt(date(2021, 4, 5), hours(10) + minutes(0));
    
    // Offset 0 - current bar
    auto entry = spy.getTimeSeriesEntry(basePt, 0);
    REQUIRE(entry.getCloseValue() == createDecimal("102.0"));
    
    // Offset 1 - one bar earlier
    entry = spy.getTimeSeriesEntry(basePt, 1);
    REQUIRE(entry.getCloseValue() == createDecimal("101.0"));
    
    // Offset 2 - two bars earlier
    entry = spy.getTimeSeriesEntry(basePt, 2);
    REQUIRE(entry.getCloseValue() == createDecimal("100.5"));
  }
}

TEST_CASE("Security Clone with Different Time Series", "[Security][CloneWithDifferentTimeSeries]")
{
  SECTION("EquitySecurity clone with subset of data")
  {
    auto entry1 = createEquityEntry("20160104", "200.49", "201.03", "198.59", "201.02", 222353400);
    auto entry2 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
    auto entry3 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    
    auto fullSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    fullSeries->addEntry(*entry1);
    fullSeries->addEntry(*entry2);
    fullSeries->addEntry(*entry3);
    
    auto subsetSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    subsetSeries->addEntry(*entry2);
    subsetSeries->addEntry(*entry3);
    
    EquitySecurity<DecimalType> original("SPY", "SPDR S&P 500 ETF", fullSeries);
    auto cloned = original.clone(subsetSeries);
    
    // Metadata should be the same
    REQUIRE(cloned->getSymbol() == original.getSymbol());
    REQUIRE(cloned->getName() == original.getName());
    REQUIRE(cloned->getBigPointValue() == original.getBigPointValue());
    REQUIRE(cloned->getTick() == original.getTick());
    
    // Time series should be different
    REQUIRE(cloned->getTimeSeries() != original.getTimeSeries());
    REQUIRE(cloned->getTimeSeries()->getNumEntries() == 2);
    REQUIRE(original.getTimeSeries()->getNumEntries() == 3);
    
    // Cloned series should only have subset of dates
    REQUIRE_FALSE(cloned->isDateFound(date(2016, 1, 4)));
    REQUIRE(cloned->isDateFound(date(2016, 1, 5)));
    REQUIRE(cloned->isDateFound(date(2016, 1, 6)));
  }
  
  SECTION("FuturesSecurity clone with different timeframe")
  {
    auto dailyEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                            "3656.81982", "3672.20068", 50000);
    auto dailySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    dailySeries->addEntry(*dailyEntry);
    
    auto intradayEntry = createTimeSeriesEntry("19851118", "09:00", "3664.51025", "3687.58178",
                                               "3656.81982", "3672.20068", "10000");
    auto intradaySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::INTRADAY, TradingVolume::CONTRACTS);
    intradaySeries->addEntry(*intradayEntry);
    
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    FuturesSecurity<DecimalType> dailyEs("ES", "E-mini S&P 500", bigPointValue, tickValue, dailySeries);
    auto intradayEs = dailyEs.clone(intradaySeries);
    
    // Metadata should be the same
    REQUIRE(intradayEs->getSymbol() == dailyEs.getSymbol());
    REQUIRE(intradayEs->getBigPointValue() == dailyEs.getBigPointValue());
    REQUIRE(intradayEs->getTick() == dailyEs.getTick());
    
    // Time series should be different
    REQUIRE(intradayEs->getTimeSeries() != dailyEs.getTimeSeries());
    REQUIRE(intradayEs->getTimeSeries()->getTimeFrame() == TimeFrame::INTRADAY);
    REQUIRE(dailyEs.getTimeSeries()->getTimeFrame() == TimeFrame::DAILY);
  }
}

TEST_CASE("Security Move Constructor and Move Assignment Operator", "[Security][MoveSemantics]")
{
  // Setup test data
  auto entry0 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto entry1 = createEquityEntry("20160105", "201.40", "201.90", "200.05", "201.36", 105999900);
  
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry0);
  spySeries->addEntry(*entry1);

  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");
  
  SECTION("EquitySecurity Move Constructor")
  {
    EquitySecurity<DecimalType> original(equitySymbol, equityName, spySeries);
    
    // Store original values for comparison
    auto originalSymbol = original.getSymbol();
    auto originalName = original.getName();
    auto originalBPV = original.getBigPointValue();
    auto originalTick = original.getTick();
    auto originalTimeSeries = original.getTimeSeries();
    auto originalNumEntries = original.getTimeSeries()->getNumEntries();
    
    // Move construct
    EquitySecurity<DecimalType> moved(std::move(original));
    
    // Verify all properties were moved correctly
    REQUIRE(moved.getSymbol() == originalSymbol);
    REQUIRE(moved.getName() == originalName);
    REQUIRE(moved.getBigPointValue() == originalBPV);
    REQUIRE(moved.getTick() == originalTick);
    REQUIRE(moved.getTimeSeries() == originalTimeSeries);
    REQUIRE(moved.getTimeSeries()->getNumEntries() == originalNumEntries);
    REQUIRE(moved.isEquitySecurity());
    REQUIRE_FALSE(moved.isFuturesSecurity());
    
    // Verify moved object is fully functional
    date d(2016, 1, 6);
    REQUIRE(moved.getCloseValue(d, 0) == createDecimal("198.82"));
  }
  
  SECTION("EquitySecurity Move Assignment Operator")
  {
    EquitySecurity<DecimalType> original(equitySymbol, equityName, spySeries);
    
    // Store original values
    auto originalSymbol = original.getSymbol();
    auto originalName = original.getName();
    auto originalTimeSeries = original.getTimeSeries();
    
    // Create a different security to assign to
    auto otherEntry = createEquityEntry("20160107", "199.00", "200.00", "198.00", "199.50", 1000000);
    auto otherSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    otherSeries->addEntry(*otherEntry);
    
    EquitySecurity<DecimalType> target("OTHER", "Other Security", otherSeries);
    
    // Perform move assignment
    target = std::move(original);
    
    // Verify all properties were moved correctly
    REQUIRE(target.getSymbol() == originalSymbol);
    REQUIRE(target.getName() == originalName);
    REQUIRE(target.getTimeSeries() == originalTimeSeries);
    REQUIRE(target.isEquitySecurity());
    
    // Verify target is fully functional
    date d(2016, 1, 6);
    REQUIRE(target.getCloseValue(d, 0) == createDecimal("198.82"));
  }
  
  SECTION("EquitySecurity Move Self-Assignment")
  {
    EquitySecurity<DecimalType> security(equitySymbol, equityName, spySeries);
    
    // Store original values
    auto originalSymbol = security.getSymbol();
    auto originalName = security.getName();
    // Self move-assignment should be safe (though not recommended in practice)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wself-move"
    security = std::move(security);
    #pragma GCC diagnostic pop
    
    // Object should still be valid (implementation-defined state, but should not crash)
    // At minimum, the object should be in a valid state
    REQUIRE_NOTHROW(security.getSymbol());
    REQUIRE_NOTHROW(security.getName());
  }
  
  SECTION("FuturesSecurity Move Constructor")
  {
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178", 
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    FuturesSecurity<DecimalType> original("ES", "E-mini S&P 500", bigPointValue, 
                                          tickValue, futuresSeries);
    
    // Store original values
    auto originalSymbol = original.getSymbol();
    auto originalName = original.getName();
    auto originalBPV = original.getBigPointValue();
    auto originalTick = original.getTick();
    auto originalTimeSeries = original.getTimeSeries();
    
    // Move construct
    FuturesSecurity<DecimalType> moved(std::move(original));
    
    // Verify all properties were moved correctly
    REQUIRE(moved.getSymbol() == originalSymbol);
    REQUIRE(moved.getName() == originalName);
    REQUIRE(moved.getBigPointValue() == originalBPV);
    REQUIRE(moved.getTick() == originalTick);
    REQUIRE(moved.getTimeSeries() == originalTimeSeries);
    REQUIRE_FALSE(moved.isEquitySecurity());
    REQUIRE(moved.isFuturesSecurity());
    
    // Verify moved object is fully functional
    date d(1985, 11, 18);
    REQUIRE(moved.getCloseValue(d, 0) == createDecimal("3672.20068"));
  }
  
  SECTION("FuturesSecurity Move Assignment Operator")
  {
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    FuturesSecurity<DecimalType> original("ES", "E-mini S&P 500", bigPointValue,
                                          tickValue, futuresSeries);
    
    // Store original values
    auto originalSymbol = original.getSymbol();
    auto originalBPV = original.getBigPointValue();
    auto originalTick = original.getTick();
    
    // Create a different security
    DecimalType otherBPV(createDecimal("100.0"));
    DecimalType otherTick(createDecimal("0.10"));
    auto otherEntry = createTimeSeriesEntry("19851119", "3710.65307617188", "3722.18872070313",
                                            "3679.89135742188", "3714.49829101563", 0);
    auto otherSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    otherSeries->addEntry(*otherEntry);
    
    FuturesSecurity<DecimalType> target("NQ", "E-mini Nasdaq", otherBPV, otherTick, otherSeries);
    
    // Perform move assignment
    target = std::move(original);
    
    // Verify all properties were moved correctly
    REQUIRE(target.getSymbol() == originalSymbol);
    REQUIRE(target.getBigPointValue() == originalBPV);
    REQUIRE(target.getTick() == originalTick);
    REQUIRE(target.isFuturesSecurity());
    
    // Verify target is fully functional
    date d(1985, 11, 18);
    REQUIRE(target.getCloseValue(d, 0) == createDecimal("3672.20068"));
  }
  
  SECTION("FuturesSecurity Move Self-Assignment")
  {
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    FuturesSecurity<DecimalType> security("ES", "E-mini S&P 500", bigPointValue,
                                         tickValue, futuresSeries);
    
    // Self move-assignment should be safe
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wself-move"
    security = std::move(security);
    #pragma GCC diagnostic pop
    
    // Object should still be in a valid state
    REQUIRE_NOTHROW(security.getSymbol());
    REQUIRE_NOTHROW(security.getBigPointValue());
  }
}

TEST_CASE("Move Semantics with Containers", "[Security][MoveSemantics][Containers]")
{
  SECTION("EquitySecurity in vector with move semantics")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    spySeries->addEntry(*entry);
    
    std::vector<EquitySecurity<DecimalType>> securities;
    
    // Move construct into vector (uses move constructor)
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", spySeries);
    securities.push_back(std::move(spy));
    
    // Verify the security in the vector is functional
    REQUIRE(securities.size() == 1);
    REQUIRE(securities[0].getSymbol() == "SPY");
    REQUIRE(securities[0].getName() == "SPDR S&P 500 ETF");
    
    date d(2016, 1, 6);
    REQUIRE(securities[0].getCloseValue(d, 0) == createDecimal("198.82"));
  }
  
  SECTION("FuturesSecurity in vector with move semantics")
  {
    auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                              "3656.81982", "3672.20068", 0);
    auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    futuresSeries->addEntry(*futuresEntry);
    
    DecimalType bigPointValue(createDecimal("50.0"));
    DecimalType tickValue(createDecimal("0.25"));
    
    std::vector<FuturesSecurity<DecimalType>> securities;
    
    // Move construct into vector
    FuturesSecurity<DecimalType> es("ES", "E-mini S&P 500", bigPointValue, tickValue, futuresSeries);
    securities.push_back(std::move(es));
    
    // Verify the security in the vector is functional
    REQUIRE(securities.size() == 1);
    REQUIRE(securities[0].getSymbol() == "ES");
    REQUIRE(securities[0].getBigPointValue() == bigPointValue);
    
    date d(1985, 11, 18);
    REQUIRE(securities[0].getCloseValue(d, 0) == createDecimal("3672.20068"));
  }
  
  SECTION("Multiple securities moved into vector")
  {
    auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series1 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series1->addEntry(*entry1);
    
    auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
    auto series2 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series2->addEntry(*entry2);
    
    std::vector<EquitySecurity<DecimalType>> portfolio;
    
    // Move multiple securities
    EquitySecurity<DecimalType> spy("SPY", "SPDR S&P 500 ETF", series1);
    EquitySecurity<DecimalType> aapl("AAPL", "Apple Inc.", series2);
    
    portfolio.push_back(std::move(spy));
    portfolio.push_back(std::move(aapl));
    
    REQUIRE(portfolio.size() == 2);
    REQUIRE(portfolio[0].getSymbol() == "SPY");
    REQUIRE(portfolio[1].getSymbol() == "AAPL");
    
    date d(2016, 1, 6);
    REQUIRE(portfolio[0].getCloseValue(d, 0) == createDecimal("198.82"));
    REQUIRE(portfolio[1].getCloseValue(d, 0) == createDecimal("90.75"));
  }
}

TEST_CASE("Move Semantics with Factory Functions", "[Security][MoveSemantics][Factory]")
{
  SECTION("Factory function returning EquitySecurity by value (RVO/move)")
  {
    auto createTestSecurity = []() -> EquitySecurity<DecimalType> {
      auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
      series->addEntry(*entry);
      
      // Return by value - will use RVO or move constructor
      return EquitySecurity<DecimalType>("SPY", "SPDR S&P 500 ETF", series);
    };
    
    auto security = createTestSecurity();
    
    // Verify the returned security is functional
    REQUIRE(security.getSymbol() == "SPY");
    REQUIRE(security.getName() == "SPDR S&P 500 ETF");
    
    date d(2016, 1, 6);
    REQUIRE(security.getCloseValue(d, 0) == createDecimal("198.82"));
  }
  
  SECTION("Factory function returning FuturesSecurity by value")
  {
    auto createFutures = []() -> FuturesSecurity<DecimalType> {
      auto entry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                         "3656.81982", "3672.20068", 0);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
      series->addEntry(*entry);
      
      DecimalType bpv(createDecimal("50.0"));
      DecimalType tick(createDecimal("0.25"));
      
      return FuturesSecurity<DecimalType>("ES", "E-mini S&P 500", bpv, tick, series);
    };
    
    auto security = createFutures();
    
    // Verify functionality
    REQUIRE(security.getSymbol() == "ES");
    REQUIRE(security.getBigPointValue() == createDecimal("50.0"));
    
    date d(1985, 11, 18);
    REQUIRE(security.getCloseValue(d, 0) == createDecimal("3672.20068"));
  }
}

TEST_CASE("Move Semantics Performance Characteristics", "[Security][MoveSemantics][Performance]")
{
  SECTION("Move is more efficient than copy for large symbol names")
  {
    // Create securities with intentionally long strings to demonstrate move benefits
    std::string longSymbol(1000, 'A'); // 1000-character symbol
    std::string longName(1000, 'B');   // 1000-character name
    
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series->addEntry(*entry);
    
    EquitySecurity<DecimalType> original(longSymbol, longName, series);
    
    // Move should be fast regardless of string length
    EquitySecurity<DecimalType> moved(std::move(original));
    
    // Verify the long strings were moved
    REQUIRE(moved.getSymbol() == longSymbol);
    REQUIRE(moved.getName() == longName);
  }
  
  SECTION("Shared_ptr is moved, not copied")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series->addEntry(*entry);
    
    EquitySecurity<DecimalType> original("SPY", "SPDR S&P 500 ETF", series);
    long afterConstructUseCount = series.use_count();
    
    // Move the security
    EquitySecurity<DecimalType> moved(std::move(original));
    long afterMoveUseCount = series.use_count();
    
    // After move, use count should remain the same (ownership transferred)
    REQUIRE(afterMoveUseCount == afterConstructUseCount);
  }
}

TEST_CASE("Move Semantics Edge Cases", "[Security][MoveSemantics][EdgeCases]")
{
  SECTION("Move empty/minimal EquitySecurity")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series->addEntry(*entry);
    
    // Create security with minimal data
    EquitySecurity<DecimalType> original("", "", series);
    EquitySecurity<DecimalType> moved(std::move(original));
    
    // Should work even with empty strings
    REQUIRE(moved.getSymbol() == "");
    REQUIRE(moved.getName() == "");
    REQUIRE(moved.getTimeSeries()->getNumEntries() == 1);
  }
  
  SECTION("Move FuturesSecurity with extreme values")
  {
    auto entry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178",
                                       "3656.81982", "3672.20068", 0);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
    series->addEntry(*entry);
    
    // Extreme values
    DecimalType hugeBPV(createDecimal("999999.99"));
    DecimalType tinyTick(createDecimal("0.00000001"));
    
    FuturesSecurity<DecimalType> original("TEST", "Test", hugeBPV, tinyTick, series);
    FuturesSecurity<DecimalType> moved(std::move(original));
    
    // Values should be preserved exactly
    REQUIRE(moved.getBigPointValue() == hugeBPV);
    REQUIRE(moved.getTick() == tinyTick);
  }
  
  SECTION("Chain of moves")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series->addEntry(*entry);
    
    EquitySecurity<DecimalType> sec1("SPY", "SPDR S&P 500 ETF", series);
    EquitySecurity<DecimalType> sec2(std::move(sec1));
    EquitySecurity<DecimalType> sec3(std::move(sec2));
    EquitySecurity<DecimalType> sec4(std::move(sec3));
    
    // Final security should still be functional
    REQUIRE(sec4.getSymbol() == "SPY");
    REQUIRE(sec4.getName() == "SPDR S&P 500 ETF");
    
    date d(2016, 1, 6);
    REQUIRE(sec4.getCloseValue(d, 0) == createDecimal("198.82"));
  }
}

TEST_CASE("Move Semantics Compatibility with Existing Code", "[Security][MoveSemantics][Compatibility]")
{
  SECTION("Clone returns can be moved")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series->addEntry(*entry);
    
    EquitySecurity<DecimalType> original("SPY", "SPDR S&P 500 ETF", series);
    
    // Clone returns shared_ptr, but the underlying object can be moved if dereferenced
    auto clonedPtr = original.clone(series);
    
    // Verify clone is functional
    REQUIRE(clonedPtr->getSymbol() == "SPY");
  }
  
  SECTION("Move semantics don't break polymorphism")
  {
    auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
    auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    series->addEntry(*entry);
    
    EquitySecurity<DecimalType> equity("SPY", "SPDR S&P 500 ETF", series);
    
    // Move into base class container via pointer
    std::vector<std::shared_ptr<Security<DecimalType>>> portfolio;
    portfolio.push_back(std::make_shared<EquitySecurity<DecimalType>>(std::move(equity)));
    
    // Polymorphic behavior should still work
    REQUIRE(portfolio[0]->isEquitySecurity());
    REQUIRE_FALSE(portfolio[0]->isFuturesSecurity());
  }
}
