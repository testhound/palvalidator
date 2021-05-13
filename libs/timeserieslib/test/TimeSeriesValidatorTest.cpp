#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../DecimalConstants.h"
#include "../TimeSeriesValidator.h"

typedef dec::decimal<7> DecimalType;

using namespace mkc_timeseries;

TEST_CASE ("TimeSeriesValidator operations", "[TimeSeriesValidator]")
{
  std::shared_ptr<TimeSeriesValidator<DecimalType>> timeSeriesValidator;

  SECTION ("TimeSeriesValidator Holidays", "[TimeSeriesValidator]")
  {
      
  }

  SECTION ("TimeSeriesValidator TimeFramesInRange", "[TimeSeriesValidator]")
  {
  }

  SECTION ("TimeSeriesValidator TimeFrameOutOfRange", "[TimeSeriesValidator]")
  {
  }

  SECTION ("TimeSeriesValidator MissingHourlyDays", "[TimeSeriesValidator]")
  {
  }

  SECTION ("TimeSeriesValidator MissingDailyDays", "[TimeSeriesValidator]")
  {
  }

  SECTION ("TimeSeriesValidator ValidDailyAndHourlyDays", "[TimeSeriesValidator]")
  {
  }

  SECTION ("TimeSeriesValidator MissingDailyDays", "[TimeSeriesValidator]")
  {
  }

  SECTION ("TimeSeriesValidator DeleteFromSeries", "[TimeSeriesValidator]")
  {
  }
}
