#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "TimeSeriesCsvReader.h"
#include "TimeSeriesCsvWriter.h"
#include "SyntheticTimeSeriesCreator.h"
#include "DecimalConstants.h"
#include "TimeFrameDiscovery.h"
#include <map>

typedef dec::decimal<7> DecimalType;

using namespace mkc_timeseries;
using namespace boost::gregorian;
using boost::posix_time::time_duration;

TEST_CASE ("SyntheticTimeSeriesCreator operations", "[SyntheticTimeSeriesCreator]")
{
  std::shared_ptr<TimeFrameDiscovery<DecimalType>> msftTimeFrameDiscovery;
  std::shared_ptr<TimeFrameDiscovery<DecimalType>> kcTimeFrameDiscovery;

  std::shared_ptr<SyntheticTimeSeriesCreator<DecimalType>> msftSyntheticTimeSeriesCreator;
  std::shared_ptr<SyntheticTimeSeriesCreator<DecimalType>> kcSyntheticTimeSeriesCreator;

  /* 
  * Unique KC Times (from Pandas):
  *     '05:15', '06:15', '07:15', '08:15', '09:15', '10:15', '11:15', '12:15', '13:15', '13:30'
  *
  * Unique MSFT TImes:
  *     9:00, 10:00, 11:00, 12:00, 13:00, 14:00, 15:00
  */
  SECTION ("SyntheticTimeSeriesCreator distinct", "[SyntheticTimeSeriesCreator]")
  {
    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(
        "MSFT_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );
    reader->readFile();
    msftTimeFrameDiscovery = std::make_shared<TimeFrameDiscovery<DecimalType>>(reader->getTimeSeries());
    msftTimeFrameDiscovery->inferTimeFrames();

    reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(
        "KC_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );
    reader->readFile();
    kcTimeFrameDiscovery = std::make_shared<TimeFrameDiscovery<DecimalType>>(reader->getTimeSeries());
    kcTimeFrameDiscovery->inferTimeFrames();

    REQUIRE(kcTimeFrameDiscovery->numTimeFrames()==10);
    REQUIRE(msftTimeFrameDiscovery->numTimeFrames()==7);
  }

  SECTION ("SyntheticTimeSeriesCreator MSFT counts", "[SyntheticTimeSeriesCreator]")
  {
    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(
        "MSFT_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );
    reader->readFile();

    int msftRowCount = reader->getTimeSeries()->getNumEntries();
    msftTimeFrameDiscovery = std::make_shared<TimeFrameDiscovery<DecimalType>>(reader->getTimeSeries());
    msftTimeFrameDiscovery->inferTimeFrames();
    msftSyntheticTimeSeriesCreator = std::make_shared<SyntheticTimeSeriesCreator<DecimalType>>(reader->getTimeSeries());

    int msftAggregateCount = 0;
    int partialDays = 0;
    for(int i = 0; i < msftTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_duration time = msftTimeFrameDiscovery->getTimeFrame(i);
        msftSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);
        msftAggregateCount += msftSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();

	partialDays += msftSyntheticTimeSeriesCreator->getNumPartialDays(i + 1);
    }

    REQUIRE(msftRowCount + partialDays == msftAggregateCount); // ensure the files written have the same number of entries as the original time series file
  }

  // same test as above but with different time frames (15 minutes)
  SECTION ("SyntheticTimeSeriesCreator KC counts", "[SyntheticTimeSeriesCreator]")
  {
    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(
        "KC_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );
    reader->readFile();

    int kcRowCount = reader->getTimeSeries()->getNumEntries();
    kcTimeFrameDiscovery = std::make_shared<TimeFrameDiscovery<DecimalType>>(reader->getTimeSeries());
    kcTimeFrameDiscovery->inferTimeFrames();
    kcSyntheticTimeSeriesCreator = std::make_shared<SyntheticTimeSeriesCreator<DecimalType>>(reader->getTimeSeries());

    int  kcAggregateCount = 0;
    int partialDays = 0;
    for(int i = 0; i < kcTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_duration time = kcTimeFrameDiscovery->getTimeFrame(i);
        kcSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);

        kcAggregateCount += kcSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();
	partialDays += kcSyntheticTimeSeriesCreator->getNumPartialDays(i + 1);
    }

    REQUIRE(kcRowCount + partialDays == kcAggregateCount);
  }

  SECTION ("SyntheticTimeSeriesCreator MSFT counts", "[SyntheticTimeSeriesCreator]")
  {
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> msftHourlyCsvFile("MSFT_RAD_Hourly.txt");
    msftHourlyCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", "Close", "Up", "Down");

    std::string dateStamp, timeStamp;
    std::string openString, highString, lowString, closeString;
    std::string up, down;
    
    struct tm tm = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::map<time_t, int> uniqueTimeFrameCountMap;
    while (msftHourlyCsvFile.read_row(dateStamp, timeStamp, openString, highString, lowString, closeString, up, down))
    {
        strptime(timeStamp.c_str(), "%H:%M", &tm);
        time_t time = mktime(&tm);
        
        std::map<time_t, int>::iterator it = uniqueTimeFrameCountMap.find(time);
        if(it != uniqueTimeFrameCountMap.end()) 
            it->second++;
        else 
            uniqueTimeFrameCountMap[time] = 1;
    }

    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(
        "MSFT_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );
    reader->readFile();
    msftSyntheticTimeSeriesCreator = std::make_shared<SyntheticTimeSeriesCreator<DecimalType>>(reader->getTimeSeries());
    msftTimeFrameDiscovery = std::make_shared<TimeFrameDiscovery<DecimalType>>(reader->getTimeSeries());
    msftTimeFrameDiscovery->inferTimeFrames();

    for(int i = 0; i < msftTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_duration time = msftTimeFrameDiscovery->getTimeFrame(i);
        msftSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);

        int filesize = msftSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();

        tm.tm_hour = time.hours(); tm.tm_min = time.minutes(); tm.tm_sec = time.seconds();
        time_t t_time = mktime(&tm);
        auto it = uniqueTimeFrameCountMap.find(t_time);

        // Ensure that the appropriate number of time stamps were written to each file.
        // That is, if there were four 9:00 entries in the original hourly file, there 
        // should be four entries read in from the synthetic timeframe file generated for
        // the 9:00 time stamp.
        REQUIRE(it->second + msftSyntheticTimeSeriesCreator->getNumPartialDays(i + 1) == filesize); 

    }
  }

  SECTION ("SyntheticTimeSeriesCreator KC counts", "[SyntheticTimeSeriesCreator]")
  {
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> kcHourlyCsvFile("KC_RAD_Hourly.txt");
    kcHourlyCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", "Close", "Up", "Down");

    std::string dateStamp, timeStamp;
    std::string openString, highString, lowString, closeString;
    std::string up, down;
    
    struct tm tm = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::map<time_t, int> uniqueTimeFrameCountMap;
    while (kcHourlyCsvFile.read_row(dateStamp, timeStamp, openString, highString, lowString, closeString, up, down))
    {
        strptime(timeStamp.c_str(), "%H:%M", &tm);
        time_t time = mktime(&tm);
        
        std::map<time_t, int>::iterator it = uniqueTimeFrameCountMap.find(time);
        if(it != uniqueTimeFrameCountMap.end()) 
            it->second++;
        else 
            uniqueTimeFrameCountMap[time] = 1;
    }

    std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(
        "KC_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick
    );
    reader->readFile();
    kcSyntheticTimeSeriesCreator = std::make_shared<SyntheticTimeSeriesCreator<DecimalType>>(reader->getTimeSeries());
    kcTimeFrameDiscovery = std::make_shared<TimeFrameDiscovery<DecimalType>>(reader->getTimeSeries());
    kcTimeFrameDiscovery->inferTimeFrames();

    for(int i = 0; i < kcTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_duration time = kcTimeFrameDiscovery->getTimeFrame(i);
        kcSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);

        int filesize = kcSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();

        tm.tm_hour = time.hours(); tm.tm_min = time.minutes(); tm.tm_sec = time.seconds();
        time_t t_time = mktime(&tm);
        auto it = uniqueTimeFrameCountMap.find(t_time);

        // Ensure that the appropriate number of time stamps were written to each file.
        // That is, if there were four 9:00 entries in the original hourly file, there 
        // should be four entries read in from the synthetic timeframe file generated for
        // the 9:00 time stamp.
        REQUIRE(it->second + kcSyntheticTimeSeriesCreator->getNumPartialDays(i + 1) == filesize); 
    }
  }


}
