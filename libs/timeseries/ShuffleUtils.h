#ifndef SHUFFLE_UTILS_H
#define SHUFFLE_UTILS_H

// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016

#include <vector>
#include <algorithm>
#include "RandomMersenne.h"

namespace mkc_timeseries {

/**
 * @brief Performs an in-place Fisher–Yates shuffle on a vector.
 *
 * @tparam T The element type stored in the vector.
 * @param v The vector to shuffle.
 * @param rng A RandomMersenne instance providing DrawNumberExclusive(uint32) to generate indices.
 */
template <typename T>
void inplaceShuffle(std::vector<T>& v, RandomMersenne& rng)
{
    const std::size_t n = v.size();
    if (n <= 1) {
        return;
    }
    // Fisher–Yates shuffle: for i from n-1 down to 1, swap v[i] with v[j] where j in [0..i]
    for (std::size_t i = n - 1; i > 0; --i) {
        // DrawNumberExclusive(i+1) returns a uint32 in [0, i]
        const uint32_t bound = static_cast<uint32_t>(i + 1);
        const std::size_t j = static_cast<std::size_t>(rng.DrawNumberExclusive(bound));
        std::swap(v[i], v[j]);
    }
}

} // namespace mkc_timeseries

#endif // SHUFFLE_UTILS_H
