#ifndef __BID_ASK_SPREAD_H
#define __BID_ASK_SPREAD_H 1

#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <vector>
#include <deque>
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

  /**
   * @class EdgeSpreadCalculator
   * @brief Implements the Ardia, Guidotti, and Kroencke (2022) EDGE bid-ask spread estimator.
   *
   * @details
   * This class provides a static method to calculate a time series of the estimated
   * bid-ask spread using all four Open, High, Low, and Close (OHLC) prices. The
   * implementation is based on the research paper:
   *
   * **"Efficient Estimation of Bid-Ask Spreads from Open, High, Low, and Close Prices"**
   * by David Ardia, Emanuele Guidotti, and Tim A. Kroencke.
   *
   * **Core Idea of the Algorithm:**
   *
   * The EDGE (Efficient Discrete Generalized Estimator) model is a sophisticated and
   * statistically efficient estimator based on the Generalized Method of Moments (GMM).
   *
   * It improves upon prior methods like Corwin-Schultz by:
   * 1.  Using all available OHLC price information, not just High and Low.
   * 2.  Constructing multiple "moment estimators" from the data.
   * 3.  Optimally weighting these estimators to produce a final estimate with minimum variance.
   *
   * This specific implementation computes the EDGE spread over a **rolling window**,
   * providing a time-varying estimate of liquidity rather than a single static value.
   * The spread for day `t` is estimated using data from the `window_len` preceding trading days.
   *
   * @tparam Decimal The numeric type for price data (e.g., double, float).
   * @tparam LookupPolicy The lookup policy used by the OHLCTimeSeries, required for template matching.
   */
  template <class Decimal, class LookupPolicy = mkc_timeseries::LogNLookupPolicy<Decimal>>
  class EdgeSpreadCalculator
  {
  public:
    /**
     * @brief Calculates a vector of rolling proportional bid-ask spreads using the EDGE method.
     *
     * @details
     * For each trading day `t` in the series (starting from the second day), this method
     * estimates the proportional spread `S` by looking at a window of the preceding `window_len`
     * valid trading day pairs. The result is a time series of spread estimates.
     *
     * @param series The OHLC time series to analyze.
     * @param window_len The number of trading days in the rolling window used for estimation.
     * @param eps A small tolerance value for floating-point comparisons (e.g., checking if Open == High).
     * @return A std::vector<Decimal> containing the proportional spread for each period. The vector
     * will be shorter than the input series, as the first estimate is produced at t=2.
     */
    static std::vector<Decimal>
    calculateProportionalSpreadsVector(const OHLCTimeSeries<Decimal, LookupPolicy>& series,
                                       unsigned window_len = 30,
                                       const Decimal& eps = DecimalConstants<Decimal>::DecimalZero)
    {
        std::vector<Decimal> out;
        const std::size_t n = series.getNumEntries();
        if (n < 2 || window_len == 0) return out;

        out.reserve(n - 1);

        
        // --- ALGORITHM SETUP ---
        // Initialize iterators to traverse adjacent (t-1, t) pairs of days.
        auto it = series.beginSortedAccess();
        auto prev_it = it; ++it;

        // The deque `win` will store the records for all valid pairs within the current rolling window.
        // It allows for efficient addition of new pairs (push_back) and removal of old pairs (pop_front).
        struct PairRec {
            Decimal X1, X2;
            bool oeqh, oeql, ceqh, ceql;  // day-t extremes
            std::size_t t_idx;            // 1-based index of day t in the full series
        };
        std::deque<PairRec> win;

        // Running sums are maintained for O(1) updates as the window slides. This is far more
        // efficient than recalculating sums over the whole window at each step.
        Decimal sumX1 = DecimalConstants<Decimal>::DecimalZero;
        Decimal sumX2 = DecimalConstants<Decimal>::DecimalZero;
        Decimal sumSqX1 = DecimalConstants<Decimal>::DecimalZero;
        Decimal sumSqX2 = DecimalConstants<Decimal>::DecimalZero;
        std::size_t cnt_oh = 0, cnt_ol = 0, cnt_ch = 0, cnt_cl = 0;

        const auto zero = DecimalConstants<Decimal>::DecimalZero;
        const auto one  = DecimalConstants<Decimal>::DecimalOne;
        const auto two  = DecimalConstants<Decimal>::DecimalTwo;
        const auto four = two + two;
        const Decimal half = one / two;

        auto leqAbs = [&](const Decimal& d){ return d.abs() <= eps; };

        // 1-based day index for the "t" side of each pair
        std::size_t t_index = 1;

        for (; it != series.endSortedAccess(); ++it, ++prev_it, ++t_index) {
            const auto& e_tm1 = *prev_it;
            const auto& e_t   = *it;

            const Decimal O_tm1 = e_tm1.getOpenValue();
            const Decimal H_tm1 = e_tm1.getHighValue();
            const Decimal L_tm1 = e_tm1.getLowValue();
            const Decimal C_tm1 = e_tm1.getCloseValue();

            const Decimal O_t = e_t.getOpenValue();
            const Decimal H_t = e_t.getHighValue();
            const Decimal L_t = e_t.getLowValue();
            const Decimal C_t = e_t.getCloseValue();

            // --- Step 1: Filter out invalid pairs ---
            // The model requires log-prices, so all prices must be positive.
            bool valid_pair = (O_tm1 > zero && H_tm1 > zero && L_tm1 > zero && C_tm1 > zero &&
                               O_t   > zero && H_t   > zero && L_t   > zero && C_t   > zero);

            // "No-trade" heuristic: H_t == L_t == C_{t-1} → skip pair if true.
            if (valid_pair && leqAbs(H_t - L_t) && leqAbs(H_t - C_tm1)) {
                valid_pair = false;
            }

	    // --- Step 2: Calculate moment estimators for the current valid pair ---
            if (valid_pair) {
                const Decimal o_tm1 = DEC_NAMESPACE::log(O_tm1);
                const Decimal h_tm1 = DEC_NAMESPACE::log(H_tm1);
                const Decimal l_tm1 = DEC_NAMESPACE::log(L_tm1);
                const Decimal c_tm1 = DEC_NAMESPACE::log(C_tm1);

                const Decimal o_t = DEC_NAMESPACE::log(O_t);
                const Decimal h_t = DEC_NAMESPACE::log(H_t);
                const Decimal l_t = DEC_NAMESPACE::log(L_t);
                const Decimal c_t = DEC_NAMESPACE::log(C_t);

		// eta (η) is the log of the geometric mean of High and Low, i.e., log(sqrt(H*L)),
                // which simplifies to (log(H) + log(L)) / 2. It represents the log-midprice.
                const Decimal eta_tm1 = (h_tm1 + l_tm1) * half;
                const Decimal eta_t   = (h_t   + l_t)   * half;

                // These are the core moment estimators from the EDGE paper (small-mean approximation).
                // They are constructed such that their expected value is a function of the squared spread S^2.
                // E[X1] ≈ -S^2/2, E[X2] ≈ -S^2/2
                const Decimal ot_minus_ctm1 = (o_t - c_tm1);
                const Decimal X1 = (eta_t - o_t) * ot_minus_ctm1
                                 +  ot_minus_ctm1 * (c_tm1 - eta_tm1);

                // X2_t = (η_t - o_t)(o_t - η_{t-1}) + (η_t - c_{t-1})(c_{t-1} - η_{t-1})
                const Decimal X2 = (eta_t - o_t)   * (o_t   - eta_tm1)
                                 + (eta_t - c_tm1) * (c_tm1 - eta_tm1);

		// Determine if the open or close price was the extreme price of the day.
                // This is used to calculate the `nu` correction factor later.
                const bool oeqh = leqAbs(O_t - H_t);
                const bool oeql = leqAbs(O_t - L_t);
                const bool ceqh = leqAbs(C_t - H_t);
                const bool ceql = leqAbs(C_t - L_t);

                // --- Step 3: Update the rolling window with the new pair ---
                // Add the new pair's data to the back of the deque and update running sums.
                win.push_back({X1, X2, oeqh, oeql, ceqh, ceql, t_index});
                sumX1 += X1;      sumSqX1 += X1 * X1;
                sumX2 += X2;      sumSqX2 += X2 * X2;
                if (oeqh)
		  ++cnt_oh;

		if (oeql)
		  ++cnt_ol;

                if (ceqh)
		  ++cnt_ch;

		if (ceql)
		  ++cnt_cl;
            }

            // --- Step 4: Eject old pairs that have fallen out of the window ---
            // Determine the leftmost index that should be included in the window for day `t`.
            const std::size_t left = (t_index >= window_len) ? (t_index - window_len + 1) : 1;
            while (!win.empty() && win.front().t_idx < left) {
                const auto& r = win.front();
                sumX1 -= r.X1;   sumSqX1 -= r.X1 * r.X1;
                sumX2 -= r.X2;   sumSqX2 -= r.X2 * r.X2;
		
                if (r.oeqh)
		  --cnt_oh;
		if (r.oeql)
		  --cnt_ol;
                if (r.ceqh)
		  --cnt_ch;
		if (r.ceql)
		  --cnt_cl;

                win.pop_front();
            }

            // --- Step 5: Compute the EDGE spread for day `t` using the current window's data ---
            const std::size_t Npairs = win.size();
            if (Npairs == 0) {
                continue; // nothing to emit for this t
            }

            const Decimal N = Decimal(Npairs);
            const Decimal EX1 = sumX1 / N;
            const Decimal EX2 = sumX2 / N;

            // Sample variances (guard Npairs==1)
            Decimal VX1 = zero, VX2 = zero;
            if (Npairs >= 2) {
                VX1 = (sumSqX1 - (sumX1 * sumX1) / N) / Decimal(Npairs - 1);
                VX2 = (sumSqX2 - (sumX2 * sumX2) / N) / Decimal(Npairs - 1);
            }

            // Calculate the diagonal-optimal weights to minimize the variance of the final estimator.
            // This is the "Efficient" part of the EDGE model.
            const Decimal denomV = VX1 + VX2;
            const Decimal w1 = (denomV > zero) ? (VX2 / denomV) : half;
            const Decimal w2 = (denomV > zero) ? (VX1 / denomV) : half;

            // Calculate `nu` (ν), the frequency of the open/close being an extreme price.
            // This term corrects for the fact that trades at the open or close can influence the range.
            const Decimal nu_oh = Decimal(cnt_oh) / N;
            const Decimal nu_ol = Decimal(cnt_ol) / N;
            const Decimal nu_ch = Decimal(cnt_ch) / N;
            const Decimal nu_cl = Decimal(cnt_cl) / N;

            const Decimal nu_open  = (nu_oh + nu_ol) * half;
            const Decimal nu_close = (nu_ch + nu_cl) * half;
            const Decimal nu_avg   = (nu_open + nu_close) * half;

            // The final EDGE estimator for the squared spread (S^2).
            // Formula: S^2 = max(0, -2*(w1*E[X1]+w2*E[X2]) / (1 - 4*w1*w2*nu_avg))
            const Decimal k = four * w1 * w2;
            const Decimal denom = one - k * nu_avg;
            Decimal S2 = (denom > zero)
                       ? (-DecimalConstants<Decimal>::DecimalTwo * (w1 * EX1 + w2 * EX2) / denom)
                       : zero;
            if (S2 < zero) S2 = zero;

            out.push_back(DEC_NAMESPACE::sqrt(S2));
        }

        return out;
    }
  };
  
} // namespace mkc_timeseries

#endif // __BID_ASK_SPREAD_H
