#pragma once

#include "number.h"

namespace palvalidator
{
namespace utils
{

/**
 * @brief Enumeration of available validation methods for Monte Carlo testing
 */
enum class ValidationMethod
{
    Masters,           ///< Masters multiple testing correction
    RomanoWolf,        ///< Romano-Wolf stepdown procedure
    BenjaminiHochberg, ///< Benjamini-Hochberg FDR control
    Unadjusted         ///< No multiple testing correction
};

/**
 * @brief Parameters for validation configuration
 */
struct ValidationParameters
{
    unsigned long permutations;                  ///< Number of Monte Carlo permutations
    num::DefaultNumber pValueThreshold;          ///< P-value threshold for significance
    num::DefaultNumber falseDiscoveryRate;       ///< False Discovery Rate for Benjamini-Hochberg
};

/**
 * @brief Risk parameters for performance evaluation
 */
struct RiskParameters
{
    num::DefaultNumber riskFreeRate;  ///< Risk-free rate of return
    num::DefaultNumber riskPremium;   ///< Risk premium over risk-free rate
};

/**
 * @brief Convert ValidationMethod enum to string representation
 * @param method The validation method to convert
 * @return String representation of the validation method
 * @throws std::invalid_argument if method is unknown
 */
std::string getValidationMethodString(ValidationMethod method);

} // namespace utils
} // namespace palvalidator