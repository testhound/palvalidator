// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STOP_TARGET_DETAIL_H
#define __STOP_TARGET_DETAIL_H 1

#include <string>
#include "number.h"
#include "csv.h"
#include <cstdlib>
#include "DecimalConstants.h"

using Decimal = num::DefaultNumber;

class StopTargetDetail
{
 public:
  StopTargetDetail (const Decimal& stopLoss, const Decimal& profitTarget,
		    unsigned int minHoldingPeriod, unsigned int maxHoldingPeriod)
    : mStop (stopLoss),
    mProfitTarget(profitTarget),
    mMinimumHoldingPeriod (minHoldingPeriod),
    mMaxHoldingPeriod (maxHoldingPeriod)
  {}

  Decimal getStopLoss() const
  {
      return mStop;
  }
  
  Decimal getProfitTarget() const
  {
    return mProfitTarget;
  }
  
  unsigned int getMinHoldingPeriod() const
  {
    return mMinimumHoldingPeriod;
  }
  
  unsigned int getMaxHoldingPeriod() const
  {
    return mMaxHoldingPeriod;
  }
  
 private:
  Decimal mStop;
  Decimal mProfitTarget;
  unsigned int mMinimumHoldingPeriod;
  unsigned int mMaxHoldingPeriod;
};

//
// class StopTargetDetailReader
//
// Reads in a file with profit, target, min hold period, max hold period information
//
// NOTE:
// First line is for First Standard Deviation stop loss, profit target
// Second line is for Second Standard Deviation stop loss, profit target

class StopTargetDetailReader
{
 public:
  StopTargetDetailReader (const std::string& fileName)
    : mDev1Details (mkc_timeseries::DecimalConstants<Decimal>::DecimalZero,
		    mkc_timeseries::DecimalConstants<Decimal>::DecimalZero,
		    0,
		    0),
      mDev2Details (mkc_timeseries::DecimalConstants<Decimal>::DecimalZero,
		    mkc_timeseries::DecimalConstants<Decimal>::DecimalZero,
		    0,
		    0)
  {
    io::CSVReader<4> csvConfigFile(fileName.c_str());

    csvConfigFile.set_header("Stop", "Target", "MinHold","MaxHold");

    std::string stopLossStr, profitTargetStr, minHoldStr, maxHoldStr;
    Decimal stopLoss, profitTarget;
    unsigned int minHold, maxHold;

    csvConfigFile.read_row (stopLossStr, profitTargetStr, minHoldStr, maxHoldStr);
    stopLoss = num::fromString<Decimal>(stopLossStr);
    profitTarget = num::fromString<Decimal>(profitTargetStr);
    minHold = std::atoi (minHoldStr.c_str());
    maxHold = std::atoi (maxHoldStr.c_str());

    mDev1Details = StopTargetDetail (stopLoss, profitTarget, minHold, maxHold);

    csvConfigFile.read_row (stopLossStr, profitTargetStr, minHoldStr, maxHoldStr);
    stopLoss = num::fromString<Decimal>(stopLossStr);
    profitTarget = num::fromString<Decimal>(profitTargetStr);
    minHold = std::atoi (minHoldStr.c_str());
    maxHold = std::atoi (maxHoldStr.c_str());

    mDev2Details = StopTargetDetail (stopLoss, profitTarget, minHold, maxHold);
  }

  StopTargetDetail getDev1Detail() const
  {
    return mDev1Details;
  }

  StopTargetDetail getDev2Detail() const
  {
    return mDev2Details;
  }


 private:
  StopTargetDetail mDev1Details;
  StopTargetDetail mDev2Details;
};

#endif
