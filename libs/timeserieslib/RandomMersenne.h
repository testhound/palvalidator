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
#include <ext/random>

using uint32 = unsigned int;

class RandomMersenne 
{
public:
  RandomMersenne()
    : mRandGen()
  {}
  uint32 DrawNumber(uint32 min, uint32 max)
  {
    return mRandGen.uniform(min, max);
    //return pcg_extras::bounded_rand (mRandGen.engine(), max);
  }

private:
  //randutils::random_generator<__gnu_cxx::sfmt19937> mRandGen; // Simd Mersenne Twister
  //randutils::mt19937_rng mRandGen; // Mersenne Twister RNG
  randutils::random_generator<pcg32> mRandGen; // Try out the new PCG generator
};

#endif
