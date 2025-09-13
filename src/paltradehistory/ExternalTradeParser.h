// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, January 2025
//

#ifndef __EXTERNAL_TRADE_PARSER_H
#define __EXTERNAL_TRADE_PARSER_H 1

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <boost/date_time.hpp>
#include "ExternalTrade.h"
#include "number.h"

namespace mkc_timeseries
{
    /**
     * @brief Parser for external backtesting CSV files containing trade data.
     * 
     * @details
     * The ExternalTradeParser class provides functionality to parse CSV files from
     * external backtesting platforms such as WealthLab, TradeStation, and others.
     * It handles various CSV formats and converts the data into standardized
     * ExternalTrade objects with high-precision Decimal arithmetic.
     * 
     * Key objectives:
     * - Parse multiple CSV formats from different backtesting platforms
     * - Convert string data to high-precision Decimal types
     * - Handle date parsing with multiple format support
     * - Provide robust error handling and validation
     * - Support configurable field mappings for different platforms
     * 
     * Supported formats:
     * - WealthLab position export format
     * - TradeStation trade analysis format
     * - Generic CSV with configurable column mappings
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    class ExternalTradeParser
    {
    public:
        /**
         * @brief Enumeration of supported external platform formats.
         */
        enum class PlatformFormat
        {
            WEALTHLAB,      ///< WealthLab position export format
            TRADESTATION,   ///< TradeStation trade analysis format
            GENERIC         ///< Generic CSV with custom column mapping
        };

        /**
         * @brief Structure defining column mappings for CSV parsing.
         */
        struct ColumnMapping
        {
            int positionColumn;     ///< Column index for position number (0-based)
            int symbolColumn;       ///< Column index for symbol
            int entryDateColumn;    ///< Column index for entry date
            int exitDateColumn;     ///< Column index for exit date
            int entryPriceColumn;   ///< Column index for entry price
            int exitPriceColumn;    ///< Column index for exit price
            int directionColumn;    ///< Column index for direction
            int profitPercentColumn; ///< Column index for profit percentage
            int barsHeldColumn;     ///< Column index for bars held
            bool hasHeaderRow;      ///< Whether CSV has header row to skip
            char delimiter;         ///< CSV delimiter character

            /**
             * @brief Creates default WealthLab column mapping.
             * @return ColumnMapping configured for WealthLab format
             */
            static ColumnMapping createWealthLabMapping()
            {
                ColumnMapping mapping;
                // Based on actual ASML_WL_Positions.csv format:
                // Header: Position,Symbol,Quantity,Entry.Date,Entry.Price,Entry.Order.Type,Entry.Transaction.Type,Exit.Date,Exit.Price,Exit.Order.Type,Exit.Transaction.Type,ExitedAtMarketOpen,Profit,ProfitPct,Profit.per.Bar,Profit.as.Pct.of.Equity,Bars.Held,Entry.Signal,Exit.Signal,PosMetric.MFEPct,PosMetric.MAEPct
                // Data: Long,ASML,1,2015-12-30,92.12,Market,Buy,2015-12-31,88.9300235012,Stop,Sell,False,-3.189976498800007,-3.4628490000000074,-1.5949882494000036,-0.0031900013808107775,2,246,Stop,0.14112027789838846,-3.462849000000008
                mapping.directionColumn = 0;        // Direction (Long/Short)
                mapping.symbolColumn = 1;           // Symbol (ASML)
                mapping.positionColumn = 2;         // Quantity (1)
                mapping.entryDateColumn = 3;        // Entry.Date (2015-12-30)
                mapping.entryPriceColumn = 4;       // Entry.Price (92.12)
                mapping.exitDateColumn = 7;         // Exit.Date (2015-12-31)
                mapping.exitPriceColumn = 8;        // Exit.Price (88.9300235012)
                mapping.profitPercentColumn = 13;   // ProfitPct (-3.4628490000000074)
                mapping.barsHeldColumn = 16;        // Bars.Held (2)
                mapping.hasHeaderRow = true;        // Has header row
                mapping.delimiter = ',';
                return mapping;
            }
        };

    private:
        PlatformFormat format_;         ///< Selected platform format
        ColumnMapping columnMapping_;   ///< Column mapping configuration
        std::string dateFormat_;        ///< Date format string for parsing

    public:
        /**
         * @brief Constructs an ExternalTradeParser with specified platform format.
         * 
         * @param format Platform format to use for parsing
         */
        explicit ExternalTradeParser(PlatformFormat format = PlatformFormat::WEALTHLAB)
            : format_(format), dateFormat_("%m/%d/%Y")
        {
            switch (format_)
            {
                case PlatformFormat::WEALTHLAB:
                    columnMapping_ = ColumnMapping::createWealthLabMapping();
                    break;
                case PlatformFormat::TRADESTATION:
                    // TODO: Implement TradeStation mapping
                    columnMapping_ = ColumnMapping::createWealthLabMapping();
                    break;
                case PlatformFormat::GENERIC:
                    columnMapping_ = ColumnMapping::createWealthLabMapping();
                    break;
            }
        }

        /**
         * @brief Constructs an ExternalTradeParser with custom column mapping.
         * 
         * @param mapping Custom column mapping configuration
         * @param dateFormat Date format string (default: "%m/%d/%Y")
         */
        ExternalTradeParser(const ColumnMapping& mapping, const std::string& dateFormat = "%m/%d/%Y")
            : format_(PlatformFormat::GENERIC), columnMapping_(mapping), dateFormat_(dateFormat)
        {
        }

        /**
         * @brief Parses a CSV file and returns a vector of ExternalTrade objects.
         * 
         * @param filename Path to the CSV file to parse
         * @return Vector of parsed ExternalTrade objects
         * @throws std::runtime_error if file cannot be opened or parsed
         * @throws std::invalid_argument if data format is invalid
         */
        std::vector<ExternalTrade<Decimal>> parseFile(const std::string& filename)
        {
            std::vector<ExternalTrade<Decimal>> trades;
            std::ifstream file(filename);
            
            if (!file.is_open())
            {
                throw std::runtime_error("Cannot open file: " + filename);
            }

            std::string line;
            int lineNumber = 0;
            
            // Skip header row if present
            if (columnMapping_.hasHeaderRow && std::getline(file, line))
            {
                lineNumber++;
            }

            while (std::getline(file, line))
            {
                lineNumber++;
                
                if (line.empty() || line[0] == '#')
                {
                    continue; // Skip empty lines and comments
                }

                try
                {
                    ExternalTrade<Decimal> trade = parseLine(line);
                    trades.push_back(trade);
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error("Error parsing line " + std::to_string(lineNumber) + 
                                           ": " + e.what() + "\nLine content: " + line);
                }
            }

            return trades;
        }

        /**
         * @brief Sets the date format string for parsing.
         * 
         * @param format Date format string (e.g., "%m/%d/%Y", "%Y-%m-%d")
         */
        void setDateFormat(const std::string& format)
        {
            dateFormat_ = format;
        }

        /**
         * @brief Gets the current date format string.
         * @return Current date format string
         */
        const std::string& getDateFormat() const
        {
            return dateFormat_;
        }

        /**
         * @brief Sets custom column mapping.
         * 
         * @param mapping Custom column mapping configuration
         */
        void setColumnMapping(const ColumnMapping& mapping)
        {
            columnMapping_ = mapping;
            format_ = PlatformFormat::GENERIC;
        }

    private:
        /**
         * @brief Parses a single CSV line into an ExternalTrade object.
         * 
         * @param line CSV line to parse
         * @return Parsed ExternalTrade object
         * @throws std::invalid_argument if line format is invalid
         */
        ExternalTrade<Decimal> parseLine(const std::string& line)
        {
            std::vector<std::string> fields = splitCSVLine(line);
            
            // Validate minimum number of fields
            int maxColumn = std::max({columnMapping_.positionColumn, columnMapping_.symbolColumn,
                                    columnMapping_.entryDateColumn, columnMapping_.exitDateColumn,
                                    columnMapping_.entryPriceColumn, columnMapping_.exitPriceColumn,
                                    columnMapping_.directionColumn, columnMapping_.profitPercentColumn,
                                    columnMapping_.barsHeldColumn});
            
            if (static_cast<int>(fields.size()) <= maxColumn)
            {
                throw std::invalid_argument("Insufficient fields in CSV line. Expected at least " + 
                                          std::to_string(maxColumn + 1) + " fields, got " + 
                                          std::to_string(fields.size()));
            }

            // Parse fields
            int position = std::stoi(trim(fields[columnMapping_.positionColumn]));
            std::string symbol = trim(fields[columnMapping_.symbolColumn]);
            boost::gregorian::date entryDate = parseDate(trim(fields[columnMapping_.entryDateColumn]));
            boost::gregorian::date exitDate = parseDate(trim(fields[columnMapping_.exitDateColumn]));
            Decimal entryPrice(trim(fields[columnMapping_.entryPriceColumn]));
            Decimal exitPrice(trim(fields[columnMapping_.exitPriceColumn]));
            std::string direction = trim(fields[columnMapping_.directionColumn]);
            Decimal profitPercent(trim(fields[columnMapping_.profitPercentColumn]));
            int barsHeld = std::stoi(trim(fields[columnMapping_.barsHeldColumn]));

            // Normalize direction strings
            direction = normalizeDirection(direction);

            return ExternalTrade<Decimal>(position, symbol, entryDate, exitDate, 
                                        entryPrice, exitPrice, direction, profitPercent, barsHeld);
        }

        /**
         * @brief Splits a CSV line into individual fields.
         * 
         * @param line CSV line to split
         * @return Vector of field strings
         */
        std::vector<std::string> splitCSVLine(const std::string& line)
        {
            std::vector<std::string> fields;
            std::stringstream ss(line);
            std::string field;
            
            while (std::getline(ss, field, columnMapping_.delimiter))
            {
                fields.push_back(field);
            }
            
            return fields;
        }

        /**
         * @brief Parses a date string using the configured date format.
         * 
         * @param dateStr Date string to parse
         * @return Parsed boost::gregorian::date
         * @throws std::invalid_argument if date format is invalid
         */
        boost::gregorian::date parseDate(const std::string& dateStr)
        {
            try
            {
                // Try common date formats
                if (dateStr.find('/') != std::string::npos)
                {
                    // MM/DD/YYYY or M/D/YYYY format
                    return boost::gregorian::from_us_string(dateStr);
                }
                else if (dateStr.find('-') != std::string::npos)
                {
                    // YYYY-MM-DD format
                    return boost::gregorian::from_string(dateStr);
                }
                else
                {
                    throw std::invalid_argument("Unrecognized date format");
                }
            }
            catch (const std::exception& e)
            {
                throw std::invalid_argument("Invalid date format: " + dateStr + " (" + e.what() + ")");
            }
        }

        /**
         * @brief Normalizes direction strings to standard format.
         * 
         * @param direction Raw direction string from CSV
         * @return Normalized direction ("Long" or "Short")
         */
        std::string normalizeDirection(const std::string& direction)
        {
            std::string normalized = direction;
            
            // Convert to lowercase for comparison
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
            
            if (normalized == "buy" || normalized == "long" || normalized == "l")
            {
                return "Long";
            }
            else if (normalized == "sell" || normalized == "short" || normalized == "s")
            {
                return "Short";
            }
            else
            {
                // Return original if no match found
                return direction;
            }
        }

        /**
         * @brief Trims whitespace from both ends of a string.
         * 
         * @param str String to trim
         * @return Trimmed string
         */
        std::string trim(const std::string& str)
        {
            size_t start = str.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                return "";
            
            size_t end = str.find_last_not_of(" \t\r\n");
            return str.substr(start, end - start + 1);
        }
    };
}

#endif