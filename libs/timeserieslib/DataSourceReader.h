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
     * CSV file, and returns the filename (for deleting later);
     */
    std::string createTemporaryFile(std::string ticker, std::string resolution,
            std::string startDatetime, std::string endDatetime)
    {
      boost::posix_time::ptime bStartDatetime = boost::posix_time::time_from_string(startDatetime);
      boost::posix_time::ptime bEndDatetime = boost::posix_time::time_from_string(endDatetime);

      std::string uri = buildDataFetchUri(ticker, resolution, bStartDatetime, bEndDatetime);
      std::string timestamp = boost::lexical_cast<std::string>(
        timestampFromPtime(boost::posix_time::second_clock::local_time()));
      std::string filename = (boost::format("%1%_%2%_%3%.csv") % timestamp % ticker % resolution).str();
      tempFilenames.push_back(filename);

      Json::Value json = getJson(uri);

      if (!validApiResponse(json)) // no data returned - error
        throw McptConfigurationFileReaderException("No data returned from API call.");

      // transform JSON into CSV in TradeStation format.
      std::ofstream csvFile;
      csvFile.open("./" + filename);
      if(boost::iequals(resolution, "D"))
        csvFile << "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Vol\",\"OI\"" << std::endl;
      else if (boost::iequals(resolution, "60")) 
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
    
    std::vector<std::string> tempFilenames;

    virtual std::string buildDataFetchUri(std::string ticker, std::string resolution, 
            boost::posix_time::ptime startDatetime, boost::posix_time::ptime endDatetime) = 0;
    virtual bool validApiResponse(Json::Value json) = 0;
    virtual std::string getCsvRow(Json::Value json, Json::Value::ArrayIndex idx) = 0;
    virtual int getJsonArraySize(Json::Value json) = 0;

    time_t timestampFromPtime(boost::posix_time::ptime time) 
    {
      boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
      boost::posix_time::time_duration::sec_type x = (time - epoch).total_seconds();

      return time_t(x);
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

    Json::Value getJson(std::string uri) 
    {
      CURL *curl;
      CURLcode res;
      std::string buffer;

      curl = curl_easy_init();

      if(curl)
      {
        curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, jsonWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        res = curl_easy_perform(curl);

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
    std::string buildDataFetchUri(std::string ticker, std::string resolution,
            boost::posix_time::ptime startDatetime, boost::posix_time::ptime endDatetime)
    {
      // valid resolutions: 1, 5, 15, 30, 60, D, W, M
      std::string uri = "https://finnhub.io/api/v1/stock/candle?symbol=%1%&resolution=%2%&from=%3%&to=%4%&format=json&token=%5%";

      std::string startTimestamp = boost::lexical_cast<std::string>(timestampFromPtime(startDatetime));
      std::string endTimestamp = boost::lexical_cast<std::string>(timestampFromPtime(endDatetime));

      return (boost::format(uri) % 
                ticker % 
                resolution % 
                startTimestamp % 
                endTimestamp % 
                mApiKey
      ).str();
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
}
#endif
