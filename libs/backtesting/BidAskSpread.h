#ifndef __BID_ASK_SPREAD_H
#define __BID_ASK_SPREAD_H 1

#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <vector>
#include "TimeSeries.h"
#include "decimal_math.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{
    /**
     * @class CorwinSchultzSpreadCalculator
     * @brief Implements the Corwin and Schultz (2012) bid-ask spread estimator.
     *
     * @details
     * This class provides static methods to calculate the estimated bid-ask spread
     * using only the high and low prices from consecutive time series entries. The
     * implementation is based on the research paper:
     *
     * **"A Simple Way to Estimate Bid-Ask Spreads from Daily High and Low Prices"**
     * by Shane A. Corwin and Paul Schultz, The Journal of Finance, 2012.
     *
     * **Core Idea of the Algorithm:**
     * The estimator is founded on the principle that the observed high-low price range
     * for a security consists of two components: one from its fundamental price volatility
     * and another from the bid-ask spread. The key insight is that the volatility
     * component scales with the length of the observation period, while the spread
     * component does not.
     *
     * By comparing the squared log-ratio of high-to-low prices over a two-day period (`gamma`)
     * with the sum of squared log-ratios of two individual one-day periods (`beta`),
     * the algorithm can isolate and solve for the spread.
     *
     * For future maintenance, refer to the original paper for the detailed derivation
     * of the formulas for `alpha`, `beta`, `gamma`, and the final spread `S`.
     *
     * @tparam Decimal The numeric type for price data (e.g., double, float).
     * @tparam LookupPolicy The lookup policy used by the OHLCTimeSeries, required for template matching.
     */
    template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>>
    class CorwinSchultzSpreadCalculator
    {
    private:
        // Static constant for the denominator used in alpha calculation: 3 - 2*sqrt(2)
        static const Decimal ALPHA_DENOMINATOR;
        
    public:
        // SECTION: Proportional (Percentage) Spread Calculation

        /**
         * @brief Calculates the proportional (percentage) bid-ask spread for a single two-day period.
         *
         * The calculation is performed for the period covering `date_t1` and the
         * immediately preceding entry in the time series. The result is a decimal ratio (e.g., 0.01 for 1%).
         *
         * @param series The OHLC time series containing the price data.
         * @param date_t1 The ptime for the second day (day t+1) of the two-day estimation period.
         * @return The estimated proportional spread. Can be negative in volatile conditions.
         * @throws std::runtime_error if data for the required two consecutive days cannot be found.
         */
        static Decimal calculateProportionalSpread(const OHLCTimeSeries<Decimal, LookupPolicy>& series,
                                                   const boost::posix_time::ptime& date_t1)
        {
            try
            {
                const auto entry_t1 = series.getTimeSeriesEntry(date_t1, 0);
                const auto entry_t0 = series.getTimeSeriesEntry(date_t1, 1);

                return calculateProportionalSpread(entry_t0, entry_t1);
            }
            catch (const TimeSeriesException& e)
            {
                throw std::runtime_error(
                    "CorwinSchultzSpreadCalculator: Could not find data for the two consecutive periods ending on " +
                    boost::posix_time::to_simple_string(date_t1) + ". Original error: " + e.what());
            }
        }

        /**
         * @brief Calculates the proportional (percentage) bid-ask spread from two consecutive OHLC entries.
         *
         * This overload allows direct calculation if you have already fetched the entries.
         * The result is a decimal ratio (e.g., 0.01 for 1%).
         *
         * @param entry_t0 The OHLC entry for the first day (day t).
         * @param entry_t1 The OHLC entry for the second day (day t+1).
         * @return The estimated proportional spread.
         */
        static Decimal calculateProportionalSpread(const OHLCTimeSeriesEntry<Decimal>& entry_t0,
                                                   const OHLCTimeSeriesEntry<Decimal>& entry_t1)
        {
            // This is the core calculation logic from the paper.
            // The goal is to isolate the spread component from the volatility component
            // by comparing the price range over different time intervals.

            // Frequently used constants for better readability
            const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
            const Decimal one = DecimalConstants<Decimal>::DecimalOne;
            const Decimal two = DecimalConstants<Decimal>::DecimalTwo;

            Decimal H0 = entry_t0.getHighValue();
            Decimal L0 = entry_t0.getLowValue();
            Decimal H1 = entry_t1.getHighValue();
            Decimal L1 = entry_t1.getLowValue();

            if (L0 <= zero || L1 <= zero)
            {
                throw std::domain_error("CorwinSchultzSpreadCalculator: Low price cannot be zero or negative.");
            }

            // Determine the highest high and lowest low over the combined two-day period.
            Decimal H_two_day = std::max(H0, H1);
            Decimal L_two_day = std::min(L0, L1);
            
            // --- Step 1: Calculate Beta (β) ---
            // Beta represents the sum of the squared "apparent" volatility for two
            // individual one-day periods. Each day's High-Low range contains both true
            // volatility and the bid-ask spread. Therefore, beta captures the effect
            // of two days of volatility PLUS two instances of the spread.
            // Formula: beta = [ln(H0/L0)]^2 + [ln(H1/L1)]^2
            Decimal log_ratio_t0 = std::log(H0 / L0);
            Decimal log_ratio_t1 = std::log(H1 / L1);
            Decimal beta = std::pow(log_ratio_t0, 2) + std::pow(log_ratio_t1, 2);

            // --- Step 2: Calculate Gamma (γ) ---
            // Gamma represents the squared "apparent" volatility over a single two-day
            // period. It captures the same two days of true volatility as beta, but because
            // it's a single continuous range (from the highest high to the lowest low),
            // it only captures ONE instance of the bid-ask spread.
            // Formula: gamma = [ln(H_two_day/L_two_day)]^2
            Decimal gamma = std::pow(std::log(H_two_day / L_two_day), 2);

            // --- Step 3: Solve for Alpha (α) ---
            // The difference between beta and gamma is the key to isolating the spread.
            // Alpha is an intermediate variable derived by solving the system of equations
            // for the spread. A negative gamma term in the formula reflects that a larger
            // two-day range (higher gamma) implies a smaller spread, all else being equal.
            // Formula: alpha = (sqrt(2*beta) - sqrt(beta)) / (3 - 2*sqrt(2)) - sqrt(gamma / (3 - 2*sqrt(2)))
            if (ALPHA_DENOMINATOR <= zero) {
                 throw std::runtime_error("CorwinSchultzSpreadCalculator: Internal math error, alpha denominator is non-positive.");
            }
            
            Decimal term_beta = std::sqrt(beta);
            Decimal term_2beta = std::sqrt(two * beta);

            Decimal first_term = (term_2beta - term_beta) / ALPHA_DENOMINATOR;
            Decimal second_term = zero;
            // The term under the square root can be negative if gamma is very large,
            // indicating high volatility that swamps the spread effect. This leads to a negative spread estimate.
            if ((gamma / ALPHA_DENOMINATOR) >= zero)
            {
                 second_term = std::sqrt(gamma / ALPHA_DENOMINATOR);
            }
            Decimal alpha = first_term - second_term;

            // --- Step 4: Calculate the Spread (S) ---
            // This final step converts the isolated alpha component back into a proportional spread percentage.
            // Formula: S = (2 * (e^alpha - 1)) / (1 + e^alpha)
            Decimal exp_alpha = std::exp(alpha);
            Decimal spread = (two * (exp_alpha - one)) / (one + exp_alpha);

            return spread;
        }

        /**
         * @brief Calculates the average proportional bid-ask spread over an entire time series.
         *
         * This method iterates through all overlapping two-day periods, calculates the
         * proportional spread for each, and returns the average. Negative spreads are floored at zero.
         *
         * @param series The OHLC time series to analyze.
         * @return The average proportional spread.
         */
        static Decimal calculateAverageProportionalSpread(const OHLCTimeSeries<Decimal, LookupPolicy>& series)
        {
            auto spreads = calculateProportionalSpreadsVector(series);
            if (spreads.empty())
            {
                return DecimalConstants<Decimal>::DecimalZero;
            }
            
            Decimal sum = std::accumulate(spreads.begin(), spreads.end(), DecimalConstants<Decimal>::DecimalZero);
            return sum / spreads.size();
        }

        /**
         * @brief Calculates a vector of proportional bid-ask spreads for all overlapping periods.
         *
         * Iterates through all overlapping two-day periods and returns a vector of the
         * resulting proportional spreads. Negative spreads are floored at zero.
         *
         * @param series The OHLC time series to analyze.
         * @return A std::vector<Decimal> containing the proportional spread for each period.
         */
        static std::vector<Decimal> calculateProportionalSpreadsVector(const OHLCTimeSeries<Decimal, LookupPolicy>& series)
        {
            std::vector<Decimal> spreads;
            if (series.getNumEntries() < 2)
            {
                return spreads;
            }
            spreads.reserve(series.getNumEntries() - 1);

            auto it = series.beginSortedAccess();
            auto prev_it = it;
            it++;

            for (; it != series.endSortedAccess(); ++it, ++prev_it)
            {
                try
                {
                    Decimal spread = calculateProportionalSpread(*prev_it, *it);
                    spreads.push_back(std::max(DecimalConstants<Decimal>::DecimalZero, spread));
                }
                catch (const std::domain_error& e)
                {
                     std::cerr << "Warning: Skipping a period in vector calculation due to math error: " << e.what() << std::endl;
                }
            }
            return spreads;
        }

        // SECTION: Dollar Spread Calculation

        /**
         * @brief Calculates the estimated dollar bid-ask spread for a single two-day period.
         *
         * This is computed as the proportional spread multiplied by the closing price of the second day.
         *
         * @param series The OHLC time series containing the price data.
         * @param date_t1 The ptime for the second day (day t+1) of the two-day estimation period.
         * @return The estimated dollar spread.
         */
        static Decimal calculateDollarSpread(const OHLCTimeSeries<Decimal, LookupPolicy>& series,
                                             const boost::posix_time::ptime& date_t1)
        {
            const auto entry_t1 = series.getTimeSeriesEntry(date_t1, 0);
            const auto entry_t0 = series.getTimeSeriesEntry(date_t1, 1);

            return calculateDollarSpread(entry_t0, entry_t1);
        }

        /**
         * @brief Calculates the estimated dollar bid-ask spread from two consecutive OHLC entries.
         *
         * @param entry_t0 The OHLC entry for the first day (day t).
         * @param entry_t1 The OHLC entry for the second day (day t+1).
         * @return The estimated dollar spread.
         */
        static Decimal calculateDollarSpread(const OHLCTimeSeriesEntry<Decimal>& entry_t0,
                                             const OHLCTimeSeriesEntry<Decimal>& entry_t1)
        {
            Decimal proportionalSpread = calculateProportionalSpread(entry_t0, entry_t1);
            return proportionalSpread * entry_t1.getCloseValue();
        }

        /**
         * @brief Calculates the average dollar bid-ask spread over an entire time series.
         *
         * @param series The OHLC time series to analyze.
         * @return The average dollar spread.
         */
        static Decimal calculateAverageDollarSpread(const OHLCTimeSeries<Decimal, LookupPolicy>& series)
        {
            auto spreads = calculateDollarSpreadsVector(series);
            if (spreads.empty())
            {
                return DecimalConstants<Decimal>::DecimalZero;
            }

            Decimal sum = std::accumulate(spreads.begin(), spreads.end(), DecimalConstants<Decimal>::DecimalZero);
            return sum / spreads.size();
        }

        /**
         * @brief Calculates a vector of dollar bid-ask spreads for all overlapping periods.
         *
         * @param series The OHLC time series to analyze.
         * @return A std::vector<Decimal> containing the estimated dollar spread for each period.
         */
        static std::vector<Decimal> calculateDollarSpreadsVector(const OHLCTimeSeries<Decimal, LookupPolicy>& series)
        {
            std::vector<Decimal> spreads;
            if (series.getNumEntries() < 2)
            {
                return spreads;
            }
            spreads.reserve(series.getNumEntries() - 1);

            auto it = series.beginSortedAccess();
            auto prev_it = it;
            it++;

            for (; it != series.endSortedAccess(); ++it, ++prev_it)
            {
                try
                {
                    Decimal spread = calculateDollarSpread(*prev_it, *it);
                    spreads.push_back(std::max(DecimalConstants<Decimal>::DecimalZero, spread));
                }
                catch (const std::domain_error& e)
                {
                     std::cerr << "Warning: Skipping a period in vector calculation due to math error: " << e.what() << std::endl;
                }
            }
            return spreads;
        }
    };

    // Definition of static constant for alpha denominator: 3 - 2*sqrt(2) ≈ 0.171572875
    template <class Decimal, class LookupPolicy>
    const Decimal CorwinSchultzSpreadCalculator<Decimal, LookupPolicy>::ALPHA_DENOMINATOR =
        DecimalConstants<Decimal>::DecimalThree - DecimalConstants<Decimal>::DecimalTwo * DecimalSqrtConstants<Decimal>::getSqrt(2);

} // namespace mkc_timeseries

#endif // __BID_ASK_SPREAD_H
