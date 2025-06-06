#include <catch2/catch_test_macros.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Security.h"
#include "SecurityFactory.h"
#include "TestUtils.h"

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
