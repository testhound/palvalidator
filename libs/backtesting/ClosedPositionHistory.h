// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#pragma once

#include <exception>
#include <memory>
#include <map>
#include <vector>
#include <cstdint>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include "TradingPosition.h"
#include "TimeSeriesEntry.h"
#include "StatUtils.h"
#include "TimeSeriesIndicators.h"
#include "PatternPositionRegistry.h"
#include "TradeResampling.h"

namespace mkc_timeseries
{
  using boost::accumulators::accumulator_set;
  using boost::accumulators::stats;
  using boost::accumulators::median;
  using boost::accumulators::mean;
  using boost::accumulators::sum;
  // TimeSeriesDate is boost::gregorian::date, ptime is boost::posix_time::ptime
  // We will use ptime as the key for the map.

  typedef boost::accumulators::tag::median median_tag;
  typedef boost::accumulators::tag::mean mean_tag;
  typedef boost::accumulators::tag::sum sum_tag;

  class ClosedPositionHistoryException : public std::runtime_error
  {
  public:
    ClosedPositionHistoryException(const std::string msg)
      : std::runtime_error(msg)
    {}

    ~ClosedPositionHistoryException()
    {}

  };

  template <class Decimal>
  struct ExpandedBarMetrics
  {
    Decimal closeToClose;
    Decimal openToClose;
    Decimal highToOpen;
    Decimal lowToOpen;
  };
  
  template <class Decimal> class ClosedPositionHistory
  {
  public:
    // Changed map key from TimeSeriesDate (boost::gregorian::date) to ptime
    typedef typename std::multimap<ptime,std::shared_ptr<TradingPosition<Decimal>>>::iterator PositionIterator;
    typedef typename std::multimap<ptime,std::shared_ptr<TradingPosition<Decimal>>>::const_iterator ConstPositionIterator;
    typedef std::vector<unsigned int>::const_iterator ConstBarsInPositionIterator;
    typedef std::vector<double>::const_iterator ConstTradeReturnIterator;

    ClosedPositionHistory()
      : mPositions(),
        mSumWinners(DecimalConstants<Decimal>::DecimalZero),
        mSumLosers(DecimalConstants<Decimal>::DecimalZero),
	mLogSumWinners(DecimalConstants<Decimal>::DecimalZero),
	mLogSumLosers(DecimalConstants<Decimal>::DecimalZero),
        mNumWinners(0),
        mNumLosers(0),
        mNumBarsInMarket(0),
        mRMultipleSum(DecimalConstants<Decimal>::DecimalZero),
        mWinnersStats(),
        mLosersStats(),
        mWinnersVect(),
        mLosersVect(),
        mBarsPerPosition(),
        mBarsPerWinningPosition(),
        mBarsPerLosingPosition(),
        mNumConsecutiveLosses(0)
    {}

    ClosedPositionHistory(const ClosedPositionHistory<Decimal>& rhs)
      : mPositions(rhs.mPositions),
        mSumWinners(rhs.mSumWinners),
        mSumLosers(rhs.mSumLosers),
	mLogSumWinners(rhs.mLogSumWinners),
	mLogSumLosers(rhs.mLogSumLosers),
        mNumWinners(rhs.mNumWinners),
        mNumLosers(rhs.mNumLosers),
        mNumBarsInMarket(rhs.mNumBarsInMarket),
        mRMultipleSum(rhs.mRMultipleSum),
        mWinnersStats(rhs.mWinnersStats),
        mLosersStats(rhs.mLosersStats),
        mWinnersVect(rhs.mWinnersVect),
        mLosersVect(rhs.mLosersVect),
        mBarsPerPosition(rhs.mBarsPerPosition),
        mBarsPerWinningPosition(rhs.mBarsPerWinningPosition),
        mBarsPerLosingPosition(rhs.mBarsPerLosingPosition),
        mNumConsecutiveLosses(rhs.mNumConsecutiveLosses)
    {}

    ClosedPositionHistory<Decimal>&
    operator=(const ClosedPositionHistory<Decimal> &rhs)
    {
      if (this == &rhs)
        return *this;

      mPositions = rhs.mPositions;
      mSumWinners = rhs.mSumWinners;
      mSumLosers = rhs.mSumLosers;
      mLogSumWinners = rhs.mLogSumWinners;
      mLogSumLosers = rhs.mLogSumLosers;
      mNumWinners = rhs.mNumWinners;
      mNumLosers = rhs.mNumLosers;
      mNumBarsInMarket = rhs.mNumBarsInMarket;
      mRMultipleSum = rhs.mRMultipleSum;
      mWinnersStats = rhs.mWinnersStats;
      mLosersStats = rhs.mLosersStats;
      mWinnersVect = rhs.mWinnersVect;
      mLosersVect = rhs.mLosersVect;
      mBarsPerPosition = rhs.mBarsPerPosition;
      mBarsPerWinningPosition = rhs.mBarsPerWinningPosition;
      mBarsPerLosingPosition = rhs.mBarsPerLosingPosition;
      mNumConsecutiveLosses = rhs.mNumConsecutiveLosses;

      return *this;
    }

    ClosedPositionHistory(ClosedPositionHistory<Decimal>&& rhs) noexcept
      : mPositions(std::move(rhs.mPositions)),
	mSumWinners(std::move(rhs.mSumWinners)),
	mSumLosers(std::move(rhs.mSumLosers)),
	mLogSumWinners(std::move(rhs.mLogSumWinners)),
	mLogSumLosers(std::move(rhs.mLogSumLosers)),
	mNumWinners(rhs.mNumWinners),
	mNumLosers(rhs.mNumLosers),
	mNumBarsInMarket(rhs.mNumBarsInMarket),
	mRMultipleSum(std::move(rhs.mRMultipleSum)),
	mWinnersStats(std::move(rhs.mWinnersStats)),
	mLosersStats(std::move(rhs.mLosersStats)),
	mWinnersVect(std::move(rhs.mWinnersVect)),
	mLosersVect(std::move(rhs.mLosersVect)),
	mBarsPerPosition(std::move(rhs.mBarsPerPosition)),
	mBarsPerWinningPosition(std::move(rhs.mBarsPerWinningPosition)),
	mBarsPerLosingPosition(std::move(rhs.mBarsPerLosingPosition)),
	mNumConsecutiveLosses(rhs.mNumConsecutiveLosses)
    {
      // Reset rhs to valid empty state
      rhs.mNumWinners = 0;
      rhs.mNumLosers = 0;
      rhs.mNumBarsInMarket = 0;
      rhs.mNumConsecutiveLosses = 0;
      rhs.mSumWinners = DecimalConstants<Decimal>::DecimalZero;
      rhs.mSumLosers = DecimalConstants<Decimal>::DecimalZero;
      rhs.mLogSumWinners = DecimalConstants<Decimal>::DecimalZero;
      rhs.mLogSumLosers = DecimalConstants<Decimal>::DecimalZero;
      rhs.mRMultipleSum = DecimalConstants<Decimal>::DecimalZero;
    }

    // ============================================================================
    // MOVE ASSIGNMENT OPERATOR
    // ============================================================================
    // Add this after the copy assignment operator (around line 134)

    ClosedPositionHistory<Decimal>&
    operator=(ClosedPositionHistory<Decimal>&& rhs) noexcept
    {
      if (this == &rhs)
	return *this;

      // Move all member variables
      mPositions = std::move(rhs.mPositions);
      mSumWinners = std::move(rhs.mSumWinners);
      mSumLosers = std::move(rhs.mSumLosers);
      mLogSumWinners = std::move(rhs.mLogSumWinners);
      mLogSumLosers = std::move(rhs.mLogSumLosers);
      mNumWinners = rhs.mNumWinners;
      mNumLosers = rhs.mNumLosers;
      mNumBarsInMarket = rhs.mNumBarsInMarket;
      mRMultipleSum = std::move(rhs.mRMultipleSum);
      mWinnersStats = std::move(rhs.mWinnersStats);
      mLosersStats = std::move(rhs.mLosersStats);
      mWinnersVect = std::move(rhs.mWinnersVect);
      mLosersVect = std::move(rhs.mLosersVect);
      mBarsPerPosition = std::move(rhs.mBarsPerPosition);
      mBarsPerWinningPosition = std::move(rhs.mBarsPerWinningPosition);
      mBarsPerLosingPosition = std::move(rhs.mBarsPerLosingPosition);
      mNumConsecutiveLosses = rhs.mNumConsecutiveLosses;

      // Reset rhs to valid empty state
      rhs.mNumWinners = 0;
      rhs.mNumLosers = 0;
      rhs.mNumBarsInMarket = 0;
      rhs.mNumConsecutiveLosses = 0;
      rhs.mSumWinners = DecimalConstants<Decimal>::DecimalZero;
      rhs.mSumLosers = DecimalConstants<Decimal>::DecimalZero;
      rhs.mLogSumWinners = DecimalConstants<Decimal>::DecimalZero;
      rhs.mLogSumLosers = DecimalConstants<Decimal>::DecimalZero;
      rhs.mRMultipleSum = DecimalConstants<Decimal>::DecimalZero;

      return *this;
    }
    
    ~ClosedPositionHistory()
    {}

    void addClosedPosition(std::shared_ptr<TradingPosition<Decimal>> position)
    {
      if (position->isPositionOpen())
        throw ClosedPositionHistoryException ("ClosedPositionHistory:addClosedPosition - cannot add open position");

      // Changed to use ptime and getEntryDateTime() for the map key
      ptime dt = position->getEntryDateTime(); //

      unsigned int holdingPeriod = position->getNumBarsInPosition();
      mBarsPerPosition.push_back (holdingPeriod);
      mNumBarsInMarket += position->getNumBarsInPosition();

      if (position->RMultipleStopSet())
	{
	  // If the R multiple stop equals entry, risk distance is zero → avoid division by zero
	  if (position->getRMultipleStop() != position->getEntryPrice())
	    {
	      mRMultipleSum += position->getRMultiple();
	    }
	  // else: skip contributing to expectancy; R is undefined at breakeven
	}

      // Insert using the ptime key
      mPositions.insert(std::make_pair(dt, position));

      Decimal percReturn (position->getPercentReturn());

      if (position->isWinningPosition())
        {
          mNumWinners++;
          mSumWinners += position->getPercentReturn();
          
          // Safely compute log trade return, handling edge cases
          try {
            mLogSumWinners += position->getLogTradeReturn();
          } catch (const std::domain_error& e) {
            // Log the error with position details for debugging
            std::cerr << "Warning: Failed to compute log trade return for winning position "
                      << position->getPositionID() << " (" << position->getTradingSymbol() << "): "
                      << e.what() << std::endl;
            std::cerr << "  Entry Price: " << position->getEntryPrice()
                      << ", Exit Price: " << position->getExitPrice()
                      << ", Trade Return: " << position->getTradeReturn() << std::endl;
            
            // Use safe approximation based on the percent return using log(1+r) formula
            Decimal safeLogReturn = std::log(DecimalConstants<Decimal>::DecimalOne + position->getTradeReturn());
            mLogSumWinners += safeLogReturn;
          }
          
          mWinnersStats (num::to_double(position->getPercentReturn()));
          mWinnersVect.push_back(num::to_double(position->getPercentReturn()));
          mBarsPerWinningPosition.push_back (position->getNumBarsInPosition());
          
          // Reset consecutive losses counter on winning position
          mNumConsecutiveLosses = 0;
        }
      else if (position->isLosingPosition())
        {
          mNumLosers++;
          mSumLosers += position->getPercentReturn();
          
          // Safely compute log trade return, handling edge cases
          try {
            mLogSumLosers += position->getLogTradeReturn();
          } catch (const std::domain_error& e) {
            // Log the error with position details for debugging
            std::cerr << "Warning: Failed to compute log trade return for losing position "
                      << position->getPositionID() << " (" << position->getTradingSymbol() << "): "
                      << e.what() << std::endl;
            std::cerr << "  Entry Price: " << position->getEntryPrice()
                      << ", Exit Price: " << position->getExitPrice()
                      << ", Trade Return: " << position->getTradeReturn() << std::endl;
            
            // Use safe approximation based on the percent return using log(1+r) formula
            Decimal safeLogReturn = std::log(DecimalConstants<Decimal>::DecimalOne + position->getTradeReturn());
            mLogSumLosers += safeLogReturn;
          }
          
          mLosersStats (num::to_double(percReturn));
          mLosersVect.push_back(num::to_double(num::abs(percReturn)));
          mBarsPerLosingPosition.push_back (position->getNumBarsInPosition());
          
          // Increment consecutive losses counter on losing position
          mNumConsecutiveLosses++;
        }
      else
        throw std::logic_error(std::string("ClosedPositionHistory:addClosedPosition - position not winner or lsoer"));
    }

    void addClosedPosition (const TradingPositionLong<Decimal>& position)
    {
      addClosedPosition (std::make_shared<TradingPositionLong<Decimal>>(position));
    }

    void addClosedPosition (const TradingPositionShort<Decimal>& position)
    {
      addClosedPosition (std::make_shared<TradingPositionShort<Decimal>>(position));
    }

    const Decimal getRMultipleExpectancy() const
    {
      uint32_t numPos = getNumPositions();

      if ((numPos > 0) && (mRMultipleSum > DecimalConstants<Decimal>::DecimalZero))
        return mRMultipleSum / Decimal(numPos);
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    uint32_t getNumPositions() const
    {
      return mPositions.size();
    }

    uint32_t getNumEntriesInBarsPerPosition() const
    {
      return mBarsPerPosition.size();
    }

    uint32_t getNumWinningPositions() const
    {
      return mNumWinners;
    }

    uint32_t getNumLosingPositions() const
    {
      return mNumLosers;
    }

    uint32_t getNumBarsInMarket() const
    {
      return mNumBarsInMarket;
    }

    ////
    unsigned int getMedianHoldingPeriod() const
    {
      if (mBarsPerPosition.empty())
        return 0;

      // Convert to double vector to avoid integer division truncation
      std::vector<double> doubleValues;
      doubleValues.reserve(mBarsPerPosition.size());
      for (unsigned int val : mBarsPerPosition) {
        doubleValues.push_back(static_cast<double>(val));
      }
      
      // Use the Median function from TimeSeriesIndicators.h with double precision
      double medianValue = Median(doubleValues);
      
      // Round to nearest integer (same behavior as original llround)
      return static_cast<unsigned int>(std::llround(medianValue));
    }


    ////
    uint32_t getNumConsecutiveLosses() const
    {
      return mNumConsecutiveLosses;
    }
    
    /**
     * @brief Extract high-resolution bar-by-bar returns from all closed trades.
     *
     * @details
     * Iterates through each closed `TradingPosition` and its bar history to
     * compute per-bar returns. This method provides a more accurate return
     * calculation for the final bar of the trade by using the actual `exitPrice`
     * from the `TradingPosition` instead of the bar's close price. This ensures
     * the return series perfectly reflects the realized P&L of the strategy.
     *
     * For all intermediate bars, a standard mark-to-market (close-to-reference)
     * return is used. The sequence of returns is:
     * 1. (First Bar Close - Entry Price) / Entry Price
     * 2. (Second Bar Close - First Bar Close) / First Bar Close
     * 3. ...
     * 4. (Exit Price - Last Bar's Previous Close) / Last Bar's Previous Close
     *
     * @return Vector of Decimal returns, one entry per bar across all closed trades.
     */
    std::vector<Decimal> getHighResBarReturns() const
    {
        std::vector<Decimal> allReturns;
        // Iterate through each closed position in the history
        for (auto it = mPositions.begin(); it != mPositions.end(); ++it)
        {
            const auto& pos = it->second; // pos is a std::shared_ptr<TradingPosition<Decimal>>

            auto bar_it = pos->beginPositionBarHistory();
            auto bar_end = pos->endPositionBarHistory();

            if (bar_it == bar_end) {
                continue; // Skip positions with no bar history
            }

            // The first reference price is the actual entry price of the trade
            Decimal prevReferencePrice = pos->getEntryPrice();

            // Loop through all bars that were recorded while the position was open
            for (; bar_it != bar_end; ++bar_it)
            {
                const auto& currentBar = bar_it->second;
                Decimal returnForThisBar;

                // Check if this is the last bar in the position's recorded history
                if (std::next(bar_it) == bar_end)
                {
                    // For the final bar, the return is calculated to the actual exit price
                    Decimal exitPrice = pos->getExitPrice();
                    if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                        returnForThisBar = (exitPrice - prevReferencePrice) / prevReferencePrice;
                    else
                        returnForThisBar = DecimalConstants<Decimal>::DecimalZero;

                }
                else
                {
                    // For all intermediate bars, calculate the mark-to-market return (close-to-reference)
                    Decimal currentClose = currentBar.getCloseValue();
                     if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                        returnForThisBar = (currentClose - prevReferencePrice) / prevReferencePrice;
                    else
                        returnForThisBar = DecimalConstants<Decimal>::DecimalZero;


                    // The reference for the next bar becomes the close of the current bar
                    prevReferencePrice = currentClose;
                }

                // For short positions, a decrease in price is a gain, so we invert the return.
                if (pos->isShortPosition())
                {
                    returnForThisBar *= -1;
                }

                allReturns.push_back(returnForThisBar);
            }
        }
        return allReturns;
    }

    /**
     * @brief Extracts returns as a vector of Trade objects.
     * 
     * Each Trade contains the contiguous mark-to-market daily returns for one 
     * closed position. Open positions are excluded.
     *
     * ENTRY CONVENTION: This implementation assumes entry occurs at the open 
     * of the bar following the signal. The first bar return is computed as:
     *   (first_bar_close - entry_open) / entry_open
     * This correctly captures the full intrabar price movement from entry to 
     * first close, with no inflation of duration.
     *
     * EXIT CONVENTION: The last bar return is computed using the actual exit 
     * price (limit fill, stop fill, or next-open market exit) rather than 
     * the bar close, ensuring the trade's terminal return is accurate.
     *
     * @return Vector of Trade objects, one per closed position, in chronological order.
     */
    std::vector<Trade<Decimal>>
    getTradeLevelReturns(bool applyCosts = false,
			 Decimal costPerSide = DecimalConstants<Decimal>::DefaultEquitySlippage) const
    {
      const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
      const Decimal one  = DecimalConstants<Decimal>::DecimalOne;

      if (applyCosts)
	{
	  if (costPerSide < zero || costPerSide >= one)
	    throw std::domain_error("getTradeLevelReturns: costPerSide must be in [0, 1).");
      }

      std::vector<Trade<Decimal>> tradeReturns;
      tradeReturns.reserve(mPositions.size());

      const Decimal minusOne = DecimalConstants<Decimal>::DecimalMinusOne;

      // Effective execution prices that apply cost ONCE per side (entry and exit).
      // costPerSide is a fraction (0.001 = 0.1%).
      auto getEffectiveEntryPrice = [&](auto const& positionPtr) -> Decimal
      {
	const Decimal rawEntry = positionPtr->getEntryPrice();
	if (!applyCosts)
	  return rawEntry;

	// Long: pay more on entry (worse) => entry*(1+c)
	// Short: receive less on sell (worse) => entry*(1-c)
	return positionPtr->isShortPosition()
	  ? rawEntry * (one - costPerSide)
	  : rawEntry * (one + costPerSide);
      };

      auto getEffectiveExitPrice = [&](auto const& positionPtr) -> Decimal
      {
	const Decimal rawExit = positionPtr->getExitPrice();
	if (!applyCosts)
	  return rawExit;

	// Long: receive less on exit (worse) => exit*(1-c)
	// Short: pay more to buy-to-cover (worse) => exit*(1+c)
	return positionPtr->isShortPosition()
	  ? rawExit * (one + costPerSide)
	  : rawExit * (one - costPerSide);
      };

      for (auto const& [ptime, pos] : mPositions)
	{
	  std::vector<Decimal> dailySequence;

	  const bool sameBarPosition = (pos->getEntryDateTime() == pos->getExitDateTime());

	  if (sameBarPosition)
	    {
	      const Decimal entryRef = getEffectiveEntryPrice(pos);
	      const Decimal exitPx   = getEffectiveExitPrice(pos);

	      if (entryRef == zero)
		throw std::domain_error("getTradeLevelReturns: effective entry price is zero.");

	      Decimal r = (exitPx - entryRef) / entryRef;

	      // Convert price-return to P&L-return for shorts
	      if (pos->isShortPosition())
		r *= minusOne;

	      dailySequence.push_back(r);
	    }
	  else
	    {
	      Decimal prevRef = getEffectiveEntryPrice(pos);
	      if (prevRef == zero)
		throw std::domain_error("getTradeLevelReturns: effective entry price is zero.");

	      for (auto bar_it = pos->beginPositionBarHistory();
		   bar_it != pos->endPositionBarHistory();
		   ++bar_it)
		{
		  Decimal r = zero;
		  const bool isLastBar = (std::next(bar_it) == pos->endPositionBarHistory());

		  if (isLastBar)
		    {
		      const Decimal exitPx = getEffectiveExitPrice(pos);
		      r = (exitPx - prevRef) / prevRef;
		    }
		  else
		    {
		      const Decimal closePx = bar_it->second.getCloseValue();
		      r = (closePx - prevRef) / prevRef;
		      prevRef = closePx;

		      if (prevRef == zero) {
			throw std::domain_error("getTradeLevelReturns: bar close is zero.");
		      }
		    }

		  if (pos->isShortPosition()) r *= minusOne;
		  dailySequence.push_back(r);
		}
	    }

	  if (!dailySequence.empty())
	    {
	      // Uses Trade(std::vector<Decimal>&&) — perfect for your class
	      tradeReturns.emplace_back(std::move(dailySequence));
	    }
	}

      return tradeReturns;
    }
    
    std::vector<std::pair<boost::posix_time::ptime, Decimal>>
    getHighResBarReturnsWithDates() const
    {
      using boost::posix_time::ptime;
      std::vector<std::pair<ptime, Decimal>> all;

      // Iterate every closed position (keyed by entry ptime)
      for (auto it = mPositions.begin(); it != mPositions.end(); ++it)
	{
	  const auto& pos = it->second;

	  // Walk the recorded bar history for the position
	  auto bar_it  = pos->beginPositionBarHistory();
	  auto bar_end = pos->endPositionBarHistory();
	  if (bar_it == bar_end)
            continue;

	  // First reference is the actual entry price (so the first return
	  // is entry→1st-close; final return is last-ref→exitPrice)
	  Decimal prevReferencePrice = pos->getEntryPrice();

	  for (; bar_it != bar_end; ++bar_it)
	    {
	      const auto& tsBar   = bar_it->first;   // ptime timestamp
	      const auto& barInfo = bar_it->second;  // OpenPositionBar<Decimal>
	      Decimal returnForBar;

	      // If this is the final recorded bar for the position, we compute to the *exit price*
	      if (std::next(bar_it) == bar_end)
		{
		  const Decimal exitPrice = pos->getExitPrice();
		  if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                    returnForBar = (exitPrice - prevReferencePrice) / prevReferencePrice;
		  else
                    returnForBar = DecimalConstants<Decimal>::DecimalZero;

		  // Record the return at the *exit* timestamp to reflect realized P&L timing
		  const auto& exitTimeStamp = pos->getExitDateTime();
		  if (pos->isShortPosition())
                    returnForBar *= -1;

		  all.emplace_back(exitTimeStamp, returnForBar);
		}
	      else
		{
		  // Intermediate bars: mark-to-market using the bar close
		  const Decimal currentClose = barInfo.getCloseValue();
		  if (prevReferencePrice != DecimalConstants<Decimal>::DecimalZero)
                    returnForBar = (currentClose - prevReferencePrice) / prevReferencePrice;
		  else
                    returnForBar = DecimalConstants<Decimal>::DecimalZero;

		  if (pos->isShortPosition())
                    returnForBar *= -1;

		  all.emplace_back(tsBar, returnForBar);

		  // Next reference is the current close
		  prevReferencePrice = currentClose;
		}
	    }
	}
      return all;
    }
    
    std::vector<ExpandedBarMetrics<Decimal>> getExpandedHighResBarReturns() const
    {
      std::vector<ExpandedBarMetrics<Decimal>> result;

      for (const auto& posEntry : mPositions)
	{
	  const auto& pos = posEntry.second;
	  auto barIt = pos->beginPositionBarHistory();
	  auto endIt = pos->endPositionBarHistory();

	  if (barIt == endIt)
            continue;

	  auto prev = barIt;
	  for (auto curr = std::next(barIt); curr != endIt; ++curr)
	    {
	      const auto& prevBar = prev->second;
	      const auto& bar = curr->second;

	      Decimal prevClose = prevBar.getCloseValue();
	      Decimal open = bar.getOpenValue();
	      Decimal high = bar.getHighValue();
	      Decimal low = bar.getLowValue();
	      Decimal close = bar.getCloseValue();

	      if (prevClose == Decimal(0))
                continue;

	      ExpandedBarMetrics<Decimal> metrics;
	      metrics.closeToClose = (close - prevClose) / prevClose;
	      metrics.openToClose = (close - open) / open;
	      metrics.highToOpen  = (high - open) / open;
	      metrics.lowToOpen   = (low - open) / open;

	      result.push_back(metrics);
	      prev = curr;
	    }
	}

      return result;
    }

    Decimal getAverageWinningTrade() const
    {
      if (mNumWinners >= 1)
        return (Decimal(mSumWinners) /Decimal(mNumWinners));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricMean(std::vector<double> const&data) const
    {
      const double too_large = 1.e64;
      const double too_small = 1.e-64;
      double sum_log = 0.0;
      double product = 1.0;
      for(auto x:data) {
          product *= x;
          if(product > too_large || product < too_small) {
              sum_log+= std::log(product);
              product = 1;
            }
        }
      return (Decimal (std::exp((sum_log + std::log(product))/data.size())));
    }

    Decimal getGeometricWinningTrade() const
    {
      if (mNumWinners >= 1)
        return (Decimal (getGeometricMean (mWinnersVect)));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianWinningTrade() const
    {
      if (mNumWinners >= 1)
      {
        // Use the vector data for a more reliable median calculation
        std::vector<double> sortedWinners = mWinnersVect;
        std::sort(sortedWinners.begin(), sortedWinners.end());
        
        size_t n = sortedWinners.size();
        if (n % 2 == 1)
        {
          // Odd number of elements: return middle element
          return Decimal(sortedWinners[n/2]);
        }
        else
        {
          // Even number of elements: return average of two middle elements
          return Decimal((sortedWinners[n/2 - 1] + sortedWinners[n/2]) / 2.0);
        }
      }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getAverageLosingTrade() const
    {
      if (mNumLosers >= 1)
        return (Decimal(mSumLosers) /Decimal(mNumLosers));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricLosingTrade() const
    {
      if (mNumLosers >= 1)
        return (Decimal (getGeometricMean (mLosersVect)));
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianLosingTrade() const
    {
      if (mNumLosers >= 1)
      {
        // Use the vector data for a more reliable median calculation
        // Note: mLosersVect contains absolute values, but we need to return negative values for losses
        std::vector<double> sortedLosers = mLosersVect;
        std::sort(sortedLosers.begin(), sortedLosers.end());
        
        size_t n = sortedLosers.size();
        double medianValue;
        if (n % 2 == 1)
        {
          // Odd number of elements: return middle element
          medianValue = sortedLosers[n/2];
        }
        else
        {
          // Even number of elements: return average of two middle elements
          medianValue = (sortedLosers[n/2 - 1] + sortedLosers[n/2]) / 2.0;
        }
        
        // Return as negative since these are losses and mLosersVect stores absolute values
        return Decimal(-medianValue);
      }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPercentWinners() const
    {
      if (getNumPositions() > 0)
        return ((Decimal(mNumWinners) / Decimal(getNumPositions())) *
                DecimalConstants<Decimal>::DecimalOneHundred);
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPercentLosers() const
    {
      if (getNumPositions() > 0)
        return ((Decimal(mNumLosers) / Decimal(getNumPositions())) *
                DecimalConstants<Decimal>::DecimalOneHundred);
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPayoffRatio() const
    {
      if (getNumPositions() > 0)
        {
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
            {
              Decimal avgLoser = num::abs(getAverageLosingTrade());
              if (avgLoser != DecimalConstants<Decimal>::DecimalZero)
                return (getAverageWinningTrade() / avgLoser);
              else
                return (getAverageWinningTrade());
            }
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (getAverageWinningTrade());
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getPayoffRatio - getNumPositions > 0 error"));

        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricPayoffRatio() const
    {
      if (getNumPositions() > 0)
        {
          if (getGeometricLosingTrade() == DecimalConstants<Decimal>::DecimalZero)
            return getPayoffRatio();
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
            return (getGeometricWinningTrade() / getGeometricLosingTrade());
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (getGeometricWinningTrade());
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getGeometricPayoffRatio - getNumPositions > 0 error"));

        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianPayoffRatio() const
    {
      if (getNumPositions() > 0)
        {
          if (getMedianLosingTrade() == DecimalConstants<Decimal>::DecimalZero)
            return getPayoffRatio();
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
            return (getMedianWinningTrade() / num::abs(getMedianLosingTrade()));
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (getMedianWinningTrade());
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getMedianPayoffRatio - getNumPositions > 0 error"));

        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getPessimisticReturnRatio() const
    {
      if (getNumPositions() > 0)
        {
          if ((mNumWinners == 0) || (mNumWinners == 1))
            return (DecimalConstants<Decimal>::DecimalZero);

          Decimal numTrades(getNumPositions());
          Decimal numerator = (((Decimal(mNumWinners) -
                                 DecimalSqrtConstants<Decimal>::getSqrt (mNumWinners))/numTrades)
                               * getMedianWinningTrade());

          if (mNumLosers == 0)
            return numerator;

          Decimal denominator = (((Decimal(mNumLosers) +
                                   DecimalSqrtConstants<Decimal>::getSqrt (mNumLosers))/numTrades)
                                 * num::abs(getMedianLosingTrade()));

          if (denominator == DecimalConstants<Decimal>::DecimalZero)
            return numerator;
          else
            return numerator / denominator;
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getProfitFactorCommon(const Decimal& winnersSum, const Decimal& losersSum) const
    {
      if (getNumPositions() > 0)
        {
          if ((mNumWinners >= 1) and (mNumLosers >= 1))
	    {
	      if (num::abs(losersSum) == DecimalConstants<Decimal>::DecimalZero)
		return (DecimalConstants<Decimal>::DecimalOneHundred);
	      else
		return (winnersSum / num::abs(losersSum));
	    }
          else if (mNumWinners == 0)
            return (DecimalConstants<Decimal>::DecimalZero);
          else if (mNumLosers == 0)
            return (DecimalConstants<Decimal>::DecimalOneHundred);
          else
            throw std::logic_error(std::string("ClosedPositionHistory:getProfitFactor - getNumPositions > 0 error"));
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);

    }
    
    Decimal getProfitFactor() const
    {
      return getProfitFactorCommon(mSumWinners, mSumLosers);
    }

    // Add this calculation (natural log of trade returns) which is the preferred
    // method of Timothy Master's
    // as described in his book "Permutation and Randomization Tests for
    // Trading System Development
    Decimal getLogProfitFactor() const
    {
      return getProfitFactorCommon(mLogSumWinners, mLogSumLosers);
    }

    Decimal getHighResProfitFactor() const
    {
      auto returns = getHighResBarReturns();
      return StatUtils<Decimal>::computeProfitFactor(returns, false);
    }

    Decimal getHighResProfitability() const
    {
      auto returns = getHighResBarReturns();
      auto [pf, profitability] = StatUtils<Decimal>::computeProfitability(returns);
      return profitability;
    }
    
    Decimal getPALProfitability() const
    {
      if (getNumPositions() > 0)
        {
          Decimal pf(getProfitFactor());
          Decimal payoffRatio(getPayoffRatio());

          Decimal denominator (pf + payoffRatio);
          if (denominator > DecimalConstants<Decimal>::DecimalZero)
            return ((pf/denominator) * DecimalConstants<Decimal>::DecimalOneHundred);
          else
            return (DecimalConstants<Decimal>::DecimalZero);
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getMedianPALProfitability() const
    {
      if (getNumPositions() > 0)
        {
          Decimal pf(getProfitFactor());
          Decimal payoffRatio(getMedianPayoffRatio());

          Decimal denominator (pf + payoffRatio);
          if (denominator > DecimalConstants<Decimal>::DecimalZero)
            {
              Decimal ratio(pf/denominator);
              return (ratio * DecimalConstants<Decimal>::DecimalOneHundred);
            }
          else
            return (DecimalConstants<Decimal>::DecimalZero);
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }

    Decimal getGeometricPALProfitability() const
    {
      if (getNumPositions() > 0)
        {
          Decimal pf(getProfitFactor());
          Decimal payoffRatio(getGeometricPayoffRatio());

          Decimal denominator (pf + payoffRatio);
          if (denominator > DecimalConstants<Decimal>::DecimalZero)
            {
              Decimal ratio(pf/denominator);
              return (ratio * DecimalConstants<Decimal>::DecimalOneHundred);
            }
          else
            return (DecimalConstants<Decimal>::DecimalZero);
        }
      else
        return (DecimalConstants<Decimal>::DecimalZero);
    }


    Decimal getCumulativeReturn() const
    {
      Decimal cumReturn(0);

      // Iterator type changes due to typedef
      ClosedPositionHistory::ConstPositionIterator it = beginTradingPositions();
      if (it != endTradingPositions())
        {
          cumReturn = it->second->getTradeReturnMultiplier();
          it++;

          for (; it != endTradingPositions(); it++)
            {
              cumReturn = cumReturn * it->second->getTradeReturnMultiplier();
            }

          cumReturn = cumReturn - DecimalConstants<Decimal>::DecimalOne;
        }

      return cumReturn;
    }

    // Return type changes due to typedef
    ClosedPositionHistory::ConstPositionIterator beginTradingPositions() const
    {
      return mPositions.begin();
    }

    // Return type changes due to typedef
    ClosedPositionHistory::ConstPositionIterator endTradingPositions() const
    {
      return mPositions.end();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator beginBarsPerPosition() const
    {
      return mBarsPerPosition.begin();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator endBarsPerPosition() const
    {
      return mBarsPerPosition.end();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator beginBarsPerWinningPosition() const
    {
      return mBarsPerWinningPosition.begin();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator endBarsPerWinningPosition() const
    {
      return mBarsPerWinningPosition.end();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator beginBarsPerLosingPosition() const
    {
      return mBarsPerLosingPosition.begin();
    }

    ClosedPositionHistory::ConstBarsInPositionIterator endBarsPerLosingPosition() const
    {
      return mBarsPerLosingPosition.end();
    }

    ClosedPositionHistory::ConstTradeReturnIterator beginWinnersReturns() const
    {
      return mWinnersVect.begin();
    }

    ClosedPositionHistory::ConstTradeReturnIterator endWinnersReturns() const
    {
      return mWinnersVect.end();
    }

    ClosedPositionHistory::ConstTradeReturnIterator beginLosersReturns() const
    {
      return mLosersVect.begin();
    }

    ClosedPositionHistory::ConstTradeReturnIterator endLosersReturns() const
    {
      return mLosersVect.end();
    }

    /**
     * @brief Get the pattern associated with a position (PRIMARY REQUIREMENT)
     * @param position The trading position to look up
     * @return Shared pointer to the associated pattern, or nullptr if none exists
     */
    std::shared_ptr<PriceActionLabPattern>
    getPatternForPosition(std::shared_ptr<TradingPosition<Decimal>> position) const
    {
      if (!position) {
        return nullptr;
      }
      return PatternPositionRegistry::getInstance().getPatternForPosition(position->getPositionID());
    }

    /**
     * @brief Get all closed positions associated with a specific pattern
     * @param pattern The pattern to look up
     * @return Vector of positions associated with the pattern
     */
    std::vector<std::shared_ptr<TradingPosition<Decimal>>>
    getPositionsForPattern(std::shared_ptr<PriceActionLabPattern> pattern) const
    {
      if (!pattern) {
        return std::vector<std::shared_ptr<TradingPosition<Decimal>>>();
      }
      
      auto& registry = PatternPositionRegistry::getInstance();
      auto positionIDs = registry.getPositionsForPattern(pattern);
      
      std::vector<std::shared_ptr<TradingPosition<Decimal>>> result;
      for (uint32_t positionID : positionIDs) {
        // Find position by ID in mPositions
        for (const auto& entry : mPositions) {
          if (entry.second->getPositionID() == positionID) {
            result.push_back(entry.second);
            break;
          }
        }
      }
      return result;
    }

    /**
     * @brief Get all closed positions that have associated patterns
     * @return Vector of positions with patterns
     */
    std::vector<std::shared_ptr<TradingPosition<Decimal>>>
    getPositionsWithPatterns() const
    {
      std::vector<std::shared_ptr<TradingPosition<Decimal>>> result;
      auto& registry = PatternPositionRegistry::getInstance();
      
      for (const auto& entry : mPositions) {
        if (registry.hasPatternForPosition(entry.second->getPositionID())) {
          result.push_back(entry.second);
        }
      }
      return result;
    }

    /**
     * @brief Get count of positions with patterns
     * @return Number of positions that have associated patterns
     */
    size_t getPositionCountWithPatterns() const
    {
      size_t count = 0;
      auto& registry = PatternPositionRegistry::getInstance();
      
      for (const auto& entry : mPositions) {
        if (registry.hasPatternForPosition(entry.second->getPositionID())) {
          count++;
        }
      }
      return count;
    }

  private:
    // Changed map key from TimeSeriesDate to ptime
    std::multimap<ptime,std::shared_ptr<TradingPosition<Decimal>>> mPositions;
    Decimal mSumWinners;
    Decimal mSumLosers;
    Decimal mLogSumWinners;
    Decimal mLogSumLosers;
    unsigned int mNumWinners;
    unsigned int mNumLosers;
    unsigned int mNumBarsInMarket;
    Decimal mRMultipleSum;
    accumulator_set<double, stats<median_tag>> mWinnersStats;
    accumulator_set<double, stats<median_tag>> mLosersStats;
    std::vector<double> mWinnersVect;
    std::vector<double> mLosersVect;

    // Vector that holds total number of bars for each position
    std::vector<unsigned int> mBarsPerPosition;
    std::vector<unsigned int> mBarsPerWinningPosition;
    std::vector<unsigned int> mBarsPerLosingPosition;
    
    // Number of consecutive losing trades
    unsigned int mNumConsecutiveLosses;
  };
}

