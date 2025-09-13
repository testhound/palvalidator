#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <iomanip>
#include <filesystem>
#include <sstream>

#include "ValidatorConfiguration.h"
#include "SecurityAttributesFactory.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "Portfolio.h"
#include "number.h"

// Include comparison functionality headers
#include "ExternalTrade.h"
#include "GeneratedTrade.h"
#include "ComparisonTolerance.h"
#include "ExternalTradeParser.h"
#include "TradeComparator.h"
#include "ComparisonReporter.h"

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

void usage()
{
    printf("Usage: paltradehistory <config file> [options]\n");
    printf("  Generates a CSV file with detailed trade history from PAL patterns.\n");
    printf("  Output file will be named: <security_symbol>_trade_history.csv\n\n");
    printf("Options:\n");
    printf("  --compare <external_file>    Compare generated trades with external backtesting results\n");
    printf("  --tolerance-strict           Use strict comparison tolerances (default)\n");
    printf("  --tolerance-relaxed          Use relaxed comparison tolerances\n");
    printf("  --report-format <format>     Output format: console, csv, text, json (default: console)\n");
    printf("  --report-file <filename>     Output file for comparison report\n");
    printf("  --help                       Show this help message\n\n");
    printf("Examples:\n");
    printf("  paltradehistory config.json\n");
    printf("  paltradehistory config.json --compare ASML_WL_Positions.csv\n");
    printf("  paltradehistory config.json --compare external.csv --report-format csv --report-file comparison.csv\n");
}

std::string createTradeHistoryFileName(const std::string& securitySymbol)
{
    return securitySymbol + "_trade_history.csv";
}

void writeTradeHistoryCSV(const std::string& filename,
                         const std::string& securitySymbol,
                         const ClosedPositionHistory<Num>& closedHistory)
{
    std::ofstream csvFile(filename);
    if (!csvFile.is_open()) {
        throw std::runtime_error("Failed to create CSV file: " + filename);
    }

    // Write CSV header
    csvFile << "Ticker,Direction,EntryDateTime,EntryPrice,ExitDate,ExitPrice,PercentReturn,BarsInPosition\n";

    // Iterate through all closed positions
    auto it = closedHistory.beginTradingPositions();
    for (; it != closedHistory.endTradingPositions(); ++it) {
        const auto& position = it->second;  // Access the shared_ptr<TradingPosition> from the pair
        
        // Extract trade details
        std::string direction = position->isLongPosition() ? "Long" : "Short";
        
        // Format entry date/time
        auto entryDateTime = position->getEntryDateTime();
        std::ostringstream entryStream;
        entryStream << boost::posix_time::to_simple_string(entryDateTime);
        
        // Format exit date
        auto exitDate = position->getExitDate();
        std::ostringstream exitStream;
        exitStream << boost::gregorian::to_simple_string(exitDate);
        
        // Write trade data to CSV
        csvFile << securitySymbol << ","
                << direction << ","
                << entryStream.str() << ","
                << std::fixed << std::setprecision(2) << position->getEntryPrice() << ","
                << exitStream.str() << ","
                << std::fixed << std::setprecision(2) << position->getExitPrice() << ","
                << std::fixed << std::setprecision(2) << position->getPercentReturn() << ","
                << position->getNumBarsInPosition() << "\n";
    }
    
    csvFile.close();
}

std::vector<GeneratedTrade<Num>> convertToGeneratedTrades(const std::string& securitySymbol,
                                                         const ClosedPositionHistory<Num>& closedHistory)
{
    std::vector<GeneratedTrade<Num>> generatedTrades;
    
    auto it = closedHistory.beginTradingPositions();
    for (; it != closedHistory.endTradingPositions(); ++it) {
        const auto& position = it->second;
        
        std::string direction = position->isLongPosition() ? "Long" : "Short";
        
        GeneratedTrade<Num> trade(
            securitySymbol,
            direction,
            position->getEntryDateTime(),
            position->getExitDateTime(),
            position->getEntryPrice(),
            position->getExitPrice(),
            position->getPercentReturn(),
            static_cast<int>(position->getNumBarsInPosition())
        );
        
        generatedTrades.push_back(trade);
    }
    
    return generatedTrades;
}

ComparisonReporter<Num>::ReportFormat parseReportFormat(const std::string& format)
{
    if (format == "console") return ComparisonReporter<Num>::ReportFormat::CONSOLE;
    if (format == "csv") return ComparisonReporter<Num>::ReportFormat::CSV;
    if (format == "text") return ComparisonReporter<Num>::ReportFormat::DETAILED_TEXT;
    if (format == "json") return ComparisonReporter<Num>::ReportFormat::JSON;
    
    std::cout << "Warning: Unknown report format '" << format << "', using console format." << std::endl;
    return ComparisonReporter<Num>::ReportFormat::CONSOLE;
}

void performComparison(const std::string& externalFile,
                      const std::vector<GeneratedTrade<Num>>& generatedTrades,
                      bool useRelaxedTolerance,
                      const std::string& reportFormat,
                      const std::string& reportFile)
{
    try {
        std::cout << "\nPerforming trade comparison..." << std::endl;
        std::cout << "External file: " << externalFile << std::endl;
        
        // Parse external trades
        ExternalTradeParser<Num> parser(ExternalTradeParser<Num>::PlatformFormat::WEALTHLAB);
        std::vector<ExternalTrade<Num>> externalTrades = parser.parseFile(externalFile);
        
        std::cout << "Loaded " << externalTrades.size() << " external trades" << std::endl;
        std::cout << "Generated " << generatedTrades.size() << " PAL trades" << std::endl;
        
        // Set up comparison tolerance
        ComparisonTolerance<Num> tolerance;
        if (useRelaxedTolerance) {
            tolerance = ComparisonTolerance<Num>::createRelaxedTolerance();
            std::cout << "Using relaxed comparison tolerances" << std::endl;
        } else {
            tolerance = ComparisonTolerance<Num>::createStrictTolerance();
            std::cout << "Using strict comparison tolerances" << std::endl;
        }
        
        // Perform comparison
        TradeComparator<Num> comparator(TradeComparator<Num>::MatchingStrategy::FUZZY, tolerance);
        ComparisonResults<Num> results = comparator.compareTradeCollections(generatedTrades, externalTrades);
        
        // Generate report
        ComparisonReporter<Num> reporter;
        ComparisonReporter<Num>::ReportFormat format = parseReportFormat(reportFormat);
        
        std::cout << "\nComparison Results Summary:" << std::endl;
        std::cout << "===========================" << std::endl;
        std::cout << "Total Generated Trades: " << results.totalGenerated << std::endl;
        std::cout << "Total External Trades:  " << results.totalExternal << std::endl;
        std::cout << "Total Matched Trades:   " << results.totalMatched << std::endl;
        std::cout << "Match Percentage:       " << std::fixed << std::setprecision(2) 
                  << results.matchPercentage << "%" << std::endl;
        std::cout << "Average Match Score:    " << std::fixed << std::setprecision(4) 
                  << results.averageMatchScore << std::endl;
        
        // Generate detailed report
        if (format == ComparisonReporter<Num>::ReportFormat::CONSOLE) {
            std::cout << "\nDetailed Comparison Report:" << std::endl;
            std::cout << "===========================" << std::endl;
            reporter.generateReport(results, format);
        } else {
            std::string filename = reportFile.empty() ? "comparison_report" : reportFile;
            if (reporter.generateReport(results, format, filename)) {
                std::cout << "Detailed comparison report written to: " << filename << std::endl;
            } else {
                std::cout << "Error: Failed to write comparison report to file." << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error during comparison: " << e.what() << std::endl;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage();
        return 1;
    }

    std::string configurationFileName = std::string(argv[1]);
    
    // Parse command line options
    std::string externalFile;
    bool performComparisonFlag = false;
    bool useRelaxedTolerance = false;
    std::string reportFormat = "console";
    std::string reportFile;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            usage();
            return 0;
        } else if (arg == "--compare" && i + 1 < argc) {
            externalFile = argv[++i];
            performComparisonFlag = true;
        } else if (arg == "--tolerance-strict") {
            useRelaxedTolerance = false;
        } else if (arg == "--tolerance-relaxed") {
            useRelaxedTolerance = true;
        } else if (arg == "--report-format" && i + 1 < argc) {
            reportFormat = argv[++i];
        } else if (arg == "--report-file" && i + 1 < argc) {
            reportFile = argv[++i];
        } else {
            std::cout << "Unknown option: " << arg << std::endl;
            usage();
            return 1;
        }
    }
    
    // Check if configuration file exists
    if (!std::filesystem::exists(configurationFileName)) {
        std::cout << "Error: Configuration file '" << configurationFileName << "' does not exist." << std::endl;
        return 1;
    }
    
    // Check if external file exists when comparison is requested
    if (performComparisonFlag && !std::filesystem::exists(externalFile)) {
        std::cout << "Error: External comparison file '" << externalFile << "' does not exist." << std::endl;
        return 1;
    }

    try {
        // Read configuration file
        std::cout << "Reading configuration file: " << configurationFileName << std::endl;
        ValidatorConfigurationFileReader reader(configurationFileName);
        auto config = reader.readConfigurationFile();
        
        std::cout << "Security: " << config->getSecurity()->getSymbol() << std::endl;
        std::cout << "Out-of-sample period: " << config->getOosDateRange().getFirstDateTime()
                  << " to " << config->getOosDateRange().getLastDateTime() << std::endl;

        // Create meta-strategy portfolio
        auto metaPortfolio = std::make_shared<Portfolio<Num>>("Trade History Portfolio");
        metaPortfolio->addSecurity(config->getSecurity());
        
        // Create PalMetaStrategy
        auto metaStrategy = std::make_shared<PalMetaStrategy<Num>>("Trade History Strategy", metaPortfolio);
        
        // Add all patterns from configuration
        auto patterns = config->getPricePatterns();
        std::cout << "Adding " << patterns->getNumPatterns() << " patterns to meta-strategy..." << std::endl;
        
        auto patternIt = patterns->allPatternsBegin();
        for (; patternIt != patterns->allPatternsEnd(); ++patternIt) {
            metaStrategy->addPricePattern(*patternIt);
        }

        // Determine timeframe and create backtester
        auto timeFrame = config->getSecurity()->getTimeSeries()->getTimeFrame();
        std::cout << "Running backtest on out-of-sample period..." << std::endl;
        
        auto backtester = BackTesterFactory<Num>::backTestStrategy(
            metaStrategy, 
            timeFrame, 
            config->getOosDateRange()
        );

        // Get closed position history
        const auto& closedHistory = backtester->getClosedPositionHistory();
        std::cout << "Backtest completed. Found " << closedHistory.getNumPositions() << " closed trades." << std::endl;

        if (closedHistory.getNumPositions() == 0) {
            std::cout << "No trades were generated. CSV file will not be created." << std::endl;
            if (performComparisonFlag) {
                std::cout << "Cannot perform comparison with zero generated trades." << std::endl;
            }
            return 0;
        }

        // Generate CSV file
        std::string csvFileName = createTradeHistoryFileName(config->getSecurity()->getSymbol());
        std::cout << "Writing trade history to: " << csvFileName << std::endl;
        
        writeTradeHistoryCSV(csvFileName, config->getSecurity()->getSymbol(), closedHistory);
        
        std::cout << "Trade history successfully written to " << csvFileName << std::endl;
        std::cout << "Total trades: " << closedHistory.getNumPositions() << std::endl;

        // Perform comparison if requested
        if (performComparisonFlag) {
            std::vector<GeneratedTrade<Num>> generatedTrades = 
                convertToGeneratedTrades(config->getSecurity()->getSymbol(), closedHistory);
            
            performComparison(externalFile, generatedTrades, useRelaxedTolerance, reportFormat, reportFile);
        }

    } catch (const SecurityAttributesFactoryException& e) {
        std::cout << "SecurityAttributesFactoryException: " << e.what() << std::endl;
        return 1;
    } catch (const ValidatorConfigurationException& e) {
        std::cout << "ValidatorConfigurationException: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}