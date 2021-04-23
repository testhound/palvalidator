#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvWriter.h"
#include "../TimeSeriesCsvReader.h"
#include "../TimeSeries.h"
#include "../DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

std::shared_ptr<OHLCTimeSeriesEntry<DecimalType>>
    createEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
       return createTimeSeriesEntry(dateString, openPrice, highPrice, lowPrice, closePrice, vol); 
  }



TEST_CASE ("TimeSeries operations", "[TimeSeries]")
{
  auto entry8 = createEquityEntry ("20160108", "195.19", "195.85", "191.58","191.92",
				   0);
  auto entry7 = createEquityEntry ("20160107", "195.33", "197.44", "193.59","194.05",
				   0);

  auto entry6 = createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   0);

  auto entry5 = createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   0);

  auto entry4 = createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   0);

  auto entry3 = createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   0);

  auto entry2 = createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   0);

  auto entry1 = createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				  0);

  auto entry0 = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   0);
  OHLCTimeSeries<DecimalType> spySeries(TimeFrame::DAILY, TradingVolume::SHARES);

  spySeries.addEntry (*entry8);
  spySeries.addEntry (*entry7);
  spySeries.addEntry (*entry6);
  spySeries.addEntry (*entry5);
  spySeries.addEntry (*entry4);
  spySeries.addEntry (*entry3);
  spySeries.addEntry (*entry2);
  spySeries.addEntry (*entry1);
  spySeries.addEntry (*entry0);

  
  
  SECTION ("Timeseries size test", "[TimeSeries]")
    {
      std::string fileName("spy_pal_format.csv");

      PalTimeSeriesCsvWriter<DecimalType> writer (fileName, spySeries);
      writer.writeFile();

      PALFormatCsvReader<DecimalType> csvFile (fileName, TimeFrame::DAILY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick);
      csvFile.readFile();

      std::shared_ptr<OHLCTimeSeries<DecimalType>> testTimeSeries = csvFile.getTimeSeries();

      REQUIRE (*testTimeSeries == spySeries);
    }



  
}
