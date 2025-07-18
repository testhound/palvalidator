// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, December 2024
//

#ifndef __OUTPUT_FORMAT_H
#define __OUTPUT_FORMAT_H 1

namespace mkc_timeseries
{
    /**
     * @brief Enumeration of supported CSV output formats for time series data.
     * 
     * This enum defines the various output formats that can be used when writing
     * OHLC time series data to CSV files. Each format has specific requirements
     * for headers, field ordering, date formatting, and special fields.
     */
    enum class OutputFormat {
        PAL_EOD,                    ///< Original PalTimeSeriesCsvWriter format: Date,Open,High,Low,Close
        PAL_VOLUME_FOR_CLOSE,       ///< Original PalVolumeForCloseCsvWriter format: Date,Open,High,Low,Volume
        TRADESTATION_EOD,           ///< TradeStation daily format: "Date","Time","Open","High","Low","Close","Vol","OI"
        TRADESTATION_INTRADAY,      ///< TradeStation intraday format: "Date","Time","Open","High","Low","Close","Up","Down"
        PAL_INTRADAY,               ///< PAL intraday format with sequential numbers: 10000001 Open High Low Close
        PAL_INDICATOR_EOD,          ///< PAL format with indicator replacing close: Date,Open,High,Low,Indicator
        PAL_INDICATOR_INTRADAY      ///< PAL intraday format with indicator: Sequential# Open High Low Indicator
    };
}

#endif // __OUTPUT_FORMAT_H