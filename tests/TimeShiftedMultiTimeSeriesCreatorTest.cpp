#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "TimeShiftedMultiTimeSeriesCreator.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

typedef dec::decimal<7> DecimalType;

using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::time_duration;

TEST_CASE ("TimeShiftedMultiTimeSeriesCreator operations", "[DailyTimeShiftedMultiTimeSeriesCreator]")
{
  std::string equitySymbol("SSO");
  std::string equityName("ProShares Ultra S&P500");

  auto ssoDailyFile = std::make_shared<TradeStationFormatCsvReader<DecimalType>>("SSO_RAD_Daily.txt",
										 TimeFrame::DAILY,
										 TradingVolume::SHARES,
										 DecimalConstants<DecimalType>::EquityTick);
  ssoDailyFile->readFile();

  auto ssoIntradayFile = std::make_shared<TradeStationFormatCsvReader<DecimalType>>("SSO_RAD_Hourly.txt",
										    TimeFrame::INTRADAY,
										    TradingVolume::SHARES,
										    DecimalConstants<DecimalType>::EquityTick);
 
  ssoIntradayFile->readFile();

  auto ssoDaily = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, equityName, ssoDailyFile->getTimeSeries());
  auto ssoHourly = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, equityName, ssoIntradayFile->getTimeSeries());

  auto timeShiftedCreator = std::make_shared<DailyTimeShiftedMultiTimeSeriesCreator<DecimalType>>("SSO_RAD_Hourly.txt",
												  ssoDaily);
  timeShiftedCreator->createShiftedTimeSeries();

  SECTION ("DailyTimeShiftedMultiTimeSeriesCreator", "[DailyTimeShiftedMultiTimeSeriesCreator]")
  {
    REQUIRE(timeShiftedCreator->numTimeSeriesCreated() == 7);

    std::shared_ptr<OHLCTimeSeries<Decimal>> outerTimeSeries;
    std::shared_ptr<OHLCTimeSeries<Decimal>> innerTimeSeries;

    auto outer_it = timeShiftedCreator->beginShiftedTimeSeries();
    for(; outer_it != timeShiftedCreator->endShiftedTimeSeries; outer_it++)
      {
	outerTimeSeries = *outer_it;

	auto inner_it = outer_it + 1;
	for(; inner_it != timeShiftedCreator->endShiftedTimeSeries; inner_it++)
	  {
	    innerTimeSeries = *inner_it;
	  }
      }

  }
}
