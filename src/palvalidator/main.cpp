#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <variant>
#include <algorithm>
#include <set>
#include <numeric>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <sstream>
#include <filesystem>
#include "ValidatorConfiguration.h"
#include "SecurityAttributesFactory.h"
#include "PALMastersMonteCarloValidation.h"
#include "PALRomanoWolfMonteCarloValidation.h"
#include "PALMonteCarloValidation.h"
#include "MonteCarloPermutationTest.h"
#include "MultipleTestingCorrection.h"
#include "PermutationTestComputationPolicy.h"
#include "PermutationTestResultPolicy.h"
#include "MonteCarloTestPolicy.h"
#include "PermutationStatisticsCollector.h"
#include "LogPalPattern.h"
#include "number.h"
#include <cstdlib>

// New policy architecture includes
#include "PolicyRegistry.h"
#include "PolicyConfiguration.h"
#include "PolicyFactory.h"
#include "PolicySelector.h"
#include "PolicyRegistration.h"
#include "ValidationInterface.h"
#include "BiasCorrectedBootstrap.h"

using namespace mkc_timeseries;

using Num = num::DefaultNumber;

// ---- Enums, Structs, and Helper Functions ----

enum class ValidationMethod
{
    Masters,
    RomanoWolf,
    BenjaminiHochberg,
    Unadjusted
};

// ComputationPolicy enum removed - now using dynamic policy selection

struct ValidationParameters
{
    unsigned long permutations;
    Num pValueThreshold;
    Num falseDiscoveryRate; // For Benjamini-Hochberg
};

std::string getValidationMethodString(ValidationMethod method)
{
    switch (method)
    {
        case ValidationMethod::Masters:
            return "Masters";
        case ValidationMethod::RomanoWolf:
            return "RomanoWolf";
        case ValidationMethod::BenjaminiHochberg:
            return "BenjaminiHochberg";
        case ValidationMethod::Unadjusted:
            return "Unadjusted";
        default:
            throw std::invalid_argument("Unknown validation method");
    }
}

// getComputationPolicyString function removed - now using dynamic policy names

static std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%b_%d_%Y_%H%M");
    return ss.str();
}

static std::string createSurvivingPatternsFileName (const std::string& securitySymbol, ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

static std::string createDetailedSurvivingPatternsFileName (const std::string& securitySymbol,
                                                            ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_SurvivingPatterns_" + getCurrentTimestamp() + ".txt";
}

static std::string createDetailedRejectedPatternsFileName(const std::string& securitySymbol,
                                                          ValidationMethod method)
{
    std::string methodDir = getValidationMethodString(method);
    std::filesystem::create_directories(methodDir);
    return methodDir + "/" + securitySymbol + "_" + getValidationMethodString(method) + "_Detailed_RejectedPatterns_" + getCurrentTimestamp() + ".txt";
}

void writeBacktestPerformanceReport(std::ofstream& file, std::shared_ptr<BackTester<Num>> backtester)
{
    auto positionHistory = backtester->getClosedPositionHistory();
    
    // Write performance metrics to file
    file << "=== Backtest Performance Report ===" << std::endl;
    file << "Total Closed Positions: " << positionHistory.getNumPositions() << std::endl;
    file << "Number of Winning Trades: " << positionHistory.getNumWinningPositions() << std::endl;
    file << "Number of Losing Trades: " << positionHistory.getNumLosingPositions() << std::endl;
    file << "Total Bars in Market: " << positionHistory.getNumBarsInMarket() << std::endl;
    file << "Percent Winners: " << positionHistory.getPercentWinners() << "%" << std::endl;
    file << "Percent Losers: " << positionHistory.getPercentLosers() << "%" << std::endl;
    file << "Profit Factor: " << positionHistory.getProfitFactor() << std::endl;
    file << "High Resolution Profit Factor: " << positionHistory.getHighResProfitFactor() << std::endl;
    file << "PAL Profitability: " << positionHistory.getPALProfitability() << "%" << std::endl;
    file << "High Resolution Profitability: " << positionHistory.getHighResProfitability() << std::endl;
    file << "===================================" << std::endl << std::endl;
}

// Calculate theoretical PAL profitability based on strategy's risk/reward parameters
template<typename Num>
Num calculateTheoreticalPALProfitability(std::shared_ptr<PalStrategy<Num>> strategy,
                                         Num targetProfitFactor = DecimalConstants<Num>::DecimalTwo)
{
    auto pattern = strategy->getPalPattern();
    Num target = pattern->getProfitTargetAsDecimal();
    Num stop = pattern->getStopLossAsDecimal();
    
    if (stop == DecimalConstants<Num>::DecimalZero) {
        return DecimalConstants<Num>::DecimalZero;
    }
    
    Num payoffRatio = target / stop;
    Num oneHundred = DecimalConstants<Num>::DecimalOneHundred;
    
    // Formula from BootStrappedProfitabilityPFPolicy::getPermutationTestStatistic
    Num expectedPALProfitability = (targetProfitFactor / (targetProfitFactor + payoffRatio)) * oneHundred;
    
    return expectedPALProfitability;
}

// Filter surviving strategies based on BCa bootstrap performance

template<typename Num>
std::vector<std::shared_ptr<PalStrategy<Num>>>
filterSurvivingStrategiesByPerformance(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration theTimeFrame)
{
  std::vector<std::shared_ptr<PalStrategy<Num>>> filteredStrategies;

  // --- Define Filtering Parameters ---
  const Num costBufferMultiplier = Num("1.5");
  const Num riskFreeRate = Num("0.03");
  const Num riskFreeMultiplier = Num("2.0");
  const Num riskFreeHurdle = riskFreeRate * riskFreeMultiplier;

  std::cout << "\nFiltering " << survivingStrategies.size() << " surviving strategies by BCa performance..." << std::endl;
  std::cout << "Filter 1 (Statistical Viability): Annualized Lower Bound > 0" << std::endl;
  std::cout << "Filter 2 (Economic Significance): Annualized Lower Bound > (Annualized Cost Hurdle * " << costBufferMultiplier << ")" << std::endl;
  std::cout << "Filter 3 (Risk-Adjusted Return): Annualized Lower Bound > (Risk-Free Rate * " << riskFreeMultiplier << ")" << std::endl;
  std::cout << "  - Cost assumptions: $0 commission, 0.01% slippage/spread per side." << std::endl;
  std::cout << "  - Risk-Free Rate assumption: " << (riskFreeRate * DecimalConstants<Num>::DecimalOneHundred) << "%." << std::endl;

  for (const auto& strategy : survivingStrategies)
  {
    try
    {
      // 1. Create fresh portfolio and clone strategy for backtesting
      auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
      freshPortfolio->addSecurity(baseSecurity);
      auto clonedStrat = strategy->clone2(freshPortfolio);

      // 2. Run backtest to get the full return series
      auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat, theTimeFrame, backtestingDates);

      // 3. Get the high-resolution returns
      auto highResReturns = backtester->getAllHighResReturns(clonedStrat.get());

      // A strategy must have a minimum number of returns to be bootstrapped reliably
      if (highResReturns.size() < 20)
      {
        std::cout << "✗ Strategy filtered out: " << strategy->getStrategyName()
                  << " - Insufficient returns for bootstrap (" << highResReturns.size() << " < 20)." << std::endl;
        continue;
      }

      // 4. Perform BCa Bootstrap (geometric mean)
      unsigned int num_resamples = 2000;
      double confidence_level = 0.95;
      GeoMeanStat<Num> stat; // geometric mean statistic

      BCaBootStrap<Num> bcaGeo(highResReturns, num_resamples, confidence_level, stat);

      // 5. Annualize the results
      double annualizationFactor;
      if (theTimeFrame == TimeFrame::INTRADAY)
        annualizationFactor = calculateAnnualizationFactor(theTimeFrame,
                                  baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes());
      else
        annualizationFactor = calculateAnnualizationFactor(theTimeFrame);

      BCaAnnualizer<Num> annualizerGeo(bcaGeo, annualizationFactor);

      Num annualizedLowerBoundGeo = annualizerGeo.getAnnualizedLowerBound();

      // 6. Calculate Hurdles
      // Cost-based hurdle

      const Num slippagePerSide = Num("0.001"); // 0.10% per side
      const Num slippagePerRoundTrip = slippagePerSide * DecimalConstants<Num>::DecimalTwo; // 0.20% per trade
      Num annualizedTrades(backtester->getEstimatedAnnualizedTrades());
      Num annualizedCostHurdle = annualizedTrades * slippagePerRoundTrip;
      Num costBasedRequiredReturn = annualizedCostHurdle * costBufferMultiplier;

      // The final hurdle is the HIGHER of the cost-based return and the risk-free return
      Num finalRequiredReturn = std::max(costBasedRequiredReturn, riskFreeHurdle);

      // 7. Apply Combined Filter
      if (annualizedLowerBoundGeo > finalRequiredReturn)
      {
        filteredStrategies.push_back(strategy);

        // ALSO compute/print arithmetic-mean lower bound for comparison (only when passing)
        BCaBootStrap<Num> bcaMean(highResReturns, num_resamples, confidence_level); // default statistic: arithmetic mean
        BCaAnnualizer<Num> annualizerMean(bcaMean, annualizationFactor);
        Num annualizedLowerBoundMean = annualizerMean.getAnnualizedLowerBound();

        std::cout << "✓ Strategy passed: " << strategy->getStrategyName()
                  << " (Lower Bound = " << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
                  << "% > Required Return = " << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
                  << std::endl;

        std::cout << "   ↳ Lower bounds (annualized): "
                  << "GeoMean = " << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred) << "%, "
                  << "Mean = "    << (annualizedLowerBoundMean * DecimalConstants<Num>::DecimalOneHundred) << "%"
                  << std::endl;
      }
      else
      {
        std::cout << "✗ Strategy filtered out: " << strategy->getStrategyName()
                  << " (Lower Bound = " << (annualizedLowerBoundGeo * DecimalConstants<Num>::DecimalOneHundred)
                  << "% <= Required Return = " << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%)"
                  << std::endl;
      }
    }
    catch (const std::exception& e)
    {
      std::cout << "Warning: Failed to evaluate strategy '" << strategy->getStrategyName()
                << "' performance: " << e.what() << std::endl;
      std::cout << "Excluding strategy from filtered results." << std::endl;
    }
  }

  std::cout << "\nBCa Performance Filtering complete: " << filteredStrategies.size() << "/"
            << survivingStrategies.size() << " strategies passed criteria." << std::endl;

  return filteredStrategies;
}

template<typename Num>
void filterMetaStrategy(
    const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
    std::shared_ptr<Security<Num>> baseSecurity,
    const DateRange& backtestingDates,
    TimeFrame::Duration theTimeFrame)
{
  if (survivingStrategies.empty()) {
    std::cout << "\n[Meta] No surviving strategies to aggregate.\n";
    return;
  }

  std::cout << "\n[Meta] Building equal-weight portfolio from "
            << survivingStrategies.size() << " survivors...\n";

  // Gather per-strategy high-res returns and annualized trade counts
  std::vector<std::vector<Num>> survivorReturns;
  survivorReturns.reserve(survivingStrategies.size());
  std::vector<Num> survivorAnnualizedTrades;
  survivorAnnualizedTrades.reserve(survivingStrategies.size());

  size_t T = std::numeric_limits<size_t>::max();

  for (const auto& strat : survivingStrategies) {
    try {
      auto freshPortfolio = std::make_shared<Portfolio<Num>>(strat->getStrategyName() + " Portfolio");
      freshPortfolio->addSecurity(baseSecurity);
      auto cloned = strat->clone2(freshPortfolio);

      auto bt = BackTesterFactory<Num>::backTestStrategy(cloned, theTimeFrame, backtestingDates);
      auto r  = bt->getAllHighResReturns(cloned.get());

      if (r.size() < 2) continue; // skip pathological

      T = std::min(T, r.size());
      survivorReturns.push_back(std::move(r));
      survivorAnnualizedTrades.push_back(Num(bt->getEstimatedAnnualizedTrades()));
    }
    catch (const std::exception& e) {
      std::cout << "  [Meta] Skipping " << strat->getStrategyName() << " due to error: " << e.what() << "\n";
    }
  }

  if (survivorReturns.empty() || T < 2) {
    std::cout << "[Meta] Not enough aligned data to form portfolio.\n";
    return;
  }

  // Equal-weight portfolio series (truncate to shortest length T)
  const size_t n = survivorReturns.size();
  const Num w = Num(1) / Num(static_cast<int>(n));

  std::vector<Num> metaReturns(T, DecimalConstants<Num>::DecimalZero);
  for (size_t i = 0; i < n; ++i) {
    for (size_t t = 0; t < T; ++t) {
      metaReturns[t] += w * survivorReturns[i][t];
    }
  }

  {
    const Num am = StatUtils<Num>::computeMean(metaReturns);
    const Num gm = GeoMeanStat<Num>{}(metaReturns);
    std::cout << "      Per-period point estimates (pre-annualization): "
              << "Arithmetic mean =" << (am * DecimalConstants<Num>::DecimalOneHundred) << "%, "
              << "Geometric mean =" << (gm * DecimalConstants<Num>::DecimalOneHundred) << "%\n";
  }
  
  // Annualization factor (same logic as strategy-level)
  double annualizationFactor;
  if (theTimeFrame == TimeFrame::INTRADAY)
    {
      auto minutes = baseSecurity->getTimeSeries()->getIntradayTimeFrameDurationInMinutes();
      annualizationFactor = calculateAnnualizationFactor(theTimeFrame,
							 minutes);
    }
  else
    {
      annualizationFactor = calculateAnnualizationFactor(theTimeFrame);
    }

  // Bootstrap portfolio series — GeoMean (decision) and arithmetic mean (comparison)
  unsigned int num_resamples   = 2000;
  double       confidence_level = 0.95;

  GeoMeanStat<Num> statGeo;
  BCaBootStrap<Num> metaGeo(metaReturns, num_resamples, confidence_level, statGeo);
  BCaAnnualizer<Num> metaGeoAnn(metaGeo, annualizationFactor);

  BCaBootStrap<Num> metaMean(metaReturns, num_resamples, confidence_level); // default = arithmetic mean
  BCaAnnualizer<Num> metaMeanAnn(metaMean, annualizationFactor);

  const Num lbGeoPeriod  = metaGeo.getLowerBound();
  const Num lbMeanPeriod = metaMean.getLowerBound();

  std::cout << "      Per-period BCa lower bounds (pre-annualization): "
	    << "Geo="  << (lbGeoPeriod  * DecimalConstants<Num>::DecimalOneHundred) << "%, "
	    << "Mean=" << (lbMeanPeriod * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

  // Portfolio-level cost hurdle (0.10% per side => 0.20% round trip).
  // With equal-weight capital, scale trade counts by weight.
  const Num costBufferMultiplier = Num("1.5");
  const Num riskFreeRate         = Num("0.03");
  const Num riskFreeMultiplier   = Num("2.0");
  const Num riskFreeHurdle       = riskFreeRate * riskFreeMultiplier;

  const Num slippagePerSide      = Num("0.001"); // 0.10%
  const Num slippagePerRoundTrip = slippagePerSide * DecimalConstants<Num>::DecimalTwo; // 0.20%

  Num sumTrades = DecimalConstants<Num>::DecimalZero;
  for (const auto& tr : survivorAnnualizedTrades) sumTrades += tr;
  // Equal-weight portfolio: cost scales with weight
  Num portfolioAnnualizedTrades = w * sumTrades;

  Num annualizedCostHurdle = portfolioAnnualizedTrades * slippagePerRoundTrip;
  Num costBasedRequiredReturn = annualizedCostHurdle * costBufferMultiplier;
  Num finalRequiredReturn = std::max(costBasedRequiredReturn, riskFreeHurdle);

  const Num lbGeoAnn  = metaGeoAnn.getAnnualizedLowerBound();
  const Num lbMeanAnn = metaMeanAnn.getAnnualizedLowerBound();

  std::cout << "\n[Meta] Portfolio of " << n << " survivors (equal-weight):\n"
            << "      Annualized Lower Bound (GeoMean): " << (lbGeoAnn  * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
            << "      Annualized Lower Bound (Mean):    " << (lbMeanAnn * DecimalConstants<Num>::DecimalOneHundred) << "%\n"
            << "      Required Return (max(cost,riskfree)): "
            << (finalRequiredReturn * DecimalConstants<Num>::DecimalOneHundred) << "%\n";

  if (lbGeoAnn > finalRequiredReturn) {
    std::cout << "      RESULT: ✓ Metastrategy PASSES\n";
  } else {
    std::cout << "      RESULT: ✗ Metastrategy FAILS\n";
  }

  std::cout << "      Costs assumed: $0 commission, 0.10% slippage/spread per side (≈0.20% round-trip).\n";
}

void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                        ValidationMethod method,
                                        ValidationInterface* validation,
                                        const DateRange& backtestingDates,
                                        TimeFrame::Duration theTimeFrame)
{
    std::string detailedPatternsFileName(createDetailedSurvivingPatternsFileName(baseSecurity->getSymbol(),
                                                                                 method));
    std::ofstream survivingPatternsFile(detailedPatternsFileName);
    
    auto survivingStrategies = validation->getSurvivingStrategies();
    for (const auto& strategy : survivingStrategies)
    {
        try
        {
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                       theTimeFrame,
                                                                       backtestingDates);
            // Note: monteCarloStats not used for surviving patterns in this implementation
            // auto& monteCarloStats = validation->getStatisticsCollector();
            survivingPatternsFile << "Surviving Pattern:" << std::endl << std::endl;
            LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
            survivingPatternsFile << std::endl;
            writeBacktestPerformanceReport(survivingPatternsFile, backtester);
            survivingPatternsFile << std::endl << std::endl;
            //BacktestingStatPolicy<Num>::printDetailedScoreBreakdown(backtester, survivingPatternsFile);
            //writeMonteCarloPermutationStats(monteCarloStats, survivingPatternsFile, clonedStrat);
        }
        catch (const std::exception& e)
        {
            std::cout << "Exception " << e.what() << std::endl;
            break;
        }
    }
}

// Overloaded version that takes a filtered strategies list directly with validation summary
void writeDetailedSurvivingPatternsFile(std::shared_ptr<Security<Num>> baseSecurity,
                                         ValidationMethod method,
                                         const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
                                         const DateRange& backtestingDates,
                                         TimeFrame::Duration theTimeFrame,
                                         const std::string& policyName,
                                         const ValidationParameters& params)
{
    std::string detailedPatternsFileName(createDetailedSurvivingPatternsFileName(baseSecurity->getSymbol(),
                                                                                 method));
    std::ofstream survivingPatternsFile(detailedPatternsFileName);
    
    // Write validation summary header
    survivingPatternsFile << "=== VALIDATION SUMMARY ===" << std::endl;
    survivingPatternsFile << "Security Ticker: " << baseSecurity->getSymbol() << std::endl;
    survivingPatternsFile << "Validation Method: " << getValidationMethodString(method) << std::endl;
    survivingPatternsFile << "Computation Policy: " << policyName << std::endl;
    survivingPatternsFile << "Out-of-Sample Range: " << backtestingDates.getFirstDateTime()
                          << " to " << backtestingDates.getLastDateTime() << std::endl;
    survivingPatternsFile << "Number of Permutations: " << params.permutations << std::endl;
    survivingPatternsFile << "P-Value Threshold: " << params.pValueThreshold << std::endl;
    if (method == ValidationMethod::BenjaminiHochberg) {
        survivingPatternsFile << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
    }
    survivingPatternsFile << "Total Surviving Strategies (Performance Filtered): " << strategies.size() << std::endl;
    survivingPatternsFile << "===========================" << std::endl << std::endl;
    
    for (const auto& strategy : strategies)
    {
        try
        {
            auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
            freshPortfolio->addSecurity(baseSecurity);
            auto clonedStrat = strategy->clone2(freshPortfolio);
            auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                       theTimeFrame,
                                                                       backtestingDates);
            // Note: monteCarloStats not used for surviving patterns in this implementation
            // auto& monteCarloStats = validation->getStatisticsCollector();
            survivingPatternsFile << "Surviving Pattern:" << std::endl << std::endl;
            LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
            survivingPatternsFile << std::endl;
            writeBacktestPerformanceReport(survivingPatternsFile, backtester);
            survivingPatternsFile << std::endl << std::endl;
            //BacktestingStatPolicy<Num>::printDetailedScoreBreakdown(backtester, survivingPatternsFile);
            //writeMonteCarloPermutationStats(monteCarloStats, survivingPatternsFile, clonedStrat);
        }
        catch (const std::exception& e)
        {
            std::cout << "Exception " << e.what() << std::endl;
            break;
        }
    }
}

void writeDetailedRejectedPatternsFile(const std::string& securitySymbol,
                                       ValidationMethod method,
                                       ValidationInterface* validation,
                                       const DateRange& backtestingDates,
                                       TimeFrame::Duration theTimeFrame,
                                       const Num& pValueThreshold,
                                       std::shared_ptr<Security<Num>> baseSecurity,
                                       const std::vector<std::shared_ptr<PalStrategy<Num>>>& performanceFilteredStrategies = {})
{
    std::string detailedPatternsFileName = createDetailedRejectedPatternsFileName(securitySymbol, method);
    std::ofstream rejectedPatternsFile(detailedPatternsFileName);
    
    // Get all strategies and identify rejected ones with their p-values
    auto allStrategies = validation->getAllTestedStrategies();
    std::set<std::shared_ptr<PalStrategy<Num>>> survivingSet;
    auto survivingStrategies = validation->getSurvivingStrategies();
    for (const auto& strategy : survivingStrategies)
    {
        survivingSet.insert(strategy);
    }
    
    std::vector<std::pair<std::shared_ptr<PalStrategy<Num>>, Num>> rejectedStrategiesWithPValues;
    for (const auto& [strategy, pValue] : allStrategies)
    {
        if (survivingSet.find(strategy) == survivingSet.end())
        {
            rejectedStrategiesWithPValues.emplace_back(strategy, pValue);
        }
    }
    
    // Write header
    rejectedPatternsFile << "=== REJECTED PATTERNS REPORT ===" << std::endl;
    rejectedPatternsFile << "Total Rejected Patterns: " << rejectedStrategiesWithPValues.size() << std::endl;
    rejectedPatternsFile << "P-Value Threshold: " << pValueThreshold << std::endl;
    rejectedPatternsFile << "Validation Method: " <<
        (method == ValidationMethod::Masters ? "Masters" : "Romano-Wolf") << std::endl;
    rejectedPatternsFile << "=================================" << std::endl << std::endl;
    
    if (rejectedStrategiesWithPValues.empty())
    {
        rejectedPatternsFile << "No rejected patterns found." << std::endl;
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "All " << validation->getNumSurvivingStrategies()
                            << " tested patterns survived the validation process." << std::endl;
        rejectedPatternsFile << "This indicates very strong patterns or a lenient p-value threshold." << std::endl;
        
        // Write basic summary statistics even when no rejected patterns are found
        struct RejectionReasonStats {
            int totalPatterns = 0;
            int survivingPatterns = 0;
            int rejectedPatterns = 0;
            double rejectionRate = 0.0;
        };
        
        RejectionReasonStats basicStats = {};
        basicStats.totalPatterns = static_cast<int>(allStrategies.size());
        basicStats.survivingPatterns = validation->getNumSurvivingStrategies();
        basicStats.rejectedPatterns = basicStats.totalPatterns - basicStats.survivingPatterns;
        basicStats.rejectionRate = basicStats.totalPatterns > 0 ?
            (double)basicStats.rejectedPatterns / basicStats.totalPatterns * 100.0 : 0.0;
        
        rejectedPatternsFile << std::endl;
        rejectedPatternsFile << "=== Summary Statistics ===" << std::endl;
        rejectedPatternsFile << "Total Patterns Tested: " << basicStats.totalPatterns << std::endl;
        rejectedPatternsFile << "Surviving Patterns: " << basicStats.survivingPatterns << std::endl;
        rejectedPatternsFile << "Rejected Patterns: " << basicStats.rejectedPatterns << std::endl;
        rejectedPatternsFile << "Rejection Rate: " << std::fixed << std::setprecision(2)
                            << basicStats.rejectionRate << "%" << std::endl;
        
        return;
    }
    
    // Sort rejected strategies by p-value (ascending)
    std::sort(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Write detailed information for each rejected strategy
    for (const auto& [strategy, pValue] : rejectedStrategiesWithPValues)
    {
        // Write rejected pattern details inline since function is not defined
        rejectedPatternsFile << "Rejected Pattern (p-value: " << pValue << "):" << std::endl;
        LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
        rejectedPatternsFile << "P-Value: " << pValue << std::endl;
        rejectedPatternsFile << "Threshold: " << pValueThreshold << std::endl;
        rejectedPatternsFile << "Reason: P-value exceeds threshold" << std::endl;
        rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
    }
    
    // Calculate and write summary statistics
    // Write summary statistics inline since functions are not defined
    rejectedPatternsFile << std::endl << "=== Summary Statistics ===" << std::endl;
    rejectedPatternsFile << "Total Rejected Patterns: " << rejectedStrategiesWithPValues.size() << std::endl;
    rejectedPatternsFile << "Validation Method: " << getValidationMethodString(method) << std::endl;
    rejectedPatternsFile << "P-Value Threshold: " << pValueThreshold << std::endl;
    
    if (!rejectedStrategiesWithPValues.empty()) {
        auto minPValue = std::min_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        auto maxPValue = std::max_element(rejectedStrategiesWithPValues.begin(), rejectedStrategiesWithPValues.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; })->second;
        rejectedPatternsFile << "Min P-Value: " << minPValue << std::endl;
        rejectedPatternsFile << "Max P-Value: " << maxPValue << std::endl;
    }
    
    // Add performance-filtered strategies section
    if (!performanceFilteredStrategies.empty()) {
        rejectedPatternsFile << std::endl << std::endl;
        rejectedPatternsFile << "=== PERFORMANCE-FILTERED PATTERNS ===" << std::endl;
        rejectedPatternsFile << "These patterns survived Monte Carlo validation but were filtered out due to insufficient backtesting performance." << std::endl;
        rejectedPatternsFile << "Total Performance-Filtered Patterns: " << performanceFilteredStrategies.size() << std::endl;
        rejectedPatternsFile << "Filtering Criteria: Profit Factor >= 1.75 AND PAL Profitability >= 85% of theoretical" << std::endl;
        rejectedPatternsFile << "=======================================" << std::endl << std::endl;
        
        for (const auto& strategy : performanceFilteredStrategies) {
            try {
                // Create fresh portfolio and clone strategy for backtesting
                auto freshPortfolio = std::make_shared<Portfolio<Num>>(strategy->getStrategyName() + " Portfolio");
                freshPortfolio->addSecurity(baseSecurity);
                auto clonedStrat = strategy->clone2(freshPortfolio);
                
                // Run backtest to get performance metrics for reporting
                auto backtester = BackTesterFactory<Num>::backTestStrategy(clonedStrat,
                                                                           theTimeFrame,
                                                                           backtestingDates);
                
                // Extract performance metrics
                auto positionHistory = backtester->getClosedPositionHistory();
                Num profitFactor = positionHistory.getProfitFactor();
                Num actualPALProfitability = positionHistory.getPALProfitability();
                
                // Calculate theoretical PAL profitability
                Num theoreticalPALProfitability = calculateTheoreticalPALProfitability(strategy);
                
                // Write pattern details
                rejectedPatternsFile << "Performance-Filtered Pattern:" << std::endl;
                LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
                rejectedPatternsFile << std::endl;
                
                // Write performance metrics that caused rejection
                rejectedPatternsFile << "=== Performance Metrics ===" << std::endl;
                rejectedPatternsFile << "Profit Factor: " << profitFactor << " (Required: >= 1.75)" << std::endl;
                rejectedPatternsFile << "PAL Profitability: " << actualPALProfitability << "%" << std::endl;
                rejectedPatternsFile << "Theoretical PAL Profitability: " << theoreticalPALProfitability << "%" << std::endl;
                
                if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero) {
                    Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                    rejectedPatternsFile << "PAL Ratio: " << (palRatio * DecimalConstants<Num>::DecimalOneHundred) << "% (Required: >= 85%)" << std::endl;
                }
                
                rejectedPatternsFile << "Reason: ";
                bool profitFactorFailed = profitFactor < DecimalConstants<Num>::DecimalOnePointSevenFive;
                bool palProfitabilityFailed = false;
                
                if (theoreticalPALProfitability > DecimalConstants<Num>::DecimalZero) {
                    Num palRatio = actualPALProfitability / theoreticalPALProfitability;
                    Num eightyFivePercent = DecimalConstants<Num>::createDecimal("0.85");
                    palProfitabilityFailed = palRatio < eightyFivePercent;
                }
                
                if (profitFactorFailed && palProfitabilityFailed) {
                    rejectedPatternsFile << "Both Profit Factor and PAL Profitability criteria failed";
                } else if (profitFactorFailed) {
                    rejectedPatternsFile << "Profit Factor below threshold";
                } else if (palProfitabilityFailed) {
                    rejectedPatternsFile << "PAL Profitability below 85% of theoretical";
                }
                
                rejectedPatternsFile << std::endl << std::endl << "---" << std::endl << std::endl;
                
            } catch (const std::exception& e) {
                rejectedPatternsFile << "Performance-Filtered Pattern (Error in analysis):" << std::endl;
                LogPalPattern::LogPattern(strategy->getPalPattern(), rejectedPatternsFile);
                rejectedPatternsFile << "Error: " << e.what() << std::endl;
                rejectedPatternsFile << std::endl << "---" << std::endl << std::endl;
            }
        }
    }
}



// ---- Core Logic ----

// This is the common worker function that runs the validation and prints results.
// It is called by the higher-level functions AFTER the validation object has been created.
void runValidationWorker(std::unique_ptr<ValidationInterface> validation,
                         std::shared_ptr<ValidatorConfiguration<Num>> config,
                         const ValidationParameters& params,
                         ValidationMethod validationMethod,
                         const std::string& policyName,
                         bool partitionByFamily = false)
{
    std::cout << "Starting Monte Carlo validation...\n" << std::endl;

    validation->runPermutationTests(config->getSecurity(),
        config->getPricePatterns(),
        config->getOosDateRange(),
        params.pValueThreshold,
        true, // Enable verbose logging by default
        partitionByFamily); // Pass the partitioning preference

    std::cout << "\nMonte Carlo validation completed." << std::endl;
    std::cout << "Number of surviving strategies = " << validation->getNumSurvivingStrategies() << std::endl;

    // -- Output --
    std::vector<std::shared_ptr<PalStrategy<Num>>> performanceFilteredStrategies;
    
    if (validation->getNumSurvivingStrategies() > 0)
    {
        auto survivingStrategies = validation->getSurvivingStrategies();
        
        // Apply performance-based filtering to surviving strategies
        std::cout << "\nApplying performance-based filtering to surviving strategies..." << std::endl;
        auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
        auto filteredStrategies = filterSurvivingStrategiesByPerformance<Num>(
            survivingStrategies,
            config->getSecurity(),
            config->getOosDateRange(),
            timeFrame
        );
        
        // Identify strategies that were filtered out due to performance criteria
        std::set<std::shared_ptr<PalStrategy<Num>>> filteredSet(filteredStrategies.begin(), filteredStrategies.end());
        for (const auto& strategy : survivingStrategies) {
            if (filteredSet.find(strategy) == filteredSet.end()) {
                performanceFilteredStrategies.push_back(strategy);
            }
        }

	if (!filteredStrategies.empty())
	  {
	    filterMetaStrategy<Num>(filteredStrategies,
				    config->getSecurity(),
				    config->getOosDateRange(),
				    timeFrame);
	  }
	
        std::cout << "Performance filtering results: " << filteredStrategies.size() << " passed, "
                  << performanceFilteredStrategies.size() << " filtered out" << std::endl;
        
        // Write the performance-filtered surviving patterns to the basic file
        if (!filteredStrategies.empty()) {
            std::string fn = createSurvivingPatternsFileName(config->getSecurity()->getSymbol(), validationMethod);
            std::ofstream survivingPatternsFile(fn);
            std::cout << "Writing surviving patterns to file: " << fn << std::endl;
            
            for (const auto& strategy : filteredStrategies)
            {
                LogPalPattern::LogPattern (strategy->getPalPattern(), survivingPatternsFile);
            }
        }

        // Write detailed report using filtered strategies
        if (!filteredStrategies.empty()) {
            std::cout << "Writing detailed surviving patterns report for " << filteredStrategies.size()
                      << " performance-filtered strategies..." << std::endl;
            writeDetailedSurvivingPatternsFile(config->getSecurity(), validationMethod, filteredStrategies,
                                               config->getOosDateRange(), timeFrame, policyName, params);
        } else {
            std::cout << "No strategies passed performance filtering criteria. Skipping detailed report." << std::endl;
        }
    }

    std::cout << "Writing detailed rejected patterns report..." << std::endl;
    auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
    writeDetailedRejectedPatternsFile(config->getSecurity()->getSymbol(), validationMethod, validation.get(),
                                      config->getOosDateRange(), timeFrame, params.pValueThreshold,
                                      config->getSecurity(), performanceFilteredStrategies);
    
    std::cout << "Validation run finished." << std::endl;
}

// ---- Validation Method Specific Orchestrators ----

// Orchestrator for Masters Validation
void runValidationForMasters(std::shared_ptr<ValidatorConfiguration<Num>> config,
                             const ValidationParameters& params,
                             const std::string& policyName,
                             bool partitionByFamily = false)
{
    std::cout << "\nUsing Masters validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }
    
    try {
        auto validation = statistics::PolicyFactory::createMastersValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Masters, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Masters validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Romano-Wolf Validation
void runValidationForRomanoWolf(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                const std::string& policyName,
                                bool partitionByFamily = false)
{
    std::cout << "\nUsing Romano-Wolf validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;

    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }

    try {
        auto validation = statistics::PolicyFactory::createRomanoWolfValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::RomanoWolf, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Romano-Wolf validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Benjamini-Hochberg Validation
void runValidationForBenjaminiHochberg(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                       const ValidationParameters& params,
                                       const std::string& policyName,
                                       bool partitionByFamily = false)
{
    std::cout << "\nUsing Benjamini-Hochberg validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    std::cout << "[INFO] False Discovery Rate (FDR) set to: " << params.falseDiscoveryRate << std::endl;

    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: None (all patterns tested together)" << std::endl;
    }

    try {
        auto validation = statistics::PolicyFactory::createBenjaminiHochbergValidation(
            policyName, params.permutations, params.falseDiscoveryRate.getAsDouble());
        runValidationWorker(std::move(validation), config, params, ValidationMethod::BenjaminiHochberg, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Benjamini-Hochberg validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}

// Orchestrator for Unadjusted Validation
void runValidationForUnadjusted(std::shared_ptr<ValidatorConfiguration<Num>> config,
                                const ValidationParameters& params,
                                const std::string& policyName,
                                bool partitionByFamily = false)
{
    std::cout << "\nUsing Unadjusted validation with " << policyName
              << " and " << params.permutations << " permutations." << std::endl;
    
    if (partitionByFamily) {
        std::cout << "Pattern partitioning: By detailed family (Category, SubType, Direction)" << std::endl;
    } else {
        std::cout << "Pattern partitioning: By direction only (Long vs Short)" << std::endl;
    }
    
    try {
        auto validation = statistics::PolicyFactory::createUnadjustedValidation(policyName, params.permutations);
        runValidationWorker(std::move(validation), config, params, ValidationMethod::Unadjusted, policyName, partitionByFamily);
    } catch (const std::exception& e) {
        std::cerr << "Error creating Unadjusted validation with policy '" << policyName << "': " << e.what() << std::endl;
        throw;
    }
}


// ---- Main Application Entry Point ----

void usage()
{
    printf("Usage: PalValidator <config file>\n");
    printf("  All other parameters will be requested via interactive prompts.\n");
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage();
        return 1;
    }
    
    // Initialize the policy registry with all available policies
    std::cout << "Initializing policy registry..." << std::endl;
    statistics::initializePolicyRegistry();
    
    // Load policy configuration (optional)
    palvalidator::PolicyConfiguration policyConfig;
    std::string configPath = "policies.json";
    if (!policyConfig.loadFromFile(configPath)) {
        std::cout << "No policy configuration file found, using defaults." << std::endl;
        policyConfig = palvalidator::PolicyConfiguration::createDefault();
    }
    
    // -- Configuration File Reading with existence check --
    std::string configurationFileName = std::string(argv[1]);
    std::shared_ptr<ValidatorConfiguration<Num>> config;
    
    // Check if configuration file exists before asking for other inputs
    if (!std::filesystem::exists(configurationFileName)) {
        std::cout << "Error: Configuration file '" << configurationFileName << "' does not exist." << std::endl;
        std::cout << "Please enter the correct configuration file path: ";
        std::getline(std::cin, configurationFileName);
    }
    
    // Try to read the configuration file
    ValidatorConfigurationFileReader reader(configurationFileName);
    try {
        config = reader.readConfigurationFile();
    }
    catch (const SecurityAttributesFactoryException& e) {
        std::cout << "SecurityAttributesFactoryException: Error reading configuration file: " << e.what() << std::endl;
        return 1;
    }
    catch (const ValidatorConfigurationException& e) {
        std::cout << "ValidatorConfigurationException thrown when reading configuration file: " << e.what() << std::endl;
        return 1;
    }
    
    // -- Get parameters interactively --
    ValidationParameters params;
    std::string input;

    std::cout << "\nEnter number of permutations (default: 5000): ";
    std::getline(std::cin, input);
    params.permutations = input.empty() ? 5000 : std::stoul(input);

    std::cout << "Enter p-value threshold (default: 0.05): ";
    std::getline(std::cin, input);
    params.pValueThreshold = input.empty() ? Num(0.05) : Num(std::stod(input));
    
    // Ask for Validation Method
    std::cout << "\nChoose validation method:" << std::endl;
    std::cout << "  1. Masters (default)" << std::endl;
    std::cout << "  2. Romano-Wolf" << std::endl;
    std::cout << "  3. Benjamini-Hochberg" << std::endl;
    std::cout << "  4. Unadjusted" << std::endl;
    std::cout << "Enter choice (1, 2, 3, or 4): ";
    std::getline(std::cin, input);
    
    ValidationMethod validationMethod = ValidationMethod::Masters;
    if (input == "2") {
        validationMethod = ValidationMethod::RomanoWolf;
    } else if (input == "3") {
        validationMethod = ValidationMethod::BenjaminiHochberg;
    } else if (input == "4") {
        validationMethod = ValidationMethod::Unadjusted;
    }
    
    // Conditionally ask for FDR
    params.falseDiscoveryRate = Num(0.10); // Set default
    if (validationMethod == ValidationMethod::BenjaminiHochberg) {
        std::cout << "Enter False Discovery Rate (FDR) for Benjamini-Hochberg (default: 0.10): ";
        std::getline(std::cin, input);
        if (!input.empty()) {
            params.falseDiscoveryRate = Num(std::stod(input));
        }
    }
    
    // Ask about pattern partitioning for Masters, Romano-Wolf, and Benjamini-Hochberg methods
    bool partitionByFamily = false;
    if (validationMethod == ValidationMethod::Masters ||
        validationMethod == ValidationMethod::RomanoWolf ||
        validationMethod == ValidationMethod::BenjaminiHochberg) {
        std::cout << "\nPattern Partitioning Options:" << std::endl;
        
        if (validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "  1. No Partitioning (all patterns tested together) - Default" << std::endl;
            std::cout << "  2. By Detailed Family (Category, SubType, Direction)" << std::endl;
        } else {
            std::cout << "  1. By Direction Only (Long vs Short) - Default" << std::endl;
            std::cout << "  2. By Detailed Family (Category, SubType, Direction)" << std::endl;
        }
        
        std::cout << "Choose partitioning method (1 or 2): ";
        std::getline(std::cin, input);
        
        if (input == "2") {
            partitionByFamily = true;
            std::cout << "Selected: Detailed family partitioning" << std::endl;
        } else {
            if (validationMethod == ValidationMethod::BenjaminiHochberg) {
                std::cout << "Selected: No partitioning (default)" << std::endl;
            } else {
                std::cout << "Selected: Direction-only partitioning (default)" << std::endl;
            }
        }
    }
    
    // Interactive policy selection using the new system
    std::cout << "\n=== Policy Selection ===" << std::endl;
    auto availablePolicies = palvalidator::PolicyRegistry::getAvailablePolicies();
    std::cout << "Available policies: " << availablePolicies.size() << std::endl;
    
    std::string selectedPolicy;
    if (policyConfig.getPolicySettings().interactiveMode) {
        selectedPolicy = statistics::PolicySelector::selectPolicy(availablePolicies, &policyConfig);
    } else {
        // Use default policy from configuration
        selectedPolicy = policyConfig.getDefaultPolicy();
        if (selectedPolicy.empty() || !palvalidator::PolicyRegistry::isPolicyAvailable(selectedPolicy)) {
            selectedPolicy = "GatedPerformanceScaledPalPolicy"; // Fallback default
        }
        std::cout << "Using configured default policy: " << selectedPolicy << std::endl;
    }

    // Display selected policy information
    try {
        auto metadata = palvalidator::PolicyRegistry::getPolicyMetadata(selectedPolicy);
        std::cout << "\nSelected Policy: " << metadata.displayName << std::endl;
        std::cout << "Description: " << metadata.description << std::endl;
        std::cout << "Category: " << metadata.category << std::endl;
        if (metadata.isExperimental) {
            std::cout << "⚠️  WARNING: This is an experimental policy!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not retrieve policy metadata: " << e.what() << std::endl;
    }
    
    // -- Summary --
    std::cout << "\n=== Configuration Summary ===" << std::endl;
    std::cout << "Security Ticker: " << config->getSecurity()->getSymbol() << std::endl;
    std::cout << "In-Sample Range: " << config->getInsampleDateRange().getFirstDateTime()
              << " to " << config->getInsampleDateRange().getLastDateTime() << std::endl;
    std::cout << "Validation Method: " << getValidationMethodString(validationMethod) << std::endl;
    std::cout << "Computation Policy: " << selectedPolicy << std::endl;
    if (validationMethod == ValidationMethod::Masters ||
        validationMethod == ValidationMethod::RomanoWolf ||
        validationMethod == ValidationMethod::BenjaminiHochberg) {
        if (validationMethod == ValidationMethod::BenjaminiHochberg) {
            std::cout << "Pattern Partitioning: " << (partitionByFamily ? "By Detailed Family" : "None") << std::endl;
        } else {
            std::cout << "Pattern Partitioning: " << (partitionByFamily ? "By Detailed Family" : "By Direction Only") << std::endl;
        }
    } else if (validationMethod == ValidationMethod::Unadjusted) {
        std::cout << "Pattern Partitioning: None (not applicable for Unadjusted)" << std::endl;
    }
    std::cout << "Permutations: " << params.permutations << std::endl;
    std::cout << "P-Value Threshold: " << params.pValueThreshold << std::endl;
    if (validationMethod == ValidationMethod::BenjaminiHochberg) {
        std::cout << "False Discovery Rate: " << params.falseDiscoveryRate << std::endl;
    }
    std::cout << "=============================" << std::endl;

    // -- Top-level dispatch based on the VALIDATION METHOD --
    try {
        switch (validationMethod)
        {
            case ValidationMethod::Masters:
                runValidationForMasters(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::RomanoWolf:
                runValidationForRomanoWolf(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::BenjaminiHochberg:
                runValidationForBenjaminiHochberg(config, params, selectedPolicy, partitionByFamily);
                break;
            case ValidationMethod::Unadjusted:
                runValidationForUnadjusted(config, params, selectedPolicy, partitionByFamily);
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Validation failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
