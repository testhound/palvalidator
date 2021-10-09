#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "DateRange.h"
#include "BoostDateHelper.h"
#include "McptConfigurationFileReader.h"
#include "DataSourceReader.h"
//#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("DataSourceReader operations", "[DataSourceReader]")
{
  BarchartReader barChartApiReader("9bff8ed715c16109a1ce5c63341bb860");

  boost::gregorian::date insample_firstDate (2012, Apr, 4);
  boost::gregorian::date insample_lastDate (2018, Apr, 4);

  DateRange insample_dates(insample_firstDate, insample_lastDate);

  boost::gregorian::date oos_firstDate (2018, Apr, 5);
  boost::gregorian::date oos_lastDate (2021, Apr, 2);

  DateRange oos_dates(oos_firstDate, oos_lastDate);
  
  std::string ssoTicker("SSO");
  std::string dailyTimeFrame("Daily");
  std::string hourlyTimeFrame("hourly");
  //finnHubApiReader.createTemporaryFile(ssoTicker, dailyTimeFrame, insample_dates, oos_dates, true);
  barChartApiReader.createTemporaryFile(ssoTicker, dailyTimeFrame, insample_dates, oos_dates, true);
  barChartApiReader.createTemporaryFile(ssoTicker, hourlyTimeFrame, insample_dates, oos_dates, true);

}
