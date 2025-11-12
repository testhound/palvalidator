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
    // Replace existing make_restart_mask with this run-length version.
    template <class Rng>
    inline std::vector<uint8_t> make_restart_mask(std::size_t m, double L, Rng& rng)
    {
      if (m < 2)
        throw std::invalid_argument("make_restart_mask: m must be >= 2");
      if (!(L >= 1.0) || !std::isfinite(L))
        throw std::invalid_argument("make_restart_mask: L must be finite and >= 1");

      const double p = (L <= 1.0) ? 1.0 : (1.0 / L);

      // geometric_distribution models the number of failures before first success.
      // We want run lengths in {1,2,...} with mean 1/p = L, so we use (k + 1).
      std::geometric_distribution<std::size_t> geo(p);

      std::vector<uint8_t> mask(m, 0u);
      std::size_t t = 0;

      // First element must be a restart.
      mask[t++] = 1u;

      while (t < m)
 {
   const std::size_t run_len = 1 + geo(mkc_timeseries::rng_utils::get_engine(rng)); // length >= 1
	  // We mark ONLY the restart at the beginning of the run.
	  // The continuation positions are zeros; the value/index resampler advances pos.
	  // Advance t by the run length: next loop iteration will mark a new restart (if any room left).
	  const std::size_t next = t + run_len;
	  if (next < m)
	    {
	      mask[t] = 1u; // new run starts at t
	    }
	  t = next;
	}

      return mask;
    }

    template <class Decimal>
    class StationaryBlockValueResampler
    {
    public:
      explicit StationaryBlockValueResampler(std::size_t L)
	: m_L(L < 1 ? 1 : L)
      {}

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
            const std::size_t take  = std::min(run, m - wrote);

            // Copy from doubled buffer; always contiguous
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

    // Adapter: make StationaryMaskValueResampler compatible with BCaBootStrap
    // (returns-by-value operator() and block jackknife API)
    template <class Decimal, class Rng = randutils::mt19937_rng>
    struct StationaryMaskValueResamplerAdapter
    {
    public:
      using Inner = palvalidator::resampling::StationaryMaskValueResampler<Decimal>;
      using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

      explicit StationaryMaskValueResamplerAdapter(std::size_t L)
 : m_inner(L)
 , m_L(std::max<std::size_t>(2, L))
      {}

      // BCa expects: vector<Decimal> operator()(x, n, rng)
      std::vector<Decimal>
      operator()(const std::vector<Decimal>& x, std::size_t n, Rng& rng) const
      {
        if (x.empty())
   {
            throw std::invalid_argument("StationaryMaskValueResamplerAdapter: empty sample.");
   }
        std::vector<Decimal> y;
        y.resize(n);
        m_inner(x, y, n, rng); // fill-by-reference
        return y;              // return-by-value to satisfy BCa
      }

      // MOutOfNPercentileBootstrap expects: void operator()(x, y, m, rng)
      void operator()(const std::vector<Decimal>& x,
                      std::vector<Decimal>& y,
                      std::size_t m,
                      Rng& rng) const
      {
        m_inner(x, y, m, rng); // delegate to inner resampler
      }

      // BCa expects a jackknife for acceleration 'a'
      // We implement a delete-block jackknife (circular, length L_eff)
      std::vector<Decimal>
      jackknife(const std::vector<Decimal>& x, const StatFn& stat) const
      {
        const std::size_t n = x.size();
        if (n < 2)
	  {
            throw std::invalid_argument("StationaryMaskValueResamplerAdapter::jackknife requires n>=2.");
	  }

        const std::size_t L_eff = std::min<std::size_t>(m_L, n - 1); // keep at least 1
        const std::size_t keep  = n - L_eff;

        std::vector<Decimal> jk(n);
        std::vector<Decimal> y(keep);

        for (std::size_t start = 0; start < n; ++start)
	  {
            // start_keep = immediately after the deleted block
            const std::size_t start_keep = (start + L_eff) % n;

            // Copy keep entries circularly: first tail then (optional) head
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

            jk[start] = stat(y);
	  }
        return jk;
      }

      std::size_t meanBlockLen() const
      {
        return m_L;
      }

      std::size_t getL() const
      {
        return m_L;
      }

    private:
      Inner        m_inner;
      std::size_t  m_L;
    };
  }
} // namespace palvalidator::resampling
