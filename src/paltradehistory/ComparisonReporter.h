// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, January 2025
//

#ifndef __COMPARISON_REPORTER_H
#define __COMPARISON_REPORTER_H 1

#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include "TradeComparator.h"
#include "number.h"

namespace mkc_timeseries
{
    /**
     * @brief Comprehensive reporting system for trade comparison results.
     * 
     * @details
     * The ComparisonReporter class provides multiple output formats for trade
     * comparison results, including summary reports, detailed match analysis,
     * mismatch reports, and statistical summaries. It supports both console
     * output and file-based reporting with configurable formatting options.
     * 
     * Key objectives:
     * - Generate comprehensive summary reports with key statistics
     * - Provide detailed trade-by-trade comparison analysis
     * - Create mismatch reports for debugging and validation
     * - Support multiple output formats (console, CSV, detailed text)
     * - Enable customizable reporting templates and formatting
     * 
     * Report types:
     * - Summary Report: High-level statistics and match percentages
     * - Detailed Report: Trade-by-trade comparison with scores
     * - Mismatch Report: Focus on unmatched trades and reasons
     * - Statistical Report: Advanced analytics and distribution analysis
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    class ComparisonReporter
    {
    public:
        /**
         * @brief Enumeration of available report formats.
         */
        enum class ReportFormat
        {
            CONSOLE,        ///< Console output with formatted text
            CSV,            ///< Comma-separated values for spreadsheet import
            DETAILED_TEXT,  ///< Detailed text report with full analysis
            JSON,           ///< JSON format for programmatic consumption
            HTML            ///< HTML format for web viewing
        };

        /**
         * @brief Configuration options for report generation.
         */
        struct ReportConfig
        {
            bool includeSummary;            ///< Include summary statistics
            bool includeMatchedTrades;      ///< Include matched trade details
            bool includeUnmatchedTrades;    ///< Include unmatched trade details
            bool includeScoreBreakdown;     ///< Include individual score components
            bool includeMismatchReasons;    ///< Include detailed mismatch reasons
            bool includeStatistics;         ///< Include advanced statistics
            int decimalPrecision;           ///< Number of decimal places for output
            std::string dateFormat;         ///< Date format string for output

            /**
             * @brief Creates default report configuration.
             */
            ReportConfig()
                : includeSummary(true), includeMatchedTrades(true), includeUnmatchedTrades(true),
                  includeScoreBreakdown(false), includeMismatchReasons(true), includeStatistics(true),
                  decimalPrecision(4), dateFormat("%Y-%m-%d")
            {
            }
        };

    private:
        ReportConfig config_;               ///< Current report configuration
        std::string outputDirectory_;       ///< Directory for output files

    public:
        /**
         * @brief Constructs a ComparisonReporter with default configuration.
         * 
         * @param outputDirectory Directory for output files (default: current directory)
         */
        explicit ComparisonReporter(const std::string& outputDirectory = ".")
            : outputDirectory_(outputDirectory)
        {
        }

        /**
         * @brief Constructs a ComparisonReporter with custom configuration.
         * 
         * @param config Report configuration settings
         * @param outputDirectory Directory for output files
         */
        ComparisonReporter(const ReportConfig& config, const std::string& outputDirectory = ".")
            : config_(config), outputDirectory_(outputDirectory)
        {
        }

        /**
         * @brief Generates a comprehensive comparison report.
         * 
         * @param results Comparison results to report
         * @param format Output format for the report
         * @param filename Output filename (empty for console output)
         * @return true if report generated successfully, false otherwise
         */
        bool generateReport(const ComparisonResults<Decimal>& results, 
                          ReportFormat format = ReportFormat::CONSOLE,
                          const std::string& filename = "")
        {
            switch (format)
            {
                case ReportFormat::CONSOLE:
                    return generateConsoleReport(results);
                case ReportFormat::CSV:
                    return generateCSVReport(results, filename.empty() ? "comparison_report.csv" : filename);
                case ReportFormat::DETAILED_TEXT:
                    return generateDetailedTextReport(results, filename.empty() ? "comparison_report.txt" : filename);
                case ReportFormat::JSON:
                    return generateJSONReport(results, filename.empty() ? "comparison_report.json" : filename);
                case ReportFormat::HTML:
                    return generateHTMLReport(results, filename.empty() ? "comparison_report.html" : filename);
                default:
                    return false;
            }
        }

        /**
         * @brief Generates a summary-only report for quick overview.
         * 
         * @param results Comparison results to summarize
         * @param format Output format for the summary
         * @param filename Output filename (empty for console output)
         * @return true if summary generated successfully, false otherwise
         */
        bool generateSummary(const ComparisonResults<Decimal>& results,
                           ReportFormat format = ReportFormat::CONSOLE,
                           const std::string& filename = "")
        {
            ReportConfig originalConfig = config_;
            config_.includeMatchedTrades = false;
            config_.includeUnmatchedTrades = false;
            config_.includeScoreBreakdown = false;
            
            bool success = generateReport(results, format, filename);
            
            config_ = originalConfig; // Restore original configuration
            return success;
        }

        /**
         * @brief Sets the report configuration.
         * 
         * @param config New report configuration
         */
        void setConfig(const ReportConfig& config)
        {
            config_ = config;
        }

        /**
         * @brief Sets the output directory for file-based reports.
         * 
         * @param directory Output directory path
         */
        void setOutputDirectory(const std::string& directory)
        {
            outputDirectory_ = directory;
        }

    private:
        /**
         * @brief Generates console-formatted report.
         */
        bool generateConsoleReport(const ComparisonResults<Decimal>& results)
        {
            std::cout << std::fixed << std::setprecision(config_.decimalPrecision);
            
            if (config_.includeSummary)
            {
                generateSummarySection(std::cout, results);
            }
            
            if (config_.includeMatchedTrades && !results.matchedTrades.empty())
            {
                generateMatchedTradesSection(std::cout, results);
            }
            
            if (config_.includeUnmatchedTrades && 
                (!results.unmatchedGenerated.empty() || !results.unmatchedExternal.empty()))
            {
                generateUnmatchedTradesSection(std::cout, results);
            }
            
            if (config_.includeStatistics)
            {
                generateStatisticsSection(std::cout, results);
            }
            
            return true;
        }

        /**
         * @brief Generates CSV-formatted report.
         */
        bool generateCSVReport(const ComparisonResults<Decimal>& results, const std::string& filename)
        {
            std::string fullPath = outputDirectory_ + "/" + filename;
            std::ofstream file(fullPath);
            
            if (!file.is_open())
            {
                return false;
            }

            // CSV Header
            file << "Type,Generated_Symbol,Generated_Direction,Generated_Entry_Date,Generated_Exit_Date,"
                 << "Generated_Entry_Price,Generated_Exit_Price,Generated_Return,"
                 << "External_Symbol,External_Direction,External_Entry_Date,External_Exit_Date,"
                 << "External_Entry_Price,External_Exit_Price,External_Return,"
                 << "Match_Score,Match_Status";
            
            if (config_.includeScoreBreakdown)
            {
                file << ",Symbol_Score,Direction_Score,Entry_Date_Score,Exit_Date_Score,"
                     << "Entry_Price_Score,Exit_Price_Score,Return_Score";
            }
            
            if (config_.includeMismatchReasons)
            {
                file << ",Mismatch_Reason";
            }
            
            file << "\n";

            // Matched trades
            for (size_t i = 0; i < results.matchedTrades.size(); ++i)
            {
                const auto& pair = results.matchedTrades[i];
                const auto& matchDetail = results.matchDetails[i];
                
                file << "MATCHED," 
                     << pair.first.getSymbol() << "," << pair.first.getDirection() << ","
                     << pair.first.getEntryDate() << "," << pair.first.getExitDate() << ","
                     << pair.first.getEntryPrice() << "," << pair.first.getExitPrice() << ","
                     << pair.first.getPercentReturn() << ","
                     << pair.second.getSymbol() << "," << pair.second.getDirection() << ","
                     << pair.second.getEntryDate() << "," << pair.second.getExitDate() << ","
                     << pair.second.getEntryPrice() << "," << pair.second.getExitPrice() << ","
                     << pair.second.getProfitPercent() << ","
                     << matchDetail.matchScore << ",MATCH";
                
                if (config_.includeScoreBreakdown)
                {
                    file << "," << matchDetail.symbolScore << "," << matchDetail.directionScore << ","
                         << matchDetail.entryDateScore << "," << matchDetail.exitDateScore << ","
                         << matchDetail.entryPriceScore << "," << matchDetail.exitPriceScore << ","
                         << matchDetail.returnScore;
                }
                
                if (config_.includeMismatchReasons)
                {
                    file << ",";
                }
                
                file << "\n";
            }

            // Unmatched generated trades
            if (config_.includeUnmatchedTrades)
            {
                for (const auto& trade : results.unmatchedGenerated)
                {
                    file << "UNMATCHED_GENERATED,"
                         << trade.getSymbol() << "," << trade.getDirection() << ","
                         << trade.getEntryDate() << "," << trade.getExitDate() << ","
                         << trade.getEntryPrice() << "," << trade.getExitPrice() << ","
                         << trade.getPercentReturn() << ","
                         << ",,,,,,,"  // Empty external fields
                         << "0,NO_MATCH";
                    
                    if (config_.includeScoreBreakdown)
                    {
                        file << ",0,0,0,0,0,0,0";
                    }
                    
                    if (config_.includeMismatchReasons)
                    {
                        file << ",No matching external trade found";
                    }
                    
                    file << "\n";
                }

                // Unmatched external trades
                for (const auto& trade : results.unmatchedExternal)
                {
                    file << "UNMATCHED_EXTERNAL,"
                         << ",,,,,,,"  // Empty generated fields
                         << trade.getSymbol() << "," << trade.getDirection() << ","
                         << trade.getEntryDate() << "," << trade.getExitDate() << ","
                         << trade.getEntryPrice() << "," << trade.getExitPrice() << ","
                         << trade.getProfitPercent() << ","
                         << "0,NO_MATCH";
                    
                    if (config_.includeScoreBreakdown)
                    {
                        file << ",0,0,0,0,0,0,0";
                    }
                    
                    if (config_.includeMismatchReasons)
                    {
                        file << ",No matching generated trade found";
                    }
                    
                    file << "\n";
                }
            }

            file.close();
            return true;
        }

        /**
         * @brief Generates detailed text report.
         */
        bool generateDetailedTextReport(const ComparisonResults<Decimal>& results, const std::string& filename)
        {
            std::string fullPath = outputDirectory_ + "/" + filename;
            std::ofstream file(fullPath);
            
            if (!file.is_open())
            {
                return false;
            }

            file << std::fixed << std::setprecision(config_.decimalPrecision);
            
            file << "=================================================================\n";
            file << "                    TRADE COMPARISON REPORT\n";
            file << "=================================================================\n\n";
            
            if (config_.includeSummary)
            {
                generateSummarySection(file, results);
            }
            
            if (config_.includeMatchedTrades && !results.matchedTrades.empty())
            {
                generateMatchedTradesSection(file, results);
            }
            
            if (config_.includeUnmatchedTrades && 
                (!results.unmatchedGenerated.empty() || !results.unmatchedExternal.empty()))
            {
                generateUnmatchedTradesSection(file, results);
            }
            
            if (config_.includeStatistics)
            {
                generateStatisticsSection(file, results);
            }

            file.close();
            return true;
        }

        /**
         * @brief Generates JSON-formatted report.
         */
        bool generateJSONReport(const ComparisonResults<Decimal>& results, const std::string& filename)
        {
            std::string fullPath = outputDirectory_ + "/" + filename;
            std::ofstream file(fullPath);
            
            if (!file.is_open())
            {
                return false;
            }

            file << "{\n";
            file << "  \"summary\": {\n";
            file << "    \"totalGenerated\": " << results.totalGenerated << ",\n";
            file << "    \"totalExternal\": " << results.totalExternal << ",\n";
            file << "    \"totalMatched\": " << results.totalMatched << ",\n";
            file << "    \"matchPercentage\": " << results.matchPercentage << ",\n";
            file << "    \"averageMatchScore\": " << results.averageMatchScore << "\n";
            file << "  }";
            
            // Add matched trades, unmatched trades, etc. in JSON format
            // (Implementation would continue with full JSON structure)
            
            file << "\n}\n";
            file.close();
            return true;
        }

        /**
         * @brief Generates HTML-formatted report.
         */
        bool generateHTMLReport(const ComparisonResults<Decimal>& results, const std::string& filename)
        {
            // Implementation would generate HTML with tables and styling
            return generateDetailedTextReport(results, filename); // Placeholder
        }

        /**
         * @brief Generates summary section for reports.
         */
        template<typename StreamType>
        void generateSummarySection(StreamType& stream, const ComparisonResults<Decimal>& results)
        {
            stream << "SUMMARY STATISTICS\n";
            stream << "==================\n";
            stream << "Total Generated Trades: " << results.totalGenerated << "\n";
            stream << "Total External Trades:  " << results.totalExternal << "\n";
            stream << "Total Matched Trades:   " << results.totalMatched << "\n";
            stream << "Match Percentage:       " << results.matchPercentage << "%\n";
            stream << "Average Match Score:    " << results.averageMatchScore << "\n";
            stream << "Unmatched Generated:    " << results.unmatchedGenerated.size() << "\n";
            stream << "Unmatched External:     " << results.unmatchedExternal.size() << "\n\n";
        }

        /**
         * @brief Generates matched trades section for reports.
         */
        template<typename StreamType>
        void generateMatchedTradesSection(StreamType& stream, const ComparisonResults<Decimal>& results)
        {
            stream << "MATCHED TRADES (" << results.matchedTrades.size() << ")\n";
            stream << "==============\n";
            
            for (size_t i = 0; i < results.matchedTrades.size(); ++i)
            {
                const auto& pair = results.matchedTrades[i];
                const auto& matchDetail = results.matchDetails[i];
                
                stream << "Match #" << (i + 1) << " (Score: " << matchDetail.matchScore << ")\n";
                stream << "  Generated: " << pair.first.getSymbol() << " " << pair.first.getDirection()
                       << " " << pair.first.getEntryDate() << " -> " << pair.first.getExitDate()
                       << " (" << pair.first.getPercentReturn() << "%)\n";
                stream << "  External:  " << pair.second.getSymbol() << " " << pair.second.getDirection()
                       << " " << pair.second.getEntryDate() << " -> " << pair.second.getExitDate()
                       << " (" << pair.second.getProfitPercent() << "%)\n\n";
            }
        }

        /**
         * @brief Generates unmatched trades section for reports.
         */
        template<typename StreamType>
        void generateUnmatchedTradesSection(StreamType& stream, const ComparisonResults<Decimal>& results)
        {
            if (!results.unmatchedGenerated.empty())
            {
                stream << "UNMATCHED GENERATED TRADES (" << results.unmatchedGenerated.size() << ")\n";
                stream << "==========================\n";
                
                for (const auto& trade : results.unmatchedGenerated)
                {
                    stream << "  " << trade.getSymbol() << " " << trade.getDirection()
                           << " " << trade.getEntryDate() << " -> " << trade.getExitDate()
                           << " (" << trade.getPercentReturn() << "%)\n";
                }
                stream << "\n";
            }

            if (!results.unmatchedExternal.empty())
            {
                stream << "UNMATCHED EXTERNAL TRADES (" << results.unmatchedExternal.size() << ")\n";
                stream << "=========================\n";
                
                for (const auto& trade : results.unmatchedExternal)
                {
                    stream << "  " << trade.getSymbol() << " " << trade.getDirection()
                           << " " << trade.getEntryDate() << " -> " << trade.getExitDate()
                           << " (" << trade.getProfitPercent() << "%)\n";
                }
                stream << "\n";
            }
        }

        /**
         * @brief Generates statistics section for reports.
         */
        template<typename StreamType>
        void generateStatisticsSection(StreamType& stream, const ComparisonResults<Decimal>& results)
        {
            stream << "ADVANCED STATISTICS\n";
            stream << "===================\n";
            
            if (results.totalMatched > 0)
            {
                // Calculate additional statistics
                Decimal minScore = DecimalConstants<Decimal>::DecimalOne;
                Decimal maxScore = DecimalConstants<Decimal>::DecimalZero;
                
                for (const auto& match : results.matchDetails)
                {
                    if (match.matchScore < minScore) minScore = match.matchScore;
                    if (match.matchScore > maxScore) maxScore = match.matchScore;
                }
                
                stream << "Minimum Match Score:    " << minScore << "\n";
                stream << "Maximum Match Score:    " << maxScore << "\n";
                stream << "Score Range:            " << (maxScore - minScore) << "\n";
            }
            
            stream << "\n";
        }
    };
}

#endif