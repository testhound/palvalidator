#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "SecurityAttributesFactory.h"
#include "McptConfigurationFileReader.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("McptConfigurationFileReaderTest-Security operations", "[Security]")
{
  std::shared_ptr<RunParameters> parameters = std::make_shared<RunParameters>();
  parameters->setUseApi(false);
  parameters->setEodDataFilePath("./CL_RAD.txt");
  parameters->setConfig1FilePath("./CL_R1_0_Dev1_Config.txt");

  McptConfigurationFileReader reader(parameters);

  std::string symbol("@CL");
  SecurityAttributesFactory<DecimalType> factory;
  SecurityAttributesFactory<DecimalType>::SecurityAttributesIterator it = factory.getSecurityAttributes(symbol);
  REQUIRE_FALSE (it == factory.endSecurityAttributes());

  auto attributes = it->second;

  std::string SecurityName("Corn Futures");

  std::shared_ptr<McptConfiguration<DecimalType>> configuration = reader.readConfigurationFile();
  auto aSecurity = configuration->getSecurity();
  REQUIRE (aSecurity->getSymbol() == attributes->getSymbol());
  REQUIRE (aSecurity->getName() == attributes->getName());
  REQUIRE (aSecurity->getBigPointValue() == attributes->getBigPointValue());
  REQUIRE (aSecurity->getTick() == attributes->getTick());
  REQUIRE (aSecurity->getFirstDate() == configuration->getInsampleDateRange().getFirstDate());
  REQUIRE (aSecurity->getLastDate() == configuration->getOosDateRange().getLastDate());
  REQUIRE (aSecurity->isFuturesSecurity());
  REQUIRE_FALSE (aSecurity->isEquitySecurity());
  REQUIRE (aSecurity->getTimeSeries()->getTimeFrame() == TimeFrame::DAILY);
}
