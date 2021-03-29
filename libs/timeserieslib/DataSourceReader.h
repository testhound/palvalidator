#ifndef __DATAREADER_H
#define __DATAREADER_H 1

#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <curl/curl.h>
#include <jsoncpp/json/json.h>
#include <ctime>
#include <stdio.h>
#include <vector>

namespace mkc_timeseries
{
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
      setApiTimeFrameRepresentation(configTimeFrame);

      boost::posix_time::time_duration zero(0, 0, 0);
      boost::posix_time::ptime bStartDatetime(isDateRange.getFirstDate() - boost::gregorian::days(7), zero);
      boost::posix_time::ptime bEndDatetime(oosDateRange.getLastDate() + boost::gregorian::days(7), zero);

      std::string uri = buildDataFetchUri(ticker, bStartDatetime, bEndDatetime);
      std::string filename = getFilename(ticker);

      if (!performDownload) {
        return filename;
      }
      tempFilenames.push_back(filename);

      Json::Value json = getJson(uri);

      if (!validApiResponse(json)) // no data returned - error
        throw McptConfigurationFileReaderException("No data returned from API call.");
        
      // transform JSON into CSV in TradeStation format.
      std::ofstream csvFile;
      csvFile.open("./" + filename);
      if(boost::iequals(mResolution, "D"))
        csvFile << "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Vol\",\"OI\"" << std::endl;
      else if (boost::iequals(mResolution, "60")) 
        csvFile << "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Up\",\"Down\"" << std::endl;

      for(Json::Value::ArrayIndex idx = 0; idx != getJsonArraySize(json); idx++) 
        csvFile << getCsvRow(json, idx) << std::endl;

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

    virtual std::string buildDataFetchUri(std::string ticker, boost::posix_time::ptime startDatetime, 
                boost::posix_time::ptime endDatetime) = 0;
    virtual bool validApiResponse(Json::Value json) = 0;
    virtual std::string getCsvRow(Json::Value json, Json::Value::ArrayIndex idx) = 0;
    virtual int getJsonArraySize(Json::Value json) = 0;
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

    std::string getFilename(std::string ticker) 
    {
      return (boost::format("%1%_RAD_%2%.txt") % ticker % (boost::iequals(mResolution, "60") ? "Hourly" : "Daily")).str();
    }

    Json::Value getJson(std::string uri) 
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

      Json::Reader reader;
      Json::Value data;
      reader.parse(buffer, data);
      
      return data;
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
    std::string buildDataFetchUri(std::string ticker, boost::posix_time::ptime startDatetime, 
                boost::posix_time::ptime endDatetime)
    {
      std::string uri = "https://finnhub.io/api/v1/stock/candle?symbol=%1%&resolution=%2%&from=%3%&to=%4%&format=json&token=%5%";

      std::string startTimestamp = boost::lexical_cast<std::string>(timestampFromPtime(startDatetime));
      std::string endTimestamp = boost::lexical_cast<std::string>(timestampFromPtime(endDatetime));

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

    bool validApiResponse(Json::Value json) 
    {
      return boost::iequals(json["s"].asString(), "ok");
    }

    int getJsonArraySize(Json::Value json) 
    {
      return (json["o"]).size();
    }

    std::string getCsvRow(Json::Value json, Json::Value::ArrayIndex idx) 
    {
      std::string csvRow = "%1%,%2%,%3%,%4%,%5%,%6%,0,0";
      boost::posix_time::ptime time  = boost::posix_time::from_time_t(json["t"][idx].asDouble());
      return (boost::format(csvRow) % 
                  ptimeToFormat(time, "%m/%d/%Y") %
                  ptimeToFormat(time, "%H:%M") %
                  priceFormat(json["o"][idx].asFloat()) % 
                  priceFormat(json["h"][idx].asFloat()) % 
                  priceFormat(json["l"][idx].asFloat()) % 
                  priceFormat(json["c"][idx].asFloat())
      ).str();
    }

  };

  static std::shared_ptr<DataSourceReader> getDataSourceReader(
         std::string dataSourceName, 
         std::string apiKey) 
  {
    if(boost::iequals(dataSourceName, "finnhub")) 
      return std::make_shared<FinnhubIOReader>(apiKey);
    else
      throw McptConfigurationFileReaderException("Data source " + dataSourceName + " not recognized");
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
      throw McptConfigurationFileReaderException(
          "Source " + dataSourceName + " does not exist in " + apiConfigFilename);

    return token;
  }  
}
#endif
