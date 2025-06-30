#pragma once
#include <vector>
#include <cmath>
#include <tuple>
#include "DecimalConstants.h"
#include "number.h"

namespace mkc_timeseries
{
  /**
   * @class StatUtils
   * @brief A template class providing static utility functions for statistical analysis of financial time series.
   * @tparam Decimal The high-precision decimal type to be used for calculations.
   */
  template<class Decimal>
    struct StatUtils
    {
    private:
      /**
       * @brief A private helper function to compute a factor from gains and losses.
       * @details This function encapsulates the core ratio calculation and handles the
       * division-by-zero case where losses are zero. It can also apply
       * logarithmic compression to the result.
       * @param gains The total sum of positive values (wins).
       * @param losses The total sum of negative values (losses).
       * @param compressResult A boolean flag; if true, applies log(1 + pf) to the result.
       * @return The calculated factor as a Decimal.
       */
      static Decimal computeFactor(const Decimal& gains, const Decimal& losses, bool compressResult)
      {
        Decimal pf;
	
        // If there are no losses, return a fixed large number to signify a highly profitable state.
        if (losses == DecimalConstants<Decimal>::DecimalZero)
          {
            pf = DecimalConstants<Decimal>::DecimalOneHundred;
          }
        else
          {
            pf = gains / num::abs(losses);
          }
	
        // Apply log compression to the final profit factor if requested
        if (compressResult)
          return Decimal(std::log(num::to_double(DecimalConstants<Decimal>::DecimalOne + pf)));
        else
          return pf;
      }

    public:
      /**
       * @brief Computes the Profit Factor from a series of returns.
       * @details The Profit Factor is the ratio of gross profits to gross losses.
       * Formula: PF = sum(positive returns) / sum(abs(negative returns)).
       * @param xs A vector of returns.
       * @param compressResult If true, applies natural log compression to the final profit factor.
       * @return The calculated Profit Factor as a Decimal.
       */
      static Decimal computeProfitFactor(const std::vector<Decimal>& xs, bool compressResult = false)
      {
        Decimal win(DecimalConstants<Decimal>::DecimalZero);
        Decimal loss(DecimalConstants<Decimal>::DecimalZero);

        for (auto r : xs)
          {
            if (r > DecimalConstants<Decimal>::DecimalZero)
              win += r;
            else
              loss += r;
          }
	
        return computeFactor(win, loss, compressResult);
      }

      /**
       * @brief Computes the Log Profit Factor from a series of returns.
       * @details This variant takes the logarithm of each return (plus one) before summing,
       * which gives less weight to extreme outliers. Returns where (1+r) is not
       * positive are ignored.
       * Formula: LPF = sum(log(1+r>0)) / abs(sum(log(1+r<0))).
       * @param xs A vector of returns.
       * @param compressResult If true, applies an additional natural log compression to the final result.
       * @return The calculated Log Profit Factor as a Decimal.
       */
      static Decimal computeLogProfitFactor(const std::vector<Decimal>& xs, bool compressResult = false)
      {
        Decimal lw(DecimalConstants<Decimal>::DecimalZero);
        Decimal ll(DecimalConstants<Decimal>::DecimalZero);

        for (auto r : xs)
          {
            double m = 1 + num::to_double(r);
            if (m <= 0)
              continue;
	    
            Decimal lr(std::log(m));
            if (r > DecimalConstants<Decimal>::DecimalZero)
              lw += lr;
            else
              ll += lr;
          }
	
        return computeFactor(lw, ll, compressResult);
      }

      /**
       * @brief Computes the Profit Factor and the required Win Rate (Profitability).
       * @details This function efficiently calculates both the Profit Factor and the minimum
       * win rate required for a strategy to be profitable, based on the relationship
       * between Profit Factor (PF) and Payoff Ratio (Rwl).
       * Formula: P = 100 * PF / (PF + Rwl).
       * @param xs A vector of returns.
       * @return A std::tuple<Decimal, Decimal> containing:
       * - get<0>: The Profit Factor.
       * - get<1>: The required Win Rate (Profitability) as a percentage.
       */
      static std::tuple<Decimal, Decimal> computeProfitability(const std::vector<Decimal>& xs)
      {
        if (xs.empty())
        {
            return std::make_tuple(DecimalConstants<Decimal>::DecimalZero, DecimalConstants<Decimal>::DecimalZero);
        }

        Decimal grossWins(DecimalConstants<Decimal>::DecimalZero);
        Decimal grossLosses(DecimalConstants<Decimal>::DecimalZero);
        std::size_t numWinningTrades = 0;
        std::size_t numLosingTrades = 0;

        // Efficiently calculate all required components in one loop
        for (const auto& r : xs)
        {
            if (r > DecimalConstants<Decimal>::DecimalZero)
            {
                grossWins += r;
                numWinningTrades++;
            }
            else if (r < DecimalConstants<Decimal>::DecimalZero)
            {
                grossLosses += r;
                numLosingTrades++;
            }
        }

        // 1. Calculate Profit Factor (PF) by reusing the existing helper
        Decimal pf = computeFactor(grossWins, grossLosses, false);

        // 2. Calculate Payoff Ratio (Rwl = AWT / ALT)
        Decimal rwl(DecimalConstants<Decimal>::DecimalZero);
        if (numWinningTrades > 0 && numLosingTrades > 0)
        {
            Decimal awt = grossWins / Decimal(numWinningTrades);
            Decimal alt = num::abs(grossLosses) / Decimal(numLosingTrades);

            // Avoid division by zero if average loss is zero
            if (alt > DecimalConstants<Decimal>::DecimalZero)
            {
                rwl = awt / alt;
            }
        }

        // 3. Calculate Profitability (P) using the provided formula: P = 100 * PF / (PF + Rwl)
        Decimal p(DecimalConstants<Decimal>::DecimalZero);
        Decimal denominator = pf + rwl;
        if (denominator > DecimalConstants<Decimal>::DecimalZero)
        {
            p = (DecimalConstants<Decimal>::DecimalOneHundred * pf) / denominator;
        }

        return std::make_tuple(pf, p);
      }
    };
}
