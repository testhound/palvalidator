#include <string>
#include <vector>
#include <stdio.h>
#include "ValidatorConfiguration.h"
#include "PALMastersMonteCarloValidation.h"
#include "MonteCarloTestPolicy.h"
#include "LogPalPattern.h"
#include "number.h"
#include <cstdlib>

using namespace mkc_timeseries;

using Num = num::DefaultNumber;
using StatPolicy = AllHighResLogPFPolicy<Num>;

void usage()
{
  printf("Usage: PalValidator <configuration file> <permutations count> <p-value threshold>\n\n");
}


int error_with_usage()
{
  usage();
  return 1;
}

template<typename T>
T with_default(std::string) {

}

int main(int argc, char **argv)
{
	if (argc < 2) {
		error_with_usage();
	}

	const auto args = std::vector<std::string>(argv, argv + argc);
	const auto configurationFileName = std::string(args[1]);
	auto permutations = 5000;
	if (args.size() >= 3 && !args[2].empty()) {
		permutations = std::stoi(args[2]);
	}
	auto pvalThreshold = Num(0.05);
	if (args.size() >= 4 && !args[3].empty()) {
		pvalThreshold = Num(std::stod(args[3]));
	}
	ValidatorConfigurationFileReader reader(configurationFileName);

	const auto config = reader.readConfigurationFile();
	PALMastersMonteCarloValidation<Num, StatPolicy> validation(permutations);

	validation.runPermutationTests(config->getSecurity(),
								config->getPricePatterns(),
								config->getInsampleDateRange(),
								pvalThreshold);

	for (auto it = validation.beginSurvivingStrategies();
		it != validation.endSurvivingStrategies();
		++it) {
		LogPalPattern::LogPattern ((*it)->getPalPattern(), std::cout);
	}

	return 0;
}

