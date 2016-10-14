// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __MCPT_CONFIGURATION_FILE_H
#define __MCPT_CONFIGURATION_FILE_H 1

#include <string>
#include <memory>
#include <exception>
#include <boost/date_time.hpp>
#include "Security.h"
#include "DateRange.h"
#include "BackTester.h"
#include "PalAst.h"

namespace mkc_timeseries
{
  class McptConfigurationFileReaderException : public std::runtime_error
  {
  public:
  McptConfigurationFileReaderException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~McptConfigurationFileReaderException()
      {}
  };

  template <class Decimal>
  class McptConfiguration
  {
  public:
    McptConfiguration (std::shared_ptr<BackTester<Decimal>> aBacktester,
		       std::shared_ptr<BackTester<Decimal>> aInSampleBacktester,
		       std::shared_ptr<mkc_timeseries::Security<Decimal>> aSecurity, 
		       PriceActionLabSystem* patterns,
		       const DateRange& insampleDateRange, 
		       const DateRange& oosDateRange)
      : mBacktester (aBacktester),
	mInSampleBacktester (aInSampleBacktester),
	mSecurity(aSecurity),
	mPricePatterns(patterns),
	mInsampleDateRange(insampleDateRange),
	mOosDateRange(oosDateRange)
    {}

    McptConfiguration (const McptConfiguration& rhs)
      : mBacktester (rhs.mBacktester),
	mInSampleBacktester (rhs.mInSampleBacktester),
	mSecurity(rhs.mSecurity),
	mPricePatterns(rhs.mPricePatterns),
	mInsampleDateRange(rhs.mInsampleDateRange),
	mOosDateRange(rhs.mOosDateRange)
    {}

    McptConfiguration<Decimal>& 
    operator=(const McptConfiguration<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mBacktester = rhs.mBacktester;
      mInSampleBacktester = rhs.mInSampleBacktester;
      mSecurity= rhs.mSecurity;
      mPricePatterns= rhs.mPricePatterns;
      mInsampleDateRange= rhs.mInsampleDateRange;
      mOosDateRange= rhs.mOosDateRange;

      return *this;
    }

    ~McptConfiguration()
    {}

    std::shared_ptr<BackTester<Decimal>> getBackTester() const
    {
      return mBacktester;
    }

    std::shared_ptr<BackTester<Decimal>> getInSampleBackTester() const
    {
      return mInSampleBacktester;
    }

    std::shared_ptr<Security<Decimal>> getSecurity() const
    {
      return mSecurity;
    }

    PriceActionLabSystem *getPricePatterns() const
    {
      return mPricePatterns;
    }

    const DateRange& getInsampleDateRange() const
    {
      return mInsampleDateRange;
    }

    const DateRange& getOosDateRange() const
    {
      return mOosDateRange;
    }

  private:
    std::shared_ptr<BackTester<Decimal>> mBacktester;
    std::shared_ptr<BackTester<Decimal>> mInSampleBacktester;
    std::shared_ptr<Security<Decimal>> mSecurity;
    PriceActionLabSystem *mPricePatterns;
    DateRange mInsampleDateRange;
    DateRange mOosDateRange;
  };

  class McptConfigurationFileReader
  {
  public:
    McptConfigurationFileReader (const std::string& configurationFileName);
    ~McptConfigurationFileReader()
      {}

    std::shared_ptr<McptConfiguration<decimal<7>>> readConfigurationFile();

  private:
    std::string mConfigurationFileName;
  };
}

#endif
