#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../PALMonteCarloValidation.h"

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
  //McptConfigurationFileReader reader("C2Config.txt");
  McptConfigurationFileReader reader("SampleConfig.txt");
  
  std::shared_ptr<McptConfiguration<7>> configuration = reader.readConfigurationFile();
  PALMonteCarloValidation<7, MonteCarloPermuteMarketChanges<7>> validation(configuration, 300);
  validation.runPermutationTests();

  // Note to write out surviving patterns, iterate over surviving patterns on
  // PALMonteCarloValidation and use the LogPalPattern class
}
