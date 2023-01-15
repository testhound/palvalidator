#ifndef __DATAREADER_H
#define __DATAREADER_H 1

#include <exception>
#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <ctime>
#include <stdio.h>
#include <vector>
#include "DateRange.h"
#include "csv.h"

namespace mkc_timeseries
{
  class DataSourceReaderException : public std::runtime_error
  {
  public:
  DataSourceReaderException(const std::string msg)
    : std::runtime_error(msg)
      {}

    ~DataSourceReaderException()
      {}
  };

  class DataSourceReader
  {
      /* a super class for the other data sources.
       * is the best plan of action to create a CSV in 
       * the PAL format as a temporary file, read the
       * temp file via the prebuilt readers, then delete
       * the temp files?
       */
  public:
    DataSourceReader(std::string APIToken) : mApiKey(APIToken)
    {}

    /*
     * Constructes a URI for the platform, downloads the data to a temporary
     * CSV file, and returns the filename
     */
    std::string createTemporaryFile(std::string ticker, std::string configTimeFrame,
            DateRange isDateRange, DateRange oosDateRange, bool performDownload)
    {
      DateRange dRange(isDateRange.getFirstDate(), oosDateRange.getLastDate());
      return this->createTemporaryFile(ticker, configTimeFrame, dRange, performDownload);
    }

    /*
     * Constructes a URI for the platform, downloads the data to a temporary
     * CSV file, and returns the filename
     */
    std::string createTemporaryFile(std::string ticker, std::string configTimeFrame,
				    DateRange dateRangeToCollect, bool performDownload)
    {
      setApiTimeFrameRepresentation(configTimeFrame);
      std::string uri = buildDataFetchUri(ticker, dateRangeToCollect.getFirstDate() - boost::gregorian::days(2), 
					  dateRangeToCollect.getLastDate() + boost::gregorian::days(2));
      std::string filename = getFilename(ticker, configTimeFrame);

      if (!performDownload) {
        return filename;
      }
      tempFilenames.push_back(filename);

      rapidjson::Document jsonDocument = getJson(uri);

      if (!validApiResponse(jsonDocument)) // no data returned - error
        throw DataSourceReaderException("No data returned from API call.");
        
      // transform JSON into CSV in TradeStation format.
      std::ofstream csvFile;
      csvFile.open("./" + filename);
      if(boost::iequals(configTimeFrame, "daily"))
        csvFile << "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Vol\",\"OI\"" << std::endl;
      else if (boost::iequals(configTimeFrame, "hourly")) 
        csvFile << "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Up\",\"Down\"" << std::endl;

      rapidjson::SizeType resultSize = getJsonArraySize(jsonDocument);
      std::string csvString = "";
      for(rapidjson::SizeType idx = 0; idx != resultSize; idx++) 
      {
        csvString += getCsvRow(jsonDocument, idx);
        csvString += "\n";
      }

      csvFile << csvString;
      csvFile.close();

      return filename;
    }
    
    /*
     * Destroys the temp file created for the CSV time series reader.
     */
    void destroyFiles()
    {
      for(auto it = tempFilenames.begin(); it != tempFilenames.end(); it++)
      {
        if(std::remove((*it).c_str()) == 0)
        {
            // error deleting file
        }
      }
    }

  protected:
    const std::string mApiKey;
    std::string mResolution;
    std::vector<std::string> tempFilenames;

    virtual std::string buildDataFetchUri(std::string ticker, boost::gregorian::date startDatetime, boost::gregorian::date endDatetime) = 0;
    virtual bool validApiResponse(rapidjson::Document& jsonDocument) = 0;
    virtual std::string getCsvRow(rapidjson::Document& jsonDocument, rapidjson::SizeType idx) = 0;
    virtual rapidjson::SizeType getJsonArraySize(rapidjson::Document& jsonDocument) = 0;
    virtual void setApiTimeFrameRepresentation(std::string configTimeFrame) = 0;

    time_t timestampFromPtime(boost::posix_time::ptime time) 
    {
      boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
      boost::posix_time::time_duration::sec_type secs = (time - epoch).total_seconds();

      return time_t(secs);
    }

    std::string ptimeToFormat(boost::posix_time::ptime time, std::string format) 
    {
      std::stringstream ss;
      boost::posix_time::time_facet* facet = new boost::posix_time::time_facet();
      facet->format(format.c_str());
      
      ss.imbue(std::locale(std::locale::classic(), facet));
      ss << time;

      return ss.str();
    }

    std::string priceFormat(float priceValue) 
    {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << priceValue;
      return ss.str();
    }

  private:
    static size_t jsonWriteCallback(void *ptr, size_t size, size_t nmemb, std::string* data) 
    {
      data->append((char*)ptr, size*nmemb);
      return size*nmemb;
    }

    std::string getFilename(std::string ticker, std::string configTimeFrame) 
    {
      return (boost::format("%1%_RAD_%2%.txt") % ticker % (boost::iequals(configTimeFrame, "hourly") ? "Hourly" : "Daily")).str();
    }

    rapidjson::Document getJson(std::string uri) 
    {
      CURL *curl;
      std::string buffer;

      curl = curl_easy_init();

      if(curl)
      {
        curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, jsonWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

        curl_easy_perform(curl);

        curl_easy_cleanup(curl);
      }

      rapidjson::Document document;
      document.Parse(buffer.c_str());
      
      return document;
    }

  };

  /*
   * Data reader implementation for Finnhub.IO.
   */
  class FinnhubIOReader : public DataSourceReader 
  {
  public:
    FinnhubIOReader(std::string APIToken) : DataSourceReader(APIToken)
    {}

  protected:
    std::string buildDataFetchUri(std::string ticker, boost::gregorian::date startDatetime, boost::gregorian::date endDatetime)
    {
      boost::posix_time::time_duration zero(0, 0, 0);
      boost::posix_time::ptime bStartDatetime(startDatetime, zero);
      boost::posix_time::ptime bEndDatetime(endDatetime, zero);

      std::string uri = "https://finnhub.io/api/v1/stock/candle?symbol=%1%&resolution=%2%&from=%3%&to=%4%&format=json&token=%5%";

      std::string startTimestamp = boost::lexical_cast<std::string>(timestampFromPtime(bStartDatetime));
      std::string endTimestamp = boost::lexical_cast<std::string>(timestampFromPtime(bEndDatetime));

      return (boost::format(uri) % 
                ticker % 
                mResolution % 
                startTimestamp % 
                endTimestamp % 
                mApiKey
      ).str();
    }

    void setApiTimeFrameRepresentation(std::string configTimeFrame)
    {
      if(boost::iequals(configTimeFrame, "Daily")) 
        mResolution = "D";
      if(boost::iequals(configTimeFrame, "hourly"))
        mResolution = "60";
    }

    bool validApiResponse(rapidjson::Document& json) 
    {
      return boost::iequals(json["s"].GetString(), "ok");
    }

    rapidjson::SizeType getJsonArraySize(rapidjson::Document& json)
    {
      const rapidjson::Value& results = json["c"].GetArray();
      return results.Size();
    }

    std::string getCsvRow(rapidjson::Document& json, rapidjson::SizeType idx) 
    {
      std::string csvRow = "%1%,%2%,%3%,%4%,%5%,%6%,0,0";
      boost::posix_time::ptime time  = boost::posix_time::from_time_t(json["t"].GetArray()[idx].GetInt());
      return (boost::format(csvRow) % 
                  ptimeToFormat(time, "%m/%d/%Y") %
                  ptimeToFormat(time, "%H:%M") %
                  priceFormat(json["o"].GetArray()[idx].GetFloat()) % 
                  priceFormat(json["h"].GetArray()[idx].GetFloat()) % 
                  priceFormat(json["l"].GetArray()[idx].GetFloat()) % 
                  priceFormat(json["c"].GetArray()[idx].GetFloat())
      ).str();
    }

  };

  /*
  * Barchart DataSourceReader implementation.
  */
  class BarchartReader : public DataSourceReader 
  {
    public:
      BarchartReader(std::string APIToken) : DataSourceReader(APIToken)
      {}
    protected:
      std::string buildDataFetchUri(std::string ticker, boost::gregorian::date startDatetime, boost::gregorian::date endDatetime)
      {
        std::string uri = "http://ondemand.websol.barchart.com/getHistory.json?apikey=%1%&symbol=%2%&type=%3%&startDate=%4%&endDate=%5%";

        boost::posix_time::time_duration zero(0, 0, 0);
        boost::posix_time::ptime bStartDatetime(startDatetime, zero);
        boost::posix_time::ptime bEndDatetime(endDatetime, zero);

        std::string startDate = ptimeToFormat(bStartDatetime, "%Y%m%d");
        std::string endDate = ptimeToFormat(bEndDatetime, "%Y%m%d");

        return (boost::format(uri) % mApiKey % ticker % mResolution % startDate % endDate).str();
      }

      void setApiTimeFrameRepresentation(std::string configTimeFrame)
      {
        if(boost::iequals(configTimeFrame, "Daily")) 
          mResolution = "daily";
        if(boost::iequals(configTimeFrame, "hourly"))
          mResolution = "minutes&interval=60";
      }

      bool validApiResponse(rapidjson::Document& json) 
      {
        return json["status"]["code"].GetInt() == 200;
      }

      rapidjson::SizeType getJsonArraySize(rapidjson::Document& json)
      {
        return (json["results"].GetArray()).Size();
      }

      std::string getCsvRow(rapidjson::Document& json, rapidjson::SizeType idx) 
      {
        const rapidjson::Value& results = json["results"].GetArray()[idx];
        std::string csvRow = "%1%,%2%,%3%,%4%,%5%,%6%,0,0";
        boost::posix_time::ptime time = boost::date_time::parse_delimited_time<boost::posix_time::ptime>(results["timestamp"].GetString(), 'T');

        return (boost::format(csvRow) % 
                    ptimeToFormat(time, "%m/%d/%Y") %
                    ptimeToFormat(time, "%H:%M") %
                    priceFormat(results["open"].GetFloat()) % 
                    priceFormat(results["high"].GetFloat()) % 
                    priceFormat(results["low"].GetFloat()) % 
                    priceFormat(results["close"].GetFloat())
        ).str();
      }
  };

  class DataSourceReaderFactory
  {
  public:
    static std::shared_ptr<DataSourceReader> getDataSourceReader(std::string dataSourceName,
								 std::string apiKey)
    {
      if(boost::iequals(dataSourceName, "finnhub"))
	return std::make_shared<FinnhubIOReader>(apiKey);
      else if(boost::iequals(dataSourceName, "barchart"))
	return std::make_shared<BarchartReader>(apiKey);
      else
	throw DataSourceReaderException("Data source " + dataSourceName + " not recognized");
    }

    static std::string getApiTokenFromFile(std::string apiConfigFilename, std::string dataSourceName)
    {
      std::string source, token = "";
      io::CSVReader<2> csvApiConfig(apiConfigFilename.c_str());
      csvApiConfig.set_header("Source", "Token");

      while(csvApiConfig.read_row(source, token))
	if(boost::iequals(dataSourceName, source))
	  break;

      if(token.empty())
	throw DataSourceReaderException("Source " + dataSourceName + " does not exist in " + apiConfigFilename);

      return token;
    }

  };
  //std::shared_ptr<DataSourceReader> getDataSourceReader(std::string dataSourceName, std::string apiKey);
  //std::string getApiTokenFromFile(std::string apiConfigFilename, std::string dataSourceName);
}
#endif
