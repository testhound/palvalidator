// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef TIMEFILTEREDCSVREADER_H
#define TIMEFILTEREDCSVREADER_H

#include "TimeSeriesCsvReader.h"

using namespace mkc_timeseries;

namespace mkc_searchalgo
{

  static time_t getTimeFromString(const std::string& timeStamp)
  {
    struct std::tm tm{0,0,0,0,0,2000,0,0,-1};
    try
    {
      strptime(timeStamp.c_str(), "%H:%M", &tm);
      return std::mktime(&tm);
    }
    catch (const std::exception& e)
    {
        std::cout << "Time conversion exception." << std::endl;
        throw TimeSeriesEntryException("Time conversion exception when converting: " + timeStamp + "\nException details: " + std::string(e.what()));
    }
  }


  template <class Decimal>
  class TimeFilteredCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    TimeFilteredCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame,
                        TradingVolume::VolumeUnit unitsOfVolume,
                        const Decimal& minimumTick, const time_t& timeFilter) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str()),
      mTimeFilter(timeFilter),
      mOpen(DecimalConstants<Decimal>::DecimalZero),
      mHigh(DecimalConstants<Decimal>::DecimalZero),
      mLow(DecimalConstants<Decimal>::DecimalZero),
      mClose(DecimalConstants<Decimal>::DecimalZero)
    {}

     TimeFilteredCsvReader(const TimeFilteredCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
        mCsvFile(rhs.mCsvFile),
        mTimeFilter(rhs.mTimeFilter)
    {}

    TimeFilteredCsvReader&
    operator=(const TimeFilteredCsvReader &rhs)
    {
      if (this == &rhs)
        return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      mTimeFilter = rhs.mTimeFilter;

      return *this;
    }

    ~TimeFilteredCsvReader()
    {}

    void readFile()
    {
      mCsvFile.set_header("Date", "Time", "Open", "High", "Low", "Close");

      std::string dateStamp, timeStamp;
      std::string openString, highString, lowString, closeString;

      Decimal openPrice, highPrice, lowPrice, closePrice;
      //boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, timeStamp, openString, highString, lowString, closeString))
        {
          openPrice = this->DecimalRound (num::fromString<Decimal>(openString.c_str()));
          highPrice = this->DecimalRound (num::fromString<Decimal>(highString.c_str()));
          lowPrice = this->DecimalRound (num::fromString<Decimal>(lowString.c_str()));
          closePrice = this->DecimalRound (num::fromString<Decimal>(closeString.c_str()));


          time_t tstamp = getTimeFromString(timeStamp);
          std::cout << std::asctime(std::localtime(&mTimeFilter)) << std::endl;
          std::cout << std::asctime(std::localtime(&tstamp)) << std::endl;
          // create entry
          if (tstamp == mTimeFilter)
            {
              if (mOpen == DecimalConstants<Decimal>::DecimalZero)
                {
                  TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (mEntryDate, mOpen,
                                                                                        mHigh, mLow,
                                                                                        mClose,
                                                                                        DecimalConstants<Decimal>::DecimalZero,
                                                                                        TimeSeriesCsvReader<Decimal>::getTimeFrame()));
                }
              // to reset
              mOpen = openPrice;
              mHigh = DecimalConstants<Decimal>::DecimalZero;
              mLow = highPrice * DecimalConstants<Decimal>::DecimalOneHundred;
              mEntryDate = boost::gregorian::from_undelimited_string(dateStamp);

            }
            if (highPrice > mHigh)
              mHigh = highPrice;

            if (lowPrice < mLow)
              mLow = lowPrice;
            mClose = closePrice;
        }
    }

    const std::tm& getTimeFilter() const { return mTimeFilter; }

  private:
    io::CSVReader<6> mCsvFile;
    time_t mTimeFilter;
    Decimal mOpen;
    Decimal mHigh;
    Decimal mLow;
    Decimal mClose;
    boost::gregorian::date mEntryDate;
  };


  template <class Decimal>
  class TradeStationTimeFilteredCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    TradeStationTimeFilteredCsvReader (const std::string& fileName,
                                 TimeFrame::Duration timeFrame,
                                 TradingVolume::VolumeUnit unitsOfVolume,
                                 const Decimal& minimumTick, const time_t& timeFilter) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C")),
      mTimeFilter(timeFilter),
      mOpen(DecimalConstants<Decimal>::DecimalZero),
      mHigh(DecimalConstants<Decimal>::DecimalZero),
      mLow(DecimalConstants<Decimal>::DecimalZero),
      mClose(DecimalConstants<Decimal>::DecimalZero)
    {}

     TradeStationTimeFilteredCsvReader(const TradeStationTimeFilteredCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
         mCsvFile(rhs.mCsvFile),
         mDateParser(rhs.mDateParser)
    {}

    TradeStationTimeFilteredCsvReader&
    operator=(const TradeStationTimeFilteredCsvReader &rhs)
    {
      if (this == &rhs)
        return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      mDateParser = rhs.mDateParser;
      return *this;
    }

    ~TradeStationTimeFilteredCsvReader()
    {}

    void readFile()
    {
      mCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low",
                           "Close", "Up", "Down");

      std::string dateStamp, timeStamp;
      std::string openString, highString, lowString, closeString;
      std::string volumeString, openInterestString;

      Decimal openPrice, highPrice, lowPrice, closePrice;
      Decimal volume, openInterest;
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
        special_parser;

      while (mCsvFile.read_row(dateStamp, timeStamp, openString, highString,
                               lowString, closeString,
                               volumeString, openInterestString))
        {
          openPrice = num::fromString<Decimal>(openString.c_str());
          highPrice = num::fromString<Decimal>(highString.c_str());
          lowPrice =  num::fromString<Decimal>(lowString.c_str());
          closePrice = num::fromString<Decimal>(closeString.c_str());
          volume = num::fromString<Decimal>(volumeString.c_str());

          time_t tstamp = getTimeFromString(timeStamp);

          // create entry
          if (tstamp == mTimeFilter)
            {
              if (mOpen != DecimalConstants<Decimal>::DecimalZero)
                {
                    std::cout << "new Entry: " << mEntryDate << ", time: "<< std::asctime(std::localtime(&mTimeFilter)) <<
                                 "current: " << openPrice << "," << highPrice << "," << lowPrice << "," << closePrice <<
                                 ", Entry: " << mOpen << "," << mHigh << "," << mLow << "," << closePrice << std::endl;
                    TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (mEntryDate, mOpen,
                                                                                          mHigh, mLow,
                                                                                          mClose,
                                                                                          volume,
                                                                                          TimeSeriesCsvReader<Decimal>::getTimeFrame()));
                }
              // init
              mOpen = openPrice;
              mHigh = DecimalConstants<Decimal>::DecimalZero;
              mLow = highPrice * DecimalConstants<Decimal>::DecimalOneHundred;
              mEntryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);
            }
            if (highPrice > mHigh)
              mHigh = highPrice;

            if (lowPrice < mLow)
              mLow = lowPrice;
            mClose = closePrice;

        }
    }

  private:
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
    time_t mTimeFilter;
    Decimal mOpen;
    Decimal mHigh;
    Decimal mLow;
    Decimal mClose;
    boost::gregorian::date mEntryDate;

  };



}

#endif // TIMEFILTEREDCSVREADER_H
