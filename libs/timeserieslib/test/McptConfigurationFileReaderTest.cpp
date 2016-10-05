#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../McptConfigurationFileReader.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef decimal<7> DecimalType;

DecimalType
createDecimal(const std::string& valueString)
{
  return fromString<DecimalType>(valueString);
}

date createDate (const std::string& dateString)
{
  return from_undelimited_string(dateString);
}


TEST_CASE ("Security operations", "[Security]")
{
  McptConfigurationFileReader reader("SampleConfig.txt");

  
  std::string symbol("@C");
  std::string SecurityName("Corn Futures");
  decimal<7> cornBigPointValue(createDecimal("50.0"));
  decimal<7> cornTickValue(createDecimal("0.25"));

  std::shared_ptr<McptConfiguration<7>> configuration = reader.readConfigurationFile();
  auto aSecurity = configuration->getSecurity();
  REQUIRE (aSecurity->getSymbol() == symbol);
  REQUIRE (aSecurity->getName() == SecurityName);
  REQUIRE (aSecurity->getBigPointValue() == cornBigPointValue);
  REQUIRE (aSecurity->getTick() == cornTickValue);
  REQUIRE (aSecurity->getFirstDate() == createDate("19850301"));
  REQUIRE (aSecurity->getLastDate() == createDate("20160210"));
  REQUIRE (aSecurity->isFuturesSecurity());
  REQUIRE_FALSE (aSecurity->isEquitySecurity());
  REQUIRE (aSecurity->getTimeSeries()->getTimeFrame() == TimeFrame::DAILY);

  REQUIRE (configuration->getPricePatterns()->getNumPatterns() == 3);
  //REQUIRE (corn2->getTick() == corn.getTick());
  //REQUIRE_FALSE (corn2->isEquitySecurity());
  //REQUIRE (corn2->isFuturesSecurity());


}
