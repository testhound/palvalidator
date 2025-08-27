// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __CSVREADER_H
#define __CSVREADER_H 1

#include <fstream>
#include <stdexcept>
#include "TimeSeries.h"
#include <boost/date_time.hpp>
#include "DecimalConstants.h"
#include "csv.h"

namespace mkc_timeseries
{
  using boost::posix_time::ptime;
  using boost::posix_time::time_duration;
  using boost::posix_time::duration_from_string;

  template <class Decimal>
  class TimeSeriesCsvReader
  {
  public:
    TimeSeriesCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
			 TradingVolume::VolumeUnit unitsOfVolume,
			 const Decimal& minimumTick) 
      : mFileName (fileName),
	mTimeSeries(std::make_shared<OHLCTimeSeries<Decimal>> (timeFrame,
							       unitsOfVolume)),
	mMinimumTick(minimumTick),
	mMinimumTickDiv2(minimumTick / DecimalConstants<Decimal>::DecimalTwo)
    {
      // ensure file exists (all readers inherit this check)
      std::ifstream fin(mFileName);
      if (!fin.is_open())
	throw std::runtime_error("Cannot open file: " + mFileName);
    }

    TimeSeriesCsvReader(const TimeSeriesCsvReader& rhs)
      : mFileName(rhs.mFileName),
	mTimeSeries(rhs.mTimeSeries),
	mMinimumTick(rhs.mMinimumTick),
	mMinimumTickDiv2(rhs.mMinimumTickDiv2)
    {}

    TimeSeriesCsvReader& 
    operator=(const TimeSeriesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      mFileName = rhs.mFileName;
      mTimeSeries = rhs.mTimeSeries;
      mMinimumTick = rhs.mMinimumTick;
      mMinimumTickDiv2 = rhs.mMinimumTickDiv2;
      
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

    const Decimal& getTick() const
    {
      return mMinimumTick;
    }
    
    void addEntry (OHLCTimeSeriesEntry<Decimal>&& entry)
    {
      mTimeSeries->addEntry(std::move(entry));
    }

    std::shared_ptr<OHLCTimeSeries<Decimal>> getTimeSeries() 
    {
      return mTimeSeries;
    }

    virtual void readFile() = 0;

  protected:
    Decimal DecimalRound (const Decimal& price)
    {
      //return price;
      return num::Round2Tick (price, getTick(), mMinimumTickDiv2);
    }
    
    bool checkForErrors (boost::gregorian::date entryDate,
			 const Decimal& openPrice, const Decimal& highPrice, 
			 const Decimal& lowPrice, const Decimal& closePrice)
    {
      bool errorFound = false;

      if (highPrice < openPrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" high of ") +num::toString (highPrice) +std::string(" is less that open of ") +num::toString (openPrice) << std::endl;
	}

      if (highPrice < lowPrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" high of ") +num::toString (highPrice) +std::string(" is less that low of ") +num::toString (lowPrice) << std::endl;
	}

      if (highPrice < closePrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" high of ") +num::toString (highPrice) +std::string(" is less that close of ") +num::toString (closePrice) << std::endl;
	}

      if (lowPrice > openPrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" low of ") +num::toString (lowPrice) +std::string (" is greater than open of ") +num::toString (openPrice) << std::endl;
	}
      
      if (lowPrice > closePrice)
	{
	  errorFound = true;
	  std::cout << std::string ("OHLC Error: on - ") +boost::gregorian::to_simple_string (entryDate) +std::string (" low of ") +num::toString (lowPrice) +std::string (" is greater than close of ") +num::toString (closePrice) << std::endl;
	}

      return errorFound;
    }

  private:
    std::string mFileName;
    std::shared_ptr<OHLCTimeSeries<Decimal>> mTimeSeries;
    Decimal mMinimumTick;
    Decimal mMinimumTickDiv2;
  };

  // Reader for Price Action Lab CSV Formatted Files

  template <class Decimal>
  class PALFormatCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    PALFormatCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame = TimeFrame::DAILY, 
			TradingVolume::VolumeUnit unitsOfVolume = TradingVolume::SHARES,
			const Decimal& minimumTick = DecimalConstants<Decimal>::EquityTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str())
    {}

     PALFormatCsvReader(const PALFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    PALFormatCsvReader& 
    operator=(const PALFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;

      return *this;
    }

    ~PALFormatCsvReader()
    {}

    void readFile()
    {
      // disallow intraday timeframe for PALFormatCsvReader because it is not supported
      if (this->getTimeFrame() == TimeFrame::INTRADAY)
        throw std::runtime_error(
          "PALFormatCsvReader does not support intraday timeframe"
       );
      
      mCsvFile.set_header("Date", "Open", "High", "Low", "Close");

      std::string dateStamp;
      std::string openString, highString, lowString, closeString;

      Decimal openPrice, highPrice, lowPrice, closePrice;
      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString))
	{
	  openPrice = this->DecimalRound (num::fromString<Decimal>(openString.c_str()));
	  highPrice = this->DecimalRound (num::fromString<Decimal>(highString.c_str()));
	  lowPrice = this->DecimalRound (num::fromString<Decimal>(lowString.c_str()));
	  closePrice = this->DecimalRound (num::fromString<Decimal>(closeString.c_str()));
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);

	  TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
										highPrice, lowPrice, 
										closePrice,
										DecimalConstants<Decimal>::DecimalZero,
										TimeSeriesCsvReader<Decimal>::getTimeFrame()));
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

  template <class Decimal>
  class CSIExtendedFuturesCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    CSIExtendedFuturesCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
				 TradingVolume::VolumeUnit unitsOfVolume,
				 const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str())
    {}

     CSIExtendedFuturesCsvReader(const CSIExtendedFuturesCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    CSIExtendedFuturesCsvReader& 
    operator=(const CSIExtendedFuturesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
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

      Decimal openPrice, highPrice, lowPrice, closePrice, unadjustedClosePrice;
      Decimal volume;


      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString, 
			       volString, OIString, rollDateString, unadjustedCloseString))
	{
	  openPrice = this->DecimalRound (num::fromString<Decimal>(openString.c_str()));
	  highPrice = this->DecimalRound (num::fromString<Decimal>(highString.c_str()));
	  lowPrice = this->DecimalRound (num::fromString<Decimal>(lowString.c_str()));
	  closePrice = this->DecimalRound (num::fromString<Decimal>(closeString.c_str()));
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);
	  volume = num::fromString<Decimal> (volString.c_str());
	  TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
										highPrice, lowPrice, 
										closePrice, volume, 
										TimeSeriesCsvReader<Decimal>::getTimeFrame()));
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

  template <class Decimal>
  class CSIErrorCheckingExtendedFuturesCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    CSIErrorCheckingExtendedFuturesCsvReader (const std::string& fileName,
					      TimeFrame::Duration timeFrame, 
					      TradingVolume::VolumeUnit unitsOfVolume,
					      const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str())
    {}

     CSIErrorCheckingExtendedFuturesCsvReader(const CSIErrorCheckingExtendedFuturesCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    CSIErrorCheckingExtendedFuturesCsvReader& 
    operator=(const CSIErrorCheckingExtendedFuturesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
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

      Decimal openPrice, highPrice, lowPrice, closePrice, unadjustedClosePrice;
      Decimal volume, openInterest;

      bool errorResult = false;

      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString, 
			       volString, OIString, rollDateString, unadjustedCloseString))
	{
	  openPrice = this->DecimalRound (num::fromString<Decimal>(openString.c_str()));
	  highPrice = this->DecimalRound (num::fromString<Decimal>(highString.c_str()));
	  lowPrice = this->DecimalRound (num::fromString<Decimal>(lowString.c_str()));
	  closePrice = this->DecimalRound (num::fromString<Decimal>(closeString.c_str()));
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);
	  volume = num::fromString<Decimal>(volString.c_str());

	  errorResult = TimeSeriesCsvReader<Decimal>::checkForErrors (entryDate, openPrice, 
								   highPrice, lowPrice, 
								   closePrice);
	  if (errorResult == false)
	    TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
										  highPrice, lowPrice, 
										  closePrice, volume, 
										  TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	}
    }
  private:
    io::CSVReader<9> mCsvFile;
  };

  //
  // class for reading CSI Extended Futures formatted data files
  //
  // The file format is:
  // Date, Open, High, Low, Close, Volume, Open Interest
  //

  template <class Decimal>
  class CSIFuturesCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    CSIFuturesCsvReader (const std::string& fileName, TimeFrame::Duration timeFrame, 
			 TradingVolume::VolumeUnit unitsOfVolume,
			 const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str())
    {}

    CSIFuturesCsvReader(const CSIFuturesCsvReader& rhs)
      : TimeSeriesCsvReader<Decimal>(rhs),
      mCsvFile(rhs.mCsvFile)
    {}

    CSIFuturesCsvReader& 
    operator=(const CSIFuturesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;

      return *this;
    }

    ~CSIFuturesCsvReader()
    {}

    void readFile()
    {
      mCsvFile.set_header("Date", "Open", "High", "Low", "Close", "Vol", "OI");

      std::string dateStamp;
      std::string openString, highString, lowString, closeString, volString, OIString;
      std::string rollDateString, unadjustedCloseString;

      Decimal openPrice, highPrice, lowPrice, closePrice, unadjustedClosePrice;
      Decimal volume;


      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString, 
			       volString, OIString))
	{
	  openPrice = this->DecimalRound (num::fromString<Decimal>(openString.c_str()));
	  highPrice = this->DecimalRound (num::fromString<Decimal>(highString.c_str()));
	  lowPrice = this->DecimalRound (num::fromString<Decimal>(lowString.c_str()));
	  closePrice = this->DecimalRound (num::fromString<Decimal>(closeString.c_str()));
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);
	  volume = num::fromString<Decimal> (volString.c_str());
	  TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
										highPrice, lowPrice, 
										closePrice, volume, 
										TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	}
    }
  private:
    io::CSVReader<7> mCsvFile;
  };

  //
  //
  // class for reading CSI Futures formatted data files with error checkin
  //
  // The file format is:
  // Date, Open, High, Low, Close, Volume, Open Interest
  //

  template <class Decimal>
  class CSIErrorCheckingFuturesCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    CSIErrorCheckingFuturesCsvReader (const std::string& fileName,
					      TimeFrame::Duration timeFrame, 
					      TradingVolume::VolumeUnit unitsOfVolume,
					      const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str())
    {}

     CSIErrorCheckingFuturesCsvReader(const CSIErrorCheckingFuturesCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    CSIErrorCheckingFuturesCsvReader& 
    operator=(const CSIErrorCheckingFuturesCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;

      return *this;
    }

    ~CSIErrorCheckingFuturesCsvReader()
    {}

    void readFile()
    {
      mCsvFile.set_header("Date", "Open", "High", "Low", "Close", "Vol", "OI");

      std::string dateStamp;
      std::string openString, highString, lowString, closeString, volString, OIString;

      Decimal openPrice, highPrice, lowPrice, closePrice, unadjustedClosePrice;
      Decimal volume, openInterest;

      bool errorResult = false;

      boost::gregorian::date entryDate;
      while (mCsvFile.read_row(dateStamp, openString, highString, lowString, closeString, 
			       volString, OIString))
	{
	  openPrice = this->DecimalRound (num::fromString<Decimal>(openString.c_str()));
	  highPrice = this->DecimalRound (num::fromString<Decimal>(highString.c_str()));
	  lowPrice = this->DecimalRound (num::fromString<Decimal>(lowString.c_str()));
	  closePrice = this->DecimalRound (num::fromString<Decimal>(closeString.c_str()));
	  entryDate = boost::gregorian::from_undelimited_string(dateStamp);
	  volume = num::fromString<Decimal>(volString.c_str());

	  errorResult = TimeSeriesCsvReader<Decimal>::checkForErrors (entryDate, openPrice, 
								   highPrice, lowPrice, 
								   closePrice);
	  if (errorResult == false)
	    TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
										  highPrice, lowPrice, 
										  closePrice, volume, 
										  TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	}
    }
  private:
    io::CSVReader<7> mCsvFile;
  };

  //
  // class for reading TradeStation formatted data files
  //


  template <class Decimal>
  class TradeStationFormatCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    TradeStationFormatCsvReader (const std::string& fileName,
				 TimeFrame::Duration timeFrame, 
				 TradingVolume::VolumeUnit unitsOfVolume,
				 const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
    {}

     TradeStationFormatCsvReader(const TradeStationFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	 mCsvFile(rhs.mCsvFile),
	 mDateParser(rhs.mDateParser)
    {}

    TradeStationFormatCsvReader& 
    operator=(const TradeStationFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      mDateParser = rhs.mDateParser;
      return *this;
    }

    ~TradeStationFormatCsvReader()
    {}

    void readFile()
    {
      if (this->getTimeFrame() != TimeFrame::INTRADAY)
	mCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", 
			     "Close", "Vol", "OI");
      else
	mCsvFile.read_header(io::ignore_extra_column, "Date", "Time", "Open", "High", "Low", 
			     "Close", "Up", "Down");
 

      std::string dateStamp, timeString;
      std::string openString, highString, lowString, closeString;
      std::string volumeString, openInterestString;
      
      Decimal openPrice, highPrice, lowPrice, closePrice;
      Decimal volume, openInterest;
      time_duration barTime(0, 0, 0);
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
	special_parser;

      int lineNo = 1;
      while (mCsvFile.read_row(dateStamp, timeString, openString, highString, 
			       lowString, closeString,
			       volumeString, openInterestString))
	{
	  openPrice = num::fromString<Decimal>(openString.c_str());
	  highPrice = num::fromString<Decimal>(highString.c_str());
	  lowPrice =  num::fromString<Decimal>(lowString.c_str());
	  closePrice = num::fromString<Decimal>(closeString.c_str());
	  volume = num::fromString<Decimal>(volumeString.c_str());
	  entryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);
	  barTime = duration_from_string(timeString);
	  TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (ptime (entryDate, barTime), openPrice, 
								      highPrice, lowPrice, 
								      closePrice, 
								      volume, 
								      TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	  ++lineNo;
	}
      // 2) no data rows at all?
      if (lineNo == 1)
        throw std::runtime_error("No data rows found in file: " + this->getFileName());
    }

  private:
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
  };

  ///////

  template <class Decimal>
  class TradeStationErrorCheckingFormatCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    TradeStationErrorCheckingFormatCsvReader (const std::string& fileName,
					      TimeFrame::Duration timeFrame, 
					      TradingVolume::VolumeUnit unitsOfVolume,
					      const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
    {}

     TradeStationErrorCheckingFormatCsvReader(const TradeStationErrorCheckingFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	 mCsvFile(rhs.mCsvFile),
	 mDateParser(rhs.mDateParser)
    {}

    TradeStationErrorCheckingFormatCsvReader& 
    operator=(const TradeStationErrorCheckingFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
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
      
      Decimal openPrice, highPrice, lowPrice, closePrice;
      Decimal volume, openInterest;
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
	special_parser;

      bool errorResult = false;

      while (mCsvFile.read_row(dateStamp, timeString, openString, highString, 
			       lowString, closeString,
			       volumeString, openInterestString))
	{
	  openPrice = num::fromString<Decimal>(openString.c_str());
	  highPrice = num::fromString<Decimal>(highString.c_str());
	  lowPrice = num::fromString<Decimal>(lowString.c_str());
	  closePrice = num::fromString<Decimal>(closeString.c_str());
	  volume = num::fromString<Decimal>(volumeString.c_str());
	  entryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);

	  errorResult = TimeSeriesCsvReader<Decimal>::checkForErrors (entryDate, openPrice, 
								   highPrice, lowPrice, 
								   closePrice);

	  if (errorResult == false)
	    TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
									    highPrice, lowPrice, 
									    closePrice, 
									    volume, 
									    TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	}
    }

  private:
    io::CSVReader<8, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
  };


  ////
  /// Class to read TradeStation data file with extra custom indicator column

  //
  // class for reading TradeStation formatted data files
  //


  template <class Decimal>
  class TradeStationIndicator1CsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    TradeStationIndicator1CsvReader (const std::string& fileName,
				     TimeFrame::Duration timeFrame, 
				     TradingVolume::VolumeUnit unitsOfVolume,
				     const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
    {}

     TradeStationIndicator1CsvReader(const TradeStationIndicator1CsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	 mCsvFile(rhs.mCsvFile),
	 mDateParser(rhs.mDateParser)
    {}

    TradeStationIndicator1CsvReader& 
    operator=(const TradeStationIndicator1CsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      mDateParser = rhs.mDateParser;
      return *this;
    }

    ~TradeStationIndicator1CsvReader()
    {}

    void readFile()
    {
      int lineNum = 1;
      
      mCsvFile.set_header("Date", "Time", "Open", "High", "Low", 
			   "Close", "Vol", "OI", "Indicator1");

      std::string dateStamp, timeString;
      std::string openString, highString, lowString, closeString;
      std::string volumeString, openInterestString, indicator1String;
      
      Decimal openPrice, highPrice, lowPrice, closePrice;
      Decimal indicator1;
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
	special_parser;

      std::cout << "TradeStationIndicator1CsvReader.readFile" << std::endl << std::endl;
      
      while (mCsvFile.read_row(dateStamp, timeString, openString, highString, 
			       lowString, closeString,
			       volumeString, openInterestString, indicator1String))
	{
	  std::cout << "line num = " << lineNum << std::endl;
	  
	  openPrice = num::fromString<Decimal>(openString.c_str());
	  highPrice = num::fromString<Decimal>(highString.c_str());
	  lowPrice =  num::fromString<Decimal>(lowString.c_str());
	  closePrice = num::fromString<Decimal>(closeString.c_str());
	  indicator1 = num::fromString<Decimal>(indicator1String.c_str());
	  entryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);

	  std::cout << entryDate << ", " << openPrice << ", " << highPrice << ", " << lowPrice << ", " << closePrice << ", " << indicator1 << std::endl;
	  TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
										highPrice, lowPrice, 
										closePrice, 
										indicator1, 
										TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	  lineNum++;
	}
    }

  private:
    io::CSVReader<9, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
  };


  // --- Wealth-Lab CSV reader ----------------------------------------------------
  //
  // Format (header + rows):
  // Date/Time,Open,High,Low,Close,Volume
  // 5/30/2000,0.22578125,0.23463542,0.22473957,0.22890625,306210240
  //
  // Notes:
  // - Date is US-style M/D/YYYY or MM/DD/YYYY (no zero-padding required).
  // - Time is optional in the "Date/Time" column. For non-intraday timeframes,
  //   the reader uses the date-only constructor (00:00) like other readers.
  // - For intraday (if enabled by caller) and when a time is present, we parse it.

  template <class Decimal>
  class WealthLabCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    WealthLabCsvReader(const std::string& fileName,
		       TimeFrame::Duration timeFrame = TimeFrame::DAILY,
		       TradingVolume::VolumeUnit unitsOfVolume = TradingVolume::SHARES,
		       const Decimal& minimumTick = DecimalConstants<Decimal>::EquityTick)
      : TimeSeriesCsvReader<Decimal>(fileName, timeFrame, unitsOfVolume, minimumTick),
	mCsvFile(fileName.c_str())
    {}

    WealthLabCsvReader(const WealthLabCsvReader& rhs)
      : TimeSeriesCsvReader<Decimal>(rhs),
	mCsvFile(rhs.mCsvFile)
    {}

    WealthLabCsvReader& operator=(const WealthLabCsvReader& rhs)
    {
      if (this == &rhs) return *this;
      TimeSeriesCsvReader<Decimal>::operator=(rhs);
      mCsvFile = rhs.mCsvFile;
      return *this;
    }

    ~WealthLabCsvReader() {}

    void readFile()
    {
      // Wealth-Lab files include a header row
      mCsvFile.read_header(io::ignore_extra_column,
			   "Date/Time", "Open", "High", "Low", "Close", "Volume");

      std::string dateTimeField;
      std::string openStr, highStr, lowStr, closeStr, volStr;

      while (mCsvFile.read_row(dateTimeField, openStr, highStr, lowStr, closeStr, volStr))
	{
	  // Prices rounded to tick using the base helper
	  const Decimal open  = this->DecimalRound(num::fromString<Decimal>(openStr.c_str()));
	  const Decimal high  = this->DecimalRound(num::fromString<Decimal>(highStr.c_str()));
	  const Decimal low   = this->DecimalRound(num::fromString<Decimal>(lowStr.c_str()));
	  const Decimal close = this->DecimalRound(num::fromString<Decimal>(closeStr.c_str()));
	  const Decimal vol   = num::fromString<Decimal>(volStr.c_str());

	  // Split "Date/Time" into date and (optional) time
	  std::string datePart = dateTimeField;
	  std::string timePart;
	  if (auto sp = dateTimeField.find(' '); sp != std::string::npos) {
	    datePart = dateTimeField.substr(0, sp);
	    timePart = dateTimeField.substr(sp + 1);
	  }

	  // Parse US-style date (accepts M/D/YYYY and MM/DD/YYYY).
	  const boost::gregorian::date d = boost::gregorian::from_us_string(datePart);

	  if (this->getTimeFrame() == TimeFrame::INTRADAY && !timePart.empty()) {
	    // Parse HH:MM or HH:MM:SS for intraday rows
	    const boost::posix_time::time_duration td =
	      boost::posix_time::duration_from_string(timePart);
	    TimeSeriesCsvReader<Decimal>::addEntry(
						   OHLCTimeSeriesEntry<Decimal>(boost::posix_time::ptime(d, td),
										open, high, low, close, vol,
										TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	  } else {
	    // Non-intraday (daily/weekly/etc.) or no time present
	    TimeSeriesCsvReader<Decimal>::addEntry(
						   OHLCTimeSeriesEntry<Decimal>(d,
										open, high, low, close, vol,
										TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	  }
	}
    }

  private:
    // Match other readers’ CSV policies (trim spaces; allow quoted fields).
    // Examples of the same pattern appear in your other readers. :contentReference[oaicite:2]{index=2}
    io::CSVReader<6, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
  };  
  ////
///////

  template <class Decimal>
  class PinnacleErrorCheckingFormatCsvReader : public TimeSeriesCsvReader<Decimal>
  {
  public:
    PinnacleErrorCheckingFormatCsvReader (const std::string& fileName,
					  TimeFrame::Duration timeFrame, 
					  TradingVolume::VolumeUnit unitsOfVolume,
					  const Decimal& minimumTick) :
      TimeSeriesCsvReader<Decimal> (fileName, timeFrame, unitsOfVolume, minimumTick),
      mCsvFile (fileName.c_str()),
      mDateParser(std::string("%m/%d/%YYYY"), std::locale("C"))
    {}

     PinnacleErrorCheckingFormatCsvReader(const PinnacleErrorCheckingFormatCsvReader& rhs)
       : TimeSeriesCsvReader<Decimal>(rhs),
	 mCsvFile(rhs.mCsvFile),
	 mDateParser(rhs.mDateParser)
    {}

    PinnacleErrorCheckingFormatCsvReader& 
    operator=(const PinnacleErrorCheckingFormatCsvReader &rhs)
    {
      if (this == &rhs)
	return *this;

      TimeSeriesCsvReader<Decimal>::operator=(rhs);
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
      
      Decimal openPrice, highPrice, lowPrice, closePrice;
      Decimal volume, openInterest;
      boost::gregorian::date entryDate;

      std::string dateFormat("%m/%d/%YYYY");
      boost::date_time::special_values_parser<boost::gregorian::date, char>
	special_parser;

      bool errorResult = false;

      while (mCsvFile.read_row(dateStamp, openString, highString, 
			       lowString, closeString,
			       volumeString, openInterestString))
	{
	  openPrice = num::fromString<Decimal>(openString.c_str());
	  highPrice = num::fromString<Decimal>(highString.c_str());
	  lowPrice = num::fromString<Decimal>(lowString.c_str());
	  closePrice = num::fromString<Decimal>(closeString.c_str());
	  entryDate = mDateParser.parse_date (dateStamp, dateFormat, special_parser);
	  volume = num::fromString<Decimal>(volumeString.c_str());
	  errorResult = TimeSeriesCsvReader<Decimal>::checkForErrors (entryDate, openPrice, 
								   highPrice, lowPrice, 
								   closePrice);

	  if (errorResult == false)
	    TimeSeriesCsvReader<Decimal>::addEntry (OHLCTimeSeriesEntry<Decimal> (entryDate, openPrice, 
									    highPrice, lowPrice, 
									    closePrice, 
									    volume, 
									    TimeSeriesCsvReader<Decimal>::getTimeFrame()));
	}
    }

  private:
    io::CSVReader<7, io::trim_chars<' '>, io::double_quote_escape<',','\"'>> mCsvFile;
    boost::date_time::format_date_parser<boost::gregorian::date, char> mDateParser;
  };

}
#endif
