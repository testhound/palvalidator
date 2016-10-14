// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __CSVWRITER_H
#define __CSVWRITER_H 1

#include "TimeSeries.h"
#include <boost/date_time.hpp>
#include <fstream>

namespace mkc_timeseries
{
  using namespace dec;

  //
  // class PalTimeSeriesCsvWriter
  //
  // Write a OHLCTimeSeries out in a format that
  // can be read by PriceActionLab

  template <int Prec>
  class PalTimeSeriesCsvWriter
  {
    using Decimal = decimal<Prec>;

  public:
    PalTimeSeriesCsvWriter (const std::string& fileName, 
			    const OHLCTimeSeries<Decimal>& series)
      : mCsvFile (fileName),
	mTimeSeries (series)
    {
    }

    PalTimeSeriesCsvWriter (const PalTimeSeriesCsvWriter& rhs)
      : mCsvFile (rhs.mCsvFile),
	mTimeSeries (rhs.mTimeSeries)
    {}

    PalTimeSeriesCsvWriter& 
    operator=(const PalTimeSeriesCsvWriter &rhs)
    {
      if (this == &rhs)
	return *this;

      mCsvFile = rhs.mCsvFile;
      mTimeSeries = rhs.mTimeSeries;

      return *this;
    }

    ~PalTimeSeriesCsvWriter()
    {}

    void writeFile()
    {
      typename OHLCTimeSeries<Decimal>::ConstTimeSeriesIterator it = 
	mTimeSeries.beginSortedAccess();
      boost::gregorian::date timeSeriesDate;

      for (; it != mTimeSeries.endSortedAccess(); it++)
	{
	  timeSeriesDate = it->first;
	  const auto& timeSeriesEntry = it->second;

	  mCsvFile << boost::gregorian::to_iso_string (timeSeriesDate) << "," << 
	    timeSeriesEntry.getOpenValue() << "," <<
	    timeSeriesEntry.getHighValue() << "," << timeSeriesEntry.getLowValue() << "," <<
	    timeSeriesEntry.getCloseValue() << std::endl;
	}
    }

  private:
    std::ofstream mCsvFile;
    OHLCTimeSeries<Decimal> mTimeSeries;
  };
}

#endif


