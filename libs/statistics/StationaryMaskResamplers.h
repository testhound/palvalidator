// StationaryMaskResamplers.h
#pragma once
#include <vector>
#include <random>
#include <stdexcept>
#include <cstddef>
#include <cmath>
#include <functional>
#include "randutils.hpp"
#include "RngUtils.h"

namespace palvalidator
{
  namespace resampling
  {
    /**
     * @brief Build a Bernoulli “restart mask” for stationary block resampling.
     *
     * The mask encodes where a new block begins when generating a stationary bootstrap
     * sequence. By convention, mask[0] == 1 to force a restart on the first element.
     * For t > 0, a restart occurs with probability p = 1/L where L is the mean block
     * length. When mask[t] == 1 a new block starts at position t; otherwise the block
     * continues by advancing the source index by +1 (with wraparound).
     *
     * @tparam Rng
     *   A RNG type that exposes `rng.engine()` returning a C++17 UniformRandomBitGenerator.
     *
     * @param m
     *   Length of the output mask (i.e., length of the resampled series). Must be >= 2.
     * @param L
     *   Mean block length (stationary bootstrap parameter). Must be finite and >= 1.
     *   The effective restart probability is p = 1/L (clamped to 1.0 if L == 1).
     * @param rng
     *   RNG instance (passed by reference) used to draw Bernoulli restarts.
     *
     * @return std::vector<uint8_t>
     *   A vector of length m with 1 marking the start of a new block and 0 otherwise.
     *
     * @throws std::invalid_argument
     *   If m < 2, or L < 1, or L is not finite.
     *
     * @note
     *   This function just draws the restart decisions; callers decide how to map
     *   blocks to actual indices or values. The first element is always a restart.
     *
     * @see StationaryMaskValueResampler, StationaryMaskIndexResampler
     */
// Corrected Bernoulli version of make_restart_mask
    template <class Rng>
    inline std::vector<uint8_t> make_restart_mask(std::size_t m, double L, Rng& rng)
    {
      if (m < 2)
        throw std::invalid_argument("make_restart_mask: m must be >= 2");
      if (!(L >= 1.0) || !std::isfinite(L))
        throw std::invalid_argument("make_restart_mask: L must be finite and >= 1");

      // Stationary Bootstrap definition:
      // A new block starts at any time t with probability p = 1/L.
      // mask[0] is always 1 (by convention/necessity).
      
      // Handle edge cases for numerical stability
      const double p = (L <= 1.0) ? 1.0 : (1.0 / L);
      
      // For very large L (p very small), probability may underflow.
      // Set a minimum threshold: if p < epsilon, treat as "one long block"
      constexpr double min_p = std::numeric_limits<double>::epsilon() * 10.0;
      const bool effectively_infinite_L = (p < min_p && L > 1.0);

      std::vector<uint8_t> mask(m);
      mask[0] = 1u; // Always restart at t=0

      if (effectively_infinite_L)
      {
          // L is so large that p ≈ 0. Just make one block (no restarts after t=0).
          // This avoids underflow issues with bernoulli_distribution.
          std::fill(mask.begin() + 1, mask.end(), 0u);
      }
      else
      {
          std::bernoulli_distribution bern(p);
          
          // For t=1 to m-1, decide if we restart
          for (std::size_t t = 1; t < m; ++t) {
              // get_engine(rng) is available via your existing include "RngUtils.h"
              mask[t] = bern(mkc_timeseries::rng_utils::get_engine(rng)) ? 1u : 0u;
          }
      }

      return mask;
    }

    template <class Decimal>
    class StationaryBlockValueResampler
    {
    public:
      explicit StationaryBlockValueResampler(std::size_t L)
	: m_L(L)
      {
	if (m_L < 1)
	  {
	    throw std::invalid_argument("StationaryBlockValueResampler: L must be >= 1");
	  }
      }

      std::size_t getL() const
      {
	return m_L;
      }

      template <class Rng>
      void operator()(const std::vector<Decimal>& x,
		      std::vector<Decimal>& y,
		      std::size_t m,
		      Rng& rng) const
      {
        const std::size_t n = x.size();

	// NEW: strict validation to match mask resamplers & unit tests
	if (n < 2)
	  {
	    throw std::invalid_argument("StationaryBlockValueResampler: x.size() must be >= 2");
	  }
	if (m < 2)
	  {
	    throw std::invalid_argument("StationaryBlockValueResampler: m must be >= 2");
	  }

        // Build doubled buffer once per call for contiguous copies across wrap.
        std::vector<Decimal> x2;
        x2.reserve(n * 2);
        x2.insert(x2.end(), x.begin(), x.end());
        x2.insert(x2.end(), x.begin(), x.end());

        // Geometric block lengths with mean L → p = 1/L
        const double p = (m_L <= 1 ? 1.0 : 1.0 / static_cast<double>(m_L));
        std::geometric_distribution<std::size_t> geo(p);
        std::uniform_int_distribution<std::size_t> ustart(0, n - 1);

        y.resize(m);
        std::size_t wrote = 0;

        while (wrote < m)
	  {
            const std::size_t start = ustart(mkc_timeseries::rng_utils::get_engine(rng));
            const std::size_t run   = 1 + geo(mkc_timeseries::rng_utils::get_engine(rng));      // length ≥ 1
            
            // CRITICAL FIX: Allow blocks to extend beyond n by using the doubled buffer
            // The doubled buffer allows reading up to n contiguous elements from any start in [0, n-1]
            // But we can actually read more if the block wraps - up to 2*n total is safe
            // For blocks that need to wrap multiple times, we limit to what we can safely copy
            const std::size_t max_from_start = (n * 2) - start;
            const std::size_t take  = std::min({run, m - wrote, max_from_start});

            // Copy from doubled buffer; always contiguous and safe
            std::copy_n(x2.begin() + static_cast<std::ptrdiff_t>(start),
                        static_cast<std::ptrdiff_t>(take),
                        y.begin() + static_cast<std::ptrdiff_t>(wrote));

            wrote += take;
	  }
      }

    private:
      std::size_t m_L;
    };
    
    /**
     * @brief Stationary bootstrap resampler that returns resampled values (“value mode”).
     *
     * Implements Politis–Romano’s stationary bootstrap using the restart-mask approach:
     * each output position either starts a new block (random uniform start in [0, n-1])
     * or continues the previous block by advancing the source index by +1 with wraparound.
     *
     * This variant copies the *values* from the input series into the output.
     * Use @ref StationaryMaskIndexResampler if you need only the index stream
     * (e.g., to synchronize multiple series using shared indices).
     *
     * @tparam Decimal  Numeric value type of the series (e.g., dec::decimal<8>).
     */
    template <class Decimal>
    class StationaryMaskValueResampler
    {
    public:
      /**
       * @brief Construct a value-mode stationary resampler.
       *
       * @param mean_block_length
       *   Mean block length L used to parameterize the restart probability p = 1/L.
       *   Must be >= 1.
       *
       * @throws std::invalid_argument
       *   If mean_block_length < 1.
       */
      explicit StationaryMaskValueResampler(std::size_t mean_block_length)
	: m_L(mean_block_length)
      {
	if (m_L < 1)
	  {
	    throw std::invalid_argument("StationaryMaskValueResampler: L must be >= 1");
	  }
      }

      /**
       * @brief Resample @p x into @p y using the stationary bootstrap (value mode).
       *
       * @tparam Rng
       *   A RNG type that exposes `rng.engine()` returning a UniformRandomBitGenerator.
       *
       * @param x
       *   Input series of length n (n >= 2).
       * @param y
       *   Output buffer that will be resized to m and filled with resampled values.
       * @param m
       *   Desired output length (m >= 2).
       * @param rng
       *   RNG used for both restart mask draws and random block starts.
       *
       * @throws std::invalid_argument
       *   If x.size() < 2 or m < 2.
       *
       * @complexity
       *   O(m). Memory: O(m) for the temporary restart mask.
       *
       * @details
       *   Algorithm:
       *   1. Draw a restart mask of length m using p = 1/L.
       *   2. For each t in [0, m):
       *      - If mask[t] == 1 or we have not yet chosen a start, pick a new start
       *        `pos ~ Uniform{0, …, n-1}`.
       *      - Else continue the previous block: `pos = (pos + 1) % n`.
       *      - Emit `y[t] = x[pos]`.
       */
      template <class Rng>
      void operator()(const std::vector<Decimal>& x,
		      std::vector<Decimal>&       y,
		      std::size_t                 m,
		      Rng&                         rng) const
      {
	const std::size_t n = x.size();
	if (n < 2)
	  {
	    throw std::invalid_argument("StationaryMaskValueResampler: x.size() must be >= 2");
	  }
	if (m < 2)
	  {
	    throw std::invalid_argument("StationaryMaskValueResampler: m must be >= 2");
	  }

	y.resize(m);

	const auto mask = make_restart_mask(m, static_cast<double>(m_L), rng);
	std::uniform_int_distribution<std::size_t> ustart(0, n - 1);

	std::size_t pos = 0;
	bool have_pos = false;

	for (std::size_t t = 0; t < m; ++t)
	  {
	    if (mask[t] || !have_pos)
	      {
	 pos = ustart(mkc_timeseries::rng_utils::get_engine(rng));
		have_pos = true;
	      }
	    else
	      {
		++pos;
		if (pos == n)
		  pos = 0;
	      }
	    y[t] = x[pos];
	  }
      }

      /**
       * @brief Get the mean block length L used by this resampler.
       * @return L as provided at construction.
       */
      std::size_t getL() const
      {
	return m_L;
      }

    private:
      std::size_t m_L;
    };

    /**
     * @brief Stationary bootstrap resampler that outputs only indices (“index mode”).
     *
     * This variant emits the index trajectory that would be used to copy values from
     * an input series when performing stationary resampling. It is useful when you need
     * to synchronize resampling across multiple strategies/series by sharing the same
     * index stream (i.e., cross-strategy dependence preservation).
     *
     * @note
     *   Downstream code can map indices to values independently for each series.
     */
    class StationaryMaskIndexResampler
    {
    public:
      /**
       * @brief Construct an index-mode stationary resampler.
       *
       * @param mean_block_length
       *   Mean block length L used to parameterize the restart probability p = 1/L.
       *   Must be >= 1.
       *
       * @throws std::invalid_argument
       *   If mean_block_length < 1.
       */
      explicit StationaryMaskIndexResampler(std::size_t mean_block_length)
	: m_L(mean_block_length)
      {
	if (m_L < 1)
	  {
	    throw std::invalid_argument("StationaryMaskIndexResampler: L must be >= 1");
	  }
      }

      /**
       * @brief Emit a stationary-bootstrap index stream of length @p m over @p n elements.
       *
       * @tparam Rng
       *   A RNG type that exposes `rng.engine()` returning a UniformRandomBitGenerator.
       *
       * @param n
       *   Size of the conceptual source array (n >= 2).
       * @param out_idx
       *   Output buffer resized to m and filled with indices in [0, n-1].
       * @param m
       *   Desired output length (m >= 2).
       * @param rng
       *   RNG used for both restart mask draws and random block starts.
       *
       * @throws std::invalid_argument
       *   If n < 2 or m < 2.
       *
       * @complexity
       *   O(m). Memory: O(m) for the temporary restart mask.
       *
       * @details
       *   Same algorithm as value mode, but writes indices instead of values:
       *   1. Draw restart mask using p = 1/L.
       *   2. For each t in [0, m): choose a new random start index when mask[t] == 1,
       *      else advance by +1 with wraparound. Store the index in @p out_idx[t].
       */
      template <class Rng>
      void operator()(std::size_t             n,
		      std::vector<std::size_t>& out_idx,
		      std::size_t             m,
		      Rng&                     rng) const
      {
	if (n < 2)
	  {
	    throw std::invalid_argument("StationaryMaskIndexResampler: n must be >= 2");
	  }
	if (m < 2)
	  {
	    throw std::invalid_argument("StationaryMaskIndexResampler: m must be >= 2");
	  }

	out_idx.resize(m);

	const auto mask = make_restart_mask(m, static_cast<double>(m_L), rng);
	std::uniform_int_distribution<std::size_t> ustart(0, n - 1);

	std::size_t pos = 0;
	bool have_pos = false;

	for (std::size_t t = 0; t < m; ++t)
	  {
	    if (mask[t] || !have_pos)
	      {
	 pos = ustart(mkc_timeseries::rng_utils::get_engine(rng));
		have_pos = true;
	      }
	    else
	      {
		pos = (pos + 1) % n;
	      }
	    out_idx[t] = pos;
	  }
      }

      /**
       * @brief Get the mean block length L used by this resampler.
       * @return L as provided at construction.
       */
      std::size_t getL() const
      {
	return m_L;
      }

    private:
      std::size_t m_L;
    };

    // ============================================================================
    // Adapter: make StationaryMaskValueResampler compatible with BCaBootStrap
    // (returns-by-value operator() and block jackknife API)
    //
    // REFACTORED for trade-level bootstrap support while maintaining 100%
    // backward compatibility with existing bar-level code.
    //
    // Template parameter T can be:
    //   - Decimal (bar-level): Traditional usage
    //   - Trade<Decimal> (trade-level): New capability
    //
    // The adapter delegates resampling to StationaryMaskValueResampler<T> and
    // provides both traditional and generic jackknife overloads for BCa.
    // ============================================================================
    template <class T, class Rng = randutils::mt19937_rng>
    struct StationaryMaskValueResamplerAdapter
    {
    public:
      using Inner = palvalidator::resampling::StationaryMaskValueResampler<T>;
      
      // BACKWARD COMPATIBILITY: Traditional StatFn typedef (T -> T)
      // Existing code may reference this type explicitly
      using StatFn = std::function<T(const std::vector<T>&)>;

      /**
       * @brief Construct the adapter with a mean block length.
       * @param L Mean block length for stationary bootstrap (must be >= 1)
       * @throws std::invalid_argument if L < 1
       */
      explicit StationaryMaskValueResamplerAdapter(std::size_t L)
        : m_inner(L)
        , m_L(L)
      {}

      /**
       * @brief Resample and return by value (BCa interface).
       * 
       * This is the signature expected by BCaBootStrap for generating
       * bootstrap resamples.
       *
       * @param x Input sample of length n_orig
       * @param n Desired output length (typically same as x.size())
       * @param rng Random number generator
       * @return Resampled vector of length n
       * @throws std::invalid_argument if x is empty
       */
      std::vector<T>
      operator()(const std::vector<T>& x, std::size_t n, Rng& rng) const
      {
        if (x.empty())
          {
            throw std::invalid_argument("StationaryMaskValueResamplerAdapter: empty sample.");
          }
        std::vector<T> y;
        y.resize(n);
        m_inner(x, y, n, rng); // fill-by-reference
        return y;              // return-by-value to satisfy BCa
      }

      /**
       * @brief Resample by filling an output buffer (MOutOfNPercentileBootstrap interface).
       *
       * This is the signature expected by MOutOfNPercentileBootstrap which
       * provides its own output buffer.
       *
       * @param x Input sample
       * @param y Output buffer (resized and filled by this call)
       * @param m Desired output length
       * @param rng Random number generator
       */
      void operator()(const std::vector<T>& x,
                      std::vector<T>& y,
                      std::size_t m,
                      Rng& rng) const
      {
        m_inner(x, y, m, rng); // delegate to inner resampler
      }

      /**
       * @brief Delete-block jackknife for BCa acceleration constant.
       * BACKWARD COMPATIBLE VERSION: Traditional signature (T -> T)
       *
       * This overload is called when the statistic function matches the
       * traditional StatFn signature (same type in and out).
       *
       * Existing bar-level code uses this automatically.
       *
       * @param x Input sample of length n
       * @param stat Statistic function (std::function<T(const std::vector<T>&)>)
       * @return Vector of jackknife pseudo-values
       */
      std::vector<T>
      jackknife(const std::vector<T>& x, const StatFn& stat) const
      {
        // Delegate to the generic implementation
        return jackknife_impl(x, stat);
      }

      /**
       * @brief Delete-block jackknife for BCa acceleration constant.
       * GENERIC VERSION: Supports statistics with different return type (Trade<Decimal> -> Decimal)
       *
       * This overload is called when the statistic function does NOT match
       * the traditional StatFn signature. It auto-deduces the return type.
       *
       * Enabled only when StatFunc is not convertible to StatFn (avoids ambiguity).
       *
       * Trade-level code uses this automatically when stat returns Decimal
       * from vector<Trade<Decimal>>.
       *
       * @param x Input sample of length n
       * @param stat Statistic function (any callable with compatible signature)
       * @return Vector of jackknife pseudo-values (type auto-deduced)
       */
      template <class StatFunc,
                typename = typename std::enable_if<
                  !std::is_convertible<StatFunc, StatFn>::value
                >::type>
      auto jackknife(const std::vector<T>& x, const StatFunc& stat) const
        -> std::vector<decltype(stat(x))>
      {
        return jackknife_impl(x, stat);
      }

    private:
      /**
       * @brief Common implementation for both jackknife overloads.
       *
       * Implements Künsch (1989) delete-block jackknife:
       * - Deletes non-overlapping blocks of length L_eff
       * - Steps by L_eff each iteration (NOT sliding window)
       * - Returns floor(n/L_eff) pseudo-values (NOT n)
       *
       * This avoids the systematic underestimation of |a| caused by
       * highly-correlated pseudo-values in sliding-window approaches.
       *
       * @param x Input sample of length n
       * @param stat Statistic function
       * @return Vector of jackknife pseudo-values
       * @throws std::invalid_argument if n < 3 or sample too small for block length
       */
      template <class StatFunc>
      auto jackknife_impl(const std::vector<T>& x, const StatFunc& stat) const
        -> std::vector<decltype(stat(x))>
      {
        using ResultType = decltype(stat(x));
        
        const std::size_t n = x.size();

        // --- Guard 1: Absolute minimum for any jackknife ---
        // Need at least L_eff observations to delete AND enough remaining
        // observations for the statistic to be defined (minKeep >= 2).
        const std::size_t minKeep = 2;
        if (n < minKeep + 1)
          {
            throw std::invalid_argument(
                                        "StationaryMaskValueResamplerAdapter::jackknife requires n >= 3.");
          }

        // --- Guard 2: Clamp L_eff so we always retain at least minKeep observations ---
        // Without this, near n==L cases produce keep==1 and degenerate stat calls.
        const std::size_t L_eff = std::min<std::size_t>(m_L, n - minKeep);

        // --- Guard 3: Ensure the sample is large enough for this block length ---
        // We need at least 2 non-overlapping blocks: one to delete, one to keep.
        if (n < L_eff + minKeep)
          {
            throw std::invalid_argument(
                                        "StationaryMaskValueResamplerAdapter::jackknife: sample too small "
                                        "for delete-block jackknife with this block length. "
                                        "Reduce block length or increase sample size.");
          }

        const std::size_t keep = n - L_eff;
        
        // --- Künsch (1989) delete-block jackknife: non-overlapping blocks only ---
        // Previously the loop ran n times (start = 0, 1, 2, ..., n-1), producing
        // n highly-correlated pseudo-values differing by only one observation.
        // This is a sliding-window delete, which overcounts and causes systematic
        // underestimation of |a| (the BCa acceleration constant).
        //
        // The correct delete-block jackknife uses only floor(n/L_eff) non-overlapping
        // blocks, stepping by L_eff each iteration. Each pseudo-value then reflects
        // the influence of a genuinely distinct segment of the time series on the
        // statistic.
        //
        // BCaBootStrap::calculateBCaBounds() is confirmed safe for variable-length
        // jackknife output: it captures jk_stats.size() into n_jk independently
        // of n, and all downstream loops are range-based.
        const std::size_t numBlocks = n / L_eff;   // number of pseudo-values returned
        
        std::vector<ResultType> jk(numBlocks);
        std::vector<T> y(keep);

        for (std::size_t b = 0; b < numBlocks; ++b)
          {
            // Non-overlapping start: advance by L_eff each iteration
            const std::size_t start = b * L_eff;

            // Circular index immediately after the deleted block
            const std::size_t start_keep = (start + L_eff) % n;

            // Copy 'keep' entries from x[start_keep ... ) with circular wrap.
            // At most two spans are needed (tail then optional head).
            const std::size_t tail = std::min<std::size_t>(keep, n - start_keep);

            std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
                        static_cast<std::ptrdiff_t>(tail),
                        y.begin());

            const std::size_t head = keep - tail;
            if (head != 0)
              {
                std::copy_n(x.begin(),
                            static_cast<std::ptrdiff_t>(head),
                            y.begin() + static_cast<std::ptrdiff_t>(tail));
              }

            jk[b] = stat(y);
          }

        return jk;   // size == numBlocks == n/L_eff, NOT n
      }

    public:
      /**
       * @brief Get the mean block length (alias for getL).
       * @return Mean block length L
       */
      std::size_t meanBlockLen() const
      {
        return m_L;
      }

      /**
       * @brief Get the mean block length.
       * @return Mean block length L
       */
      std::size_t getL() const
      {
        return m_L;
      }

    private:
      Inner        m_inner;  // StationaryMaskValueResampler<T>
      std::size_t  m_L;      // Mean block length
    };
  }
} // namespace palvalidator::resampling
