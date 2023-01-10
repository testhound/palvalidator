#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "HistoricDataReader.h"
#include "TimeShiftedMultiTimeSeriesCreator.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

typedef dec::decimal<7> DecimalType;

using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::time_duration;

void findMissingDatesBetweenSeries(std::shared_ptr<OHLCTimeSeries<DecimalType> >series1,
				   std::shared_ptr<OHLCTimeSeries<DecimalType>> series2)
{
    auto outer_it = series1->beginSortedAccess();
   // OHLCTimeSeriesEntry<DecimalType> entry1, entry2;
    boost::gregorian::date entry1_date;
    for(; outer_it != series1->endSortedAccess(); outer_it++)
      {
	auto entry1 = outer_it->second;
	entry1_date = entry1.getDateValue();
	if (series2->isDateFound(entry1_date) == false)
	  std::cout << "date: " << entry1_date << "not found in series2" << std::endl;
      }

}

TEST_CASE ("TimeShiftedMultiTimeSeriesCreator operations", "[DailyTimeShiftedMultiTimeSeriesCreator]")
{
  std::string equitySymbol("SSO");
  std::string equityName("ProShares Ultra S&P500");

  auto ssoDailyFile = HistoricDataReaderFactory<DecimalType>::createHistoricDataReader("SSO_RAD_Daily.txt",
										       HistoricDataReader<DecimalType>::TRADESTATION,
										       TimeFrame::DAILY,
										       TradingVolume::SHARES,
										       DecimalConstants<DecimalType>::EquityTick);
  ssoDailyFile->read();

  auto ssoIntradayFile = HistoricDataReaderFactory<DecimalType>::createHistoricDataReader("SSO_RAD_Hourly.txt",
											  HistoricDataReader<DecimalType>::TRADESTATION,
											  TimeFrame::INTRADAY,
											  TradingVolume::SHARES,
											  DecimalConstants<DecimalType>::EquityTick);
 
  ssoIntradayFile->read();

  auto ssoDaily = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, equityName, ssoDailyFile->getTimeSeries());
  auto ssoHourly = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, equityName, ssoIntradayFile->getTimeSeries());

  auto timeShiftedCreator = std::make_shared<DailyTimeShiftedMultiTimeSeriesCreator<DecimalType>>(ssoIntradayFile,
												  ssoDaily);
  timeShiftedCreator->createShiftedTimeSeries();

  SECTION ("DailyTimeShiftedMultiTimeSeriesCreator", "[DailyTimeShiftedMultiTimeSeriesCreator]")
  {
    REQUIRE(timeShiftedCreator->numTimeSeriesCreated() == 7);

    std::shared_ptr<OHLCTimeSeries<Decimal>> outerTimeSeries;
    std::shared_ptr<OHLCTimeSeries<Decimal>> innerTimeSeries;

    auto outer_it = timeShiftedCreator->beginShiftedTimeSeries();
    int outer_time_series_num = 0;
    int inner_time_series_num = 0;

    // Make sure the generated time series are the same in number of elements, etc but
    // different in the actual elements
    for(; outer_it != timeShiftedCreator->endShiftedTimeSeries(); outer_it++)
      {
	outer_time_series_num++;
	outerTimeSeries = *outer_it;
	auto inner_it = outer_it + 1;
	inner_time_series_num = outer_time_series_num;

	for(; inner_it != timeShiftedCreator->endShiftedTimeSeries(); inner_it++)
	  {
	    inner_time_series_num++;
	    innerTimeSeries = *inner_it;

	    REQUIRE(outerTimeSeries->getTimeFrame() == innerTimeSeries->getTimeFrame());
	    if (outerTimeSeries->getNumEntries() != innerTimeSeries->getNumEntries())
	      findMissingDatesBetweenSeries(outerTimeSeries, innerTimeSeries);
	    
	    REQUIRE(outerTimeSeries->getNumEntries() == innerTimeSeries->getNumEntries());
	    REQUIRE(outerTimeSeries->getVolumeUnits() == innerTimeSeries->getVolumeUnits());
	    REQUIRE(outerTimeSeries->getFirstDate() == innerTimeSeries->getFirstDate());
	    REQUIRE(outerTimeSeries->getLastDate() == innerTimeSeries->getLastDate());
	    REQUIRE(outerTimeSeries != innerTimeSeries);
	  }
      }

    outer_it = timeShiftedCreator->beginShiftedTimeSeries();
    auto ssoDailyTimeSeries = ssoDaily->getTimeSeries();

    for(; outer_it != timeShiftedCreator->endShiftedTimeSeries(); outer_it++)
      {
	outerTimeSeries = *outer_it;
	REQUIRE(outerTimeSeries->getTimeFrame() == ssoDailyTimeSeries->getTimeFrame());
	REQUIRE(outerTimeSeries->getNumEntries() == ssoDailyTimeSeries->getNumEntries());
	REQUIRE(outerTimeSeries->getVolumeUnits() == ssoDailyTimeSeries->getVolumeUnits());
	REQUIRE(outerTimeSeries->getFirstDate() == ssoDailyTimeSeries->getFirstDate());
	REQUIRE(outerTimeSeries->getLastDate() == ssoDailyTimeSeries->getLastDate());
	REQUIRE(outerTimeSeries != ssoDailyTimeSeries);
      }
  }
}
