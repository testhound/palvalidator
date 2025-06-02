/**
 * @file StopTargetDetail.h
 * @brief Defines classes for managing stop-loss and profit-target details.
 *
 * Copyright (C) MKC Associates, LLC - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Michael K. Collison <collison956@gmail.com>, July 2016
 */

#ifndef __STOP_TARGET_DETAIL_H
#define __STOP_TARGET_DETAIL_H 1

#include <string>
#include "number.h"
#include "csv.h"
#include <cstdlib>
#include "DecimalConstants.h"

/**
 * @brief Type alias for decimal numbers used in financial calculations.
 */
using Decimal = num::DefaultNumber;

/**
 * @brief Holds details for stop-loss, profit-target, and holding periods.
 *
 * This class encapsulates the parameters defining a trading strategy's
 * exit conditions based on price movement and time.
 */
class StopTargetDetail
{
 public:
  /**
   * @brief Constructs a StopTargetDetail object.
   * @param stopLoss The stop-loss level (as a Decimal).
   * @param profitTarget The profit-target level (as a Decimal).
   * @param minHoldingPeriod The minimum holding period for a trade (in bars/days).
   * @param maxHoldingPeriod The maximum holding period for a trade (in bars/days).
   */
  StopTargetDetail (const Decimal& stopLoss, const Decimal& profitTarget,
		    unsigned int minHoldingPeriod, unsigned int maxHoldingPeriod)
    : mStop (stopLoss),
    mProfitTarget(profitTarget),
    mMinimumHoldingPeriod (minHoldingPeriod),
    mMaxHoldingPeriod (maxHoldingPeriod)
  {}

  /**
   * @brief Gets the stop-loss level.
   * @return The stop-loss level as a Decimal.
   */
  Decimal getStopLoss() const
  {
      return mStop;
  }
  
  /**
   * @brief Gets the profit-target level.
   * @return The profit-target level as a Decimal.
   */
  Decimal getProfitTarget() const
  {
    return mProfitTarget;
  }
  
  /**
   * @brief Gets the minimum holding period.
   * @return The minimum holding period in bars/days.
   */
  unsigned int getMinHoldingPeriod() const
  {
    return mMinimumHoldingPeriod;
  }
  
  /**
   * @brief Gets the maximum holding period.
   * @return The maximum holding period in bars/days.
   */
  unsigned int getMaxHoldingPeriod() const
  {
    return mMaxHoldingPeriod;
  }
  
 private:
  Decimal mStop;  ///< The stop-loss level.
  Decimal mProfitTarget; ///< The profit-target level.
  unsigned int mMinimumHoldingPeriod; ///< Minimum holding period in bars/days.
  unsigned int mMaxHoldingPeriod;     ///< Maximum holding period in bars/days.
};

/**
 * @brief Reads stop-target details from a CSV file.
 *
 * This class is designed to parse a CSV file containing stop-loss,
 * profit-target, and holding period information for two different
 * "deviation" levels (typically representing different volatility regimes
 * or strategy variations).
 *
 * The CSV file is expected to have a header and two data rows:
 * - Header: "Stop", "Target", "MinHold", "MaxHold"
 * - Row 1: Details for Deviation 1
 * - Row 2: Details for Deviation 2
 */
class StopTargetDetailReader
{
 public:
  /**
   * @brief Constructs a StopTargetDetailReader and parses the specified CSV file.
   * @param fileName The path to the CSV file containing stop-target details.
   *
   * The constructor reads two lines from the CSV:
   * - The first line is interpreted as details for "Deviation 1".
   * - The second line is interpreted as details for "Deviation 2".
   * It initializes mDev1Details and mDev2Details accordingly.
   */
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

    // Read details for Deviation 1
    csvConfigFile.read_row (stopLossStr, profitTargetStr, minHoldStr, maxHoldStr);
    stopLoss = num::fromString<Decimal>(stopLossStr);
    profitTarget = num::fromString<Decimal>(profitTargetStr);
    minHold = std::atoi (minHoldStr.c_str());
    maxHold = std::atoi (maxHoldStr.c_str());

    mDev1Details = StopTargetDetail (stopLoss, profitTarget, minHold, maxHold);

    // Read details for Deviation 2
    csvConfigFile.read_row (stopLossStr, profitTargetStr, minHoldStr, maxHoldStr);
    stopLoss = num::fromString<Decimal>(stopLossStr);
    profitTarget = num::fromString<Decimal>(profitTargetStr);
    minHold = std::atoi (minHoldStr.c_str());
    maxHold = std::atoi (maxHoldStr.c_str());

    mDev2Details = StopTargetDetail (stopLoss, profitTarget, minHold, maxHold);
  }

  /**
   * @brief Gets the stop-target details for Deviation 1.
   * @return A StopTargetDetail object for Deviation 1.
   */
  StopTargetDetail getDev1Detail() const
  {
    return mDev1Details;
  }

  /**
   * @brief Gets the stop-target details for Deviation 2.
   * @return A StopTargetDetail object for Deviation 2.
   */
  StopTargetDetail getDev2Detail() const
  {
    return mDev2Details;
  }


 private:
  StopTargetDetail mDev1Details; ///< Stop-target details for Deviation 1.
  StopTargetDetail mDev2Details; ///< Stop-target details for Deviation 2.
};

#endif
