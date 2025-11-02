#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <random>
#include <limits>

namespace mkc_timeseries
{
  namespace rng_utils
  {

    // --- Detection: does Rng have .engine()? (e.g., randutils::mt19937_rng) ---
    template <typename T, typename = void>
    struct has_engine_method : std::false_type {};

    template <typename T>
    struct has_engine_method<T, std::void_t<decltype(std::declval<T&>().engine())>> : std::true_type {};

    // Return a reference to the underlying engine, whether wrapped or direct.
    template <typename Rng>
    inline auto& get_engine(Rng& rng)
    {
      if constexpr (has_engine_method<Rng>::value)
	return rng.engine();     // e.g., randutils::mt19937_rng::engine()
      else
	return rng;              // e.g., std::mt19937_64
    }

    // Pull a raw 64-bit value from the engine (when you truly need an integer).
    template <typename Rng>
    inline std::uint64_t get_random_value(Rng& rng)
    {
      if constexpr (has_engine_method<Rng>::value) 
	return static_cast<std::uint64_t>(rng.engine()());
      else
	return static_cast<std::uint64_t>(rng());
    }

    /**
     * @brief Get a random index in [0, hiExclusive).
     *
     * Uses std::uniform_int_distribution on the actual engine to avoid modulo bias
     * and to behave correctly for both 32-bit and 64-bit engines.
     *
     * @pre hiExclusive > 0
     */
    template <typename Rng>
    inline std::size_t get_random_index(Rng& rng, std::size_t hiExclusive)
    {
      // Precondition guard (no-throw fallback): if 0, return 0.
      if (hiExclusive == 0)
	return 0;

      std::uniform_int_distribution<std::size_t> dist(0, hiExclusive - 1);
      return dist(get_engine(rng));
    }

    /**
     * @brief Get a random double in [0, 1).
     *
     * Uses std::uniform_real_distribution on the actual engine, ensuring correct
     * behavior regardless of engine word size and avoiding ad-hoc integer scaling.
     */
    template <typename Rng>
    inline double get_random_uniform_01(Rng& rng)
    {
      static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
      return dist(get_engine(rng));
    }

    /**
     * @brief Bernoulli(p) using the engine-backed uniform.
     *
     * @param p Probability of true in [0,1]. Values outside are clamped.
     */
    template <typename Rng>
    inline bool bernoulli(Rng& rng, double p)
    {
      if (p <= 0.0)
	return false;
      if (p >= 1.0)
	return true;
      return get_random_uniform_01(rng) < p;
    }

  } // namespace rng_utils
} // namespace mkc_timeseries
