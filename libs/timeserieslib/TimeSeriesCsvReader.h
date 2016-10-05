// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __CSVREADER_H
#define __CSVREADER_H 1

#include "TimeSeries.h"
#include <boost/date_time.hpp>
#include "csv.h"

namespace mkc_timeseries
{
  using namespace dec;

  template <int Prec>
  class TimeSeriesCsvReader
  {
  public:
    TimeSeriesCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
			 TradingVolume::VolumeUnit unitsOfVolume) 
      : mFileName (fileName),
	mTimeSeries(std::make_shared<OHLCTimeSeries<Prec>> (timeFrame,
							unitsOfVolume))
    {}

    TimeSeriesCsvReader(const TimeSeriesCsvReader& rhs)
      : mFileName(rhs.mFileName),
	mTimeSeries(rhs.mTimeSeries)
    {}

    TimeSeriesCsvReader& 
    operator=(const TimeSeriesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      mFileName = rhs.mFileName;
      mTimeSeries = rhs.mTimeSeries;

      return *this;
    }

    virtual ~TimeSeriesCsvReader()
    {}

    const std::string& getFileName() const
    {
      return mFileName;
    }

    const TimeFrame::Duration getTimeFrame() const
    {
      return mTimeSeries->getTimeFrame();
    }

    void addEntry (const std::shared_ptr<OHLCTimeSeriesEntry<Prec>>& entry)
    {
      mTimeSeries->addEntry (entry);
    }

    void addEntry (const OHLCTimeSeriesEntry<Prec>& entry)
    {
      mTimeSeries->addEntry (entry);
    }

    const std::shared_ptr<OHLCTimeSeries<Prec>>& getTimeSeries() const
    {
      return mTimeSeries;
    }

    virtual void readFile() = 0;

  protected:
    bool checkForErrors (boost::gregorian::date entryDate,
			 const decimal<Prec>& openPrice, const decimal<Prec>& highPrice, 
			 const decimal<Prec>& lowPrice, const decimal<Prec>& closePrice)
    {
      bool errorFound = false;

      if (highPrice < openPrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" high of ") +dec::toString (highPrice) +std::string(" is less that open of ") +dec::toString (openPrice) << std::endl;
	}

      if (highPrice < lowPrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" high of ") +dec::toString (highPrice) +std::string(" is less that low of ") +dec::toString (lowPrice) << std::endl;
	}

      if (highPrice < closePrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" high of ") +dec::toString (highPrice) +std::string(" is less that close of ") +dec::toString (closePrice) << std::endl;
	}

      if (lowPrice > openPrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" low of ") +dec::toString (lowPrice) +std::string (" is greater than open of ") +dec::toString (openPrice) << std::endl;
	}
      
      if (lowPrice > closePrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" low of ") +dec::toString (lowPrice) +std::string (" is greater than close of ") +dec::toString (closePrice) << std::endl;
	}

      return errorFound;
    }

  private:
    std::string mFileName;
    std::shared_ptr<OHLCTimeSeries<Prec>> mTimeSeries;
  };

  // Reader for Price Action Lab CSV Formatted Files

  template <int Prec>
  class PALFormatCsvReader : public TimeSeriesCsvReader<Prec>
  {
  public:
    PALFormatCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
			TradingVolume::VolumeUnit unitsOfVolume) :
      TimeSeriesCsvReader<Prec> (fileName, timeFrame, unitsOfVolume),
      mCsvFile (fileName.c_str())
    {}

     PALFormatCsvReader(const PALFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Prec>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    PALFormatCsvReader& 
    operator=(const PALFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Prec>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;

      return *this;
    }

    ~PALFormatCsvReader()
    {}

    void readFile()
    {
      mCsvFile.set_header("Date", "Open", "High", "Low", "Close");

      std::string dateStamp;
      std::string openString, highString, lowString, closeString;

      decimal<Prec> openPrice, highPrice, lowPrice, closePrice;
      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString))
	{
	  openPrice = fromString<decimal<Prec>>(openString.c_str());
	  highPrice = fromString<decimal<Prec>>(highString.c_str());
	  lowPrice = fromString<decimal<Prec>>(lowString.c_str());
	  closePrice = fromString<decimal<Prec>>(closeString.c_str());
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);

	  TimeSeriesCsvReader<Prec>::addEntry (OHLCTimeSeriesEntry<Prec> (entryDate, openPrice, 
								      highPrice, lowPrice, 
								      closePrice, 0, TimeSeriesCsvReader<Prec>::getTimeFrame()));
	}
    }
  private:
    io::CSVReader<5> mCsvFile;
  };


  //
  // class for reading CSI Extended Futures formatted data files
  //
  // The file format is:
  // Date, Open, High, Low, Close, Volume, Open Interest, Rollover date, Unadjusted Close
  //

  template <int Prec>
  class CSIExtendedFuturesCsvReader : public TimeSeriesCsvReader<Prec>
  {
  public:
    CSIExtendedFuturesCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
			TradingVolume::VolumeUnit unitsOfVolume) :
      TimeSeriesCsvReader<Prec> (fileName, timeFrame, unitsOfVolume),
      mCsvFile (fileName.c_str())
    {}

     CSIExtendedFuturesCsvReader(const CSIExtendedFuturesCsvReader& rhs)
       : TimeSeriesCsvReader<Prec>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    CSIExtendedFuturesCsvReader& 
    operator=(const CSIExtendedFuturesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Prec>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;

      return *this;
    }

    ~CSIExtendedFuturesCsvReader()
    {}

    void readFile()
    {
      mCsvFile.set_header("Date", "Open", "High", "Low", "Close", "Vol", "OI", "RollDate", "UnAdjClose");

      std::string dateStamp;
      std::string openString, highString, lowString, closeString, volString, OIString;
      std::string rollDateString, unadjustedCloseString;

      decimal<Prec> openPrice, highPrice, lowPrice, closePrice, unadjustedClosePrice;
      volume_t volume;


      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString, 
			       volString, OIString, rollDateString, unadjustedCloseString))
	{
	  openPrice = fromString<decimal<Prec>>(openString.c_str());
	  highPrice = fromString<decimal<Prec>>(highString.c_str());
	  lowPrice = fromString<decimal<Prec>>(lowString.c_str());
	  closePrice = fromString<decimal<Prec>>(closeString.c_str());
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);
	  volume = std::stol (volString);
	  TimeSeriesCsvReader<Prec>::addEntry (OHLCTimeSeriesEntry<Prec> (entryDate, openPrice, 
									  highPrice, lowPrice, 
									  closePrice, volume, 
									  TimeSeriesCsvReader<Prec>::getTimeFrame()));
	}
    }
  private:
    io::CSVReader<9> mCsvFile;
  };


  //

   //
  // class for reading CSI Extended Futures formatted data files
  //
  // The file format is:
  // Date, Open, High, Low, Close, Volume, Open Interest, Rollover date, Unadjusted Close
  //

  template <int Prec>
  class CSIErrorCheckingExtendedFuturesCsvReader : public TimeSeriesCsvReader<Prec>
  {
  public:
    CSIErrorCheckingExtendedFuturesCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
			TradingVolume::VolumeUnit unitsOfVolume) :
      TimeSeriesCsvReader<Prec> (fileName, timeFrame, unitsOfVolume),
      mCsvFile (fileName.c_str())
    {}

     CSIErrorCheckingExtendedFuturesCsvReader(const CSIErrorCheckingExtendedFuturesCsvReader& rhs)
       : TimeSeriesCsvReader<Prec>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    CSIErrorCheckingExtendedFuturesCsvReader& 
    operator=(const CSIErrorCheckingExtendedFuturesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Prec>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;

      return *this;
    }

    ~CSIErrorCheckingExtendedFuturesCsvReader()
    {}

    void readFile()
    {
      mCsvFile.set_header("Date", "Open", "High", "Low", "Close", "Vol", "OI", "RollDate", "UnAdjClose");

      std::string dateStamp;
      std::string openString, highString, lowString, closeString, volString, OIString;
      std::string rollDateString, unadjustedCloseString;

      decimal<Prec> openPrice, highPrice, lowPrice, closePrice, unadjustedClosePrice;
      volume_t volume, openInterest;

      bool errorResult = false;

      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString, 
			       volString, OIString, rollDateString, unadjustedCloseString))
	{
	  openPrice = fromString<decimal<Prec>>(openString.c_str());
	  highPrice = fromString<decimal<Prec>>(highString.c_str());
	  lowPrice = fromString<decimal<Prec>>(lowString.c_str());
	  closePrice = fromString<decimal<Prec>>(closeString.c_str());
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);
	  volume = std::stol (volString);

	  errorResult = TimeSeriesCsvReader<Prec>::checkForErrors (entryDate, openPrice, 
								   highPrice, lowPrice, 
								   closePrice);
	  if (errorResult == false)
	    TimeSeriesCsvReader<Prec>::addEntry (OHLCTimeSeriesEntry<Prec> (entryDate, openPrice, 
									    highPrice, lowPrice, 
									    closePrice, volume, 
									    TimeSeriesCsvReader<Prec>::getTimeFrame()));
	}
    }
  private:
    io::CSVReader<9> mCsvFile;
  };

  //
  // class for reading TradeStation formatted data files
  //


  template <int Prec>
  class TradeStationFormatCsvReader : public TimeSeriesCsvReader<Prec>
  {
  public:
    TradeStationFormatCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
				 TradingVolume::VolumeUnit unitsOfVolume) :
      TimeSeriesCsvReader<Prec> (fileName, timeFrame, unitsOfVolume),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
    {}

     TradeStationFormatCsvReader(const TradeStationFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Prec>(rhs),
	 mCsvFile(rhs.mCsvFile),
	 mDateParser(rhs.mDateParser)
    {}

    TradeStationFormatCsvReader& 
    operator=(const TradeStationFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Prec>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      mDateParser = rhs.mDateParser;
      return *this;
    }

    ~TradeStationFormatCsvReader()
    {}

    void readFile()
    {
      mCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", 
			   "Close", "Vol", "OI");

      std::string dateStamp, timeString;
      std::string openString, highString, lowString, closeString;
      std::string volumeString, openInterestString;
      
      decimal<Prec> openPrice, highPrice, lowPrice, closePrice;
      decimal<Prec> volume, openInterest;
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
	special_parser;

      while (mCsvFile.read_row(dateStamp, timeString, openString, highString, 
			       lowString, closeString,
			       volumeString, openInterestString))
	{
	  openPrice = fromString<decimal<Prec>>(openString.c_str());
	  highPrice = fromString<decimal<Prec>>(highString.c_str());
	  lowPrice = fromString<decimal<Prec>>(lowString.c_str());
	  closePrice = fromString<decimal<Prec>>(closeString.c_str());
	  entryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);

	  TimeSeriesCsvReader<Prec>::addEntry (OHLCTimeSeriesEntry<Prec> (entryDate, openPrice, 
								      highPrice, lowPrice, 
								      closePrice, 
								      0, 
								      TimeSeriesCsvReader<Prec>::getTimeFrame()));
	}
    }

  private:
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
  };

  ///////

  template <int Prec>
  class TradeStationErrorCheckingFormatCsvReader : public TimeSeriesCsvReader<Prec>
  {
  public:
    TradeStationErrorCheckingFormatCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
					      TradingVolume::VolumeUnit unitsOfVolume) :
      TimeSeriesCsvReader<Prec> (fileName, timeFrame, unitsOfVolume),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
    {}

     TradeStationErrorCheckingFormatCsvReader(const TradeStationErrorCheckingFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Prec>(rhs),
	 mCsvFile(rhs.mCsvFile),
	 mDateParser(rhs.mDateParser)
    {}

    TradeStationErrorCheckingFormatCsvReader& 
    operator=(const TradeStationErrorCheckingFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Prec>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      mDateParser = rhs.mDateParser;
      return *this;
    }

    ~TradeStationErrorCheckingFormatCsvReader()
    {}

    void readFile()
    {
      mCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", 
			   "Close", "Vol", "OI");

      std::string dateStamp, timeString;
      std::string openString, highString, lowString, closeString;
      std::string volumeString, openInterestString;
      
      decimal<Prec> openPrice, highPrice, lowPrice, closePrice;
      decimal<Prec> volume, openInterest;
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
	special_parser;

      bool errorResult = false;

      while (mCsvFile.read_row(dateStamp, timeString, openString, highString, 
			       lowString, closeString,
			       volumeString, openInterestString))
	{
	  openPrice = fromString<decimal<Prec>>(openString.c_str());
	  highPrice = fromString<decimal<Prec>>(highString.c_str());
	  lowPrice = fromString<decimal<Prec>>(lowString.c_str());
	  closePrice = fromString<decimal<Prec>>(closeString.c_str());
	  entryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);

	  errorResult = TimeSeriesCsvReader<Prec>::checkForErrors (entryDate, openPrice, 
								   highPrice, lowPrice, 
								   closePrice);

	  if (errorResult == false)
	    TimeSeriesCsvReader<Prec>::addEntry (OHLCTimeSeriesEntry<Prec> (entryDate, openPrice, 
									    highPrice, lowPrice, 
									    closePrice, 
									    0, 
									    TimeSeriesCsvReader<Prec>::getTimeFrame()));
	}
    }

  private:
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
  };


  ////

///////

  template <int Prec>
  class PinnacleErrorCheckingFormatCsvReader : public TimeSeriesCsvReader<Prec>
  {
  public:
    PinnacleErrorCheckingFormatCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
					      TradingVolume::VolumeUnit unitsOfVolume) :
      TimeSeriesCsvReader<Prec> (fileName, timeFrame, unitsOfVolume),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
    {}

     PinnacleErrorCheckingFormatCsvReader(const PinnacleErrorCheckingFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Prec>(rhs),
	 mCsvFile(rhs.mCsvFile),
	 mDateParser(rhs.mDateParser)
    {}

    PinnacleErrorCheckingFormatCsvReader& 
    operator=(const PinnacleErrorCheckingFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Prec>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      mDateParser = rhs.mDateParser;
      return *this;
    }

    ~PinnacleErrorCheckingFormatCsvReader()
    {}

    void readFile()
    {
      mCsvFile.set_header("Date", "Open", "High", "Low", "Close", "Vol", "OI");

      std::string dateStamp;
      std::string openString, highString, lowString, closeString;
      std::string volumeString, openInterestString;
      
      decimal<Prec> openPrice, highPrice, lowPrice, closePrice;
      decimal<Prec> volume, openInterest;
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
	special_parser;

      bool errorResult = false;

      while (mCsvFile.read_row(dateStamp, openString, highString, 
			       lowString, closeString,
			       volumeString, openInterestString))
	{
	  openPrice = fromString<decimal<Prec>>(openString.c_str());
	  highPrice = fromString<decimal<Prec>>(highString.c_str());
	  lowPrice = fromString<decimal<Prec>>(lowString.c_str());
	  closePrice = fromString<decimal<Prec>>(closeString.c_str());
	  entryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);

	  errorResult = TimeSeriesCsvReader<Prec>::checkForErrors (entryDate, openPrice, 
								   highPrice, lowPrice, 
								   closePrice);

	  if (errorResult == false)
	    TimeSeriesCsvReader<Prec>::addEntry (OHLCTimeSeriesEntry<Prec> (entryDate, openPrice, 
									    highPrice, lowPrice, 
									    closePrice, 
									    0, 
									    TimeSeriesCsvReader<Prec>::getTimeFrame()));
	}
    }

  private:
    io::CSVReader<7, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
  };

}
#endif
