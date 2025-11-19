#include "PolicyFactory.h"
#include "ValidationInterface.h"
#include "PALMastersMonteCarloValidation.h"
#include "PALRomanoWolfMonteCarloValidation.h"
#include "PALMonteCarloValidation.h"
#include "MonteCarloPermutationTest.h"
#include "MultipleTestingCorrection.h"
#include "MonteCarloTestPolicy.h"
#include "PermutationStatisticsCollector.h"
#include "number.h"

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

namespace statistics {

// Static member definitions
std::unordered_map<std::string, PolicyFactory::MastersFactoryFunction> PolicyFactory::mastersFactories_;
std::unordered_map<std::string, PolicyFactory::RomanoWolfFactoryFunction> PolicyFactory::romanoWolfFactories_;
std::unordered_map<std::string, PolicyFactory::BenjaminiHochbergFactoryFunction> PolicyFactory::benjaminiHochbergFactories_;
std::unordered_map<std::string, PolicyFactory::UnadjustedFactoryFunction> PolicyFactory::unadjustedFactories_;

// Masters Validation Wrapper Template
template<template<typename> class PolicyType>
class MastersValidationWrapper : public ValidationInterface
{
private:
    PALMastersMonteCarloValidation<Num, PolicyType<Num>> validation;
public:
    explicit MastersValidationWrapper(unsigned long p) : validation(p) {}

    void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold,
                           bool verbose = false,
                           bool partitionByFamily = false) override
    {
        validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, verbose, partitionByFamily);
    }
    
    std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const override
    {
        std::vector<std::shared_ptr<PalStrategy<Num>>> s;
        for (auto it = validation.beginSurvivingStrategies(); it != validation.endSurvivingStrategies(); ++it)
            s.push_back(*it);
        return s;
    }
    
    int getNumSurvivingStrategies() const override
    {
        return validation.getNumSurvivingStrategies();
    }
    
    const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
    {
        return validation.getStatisticsCollector();
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
    {
        return validation.getAllTestedStrategies();
    }

    Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> s) const override
    {
        return validation.getStrategyPValue(s);
    }
};

// Romano-Wolf Validation Wrapper Template
template<template<typename> class PolicyType>
class RomanoWolfValidationWrapper : public ValidationInterface
{
private:
    PALRomanoWolfMonteCarloValidation<Num, PolicyType<Num>> validation;
public:
    explicit RomanoWolfValidationWrapper(unsigned long p) : validation(p) {}
    
    void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold,
                           bool verbose = false,
                           bool partitionByFamily = false) override
    {
        validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, verbose, partitionByFamily);
    }
    
    std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const override
    {
        std::vector<std::shared_ptr<PalStrategy<Num>>> s;
        for (auto it = validation.beginSurvivingStrategies(); it != validation.endSurvivingStrategies(); ++it)
            s.push_back(*it);
        return s;
    }
    
    int getNumSurvivingStrategies() const override
    {
        return validation.getNumSurvivingStrategies();
    }
    
    const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
    {
        throw std::runtime_error("Not supported for RomanoWolf");
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
    {
        return validation.getAllTestedStrategies();
    }
    
    Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> s) const override
    {
        return validation.getStrategyPValue(s);
    }
};

// Benjamini-Hochberg Validation Wrapper Template
template<template<typename> class PolicyType>
class BenjaminiHochbergValidationWrapper : public ValidationInterface
{
private:
    using BenjaminiMcpt = MonteCarloPermuteMarketChanges<
        Num,
        PolicyType,
        DefaultPermuteMarketChangesPolicy<Num, PolicyType<Num>>
    >;
    PALMonteCarloValidation<Num, BenjaminiMcpt, AdaptiveBenjaminiHochbergYr2000> validation;
    
public:
    explicit BenjaminiHochbergValidationWrapper(unsigned long p, const Num& fdr)
        : validation(p, fdr) {}
    
    void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold,
                           bool verbose = false,
                           bool partitionByFamily = false) override
    {
        validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, verbose, partitionByFamily);
    }
    
    std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const override
    {
        std::vector<std::shared_ptr<PalStrategy<Num>>> s;
        for (auto it = validation.beginSurvivingStrategies(); it != validation.endSurvivingStrategies(); ++it)
            s.push_back(*it);
        return s;
    }
    
    int getNumSurvivingStrategies() const override
    {
        return validation.getNumSurvivingStrategies();
    }
    
    const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
    {
        return validation.getStatisticsCollector();
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
    {
        return validation.getAllTestedStrategies();
    }
    
    Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> s) const override
    {
        return validation.getStrategyPValue(s);
    }
};

// Unadjusted Validation Wrapper Template
template<template<typename> class PolicyType>
class UnadjustedValidationWrapper : public ValidationInterface
{
private:
    using UnadjustedMcpt = MonteCarloPermuteMarketChanges<
        Num,
        PolicyType,
        DefaultPermuteMarketChangesPolicy<Num, PolicyType<Num>,
            PValueReturnPolicy<Num>,
            PermutationTestingNullTestStatisticPolicy<Num>,
            concurrency::ThreadPoolExecutor<>,
					  WilsonPValueComputationPolicy<Num>,
					  SyntheticNullModel::N1_MaxDestruction>
    >;
  PALMonteCarloValidation<Num, UnadjustedMcpt, UnadjustedPValueStrategySelection, concurrency::StdAsyncExecutor> validation;
    
public:
    explicit UnadjustedValidationWrapper(unsigned long p) : validation(p) {}
    
    void runPermutationTests(std::shared_ptr<Security<Num>> baseSecurity,
                           std::shared_ptr<PriceActionLabSystem> patterns,
                           const DateRange& dateRange,
                           const Num& pvalThreshold,
                           bool verbose = false,
                           bool partitionByFamily = false) override
    {
        validation.runPermutationTests(baseSecurity, patterns, dateRange, pvalThreshold, verbose, partitionByFamily);
    }
    
    std::vector<std::shared_ptr<PalStrategy<Num>>> getSurvivingStrategies() const override
    {
        std::vector<std::shared_ptr<PalStrategy<Num>>> s;
        for (auto it = validation.beginSurvivingStrategies(); it != validation.endSurvivingStrategies(); ++it)
            s.push_back(*it);
        return s;
    }
    
    int getNumSurvivingStrategies() const override
    {
        return validation.getNumSurvivingStrategies();
    }
    
    const PermutationStatisticsCollector<Num>& getStatisticsCollector() const override
    {
        return validation.getStatisticsCollector();
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> getAllTestedStrategies() const override
    {
        return validation.getAllTestedStrategies();
    }
    
    Num getStrategyPValue(std::shared_ptr<PalStrategy<Num>> s) const override
    {
        return validation.getStrategyPValue(s);
    }
};

// Template specializations for all policy types
// Basic Policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>>(permutations);
}

// Return-based Policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>>(permutations);
}

// PAL-specific Policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>>(permutations);
}

// Enhanced and Hybrid Policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>>(permutations);
}

// Swing Trading Policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>>(permutations);
}

// Bootstrap-based Policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>(unsigned long permutations)
{
    return std::make_unique<MastersValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>>(permutations);
}

// Romano-Wolf specializations for all policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>(unsigned long permutations)
{
    return std::make_unique<RomanoWolfValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>>(permutations);
}

// Benjamini-Hochberg specializations for all policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>>(permutations, Num(falseDiscoveryRate));
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>(
    unsigned long permutations, double falseDiscoveryRate)
{
    return std::make_unique<BenjaminiHochbergValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>>(permutations, Num(falseDiscoveryRate));
}

// Unadjusted specializations for all policies
template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::AllHighResLogPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::RobustProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::NonGranularProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::CumulativeReturnPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::NormalizedReturnPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::PessimisticReturnRatioPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::PalProfitabilityPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::GatedPerformanceScaledPalPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::ConfidenceAdjustedPalPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::EnhancedBarScorePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::HybridEnhancedTradeAwarePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::AccumulationSwingIndexPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::HybridSwingTradePolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::ProfitFactorGatedSwingPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::BootStrappedProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::BootStrappedLogProfitFactorPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::BootStrappedProfitabilityPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::BootStrappedLogProfitabilityPFPolicy>>(permutations);
}

template<>
std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>(unsigned long permutations)
{
    return std::make_unique<UnadjustedValidationWrapper<mkc_timeseries::BootStrappedSharpeRatioPolicy>>(permutations);
}

// Public factory methods
std::unique_ptr<ValidationInterface> PolicyFactory::createMastersValidation(
    const std::string& policyName,
    unsigned long permutations)
{
    auto it = mastersFactories_.find(policyName);
    if (it == mastersFactories_.end()) {
        throw std::invalid_argument("Policy not registered for Masters validation: " + policyName);
    }
    return it->second(permutations);
}

std::unique_ptr<ValidationInterface> PolicyFactory::createRomanoWolfValidation(
    const std::string& policyName,
    unsigned long permutations)
{
    auto it = romanoWolfFactories_.find(policyName);
    if (it == romanoWolfFactories_.end()) {
        throw std::invalid_argument("Policy not registered for Romano-Wolf validation: " + policyName);
    }
    return it->second(permutations);
}

std::unique_ptr<ValidationInterface> PolicyFactory::createBenjaminiHochbergValidation(
    const std::string& policyName,
    unsigned long permutations,
    double falseDiscoveryRate)
{
    auto it = benjaminiHochbergFactories_.find(policyName);
    if (it == benjaminiHochbergFactories_.end()) {
        throw std::invalid_argument("Policy not registered for Benjamini-Hochberg validation: " + policyName);
    }
    return it->second(permutations, falseDiscoveryRate);
}

std::unique_ptr<ValidationInterface> PolicyFactory::createUnadjustedValidation(
    const std::string& policyName,
    unsigned long permutations)
{
    auto it = unadjustedFactories_.find(policyName);
    if (it == unadjustedFactories_.end()) {
        throw std::invalid_argument("Policy not registered for Unadjusted validation: " + policyName);
    }
    return it->second(permutations);
}

} // namespace statistics
