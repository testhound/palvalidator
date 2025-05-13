#include <catch2/catch_test_macros.hpp>
#include "ValidatorConfiguration.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;


TEST_CASE ("Security operations", "[Security]")
{
  ValidatorConfigurationFileReader reader("QQQ_config.txt");

  
  std::string symbol("QQQ");
    DecimalType qqqBigPointValue(createDecimal("1.0"));
  DecimalType qqqTickValue(createDecimal("0.01"));

  auto oosFirstDate = createDate("20210820");
  auto oosLastDate = createDate("20250331");
  
  std::shared_ptr<ValidatorConfiguration<DecimalType>> configuration = reader.readConfigurationFile();
  auto aSecurity = configuration->getSecurity();
  REQUIRE (aSecurity->getSymbol() == symbol);
    REQUIRE (aSecurity->getBigPointValue() == qqqBigPointValue);
  REQUIRE (aSecurity->getTick() == qqqTickValue);
  REQUIRE (aSecurity->getFirstDate() == createDate("20070402"));
  REQUIRE (aSecurity->getLastDate() == createDate("20250331"));
  REQUIRE (aSecurity->isEquitySecurity());
  REQUIRE (aSecurity->getTimeSeries()->getTimeFrame() == TimeFrame::DAILY);

  REQUIRE (configuration->getPricePatterns()->getNumPatterns() == 7);
  DateRange inSampleDateRange = configuration->getInsampleDateRange();
  DateRange ooSampleDateRange = configuration->getOosDateRange();

  
  REQUIRE (inSampleDateRange.getFirstDate() == createDate("20070402"));
  REQUIRE (inSampleDateRange.getLastDate() == createDate("20210819"));
  REQUIRE (ooSampleDateRange.getFirstDate() == oosFirstDate);
  REQUIRE (ooSampleDateRange.getLastDate() == oosLastDate);

  auto oosBackTester = configuration->getBackTester();
  REQUIRE (oosBackTester->numBackTestRanges() == 1);
  REQUIRE (oosBackTester->getStartDate() == oosFirstDate);
  REQUIRE (oosBackTester->getEndDate() == oosLastDate);
}
