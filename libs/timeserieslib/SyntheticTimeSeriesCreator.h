#ifndef SYNTHETICTIMESERIESCREATOR_H
#define SYNTHETICTIMESERIESCREATOR_H

#include "TimeSeriesCsvWriter.h"
#include "TimeSeries.h"
#include "McptConfigurationFileReader.h"
#include "TimeSeriesCsvReader.h"


namespace mkc_timeseries
{
    template <class Decimal>
    class SyntheticTimeSeriesCreator : public TimeSeriesCsvReader<Decimal>
    {
        public:
            SyntheticTimeSeriesCreator(const std::string& fileName, 
                            TimeFrame::Duration timeFrame,
                            TradingVolume::VolumeUnit unitsOfVolume,
                            const Decimal& minimumTick) : 
                TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
                mTimeFrame(timeFrame),
                mUnitsOfVolume(unitsOfVolume),
                mMinimumTick(minimumTick),
                mFilename(fileName),
                mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
            {}

            void readFile() {}

            virtual void createSyntheticTimeSeries(int timeFrameId, time_t filterTime) = 0;
            
            std::shared_ptr<OHLCTimeSeries<Decimal>> getSyntheticTimeSeries(int timeFrameId)
            {
                std::string timeFrameFilename = getTimeFrameFilename(timeFrameId);
                std::shared_ptr<PALFormatCsvReader<Decimal>> timeFrameFileReader = std::make_shared<PALFormatCsvReader<Decimal>>(
                    timeFrameFilename, mTimeFrame, mUnitsOfVolume, mMinimumTick
                );
                timeFrameFileReader->readFile();
                return timeFrameFileReader->getTimeSeries();
            }

            void rewriteTimeFrameFile(int timeFrameId, std::shared_ptr<OHLCTimeSeries<Decimal>> series) 
            {
                std::string timeFrameFilename = this->getTimeFrameFilename(timeFrameId);
                PalTimeSeriesCsvWriter<Decimal> csvWriter(timeFrameFilename, *series);
                csvWriter.writeFile();
            }

        protected:
            TimeFrame::Duration mTimeFrame;
            TradingVolume::VolumeUnit mUnitsOfVolume;
            const Decimal& mMinimumTick;
            std::string mFilename;
            boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;

            std::string getTimeFrameFilename(int timeFrameId) 
            {
                return std::string(mFilename + std::string("_timeframe_") + std::to_string(timeFrameId));
            }

    };  

    template <class Decimal>
    class TradestationHourlySyntheticTimeSeriesCreator : public SyntheticTimeSeriesCreator<Decimal>
    {
        public:
            TradestationHourlySyntheticTimeSeriesCreator(
                    const std::string& fileName, TimeFrame::Duration timeFrame,
                    TradingVolume::VolumeUnit unitsOfVolume, const Decimal& minimumTick) :
                    SyntheticTimeSeriesCreator<Decimal>(fileName, timeFrame, unitsOfVolume, minimumTick)
            {}

            void createSyntheticTimeSeries(int timeFrameId, time_t filterTime) 
            {
                io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> csvFile(this->mFilename.c_str());
                csvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", "Close", "Up", "Down");
                std::string dateStamp, timeStamp;
                std::string openString, highString, lowString, closeString;
                std::string up, down;
                std::string dateFormat = "%m/%d/%YYYY";
                
                Decimal openPrice, highPrice, lowPrice, closePrice, upVol;
                boost::date_time::special_values_parser<boost::gregorian::date, char> special_parser;

                std::shared_ptr<OHLCTimeSeries<Decimal>> series(std::make_shared<OHLCTimeSeries<Decimal>>(this->mTimeFrame, this->mUnitsOfVolume));
                while (csvFile.read_row(dateStamp, timeStamp, openString, highString,
                                        lowString, closeString, up, down))
                {
                    openPrice = num::fromString<Decimal>(openString.c_str());
                    highPrice = num::fromString<Decimal>(highString.c_str());
                    lowPrice =  num::fromString<Decimal>(lowString.c_str());
                    closePrice = num::fromString<Decimal>(closeString.c_str());
                    upVol = num::fromString<Decimal>(up.c_str());

                    struct tm tm = {0, 0, 0, 0, 0, 0, 0, 0, 0};
                    strptime(timeStamp.c_str(), "%H:%M", &tm);
                    time_t timeStamp = mktime(&tm);

                    if(timeStamp == filterTime) 
                    {
                        if(mOpen != DecimalConstants<Decimal>::DecimalZero)
                        series->addEntry(OHLCTimeSeriesEntry<Decimal>(
                                    mEntryDate, mOpen, mHigh, mLow, 
                                    mClose, upVol, TimeSeriesCsvReader<Decimal>::getTimeFrame()
                        ));

                        mOpen = openPrice;
                        mHigh = DecimalConstants<Decimal>::DecimalZero;
                        mLow = highPrice * DecimalConstants<Decimal>::DecimalOneHundred;
                        mEntryDate = this->mDateParser.parse_date(dateStamp, dateFormat, special_parser);
                    }
                    if (highPrice > mHigh)
                        mHigh = highPrice;

                    if (lowPrice < mLow)
                        mLow = lowPrice;
                    mClose = closePrice;
                }

                std::string timeFrameFilename = this->getTimeFrameFilename(timeFrameId);
                PalTimeSeriesCsvWriter<Decimal> csvWriter(timeFrameFilename, *series);
                csvWriter.writeFile();
            }
        private:
            Decimal mOpen = DecimalConstants<Decimal>::DecimalZero;
            Decimal mHigh = DecimalConstants<Decimal>::DecimalZero;
            Decimal mLow = DecimalConstants<Decimal>::DecimalZero;
            Decimal mClose = DecimalConstants<Decimal>::DecimalZero;
            boost::gregorian::date mEntryDate;
    };
}

#endif