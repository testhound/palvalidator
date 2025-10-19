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
#include <random>
#include <array>

using uint32 = unsigned int;

/**
 * @brief A class that provides random number generation using the PCG (Permuted Congruential Generator) algorithm.
 *
 * This class offers methods to draw random unsigned 32-bit integers within specified ranges.
 * It uses a thread-local instance of the PCG32 generator for thread safety.
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
   */
  uint32 DrawNumber(uint32 min, uint32 max)
  {
    return mRandGen.uniform(min, max);
  }

  uint32 DrawNumber(uint32 max)
  {
    return pcg_extras::bounded_rand (mRandGen.engine(), max + 1);
  }

  /**
     * @brief Draws a random unsigned 32-bit integer within the exclusive upper bound range [0, exclusiveUpperBound - 1].
     *
     * This method is particularly useful for generating indices for zero-based containers like vectors.
     *
     * @param exclusiveUpperBound The exclusive upper bound of the range.
     * The generated number will be less than this value.
     *
     * @return A random unsigned 32-bit integer within the specified range.
     */
    uint32 DrawNumberExclusive(uint32 exclusiveUpperBound)
    {
        return pcg_extras::bounded_rand (mRandGen.engine(), exclusiveUpperBound);
    }

private:
  randutils::random_generator<pcg32> mRandGen;
  };

#endif
