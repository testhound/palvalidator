// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __ITIMESERIES_FORMATTER_H
#define __ITIMESERIES_FORMATTER_H 1

#include <fstream>
#include "TimeSeriesEntry.h"

namespace mkc_timeseries
{
    /**
     * @brief Interface for formatting OHLC time series entries to CSV output.
     * 
     * This interface defines the contract for all time series formatters.
     * Each formatter implements a specific output format (PAL, TradeStation, etc.)
     * and handles the details of header generation, entry formatting, and
     * any format-specific requirements like sequential counters.
     * 
     * @tparam Decimal The numeric type used for price and volume data (e.g., double, float).
     */
    template <class Decimal>
    class ITimeSeriesFormatter
    {
    public:
        virtual ~ITimeSeriesFormatter() = default;

        /**
         * @brief Writes the CSV header to the output file.
         * 
         * Some formats require headers (e.g., TradeStation formats), while others
         * do not (e.g., PAL formats). Implementations should write appropriate
         * headers or do nothing if no header is required.
         * 
         * @param file The output file stream to write to.
         */
        virtual void writeHeader(std::ofstream& file) = 0;

        /**
         * @brief Writes a single OHLC entry to the output file.
         *
         * Formats the given entry according to the specific format requirements
         * and writes it to the file. Each formatter manages its own internal
         * state as needed (e.g., sequential counters for PAL Intraday format).
         *
         * @param file The output file stream to write to.
         * @param entry The OHLC time series entry to format and write.
         */
        virtual void writeEntry(std::ofstream& file,
                               const OHLCTimeSeriesEntry<Decimal>& entry) = 0;
    };
}

#endif // __ITIMESERIES_FORMATTER_H