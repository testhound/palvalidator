#include <iostream>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <boost/date_time/gregorian/gregorian.hpp>

// Project includes
#include "PalSetupTypes.h"
#include "UserInterface.h"
#include "DirectoryManager.h"
#include "TimeSeriesProcessor.h"
#include "QuantizationAnalyzer.h"
#include "StatisticsCalculator.h"
#include "FileOperations.h"
#include "BidAskAnalyzer.h"

// Library includes
#include "SecurityAttributesFactory.h"
#include "DecimalConstants.h"
#include "TimeSeriesCsvWriter.h"

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

int main(int argc, char** argv)
{
  try {
    // 1. Initialize components
    UserInterface ui;
    DirectoryManager dirManager;
    TimeSeriesProcessor tsProcessor;
    QuantizationAnalyzer quantAnalyzer;
    StatisticsCalculator statsCalculator;
    FileOperations fileOps;
        
    // 2. Parse command line and collect user input to build configuration
    auto config = ui.parseCommandLineArgs(argc, argv);
        
    // 3. Create directory structure (only if not in stats-only mode)
    DirectoryPaths paths(fs::path(), fs::path(), fs::path(), fs::path(), fs::path(), {}, {});
    if (!config.isStatsOnlyMode()) {
      paths = dirManager.createDirectoryStructure(config);
    }
        
    // 4. Load and analyze time series
    auto reader = tsProcessor.createTimeSeriesReader(
						     config.getFileType(), 
						     config.getHistoricDataFileName(), 
						     config.getSecurityTick(), 
						     config.getTimeFrame());
    auto timeSeries = tsProcessor.loadTimeSeries(reader);
        
    // Display setup summary with date ranges
    ui.displaySetupSummary(config, *timeSeries);
        
    // 5. Determine known tick (from CLI or SecurityAttributes)
    std::optional<double> knownTick;
    if (config.getSecurityTick().getAsDouble() > 0)
      {
	knownTick = config.getSecurityTick().getAsDouble();
      }
    else
      {
	// Try SecurityAttributesFactory if not provided via CLI
	try {
	  auto attrs = getSecurityAttributes<Num>(config.getTickerSymbol());
	  knownTick = attrs->getTick().getAsDouble();
	} catch (const SecurityAttributesFactoryException&) {
	  // attributes not found; will infer from data
	}
      }
        
    // 6. Find clean start index using quantization analysis
    CleanStartConfig trimCfg;
    if (config.getTimeFrame() == TimeFrame::INTRADAY && config.getIntradayMinutes() >= 1) {
      trimCfg.adjustForTimeFrame(config.getTimeFrame(), timeSeries->getNumEntries(), config.getIntradayMinutes());
    }
        
    auto cleanStart = quantAnalyzer.findCleanStartIndex(*timeSeries, trimCfg, knownTick);
        
    if (!cleanStart.isFound())
      {
	std::ostringstream oss;
	oss << "No clean start window found for symbol '" << config.getTickerSymbol() << "'. "
	    << "Bars=" << timeSeries->getNumEntries()
	    << ", windowBarsTried=" << trimCfg.getWindowBars()
	    << ", thresholds={maxRelTick=" << trimCfg.getMaxRelTick()
	    << ", maxZeroFrac=" << trimCfg.getMaxZeroFrac()
	    << ", minUniqueLevels=" << trimCfg.getMinUniqueLevels() << "}.";
	throw std::runtime_error(oss.str());
      }
        
    // Display clean start information
    if (cleanStart.isFound() && cleanStart.getStartIndex() > 0)
      {
	auto chosenDate = timeSeries->getEntriesCopy()[cleanStart.getStartIndex()].getDateTime().date();
	std::cout << "[Quantization-aware trim] Start index " << cleanStart.getStartIndex()
		  << " (" << boost::gregorian::to_iso_extended_string(chosenDate) << ")"
		  << "  tick≈" << cleanStart.getTick()
		  << "  relTick≈" << cleanStart.getRelTick()
		  << "  zeroFrac≈" << cleanStart.getZeroFrac() << std::endl;

	if (knownTick)
	  std::cout << "[Tick] from SecurityAttributes/CLI: " << *knownTick << std::endl;
	else
	  std::cout << "[Tick] inferred from data: " << cleanStart.getTick() << std::endl;
      }
        
    // 7. Split time series into in-sample, out-of-sample, and reserved
    auto splitData = tsProcessor.splitTimeSeries(*timeSeries, cleanStart, config);
        
    // Branch based on mode
    if (config.isStatsOnlyMode())
      {
	// Statistics-only mode: display stats and exit
	ui.displayStatisticsOnly(splitData.getInSample(),
				 splitData.getOutOfSample(),
				 config);
	std::cout << "\nStatistics analysis complete." << std::endl;
      }
    else
      {
	// Existing file-writing mode
	// 8. Calculate separate long and short stop and target statistics
	auto combinedStats = statsCalculator.calculateSeparateStopAndTarget(
									    splitData.getInSample(), config.getHoldingPeriod());
            
	// 9. Analyze bid/ask spreads on out-of-sample data
	BidAskAnalyzer bidAskAnalyzer;
	auto spreadAnalysis = bidAskAnalyzer.analyzeSpreads(
							    splitData.getOutOfSample(),
							    config.getSecurityTick());
            
	// 10. Write all output files
	fileOps.writeSeparateTargetStopFiles(paths.getPalSubDirs(), config.getTickerSymbol(), combinedStats);
	fileOps.writeDataFiles(paths.getPalSubDirs(), splitData, config);
	fileOps.writeValidationFiles(paths, splitData, config, *timeSeries);
	fileOps.writeSeparateDetailsFile(paths.getValDir(), config, combinedStats, cleanStart, splitData, spreadAnalysis);
            
	// 11. Display final results
	std::cout << "In-sample% = " << config.getInsamplePercent() << "%\n";
	std::cout << "Out-of-sample% = " << config.getOutOfSamplePercent() << "%\n";
	std::cout << "Reserved% = " << config.getReservedPercent() << "%\n";
	ui.displaySeparateResults(combinedStats, cleanStart, spreadAnalysis);
      }
        
  }
  catch (const std::exception& e)
    {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
    }
    
  return 0;
}
