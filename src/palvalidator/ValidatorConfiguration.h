// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#pragma once

#include <string>
#include <memory>
#include <exception>
#include <boost/date_time.hpp>
#include "Security.h"
#include "DateRange.h"
#include "PalAst.h"
#include "number.h"

namespace mkc_timeseries
{
  class ValidatorConfigurationException : public std::runtime_error
  {
  public:
  ValidatorConfigurationException(const std::string msg)
    : std::runtime_error(msg)
      {}

    ~ValidatorConfigurationException()
      {}
  };

  template <class Decimal>
  class ValidatorConfiguration
  {
  public:
    ValidatorConfiguration(std::shared_ptr<mkc_timeseries::Security<Decimal>> aSecurity,
                          PriceActionLabSystem* patterns,
                          const DateRange& insampleDateRange,
                          const DateRange& oosDateRange)
      : mSecurity(aSecurity),
        mPricePatterns(patterns),
        mInsampleDateRange(insampleDateRange),
        mOosDateRange(oosDateRange)
    {}

    ValidatorConfiguration (const ValidatorConfiguration& rhs)
      : mSecurity(rhs.mSecurity),
        mPricePatterns(rhs.mPricePatterns),
        mInsampleDateRange(rhs.mInsampleDateRange),
        mOosDateRange(rhs.mOosDateRange)
    {}

    ValidatorConfiguration<Decimal>&
    operator=(const ValidatorConfiguration<Decimal> &rhs)
    {
      if (this == &rhs)
        return *this;

      mSecurity = rhs.mSecurity;
      mPricePatterns = rhs.mPricePatterns;
      mInsampleDateRange = rhs.mInsampleDateRange;
      mOosDateRange = rhs.mOosDateRange;

      return *this;
    }

    ~ValidatorConfiguration()
    {}

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
    std::shared_ptr<Security<Decimal>> mSecurity;
    PriceActionLabSystem *mPricePatterns;
    DateRange mInsampleDateRange;
    DateRange mOosDateRange;
  };

  class ValidatorConfigurationFileReader
  {
    using Decimal = num::DefaultNumber;

  public:
    ValidatorConfigurationFileReader (const std::string& configurationFileName);
    ~ValidatorConfigurationFileReader()
      {}

    std::shared_ptr<ValidatorConfiguration<Decimal>> readConfigurationFile();

  private:
    std::string mConfigurationFileName;
  };
}

