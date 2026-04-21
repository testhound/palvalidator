#ifndef BOUNDEDRANDOM_H_
#define BOUNDEDRANDOM_H_

#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>

namespace mkc::random_util
{
  namespace detail
  {

    /**
     * @brief Compute a % b, avoiding integer division when a small number of
     *        subtractions suffices.
     *
     * The %-operator compiles to an integer division on x86, which is typically
     * an order of magnitude slower than other integer arithmetic. When @p a is
     * known to be less than a small multiple of @p b, replacing the division
     * with one or two comparisons and subtractions is a measurable win.
     *
     * See M.E. O'Neill, "Efficiently Generating a Number in a Range" (2018),
     * section "Optimizing Modulo".
     */
    // Helper trait: is_unsigned plus recognition of GCC's unsigned __int128
    // extension, which std::is_unsigned_v does not recognize.
    template <typename T>
    inline constexpr bool is_unsigned_or_uint128_v =
      std::is_unsigned_v<T>
#if defined(__SIZEOF_INT128__)
      || std::is_same_v<T, unsigned __int128>
#endif
      ;

    template <typename UIntType>
    [[nodiscard]] inline UIntType fast_mod_small(UIntType a, UIntType b) noexcept
    {
      static_assert(is_unsigned_or_uint128_v<UIntType>,
		    "fast_mod_small requires an unsigned type");
      if (a >= b)
	{
	  a -= b;
	  if (a >= b)
	    {
	      a %= b;
	    }
	}
      return a;
    }

    /**
     * @brief Compute 2^N mod @p upperBoundExclusive, where N is the bit width
     *        of @c UIntType.
     *
     * Relies on the unsigned identity `-x == 2^N - x (mod 2^N)` for 0 < x < 2^N,
     * so `(-upperBoundExclusive) % upperBoundExclusive` yields the desired
     * `2^N % upperBoundExclusive` without ever representing 2^N explicitly.
     * Uses @ref fast_mod_small to skip the division when possible.
     */
    template <typename UIntType>
    [[nodiscard]] inline UIntType compute_pow2n_threshold(UIntType upperBoundExclusive) noexcept
    {
      return fast_mod_small(static_cast<UIntType>(-upperBoundExclusive), upperBoundExclusive);
    }

    template <typename UniformRandomBitGenerator>
    using generator_result_t = typename UniformRandomBitGenerator::result_type;

    template <typename UniformRandomBitGenerator>
    inline constexpr bool is_full_width_zero_based_generator_v =
      std::is_unsigned_v<generator_result_t<UniformRandomBitGenerator>>
      && (UniformRandomBitGenerator::min() == 0)
      && (UniformRandomBitGenerator::max() ==
	  std::numeric_limits<generator_result_t<UniformRandomBitGenerator>>::max());

    /**
     * @brief Unbiased rejection-modulo bounded_rand for generators that do not
     *        satisfy the fast path's preconditions (non-zero min, non-power-of-2
     *        domain, or narrow result type).
     *
     * @pre @p upperBoundExclusive > 0. The public entry point validates this;
     *      this detail function does not.
     *
     * @throws std::out_of_range if @p upperBoundExclusive exceeds the
     *         generator's domain size.
     */
    template <typename UniformRandomBitGenerator>
    [[nodiscard]] inline generator_result_t<UniformRandomBitGenerator>
    bounded_rand_generic_fallback(UniformRandomBitGenerator& generator,
				  generator_result_t<UniformRandomBitGenerator> upperBoundExclusive)
    {
      using ResultType = generator_result_t<UniformRandomBitGenerator>;

      constexpr ResultType generatorMin = UniformRandomBitGenerator::min();
      constexpr ResultType generatorMax = UniformRandomBitGenerator::max();

      static_assert(generatorMax >= generatorMin,
		    "UniformRandomBitGenerator::max() must be >= min()");

      using WideType = std::conditional_t<(sizeof(ResultType) < sizeof(std::uint64_t)),
					  std::uint64_t,
#if defined(__SIZEOF_INT128__)
					  unsigned __int128
#else
					  std::uint64_t
#endif
					  >;

      const WideType domainSize = static_cast<WideType>(generatorMax)
	- static_cast<WideType>(generatorMin)
	+ static_cast<WideType>(1);

      const WideType bound = static_cast<WideType>(upperBoundExclusive);

      if (bound > domainSize)
	{
	  throw std::out_of_range(
	    "bounded_rand upperBoundExclusive exceeds generator domain size");
	}

      // threshold = domainSize % bound, computed as (domainSize - bound) % bound
      // to avoid representing domainSize explicitly when it would overflow.
      const WideType threshold = fast_mod_small(
	static_cast<WideType>(domainSize - bound), bound);

      for (;;)
	{
	  const ResultType rawValue = generator();
	  const WideType shiftedValue = static_cast<WideType>(rawValue)
	    - static_cast<WideType>(generatorMin);

	  if (shiftedValue >= threshold)
	    {
	      return static_cast<ResultType>(shiftedValue % bound);
	    }
	}
    }

    /**
     * @brief Lemire-style multiply-and-reject bounded_rand for full-width,
     *        zero-based unsigned generators (e.g. pcg32, pcg64, xoshiro*).
     *
     * Implements the "Debiased Integer Multiplication" method with O'Neill's
     * threshold-guard optimization: the expensive threshold is only computed on
     * the rare path where the low bits of the product could straddle a
     * rejection boundary.
     *
     * @pre @p upperBoundExclusive > 0. Checked in the public entry point.
     */
    template <typename UniformRandomBitGenerator>
    [[nodiscard]] inline generator_result_t<UniformRandomBitGenerator>
    bounded_rand_full_width_fast(UniformRandomBitGenerator& generator,
				 generator_result_t<UniformRandomBitGenerator> upperBoundExclusive)
    {
      using ResultType = generator_result_t<UniformRandomBitGenerator>;

      static_assert(is_full_width_zero_based_generator_v<UniformRandomBitGenerator>,
		    "Fast bounded_rand path requires a zero-based full-width unsigned generator");

      if constexpr (std::is_same_v<ResultType, std::uint32_t>)
	{
	  ResultType randomValue = generator();
	  std::uint64_t product = static_cast<std::uint64_t>(randomValue)
	    * static_cast<std::uint64_t>(upperBoundExclusive);
	  ResultType lowBits = static_cast<ResultType>(product);

	  if (lowBits < upperBoundExclusive)
	    {
	      const ResultType threshold = compute_pow2n_threshold(upperBoundExclusive);
	      while (lowBits < threshold)
		{
		  randomValue = generator();
		  product = static_cast<std::uint64_t>(randomValue)
		    * static_cast<std::uint64_t>(upperBoundExclusive);
		  lowBits = static_cast<ResultType>(product);
		}
	    }

	  return static_cast<ResultType>(product >> 32);
	}
      else if constexpr (std::is_same_v<ResultType, std::uint64_t>)
	{
#if defined(__SIZEOF_INT128__)
	  ResultType randomValue = generator();
	  unsigned __int128 product = static_cast<unsigned __int128>(randomValue)
	    * static_cast<unsigned __int128>(upperBoundExclusive);
	  ResultType lowBits = static_cast<ResultType>(product);

	  if (lowBits < upperBoundExclusive)
	    {
	      const ResultType threshold = compute_pow2n_threshold(upperBoundExclusive);
	      while (lowBits < threshold)
		{
		  randomValue = generator();
		  product = static_cast<unsigned __int128>(randomValue)
		    * static_cast<unsigned __int128>(upperBoundExclusive);
		  lowBits = static_cast<ResultType>(product);
		}
	    }

	  return static_cast<ResultType>(product >> 64);
#else
	  return bounded_rand_generic_fallback(generator, upperBoundExclusive);
#endif
	}
      else
	{
	  return bounded_rand_generic_fallback(generator, upperBoundExclusive);
	}
    }

  } // namespace detail

/**
 * @brief Draw a uniformly distributed integer in the half-open interval
 *        [0, @p upperBoundExclusive).
 *
 * Prefers a Lemire-style multiply-and-reject fast path for common full-width
 * unsigned generators (pcg32, pcg64, xoshiro*) and falls back to an unbiased
 * rejection-modulo implementation for unusual engines (non-zero min, narrow or
 * non-power-of-2 domain).
 *
 * @throws std::invalid_argument if @p upperBoundExclusive == 0.
 * @throws std::out_of_range if the generic fallback is selected and the bound
 *         exceeds the generator's domain size.
 */
template <typename UniformRandomBitGenerator>
[[nodiscard]] inline auto bounded_rand(UniformRandomBitGenerator& generator,
                                       typename UniformRandomBitGenerator::result_type upperBoundExclusive)
    -> typename UniformRandomBitGenerator::result_type
{
    using ResultType = typename UniformRandomBitGenerator::result_type;

    static_assert(std::is_unsigned_v<ResultType>,
                  "bounded_rand requires an unsigned generator result type");

    if (upperBoundExclusive == 0)
    {
        throw std::invalid_argument("bounded_rand upperBoundExclusive must be greater than zero");
    }

    if constexpr (detail::is_full_width_zero_based_generator_v<UniformRandomBitGenerator>)
    {
        return detail::bounded_rand_full_width_fast(generator, upperBoundExclusive);
    }
    else
    {
        return detail::bounded_rand_generic_fallback(generator, upperBoundExclusive);
    }
}

/**
 * @brief Draw a uniformly distributed integer in the closed interval
 *        [@p lowerBoundInclusive, @p upperBoundInclusive].
 *
 * @throws std::invalid_argument if @p lowerBoundInclusive > @p upperBoundInclusive.
 */
template <typename UniformRandomBitGenerator>
[[nodiscard]] inline auto bounded_rand_inclusive(
    UniformRandomBitGenerator& generator,
    typename UniformRandomBitGenerator::result_type lowerBoundInclusive,
    typename UniformRandomBitGenerator::result_type upperBoundInclusive)
    -> typename UniformRandomBitGenerator::result_type
{
    using ResultType = typename UniformRandomBitGenerator::result_type;

    static_assert(std::is_unsigned_v<ResultType>,
                  "bounded_rand_inclusive requires an unsigned generator result type");

    if (lowerBoundInclusive > upperBoundInclusive)
    {
        throw std::invalid_argument(
            "bounded_rand_inclusive lowerBoundInclusive must be <= upperBoundInclusive");
    }

    // If the requested interval spans the entire ResultType domain, a single
    // generator draw is already uniformly distributed. We must short-circuit
    // here because computing width = upper - lower + 1 would wrap to zero and
    // bounded_rand would reject that with invalid_argument.
    if (lowerBoundInclusive == 0
        && upperBoundInclusive == std::numeric_limits<ResultType>::max())
    {
        return generator();
    }

    // Safe: the full-range case is handled above, so (upper - lower + 1) fits
    // in ResultType without overflow.
    const ResultType width = static_cast<ResultType>(upperBoundInclusive - lowerBoundInclusive + 1);
    return static_cast<ResultType>(lowerBoundInclusive + bounded_rand(generator, width));
}

} // namespace mkc::random_util

#endif // BOUNDEDRANDOM_H_
