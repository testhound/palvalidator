// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef RANDOMMERSENNE_H_
#define RANDOMMERSENNE_H_

#include "pcg_random.hpp"
#include "pcg_extras.hpp"
#include "randutils.hpp"
#include "BoundedRandom.h"
#include <random>
#include <array>
#include <limits>
#include <stdexcept>

using uint32 = unsigned int;

/**
 * @brief A class that provides random number generation using the PCG
 *        (Permuted Congruential Generator) algorithm.
 *
 * This class offers methods to draw random unsigned 32-bit integers within
 * specified ranges. It uses a thread-local instance of the PCG32 generator
 * for thread safety.
 *
 * Bounded-range draws route through mkc::random_util::bounded_rand and
 * mkc::random_util::bounded_rand_inclusive, which implement the Lemire-style
 * multiply-and-reject method with O'Neill's threshold optimization (see
 * "Efficiently Generating a Number in a Range", 2018). For pcg32 this is
 * typically 1.5-2x faster than pcg_extras::bounded_rand on Monte Carlo
 * shuffle workloads and avoids the biased paths in std::uniform_int_distribution.
 */

class RandomMersenne 
{
public:
  RandomMersenne()
  : mRandGen(randutils::auto_seed_256{})
  {}

  // Add this public seed method:
  void seed() {
    auto newSeed = randutils::auto_seed_256{};
    mRandGen.seed(newSeed);
  }

  // deterministic seed constructor
  explicit RandomMersenne(uint64_t seed)
  {
    seed_u64(seed);
  }

  // Entropy + stream ID: preferred for parallel Monte Carlo workers
static RandomMersenne withStream(uint64_t stream_id)
{
    RandomMersenne r;  // default ctor: auto_seed_256, wasted but unavoidable
    randutils::auto_seed_256 entropy{};
    std::array<uint32_t, 2> state_data;
    entropy.generate(state_data.begin(), state_data.end());
    uint64_t state = (static_cast<uint64_t>(state_data[1]) << 32)
                   |  static_cast<uint64_t>(state_data[0]);
    r.mRandGen.engine().seed(state, stream_id | 1ULL);
    return r;
}

// Deterministic: for reproducible testing/debugging only
static RandomMersenne withSeed(uint64_t seed)
{
    RandomMersenne r;
    r.seed_u64(seed);
    return r;
}

  // eseed with fresh entropy (already have something similar; keep it if you like)
  void reseed()
  {
    mRandGen.seed(randutils::auto_seed_256{});
  }

  // NEW: deterministic seeding from a 64-bit value
  void seed_u64(uint64_t seed)
  {
    std::array<uint32_t, 2> seed_data =
      {
	static_cast<uint32_t>(seed),
	static_cast<uint32_t>(seed >> 32)
      };

    randutils::seed_seq_fe128 seq(seed_data.begin(), seed_data.end());
    mRandGen.seed(seq);
  }

  template <class It>
  void seed_seq(It first, It last)
  {
    randutils::seed_seq_fe256 seq(first, last);  // uses full-entropy mixer
    mRandGen.seed(seq);
  }

  /**
   * @brief Draws a random unsigned 32-bit integer within the inclusive range [min, max].
   *
   * @param min The minimum value (inclusive) of the range.
   * @param max The maximum value (inclusive) of the range.
   * @return A random unsigned 32-bit integer within the specified range.
   * @throws std::invalid_argument if min > max.
   */
  uint32 DrawNumber(uint32 min, uint32 max)
  {
    return mkc::random_util::bounded_rand_inclusive(mRandGen.engine(), min, max);
  }

  /**
   * @brief Draws a random unsigned 32-bit integer in the inclusive range [0, max].
   *
   * @param max The maximum value (inclusive) of the range.
   * @return A random unsigned 32-bit integer in [0, max].
   *
   * Implemented via bounded_rand_inclusive rather than bounded_rand(engine, max + 1)
   * so that the case max == std::numeric_limits<uint32_t>::max() is handled
   * correctly (a naive +1 would wrap to zero and yield undefined behavior in
   * the underlying bounded-range routine).
   */
  uint32 DrawNumber(uint32 max)
  {
    return mkc::random_util::bounded_rand_inclusive(mRandGen.engine(),
						    static_cast<uint32>(0), max);
  }

  /**
   * @brief Draws a random unsigned 32-bit integer within the exclusive upper
   *        bound range [0, exclusiveUpperBound - 1].
   *
   * This method is particularly useful for generating indices for zero-based
   * containers like vectors.
   *
   * @param exclusiveUpperBound The exclusive upper bound of the range.
   *        The generated number will be less than this value.
   *
   * @return A random unsigned 32-bit integer within the specified range.
   *
   * @throws std::out_of_range if exclusiveUpperBound exceeds the maximum value
   *         representable as uint32_t. This check was previously a debug-only
   *         assertion; in release builds (NDEBUG) the assert vanished and an
   *         out-of-range size_t silently truncated to uint32_t, which would
   *         introduce non-uniform bias when indexing into containers larger
   *         than 2^32 elements. Promoted to an unconditional throw so the
   *         failure mode is identical across build configurations.
   * @throws std::invalid_argument if exclusiveUpperBound == 0 (propagated
   *         from mkc::random_util::bounded_rand).
   */
  uint32 DrawNumberExclusive(size_t exclusiveUpperBound)
  {
    if (exclusiveUpperBound > std::numeric_limits<uint32_t>::max())
      {
	throw std::out_of_range(
				"RandomMersenne::DrawNumberExclusive: exclusiveUpperBound exceeds uint32 range");
      }

    return mkc::random_util::bounded_rand(mRandGen.engine(),
					  static_cast<uint32_t>(exclusiveUpperBound));
  }

  void seed_stream(uint64_t seed, uint64_t stream_id)
  {
    // PCG32 stream is controlled by the increment (must be odd)
    mRandGen.engine().seed(seed, stream_id | 1ULL);
  }

private:
  randutils::random_generator<pcg32> mRandGen;
  };

#endif
