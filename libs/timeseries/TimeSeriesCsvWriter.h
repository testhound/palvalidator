// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __CSVWRITER_H
#define __CSVWRITER_H 1

#include "TimeSeries.h"
#include "OutputFormat.h"
#include "ITimeSeriesFormatter.h"
#include "TimeSeriesFormatters.h"
#include <boost/date_time.hpp>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace mkc_timeseries
{
  using boost::posix_time::ptime;

  //
  // New Unified TimeSeriesCsvWriter Class
  //
  
  /**
   * @brief Unified CSV writer for OHLC time series data supporting multiple output formats.
   *
   * This class provides a flexible way to write time series data to CSV files in various
   * formats including PAL, TradeStation, and custom formats. It uses the strategy pattern
   * with formatters to handle format-specific requirements. Each formatter manages its
   * own internal state as needed (e.g., sequential counters for PAL Intraday format).
   *
   * @tparam Decimal The numeric type used for price and volume data (e.g., double, float).
   */
  template <class Decimal>
  class TimeSeriesCsvWriter
  {
  private:
    std::ofstream mCsvFile;
    const OHLCTimeSeries<Decimal>& mTimeSeries;
    std::unique_ptr<ITimeSeriesFormatter<Decimal>> mFormatter;

  public:
    /**
     * @brief Constructs a TimeSeriesCsvWriter for the specified format.
     *
     * @param fileName The output CSV file name.
     * @param series The time series data to write.
     * @param format The output format to use.
     */
    TimeSeriesCsvWriter(const std::string& fileName,
                       const OHLCTimeSeries<Decimal>& series,
                       OutputFormat format)
        : mCsvFile(fileName), mTimeSeries(series)
    {
        mFormatter = createFormatter(format);
    }

    // Copy constructor - Note: ofstream is not copyable
    TimeSeriesCsvWriter(const TimeSeriesCsvWriter& rhs) = delete;

    // Assignment operator - Note: ofstream is not copyable
    TimeSeriesCsvWriter& operator=(const TimeSeriesCsvWriter& rhs) = delete;

    ~TimeSeriesCsvWriter() = default;

    /**
     * @brief Writes the time series data to the CSV file.
     *
     * This method writes the header (if required by the format) and then
     * iterates through all entries in the time series, formatting each
     * according to the specified output format. Each formatter manages
     * its own internal state as needed.
     */
    void writeFile()
    {
        mFormatter->writeHeader(mCsvFile);
        
        for (auto it = mTimeSeries.beginSortedAccess();
             it != mTimeSeries.endSortedAccess(); ++it)
        {
            mFormatter->writeEntry(mCsvFile, *it);
        }
    }

  private:
    /**
     * @brief Factory method to create the appropriate formatter for the given format.
     *
     * @param format The output format enum value.
     * @return A unique pointer to the appropriate formatter implementation.
     * @throws std::invalid_argument if the format is not supported.
     */
    std::unique_ptr<ITimeSeriesFormatter<Decimal>> createFormatter(OutputFormat format)
    {
        switch (format) {
            case OutputFormat::PAL_EOD:
                return std::make_unique<PalEodFormatter<Decimal>>();
            case OutputFormat::PAL_VOLUME_FOR_CLOSE:
                return std::make_unique<PalVolumeForCloseFormatter<Decimal>>();
            case OutputFormat::TRADESTATION_EOD:
                return std::make_unique<TradeStationEodFormatter<Decimal>>();
            case OutputFormat::TRADESTATION_INTRADAY:
                return std::make_unique<TradeStationIntradayFormatter<Decimal>>();
            case OutputFormat::PAL_INTRADAY:
                return std::make_unique<PalIntradayFormatter<Decimal>>();
            default:
                throw std::invalid_argument("Unsupported output format");
        }
    }
  };

  /**
   * @brief Unified CSV writer for OHLC time series data with indicator support.
   *
   * This class extends the basic TimeSeriesCsvWriter to support indicator-based output formats
   * where an indicator value (like IBS) replaces the close price in the output.
   * It uses synchronized iteration over both OHLC and indicator data.
   *
   * @tparam Decimal The numeric type used for price and indicator data (e.g., double, float).
   */
  template <class Decimal>
  class IndicatorTimeSeriesCsvWriter
  {
  private:
    std::ofstream mCsvFile;
    const OHLCTimeSeries<Decimal>& mTimeSeries;
    const NumericTimeSeries<Decimal>& mIndicatorSeries;
    std::unique_ptr<IIndicatorTimeSeriesFormatter<Decimal>> mFormatter;

  public:
    /**
     * @brief Constructs an IndicatorTimeSeriesCsvWriter for the specified format.
     *
     * @param fileName The output CSV file name.
     * @param series The OHLC time series data to write.
     * @param indicatorSeries The indicator time series data (e.g., IBS values).
     * @param format The output format to use.
     */
    IndicatorTimeSeriesCsvWriter(const std::string& fileName,
                               const OHLCTimeSeries<Decimal>& series,
                               const NumericTimeSeries<Decimal>& indicatorSeries,
                               OutputFormat format)
        : mCsvFile(fileName), mTimeSeries(series), mIndicatorSeries(indicatorSeries)
    {
        mFormatter = createIndicatorFormatter(format);
    }

    // Copy constructor - Note: ofstream is not copyable
    IndicatorTimeSeriesCsvWriter(const IndicatorTimeSeriesCsvWriter& rhs) = delete;

    // Assignment operator - Note: ofstream is not copyable
    IndicatorTimeSeriesCsvWriter& operator=(const IndicatorTimeSeriesCsvWriter& rhs) = delete;

    ~IndicatorTimeSeriesCsvWriter() = default;

    /**
     * @brief Writes the time series data with indicator values to the CSV file.
     *
     * This method writes the header (if required by the format) and then
     * iterates through all entries in both the OHLC and indicator time series,
     * formatting each according to the specified output format.
     * Perfect date alignment is assumed between OHLC and indicator data.
     */
    void writeFile()
    {
        mFormatter->writeHeader(mCsvFile);
        
        auto ohlcIt = mTimeSeries.beginSortedAccess();
        auto indicatorIt = mIndicatorSeries.beginSortedAccess();
        
        while (ohlcIt != mTimeSeries.endSortedAccess() &&
               indicatorIt != mIndicatorSeries.endSortedAccess())
        {
            const auto& ohlcEntry = *ohlcIt;
            const auto& indicatorEntry = indicatorIt->second;
            
            // Perfect date alignment assumed - dates should match
            mFormatter->writeEntry(mCsvFile, ohlcEntry, indicatorEntry->getValue());
            
            ++ohlcIt;
            ++indicatorIt;
        }
    }

  private:
    /**
     * @brief Factory method to create the appropriate indicator formatter for the given format.
     *
     * @param format The output format enum value.
     * @return A unique pointer to the appropriate indicator formatter implementation.
     * @throws std::invalid_argument if the format is not supported.
     */
    std::unique_ptr<IIndicatorTimeSeriesFormatter<Decimal>> createIndicatorFormatter(OutputFormat format)
    {
        switch (format) {
            case OutputFormat::PAL_INDICATOR_EOD:
                return std::make_unique<PalIndicatorEodFormatter<Decimal>>();
            case OutputFormat::PAL_INDICATOR_INTRADAY:
                return std::make_unique<PalIndicatorIntradayFormatter<Decimal>>();
            default:
                throw std::invalid_argument("Unsupported indicator output format");
        }
    }
  };

  //
  // Legacy Classes for Backward Compatibility
  //
  // These classes maintain the original API by wrapping the new unified
  // TimeSeriesCsvWriter with the appropriate OutputFormat.
  //

  /**
   * @brief Legacy PAL time series CSV writer for backward compatibility.
   *
   * Write a OHLCTimeSeries out in a format that can be read by PriceActionLab.
   * This class maintains the original API by internally using TimeSeriesCsvWriter
   * with OutputFormat::PAL_EOD.
   */
  template <class Decimal>
  class PalTimeSeriesCsvWriter
  {
  private:
    std::unique_ptr<TimeSeriesCsvWriter<Decimal>> mWriter;

  public:
    PalTimeSeriesCsvWriter(const std::string& fileName,
                          const OHLCTimeSeries<Decimal>& series)
        : mWriter(std::make_unique<TimeSeriesCsvWriter<Decimal>>(fileName, series, OutputFormat::PAL_EOD))
    {
    }

    // Copy constructor - Note: TimeSeriesCsvWriter is not copyable due to ofstream
    PalTimeSeriesCsvWriter(const PalTimeSeriesCsvWriter& rhs) = delete;

    // Assignment operator - Note: TimeSeriesCsvWriter is not copyable
    PalTimeSeriesCsvWriter& operator=(const PalTimeSeriesCsvWriter& rhs) = delete;

    ~PalTimeSeriesCsvWriter() = default;

    void writeFile()
    {
        mWriter->writeFile();
    }
  };

  /**
   * @brief Legacy PAL volume for close CSV writer for backward compatibility.
   *
   * This class maintains the original API by internally using TimeSeriesCsvWriter
   * with OutputFormat::PAL_VOLUME_FOR_CLOSE.
   */
  template <class Decimal>
  class PalVolumeForCloseCsvWriter
  {
  private:
    std::unique_ptr<TimeSeriesCsvWriter<Decimal>> mWriter;

  public:
    PalVolumeForCloseCsvWriter(const std::string& fileName,
                              const OHLCTimeSeries<Decimal>& series)
        : mWriter(std::make_unique<TimeSeriesCsvWriter<Decimal>>(fileName, series, OutputFormat::PAL_VOLUME_FOR_CLOSE))
    {
    }

    // Copy constructor - Note: TimeSeriesCsvWriter is not copyable due to ofstream
    PalVolumeForCloseCsvWriter(const PalVolumeForCloseCsvWriter& rhs) = delete;

    // Assignment operator - Note: TimeSeriesCsvWriter is not copyable
    PalVolumeForCloseCsvWriter& operator=(const PalVolumeForCloseCsvWriter& rhs) = delete;

    ~PalVolumeForCloseCsvWriter() = default;

    /**
     * @brief Dump the time series to CSV: Date,Open,High,Low,Volume
     */
    void writeFile()
    {
        mWriter->writeFile();
    }
  };

  //
  // New Format-Specific Writer Classes
  //
  // These classes provide convenient APIs for the new output formats
  // while maintaining consistency with the legacy class naming pattern.
  //

  /**
   * @brief TradeStation EOD format CSV writer.
   *
   * Writes time series data in TradeStation's end-of-day format:
   * "Date","Time","Open","High","Low","Close","Vol","OI"
   * with MM/dd/yyyy date format, 00:00 time, and OI=0.
   */
  template <class Decimal>
  class TradeStationEodCsvWriter
  {
  private:
    std::unique_ptr<TimeSeriesCsvWriter<Decimal>> mWriter;

  public:
    TradeStationEodCsvWriter(const std::string& fileName,
                            const OHLCTimeSeries<Decimal>& series)
        : mWriter(std::make_unique<TimeSeriesCsvWriter<Decimal>>(fileName, series, OutputFormat::TRADESTATION_EOD))
    {
    }

    // Copy constructor - Note: TimeSeriesCsvWriter is not copyable due to ofstream
    TradeStationEodCsvWriter(const TradeStationEodCsvWriter& rhs) = delete;

    // Assignment operator - Note: TimeSeriesCsvWriter is not copyable
    TradeStationEodCsvWriter& operator=(const TradeStationEodCsvWriter& rhs) = delete;

    ~TradeStationEodCsvWriter() = default;

    void writeFile()
    {
        mWriter->writeFile();
    }
  };

  /**
   * @brief TradeStation Intraday format CSV writer.
   *
   * Writes time series data in TradeStation's intraday format:
   * "Date","Time","Open","High","Low","Close","Up","Down"
   * with MM/dd/yyyy date format, HH:MM time format, and Up=Down=0.
   */
  template <class Decimal>
  class TradeStationIntradayCsvWriter
  {
  private:
    std::unique_ptr<TimeSeriesCsvWriter<Decimal>> mWriter;

  public:
    TradeStationIntradayCsvWriter(const std::string& fileName,
                                 const OHLCTimeSeries<Decimal>& series)
        : mWriter(std::make_unique<TimeSeriesCsvWriter<Decimal>>(fileName, series, OutputFormat::TRADESTATION_INTRADAY))
    {
    }

    // Copy constructor - Note: TimeSeriesCsvWriter is not copyable due to ofstream
    TradeStationIntradayCsvWriter(const TradeStationIntradayCsvWriter& rhs) = delete;

    // Assignment operator - Note: TimeSeriesCsvWriter is not copyable
    TradeStationIntradayCsvWriter& operator=(const TradeStationIntradayCsvWriter& rhs) = delete;

    ~TradeStationIntradayCsvWriter() = default;

    void writeFile()
    {
        mWriter->writeFile();
    }
  };

  /**
   * @brief PAL Intraday format CSV writer.
   *
   * Writes time series data in PAL's intraday format:
   * Sequential# Open High Low Close (space-separated, no header)
   * with sequential numbering starting at 10000001.
   */
  template <class Decimal>
  class PalIntradayCsvWriter
  {
  private:
    std::unique_ptr<TimeSeriesCsvWriter<Decimal>> mWriter;

  public:
    PalIntradayCsvWriter(const std::string& fileName,
                        const OHLCTimeSeries<Decimal>& series)
        : mWriter(std::make_unique<TimeSeriesCsvWriter<Decimal>>(fileName, series, OutputFormat::PAL_INTRADAY))
    {
    }

    // Copy constructor - Note: TimeSeriesCsvWriter is not copyable due to ofstream
    PalIntradayCsvWriter(const PalIntradayCsvWriter& rhs) = delete;

    // Assignment operator - Note: TimeSeriesCsvWriter is not copyable
    PalIntradayCsvWriter& operator=(const PalIntradayCsvWriter& rhs) = delete;

    ~PalIntradayCsvWriter() = default;

    void writeFile()
    {
        mWriter->writeFile();
    }
  };

  //
  // New Indicator-Based Writer Classes
  //
  // These classes provide convenient APIs for the new indicator output formats
  // while maintaining consistency with the legacy class naming pattern.
  //

  /**
   * @brief PAL EOD format CSV writer with indicator replacing close.
   *
   * Writes time series data in PAL's end-of-day format with an indicator value
   * (such as IBS) replacing the close price: Date,Open,High,Low,Indicator
   */
  template <class Decimal>
  class PalIndicatorEodCsvWriter
  {
  private:
    std::unique_ptr<IndicatorTimeSeriesCsvWriter<Decimal>> mWriter;

  public:
    PalIndicatorEodCsvWriter(const std::string& fileName,
                           const OHLCTimeSeries<Decimal>& series,
                           const NumericTimeSeries<Decimal>& indicatorSeries)
        : mWriter(std::make_unique<IndicatorTimeSeriesCsvWriter<Decimal>>(fileName, series, indicatorSeries, OutputFormat::PAL_INDICATOR_EOD))
    {
    }

    // Copy constructor - Note: IndicatorTimeSeriesCsvWriter is not copyable due to ofstream
    PalIndicatorEodCsvWriter(const PalIndicatorEodCsvWriter& rhs) = delete;

    // Assignment operator - Note: IndicatorTimeSeriesCsvWriter is not copyable
    PalIndicatorEodCsvWriter& operator=(const PalIndicatorEodCsvWriter& rhs) = delete;

    ~PalIndicatorEodCsvWriter() = default;

    void writeFile()
    {
        mWriter->writeFile();
    }
  };

  /**
   * @brief PAL Intraday format CSV writer with indicator replacing close.
   *
   * Writes time series data in PAL's intraday format with an indicator value
   * (such as IBS) replacing the close price: Sequential# Open High Low Indicator
   * with sequential numbering starting at 10000001.
   */
  template <class Decimal>
  class PalIndicatorIntradayCsvWriter
  {
  private:
    std::unique_ptr<IndicatorTimeSeriesCsvWriter<Decimal>> mWriter;

  public:
    PalIndicatorIntradayCsvWriter(const std::string& fileName,
                                const OHLCTimeSeries<Decimal>& series,
                                const NumericTimeSeries<Decimal>& indicatorSeries)
        : mWriter(std::make_unique<IndicatorTimeSeriesCsvWriter<Decimal>>(fileName, series, indicatorSeries, OutputFormat::PAL_INDICATOR_INTRADAY))
    {
    }

    // Copy constructor - Note: IndicatorTimeSeriesCsvWriter is not copyable due to ofstream
    PalIndicatorIntradayCsvWriter(const PalIndicatorIntradayCsvWriter& rhs) = delete;

    // Assignment operator - Note: IndicatorTimeSeriesCsvWriter is not copyable
    PalIndicatorIntradayCsvWriter& operator=(const PalIndicatorIntradayCsvWriter& rhs) = delete;

    ~PalIndicatorIntradayCsvWriter() = default;

    void writeFile()
    {
        mWriter->writeFile();
    }
  };
}

#endif
