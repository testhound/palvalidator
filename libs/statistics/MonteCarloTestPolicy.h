// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __MONTE_CARLO_POLICY_H
#define __MONTE_CARLO_POLICY_H 1

#include <exception>
#include <string>
#include <memory>
#include "number.h"
#include "DecimalConstants.h"
#include "BackTester.h"
#include "StatUtils.h"

namespace mkc_timeseries
{
  // Forward declaration
  template <class Decimal> class PalStrategy;

  template <class Decimal>
  class AllHighResLogPFPolicy
  {
  public:
    /**
     * @brief Compute the permutation‐test statistic as the log‐profit‐factor
     *        over every bar‐by‐bar return (closed + open) for the sole strategy.
     */

    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      // We expect exactly one strategy per cloned backtester:
      if (bt->getNumStrategies() != 1) {
	      throw BackTesterException(
				  "AllHighResLogPFPolicy::getPermutationTestStatistic - "
				  "expected one strategy, got " 
				  + std::to_string(bt->getNumStrategies()));
      }

      // --- NEW: Enforce minimum activity thresholds ---
      const unsigned int minTradesRequired = AllHighResLogPFPolicy<Decimal>::getMinStrategyTrades();
      const unsigned int minBarsRequired = 10;
      
      // Get the total number of trades for the backtest run.
      uint32_t numTrades = bt->getNumTrades();
      
      // Pull every bar‐by‐bar return (entry→exit and any still‐open):
      std::vector<Decimal> barSeries = bt->getAllHighResReturns((*(bt->beginStrategies())).get());
      
      // If the thresholds are not met, return a neutral (zero) statistic.
      if (numTrades < minTradesRequired || barSeries.size() < minBarsRequired)
      {
          return DecimalConstants<Decimal>::DecimalZero;
      }

      // If thresholds are met, compute the statistic as before.
      return StatUtils<Decimal>::computeLogProfitFactor(barSeries, false);
    }

    /// Minimum number of closed trades required to even attempt this test
    static unsigned int getMinStrategyTrades() { return 3; }

    // Return a test statistic constant if we don't meet the minimum trade criteria
    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };

  template <class Decimal>
  class BootStrappedProfitFactorPolicy
  {
  public:
    /**
     * @brief Compute the permutation‐test statistic as the log‐profit‐factor
     *        over every bar‐by‐bar return (closed + open) for the sole strategy.
     */

    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      // We expect exactly one strategy per cloned backtester:
      if (bt->getNumStrategies() != 1) {
	      throw BackTesterException(
				  "AllHighResLogPFPolicy::getPermutationTestStatistic - "
				  "expected one strategy, got " 
				  + std::to_string(bt->getNumStrategies()));
      }

      // --- NEW: Enforce minimum activity thresholds ---
      const unsigned int minTradesRequired = BootStrappedProfitFactorPolicy<Decimal>::getMinStrategyTrades();
      const unsigned int minBarsRequired = BootStrappedProfitFactorPolicy<Decimal>::getMinBarSeriesSize();
      
      // Get the total number of trades for the backtest run.
      uint32_t numTrades = bt->getNumTrades();
      
      // Pull every bar‐by‐bar return (entry→exit and any still‐open):
      std::vector<Decimal> barSeries = bt->getAllHighResReturns((*(bt->beginStrategies())).get());
      
      // If the thresholds are not met, return a neutral (zero) statistic.
      if (numTrades < minTradesRequired || barSeries.size() < minBarsRequired)
      {
          return DecimalConstants<Decimal>::DecimalZero;
      }

      // Explicitly disambiguate the overload for computeLogProfitFactor
      auto computePF = static_cast<Decimal(*)(const std::vector<Decimal>&)>(StatUtils<Decimal>::computeProfitFactor);

      return StatUtils<Decimal>::getBootStrappedStatistic(barSeries, computePF);
    }

    /// Minimum number of closed trades required to even attempt this test
    static unsigned int getMinStrategyTrades() { return 3; }

    // Minimum number of bars in the series to be considered statistically significant
    static unsigned int getMinBarSeriesSize() { return 6; }

    // Return a test statistic constant if we don't meet the minimum trade criteria
    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };

  template <class Decimal> class NonGranularProfitFactorPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
	  uint32_t numTrades = aBackTester->getNumTrades();
	  if (numTrades < getMinStrategyTrades()) {
	    return getMinTradeFailureTestStatistic();
	  }

          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getLogProfitFactor();
        }
      else
        throw BackTesterException("NonGranularProfitFactorPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalOne;
    }

  };

  
  template <class Decimal> class CumulativeReturnPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getCumulativeReturn();
        }
      else
        throw BackTesterException("CumulativeReturnPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }

  };


  template <class Decimal> class NormalizedReturnPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {

      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          double factor;
          Decimal cumulativeReturn;
          uint32_t timeInMarket;
          Decimal normalizationRatio;

          factor = sqrt(backTesterStrategy->numTradingOpportunities());

          timeInMarket = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getNumBarsInMarket();

          if (timeInMarket == 0)
            throw BackTesterException("NormalizedReturnPolicy::getPermutationTestStatistic - time in market cannot be 0!");

          cumulativeReturn = backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getCumulativeReturn();

          normalizationRatio = Decimal( (factor / sqrt(timeInMarket)) );

          return cumulativeReturn * normalizationRatio;
        }
      else
        throw BackTesterException("NormalizedReturnPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }

  };

  //
  template <class Decimal> class PalProfitabilityPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getMedianPALProfitability();
        }
      else
        throw BackTesterException("PalProfitabilityPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }
    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }

  };

  //
  template <class Decimal> class PessimisticReturnRatioPolicy
  {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> aBackTester)
    {
      if (aBackTester->getNumStrategies() == 1)
        {
          std::shared_ptr<BacktesterStrategy<Decimal>> backTesterStrategy =
              (*(aBackTester->beginStrategies()));

          return backTesterStrategy->getStrategyBroker().getClosedPositionHistory().getPessimisticReturnRatio();
        }
      else
        throw BackTesterException("PessimisticReturnRatioPolicy::getPermutationTestStatistic - number of strategies is not equal to one, equal to "  +std::to_string(aBackTester->getNumStrategies()));
    }

    static unsigned int getMinStrategyTrades()
    {
      return 3;
    }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }

  };

  template <class Decimal>
  class RobustProfitFactorPolicy
  {
  public:
    /**
     * @brief Compute a robust profit factor statistic that handles edge cases
     *        like small samples and zero-loss trades more effectively
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      if (bt->getNumStrategies() != 1) {
        throw BackTesterException(
          "RobustProfitFactorPolicy::getPermutationTestStatistic - "
          "expected one strategy, got "
          + std::to_string(bt->getNumStrategies()));
      }

      const unsigned int minTradesRequired = getMinStrategyTrades();
      const unsigned int minBarsRequired = 5;
      
      uint32_t numTrades = bt->getNumTrades();
      std::vector<Decimal> barSeries = bt->getAllHighResReturns((*(bt->beginStrategies())).get());
      
      if (numTrades < minTradesRequired || barSeries.size() < minBarsRequired) {
        return getMinTradeFailureTestStatistic();
      }

      // Winsorize returns at 95th percentile
      std::vector<Decimal> winsorized = winsorizeReturns(barSeries, 0.05);
      
      Decimal lw(0), ll(0);
      std::vector<Decimal> losses;
      
      for (auto r : winsorized) {
        double m = 1 + num::to_double(r);
        if (m <= 0) continue;
        
        Decimal lr(std::log(m));
        if (r > 0) {
          lw += lr;
        } else {
          ll += lr;
          losses.push_back(lr);
        }
      }

      // Handle zero-loss cases with empirical smoothing
      if (ll == DecimalConstants<Decimal>::DecimalZero) {
        if (!losses.empty()) {
          // Use median of observed losses if available
          ll = median(losses);
        } else {
          // Default small loss if no losses observed
          ll = Decimal(-0.01);
        }
      }

      // Add +1 smoothing to numerator and denominator
      Decimal pf = (lw + 1) / (num::abs(ll) + 1);
      return pf;
    }

    static unsigned int getMinStrategyTrades() { return 2; }

    static Decimal getMinTradeFailureTestStatistic() {
      return Decimal(1.0); // Neutral PF instead of 0
    }

  private:
    static std::vector<Decimal> winsorizeReturns(const std::vector<Decimal>& returns, double percentile) {
      if (returns.empty()) return returns;
      
      std::vector<Decimal> sorted = returns;
      std::sort(sorted.begin(), sorted.end());
      
      size_t upper_idx = static_cast<size_t>((1.0 - percentile) * sorted.size());
      Decimal upper = sorted[std::min(upper_idx, sorted.size()-1)];
      Decimal lower = sorted[std::max(static_cast<size_t>(percentile * sorted.size()), 0UL)];
      
      std::vector<Decimal> winsorized;
      for (auto r : returns) {
        if (r > upper) winsorized.push_back(upper);
        else if (r < lower) winsorized.push_back(lower);
        else winsorized.push_back(r);
      }
      return winsorized;
    }

    static Decimal median(std::vector<Decimal>& values) {
      if (values.empty()) return Decimal(0);
      std::sort(values.begin(), values.end());
      size_t n = values.size();
      if (n % 2 == 0) {
        return (values[n/2 - 1] + values[n/2]) / Decimal(2);
      } else {
        return values[n/2];
      }
    }
  };

  template <class Decimal>
  class EnhancedBarScorePolicy {
  public:
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt) {
      if (bt->getNumStrategies() != 1)
	throw BackTesterException("Expected one strategy");

      auto strat = *(bt->beginStrategies());
      const auto& metrics = bt->getExpandedHighResReturns(strat.get());
      
      if (metrics.size() < 3)
	return DecimalConstants<Decimal>::DecimalOne;
      
      std::vector<Decimal> scores;

      // Weight constants
      const Decimal weightOpenToClose = Decimal(0.8);
      const Decimal weightHighToOpen  = Decimal(0.5);
      const Decimal weightLowToOpen   = Decimal(0.5);

      for (const auto& m : metrics) {
	scores.push_back(m.closeToClose);                        // Normal return
	scores.push_back(m.openToClose * weightOpenToClose);    // Open to close
	scores.push_back(m.highToOpen * weightHighToOpen);      // Favor upside
	scores.push_back(-num::abs(m.lowToOpen) * weightLowToOpen); // Penalize downside
      }

      // --- Add entry-bar close-to-entry-price return per trade ---
      const auto& positions = strat->getStrategyBroker().getClosedPositionHistory();
      for (auto it = positions.beginTradingPositions(); it != positions.endTradingPositions(); ++it) {
	const auto& pos = it->second;
	auto barIt = pos->beginPositionBarHistory();
	if (barIt == pos->endPositionBarHistory())
	  continue;
	
	const Decimal& entryPrice = pos->getEntryPrice();
	const Decimal& entryClose = barIt->second.getCloseValue();

	if (entryPrice != DecimalConstants<Decimal>::DecimalZero) {
	  Decimal entryReturn = (entryClose - entryPrice) / entryPrice;
	  scores.push_back(entryReturn); // One per trade
	}
      }

      return StatUtils<Decimal>::computeLogProfitFactor(scores);
    }

    static unsigned int getMinStrategyTrades() { return 2; }

    static Decimal getMinTradeFailureTestStatistic() {
      return DecimalConstants<Decimal>::DecimalOne;
    }

    static void printDetailedScoreBreakdown(std::shared_ptr<BackTester<Decimal>> bt, std::ostream& os)
    {
      if (bt->getNumStrategies() != 1)
	throw BackTesterException("Expected one strategy");

      auto strat = *(bt->beginStrategies());
      const auto& metrics = bt->getExpandedHighResReturns(strat.get());

      if (metrics.size() < 3) {
	os << "Too few high-res metrics to evaluate.\n";
	return;
      }

      std::vector<Decimal> scores;

      const Decimal weightOpenToClose = Decimal(0.8);
      const Decimal weightHighToOpen  = Decimal(0.5);
      const Decimal weightLowToOpen   = Decimal(0.5);

      os << "=== EnhancedBarScore Components ===\n";

      for (size_t i = 0; i < metrics.size(); ++i) {
	const auto& m = metrics[i];
	Decimal s1 = m.closeToClose;
	Decimal s2 = m.openToClose * weightOpenToClose;
	Decimal s3 = m.highToOpen * weightHighToOpen;
	Decimal s4 = -num::abs(m.lowToOpen) * weightLowToOpen;

	os << "Bar " << i
		  << ": CloseToClose=" << s1
		  << ", OpenToClose=" << s2
		  << ", HighToOpen=" << s3
		  << ", -|LowToOpen|=" << s4
		  << "\n";

	scores.push_back(s1);
	scores.push_back(s2);
	scores.push_back(s3);
	scores.push_back(s4);
      }

      const auto& positions = strat->getStrategyBroker().getClosedPositionHistory();
      for (auto it = positions.beginTradingPositions(); it != positions.endTradingPositions(); ++it) {
	const auto& pos = it->second;
	auto barIt = pos->beginPositionBarHistory();
	if (barIt == pos->endPositionBarHistory())
	  continue;

	const Decimal& entryPrice = pos->getEntryPrice();
	const Decimal& entryClose = barIt->second.getCloseValue();

	if (entryPrice != DecimalConstants<Decimal>::DecimalZero) {
	  Decimal entryReturn = (entryClose - entryPrice) / entryPrice;
	  os << "EntryBar Return: " << entryReturn << "\n";
	  scores.push_back(entryReturn);
	}
      }

      Decimal finalScore = StatUtils<Decimal>::computeLogProfitFactor(scores);
      os << "== Final EnhancedBarScorePolicy LogProfitFactor: " << finalScore << "\n\n";
    }
  };

template <class Decimal>
class HybridEnhancedTradeAwarePolicy
{
public:
    /**
     * @brief Computes a blended score that dynamically weights backtest performance
     * based on the number of trades.
     *
     * This policy calculates a final score as a dynamic weighted average of two components:
     * 1. EnhancedBarScore: Measures the quality of intra-bar price movement. This score
     * is statistically stable due to its high number of data points.
     * 2. TradeScore: Measures the realized profitability (ProfitFactor, PAL) of closed
     * trades. This score's reliability increases with the number of trades.
     *
     * A sigmoid "confidence factor" is used to adjust the weights. For strategies with few
     * trades, this policy trusts the more stable EnhancedBarScore. As the number of trades
     * increases, more weight is dynamically shifted to the TradeScore.
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
        if (bt->getNumStrategies() != 1)
            throw BackTesterException("HybridEnhancedTradeAwarePolicy: Expected one strategy");

        // --- 1. Calculate the Enhanced Bar Score ---
        // This score evaluates the quality of price movement during the trade.
        Decimal enhancedScore = EnhancedBarScorePolicy<Decimal>::getPermutationTestStatistic(bt);

        auto strat = *(bt->beginStrategies());
        auto& closedPositions = strat->getStrategyBroker().getClosedPositionHistory();
        uint32_t numTrades = closedPositions.getNumPositions();

        // Require a minimum number of trades to generate a meaningful score.
        if (numTrades < getMinStrategyTrades()) {
            return getMinTradeFailureTestStatistic();
        }

        // --- 2. Calculate the Trade Score ---
        // This score evaluates the actual profitability of the strategy.
        Decimal tradeScore = DecimalConstants<Decimal>::DecimalZero;
        Decimal pf = closedPositions.getProfitFactor();
        Decimal palProfitability = closedPositions.getPALProfitability();

        // Defensively check for valid, non-zero metrics
        if (pf > DecimalConstants<Decimal>::DecimalZero && palProfitability > DecimalConstants<Decimal>::DecimalZero)
        {
            auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
            if (palStrat)
            {
                // Calculate the expected PAL based on the strategy's own risk/reward
                auto pattern = palStrat->getPalPattern();
                Decimal target = pattern->getProfitTargetAsDecimal();
                Decimal stop = pattern->getStopLossAsDecimal();
                
                if (stop > DecimalConstants<Decimal>::DecimalZero)
                {
                    Decimal payoffRatio = target / stop;
                    Decimal desiredProfitFactor = DecimalConstants<Decimal>::DecimalTwo;
                    Decimal oneHundred = DecimalConstants<Decimal>::DecimalOneHundred;
                    
                    Decimal expectedPALProfitability = (desiredProfitFactor / (desiredProfitFactor + payoffRatio)) * oneHundred;

                    // Calculate penalty for underperforming PAL
                    Decimal penaltyRatio = (expectedPALProfitability > DecimalConstants<Decimal>::DecimalZero)
                                             ? (palProfitability / expectedPALProfitability)
                                             : DecimalConstants<Decimal>::DecimalOne;

                    penaltyRatio = std::max(Decimal(0.1), std::min(Decimal(1.0), penaltyRatio));
                    tradeScore = (pf - Decimal(1.0)) * penaltyRatio;
                }
            }
        }

        // --- 3. Compute Dynamic Weights based on Confidence ---
        const Decimal baseTradeScoreWeight = Decimal(0.6); // The max weight for the trade score.
        
        // Sigmoid parameters calibrated for a typical 5-15 trade count range.
        const double k = 0.5;   // Steepness of the confidence curve.
        const double x0 = 10.0; // Midpoint (trades at which confidence is 50%).

        // Confidence factor smoothly transitions from 0 to 1 as numTrades increases.
        Decimal confidenceFactor = Decimal(1.0 / (1.0 + std::exp(-k * (num::to_double(Decimal(numTrades)) - x0))));

        // The trade score's weight is scaled by our confidence in it.
        Decimal dynamicTradeScoreWeight = baseTradeScoreWeight * confidenceFactor;

        // The enhanced score's weight is the remainder, making it dominant for low-trade strategies.
        Decimal dynamicEnhancedScoreWeight = Decimal(1.0) - dynamicTradeScoreWeight;
        
        // --- 4. Compute Final Blended Score ---
        Decimal finalScore = (enhancedScore * dynamicEnhancedScoreWeight) + (tradeScore * dynamicTradeScoreWeight);

        return finalScore;
    }

    static unsigned int getMinStrategyTrades() { return 2; }

    // Return zero for failure, as profitable strategies should have a positive score.
    static Decimal getMinTradeFailureTestStatistic() {
      return DecimalConstants<Decimal>::DecimalZero;
    }

    // This function is updated to reflect the new scoring logic for easier debugging.
    static void printDetailedScoreBreakdown(std::shared_ptr<BackTester<Decimal>> bt, std::ostream& os)
    {
        if (bt->getNumStrategies() != 1)
        {
            os << "Breakdown requires a single strategy.\n";
            return;
        }

        auto strat = *(bt->beginStrategies());
        auto& closedPositions = strat->getStrategyBroker().getClosedPositionHistory();

        // Enhanced Score Details
        EnhancedBarScorePolicy<Decimal>::printDetailedScoreBreakdown(bt, os);

        // Trade Score Details
        Decimal enhancedScore = EnhancedBarScorePolicy<Decimal>::getPermutationTestStatistic(bt);
        Decimal pf = closedPositions.getProfitFactor();
        Decimal palProfitability = closedPositions.getPALProfitability();
        uint32_t numTrades = closedPositions.getNumPositions();
        Decimal tradeScore = DecimalConstants<Decimal>::DecimalZero;
        Decimal penaltyRatio = DecimalConstants<Decimal>::DecimalOne;
        Decimal expectedPAL = DecimalConstants<Decimal>::DecimalZero;

        if (numTrades >= getMinStrategyTrades() && pf > DecimalConstants<Decimal>::DecimalZero)
        {
            auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
            if (palStrat)
            {
                auto pattern = palStrat->getPalPattern();
                Decimal target = pattern->getProfitTargetAsDecimal();
                Decimal stop = pattern->getStopLossAsDecimal();

                if (stop > DecimalConstants<Decimal>::DecimalZero)
                {
                    Decimal payoffRatio = target / stop;
                    Decimal desiredProfitFactor = DecimalConstants<Decimal>::DecimalTwo;
                    Decimal oneHundred = DecimalConstants<Decimal>::DecimalOneHundred;
                    expectedPAL = (desiredProfitFactor / (desiredProfitFactor + payoffRatio)) * oneHundred;
                    
                    if (expectedPAL > DecimalConstants<Decimal>::DecimalZero)
                        penaltyRatio = palProfitability / expectedPAL;

                    penaltyRatio = std::max(Decimal(0.1), std::min(Decimal(1.0), penaltyRatio));
                    tradeScore = (pf - Decimal(1.0)) * penaltyRatio;
                }
            }
        }

        // Dynamic Weight Calculation
        const Decimal baseTradeScoreWeight = Decimal(0.6);
        const double k = 0.5;
        const double x0 = 10.0;
        Decimal confidenceFactor = Decimal(1.0 / (1.0 + std::exp(-k * (num::to_double(Decimal(numTrades)) - x0))));
        Decimal dynamicTradeScoreWeight = baseTradeScoreWeight * confidenceFactor;
        Decimal dynamicEnhancedScoreWeight = Decimal(1.0) - dynamicTradeScoreWeight;
        
        Decimal finalScore = (enhancedScore * dynamicEnhancedScoreWeight) + (tradeScore * dynamicTradeScoreWeight);

        os << "\n== Final HybridEnhancedTradeAwarePolicy Breakdown ==\n";
        os << "EnhancedBarScore: " << enhancedScore << "\n";
        os << "--------------------------------------\n";
        os << "Profit Factor: " << pf << "\n";
        os << "PAL Profitability: " << palProfitability << "%\n";
        os << "Expected PAL Profitability: " << expectedPAL << "%\n";
        os << "Penalty Ratio: " << penaltyRatio << "\n";
        os << "Trade Score ((PF-1) * Penalty): " << tradeScore << "\n";
        os << "--------------------------------------\n";
        os << "Confidence Calculation:\n";
        os << "  Number of Trades: " << numTrades << "\n";
        os << "  Confidence Factor: " << confidenceFactor << "\n";
        os << "  Dynamic Enhanced Score Weight: " << dynamicEnhancedScoreWeight << "\n";
        os << "  Dynamic Trade Score Weight: " << dynamicTradeScoreWeight << "\n";
        os << "--------------------------------------\n";
        os << "== Final Blended Score: " << finalScore << "\n\n";
    }
  };

  template <class Decimal>
  class AccumulationSwingIndexPolicy
  {
  public:
    /**
     * @brief Computes a statistic based on Welles Wilder's Accumulation Swing Index (ASI).
     *
     * This policy evaluates a strategy based on its ability to capture favorable price swings.
     * It calculates the Swing Index (SI) for each bar within every closed trade.
     *
     * Key Adaptations:
     * 1.  For short trades, the SI value is inverted. This ensures that strong downward moves
     * (which are good for shorts) contribute positively to the final score.
     * 2.  The "Limit Move" (T) from the original futures-based formula is not applicable to stocks.
     * It is replaced by a volatility normalization using the bar's "true range" (R).
     *
     * The final statistic is the log-profit-factor of this series of direction-adjusted swing scores,
     * providing a robust measure of swing-capture quality.
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      if (bt->getNumStrategies() != 1)
	throw BackTesterException("AccumulationSwingIndexPolicy: Expected one strategy");

      auto strat = *(bt->beginStrategies());
      const auto& closedPositions = strat->getStrategyBroker().getClosedPositionHistory();

      if (closedPositions.getNumPositions() < getMinStrategyTrades()) {
	return getMinTradeFailureTestStatistic();
      }

      std::vector<Decimal> directionAdjustedSwings;

      // Iterate through every closed position to analyze its bar history
      for (auto it = closedPositions.beginTradingPositions(); it != closedPositions.endTradingPositions(); ++it)
        {
	  const auto& pos = it->second;
	  auto barIt = pos->beginPositionBarHistory();
	  auto endIt = pos->endPositionBarHistory();

	  if (std::distance(barIt, endIt) < 2)
	    continue;

	  auto prevBarIt = barIt;
	  barIt++;

	  for (; barIt != endIt; ++barIt)
            {
	      const auto& currentBar = barIt->second;
	      const auto& prevBar = prevBarIt->second;

	      Decimal swingIndex = calculateSwingIndex(currentBar, prevBar);

	      // Invert the swing index for short positions
	      if (pos->isShortPosition())
                {
		  swingIndex *= -1;
                }

	      directionAdjustedSwings.push_back(swingIndex);
	      prevBarIt = barIt;
            }
        }

      if (directionAdjustedSwings.empty())
        {
	  return DecimalConstants<Decimal>::DecimalOne;
        }

      // Use the same log-profit-factor utility as other policies for a robust final score
      return StatUtils<Decimal>::computeLogProfitFactor(directionAdjustedSwings);
    }

    static unsigned int getMinStrategyTrades()
    {
      return 2;
    }

    static Decimal getMinTradeFailureTestStatistic()
    {
      // A neutral log-profit-factor is 1.0
      return DecimalConstants<Decimal>::DecimalOne;
    }

  private:
    /**
     * @brief Calculates the Swing Index for a single bar.
     */
    static Decimal calculateSwingIndex(const OpenPositionBar<Decimal>& currentBar, const OpenPositionBar<Decimal>& prevBar)
    {
      // Get OHLC values for current and previous bars
      Decimal O = currentBar.getOpenValue();
      Decimal H = currentBar.getHighValue();
      Decimal L = currentBar.getLowValue();
      Decimal C = currentBar.getCloseValue();

      Decimal O_y = prevBar.getOpenValue();
      Decimal H_y = prevBar.getHighValue();
      Decimal L_y = prevBar.getLowValue();
      Decimal C_y = prevBar.getCloseValue();

      // Step 1: Calculate R (the "true range" of the move)
      Decimal R;
      Decimal move1 = num::abs(H - C_y);
      Decimal move2 = num::abs(L - C_y);
      Decimal move3 = num::abs(H - L);

      if (move1 > move2 && move1 > move3) // Case 1: Gap up or move above yesterday's close is largest
        {
	  R = num::abs(H - C_y) - DecimalConstants<Decimal>::createDecimal("0.5") * num::abs(L - C_y) + DecimalConstants<Decimal>::createDecimal("0.25") * num::abs(C_y - O_y);
        }
      else if (move2 > move1 && move2 > move3) // Case 2: Gap down or move below yesterday's close is largest
        {
	  R = num::abs(L - C_y) - DecimalConstants<Decimal>::createDecimal("0.5") * num::abs(H - C_y) + DecimalConstants<Decimal>::createDecimal("0.25") * num::abs(C_y - O_y);
        }
      else // Case 3: Intraday high-low range is largest
        {
	  R = num::abs(H - L) + DecimalConstants<Decimal>::createDecimal("0.25") * num::abs(C_y - O_y);
        }

      if (R == DecimalConstants<Decimal>::DecimalZero)
        {
	  return DecimalConstants<Decimal>::DecimalZero;
        }
        
      // Step 2: Calculate K (the larger of the two major gap/move values)
      Decimal K = std::max(num::abs(H - C_y), num::abs(L - C_y));

      // Step 3: Calculate the unscaled Swing Index
      Decimal numerator = (C - C_y) + DecimalConstants<Decimal>::createDecimal("0.5") * (C - O) + DecimalConstants<Decimal>::createDecimal("0.25") * (C_y - O_y);
        
      // Step 4: Final Swing Index, scaled by 50 and normalized by K/R
      // We use K/R instead of K/T since T (Limit Move) is not applicable to stocks.
      Decimal swingIndex = DecimalConstants<Decimal>::createDecimal("50.0") * (numerator / R) * (K / R);

      return swingIndex;
    }
  };

  //
  template <class Decimal>
  class HybridSwingTradePolicy
  {
  public:
    /**
     * @brief Computes a hybrid score blending Swing Index quality with trade profitability.
     *
     * This policy combines two distinct measures of strategy performance:
     * 1.  Swing Quality Score: Uses the AccumulationSwingIndexPolicy to evaluate how well
     * a strategy follows the market's underlying momentum on a bar-by-bar basis.
     * 2.  Trade Performance Score: Uses the actual backtest results (Profit Factor, PAL)
     * to measure realized profitability.
     *
     * The two scores are dynamically blended using a "confidence factor" based on the
     * number of trades. This ensures that a strategy must demonstrate both fundamental
     * soundness (good swing quality) and empirical profitability to achieve a high score.
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      if (bt->getNumStrategies() != 1)
	throw BackTesterException("HybridSwingTradePolicy: Expected one strategy");

      // --- 1. Calculate the Swing Quality Score ---
      // This score is stable and measures the quality of the underlying logic.
      Decimal swingQualityScore = AccumulationSwingIndexPolicy<Decimal>::getPermutationTestStatistic(bt);

      auto strat = *(bt->beginStrategies());
      auto& closedPositions = strat->getStrategyBroker().getClosedPositionHistory();
      uint32_t numTrades = closedPositions.getNumPositions();

      if (numTrades < getMinStrategyTrades()) {
	return getMinTradeFailureTestStatistic();
      }

      // --- 2. Calculate the Trade Performance Score ---
      // This score is based on the actual, realized profitability.
      Decimal tradePerformanceScore = DecimalConstants<Decimal>::DecimalZero;
      Decimal pf = closedPositions.getProfitFactor();
      Decimal palProfitability = closedPositions.getPALProfitability();

      if (pf > DecimalConstants<Decimal>::DecimalZero && palProfitability > DecimalConstants<Decimal>::DecimalZero)
        {
	  auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
	  if (palStrat)
            {
	      auto pattern = palStrat->getPalPattern();
	      Decimal target = pattern->getProfitTargetAsDecimal();
	      Decimal stop = pattern->getStopLossAsDecimal();
                
	      if (stop > DecimalConstants<Decimal>::DecimalZero)
                {
		  Decimal payoffRatio = target / stop;
		  Decimal desiredProfitFactor = DecimalConstants<Decimal>::DecimalTwo;
		  Decimal oneHundred = DecimalConstants<Decimal>::DecimalOneHundred;
		  Decimal expectedPALProfitability = (desiredProfitFactor / (desiredProfitFactor + payoffRatio)) * oneHundred;
		  Decimal penaltyRatio = (expectedPALProfitability > DecimalConstants<Decimal>::DecimalZero)
		    ? (palProfitability / expectedPALProfitability)
		    : DecimalConstants<Decimal>::DecimalOne;
		  penaltyRatio = std::max(Decimal(0.1), std::min(Decimal(1.0), penaltyRatio));
		  tradePerformanceScore = (pf - Decimal(1.0)) * penaltyRatio;
                }
            }
        }

      // --- 3. Compute Dynamic Weights based on Confidence ---
      const Decimal baseTradeScoreWeight = Decimal(0.6);
      const double k = 0.5;   // Calibrated for 5-15 trade range
      const double x0 = 10.0; // Midpoint at 10 trades

      Decimal confidenceFactor = Decimal(1.0 / (1.0 + std::exp(-k * (num::to_double(Decimal(numTrades)) - x0))));
      Decimal dynamicTradeScoreWeight = baseTradeScoreWeight * confidenceFactor;
      Decimal dynamicSwingScoreWeight = Decimal(1.0) - dynamicTradeScoreWeight;
        
      // --- 4. Compute Final Blended Score ---
      Decimal finalScore = (swingQualityScore * dynamicSwingScoreWeight) + (tradePerformanceScore * dynamicTradeScoreWeight);

      return finalScore;
    }

    static unsigned int getMinStrategyTrades() { return 2; }

    static Decimal getMinTradeFailureTestStatistic()
    {
      // A neutral score in this blended model is zero.
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };
  //

  template <class Decimal>
  class ProfitFactorGatedSwingPolicy
  {
  public:
    /**
     * @brief Computes a score based on a hard profit-factor gate, followed by a
     * quality assessment using the Swing Index.
     *
     * This policy enforces a strict two-stage evaluation:
     * 1.  Gatekeeper Stage: A strategy is immediately disqualified (receives a score of 0)
     * if its backtest Profit Factor is below a defined threshold (`getMinProfitFactor()`).
     * This ensures all surviving patterns meet a minimum profitability requirement.
     *
     * 2.  Scoring Stage: For strategies that pass the gate, the final score is based
     * primarily on its trade performance (profit factor, PAL penalty). This base score
     * is then multiplied by a "quality bonus" derived from the AccumulationSwingIndexPolicy.
     * This allows the swing quality to differentiate between two similarly profitable strategies.
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      if (bt->getNumStrategies() != 1)
	throw BackTesterException("ProfitFactorGatedSwingPolicy: Expected one strategy");

      auto strat = *(bt->beginStrategies());
      auto& closedPositions = strat->getStrategyBroker().getClosedPositionHistory();

      if (closedPositions.getNumPositions() < getMinStrategyTrades()) {
	return getMinTradeFailureTestStatistic();
      }

      Decimal pf = closedPositions.getProfitFactor();

      // --- 1. Gatekeeper Stage ---
      // Immediately reject any strategy that does not meet the minimum Profit Factor.
      if (pf < getMinProfitFactor())
        {
	  return getMinTradeFailureTestStatistic();
        }

      // --- 2. Scoring Stage (for strategies that passed the gate) ---

      // Calculate the Trade Performance Score (this is our base score)
      Decimal tradePerformanceScore = DecimalConstants<Decimal>::DecimalZero;
      Decimal palProfitability = closedPositions.getPALProfitability();

      if (pf > DecimalConstants<Decimal>::DecimalZero && palProfitability > DecimalConstants<Decimal>::DecimalZero)
        {
	  auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
	  if (palStrat)
            {
	      auto pattern = palStrat->getPalPattern();
	      Decimal target = pattern->getProfitTargetAsDecimal();
	      Decimal stop = pattern->getStopLossAsDecimal();
                
	      if (stop > DecimalConstants<Decimal>::DecimalZero)
                {
		  Decimal payoffRatio = target / stop;
		  Decimal desiredProfitFactor = DecimalConstants<Decimal>::DecimalTwo;
		  Decimal oneHundred = DecimalConstants<Decimal>::DecimalOneHundred;
		  Decimal expectedPALProfitability = (desiredProfitFactor / (desiredProfitFactor + payoffRatio)) * oneHundred;
		  Decimal penaltyRatio = (expectedPALProfitability > DecimalConstants<Decimal>::DecimalZero)
		    ? (palProfitability / expectedPALProfitability)
		    : DecimalConstants<Decimal>::DecimalOne;
		  penaltyRatio = std::max(Decimal(0.1), std::min(Decimal(1.0), penaltyRatio));
		  tradePerformanceScore = (pf - Decimal(1.0)) * penaltyRatio;
                }
            }
        }
        
      // Calculate the Swing Quality Score to use as a bonus multiplier
      Decimal swingQualityScore = AccumulationSwingIndexPolicy<Decimal>::getPermutationTestStatistic(bt);

      // Normalize the swing score so it acts as a reasonable bonus/malus.
      // A swing score of 1.0 (neutral) results in no change.
      const Decimal swingBonusWeight = Decimal(0.25);
      Decimal qualityBonus = Decimal(1.0) + ((swingQualityScore - Decimal(1.0)) * swingBonusWeight);

      // Ensure the bonus doesn't penalize too harshly or reward too much.
      qualityBonus = std::max(Decimal(0.5), std::min(Decimal(1.5), qualityBonus));

      // The final score is the performance score, adjusted by the quality bonus.
      Decimal finalScore = tradePerformanceScore * qualityBonus;

      return finalScore;
    }

    // --- Configurable Thresholds ---

    static unsigned int getMinStrategyTrades() { return 2; }

    /**
     * @brief Defines the minimum acceptable Profit Factor.
     * This is the hard gate for the policy.
     */
    static Decimal getMinProfitFactor() { return Decimal(1.2); }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };
  //
  template <class Decimal>
  class ConfidenceAdjustedPalPolicy
  {
  public:
    /**
     * @brief Computes a score based on PAL Profitability, adjusted for statistical confidence.
     *
     * This policy isolates PAL Profitability as the primary metric of strategy quality.
     * The final score is a product of two components:
     * 1.  PAL Performance Ratio: The actual PAL Profitability divided by the theoretical
     * (or expected) PAL Profitability. This measures how effectively the strategy
     * realizes its potential.
     * 2.  Confidence Factor: A sigmoid function of the number of trades. This scales
     * the score down for strategies with few trades, reflecting the lower statistical
     * significance of the PAL metric in such cases.
     *
     * The result is a score that directly rewards high PAL profitability, but only when
     * supported by a sufficient number of trades.
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      if (bt->getNumStrategies() != 1)
	throw BackTesterException("ConfidenceAdjustedPalPolicy: Expected one strategy");

      auto strat = *(bt->beginStrategies());
      auto& closedPositions = strat->getStrategyBroker().getClosedPositionHistory();
      uint32_t numTrades = closedPositions.getNumPositions();

      if (numTrades < getMinStrategyTrades()) {
	return getMinTradeFailureTestStatistic();
      }

      // --- 1. Calculate PAL Performance Ratio ---
      Decimal palPerformanceRatio = DecimalConstants<Decimal>::DecimalZero;
      Decimal palProfitability = closedPositions.getPALProfitability();

      if (palProfitability > DecimalConstants<Decimal>::DecimalZero)
        {
	  auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
	  if (palStrat)
            {
	      auto pattern = palStrat->getPalPattern();
	      Decimal target = pattern->getProfitTargetAsDecimal();
	      Decimal stop = pattern->getStopLossAsDecimal();

	      if (stop > DecimalConstants<Decimal>::DecimalZero)
                {
		  Decimal payoffRatio = target / stop;
		  Decimal desiredProfitFactor = DecimalConstants<Decimal>::DecimalTwo;
		  Decimal oneHundred = DecimalConstants<Decimal>::DecimalOneHundred;
		  Decimal expectedPALProfitability = (desiredProfitFactor / (desiredProfitFactor + payoffRatio)) * oneHundred;

		  if (expectedPALProfitability > DecimalConstants<Decimal>::DecimalZero)
                    {
		      palPerformanceRatio = palProfitability / expectedPALProfitability;
                    }
                }
            }
        }
        
      // Clamp the ratio to a maximum of 1.0 to prevent outlier scores
      palPerformanceRatio = std::min(palPerformanceRatio, Decimal(1.0));

      // --- 2. Calculate Confidence Factor ---
      // Sigmoid parameters calibrated for a typical 5-15 trade count range.
      const double k = 0.5;
      const double x0 = 10.0;
      Decimal confidenceFactor = Decimal(1.0 / (1.0 + std::exp(-k * (num::to_double(Decimal(numTrades)) - x0))));

      // --- 3. Compute Final Score ---
      // The final score is the performance, scaled by our confidence in it.
      Decimal finalScore = palPerformanceRatio * confidenceFactor;

      return finalScore;
    }

    static unsigned int getMinStrategyTrades() { return 2; }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };
  //
  template <class Decimal>
  class GatedPerformanceScaledPalPolicy
  {
  public:
    /**
     * @brief Computes a score based on a PF gate, then a performance-scaled PAL and PF score.
     *
     * This is a multi-stage policy:
     * 1.  Gatekeeper: Immediately rejects strategies with a Profit Factor below a hard
     * threshold (`getMinProfitFactor()`).
     *
     * 2.  Performance Scaling: For strategies that pass the gate, two performance ratios are calculated:
     * a) PAL Ratio: Actual PAL / Theoretical PAL.
     * b) PF Ratio: Actual PF / Target PF.
     * These ratios are multiplied to create a combined performance score.
     *
     * 3.  Confidence Adjustment: The combined performance score is then scaled by a
     * confidence factor based on the number of trades.
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      if (bt->getNumStrategies() != 1)
	throw BackTesterException("GatedPerformanceScaledPalPolicy: Expected one strategy");

      auto strat = *(bt->beginStrategies());
      auto& closedPositions = strat->getStrategyBroker().getClosedPositionHistory();
      uint32_t numTrades = closedPositions.getNumPositions();

      if (numTrades < getMinStrategyTrades()) {
	return getMinTradeFailureTestStatistic();
      }
        
      // --- 1. Gatekeeper Stage ---
      Decimal pf = closedPositions.getProfitFactor();

      // Don't gate keep anymore since they messes up the
      // the distribution of p-values

      /*
      if (pf < getMinProfitFactor())
        {
	  return getMinTradeFailureTestStatistic();
        }
      */

      // --- 2. Performance Scaling Stage ---

      // a) Calculate PAL Performance Ratio
      Decimal palPerformanceRatio = DecimalConstants<Decimal>::DecimalZero;
      Decimal palProfitability = closedPositions.getPALProfitability();

      if (palProfitability > DecimalConstants<Decimal>::DecimalZero)
        {
	  auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
	  if (palStrat)
            {
	      auto pattern = palStrat->getPalPattern();
	      Decimal target = pattern->getProfitTargetAsDecimal();
	      Decimal stop = pattern->getStopLossAsDecimal();

	      if (stop > DecimalConstants<Decimal>::DecimalZero)
                {
		  Decimal payoffRatio = target / stop;
		  Decimal desiredProfitFactor = getTargetProfitFactor();
		  Decimal oneHundred = DecimalConstants<Decimal>::DecimalOneHundred;
		  Decimal expectedPALProfitability = (desiredProfitFactor / (desiredProfitFactor + payoffRatio)) * oneHundred;

		  if (expectedPALProfitability > DecimalConstants<Decimal>::DecimalZero)
                    {
		      palPerformanceRatio = palProfitability / expectedPALProfitability;
                    }
                }
            }
        }
      palPerformanceRatio = std::min(palPerformanceRatio, Decimal(1.0));

      // b) Calculate Profit Factor Performance Ratio
      Decimal pfPerformanceRatio = pf / getTargetProfitFactor();
      // Clamp to prevent extreme scores from a single massive trade
      pfPerformanceRatio = std::min(pfPerformanceRatio, Decimal(1.5));

      // c) Combine performance scores
      Decimal combinedPerformanceScore = palPerformanceRatio * pfPerformanceRatio;

      // --- 3. Confidence Adjustment Stage ---
      const double k = 0.5;
      const double x0 = 10.0;
      Decimal confidenceFactor = Decimal(1.0 / (1.0 + std::exp(-k * (num::to_double(Decimal(numTrades)) - x0))));

      // --- Final Score ---
      Decimal finalScore = combinedPerformanceScore * confidenceFactor;

      return finalScore;
    }

    // --- Configurable Thresholds ---
    static unsigned int getMinStrategyTrades() { return 3; }
    
    // The hard gate for entry
    static Decimal getMinProfitFactor() { return DecimalConstants<Decimal>::DecimalOnePointSevenFive; }

    // The target for performance scaling
    static Decimal getTargetProfitFactor() { return DecimalConstants<Decimal>::DecimalTwo; }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };
  //
template <class Decimal>
  class BootStrappedProfitabilityPFPolicy
  {
  public:
    /**
     * @brief Computes a score using high-res returns, gated by PF, and scaled by performance.
     *
     * This policy is a variant of GatedPerformanceScaledPalPolicy. The key difference
     * is that the core metrics (Profit Factor and Profitability) are calculated from
     * the complete series of high-resolution bar-by-bar returns, rather than from
     * the closed trade history.
     *
     * This is a multi-stage policy:
     * 1.  Metric Calculation: Profit Factor and Profitability are calculated from all
     * bar-by-bar returns using `StatUtils::computeProfitability`.
     *
     * 2.  Gatekeeper: Immediately rejects strategies if the high-res Profit Factor
     * is below a hard threshold (`getMinProfitFactor()`).
     *
     * 3.  Performance Scaling: For strategies that pass the gate, two performance ratios are calculated:
     * a) Profitability Ratio: Actual Profitability / Theoretical PAL Profitability.
     * b) PF Ratio: Actual PF / Target PF.
     * These ratios are multiplied to create a combined performance score.
     *
     */
    static Decimal getPermutationTestStatistic(std::shared_ptr<BackTester<Decimal>> bt)
    {
      if (bt->getNumStrategies() != 1)
        throw BackTesterException("BootStrappedProfitabilityPFPolicy: Expected one strategy");

      auto strat = *(bt->beginStrategies());
      uint32_t numTrades = bt->getNumTrades();

      // We still check for a minimum number of trades as a basic sanity check.
      if (numTrades < getMinStrategyTrades()) {
        return getMinTradeFailureTestStatistic();
      }

      // --- 1. Metric Calculation Stage ---
      // Pull every bar-by-bar return (entry→exit and any still-open).
      std::vector<Decimal> barSeries = bt->getAllHighResReturns(strat.get());

      // If there are no returns, we cannot score the strategy.
      if (barSeries.empty() || barSeries.size() < getMinBarSeriesSize())
      {
          return getMinTradeFailureTestStatistic();
      }

      // Compute Profit Factor and Profitability from the complete return series.
      auto [pf, profitability] = StatUtils<Decimal>::getBootStrappedProfitability(barSeries,
										  StatUtils<Decimal>::computeProfitability);
        
      // --- 2. Gatekeeper Stage ---

      /*
      if (pf < getMinProfitFactor())
      {
        return getMinTradeFailureTestStatistic();
      }
      */
      // --- 3. Performance Scaling Stage ---

      // a) Calculate Profitability Performance Ratio (replaces PAL ratio)
      Decimal profitabilityPerformanceRatio = DecimalConstants<Decimal>::DecimalZero;

      if (profitability > DecimalConstants<Decimal>::DecimalZero)
      {
        auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
        if (palStrat)
        {
          auto pattern = palStrat->getPalPattern();
          Decimal target = pattern->getProfitTargetAsDecimal();
          Decimal stop = pattern->getStopLossAsDecimal();

          if (stop > DecimalConstants<Decimal>::DecimalZero)
          {
            Decimal payoffRatio = target / stop;
            Decimal desiredProfitFactor = getTargetProfitFactor();
            Decimal oneHundred = DecimalConstants<Decimal>::DecimalOneHundred;
            Decimal expectedPALProfitability = (desiredProfitFactor / (desiredProfitFactor + payoffRatio)) * oneHundred;

            if (expectedPALProfitability > DecimalConstants<Decimal>::DecimalZero)
            {
              // Use the new 'profitability' metric here instead of palProfitability
              profitabilityPerformanceRatio = profitability / expectedPALProfitability;
            }
          }
        }
      }
      profitabilityPerformanceRatio = std::min(profitabilityPerformanceRatio, Decimal(1.0));

      // b) Calculate Profit Factor Performance Ratio (using high-res PF)
      Decimal pfPerformanceRatio = pf / getTargetProfitFactor();
      // Clamp to prevent extreme scores from a single massive trade
      pfPerformanceRatio = std::min(pfPerformanceRatio, Decimal(1.5));

      // c) Combine performance scores
      Decimal combinedPerformanceScore = profitabilityPerformanceRatio * pfPerformanceRatio;

      // --- Final Score ---
      Decimal finalScore = combinedPerformanceScore;

      return finalScore;
    }

    static Decimal getDeterministicTestStatistic(std::shared_ptr<BackTester<Decimal>> backtester)
    {
      const auto failureStat = getMinTradeFailureTestStatistic();
      const auto minTrades = getMinStrategyTrades();
      const auto minBars = getMinBarSeriesSize();

      if (backtester->getNumTrades() < minTrades)
        return failureStat;

      auto strat = *(backtester->beginStrategies());
      std::vector<Decimal> barSeries = backtester->getAllHighResReturns(strat.get());
      if (barSeries.size() < minBars)
        return failureStat;

      auto [pf, profitability] = StatUtils<Decimal>::computeProfitability(barSeries);

      if (pf < getMinProfitFactor())
        return failureStat;

      Decimal profitabilityPerformanceRatio = DecimalConstants<Decimal>::DecimalZero;
      if (profitability > DecimalConstants<Decimal>::DecimalZero)
	{
	  auto palStrat = std::dynamic_pointer_cast<PalStrategy<Decimal>>(strat);
	  if (palStrat)
	    {
	      auto pattern = palStrat->getPalPattern();
	      Decimal target = pattern->getProfitTargetAsDecimal();
	      Decimal stop = pattern->getStopLossAsDecimal();
	      if (stop > DecimalConstants<Decimal>::DecimalZero)
		{
		  Decimal payoffRatio = target / stop;
		  Decimal desiredPF = getTargetProfitFactor();
		  Decimal expectedPALProfitability = (desiredPF / (desiredPF + payoffRatio)) * Decimal(100);
		  if (expectedPALProfitability > DecimalConstants<Decimal>::DecimalZero)
		    {
		      profitabilityPerformanceRatio = profitability / expectedPALProfitability;
		    }
		}
	    }
	}
      profitabilityPerformanceRatio = std::min(profitabilityPerformanceRatio, Decimal(1.0));

      Decimal pfPerformanceRatio = pf / getTargetProfitFactor();
      pfPerformanceRatio = std::min(pfPerformanceRatio, Decimal(1.5));
      
      return profitabilityPerformanceRatio * pfPerformanceRatio;
    }

    // --- Configurable Thresholds (kept consistent with the original policy) ---
    static unsigned int getMinStrategyTrades() { return 3; }
    
    // Minimum number of bars in the series to be considered statistically significant
    static unsigned int getMinBarSeriesSize() { return 6; }

    // The hard gate for entry
    static Decimal getMinProfitFactor() { return DecimalConstants<Decimal>::DecimalOnePointSevenFive; }

    // The target for performance scaling
    static Decimal getTargetProfitFactor() { return DecimalConstants<Decimal>::DecimalTwo; }

    static Decimal getMinTradeFailureTestStatistic()
    {
      return DecimalConstants<Decimal>::DecimalZero;
    }
  };
}

#endif

