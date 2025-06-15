#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <variant>
#include <algorithm>
#include "ValidatorConfiguration.h"
#include "PALMastersMonteCarloValidation.h"
#include "PALMonteCarloValidation.h"
#include "MonteCarloPermutationTest.h"
#include "MultipleTestingCorrection.h"
#include "PermutationTestComputationPolicy.h"
#include "PermutationTestResultPolicy.h"
#include "MonteCarloTestPolicy.h"
#include "LogPalPattern.h"
#include "number.h"
#include <cstdlib>

using namespace mkc_timeseries;

using Num = num::DefaultNumber;
using StatPolicy = AllHighResLogPFPolicy<Num>;

enum class ValidationMethod {
    Masters,
    RomanoWolf
};

// Base interface for validation methods
class ValidationInterface {
public:
    using SurvivingStrategiesIterator = typename PALMastersMonteCarloValidation<Num, StatPolicy>::SurvivingStrategiesIterator;
    
    virtual ~ValidationInterface() = default;
    virtual void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                                   std::shared_ptr<PriceActionLabSystem> patterns,
                                   const DateRange& dateRange,
                                   const Num& pvalThreshold) = 0;
    virtual SurvivingStrategiesIterator beginSurvivingStrategies() const = 0;
    virtual SurvivingStrategiesIterator endSurvivingStrategies() const = 0;
};

// Wrapper for Masters validation
class MastersValidationWrapper : public ValidationInterface {
private:
    PALMastersMonteCarloValidation<Num, StatPolicy> validation;
    
public:
    explicit MastersValidationWrapper(unsigned long numPermutations)
        : validation(numPermutations) {}
    
    void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold) override {
        validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold);
    }
    
    SurvivingStrategiesIterator beginSurvivingStrategies() const override {
        return validation.beginSurvivingStrategies();
    }
    
    SurvivingStrategiesIterator endSurvivingStrategies() const override {
        return validation.endSurvivingStrategies();
    }
};

// Wrapper for Romano-Wolf validation
class RomanoWolfValidationWrapper : public ValidationInterface {
private:
    PALMonteCarloValidation<Num,
                           MonteCarloPermuteMarketChanges<Num, AllHighResLogPFPolicy, DefaultPermuteMarketChangesPolicy<Num, StatPolicy, PValueAndTestStatisticReturnPolicy<Num>>>,
                           RomanoWolfStepdownCorrection> validation;
    
public:
    explicit RomanoWolfValidationWrapper(unsigned long numPermutations)
        : validation(numPermutations) {}
    
    void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold) override {
        validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold);
    }
    
    SurvivingStrategiesIterator beginSurvivingStrategies() const override {
        return validation.beginSurvivingStrategies();
    }
    
    SurvivingStrategiesIterator endSurvivingStrategies() const override {
        return validation.endSurvivingStrategies();
    }
};

// Factory function to create validation objects
std::unique_ptr<ValidationInterface> createValidation(ValidationMethod method, unsigned long numPermutations) {
    switch (method) {
        case ValidationMethod::Masters:
            return std::make_unique<MastersValidationWrapper>(numPermutations);
        case ValidationMethod::RomanoWolf:
            return std::make_unique<RomanoWolfValidationWrapper>(numPermutations);
        default:
            throw std::invalid_argument("Unknown validation method");
    }
}

void usage()
{
  printf("Usage: PalValidator <configuration file> [permutations count] [p-value threshold] [validation method]\n");
  printf("  configuration file: Required - path to the configuration file\n");
  printf("  permutations count: Optional - number of permutations (will prompt if not provided)\n");
  printf("  p-value threshold:  Optional - p-value threshold (will prompt if not provided)\n");
  printf("  validation method:  Optional - validation method: 'masters' or 'romano-wolf' (will prompt if not provided)\n\n");
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
		return error_with_usage();
	}

	const auto args = std::vector<std::string>(argv, argv + argc);
	const auto configurationFileName = std::string(args[1]);
	
	// Handle permutations count
	auto permutations = 5000;
	if (args.size() >= 3 && !args[2].empty()) {
		permutations = std::stoi(args[2]);
	} else {
		std::cout << "Enter number of permutations (default 5000): ";
		std::string input;
		std::getline(std::cin, input);
		if (!input.empty()) {
			permutations = std::stoi(input);
		}
	}
	
	// Handle p-value threshold
	auto pvalThreshold = Num(0.05);
	if (args.size() >= 4 && !args[3].empty()) {
		pvalThreshold = Num(std::stod(args[3]));
	} else {
		std::cout << "Enter p-value threshold (default 0.05): ";
		std::string input;
		std::getline(std::cin, input);
		if (!input.empty()) {
			pvalThreshold = Num(std::stod(input));
		}
	}
	
	// Handle validation method
	ValidationMethod validationMethod = ValidationMethod::Masters; // Default to Masters
	if (args.size() >= 5 && !args[4].empty()) {
		std::string methodStr = args[4];
		std::transform(methodStr.begin(), methodStr.end(), methodStr.begin(), ::tolower);
		if (methodStr == "masters") {
			validationMethod = ValidationMethod::Masters;
		} else if (methodStr == "romano-wolf") {
			validationMethod = ValidationMethod::RomanoWolf;
		} else {
			std::cout << "Warning: Unknown validation method '" << args[4] << "'. Using Masters method." << std::endl;
		}
	} else {
		std::cout << "Choose validation method:" << std::endl;
		std::cout << "1. Masters validation (default)" << std::endl;
		std::cout << "2. Romano-Wolf validation" << std::endl;
		std::cout << "Enter choice (1 or 2, default 1): ";
		std::string input;
		std::getline(std::cin, input);
		if (!input.empty()) {
			if (input == "2" || input == "romano-wolf" || input == "Romano-Wolf") {
				validationMethod = ValidationMethod::RomanoWolf;
			} else if (input != "1" && input != "masters" && input != "Masters") {
				std::cout << "Invalid choice. Using Masters method." << std::endl;
			}
		}
	}
	
	ValidatorConfigurationFileReader reader(configurationFileName);

	const auto config = reader.readConfigurationFile();
	
	// Create validation object using factory
	auto validation = createValidation(validationMethod, permutations);
	
	// Display selected method
	std::cout << "Using " << (validationMethod == ValidationMethod::Masters ? "Masters" : "Romano-Wolf")
			  << " validation method with " << permutations << " permutations." << std::endl;

	validation->runPermutationTests(config->getSecurity(),
								   config->getPricePatterns(),
								   config->getInsampleDateRange(),
								   pvalThreshold);

	for (auto it = validation->beginSurvivingStrategies();
		it != validation->endSurvivingStrategies();
		++it) {
		LogPalPattern::LogPattern ((*it)->getPalPattern(), std::cout);
	}

	return 0;
}

