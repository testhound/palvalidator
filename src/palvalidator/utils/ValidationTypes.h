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
 * @brief Enumeration of pipeline execution modes
 */
enum class PipelineMode
{
    PermutationAndBootstrap,  ///< Full pipeline: permutation + bootstrap + write survivors
    PermutationOnly,          ///< Permutation testing only + write survivors
    BootstrapOnly            ///< Bootstrap only using survivors from previous run
};

/**
 * @brief Parameters for validation configuration
 */
struct ValidationParameters
{
    unsigned long permutations;                  ///< Number of Monte Carlo permutations
    num::DefaultNumber pValueThreshold;          ///< P-value threshold for significance
    num::DefaultNumber falseDiscoveryRate;       ///< False Discovery Rate for Benjamini-Hochberg
    PipelineMode pipelineMode;                   ///< Pipeline execution mode
    std::string survivorInputFile;               ///< Input file for bootstrap-only mode
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

/**
 * @brief Convert PipelineMode enum to string representation
 * @param mode The pipeline mode to convert
 * @return String representation of the pipeline mode
 * @throws std::invalid_argument if mode is unknown
 */
std::string getPipelineModeString(PipelineMode mode);

} // namespace utils
} // namespace palvalidator