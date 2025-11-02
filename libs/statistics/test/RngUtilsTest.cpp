#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <random>
#include <vector>
#include <numeric>
#include <cmath>

#include "RngUtils.h"
#include "randutils.hpp"

using mkc_timeseries::rng_utils::get_engine;
using mkc_timeseries::rng_utils::get_random_index;
using mkc_timeseries::rng_utils::get_random_uniform_01;
using mkc_timeseries::rng_utils::bernoulli;

static inline double mean(const std::vector<double>& v)
{
  return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

TEST_CASE("RngUtils: get_engine returns alias and preserves sequence", "[rng][engine]") {
  // std engine
  std::mt19937_64 stdrng(12345u);
  auto& e1 = get_engine(stdrng);

  // Alias check: same object
  REQUIRE(&e1 == &stdrng);

  // Sequence check without double-advancing the same object:
  auto e1_copy  = e1;       // copy state of the alias
  auto std_copy = stdrng;   // copy state of the original
  for (int i = 0; i < 10; ++i) {
    REQUIRE(e1_copy() == std_copy());
  }

  // randutils engine
  randutils::seed_seq_fe128 seed{1u,2u,3u,4u};
  randutils::mt19937_rng rrng(seed);
  auto& e2 = get_engine(rrng);

  // Alias check: same underlying engine object
  REQUIRE(&e2 == &rrng.engine());

  // Sequence check via copies
  auto e2_copy   = e2;            // copy underlying engine state
  auto rrng_copy = rrng.engine(); // copy underlying engine state
  for (int i = 0; i < 10; ++i) {
    REQUIRE(e2_copy() == rrng_copy());
  }
}

TEST_CASE("RngUtils: get_random_index range and basic coverage", "[rng][index]") {
  randutils::seed_seq_fe128 s{7u,7u,7u,7u};
  randutils::mt19937_rng rng(s);

  const std::size_t K = 17; // prime-ish size to avoid accidental patterns
  std::vector<uint64_t> counts(K, 0);
  const size_t N = 50000;

  for (size_t i = 0; i < N; ++i) {
    auto idx = get_random_index(rng, K);
    REQUIRE(idx < K); // in-range
    counts[idx]++;
  }

  // Chi-by-eye: each bucket should be nonzero and roughly close to N/K
  const double target = static_cast<double>(N) / static_cast<double>(K);
  for (auto c : counts) {
    REQUIRE(c > 0);
    // allow generous ±25% band (very loose to avoid flakiness)
    REQUIRE( std::fabs(static_cast<double>(c) - target) <= 0.25 * target );
  }
}

TEST_CASE("RngUtils: get_random_uniform_01 in [0,1) with sensible mean", "[rng][uniform01]") {
  randutils::mt19937_rng rng; // auto-seeded
  const size_t N = 100000;

  std::vector<double> vals; vals.reserve(N);
  for (size_t i = 0; i < N; ++i) {
    double u = get_random_uniform_01(rng);
    REQUIRE(u >= 0.0);
    REQUIRE(u < 1.0);
    vals.push_back(u);
  }
  const double mu = mean(vals);

  // For U(0,1), E[U] = 0.5, Var[U] = 1/12. Tolerance ~ ±0.01 is ample for N=1e5.
  REQUIRE( mu == Catch::Approx(0.5).margin(0.01) );
}

TEST_CASE("RngUtils: bernoulli respects p and edge cases", "[rng][bernoulli]") {
  randutils::seed_seq_fe128 s{2025u, 11u, 1u, 42u};
  randutils::mt19937_rng rng(s);

  // Edge cases
  REQUIRE( bernoulli(rng, -0.5) == false );
  REQUIRE( bernoulli(rng, 0.0)  == false );
  REQUIRE( bernoulli(rng, 1.0)  == true  );
  REQUIRE( bernoulli(rng, 2.0)  == true  );

  // Frequency check for p = 0.7
  const double p = 0.7;
  const size_t N = 100000;
  size_t ones = 0;
  for (size_t i = 0; i < N; ++i)
    ones += bernoulli(rng, p) ? 1 : 0;

  const double phat = static_cast<double>(ones) / static_cast<double>(N);
  // 5-sigma band: sigma = sqrt(p(1-p)/N) ≈ 0.00145 for N=1e5
  const double sigma = std::sqrt(p * (1.0 - p) / static_cast<double>(N));
  REQUIRE( std::fabs(phat - p) <= 5.0 * sigma );
}

TEST_CASE("RngUtils: get_random_index(0) is safe no-op", "[rng][index][edge]") {
  randutils::mt19937_rng rng; // any engine
  // Should not crash and just return 0 (defensive behavior)
  REQUIRE( get_random_index(rng, 0) == 0 );
}
