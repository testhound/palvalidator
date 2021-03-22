#ifndef __DATAREADER_H
#define __DATAREADER_H 1

#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
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
    DataSourceReader(std::string APIToken, std::string baseUri) 
        : mApiKey(APIToken), mBaseUri(baseUri)
    {}

    /*
     * Constructes a URI for the platform, downloads the data to a temporary
     * CSV file, and returns the filename (for deleting later);
     */
    virtual std::string createTemporaryFile(std::string ticker, std::string resolution,
            boost::gregorian::date startDate, boost::gregorian::date endDate) = 0;
    
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

    std::time_t timestampFromBoostDate(boost::gregorian::date date)
    {
      using namespace boost::posix_time;
      static ptime epoch(boost::gregorian::date(1970, 1, 1));
      time_duration::sec_type secs = (ptime(date,seconds(0)) - epoch).total_seconds();
	    return time_t(secs);
    }

  protected:
    const std::string mApiKey;
    const std::string mBaseUri;
    
    std::vector<std::string> tempFilenames;

    virtual std::string buildDataFetchUri(std::string ticker, std::string resolution, 
            boost::gregorian::date startDate, boost::gregorian::date endDate) = 0;

    static size_t jsonWriteCallback(void *ptr, size_t size, size_t nmemb, std::string* data) 
    {
      std::cout << (char*)ptr << std::endl;
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

    void jsonToCsv(Json::Value data, std::string outputFilename) 
    {

    }

  };

  /*
   * Data reader implementation for Finnhub.IO
   */
  class FinnhubIOReader : public DataSourceReader 
  {
  public:
    FinnhubIOReader(std::string APIToken) 
        : DataSourceReader(APIToken, "https://finnhub.io/api/v1/")
    {}

    std::string createTemporaryFile(std::string ticker, std::string resolution,
            boost::gregorian::date startDate, boost::gregorian::date endDate) 
    {
      std::string uri = buildDataFetchUri(ticker, resolution, startDate, endDate);
      std::string timestamp = boost::lexical_cast<std::string>(
              timestampFromBoostDate(boost::gregorian::day_clock::local_day()));
      std::string filename = (boost::format("%1%_%2%_%3%.csv") % timestamp % ticker % resolution).str();
      tempFilenames.push_back(filename);

      Json::Value json = getJson(uri);
      // transform JSON into CSV in TradeStation format.

      return filename;
    }

  protected:
    std::string buildDataFetchUri(std::string ticker, std::string resolution,
            boost::gregorian::date startDate, boost::gregorian::date endDate)
    {
      // valid resolutions: 1, 5, 15, 30, 60, D, W, M
      std::string parameters = "stock/candle?symbol=%1%&resolution=%2%&from=%3%&to=%4%&token=%5%";

      std::string startTimestamp = boost::lexical_cast<std::string>(timestampFromBoostDate(startDate));
      std::string endTimestamp = boost::lexical_cast<std::string>(timestampFromBoostDate(endDate));

      return mBaseUri + 
        (boost::format(parameters) % ticker % resolution % startTimestamp % endTimestamp % mApiKey).str();
    }
  };
}
#endif
