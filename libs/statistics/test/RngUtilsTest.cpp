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
using mkc_timeseries::rng_utils::make_seed;
using mkc_timeseries::rng_utils::make_seed_seq;
using mkc_timeseries::rng_utils::CRNKey;
using mkc_timeseries::rng_utils::CRNRng;
using mkc_timeseries::rng_utils::CRNEngineProvider;
using mkc_timeseries::rng_utils::splitmix64;
using mkc_timeseries::rng_utils::hash_combine64;

static inline double mean(const std::vector<double>& v)
{
  return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

/* =========================
 * Core RNG utility tests
 * ========================= */

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

/* =========================
 * CRNKey / CRN providers
 * ========================= */

TEST_CASE("CRNKey: seed determinism and tag-order sensitivity", "[rng][crn][CRNKey]") {
  const uint64_t MASTER = 0xD00DCAFEF00DCAFEull;

  // Same tags & order -> same seeds for any replicate
  CRNKey k1(MASTER, {1, 2, 3});
  CRNKey k2(MASTER, {1, 2, 3});
  REQUIRE(k1.make_seed_for(0) == k2.make_seed_for(0));
  REQUIRE(k1.make_seed_for(42) == k2.make_seed_for(42));

  // Different order -> almost surely different
  CRNKey k3(MASTER, {1, 3, 2});
  REQUIRE(k1.make_seed_for(0)  != k3.make_seed_for(0));
  REQUIRE(k1.make_seed_for(42) != k3.make_seed_for(42));

  // Different master -> different
  CRNKey k4(MASTER ^ 0xFFFF, {1,2,3});
  REQUIRE(k1.make_seed_for(7) != k4.make_seed_for(7));
}

TEST_CASE("CRNKey: with_tag / with_tags composes identically to explicit construction", "[rng][crn][CRNKey]") {
  const uint64_t MASTER = 0xABCDEF0102030405ull;

  CRNKey base(MASTER);
  auto k_step = base.with_tag(11).with_tag(22).with_tag(33);
  CRNKey k_full(MASTER, {11, 22, 33});

  for (size_t r : {0u, 1u, 7u, 123u}) {
    REQUIRE(k_step.make_seed_for(r) == k_full.make_seed_for(r));
  }
}

/* ---------------------------
 * CRNEngineProvider tests
 * --------------------------- */

TEST_CASE("CRNEngineProvider<std::mt19937_64>: deterministic per-replicate engines", "[rng][crn][CRNEngineProvider][std]") {
  const uint64_t MASTER = 0x1111222233334444ull;
  CRNKey key(MASTER, {0xAA, 0xBB, 0xCC});
  CRNEngineProvider<std::mt19937_64> prov(key);

  // Reference outputs for replicates 0..9
  std::vector<std::array<uint64_t,3>> ref(10);
  for (size_t r = 0; r < ref.size(); ++r) {
    auto eng = prov.make_engine(r);
    for (int i = 0; i < 3; ++i) ref[r][i] = eng();
  }

  // Recompute and compare bit-for-bit
  for (size_t r = 0; r < ref.size(); ++r) {
    auto eng = prov.make_engine(r);
    std::array<uint64_t,3> got{eng(), eng(), eng()};
    REQUIRE(got == ref[r]);
  }
}

TEST_CASE("CRNEngineProvider<randutils::mt19937_rng>: deterministic per-replicate engines", "[rng][crn][CRNEngineProvider][randutils]") {
  const uint64_t MASTER = 0xFACEFACEFACEFACEull;
  CRNKey key(MASTER, {7, 8, 9});
  CRNEngineProvider<randutils::mt19937_rng> prov(key);

  // Reference (first 3 draws of underlying std engine) for replicates 0..7
  std::vector<std::array<uint32_t,3>> ref(8);
  for (size_t r = 0; r < ref.size(); ++r) {
    auto rng = prov.make_engine(r);
    auto& eng = rng.engine();
    for (int i = 0; i < 3; ++i) ref[r][i] = static_cast<uint32_t>(eng());
  }

  // Recompute
  for (size_t r = 0; r < ref.size(); ++r) {
    auto rng = prov.make_engine(r);
    auto& eng = rng.engine();
    std::array<uint32_t,3> got{ static_cast<uint32_t>(eng()),
                                static_cast<uint32_t>(eng()),
                                static_cast<uint32_t>(eng()) };
    REQUIRE(got == ref[r]);
  }
}

// Permuting wrapper that remaps replicate index: b -> perm[b]
template <class BaseProv>
struct PermutingProv {
  using Engine = typename BaseProv::Engine;
  PermutingProv(BaseProv base, std::vector<size_t> perm)
    : base_(std::move(base)), perm_(std::move(perm)) {}
  Engine make_engine(std::size_t b) const {
    return base_.make_engine(perm_[b]);
  }
  BaseProv base_;
  std::vector<size_t> perm_;
};

TEST_CASE("CRNEngineProvider: replicate-order independence (permuted vs identity)", "[rng][crn][CRNEngineProvider][order]") {
  using Eng = randutils::mt19937_rng;

  const uint64_t MASTER = 0xBADC0FFEE0DDF00Dull;
  CRNKey key(MASTER, {0xDE, 0xAD, 0xBE, 0xEF});
  CRNEngineProvider<Eng> base(key);

  const size_t B = 512;
  std::vector<size_t> id(B), scr(B);
  std::iota(id.begin(), id.end(), 0);
  scr = id;
  std::reverse(scr.begin(), scr.end());
  std::rotate(scr.begin(), scr.begin()+17, scr.end());

  PermutingProv<CRNEngineProvider<Eng>> prov_id(base, id);
  PermutingProv<CRNEngineProvider<Eng>> prov_sc(base, scr);

  // Build first two draws from each replicate in two orders; compare per-replicate equality
  std::vector<std::array<uint32_t,2>> a(B), b(B);
  for (size_t r = 0; r < B; ++r) {
    auto rng = prov_id.make_engine(r);
    auto& e  = rng.engine();
    a[r] = { static_cast<uint32_t>(e()), static_cast<uint32_t>(e()) };
  }
  for (size_t r = 0; r < B; ++r) {
    auto rng = prov_sc.make_engine(r);
    auto& e  = rng.engine();
    b[r] = { static_cast<uint32_t>(e()), static_cast<uint32_t>(e()) };
  }

  REQUIRE(a.size() == b.size());
  // Order independence: scrambled run maps replicate r -> scr[r], so compare a[scr[r]] with b[r].
  for (size_t r = 0; r < B; ++r) {
    REQUIRE(a[scr[r]] == b[r]);
  }
}

TEST_CASE("CRNEngineProvider: different replicates produce diverse outputs", "[rng][crn][CRNEngineProvider][replicates]") {
  using Eng = std::mt19937_64;
  CRNEngineProvider<Eng> prov(CRNKey(0x0123456789ABCDEFull, {42}));

  // First draw from replicates 0..31; ensure not all identical
  std::vector<uint64_t> firsts(32);
  for (size_t r = 0; r < firsts.size(); ++r) {
    auto eng = prov.make_engine(r);
    firsts[r] = eng();
  }
  bool any_diff = false;
  for (size_t i = 1; i < firsts.size(); ++i)
    if (firsts[i] != firsts[0]) { any_diff = true; break; }
  REQUIRE(any_diff);
}

TEST_CASE("CRNEngineProvider: extending tags changes streams (sensitivity)", "[rng][crn][CRNEngineProvider][tags]") {
  using Eng = std::mt19937_64;
  const uint64_t MASTER = 0xCAFEBABECAFED00Dull;

  CRNEngineProvider<Eng> p1(CRNKey(MASTER, {11,22}));
  CRNEngineProvider<Eng> p2(CRNKey(MASTER, {11,22}).with_tag(33)); // extra tag

  auto e1 = p1.make_engine(5);
  auto e2 = p2.make_engine(5);

  // With overwhelming probability, first outputs differ when tags differ
  REQUIRE(e1() != e2());
}

/* ---------------------------
 * CRNRng (wrapper) tests
 * --------------------------- */

TEST_CASE("CRNRng<std::mt19937_64>: deterministic per-replicate engines (domain-agnostic)", "[rng][crn][crnrng][std]") {
  const uint64_t M = 0x1111222233334444ull;

  // Build a domain-agnostic tag set (e.g., {idA, idB, idC})
  CRNRng<> crn( CRNKey(M).with_tags({0xAA, 0xBB, 0xCC}) ); // default Eng = std::mt19937_64

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

TEST_CASE("CRNRng<std::mt19937_64>: tag updates via with_tag/with_tags change streams", "[rng][crn][crnrng][params]") {
  const uint64_t M = 0xDEADBEEFCAFEBABEull;

  CRNRng<> base( CRNKey(M).with_tags({0x01, 0x02}) );
  auto e_base = base.make_engine(7);

  CRNRng<> plus_one  = base.with_tag(0x03);
  auto e_plus = plus_one.make_engine(7);

  CRNRng<> plus_many = base.with_tags({0x10, 0x20});
  auto e_many = plus_many.make_engine(7);

  // The first outputs should differ when tag sets differ (with overwhelming probability)
  REQUIRE(e_base() != e_plus());
  REQUIRE(base.make_engine(7)() != e_many());
}

TEST_CASE("CRNRng<randutils::mt19937_rng>: deterministic per-replicate rng (domain-agnostic)", "[rng][crn][crnrng][randutils]") {
  const uint64_t M = 0xFACEFACEFACEFACEull;

  CRNRng<randutils::mt19937_rng> crn_ru( CRNKey(M).with_tags({7, 8, 9}) );

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

  CRNRng<> crn( CRNKey(M).with_tags({0x44, 0x55}) );

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

TEST_CASE("splitmix64: determinism and basic avalanche", "[rng][hash][splitmix64]") {
  // Determinism: same input -> same output
  REQUIRE(splitmix64(0x123456789ABCDEF0ull) == splitmix64(0x123456789ABCDEF0ull));

  // Small set should be collision-free (not a proof, just a sanity check)
  std::unordered_map<uint64_t, bool> seen;
  for (uint64_t i = 0; i < 1000; ++i) {
    auto h = splitmix64(i);
    REQUIRE(seen.find(h) == seen.end());
    seen[h] = true;
  }

  // Basic avalanche: low 8 bits shouldn't be constant across a sequence
  uint64_t first = splitmix64(0);
  bool any_diff_low8 = false;
  for (uint64_t i = 1; i < 256; ++i) {
    uint64_t h = splitmix64(i);
    if ((h & 0xFFu) != (first & 0xFFu)) { any_diff_low8 = true; break; }
  }
  REQUIRE(any_diff_low8);
}

/* ---- hash_combine64 ---- */

TEST_CASE("hash_combine64: determinism, order sensitivity, and tag extension", "[rng][hash][combine]") {
  // Determinism
  REQUIRE(hash_combine64({1,2,3}) == hash_combine64({1,2,3}));

  // Order sensitivity
  REQUIRE(hash_combine64({1,2,3}) != hash_combine64({1,3,2}));

  // Appending tags changes hash
  uint64_t h_base = hash_combine64({0xDEAD'BEEFull, 0xAA, 0xBB});
  uint64_t h_ext  = hash_combine64({0xDEAD'BEEFull, 0xAA, 0xBB, 0xCC});
  REQUIRE(h_base != h_ext);

  // Combine with splitmix64 path: different inputs should typically differ
  REQUIRE(hash_combine64({splitmix64(1), splitmix64(2)}) !=
          hash_combine64({splitmix64(2), splitmix64(1)}));
}

/* ---- make_seed_seq ---- */

TEST_CASE("make_seed_seq: same seed → identical engine sequences", "[rng][seed_seq][determinism]") {
  using mkc_timeseries::rng_utils::make_seed_seq;

  const uint64_t seed64 = 0xBADC0FFEE0DDF00Dull;

  auto ss1 = make_seed_seq(seed64);
  auto ss2 = make_seed_seq(seed64);

  std::mt19937_64 e1(ss1), e2(ss2);

  // First several draws must match bit-for-bit
  for (int i = 0; i < 16; ++i) {
    REQUIRE(e1() == e2());
  }
}

TEST_CASE("make_seed_seq: different seeds → different engine sequences", "[rng][seed_seq][diversity]") {
  using mkc_timeseries::rng_utils::make_seed_seq;

  auto ss1 = make_seed_seq(0x1111222233334444ull);
  auto ss2 = make_seed_seq(0x1111222233334445ull);

  std::mt19937_64 e1(ss1), e2(ss2);

  // Very likely to differ on first draw; if not, check a few more
  bool differ = false;
  for (int i = 0; i < 8; ++i) {
    if (e1() != e2()) { differ = true; break; }
  }
  REQUIRE(differ);
}

TEST_CASE("make_seed_seq: stable across re-initialization cycles", "[rng][seed_seq][stability]") {
  using mkc_timeseries::rng_utils::make_seed_seq;

  const uint64_t seed64 = 0xCAFEBABECAFED00Dull;

  // Build two engines from the same seed, advance one, then rebuild and compare fresh
  auto ssA = make_seed_seq(seed64);
  auto ssB = make_seed_seq(seed64);

  std::mt19937_64 ea(ssA), eb(ssB);

  // Advance 'ea' some steps
  for (int i = 0; i < 1024; ++i) (void)ea();

  // Rebuild 'ec' from the same seed and compare to a freshly built 'ed'
  auto ssC = make_seed_seq(seed64);
  auto ssD = make_seed_seq(seed64);
  std::mt19937_64 ec(ssC), ed(ssD);

  for (int i = 0; i < 16; ++i) {
    REQUIRE(ec() == ed());
  }
}
