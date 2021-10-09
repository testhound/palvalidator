#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "number.h"
#include "TimeSeries.h"
#include "SyntheticTimeSeries.h"
#include "TimeSeriesCsvReader.h"
#include "TimeSeriesCsvWriter.h"
#include "DecimalConstants.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

namespace  {

OHLCTimeSeriesEntry<DecimalType>
    createEquityEntry (const std::string& dateString,
		       const std::string& openPrice,
		       const std::string& highPrice,
		       const std::string& lowPrice,
		       const std::string& closePrice,
		       volume_t vol)
  {
    return *createTimeSeriesEntry(dateString, openPrice, highPrice, lowPrice, closePrice, vol);
  }

}

TEST_CASE ("SyntheticTimeSeriesTest-TimeSeries operations", "[TimeSeries]")
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

  OHLCTimeSeries<DecimalType> spySeries(TimeFrame::DAILY, TradingVolume::SHARES);

  spySeries.addEntry (entry0);
  spySeries.addEntry (entry1);
  spySeries.addEntry (entry2);
  spySeries.addEntry (entry3);
  spySeries.addEntry (entry4);
  spySeries.addEntry (entry5);
  spySeries.addEntry (entry6);
  spySeries.addEntry (createEquityEntry ("20160107", "195.33", "197.44", "193.59","194.05",
					 207229000));
  spySeries.addEntry (createEquityEntry ("20151224", "205.72", "206.33", "205.42","205.68",
					 48542200));
  spySeries.addEntry (createEquityEntry ("20151223", "204.69", "206.07", "204.58","206.02",
					 110987200));
  spySeries.addEntry (createEquityEntry ("20151222", "202.72", "203.85", "201.55","203.50",
					 110026200));
  spySeries.addEntry (createEquityEntry ("20151221", "201.41", "201.88", "200.09","201.67",
					 99094300));
  spySeries.addEntry (createEquityEntry ("20151218", "202.77", "202.93", "199.83","200.02",
					 251393500));
  spySeries.addEntry (createEquityEntry ("20151217", "208.40", "208.48", "204.84","204.86",
					 173092500));
  spySeries.addEntry (createEquityEntry ("20151216", "206.37", "208.39", "204.80","208.03",
					 197017000));
  spySeries.addEntry (createEquityEntry ("20151215", "204.70", "206.11", "202.87","205.03",
					 154069600));
  spySeries.addEntry (createEquityEntry ("20151214", "202.07", "203.05", "199.95","202.90",
					 182385200));
  spySeries.addEntry (createEquityEntry ("20151211", "203.35", "204.14", "201.51","201.88",
					 211173300));
  spySeries.addEntry (createEquityEntry ("20151210", "205.42", "207.43", "205.14","205.87",
					 116128900));
  spySeries.addEntry (createEquityEntry ("20151209", "206.19", "208.68", "204.18","205.34",
					 162401500));

  SyntheticTimeSeries<DecimalType> syntheticSpySeries (spySeries, DecimalConstants<DecimalType>::EquityTick,
					       DecimalConstants<DecimalType>::EquityTick / DecimalConstants<DecimalType>::DecimalTwo);
  OHLCTimeSeries<DecimalType>::TimeSeriesIterator firstElementIterator = spySeries.beginSortedAccess();
  REQUIRE (syntheticSpySeries.getFirstOpen() == firstElementIterator->second.getOpenValue());
  syntheticSpySeries.createSyntheticSeries();

  std::shared_ptr<OHLCTimeSeries<DecimalType>> p = syntheticSpySeries.getSyntheticTimeSeries();
  std::cout << "number of entries in Synthetic time series = " << p->getNumEntries() << std::endl << std:: endl;

  OHLCTimeSeries<DecimalType>::TimeSeriesIterator it = p->beginSortedAccess();

  std::cout << "Printing Synthetic time series"<< std::endl << std:: endl;
  for (; it != p->endSortedAccess(); it++)
    {
      std::cout << it->first << "," << it->second.getOpenValue() <<"," << it->second.getHighValue() << "," << it->second.getLowValue() << "," << it->second.getCloseValue() << std::endl;
    }
    std::cout << "Done Printing Synthetic time series"<< std::endl << std:: endl;

  SECTION ("Timeseries size test", "[TimeSeries]")
    {
      REQUIRE (p->getNumEntries() == spySeries.getNumEntries());
    }

  SECTION ("Timeseries date test", "[TimeSeries]")
    {
      REQUIRE (spySeries.getFirstDate() == p->getFirstDate());
      REQUIRE (spySeries.getLastDate() == p->getLastDate());
    }

  SECTION ("Timeseries time frame test", "[TimeSeries]")
    {
      REQUIRE (spySeries.getTimeFrame() == p->getTimeFrame()); 
    }

  SECTION ("Timeseries time frame test", "[TimeSeries]")
    {
      REQUIRE (spySeries != *p);
      REQUIRE_FALSE (spySeries == *p);
    }

  SECTION ("SyntheticTimeSeries OHLC creation", "[SyntheticTimeSeries]")
    {
      DecimalType prevClose (DecimalConstants<DecimalType>::createDecimal("80901.5811145"));
      DecimalType relativeOpen(DecimalConstants<DecimalType>::createDecimal("1.2380000"));
      DecimalType relativeHigh(DecimalConstants<DecimalType>::createDecimal("1.0290650"));
      DecimalType relativeLow(DecimalConstants<DecimalType>::createDecimal("0.9843769"));
      DecimalType relativeClose(DecimalConstants<DecimalType>::createDecimal("1.0249971"));

      DecimalType syntheticOpen(prevClose * relativeOpen);
      DecimalType syntheticClose(syntheticOpen * relativeClose);
      DecimalType syntheticHigh(syntheticOpen * relativeHigh);
      DecimalType syntheticLow(syntheticOpen * relativeLow);

      REQUIRE (syntheticOpen > DecimalConstants<DecimalType>::DecimalZero);
      REQUIRE (syntheticHigh > DecimalConstants<DecimalType>::DecimalZero);
      REQUIRE (syntheticLow > DecimalConstants<DecimalType>::DecimalZero);
      REQUIRE (syntheticClose > DecimalConstants<DecimalType>::DecimalZero);

      std::cout << "Synthetic open = " << syntheticOpen << std::endl;
      std::cout << "Synthetic high = " << syntheticHigh << std::endl;
      std::cout << "Synthetic low = " << syntheticLow << std::endl;
      std::cout << "Synthetic close = " << syntheticClose << std::endl;

    }

  SECTION ("SyntheticTimeSeries multiple creation", "[SyntheticTimeSeries]")
    {
      PALFormatCsvReader<DecimalType> amznCsvReader ("AMZN.txt", TimeFrame::DAILY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick);
      amznCsvReader.readFile();

      std::shared_ptr<OHLCTimeSeries<DecimalType>> amznTimeSeries = amznCsvReader.getTimeSeries();
      OHLCTimeSeries<DecimalType>::TimeSeriesIterator firstElementIterator = amznTimeSeries->beginSortedAccess();
      DecimalType openingPrice = firstElementIterator->second.getOpenValue();

      SyntheticTimeSeries<DecimalType> aTimeSeriesToDump(*amznTimeSeries, DecimalConstants<DecimalType>::EquityTick,
						 DecimalConstants<DecimalType>::EquityTick / DecimalConstants<DecimalType>::DecimalTwo);
      aTimeSeriesToDump.createSyntheticSeries();

      PalTimeSeriesCsvWriter<DecimalType> dumpFile("SyntheticSeriesOut.csv", 
					    *aTimeSeriesToDump.getSyntheticTimeSeries());
      dumpFile.writeFile();
      for (int i = 0; i < 100; i++)
	{
	  SyntheticTimeSeries<DecimalType> aTimeSeries2(*amznTimeSeries, DecimalConstants<DecimalType>::EquityTick,
					       DecimalConstants<DecimalType>::EquityTick / DecimalConstants<DecimalType>::DecimalTwo);
	  REQUIRE (aTimeSeries2.getFirstOpen() == openingPrice);
	  aTimeSeries2.createSyntheticSeries();
	}
    }
}
