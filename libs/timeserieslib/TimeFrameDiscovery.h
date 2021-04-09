#ifndef TIMEFRAMEDISCOVERY_H
#define TIMEFRAMEDISCOVERY_H

#include "TimeSeriesCsvReader.h"

namespace mkc_timeseries
{
    template <class Decimal>
    class TimeFrameDiscovery
    {
        public:
            typedef typename std::vector<time_t>::const_iterator TimeFrameIterator;

            TimeFrameDiscovery(const std::string& fileName) : mCsvFile (fileName.c_str())
            {}

            virtual void inferTimeFrames() = 0;

            time_t getTimeFrameInMinutes(int position) 
            { 
                if(position >= mTimeFrames.size()) 
                    throw McptConfigurationFileReaderException("Timeframe does not exists: id=" + std::to_string(position) + " number of time frames=" + std::to_string(mTimeFrames.size()));
                return mTimeFrames.at(position); 
            };

            int numTimeFrames() const { return mTimeFrames.size(); };
            TimeFrameIterator getTimeFramesBegin() const { return mTimeFrames.begin(); };
            TimeFrameIterator getTimeFramesEnd() const { return mTimeFrames.end(); };
            std::vector<time_t> getTimeFrames() const { return mTimeFrames; }
        protected:
            std::vector<time_t> mTimeFrames;
            io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    };

    template <class Decimal>
    class TradestationHourlyTimeFrameDiscovery : public TimeFrameDiscovery<Decimal>
    {
        public:
            TradestationHourlyTimeFrameDiscovery(const std::string& filename) : TimeFrameDiscovery<Decimal>(filename){}

            void inferTimeFrames() 
            {
                this->mCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", "Close", "Up", "Down");
                std::string dateStamp, timeStamp;
                std::string openString, highString, lowString, closeString;
                std::string up, down;

                while (this->mCsvFile.read_row(dateStamp, timeStamp, openString, highString,
                                        lowString, closeString, up, down))
                {
                    struct tm tm = {0, 0, 0, 0, 0, 0, 0, 0, 0};
                    strptime(timeStamp.c_str(), "%H:%M", &tm);
                    time_t time = mktime(&tm);

                    if(std::find(this->mTimeFrames.begin(), this->mTimeFrames.end(), time) == this->mTimeFrames.end()) 
                        this->mTimeFrames.push_back(time);
                }
        }
    };
}

#endif