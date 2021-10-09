#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "TimeSeriesCsvReader.h"
#include "DecimalConstants.h"
#include "TimeSeriesValidator.h"

typedef dec::decimal<7> Decimal;

using namespace mkc_timeseries;

TEST_CASE ("TimeSeriesValidator operations", "[TimeSeriesValidator]")
{
  std::string dataDir = "./TimeSeriesValidatorTimeSeriesData/";
  SECTION ("TimeSeriesValidator Holidays", "[TimeSeriesValidator]")
  {
    std::unique_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "early_closures.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    reader->readFile();
    std::unique_ptr<TimeSeriesValidator<Decimal>> validator = std::make_unique<TimeSeriesValidator<Decimal>>(reader->getTimeSeries(), reader->getTimeSeries(), 7);

    REQUIRE_NOTHROW(validator->validate());
  }

  SECTION ("TimeSeriesValidator MissingHourlyDays", "[TimeSeriesValidator]")
  {
    std::unique_ptr<TimeSeriesCsvReader<Decimal>> hourlyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "missing_hourly_hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    hourlyReader->readFile();

    std::unique_ptr<TimeSeriesCsvReader<Decimal>> dailyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "missing_hourly_daily.txt", TimeFrame::DAILY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    dailyReader->readFile();
    std::unique_ptr<TimeSeriesValidator<Decimal>> validator = std::make_unique<TimeSeriesValidator<Decimal>>(hourlyReader->getTimeSeries(), dailyReader->getTimeSeries(), 7);

    REQUIRE_THROWS_AS(validator->validate(), TimeSeriesValidationException);
    REQUIRE_THROWS_WITH(validator->validate(), Catch::Matchers::Contains("ERROR:") && Catch::Matchers::Contains("not found in the hourly time series"));
  }

  SECTION ("TimeSeriesValidator MissingDailyDays", "[TimeSeriesValidator]")
  {
    std::unique_ptr<TimeSeriesCsvReader<Decimal>> hourlyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "missing_daily_hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    hourlyReader->readFile();

    std::unique_ptr<TimeSeriesCsvReader<Decimal>> dailyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "missing_daily_daily.txt", TimeFrame::DAILY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    dailyReader->readFile();
    std::unique_ptr<TimeSeriesValidator<Decimal>> validator = std::make_unique<TimeSeriesValidator<Decimal>>(hourlyReader->getTimeSeries(), dailyReader->getTimeSeries(), 7);

    REQUIRE_THROWS_AS(validator->validate(), TimeSeriesValidationException);
    REQUIRE_THROWS_WITH(validator->validate(), Catch::Matchers::Contains("ERROR:") && Catch::Matchers::Contains("not found in the daily time series"));
  }

  SECTION ("TimeSeriesValidator ValidDailyAndHourlyDays", "[TimeSeriesValidator]")
  {
    std::unique_ptr<TimeSeriesCsvReader<Decimal>> hourlyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "MSFT_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    hourlyReader->readFile();

    std::unique_ptr<TimeSeriesCsvReader<Decimal>> dailyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "MSFT_RAD_Daily.txt", TimeFrame::DAILY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    dailyReader->readFile();
    std::unique_ptr<TimeSeriesValidator<Decimal>> validator = std::make_unique<TimeSeriesValidator<Decimal>>(hourlyReader->getTimeSeries(), dailyReader->getTimeSeries(), 7);

    REQUIRE_NOTHROW(validator->validate());
  }

  SECTION ("TimeSeriesValidator TooFewSevenBarDays", "[TimeSeriesValidator]")
  {
    std::unique_ptr<TimeSeriesCsvReader<Decimal>> hourlyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "missing_too_many_bars.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    hourlyReader->readFile();
    std::unique_ptr<TimeSeriesValidator<Decimal>> validator = std::make_unique<TimeSeriesValidator<Decimal>>(hourlyReader->getTimeSeries(), hourlyReader->getTimeSeries(), 7);

    REQUIRE_THROWS_AS(validator->validate(), TimeSeriesValidationException);
    REQUIRE_THROWS_WITH(validator->validate(), Catch::Matchers::Contains("ERROR:") && Catch::Matchers::Contains("Not enough days in the hourly time series had 7 bars. Expected: at least 99% Found:"));
  }

  SECTION ("TimeSeriesValidator DeleteFromSeries", "[TimeSeriesValidator]")
  {
    std::unique_ptr<TimeSeriesCsvReader<Decimal>> hourlyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "MSFT_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    hourlyReader->readFile();

    std::unique_ptr<TimeSeriesCsvReader<Decimal>> dailyReader = std::make_unique<TradeStationFormatCsvReader<Decimal>>(
      dataDir + "MSFT_RAD_Daily.txt", TimeFrame::DAILY, TradingVolume::SHARES, 
      DecimalConstants<Decimal>::EquityTick
    );
    dailyReader->readFile();
    
    int sizeHourly = hourlyReader->getTimeSeries()->getNumEntries();
    int sizeDaily = dailyReader->getTimeSeries()->getNumEntries();

    std::unique_ptr<TimeSeriesValidator<Decimal>> validator = std::make_unique<TimeSeriesValidator<Decimal>>(hourlyReader->getTimeSeries(), dailyReader->getTimeSeries(), 7);
    validator->validate();

    REQUIRE(dailyReader->getTimeSeries()->getNumEntries() == (sizeDaily - 1));
    REQUIRE(hourlyReader->getTimeSeries()->getNumEntries() == (sizeHourly - 6));
  }
}
