#ifndef HISTORIC_DATA_READER_H
#define HISTORIC_DATA_READER_H 1

#include <exception>
#include <boost/algorithm/string.hpp>
#include "DataSourceReader.h"
#include "TimeSeriesCsvReader.h"
#include "SecurityAttributesFactory.h"
#include "DateRange.h"
#include "TimeFrame.h"

namespace mkc_timeseries
{
  class HistoricDataReaderException : public std::runtime_error
  {
  public:
  HistoricDataReaderException(const std::string msg)
    : std::runtime_error(msg)
      {}

    ~HistoricDataReaderException()
      {}
  };

  template <class Decimal>
  class HistoricDataReader
  {
  public:
    enum HistoricDataFileFormat {TRADESTATION, TRADESTATION_INDICATOR1, PAL, CSI, CSI_EXTENDED};
    enum HistoricDataApi {BARCHART, FINNHUB};

    HistoricDataReader()
    {}

    virtual ~HistoricDataReader()
    {}

    virtual void read() = 0;
    virtual const std::shared_ptr<OHLCTimeSeries<Decimal>>& getTimeSeries() = 0;
    bool virtual isHistoricFileReader() const = 0;
    bool virtual isApiFileReader() const = 0;

  };

  template <class Decimal>
  class HistoricDataFileReader : public HistoricDataReader<Decimal>
  {
  public:
    HistoricDataFileReader(std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader) 
      : HistoricDataReader<Decimal>(),
      mTimeSeriesCsvReader(reader),
      mDataRead(false)
      {}
      
    ~HistoricDataFileReader()
    {}

    bool virtual isHistoricFileReader() const
    {
      return true;
    }

    bool isApiFileReader() const
    {
      return false;
    }

    void read()
    {
      if (!mDataRead)
	{
	  mTimeSeriesCsvReader->readFile();
	  mDataRead = true;
	}
    }

    const std::shared_ptr<OHLCTimeSeries<Decimal>>& getTimeSeries()
    {
      this->read();
      return mTimeSeriesCsvReader->getTimeSeries();
    }
      
  private:
    std::shared_ptr<TimeSeriesCsvReader<Decimal>> mTimeSeriesCsvReader;
    bool mDataRead;
  };

  template <class Decimal>
  class HistoricDataApiReader : public HistoricDataReader<Decimal>
  {
  public:
    HistoricDataApiReader(std::string tickerSymbol,
			  std::shared_ptr<DataSourceReader> reader,
			  const DateRange& dateRangeToCollect,
			  TimeFrame::Duration timeFrame) 
      : HistoricDataReader<Decimal>(),
      mTickerSymbol(tickerSymbol),
      mDataSourceReader(reader),
      mDateRangeToCollect(dateRangeToCollect),
      mTimeFrame(timeFrame),
      mDataRead(false)
      {}
      
    ~HistoricDataApiReader()
    {}

    bool virtual isHistoricFileReader() const
    {
      return false;
    }

    bool isApiFileReader() const
    {
      return true;
    }

    void read()
    {
      if (!mDataRead)
	{
	  //NOTE: This needs to be fixed to allow for other time frames

	  std::string timeFrameStr = (mTimeFrame == TimeFrame::DAILY) ? "daily" : "hourly";
	  std::string tempFile (mDataSourceReader->createTemporaryFile(mTickerSymbol, timeFrameStr, mDateRangeToCollect, true));
	  std::shared_ptr<SecurityAttributes<Decimal>> attributes = getSecurityAttributes<Decimal> (mTickerSymbol);

	  // NOTE: This should change in the future as it would be much better for the api reader to simply return
	  // a time series instead of writing to a temporary file
	  auto csvReader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(tempFile, mTimeFrame, 
										  attributes->getVolumeUnits(),
										  attributes->getTick());
	  mHistoricDataReader = std::make_shared<HistoricDataFileReader<Decimal>> (csvReader);
	  mHistoricDataReader->read();
	  mDataRead = true;
	}
    }

    const std::shared_ptr<OHLCTimeSeries<Decimal>>& getTimeSeries()
    {
      this->read();
      return mHistoricDataReader->getTimeSeries();
    }
      
  private:
    std::string mTickerSymbol;
    std::shared_ptr<DataSourceReader> mDataSourceReader;
    DateRange mDateRangeToCollect;
    TimeFrame::Duration mTimeFrame;
    std::shared_ptr<HistoricDataReader<Decimal>> mHistoricDataReader;
    bool mDataRead;
  };
    
  template <class Decimal>
  class HistoricDataReaderFactory
  {
  public:
    static std::shared_ptr<HistoricDataReader<Decimal>> createHistoricDataReader(const std::string& historicDataFilePath,
										 enum HistoricDataReader<Decimal>::HistoricDataFileFormat dataFileFormat,
										 TimeFrame::Duration timeFrame,
										 TradingVolume::VolumeUnit unitsOfVolume,
										 const Decimal& tickValue)
   {
     std::shared_ptr<TimeSeriesCsvReader<Decimal>> reader;
     
     if (dataFileFormat == HistoricDataReader<Decimal>::PAL)
      reader = std::make_shared<PALFormatCsvReader<Decimal>>(historicDataFilePath, timeFrame,
							     unitsOfVolume, tickValue);
    else if (dataFileFormat == HistoricDataReader<Decimal>::TRADESTATION)
      reader = std::make_shared<TradeStationFormatCsvReader<Decimal>>(historicDataFilePath, timeFrame,
								    unitsOfVolume, tickValue);
    else if (dataFileFormat == HistoricDataReader<Decimal>::CSI_EXTENDED)
      reader = std::make_shared<CSIExtendedFuturesCsvReader<Decimal>>(historicDataFilePath, timeFrame,
								      unitsOfVolume, tickValue);
    else if (dataFileFormat == HistoricDataReader<Decimal>::CSI)
      reader = std::make_shared<CSIFuturesCsvReader<Decimal>>(historicDataFilePath, timeFrame,
							      unitsOfVolume, tickValue);
    else if (dataFileFormat == HistoricDataReader<Decimal>::TRADESTATION_INDICATOR1)
      reader = std::make_shared<TradeStationIndicator1CsvReader<Decimal>>(historicDataFilePath,
									  timeFrame,
									  unitsOfVolume,
									  tickValue);
    else
      throw HistoricDataReaderException("Historic data file format not recognized");

    return std::make_shared<HistoricDataFileReader<Decimal>> (reader);
  }

  static std::shared_ptr<HistoricDataReader<Decimal>> createHistoricDataReader(std::string tickerSymbol,
									       enum HistoricDataReader<Decimal>::HistoricDataApi apiService,
									       std::string apiKey,
									       const DateRange& dateRangeToCollect,
									       TimeFrame::Duration timeFrame)
  {
    std::shared_ptr<DataSourceReader> reader;

    if(apiService == HistoricDataReader<Decimal>::FINNHUB) 
      reader = std::make_shared<FinnhubIOReader>(apiKey);
    else if(apiService == HistoricDataReader<Decimal>::BARCHART) 
      reader = std::make_shared<BarchartReader>(apiKey);
    else
      throw HistoricDataReaderException("Data source not recognized");

    return std::make_shared<HistoricDataApiReader<Decimal>>(tickerSymbol, reader, dateRangeToCollect, timeFrame);
  }

  static enum HistoricDataReader<Decimal>::HistoricDataApi getApiFromString(const std::string& dataSourceName)
  {
    if(boost::iequals(dataSourceName, "finnhub")) 
      return HistoricDataReader<Decimal>::FINNHUB;
    else if(boost::iequals(dataSourceName, "barchart"))
      return HistoricDataReader<Decimal>::BARCHART;
    else
      throw HistoricDataReaderException("Data source " + dataSourceName + " not recognized");
  }

   static enum HistoricDataReader<Decimal>::HistoricDataFileFormat getFileFormatFromString(const std::string& dataFileFormatStr)
    {
      std::string upperCaseFormatStr = boost::to_upper_copy(dataFileFormatStr);

    if (upperCaseFormatStr == std::string("PAL"))
      return HistoricDataReader<Decimal>::PAL;
    else if (upperCaseFormatStr == std::string("TRADESTATION"))
      return HistoricDataReader<Decimal>::TRADESTATION;
    else if (upperCaseFormatStr == std::string("CSIEXTENDED"))
      return HistoricDataReader<Decimal>::CSI_EXTENDED;
    else if (upperCaseFormatStr == std::string("CSI"))
      return HistoricDataReader<Decimal>::CSI;
    else if (upperCaseFormatStr == std::string("TRADESTATIONINDICATOR1"))
      return HistoricDataReader<Decimal>::TRADESTATION_INDICATOR1;
    else
      throw HistoricDataReaderException("Historic data file format " +dataFileFormatStr +" not recognized");
    }

  private:
    HistoricDataReaderFactory()
    {}
  };
}



#endif

