// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __TIMESERIES_FORMATTERS_H
#define __TIMESERIES_FORMATTERS_H 1

#include "ITimeSeriesFormatter.h"
#include <boost/date_time.hpp>
#include <iomanip>

namespace mkc_timeseries
{
    using boost::posix_time::ptime;

    /**
     * @brief Formatter for PAL EOD format: Date,Open,High,Low,Close
     * 
     * This formatter maintains backward compatibility with the original
     * PalTimeSeriesCsvWriter. It outputs date in ISO format (YYYYMMDD)
     * followed by OHLC values, with no header.
     */
    template <class Decimal>
    class PalEodFormatter : public ITimeSeriesFormatter<Decimal>
    {
    public:
        void writeHeader(std::ofstream& file) override
        {
            // No header for PAL EOD format
        }
        
        void writeEntry(std::ofstream& file,
                       const OHLCTimeSeriesEntry<Decimal>& entry) override
        {
            ptime dateTime = entry.getDateTime();
            file << boost::gregorian::to_iso_string(dateTime.date()) << ","
                 << entry.getOpenValue() << ","
                 << entry.getHighValue() << ","
                 << entry.getLowValue() << ","
                 << entry.getCloseValue() << std::endl;
        }
    };

    /**
     * @brief Formatter for PAL Volume for Close format: Date,Open,High,Low,Volume
     * 
     * This formatter maintains backward compatibility with the original
     * PalVolumeForCloseCsvWriter. It outputs date in ISO format (YYYYMMDD)
     * followed by OHLV values (volume instead of close), with no header.
     */
    template <class Decimal>
    class PalVolumeForCloseFormatter : public ITimeSeriesFormatter<Decimal>
    {
    public:
        void writeHeader(std::ofstream& file) override
        {
            // No header for PAL Volume format
        }
        
        void writeEntry(std::ofstream& file,
                       const OHLCTimeSeriesEntry<Decimal>& entry) override
        {
            ptime dateTime = entry.getDateTime();
            file << boost::gregorian::to_iso_string(dateTime.date()) << ","
                 << entry.getOpenValue() << ","
                 << entry.getHighValue() << ","
                 << entry.getLowValue() << ","
                 << entry.getVolumeValue() << std::endl;
        }
    };

    /**
     * @brief Formatter for TradeStation EOD format: "Date","Time","Open","High","Low","Close","Vol","OI"
     * 
     * This formatter outputs data in TradeStation's end-of-day format with:
     * - Quoted headers
     * - Date in MM/dd/yyyy format
     * - Time as 00:00 for daily data
     * - OI (Open Interest) field set to 0
     */
    template <class Decimal>
    class TradeStationEodFormatter : public ITimeSeriesFormatter<Decimal>
    {
    public:
        void writeHeader(std::ofstream& file) override
        {
            file << "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Vol\",\"OI\"" << std::endl;
        }
        
        void writeEntry(std::ofstream& file,
                       const OHLCTimeSeriesEntry<Decimal>& entry) override
        {
            ptime dateTime = entry.getDateTime();
            auto date = dateTime.date();
            file << std::setfill('0') << std::setw(2) << static_cast<int>(date.month()) << "/"
                 << std::setw(2) << date.day() << "/"
                 << date.year() << ",00:00,"
                 << entry.getOpenValue() << ","
                 << entry.getHighValue() << ","
                 << entry.getLowValue() << ","
                 << entry.getCloseValue() << ","
                 << entry.getVolumeValue() << ",0" << std::endl;
        }
    };

    /**
     * @brief Formatter for TradeStation Intraday format: "Date","Time","Open","High","Low","Close","Up","Down"
     * 
     * This formatter outputs data in TradeStation's intraday format with:
     * - Quoted headers
     * - Date in MM/dd/yyyy format
     * - Time in HH:MM format from the entry's timestamp
     * - Up and Down fields set to 0
     */
    template <class Decimal>
    class TradeStationIntradayFormatter : public ITimeSeriesFormatter<Decimal>
    {
    public:
        void writeHeader(std::ofstream& file) override
        {
            file << "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Up\",\"Down\"" << std::endl;
        }
        
        void writeEntry(std::ofstream& file,
                       const OHLCTimeSeriesEntry<Decimal>& entry) override
        {
            ptime dateTime = entry.getDateTime();
            auto date = dateTime.date();
            auto time = dateTime.time_of_day();
            
            file << std::setfill('0') << std::setw(2) << static_cast<int>(date.month()) << "/"
                 << std::setw(2) << date.day() << "/"
                 << date.year() << ","
                 << std::setw(2) << time.hours() << ":"
                 << std::setw(2) << time.minutes() << ","
                 << entry.getOpenValue() << ","
                 << entry.getHighValue() << ","
                 << entry.getLowValue() << ","
                 << entry.getCloseValue() << ",0,0" << std::endl;
        }
    };

    /**
     * @brief Formatter for PAL Intraday format: Sequential# Open High Low Close
     *
     * This formatter outputs data in PAL's intraday format with:
     * - No header
     * - Sequential numbering starting at 10000001 (managed internally)
     * - Space-separated values
     * - Only OHLC data (no date, time, or volume)
     *
     * Each instance of this formatter maintains its own sequential counter,
     * starting at 10000001 and incrementing with each entry written.
     */
    template <class Decimal>
    class PalIntradayFormatter : public ITimeSeriesFormatter<Decimal>
    {
    private:
        mutable long mSequentialCounter = 10000001;  ///< Internal sequential counter starting at 10000001

    public:
        void writeHeader(std::ofstream& file) override
        {
            // No header for PAL intraday format
        }
        
        void writeEntry(std::ofstream& file,
                       const OHLCTimeSeriesEntry<Decimal>& entry) override
        {
            file << mSequentialCounter << " "
                 << entry.getOpenValue() << " "
                 << entry.getHighValue() << " "
                 << entry.getLowValue() << " "
                 << entry.getCloseValue() << std::endl;
            ++mSequentialCounter;
        }
    };
}

#endif // __TIMESERIES_FORMATTERS_H