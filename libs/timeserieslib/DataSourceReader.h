#ifndef __DATAREADER_H
#define __DATAREADER_H 1

//#include "TimeSeries.h"
#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
#include "boost/format.hpp"

//#include "DecimalConstants.h"
//#include "csv.h"
#include <curl/curl.h>
#include <ctime>
#include <stdio.h>

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
    void destroyFile(std::string filename)
    {
      if(std::remove(filename.c_str()) == 0)
      {
          // error deleting file
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

    virtual std::string buildDataFetchUri(std::string ticker, std::string resolution, 
            boost::gregorian::date startDate, boost::gregorian::date endDate) = 0;
  };

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
      std::string filename = (boost::format("%1_%2_%3.csv") % timestamp % ticker % resolution).str();

      // TODO: read from API and transform into CSV in TradeStation format.
      
      return filename;
    }

  protected:
    std::string buildDataFetchUri(std::string ticker, std::string resolution,
            boost::gregorian::date startDate, boost::gregorian::date endDate)
    {
      // valid resolutions: 1, 5, 15, 30, 60, D, W, M
      std::string parameters = "candle?symbol=%1&resolution=%2&from=%3&to=%4&token=%5";

      std::string startTimestamp = boost::lexical_cast<std::string>(timestampFromBoostDate(startDate));
      std::string endTimestamp = boost::lexical_cast<std::string>(timestampFromBoostDate(endDate));


      return (boost::format(parameters) % ticker % resolution % startTimestamp % endTimestamp % mApiKey).str();
    }
  };
}
#endif
