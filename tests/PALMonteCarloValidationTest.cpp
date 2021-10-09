#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include "PALMonteCarloValidation.h"
#include "PermutationTestComputationPolicy.h"

#include "number.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;
typedef num::DefaultNumber DecimalType;

namespace {


DecimalType
createDecimal(const std::string& valueString)
{
  return num::fromString<DecimalType>(valueString);
}

date createDate (const std::string& dateString)
{
  return from_undelimited_string(dateString);
}

}

TEST_CASE ("PALMonteCarloValidationTest-Security operations", "[Security]")
{
  //McptConfigurationFileReader reader("C2Config.txt");
  std::shared_ptr<RunParameters> parameters = std::make_shared<RunParameters>();
  McptConfigurationFileReader reader(parameters);

  std::shared_ptr<McptConfiguration<DecimalType>> configuration = reader.readConfigurationFile();
  PALMonteCarloValidation<
          DecimalType,
          BestOfMonteCarloPermuteMarketChanges<DecimalType, NormalizedReturnPolicy, MultiStrategyPermuteMarketChangesPolicy<DecimalType, NormalizedReturnPolicy<DecimalType>>>,
          UnadjustedPValueStrategySelection> validation(configuration, 300);
  validation.runPermutationTests();

  // Note to write out surviving patterns, iterate over surviving patterns on
  // PALMonteCarloValidation and use the LogPalPattern class
}
