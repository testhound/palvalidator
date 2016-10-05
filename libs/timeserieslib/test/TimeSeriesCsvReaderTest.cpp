#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../TimeSeriesCsvReader.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;



TEST_CASE ("TimeSeriesCsvReader operations", "[TimeSeriesCsvReader]")
{
  //  PALFormatCsvReader<6> csvFile ("TY_RAD.csv", TimeFrame::DAILY, TradingVolume::CONTRACTS);
  // csvFile.readFile();

  TradeStationFormatCsvReader<7> gildCsvFile ("GILD.txt", TimeFrame::DAILY, TradingVolume::SHARES);
  gildCsvFile.readFile();

  CSIExtendedFuturesCsvReader<7> dollarIndexCsvFile ("DX20060R.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS);
  dollarIndexCsvFile.readFile();

  std::shared_ptr<OHLCTimeSeries<7>> dollarIndexTimeSeries = dollarIndexCsvFile.getTimeSeries();

  CSIErrorCheckingExtendedFuturesCsvReader<7> dollarIndexErrorCheckedCsvFile ("DX20060R.txt", TimeFrame::DAILY, TradingVolume::CONTRACTS);

  PinnacleErrorCheckingFormatCsvReader<7> britishPoundCsvFile ("BN_RAD.csv", 
							    TimeFrame::DAILY, 
							    TradingVolume::CONTRACTS);
  britishPoundCsvFile.readFile();
  // std::shared_ptr<OHLCTimeSeries<6>> p = csvFile.getTimeSeries();
  std::shared_ptr<OHLCTimeSeries<7>> gildTimeSeries = gildCsvFile.getTimeSeries();

  SECTION ("Timeseries time frame test", "[TimeSeriesCsvReader]")
    {
      //    REQUIRE (p->getTimeFrame() == TimeFrame::DAILY);
      REQUIRE (gildTimeSeries->getTimeFrame() == TimeFrame::DAILY);
      REQUIRE (dollarIndexTimeSeries->getTimeFrame() == TimeFrame::DAILY);
    }

  SECTION ("Timeseries first date test", "[TimeSeriesCsvReader]")
    {
      boost::gregorian::date refDate1 (1985, Nov, 20);
      boost::gregorian::date refDate2 (1992, Jan, 23);

      //REQUIRE (p->getFirstDate() == refDate1);
      REQUIRE (gildTimeSeries->getFirstDate() == refDate2);
      REQUIRE (dollarIndexTimeSeries->getFirstDate() == refDate1);
    }

  SECTION ("Timeseries last date test", "[TimeSeriesCsvReader]")
    {
      boost::gregorian::date refDate1 (2016, Jun, 2);
      boost::gregorian::date refDate2 (2016, Apr, 6);

      //REQUIRE (p->getLastDate() == refDate1);
      REQUIRE (dollarIndexTimeSeries->getLastDate() == refDate1);
      REQUIRE (gildTimeSeries->getLastDate() == refDate2);
    }

  SECTION ("Timeseries get OHLC tests", "[TimeSeriesCsvReader]")
    {
      OHLCTimeSeries<7>::ConstRandomAccessIterator it = dollarIndexTimeSeries->beginRandomAccess();

      decimal<7> refOpen (dec::fromString<decimal<7>> (std::string ("186.14547208")));
      REQUIRE ((*it)->getOpenValue() == refOpen);

      decimal<7> refHigh (dec::fromString<decimal<7>> (std::string ("187.89263334")));
      REQUIRE ((*it)->getHighValue() == refHigh);

      decimal<7> refLow (dec::fromString<decimal<7>> (std::string ("186.07267370")));
      REQUIRE ((*it)->getLowValue() == refLow);

      decimal<7> refClose (dec::fromString<decimal<7>> (std::string ("187.6159994")));
      REQUIRE ((*it)->getCloseValue() == refClose);

      it = it + 19;
      boost::gregorian::date refDate1 (1985, Dec, 18);
      REQUIRE ((*it)->getDateValue() == refDate1);
     decimal<7> refOpen0 (dec::fromString<decimal<7>> (std::string ("184.36919147")));
      REQUIRE ((*it)->getOpenValue() == refOpen0);

      decimal<7> refHigh0 (dec::fromString<decimal<7>> (std::string ("185.19909307")));
      REQUIRE ((*it)->getHighValue() == refHigh0);

      decimal<7> refLow0 (dec::fromString<decimal<7>> (std::string ("184.32551244")));
      REQUIRE ((*it)->getLowValue() == refLow0);

      decimal<7> refClose0 (dec::fromString<decimal<7>> (std::string ("185.09717533")));
      REQUIRE ((*it)->getCloseValue() == refClose0);
    }

  /*
  SECTION ("Timeseries OHLC test", "[TimeSeriesCsvReader]")
    {
      OHLCTimeSeries<6>::RandomAccessIterator it = p->beginRandomAccess();
      decimal<6> refOpen (dec::fromString<decimal<6>> (std::string ("31.590276")));
      REQUIRE ((*it)->getOpenValue() == refOpen);

      decimal<6> refHigh (dec::fromString<decimal<6>> (std::string ("31.861192")));
      REQUIRE ((*it)->getHighValue() == refHigh);

      decimal<6> refLow (dec::fromString<decimal<6>> (std::string ("31.583147")));
      REQUIRE ((*it)->getLowValue() == refLow);

      decimal<6> refClose (dec::fromString<decimal<6>> (std::string ("31.832674")));
      REQUIRE ((*it)->getCloseValue() == refClose);

      it = it + 18;
      boost::gregorian::date refDate1 (1985, Mar, 27);
      REQUIRE ((*it)->getDateValue() == refDate1);
     decimal<6> refOpen0 (dec::fromString<decimal<6>> (std::string ("31.996650")));
      REQUIRE ((*it)->getOpenValue() == refOpen0);

      decimal<6> refHigh0 (dec::fromString<decimal<6>> (std::string ("32.060813")));
      REQUIRE ((*it)->getHighValue() == refHigh0);

      decimal<6> refLow0 (dec::fromString<decimal<6>> (std::string ("31.911098")));
      REQUIRE ((*it)->getLowValue() == refLow0);

      decimal<6> refClose0 (dec::fromString<decimal<6>> (std::string ("31.946744")));
      REQUIRE ((*it)->getCloseValue() == refClose0);

      it = p->endRandomAccess();
      it--;

      decimal<6> refOpen2 (dec::fromString<decimal<6>> (std::string ("117.794029")));
      REQUIRE ((*it)->getOpenValue() == refOpen2);

      decimal<6> refHigh2 (dec::fromString<decimal<6>> (std::string ("118.216285")));
      REQUIRE ((*it)->getHighValue() == refHigh2);

      decimal<6> refLow2 (dec::fromString<decimal<6>> (std::string ("117.639205")));
      REQUIRE ((*it)->getLowValue() == refLow2);

      decimal<6> refClose2 (dec::fromString<decimal<6>> (std::string ("118.117759")));
      REQUIRE ((*it)->getCloseValue() == refClose2);

    }
  */
}
