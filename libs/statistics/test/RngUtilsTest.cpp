#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <random>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#include "RngUtils.h"
#include "randutils.hpp"

using mkc_timeseries::rng_utils::get_engine;
using mkc_timeseries::rng_utils::get_random_index;
using mkc_timeseries::rng_utils::get_random_uniform_01;
using mkc_timeseries::rng_utils::bernoulli;
using mkc_timeseries::rng_utils::CommonRandomNumberKey;
using mkc_timeseries::rng_utils::make_seed;
using mkc_timeseries::rng_utils::make_seed_seq;
using mkc_timeseries::rng_utils::CRNRng;

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

// === CRN (Common Random Numbers) tests ===
#include <algorithm>
#include <unordered_map>

using mkc_timeseries::rng_utils::CommonRandomNumberKey;
using mkc_timeseries::rng_utils::make_seed;
using mkc_timeseries::rng_utils::make_seed_seq;

TEST_CASE("CRN: CommonRandomNumberKey ctor/getters", "[rng][crn][key]") {
  const uint64_t M = 0xA1B2C3D4E5F60718ull;
  const uint64_t S = 0x0102030405060708ull;
  const uint64_t T = 3;   // stageTag
  const uint64_t L = 5;   // Lvalue
  const uint64_t R = 42;  // replicate
  const uint64_t F = 7;   // fold
  CommonRandomNumberKey k(M, S, T, L, R, F);

  REQUIRE(k.masterSeed() == M);
  REQUIRE(k.strategyId() == S);
  REQUIRE(k.stageTag()   == T);
  REQUIRE(k.Lvalue()     == L);
  REQUIRE(k.replicate()  == R);
  REQUIRE(k.fold()       == F);
}

TEST_CASE("CRN: make_seed is deterministic and field-sensitive", "[rng][crn][seed]") {
  const uint64_t M = 0x123456789ABCDEF0ull;
  CommonRandomNumberKey k1(M, /*strategy*/11, /*stage*/1, /*L*/0, /*rep*/0, /*fold*/0);
  CommonRandomNumberKey k2(M, /*strategy*/11, /*stage*/1, /*L*/0, /*rep*/0, /*fold*/0);

  // Deterministic for identical keys
  REQUIRE(make_seed(k1) == make_seed(k2));

  // Changing any field should almost surely change the seed
  CommonRandomNumberKey k_strategy(M, 12, 1, 0, 0, 0);
  CommonRandomNumberKey k_stage   (M, 11, 2, 0, 0, 0);
  CommonRandomNumberKey k_L       (M, 11, 1, 9, 0, 0);
  CommonRandomNumberKey k_rep     (M, 11, 1, 0, 1, 0);
  CommonRandomNumberKey k_fold    (M, 11, 1, 0, 0, 1);

  const auto s0 = make_seed(k1);
  REQUIRE(make_seed(k_strategy) != s0);
  REQUIRE(make_seed(k_stage)    != s0);
  REQUIRE(make_seed(k_L)        != s0);
  REQUIRE(make_seed(k_rep)      != s0);
  REQUIRE(make_seed(k_fold)     != s0);
}

TEST_CASE("CRN: per-replicate RNGs reproduce identical sequences", "[rng][crn][sequence]") {
  const uint64_t M = 0xDEADBEEFCAFEBABEull;
  const uint64_t strat = 0xAABBCCDD11223344ull;
  const uint64_t stage = 1;
  const uint64_t L     = 0;
  const uint64_t fold  = 0;

  // Build first few outputs for replicates 0..9
  std::vector<std::array<uint64_t, 3>> seq_a(10), seq_b(10);

  for (uint64_t r = 0; r < 10; ++r) {
    CommonRandomNumberKey key(M, strat, stage, L, r, fold);
    std::seed_seq sseq = make_seed_seq(make_seed(key));
    std::mt19937_64 eng(sseq);
    for (int i = 0; i < 3; ++i) seq_a[r][i] = eng();
  }

  // Recompute with fresh engines; sequences must match bit-for-bit
  for (uint64_t r = 0; r < 10; ++r) {
    CommonRandomNumberKey key(M, strat, stage, L, r, fold);
    std::seed_seq sseq = make_seed_seq(make_seed(key));
    std::mt19937_64 eng(sseq);
    for (int i = 0; i < 3; ++i) seq_b[r][i] = eng();
  }

  REQUIRE(seq_a == seq_b);
}

TEST_CASE("CRN: replicate streams are independent of iteration order (parallel safety)", "[rng][crn][order]") {
  const uint64_t M = 0xCAFED00D12345678ull;
  const uint64_t strat = 0x55AA55AA77889900ull;
  const uint64_t stage = 2;
  const uint64_t L     = 4;
  const uint64_t fold  = 0;

  // Reference sequences computed in ascending replicate order
  std::unordered_map<uint64_t, std::array<uint64_t, 2>> ref;
  for (uint64_t r = 0; r < 16; ++r) {
    CommonRandomNumberKey key(M, strat, stage, L, r, fold);
    std::seed_seq sseq = make_seed_seq(make_seed(key));
    std::mt19937_64 eng(sseq);
    ref[r] = { eng(), eng() };
  }

  // Now simulate "parallel" chunking by visiting replicates in a scrambled order
  std::vector<uint64_t> order(16);
  std::iota(order.begin(), order.end(), 0);
  std::reverse(order.begin(), order.end());           // reverse
  std::rotate(order.begin(), order.begin()+3, order.end()); // rotate

  for (auto r : order) {
    CommonRandomNumberKey key(M, strat, stage, L, r, fold);
    std::seed_seq sseq = make_seed_seq(make_seed(key));
    std::mt19937_64 eng(sseq);
    std::array<uint64_t, 2> got{ eng(), eng() };
    REQUIRE(got == ref[r]); // order independent: per-replicate stream is the same
  }
}

TEST_CASE("CRN: make_seed_seq yields reproducible mt19937_64 engines", "[rng][crn][seedseq]") {
  const uint64_t M = 0xFACEFACEFACEFACEull;
  CommonRandomNumberKey key(M, /*strategy*/123, /*stage*/9, /*L*/7, /*rep*/99, /*fold*/3);

  std::seed_seq sseq1 = make_seed_seq(make_seed(key));
  std::seed_seq sseq2 = make_seed_seq(make_seed(key));

  std::mt19937_64 e1(sseq1);
  std::mt19937_64 e2(sseq2);

  for (int i = 0; i < 20; ++i) {
    REQUIRE(e1() == e2()); // identical streams when seeded from the same key
  }
}

// === CRNRng tests ===
using mkc_timeseries::rng_utils::CRNRng;

TEST_CASE("CRNRng<std::mt19937_64>: deterministic per-replicate engines", "[rng][crn][crnrng][std]") {
  const uint64_t M = 0x1111222233334444ull;
  const uint64_t strat = 0xABCDEF1122334455ull;
  const uint64_t stage = 1;
  const uint64_t L     = 3;
  const uint64_t fold  = 0;

  CRNRng<> crn(M, strat, stage, L, fold); // default Eng = std::mt19937_64

  // Build reference outputs for replicates 0..9
  std::vector<std::array<uint64_t, 3>> ref(10);
  for (size_t r = 0; r < ref.size(); ++r) {
    auto eng = crn.make_engine(r);
    for (int i = 0; i < 3; ++i) ref[r][i] = eng();
  }

  // Recompute; must match bit-for-bit
  for (size_t r = 0; r < ref.size(); ++r) {
    auto eng = crn.make_engine(r);
    std::array<uint64_t, 3> got{eng(), eng(), eng()};
    REQUIRE(got == ref[r]);
  }
}

TEST_CASE("CRNRng<std::mt19937_64>: iteration order independence (parallel safety)", "[rng][crn][crnrng][order]") {
  const uint64_t M = 0xCAFEBABECAFED00Dull;
  const uint64_t strat = 0x5566778899AABBCCull;
  const uint64_t stage = 2;
  const uint64_t L     = 5;

  CRNRng<> crn(M, strat, stage, L);

  // Reference sequences computed in ascending replicate order
  std::unordered_map<size_t, std::array<uint64_t, 2>> ref;
  for (size_t r = 0; r < 16; ++r) {
    auto eng = crn.make_engine(r);
    ref[r] = { eng(), eng() };
  }

  // Scrambled order simulating chunked parallel loops
  std::vector<size_t> order(16);
  std::iota(order.begin(), order.end(), 0);
  std::reverse(order.begin(), order.end());
  std::rotate(order.begin(), order.begin() + 4, order.end());

  for (auto r : order) {
    auto eng = crn.make_engine(r);
    std::array<uint64_t, 2> got{ eng(), eng() };
    REQUIRE(got == ref[r]); // same per-replicate stream regardless of visit order
  }
}

TEST_CASE("CRNRng<std::mt19937_64>: with_L and with_fold change streams", "[rng][crn][crnrng][params]") {
  const uint64_t M = 0xDEADBEEFCAFEBABEull;
  const uint64_t strat = 0xA1A2A3A4A5A6A7A8ull;
  const uint64_t stage = 3;

  CRNRng<> base(M, strat, stage, /*L*/2, /*fold*/0);
  auto e_base = base.make_engine(7);

  CRNRng<> L_changed  = base.with_L(9);
  auto e_L = L_changed.make_engine(7);

  CRNRng<> fold_changed = base.with_fold(1);
  auto e_f = fold_changed.make_engine(7);

  // The first outputs should differ when L or fold differs (with overwhelming probability)
  REQUIRE(e_base() != e_L());
  REQUIRE(base.make_engine(7)() != e_f());
}

TEST_CASE("CRNRng<randutils::mt19937_rng>: deterministic per-replicate rng", "[rng][crn][crnrng][randutils]") {
  const uint64_t M = 0xFACEFACEFACEFACEull;
  const uint64_t strat = 0x0F1E2D3C4B5A6978ull;
  const uint64_t stage = 4;
  CRNRng<randutils::mt19937_rng> crn_ru(M, strat, stage, /*L*/0, /*fold*/0);

  std::vector<std::array<uint32_t, 3>> ref(8), got(8);

  // Build reference in natural order
  for (size_t r = 0; r < ref.size(); ++r) {
    auto rng = crn_ru.make_engine(r);
    auto& eng = rng.engine(); // access underlying std::mt19937
    for (int i = 0; i < 3; ++i) ref[r][i] = static_cast<uint32_t>(eng());
  }

  // Recompute in scrambled order; must match per-replicate
  std::vector<size_t> order(ref.size());
  std::iota(order.begin(), order.end(), 0);
  std::reverse(order.begin(), order.end());

  for (auto r : order) {
    auto rng = crn_ru.make_engine(r);
    auto& eng = rng.engine();
    for (int i = 0; i < 3; ++i) got[r][i] = static_cast<uint32_t>(eng());
  }

  REQUIRE(ref == got);
}

TEST_CASE("CRNRng: different replicates produce non-identical sequences", "[rng][crn][crnrng][replicates]") {
  const uint64_t M = 0x0123456789ABCDEFull;
  const uint64_t strat = 0x0011223344556677ull;
  const uint64_t stage = 5;

  CRNRng<> crn(M, strat, stage);

  // First draw from replicates 0..15; ensure at least one pair differs
  std::vector<uint64_t> firsts(16);
  for (size_t r = 0; r < firsts.size(); ++r) {
    auto eng = crn.make_engine(r);
    firsts[r] = eng();
  }

  // Sanity: not all identical
  bool any_diff = false;
  for (size_t i = 1; i < firsts.size(); ++i)
    if (firsts[i] != firsts[0]) { any_diff = true; break; }
  REQUIRE(any_diff);
}
