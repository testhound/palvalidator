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
      enum class NegativePolicy { ClampToZero, Skip, Epsilon };

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
       * @brief Calculates a vector of proportional bid-ask spreads for all overlapping (t-1, t) pairs.
       *
       * By default, negative per-pair estimates are clamped to zero (legacy behavior).
       * - Use NegativePolicy::Skip to drop negative pairs from the output vector.
       * - Use NegativePolicy::Epsilon to replace negative/invalid with a tiny positive ε:
       *     ε = max(1e-8, tick / Close_t)
       *   (keeps the vector length stable without piling zeros; units consistent with proportional spreads)
       *
       * @param series The OHLC time series to analyze.
       * @param tick   Dollar tick size of the instrument (e.g., 0.01 for U.S. stocks). Only used for Epsilon.
       * @param negPolicy Handling of negative per-pair estimates. Default: ClampToZero (backward compatible).
       * @return A std::vector<Decimal> of proportional spreads, length ≤ n-1.
       */
      static std::vector<Decimal>
      calculateProportionalSpreadsVector(const OHLCTimeSeries<Decimal, LookupPolicy>& series,
					 const Decimal& tick = DecimalConstants<Decimal>::DecimalZero,
					 NegativePolicy negPolicy = NegativePolicy::ClampToZero,
					 unsigned int window_len = 20)
      {
	std::vector<Decimal> spreads;
	const std::size_t n = series.getNumEntries();
	if (n < 2) {
	  return spreads;
	}

	// Guard silly windows (treat 0/1 as "no smoothing": i.e., emit on each pair as available)
	if (window_len == 0) window_len = 1;

	spreads.reserve(n - 1);

	// Constants
	const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
	const Decimal one  = DecimalConstants<Decimal>::DecimalOne;
	const Decimal two  = DecimalConstants<Decimal>::DecimalTwo;

	// alpha denominator: (3 - 2*sqrt(2)) as Decimal
	static const Decimal SQRT2 = DEC_NAMESPACE::sqrt(two);
	static const Decimal ALPHA_DEN = (Decimal(3) - two * SQRT2);

	// Epsilon helpers (scale-aware; falls back to 1e-8)
	auto epsMin = []() -> Decimal {
	  return Decimal(1) / Decimal(100000000); // 1e-8
	};
	auto epsFromTick = [&](const Decimal& close_t) -> Decimal {
	  if (tick > zero && close_t > zero) {
	    Decimal e = tick / close_t;               // proportional epsilon
	    return (e > epsMin()) ? e : epsMin();
	  }
	  return epsMin();
	};

	// Rolling buffers/sums for beta and gamma
	std::deque<Decimal> beta_q;
	std::deque<Decimal> gamma_q;
	Decimal beta_sum  = zero;
	Decimal gamma_sum = zero;

	// Iterate consecutive pairs (t-1, t)
	auto it_prev = series.beginSortedAccess();
	auto it_curr = it_prev;
	++it_curr;

	for (; it_curr != series.endSortedAccess(); ++it_curr, ++it_prev)
	  {
	    const auto& e0 = *it_prev;
	    const auto& e1 = *it_curr;

	    // Extract prices as Decimals
	    const Decimal H0 = e0.getHighValue();
	    const Decimal L0 = e0.getLowValue();
	    const Decimal H1 = e1.getHighValue();
	    const Decimal L1 = e1.getLowValue();
	    const Decimal C1 = e1.getCloseValue(); // for epsilon scaling only

	    // Basic validity (strictly positive lows/highs)
	    if (L0 <= zero || L1 <= zero || H0 <= zero || H1 <= zero) {
	      // Keep vector length behavior consistent with legacy: emit 0 per pair
	      spreads.push_back(zero);
	      continue;
	    }

	    // Per Corwin–Schultz:
	    // beta_t = [ln(H0/L0)]^2 + [ln(H1/L1)]^2
	    // gamma_t = [ln(max(H0, H1)/min(L0, L1))]^2
	    const Decimal lnH0L0 = DEC_NAMESPACE::log(H0 / L0);
	    const Decimal lnH1L1 = DEC_NAMESPACE::log(H1 / L1);

	    const Decimal maxH = (H0 > H1) ? H0 : H1;
	    const Decimal minL = (L0 < L1) ? L0 : L1;
	    const Decimal lnRange2 = DEC_NAMESPACE::log(maxH / minL);

	    const Decimal beta_t  = lnH0L0 * lnH0L0 + lnH1L1 * lnH1L1;
	    const Decimal gamma_t = lnRange2 * lnRange2;

	    // Push into rolling window
	    beta_q.push_back(beta_t);
	    gamma_q.push_back(gamma_t);
	    beta_sum  += beta_t;
	    gamma_sum += gamma_t;

	    if (beta_q.size() > window_len) {
	      beta_sum  -= beta_q.front();  beta_q.pop_front();
	      gamma_sum -= gamma_q.front(); gamma_q.pop_front();
	    }

	    // Compute smoothed means (use whatever is available until the window fills)
	    const Decimal w = static_cast<Decimal>(beta_q.size());
	    const Decimal beta_bar  = beta_sum  / w;
	    const Decimal gamma_bar = gamma_sum / w;

	    // alpha = (sqrt(2*beta_bar) - sqrt(beta_bar)) / (3 - 2*sqrt(2)) - sqrt(gamma_bar / (3 - 2*sqrt(2)))
	    // Use DEC_NAMESPACE math to stay in Decimal domain
	    // Guard tiny negatives from rounding with max(zero, ·) before sqrt
	    const Decimal term_beta  = (Decimal)DEC_NAMESPACE::sqrt((two * beta_bar > zero) ? two * beta_bar : zero)
	      - (Decimal)DEC_NAMESPACE::sqrt((beta_bar > zero) ? beta_bar : zero);
	    const Decimal term_gamma = (Decimal)DEC_NAMESPACE::sqrt((gamma_bar > zero) ? (gamma_bar / ALPHA_DEN) : zero);
	    const Decimal alpha      = (ALPHA_DEN != zero) ? (term_beta / ALPHA_DEN - term_gamma) : zero;

	    // S = 2 * (e^alpha - 1) / (1 + e^alpha)  == 2 * tanh(alpha/2)
	    // Compute in a numerically friendly way in Decimal domain
	    const Decimal exp_a = DEC_NAMESPACE::exp(alpha);
	    Decimal S = zero;
	    if (exp_a + one != zero) {
	      S = (two * (exp_a - one)) / (exp_a + one);
	    } else {
	      // Pathological only if exp_a == -1 (impossible here), fall back
	      S = zero;
	    }

	    // Handle negatives and near-zeros per policy
	    if (S <= zero) {
	      if (negPolicy == NegativePolicy::Skip) {
		continue; // drop this observation
	      } else if (negPolicy == NegativePolicy::Epsilon) {
		spreads.push_back(epsFromTick(C1));
	      } else { // ClampToZero
		spreads.push_back(zero);
	      }
	    } else {
	      // Optional tiny-floor to avoid exact zeros if Epsilon is requested
	      if (negPolicy == NegativePolicy::Epsilon && S < epsMin()) {
		spreads.push_back(epsFromTick(C1));
	      } else {
		spreads.push_back(S);
	      }
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
    enum class NegativePolicy { ClampToZero, Skip, Epsilon };

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
				       unsigned int window_len = 30,
				       const Decimal& tick = DecimalConstants<Decimal>::DecimalZero,
				       NegativePolicy negPolicy = NegativePolicy::ClampToZero,
				       bool sign = false)
    {
      const std::size_t n = series.getNumEntries();
      if (n < 2 || window_len == 0) {
	return {};
      }

      // Decimal constants for the algorithm
      const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
      const Decimal one  = DecimalConstants<Decimal>::DecimalOne;
      const Decimal two  = DecimalConstants<Decimal>::DecimalTwo;
      const Decimal four = two + two;
      const Decimal neg_four = zero - four;

      std::vector<Decimal> spreads;
      spreads.reserve(n - 1);

      // Helpers for tolerance and epsilon
      auto epsMin = []() -> Decimal {
	return Decimal(1) / Decimal(100000000); // 1e-8
      };
      auto epsFromTick = [&](const Decimal& close_t) -> Decimal {
	if (tick > zero && close_t > zero) {
	  Decimal e = tick / close_t;                // ~proportional epsilon
	  return (e > epsMin()) ? e : epsMin();      // floor at 1e-8
	}
	return epsMin();
      };
      auto almost_equal = [&](const Decimal& a, const Decimal& b, const Decimal& tol) -> bool {
	const Decimal diff = DEC_NAMESPACE::abs(a - b);
	return diff <= tol * (DEC_NAMESPACE::abs(a) + DEC_NAMESPACE::abs(b) + one);
      };

      auto it = series.beginSortedAccess();
      auto prev_it = it;
      ++it;

      // Holds per-day precomputed values
      struct MomentData {
	// rolling-means vector (34 entries)
	std::vector<Decimal> x_values;
      };

      std::deque<MomentData> window_data;

      for (; it != series.endSortedAccess(); ++it, ++prev_it)
	{
	  const auto& entry_t0 = *prev_it;
	  const auto& entry_t1 = *it;

	  // Extract OHLC prices
	  const Decimal o0 = entry_t0.getOpenValue();
	  const Decimal h0 = entry_t0.getHighValue();
	  const Decimal l0 = entry_t0.getLowValue();
	  const Decimal c0 = entry_t0.getCloseValue();

	  const Decimal o1 = entry_t1.getOpenValue();
	  const Decimal h1 = entry_t1.getHighValue();
	  const Decimal l1 = entry_t1.getLowValue();
	  const Decimal c1 = entry_t1.getCloseValue();

	  // Basic price validity
	  if (o0 <= zero || h0 <= zero || l0 <= zero || c0 <= zero ||
	      o1 <= zero || h1 <= zero || l1 <= zero || c1 <= zero)
	    {
	      // Keep vector length consistent; policy: push literal zero for invalid prints
	      spreads.push_back(zero);
	      continue;
	    }

	  // Compute adaptive tolerance using current close
	  const Decimal tol = epsFromTick(c1);

	  // Compute log-prices
	  const Decimal log_o0 = DEC_NAMESPACE::log(o0);
	  const Decimal log_h0 = DEC_NAMESPACE::log(h0);
	  const Decimal log_l0 = DEC_NAMESPACE::log(l0);
	  const Decimal log_c0 = DEC_NAMESPACE::log(c0);
	  const Decimal log_o1 = DEC_NAMESPACE::log(o1);
	  const Decimal log_h1 = DEC_NAMESPACE::log(h1);
	  const Decimal log_l1 = DEC_NAMESPACE::log(l1);
	  const Decimal log_c1 = DEC_NAMESPACE::log(c1);

	  const Decimal m0 = (log_h0 + log_l0) / two;
	  const Decimal m1 = (log_h1 + log_l1) / two;

	  // Log-returns
	  const Decimal r1 = m1 - log_o1;
	  const Decimal r2 = log_o1 - m0;
	  const Decimal r3 = m1 - log_c0;
	  const Decimal r4 = log_c0 - m0;
	  const Decimal r5 = log_o1 - log_c0;

	  // Indicator variables with tolerance tests
	  const bool hl_diff = !almost_equal(log_h1, log_l1, tol);
	  const bool lc_diff = !almost_equal(log_l1, log_c0, tol);
	  const Decimal tau  = (hl_diff || lc_diff) ? one : zero;

	  const Decimal po1 = tau * ( !almost_equal(log_o1, log_h1, tol) ? one : zero );
	  const Decimal po2 = tau * ( !almost_equal(log_o1, log_l1, tol) ? one : zero );
	  const Decimal pc1 = tau * ( !almost_equal(log_c0, log_h0, tol) ? one : zero );
	  const Decimal pc2 = tau * ( !almost_equal(log_c0, log_l0, tol) ? one : zero );

	  // Build the current moment vector (34 entries)
	  MomentData current_data;
	  current_data.x_values.resize(34);

	  // Base products for rolling means
	  current_data.x_values[0]  = r1 * r2;
	  current_data.x_values[1]  = r3 * r4;
	  current_data.x_values[2]  = r1 * r5;
	  current_data.x_values[3]  = r4 * r5;
	  current_data.x_values[4]  = tau;
	  current_data.x_values[5]  = r1;
	  current_data.x_values[6]  = tau * r2;
	  current_data.x_values[7]  = r3;
	  current_data.x_values[8]  = tau * r4;
	  current_data.x_values[9]  = r5;

	  // Squares & cross terms (as in the EDGE moment set)
	  current_data.x_values[10] = pow(r1 * r2, 2);
	  current_data.x_values[11] = pow(r3 * r4, 2);
	  current_data.x_values[12] = pow(r1 * r5, 2);
	  current_data.x_values[13] = pow(r4 * r5, 2);
	  current_data.x_values[14] = (r1 * r2) * (r3 * r4);
	  current_data.x_values[15] = (r1 * r5) * (r4 * r5);

	  current_data.x_values[16] = (tau * r2) * r2;
	  current_data.x_values[17] = (tau * r4) * r4;
	  current_data.x_values[18] = (tau * r5) * r5;

	  current_data.x_values[19] = (tau * r2) * (r1 * r2);
	  current_data.x_values[20] = (tau * r4) * (r3 * r4);
	  current_data.x_values[21] = (tau * r5) * (r1 * r5);
	  current_data.x_values[22] = (tau * r4) * (r4 * r5);
	  current_data.x_values[23] = (tau * r4) * (r1 * r2);
	  current_data.x_values[24] = (tau * r2) * (r3 * r4);

	  current_data.x_values[25] = (tau * r2) * r4;
	  current_data.x_values[26] = (tau * r1) * (r4 * r5);
	  current_data.x_values[27] = (tau * r5) * (r4 * r5);
	  current_data.x_values[28] = (tau * r4) * r5;
	  current_data.x_values[29] = tau * r5;

	  // Open/Close boundary prob. components
	  current_data.x_values[30] = po1;
	  current_data.x_values[31] = po2;
	  current_data.x_values[32] = pc1;
	  current_data.x_values[33] = pc2;

	  // Rolling window maintenance
	  window_data.push_back(std::move(current_data));
	  if (window_data.size() > window_len) {
	    window_data.pop_front();
	  }
	  if (window_data.empty()) {
	    continue;
	  }

	  // Rolling means m[0..33]
	  std::vector<Decimal> m(34, zero);
	  for (const auto& data : window_data) {
	    for (size_t i = 0; i < 34; ++i) {
	      m[i] += data.x_values[i];
	    }
	  }
	  for (size_t i = 0; i < 34; ++i) {
	    m[i] /= static_cast<Decimal>(window_data.size());
	  }

	  // Probabilities (means)
	  const Decimal pt = m[4];
	  const Decimal po = m[30] + m[31];
	  const Decimal pc = m[32] + m[33];

	  // Count of valid pairs in the window
	  Decimal nt = zero;
	  for (const auto& data : window_data) {
	    nt += data.x_values[4];   // tau
	  }

	  // Gate: emit with ≥ 1 valid pair (not 2)
	  if (nt < one) {
	    continue;
	  }

	  // Safe denominators (avoid collapsing to exact zeros)
	  const Decimal pt_safe = (pt > tol) ? pt : tol;
	  const Decimal po_safe = (po > tol) ? po : tol;
	  const Decimal pc_safe = (pc > tol) ? pc : tol;

	  try {
	    // Input vectors
	    const Decimal a1  = neg_four / po_safe;
	    const Decimal a2  = neg_four / pc_safe;
	    const Decimal a3  = m[5]  / pt_safe;
	    const Decimal a4  = m[8]  / pt_safe;
	    const Decimal a5  = m[7]  / pt_safe;
	    const Decimal a6  = m[9]  / pt_safe;

	    const Decimal a12 = two * a1 * a2;
	    const Decimal a11 = a1 * a1;
	    const Decimal a22 = a2 * a2;
	    const Decimal a33 = a3 * a3;
	    const Decimal a55 = a5 * a5;
	    const Decimal a66 = a6 * a6;

	    // Expectations
	    const Decimal e1 = a1 * (m[0] - a3 * m[6]) + a2 * (m[1] - a4 * m[7]);
	    const Decimal e2 = a1 * (m[2] - a3 * m[29]) + a2 * (m[3] - a4 * m[9]);

	    // Variances
	    const Decimal v1 = -pow(e1, 2) + (a11 * (m[10] - two * a3 * m[19] + a33 * m[16]) +
					      a22 * (m[11] - two * a5 * m[20] + a55 * m[17]) +
					      a12 * (m[14] - a3 * m[24] - a5 * m[23] + a3 * a5 * m[25]));
	    const Decimal v2 = -pow(e2, 2) + (a11 * (m[12] - two * a3 * m[21] + a33 * m[18]) +
					      a22 * (m[13] - two * a6 * m[22] + a66 * m[17]) +
					      a12 * (m[15] - a3 * m[27] - a6 * m[26] + a3 * a6 * m[28]));

	    // Square spread s^2
	    const Decimal vt = v1 + v2;
	    Decimal s2;
	    if (vt > zero) {
	      s2 = (v2 * e1 + v1 * e2) / vt;
	    } else {
	      s2 = (e1 + e2) / two;
	    }

	    // Root and sign
	    Decimal s = DEC_NAMESPACE::sqrt(DEC_NAMESPACE::abs(s2));
	    if (sign) {
	      s *= (s2 > zero) ? one : (zero - one);
	    }

	    // Apply NegativePolicy & near-zero handling
	    if (s <= tol) {
	      if (negPolicy == NegativePolicy::Skip) {
		continue;   // drop degenerate obs
	      } else if (negPolicy == NegativePolicy::Epsilon) {
		spreads.push_back(epsFromTick(c1));
		continue;
	      } // ClampToZero: fall through to push literal zero
	    }

	    spreads.push_back(s);
	  }
	  catch (const std::domain_error& e) {
	    // Preserve existing behavior—skip bad period, warn on stderr
	    std::cerr << "Warning: Skipping a period in vector calculation due to math error: "
		      << e.what() << std::endl;
	  }
	}

      return spreads;
    }
  };
} // namespace mkc_timeseries

#endif // __BID_ASK_SPREAD_H
