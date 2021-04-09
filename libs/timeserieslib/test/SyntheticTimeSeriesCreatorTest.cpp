#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"
#include "../TimeSeriesCsvWriter.h"
#include "../SyntheticTimeSeriesCreator.h"
#include "../DecimalConstants.h"
#include "../TimeFrameDiscovery.h"
#include <map>

typedef dec::decimal<7> DecimalType;

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("SyntheticTimeSeriesCreator operations", "[SyntheticTimeSeriesCreator]")
{
  std::shared_ptr<TimeFrameDiscovery<DecimalType>> msftTimeFrameDiscovery = std::make_shared<TradestationHourlyTimeFrameDiscovery<DecimalType>>("MSFT_RAD_Hourly.txt");
  msftTimeFrameDiscovery->inferTimeFrames();
  std::shared_ptr<TimeFrameDiscovery<DecimalType>> kcTimeFrameDiscovery = std::make_shared<TradestationHourlyTimeFrameDiscovery<DecimalType>>("KC_RAD_Hourly.txt");
  kcTimeFrameDiscovery->inferTimeFrames();

  std::shared_ptr<SyntheticTimeSeriesCreator<DecimalType>> msftSyntheticTimeSeriesCreator = std::make_shared<TradestationHourlySyntheticTimeSeriesCreator<DecimalType>>(
        "MSFT_RAD_Hourly.txt", TimeFrame::DAILY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick);
  std::shared_ptr<SyntheticTimeSeriesCreator<DecimalType>> kcSyntheticTimeSeriesCreator = std::make_shared<TradestationHourlySyntheticTimeSeriesCreator<DecimalType>>(
        "KC_RAD_Hourly.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS, DecimalConstants<DecimalType>::createDecimal("0.05"));

  /* 
  * Unique KC Times (from Pandas):
  *     '05:15', '06:15', '07:15', '08:15', '09:15', '10:15', '11:15', '12:15', '13:15', '13:30'
  *
  * Unique MSFT TImes:
  *     '12:00', '13:00', '14:00', '15:00', '16:00', '17:00', '18:00', '19:00', '20:00', '21:00', '10:00', '11:00', '22:00', '00:00', '23:00'
  */
  SECTION ("SyntheticTimeSeriesCreator distinct", "[SyntheticTimeSeriesCreator]")
  {
      REQUIRE(kcTimeFrameDiscovery->numTimeFrames()==10);
      REQUIRE(msftTimeFrameDiscovery->numTimeFrames()==15);
  }

  SECTION ("SyntheticTimeSeriesCreator MSFT counts", "[SyntheticTimeSeriesCreator]")
  {
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> msftHourlyCsvFile("MSFT_RAD_Hourly.txt");
    msftHourlyCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", "Close", "Up", "Down");

    std::string dateStamp, timeStamp;
    std::string openString, highString, lowString, closeString;
    std::string up, down;

    int msftRowCount = 0;
    while (msftHourlyCsvFile.read_row(dateStamp, timeStamp, openString, highString, lowString, closeString, up, down))
        msftRowCount++;

    int msftAggregateCount = 0;
    for(int i = 0; i < msftTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_t time = msftTimeFrameDiscovery->getTimeFrameInMinutes(i);
        msftSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);

        std::string filename(std::string("MSFT_RAD_Hourly.txt_timeframe_") + std::to_string(i+1));
        msftAggregateCount += msftSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();

        std::remove(filename.c_str());
    }

    REQUIRE(msftRowCount == msftAggregateCount); // ensure the files written have the same number of entries as the original time series file
  }

  // same test as above but with different time frames (15 minutes)
  SECTION ("SyntheticTimeSeriesCreator KC counts", "[SyntheticTimeSeriesCreator]")
  {
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> kcHourlyCsvFile("KC_RAD_Hourly.txt");
    kcHourlyCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", "Close", "Up", "Down");

    std::string dateStamp, timeStamp;
    std::string openString, highString, lowString, closeString;
    std::string up, down;

    int kcRowCount = 0;
    while (kcHourlyCsvFile.read_row(dateStamp, timeStamp, openString, highString, lowString, closeString, up, down))
        kcRowCount++;

    int  kcAggregateCount = 0;
    for(int i = 0; i < kcTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_t time = kcTimeFrameDiscovery->getTimeFrameInMinutes(i);
        kcSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);

        std::string filename(std::string("KC_RAD_Hourly.txt_timeframe_") + std::to_string(i+1));
        kcAggregateCount += kcSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();

        std::remove(filename.c_str()); // delete the file
    }

    REQUIRE(kcRowCount == kcAggregateCount);
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

    for(int i = 0; i < msftTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_t time = msftTimeFrameDiscovery->getTimeFrameInMinutes(i);
        msftSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);

        std::string filename(std::string("MSFT_RAD_Hourly.txt_timeframe_") + std::to_string(i+1));
        int filesize = msftSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();
        auto it = uniqueTimeFrameCountMap.find(time);

        // Ensure that the appropriate number of time stamps were written to each file.
        // That is, if there were four 9:00 entries in the original hourly file, there 
        // should be four entries read in from the synthetic timeframe file generated for
        // the 9:00 time stamp.
        REQUIRE(it->second == filesize); 

        std::remove(filename.c_str()); // delete the file
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

    for(int i = 0; i < kcTimeFrameDiscovery->numTimeFrames(); i++) 
    {
        time_t time = kcTimeFrameDiscovery->getTimeFrameInMinutes(i);
        kcSyntheticTimeSeriesCreator->createSyntheticTimeSeries(i+1, time);

        std::string filename(std::string("KC_RAD_Hourly.txt_timeframe_") + std::to_string(i+1));
        int filesize = kcSyntheticTimeSeriesCreator->getSyntheticTimeSeries(i+1)->getNumEntries();
        auto it = uniqueTimeFrameCountMap.find(time);

        // Ensure that the appropriate number of time stamps were written to each file.
        // That is, if there were four 9:00 entries in the original hourly file, there 
        // should be four entries read in from the synthetic timeframe file generated for
        // the 9:00 time stamp.
        REQUIRE(it->second == filesize); 

        std::remove(filename.c_str()); // delete the file
    }
  }


}
