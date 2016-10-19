#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../PALMonteCarloValidation.h"

#include "number.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef num::DefaultNumber DecimalType;

DecimalType
createDecimal(const std::string& valueString)
{
  return num::fromString<DecimalType>(valueString);
}

date createDate (const std::string& dateString)
{
  return from_undelimited_string(dateString);
}


TEST_CASE ("Security operations", "[Security]")
{
  //McptConfigurationFileReader reader("C2Config.txt");
  McptConfigurationFileReader reader("SampleConfig.txt");

  std::shared_ptr<McptConfiguration<DecimalType>> configuration = reader.readConfigurationFile();
  PALMonteCarloValidation<DecimalType, MonteCarloPermuteMarketChanges<DecimalType>> validation(configuration, 300);
  validation.runPermutationTests();

  // Note to write out surviving patterns, iterate over surviving patterns on
  // PALMonteCarloValidation and use the LogPalPattern class
}
