#include "PalSetupTypes.h"
#include "TimeFrameUtility.h"
#include <algorithm>

void CleanStartConfig::adjustForTimeFrame(mkc_timeseries::TimeFrame::Duration timeFrame, size_t totalBars, int intradayMinutes) {
    switch (timeFrame) {
        case mkc_timeseries::TimeFrame::DAILY:
            windowBars_ = 252;
            stabilityBufferBars_ = 20;
            break;
            
        case mkc_timeseries::TimeFrame::WEEKLY:
            windowBars_ = 260;
            stabilityBufferBars_ = 4;
            break;
            
        case mkc_timeseries::TimeFrame::MONTHLY:
            windowBars_ = 60;
            stabilityBufferBars_ = 3;
            break;
            
        case mkc_timeseries::TimeFrame::INTRADAY:
        default: {
            // Compute bars per day based on intraday minutes
            int barsPerDay = (intradayMinutes > 0) ? 
                std::max(1, static_cast<int>(std::round(390.0 / intradayMinutes))) : 390;
            
            int desiredDays = 20;
            // Gracefully shrink window for shorter intraday series
            while (barsPerDay * desiredDays >= static_cast<int>(totalBars) && desiredDays > 2) {
                desiredDays /= 2;
            }
            
            windowBars_ = std::max(3, barsPerDay * desiredDays);
            stabilityBufferBars_ = std::max(60, barsPerDay * 10);
            intradayMinutesPerBar_ = intradayMinutes;
            break;
        }
    }
}

bool SetupConfiguration::validatePercentages() const {
    double total = insamplePercent_ + outOfSamplePercent_ + reservedPercent_;
    return total <= 100.0 && insamplePercent_ >= 0.0 && outOfSamplePercent_ >= 0.0 && reservedPercent_ >= 0.0;
}