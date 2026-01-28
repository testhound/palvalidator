#include "BootstrapRobustnessAnalyzer.h"
#include "filtering/PerformanceFilter.h"
#include "diagnostics/NullBootstrapCollector.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace palvalidator::analysis {

  using namespace mkc_timeseries;
  using namespace palvalidator::filtering;

  // ============================================================================
  // Generate Test Seeds
  // ============================================================================

  std::vector<uint64_t> BootstrapRobustnessAnalyzer::generateTestSeeds()
  {
    std::vector<uint64_t> seeds;
    seeds.reserve(mConfig.getNumSeeds());
    
    // Use Mersenne Twister for deterministic seed generation
    std::mt19937_64 gen(mMasterSeed);
    
    for (unsigned int i = 0; i < mConfig.getNumSeeds(); i++) {
      seeds.push_back(gen());
    }
    
    return seeds;
  }

  // ============================================================================
  // Test With Single Seed (Black Box Usage of PerformanceFilter)
  // ============================================================================

  std::vector<std::shared_ptr<PalStrategy<Num>>> BootstrapRobustnessAnalyzer::testWithSeed(
											   uint64_t testSeed,
											   const std::vector<std::shared_ptr<PalStrategy<Num>>>& strategies,
											   std::shared_ptr<Security<Num>> baseSecurity,
											   const DateRange& inSampleBacktestingDates,
											   const DateRange& oosBacktestingDates,
											   TimeFrame::Duration timeFrame,
											   std::ostream& outputStream,
											   const Num& confidenceLevel,
											   unsigned int numResamples,
											   std::optional<OOSSpreadStats> oosSpreadStats)
  {
    // Create PerformanceFilter with this test seed
    // Using NullBootstrapCollector to suppress per-seed diagnostics
    auto nullCollector = std::make_shared<palvalidator::diagnostics::NullBootstrapCollector>();
    
    PerformanceFilter filter(confidenceLevel, numResamples, testSeed, nullCollector);
    
    // Call filterByPerformance - completely black box!
    // PerformanceFilter has no idea it's being called multiple times
    return filter.filterByPerformance(
				      strategies,
				      baseSecurity,
				      inSampleBacktestingDates,
				      oosBacktestingDates,
				      timeFrame,
				      outputStream,
				      oosSpreadStats
				      );
  }

  // ============================================================================
  // Run Bootstrap Robustness Analysis (Main Entry Point)
  // ============================================================================

  RobustnessAnalysisResult BootstrapRobustnessAnalyzer::analyze(
								const std::vector<std::shared_ptr<PalStrategy<Num>>>& survivingStrategies,
								std::shared_ptr<Security<Num>> baseSecurity,
								const DateRange& inSampleBacktestingDates,
								const DateRange& oosBacktestingDates,
								TimeFrame::Duration timeFrame,
								std::ostream& outputStream,
								const Num& confidenceLevel,
								unsigned int numResamples,
								std::optional<OOSSpreadStats> oosSpreadStats)
  {
    outputStream << "\n" << std::string(80, '=') << "\n";
    outputStream << "BOOTSTRAP ROBUSTNESS ANALYSIS\n";
    outputStream << std::string(80, '=') << "\n";
    outputStream << "Testing " << survivingStrategies.size() << " strategies\n";
    outputStream << "Number of bootstrap rounds: " << mConfig.getNumSeeds() << "\n";
    outputStream << "Pass rate threshold: " << (mConfig.getMinPassRate() * 100) << "%\n";
    outputStream << "Master seed: 0x" << std::hex << mMasterSeed << std::dec << "\n";
    outputStream << std::string(80, '=') << "\n\n";
    
    // Generate test seeds deterministically
    auto testSeeds = generateTestSeeds();
    
    // Track which strategies passed for each seed
    // Map: seed -> list of strategies that passed with that seed
    std::map<uint64_t, std::vector<std::shared_ptr<PalStrategy<Num>>>> resultsPerSeed;
    
    // Run filtering with each seed
    for (size_t i = 0; i < testSeeds.size(); i++) {
      uint64_t seed = testSeeds[i];
        
      outputStream << "\n" << std::string(70, '-') << "\n";
      outputStream << "Bootstrap round " << (i+1) << "/" << testSeeds.size() 
		   << " (seed: 0x" << std::hex << seed << std::dec << ")\n";
      outputStream << std::string(70, '-') << "\n";
        
      // Suppress detailed output for non-first seeds unless configured otherwise
      std::ostringstream nullStream;
      std::ostream& testStream = (mConfig.getReportDetailedResults() || i == 0) 
	? outputStream 
	: nullStream;
        
      // Call PerformanceFilter with this seed (BLACK BOX usage)
      auto survivors = testWithSeed(
				    seed, survivingStrategies, baseSecurity,
				    inSampleBacktestingDates, oosBacktestingDates,
				    timeFrame, testStream, confidenceLevel, numResamples, oosSpreadStats
				    );
        
      resultsPerSeed[seed] = survivors;
        
      outputStream << "Round " << (i+1) << " result: " 
		   << survivors.size() << "/" << survivingStrategies.size() 
		   << " strategies passed\n";
    }
    
    // Aggregate results
    auto aggregated = aggregateResults(survivingStrategies, testSeeds, resultsPerSeed);
    
    // Report
    reportResults(aggregated, outputStream);
    
    return aggregated;
  }

  // ============================================================================
  // Aggregate Results Across Seeds
  // ============================================================================

  RobustnessAnalysisResult BootstrapRobustnessAnalyzer::aggregateResults(
									 const std::vector<std::shared_ptr<PalStrategy<Num>>>& allStrategies,
									 const std::vector<uint64_t>& testSeeds,
									 const std::map<uint64_t, std::vector<std::shared_ptr<PalStrategy<Num>>>>& resultsPerSeed)
  {
    std::vector<StrategyBootstrapResult> strategyResults;
    std::vector<std::shared_ptr<PalStrategy<Num>>> acceptedStrategies;
    
    // For each strategy, count how many seeds it passed
    for (const auto& strategy : allStrategies) {
      std::vector<bool> passedForEachSeed;
      int passCount = 0;
        
      // Count passes for this strategy
      for (const auto& seed : testSeeds) {
	const auto& survivorsForSeed = resultsPerSeed.at(seed);
            
	// Check if this strategy is in the survivors list
	bool passed = std::find_if(
				   survivorsForSeed.begin(),
				   survivorsForSeed.end(),
				   [&strategy](const auto& s) { 
				     return s->getStrategyName() == strategy->getStrategyName(); 
				   }
				   ) != survivorsForSeed.end();
            
	passedForEachSeed.push_back(passed);
	if (passed) {
	  passCount++;
	}
      }
        
      // Create result for this strategy
      StrategyBootstrapResult strategyResult(
					     strategy,
					     testSeeds,
					     passedForEachSeed,
					     passCount
					     );
        
      // Determine acceptance
      double threshold = mConfig.getRequirePerfect() ? 1.0 : mConfig.getMinPassRate();
      bool accepted = (strategyResult.getPassRate() >= threshold);
      strategyResult.setAccepted(accepted);
        
      if (accepted) {
	acceptedStrategies.push_back(strategy);
      }
        
      strategyResults.push_back(strategyResult);
    }
    
    return RobustnessAnalysisResult(strategyResults, acceptedStrategies);
  }

  // ============================================================================
  // Report Results
  // ============================================================================

  void BootstrapRobustnessAnalyzer::reportResults(
						  const RobustnessAnalysisResult& results,
						  std::ostream& os) const
  {
    os << "\n" << std::string(80, '=') << "\n";
    os << "BOOTSTRAP ROBUSTNESS RESULTS\n";
    os << std::string(80, '=') << "\n\n";
    
    os << "Total strategies tested: " << results.getTotalStrategies() << "\n\n";
    
    // Pass rate distribution
    os << "Pass Rate Distribution:\n";
    os << "  Perfect (100%):      " << std::setw(4) << results.getPerfectPassRateCount() << " strategies\n";
    os << "  High (95-99%):       " << std::setw(4) << results.getHighPassRateCount() << " strategies\n";
    os << "  Moderate (80-94%):   " << std::setw(4) << results.getModeratePassRateCount() << " strategies\n";
    os << "  Low (50-79%):        " << std::setw(4) << results.getLowPassRateCount() << " strategies\n";
    os << "  Very Low (<50%):     " << std::setw(4) << results.getVeryLowPassRateCount() << " strategies\n\n";
    
    // Final decision
    os << "Final Decision (threshold: " << (mConfig.getMinPassRate() * 100) << "%):\n";
    os << "  ✓ Accepted:          " << std::setw(4) << results.getAcceptedCount() << " strategies\n";
    os << "  ✗ Rejected:          " << std::setw(4) << results.getRejectedCount() << " strategies\n\n";

    // Count survivors by direction
    size_t survivorsLong = 0, survivorsShort = 0;
    for (const auto& strategy : results.getAcceptedStrategies()) {
      const auto& name = strategy->getStrategyName();
      if (name.find("Long") != std::string::npos) {
        ++survivorsLong;
      }
      if (name.find("Short") != std::string::npos) {
        ++survivorsShort;
      }
    }
    os << "  Survivors by direction → Long: " << survivorsLong
       << ", Short: " << survivorsShort << "\n\n";

    // Individual strategy results
    os << "Individual Strategy Results:\n";
    os << std::string(80, '-') << "\n";
    
    // Sort by pass rate (descending)
    auto sortedResults = results.getStrategyResults();
    std::sort(sortedResults.begin(), sortedResults.end(),
              [](const auto& a, const auto& b) { return a.getPassRate() > b.getPassRate(); });
    
    for (const auto& sr : sortedResults) {
      os << sr.getStrategy()->getStrategyName() << ": "
	 << std::fixed << std::setprecision(1) << (sr.getPassRate() * 100) << "% "
	 << "(" << sr.getPassCount() << "/" << sr.getTotalTested() << ")";
        
      if (sr.isAccepted()) {
	os << " ✓ ACCEPTED";
      } else {
	os << " ✗ REJECTED";
      }
        
      // Show seed-by-seed if configured
      if (mConfig.getReportDetailedResults()) {
	os << " [";
	for (size_t i = 0; i < sr.getPassedForEachSeed().size(); i++) {
	  os << (sr.getPassedForEachSeed()[i] ? "✓" : "✗");
	}
	os << "]";
      }
        
      os << "\n";
    }
    
    os << std::string(80, '=') << "\n";
  }


} // namespace palvalidator::analysis
