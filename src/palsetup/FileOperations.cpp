#include "FileOperations.h"
#include "TimeSeriesCsvWriter.h"
#include "DecimalConstants.h"
#include "BidAskSpread.h"
#include "StatUtils.h"
#include "TimeSeriesIndicators.h"
#include <fstream>
#include <iostream>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace mkc_timeseries;

FileOperations::FileOperations() = default;

void FileOperations::writeConfigFile(const std::string& outputDir,
                                    const std::string& tickerSymbol,
                                    const OHLCTimeSeries<Num>& insampleSeries,
                                    const OHLCTimeSeries<Num>& outOfSampleSeries,
                                    const std::string& timeFrame) {
    
    fs::path configFileName = fs::path(outputDir) / (tickerSymbol + "_config.csv");
    std::ofstream configFile(configFileName);
    if (!configFile.is_open()) {
        std::cerr << "Error: Unable to open config file " << configFileName << std::endl;
        return;
    }

    std::string irPath = "./" + tickerSymbol + "_IR.txt";
    std::string dataPath = "./" + tickerSymbol + "_ALL.txt";
    std::string fileFormat = (timeFrame == "Intraday") ? "INTRADAY::TRADESTATION" : "PAL";

    // Format dates based on timeframe
    bool isIntraday = (timeFrame == "Intraday");
    std::string isDateStart = formatDateForConfig(insampleSeries.getFirstDateTime(), isIntraday);
    std::string isDateEnd = formatDateForConfig(insampleSeries.getLastDateTime(), isIntraday);
    std::string oosDateStart = formatDateForConfig(outOfSampleSeries.getFirstDateTime(), isIntraday);
    std::string oosDateEnd = formatDateForConfig(outOfSampleSeries.getLastDateTime(), isIntraday);

    // Write CSV line: Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame
    configFile << tickerSymbol << ","
               << irPath << ","
               << dataPath << ","
               << fileFormat << ","
               << isDateStart << ","
               << isDateEnd << ","
               << oosDateStart << ","
               << oosDateEnd << ","
               << timeFrame << std::endl;

    configFile.close();
}

void FileOperations::writeTargetStopFiles(const std::vector<fs::path>& palSubDirs,
                                         const std::string& tickerSymbol,
                                         const StatisticsResults& stats) {
    
    Num half(DecimalConstants<Num>::createDecimal("0.5"));
    
    for (const auto& currentPalDir : palSubDirs) {
        // Generate target/stop files in current subdirectory using asymmetric values
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_0_5_.TRS"), 
                           stats.getProfitTargetValue() * half, 
                           stats.getStopValue());
        
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_1_0_.TRS"), 
                           stats.getProfitTargetValue(), 
                           stats.getStopValue());
        
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_2_0_.TRS"), 
                           stats.getProfitTargetValue() * DecimalConstants<Num>::DecimalTwo, 
                           stats.getStopValue());
    }
}

void FileOperations::writeDataFiles(const std::vector<fs::path>& palSubDirs,
                                   const SplitTimeSeriesData& splitData,
                                   const SetupConfiguration& config) {
    
    for (const auto& currentPalDir : palSubDirs) {
        fs::path filePath = currentPalDir / (config.getTickerSymbol() + "_IS.txt");
        
        if (config.isIndicatorMode()) {
            writePalDataFile(filePath, splitData.getInSample(), config, &splitData.getInSampleIndicator());
        } else {
            writePalDataFile(filePath, splitData.getInSample(), config);
        }
    }
}

void FileOperations::writeValidationFiles(const DirectoryPaths& paths,
                                         const SplitTimeSeriesData& splitData,
                                         const SetupConfiguration& config,
                                         const mkc_timeseries::OHLCTimeSeries<Num>& completeTimeSeries) {
    
    // Write ALL.txt files to each risk-reward subdirectory
    for (const auto& rrDir : paths.getRiskRewardDirs()) {
        // Write complete time series data
        writeValidationDataFile(rrDir / (config.getTickerSymbol() + "_ALL.txt"),
                               completeTimeSeries, config);
        
        // Write config file to each risk-reward subdirectory
        writeConfigFile(rrDir.string(), config.getTickerSymbol(),
                       splitData.getInSample(), splitData.getOutOfSample(),
                       config.getTimeFrameStr());
    }

    // Write OOS and reserved files to main validation directory
    writeValidationDataFile(paths.getValDir() / (config.getTickerSymbol() + "_OOS.txt"),
                           splitData.getOutOfSample(), config);
    
    if (splitData.getReserved().getNumEntries() > 0) {
        writeValidationDataFile(paths.getValDir() / (config.getTickerSymbol() + "_reserved.txt"),
                               splitData.getReserved(), config);
    }
}

void FileOperations::writeDetailsFile(const fs::path& outputPath,
                                     const SetupConfiguration& config,
                                     const StatisticsResults& stats,
                                     const CleanStartResult& cleanStart) {
    
    fs::path detailsFilePath = outputPath / (config.getTickerSymbol() + "_Palsetup_Details.txt");
    std::ofstream detailsFile(detailsFilePath);
    if (!detailsFile.is_open()) {
        std::cerr << "Error: Unable to open details file " << detailsFilePath << std::endl;
        return;
    }

    detailsFile << "In-sample% = " << config.getInsamplePercent() << "%\n";
    detailsFile << "Out-of-sample% = " << config.getOutOfSamplePercent() << "%\n";
    detailsFile << "Reserved% = " << config.getReservedPercent() << "%\n";
    detailsFile << "Median = " << stats.getMedianOfRoc() << std::endl;
    detailsFile << "Qn  = " << stats.getRobustQn() << std::endl;
    detailsFile << "MAD = " << stats.getMAD() << std::endl;
    detailsFile << "Std = " << stats.getStdDev() << std::endl;
    detailsFile << "Profit Target = " << stats.getProfitTargetValue() << std::endl;
    detailsFile << "Stop = " << stats.getStopValue() << std::endl;
    detailsFile << "Skew = " << stats.getSkew() << std::endl;
    detailsFile << "CleanStartIndex = " << cleanStart.getStartIndex() << "\n";
    
    if (cleanStart.isFound()) {
        detailsFile << "InferredTick   = " << cleanStart.getTick() << "\n";
        detailsFile << "RelTick        = " << cleanStart.getRelTick() << "\n";
        detailsFile << "ZeroFrac       = " << cleanStart.getZeroFrac() << "\n";
        detailsFile << "TickSource     = " << (config.getSecurityTick().getAsDouble() > 0 ? "SecurityAttributes_or_CLI" : "Inferred") << "\n";
    }

    detailsFile.close();
}

void FileOperations::writeTargetStopFile(const fs::path& filePath, const Num& target, const Num& stop)
{
  // Open the file in binary mode
  std::ofstream tsFile(filePath.string(), std::ios::binary);
  if (!tsFile.is_open()) {
    std::cerr << "Error: Unable to open target/stop file " << filePath << std::endl;
    return;
  }
    
  // Explicitly write Windows-style line endings
  tsFile << target << "\r\n" << stop << "\r\n";
  tsFile.close();
}

void FileOperations::writePalDataFile(const fs::path& filePath, 
                                     const OHLCTimeSeries<Num>& series,
                                     const SetupConfiguration& config,
                                     const NumericTimeSeries<Num>* indicator) {
    
    if (config.isIndicatorMode() && indicator) {
        // Write indicator-based PAL files with Windows line endings
        if (config.getTimeFrameStr() == "Intraday") {
            PalIndicatorIntradayCsvWriter<Num> writer(filePath.string(), series, *indicator, true);
            writer.writeFile();
        } else {
            PalIndicatorEodCsvWriter<Num> writer(filePath.string(), series, *indicator, true);
            writer.writeFile();
        }
    } else {
        // Write standard OHLC PAL files with Windows line endings
        if (config.getTimeFrameStr() == "Intraday") {
            PalIntradayCsvWriter<Num> writer(filePath.string(), series, true);
            writer.writeFile();
        } else {
            PalTimeSeriesCsvWriter<Num> writer(filePath.string(), series, true);
            writer.writeFile();
        }
    }
}

void FileOperations::writeValidationDataFile(const fs::path& filePath,
                                            const OHLCTimeSeries<Num>& series,
                                            const SetupConfiguration& config) {
    
    if (config.getTimeFrameStr() == "Intraday") {
        TradeStationIntradayCsvWriter<Num> writer(filePath.string(), series, false);
        writer.writeFile();
    } else {
        PalTimeSeriesCsvWriter<Num> writer(filePath.string(), series, false);
        writer.writeFile();
    }
}

std::string FileOperations::formatDateForConfig(const boost::posix_time::ptime& dateTime, bool isIntraday) {
    if (isIntraday) {
        // For intraday data, use full DateTime to avoid overlapping date ranges
        // Format as YYYYMMDDTHHMMSS for ptime
        return boost::posix_time::to_iso_string(dateTime);
    } else {
        // For EOD data, use Date methods
        return boost::gregorian::to_iso_string(dateTime.date());
    }
}

void FileOperations::validateFileWrite(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        throw std::runtime_error("Failed to write file: " + filePath.string());
    }
}

void FileOperations::writeSeparateTargetStopFiles(const std::vector<fs::path>& palSubDirs,
                                                  const std::string& tickerSymbol,
                                                  const CombinedStatisticsResults& stats) {
    
    Num half(DecimalConstants<Num>::createDecimal("0.5"));
    
    for (const auto& currentPalDir : palSubDirs) {
        // Generate long target/stop files in current subdirectory
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_0_5_LONG.TRS"),
                           stats.getLongResults().getProfitTargetValue() * half,
                           stats.getLongResults().getStopValue());
        
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_1_0_LONG.TRS"),
                           stats.getLongResults().getProfitTargetValue(),
                           stats.getLongResults().getStopValue());
        
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_2_0_LONG.TRS"),
                           stats.getLongResults().getProfitTargetValue() * DecimalConstants<Num>::DecimalTwo,
                           stats.getLongResults().getStopValue());
        
        // Generate short target/stop files in current subdirectory
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_0_5_SHORT.TRS"),
                           stats.getShortResults().getProfitTargetValue() * half,
                           stats.getShortResults().getStopValue());
        
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_1_0_SHORT.TRS"),
                           stats.getShortResults().getProfitTargetValue(),
                           stats.getShortResults().getStopValue());
        
        writeTargetStopFile(currentPalDir / (tickerSymbol + "_2_0_SHORT.TRS"),
                           stats.getShortResults().getProfitTargetValue() * DecimalConstants<Num>::DecimalTwo,
                           stats.getShortResults().getStopValue());
    }
}

void FileOperations::writeSeparateDetailsFile(const fs::path& outputPath,
                                             const SetupConfiguration& config,
                                             const CombinedStatisticsResults& stats,
                                             const CleanStartResult& cleanStart,
                                             const SplitTimeSeriesData& splitData) {
    
    fs::path detailsFilePath = outputPath / (config.getTickerSymbol() + "_Palsetup_Details.txt");
    std::ofstream detailsFile(detailsFilePath);
    if (!detailsFile.is_open()) {
        std::cerr << "Error: Unable to open details file " << detailsFilePath << std::endl;
        return;
    }

    detailsFile << "In-sample% = " << config.getInsamplePercent() << "%\n";
    detailsFile << "Out-of-sample% = " << config.getOutOfSamplePercent() << "%\n";
    detailsFile << "Reserved% = " << config.getReservedPercent() << "%\n";
    
    // Date ranges
    detailsFile << "\n=== Date Ranges ===" << std::endl;
    bool isIntraday = (config.getTimeFrameStr() == "Intraday");
    detailsFile << "In-sample: " << formatDateForConfig(splitData.getInSample().getFirstDateTime(), isIntraday)
                << " to " << formatDateForConfig(splitData.getInSample().getLastDateTime(), isIntraday) << std::endl;
    detailsFile << "Out-of-sample: " << formatDateForConfig(splitData.getOutOfSample().getFirstDateTime(), isIntraday)
                << " to " << formatDateForConfig(splitData.getOutOfSample().getLastDateTime(), isIntraday) << std::endl;
    if (splitData.getReserved().getNumEntries() > 0) {
        detailsFile << "Reserved: " << formatDateForConfig(splitData.getReserved().getFirstDateTime(), isIntraday)
                    << " to " << formatDateForConfig(splitData.getReserved().getLastDateTime(), isIntraday) << std::endl;
    }
    
    // Long position statistics
    detailsFile << "\n=== Long Position Statistics ===" << std::endl;
    
    // Calculate and display long profitability
    // Profitability = 100 × PF / (PF + R), where PF = 2.0 and R = ProfitTarget/Stop
    Num longPF = DecimalConstants<Num>::DecimalTwo;
    Num longR = stats.getLongResults().getProfitTargetValue() / stats.getLongResults().getStopValue();
    Num longProfitability = DecimalConstants<Num>::DecimalOneHundred * longPF / (longPF + longR);
    detailsFile << "Long Profitability = " << longProfitability << "%" << std::endl;
    
    detailsFile << "Long Profit Target = " << stats.getLongResults().getProfitTargetValue() << std::endl;
    detailsFile << "Long Stop = " << stats.getLongResults().getStopValue() << std::endl;
    detailsFile << "Long Pos Median = " << stats.getLongResults().getPosMedian() << std::endl;
    detailsFile << "Long Pos Qn = " << stats.getLongResults().getPosQn() << std::endl;
    detailsFile << "Long Pos Skew = " << stats.getLongResults().getPosSkew() << std::endl;
    detailsFile << "Long Neg Median = " << stats.getLongResults().getNegMedian() << std::endl;
    detailsFile << "Long Neg Skew = " << stats.getLongResults().getNegSkew() << std::endl;
    detailsFile << "Long Pos Count = " << stats.getLongResults().getPosCount() << std::endl;
    detailsFile << "Long Neg Count = " << stats.getLongResults().getNegCount() << std::endl;
    
    // Short position statistics
    detailsFile << "\n=== Short Position Statistics ===" << std::endl;
    
    // Calculate and display short profitability
    // Profitability = 100 × PF / (PF + R), where PF = 2.0 and R = ProfitTarget/Stop
    Num shortPF = DecimalConstants<Num>::DecimalTwo;
    Num shortR = stats.getShortResults().getProfitTargetValue() / stats.getShortResults().getStopValue();
    Num shortProfitability = DecimalConstants<Num>::DecimalOneHundred * shortPF / (shortPF + shortR);
    detailsFile << "Short Profitability = " << shortProfitability << "%" << std::endl;
    
    detailsFile << "Short Profit Target = " << stats.getShortResults().getProfitTargetValue() << std::endl;
    detailsFile << "Short Stop = " << stats.getShortResults().getStopValue() << std::endl;
    detailsFile << "Short Neg Median = " << stats.getShortResults().getNegMedian() << std::endl;
    detailsFile << "Short Neg Qn = " << stats.getShortResults().getNegQn() << std::endl;
    detailsFile << "Short Neg Skew = " << stats.getShortResults().getNegSkew() << std::endl;
    detailsFile << "Short Pos Median = " << stats.getShortResults().getPosMedian() << std::endl;
    detailsFile << "Short Pos Skew = " << stats.getShortResults().getPosSkew() << std::endl;
    detailsFile << "Short Neg Count = " << stats.getShortResults().getNegCount() << std::endl;
    detailsFile << "Short Pos Count = " << stats.getShortResults().getPosCount() << std::endl;
    
    // Clean start information
    detailsFile << "\n=== Clean Start Information ===" << std::endl;
    detailsFile << "CleanStartIndex = " << cleanStart.getStartIndex() << "\n";
    
    if (cleanStart.isFound()) {
        detailsFile << "InferredTick   = " << cleanStart.getTick() << "\n";
        detailsFile << "RelTick        = " << cleanStart.getRelTick() << "\n";
        detailsFile << "ZeroFrac       = " << cleanStart.getZeroFrac() << "\n";
        detailsFile << "TickSource     = " << (config.getSecurityTick().getAsDouble() > 0 ? "SecurityAttributes_or_CLI" : "Inferred") << "\n";
    }
    
    // Bid/Ask Spread Analysis
    detailsFile << "\n=== Bid/Ask Spread Analysis (Out-of-Sample) ===" << std::endl;
    
    try {
        const auto& oosSeries = splitData.getOutOfSample();
        
        detailsFile << "Out-of-sample entries: " << oosSeries.getNumEntries() << std::endl;
        
        // Check if we have sufficient data for spread calculation
        if (oosSeries.getNumEntries() < 2) {
            detailsFile << "Warning: Insufficient data for bid/ask spread calculation (need at least 2 entries)" << std::endl;
        } else {
            // Calculate spreads using Corwin-Schultz method
            using CorwinSchultzCalc = mkc_timeseries::CorwinSchultzSpreadCalculator<Num>;
            auto corwinSchultzSpreads = CorwinSchultzCalc::calculateProportionalSpreadsVector(oosSeries,
                 config.getSecurityTick(),
                 CorwinSchultzCalc::NegativePolicy::Epsilon);
            
            if (!corwinSchultzSpreads.empty()) {
                auto csMean = mkc_timeseries::StatUtils<Num>::computeMean(corwinSchultzSpreads);
                auto csMedian = mkc_timeseries::MedianOfVec(corwinSchultzSpreads);
                mkc_timeseries::RobustQn<Num> qnCalc;
                const Num csQn = qnCalc.getRobustQn(corwinSchultzSpreads);
                
                // Convert to percentage terms
                auto csMeanPercent = csMean * DecimalConstants<Num>::DecimalOneHundred;
                auto csMedianPercent = csMedian * DecimalConstants<Num>::DecimalOneHundred;
                auto csQnPercent = csQn * DecimalConstants<Num>::DecimalOneHundred;
                
                detailsFile << "\nCorwin-Schultz Spread Estimator:" << std::endl;
                detailsFile << "  Calculated " << corwinSchultzSpreads.size() << " spread measurements" << std::endl;
                detailsFile << "  Mean: " << csMeanPercent << "%" << std::endl;
                detailsFile << "  Median: " << csMedianPercent << "%" << std::endl;
                detailsFile << "  Robust Qn: " << csQnPercent << "%" << std::endl;
            } else {
                detailsFile << "\nCorwin-Schultz: No valid spread calculations could be performed" << std::endl;
            }
            
            // Calculate spreads using Edge method
            using EdgeCalc = mkc_timeseries::EdgeSpreadCalculator<Num>;
            auto edgeSpreads = EdgeCalc::calculateProportionalSpreadsVector(oosSeries,
              30,
              config.getSecurityTick(),
              EdgeCalc::NegativePolicy::Epsilon);
            
            if (!edgeSpreads.empty()) {
                auto edgeMean = mkc_timeseries::StatUtils<Num>::computeMean(edgeSpreads);
                auto edgeMedian = mkc_timeseries::MedianOfVec(edgeSpreads);
                mkc_timeseries::RobustQn<Num> qnCalc;
                const Num edgeQn = qnCalc.getRobustQn(edgeSpreads);
                
                // Convert to percentage terms
                auto edgeMeanPercent = edgeMean * DecimalConstants<Num>::DecimalOneHundred;
                auto edgeMedianPercent = edgeMedian * DecimalConstants<Num>::DecimalOneHundred;
                auto edgeQnPercent = edgeQn * DecimalConstants<Num>::DecimalOneHundred;
                
                detailsFile << "\nEdge Spread Estimator (30-day window):" << std::endl;
                detailsFile << "  Calculated " << edgeSpreads.size() << " spread measurements" << std::endl;
                detailsFile << "  Mean: " << edgeMeanPercent << "%" << std::endl;
                detailsFile << "  Median: " << edgeMedianPercent << "%" << std::endl;
                detailsFile << "  Robust Qn: " << edgeQnPercent << "%" << std::endl;
            } else {
                detailsFile << "\nEdge: No valid spread calculations could be performed" << std::endl;
            }
            
            detailsFile << "\n(Note: Current slippage estimate assumption: 0.10%)" << std::endl;
        }
        
        detailsFile << "=== End Bid/Ask Spread Analysis ===" << std::endl;
        
    } catch (const std::exception& e) {
        detailsFile << "Error in bid/ask spread analysis: " << e.what() << std::endl;
    }

    detailsFile.close();
}
