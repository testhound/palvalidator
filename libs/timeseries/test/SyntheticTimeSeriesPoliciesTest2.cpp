// SyntheticTimeSeriesBlockShufflePolicyTest.cpp
//
// Unit tests for:
//   - BlockShufflePolicy (construction, apply(), block atomicity)
//   - shuffle_detail::computeBlockSize
//   - SyntheticTimeSeries<..., SyntheticNullModel::N2_BlockDays>
//
// Compile alongside the existing SyntheticTimeSeriesPoliciesTest.cpp.

#include <catch2/catch_test_macros.hpp>
#include "number.h"
#include "TimeSeries.h"
#include "SyntheticTimeSeries.h"
#include "DecimalConstants.h"
#include "TestUtils.h"    // createEquityEntry, DecimalType

#include <vector>
#include <algorithm>
#include <numeric>
#include <tuple>

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;

// ============================================================
// Anonymous namespace: shared helpers
// ============================================================
namespace
{
  using OhlcTuple = std::tuple<DecimalType, DecimalType, DecimalType, DecimalType>;

  // ---- EodFactors builders ----

  // 7-bar EodFactors: anchor (index 0) + 6 shuffleable days (indices 1–6).
  //
  // Close values are chosen so every day is uniquely identifiable in any
  // permuted output, and so that natural block grouping when L=2 is:
  //   Block 0 = (days 1, 2)   closes: 1.021, 1.022
  //   Block 1 = (days 3, 4)   closes: 1.033, 1.034
  //   Block 2 = (days 5, 6)   closes: 1.045, 1.046
  //
  // Open values are also globally distinct so PairedDayShufflePolicy source-
  // tracking (used in the comparison test) works correctly.
  EodFactors<DecimalType> buildTestFactors7()
  {
    using DT = DecimalType;
    return EodFactors<DT>(
      /*open */  {DT("1.000"), DT("1.011"), DT("0.989"), DT("1.022"), DT("0.978"),
                  DT("1.033"), DT("0.967")},
      /*high */  {DT("1.050"), DT("1.061"), DT("1.039"), DT("1.072"), DT("1.028"),
                  DT("1.083"), DT("1.017")},
      /*low  */  {DT("0.950"), DT("0.961"), DT("0.939"), DT("0.972"), DT("0.928"),
                  DT("0.983"), DT("0.917")},
      /*close*/  {DT("1.010"), DT("1.021"), DT("1.022"), DT("1.033"), DT("1.034"),
                  DT("1.045"), DT("1.046")}
    );
  }

  // 2-bar EodFactors: anchor + 1 day. Edge-case n<=2 for apply().
  EodFactors<DecimalType> buildTestFactors2()
  {
    using DT = DecimalType;
    return EodFactors<DT>(
      {DT("1.00"), DT("1.01")},
      {DT("1.05"), DT("1.06")},
      {DT("0.95"), DT("0.96")},
      {DT("1.01"), DT("1.02")}
    );
  }

  // ---- Analysis helpers ----

  // Sorted multiset of (O, H, L, C) 4-tuples from positions [from, n).
  std::vector<OhlcTuple> sortedOhlcTuples(const EodFactors<DecimalType>& f, size_t from)
  {
    std::vector<OhlcTuple> t;
    for (size_t i = from; i < f.size(); ++i)
      t.emplace_back(f.getOpen()[i], f.getHigh()[i], f.getLow()[i], f.getClose()[i]);
    std::sort(t.begin(), t.end());
    return t;
  }

  // Extracts the close values for the source block at index blockIdx (0-based)
  // given block size L, from positions [1 + blockIdx*L, min(1 + (blockIdx+1)*L, n)).
  // Used to identify which source block appears at a given output chunk.
  std::vector<DecimalType> srcBlockCloses(const EodFactors<DecimalType>& f,
                                          size_t blockIdx, size_t L)
  {
    const size_t n     = f.size();
    const size_t start = 1 + blockIdx * L;
    const size_t end   = std::min(start + L, n);
    std::vector<DecimalType> result;
    result.reserve(end - start);
    for (size_t i = start; i < end; ++i)
      result.push_back(f.getClose()[i]);
    return result;
  }

  // Extracts close values from an output EodFactors at contiguous positions
  // [start, start + count), clamped at n.
  std::vector<DecimalType> outputCloseSlice(const EodFactors<DecimalType>& f,
                                            size_t start, size_t count)
  {
    const size_t end = std::min(start + count, f.size());
    std::vector<DecimalType> result;
    result.reserve(end - start);
    for (size_t i = start; i < end; ++i)
      result.push_back(f.getClose()[i]);
    return result;
  }

  // 30-bar EOD sample series reused across integration tests.
  OHLCTimeSeries<DecimalType> createSampleEodSeries()
  {
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*createEquityEntry("20070402","43.08","43.17","42.71","43.00",89658785));
    series.addEntry(*createEquityEntry("20070403","43.23","43.72","43.20","43.57",105925137));
    series.addEntry(*createEquityEntry("20070404","43.61","43.79","43.54","43.75",85200468));
    series.addEntry(*createEquityEntry("20070405","43.70","43.99","43.64","43.97",54260779));
    series.addEntry(*createEquityEntry("20070409","44.12","44.16","43.79","43.86",63074749));
    series.addEntry(*createEquityEntry("20070410","43.85","44.09","43.82","44.09",76458129));
    series.addEntry(*createEquityEntry("20070411","44.05","44.07","43.48","43.62",118359304));
    series.addEntry(*createEquityEntry("20070412","43.55","44.03","43.36","43.97",114852449));
    series.addEntry(*createEquityEntry("20070413","43.98","44.13","43.70","44.06",94594604));
    series.addEntry(*createEquityEntry("20070416","44.23","44.56","44.23","44.47",73028087));
    series.addEntry(*createEquityEntry("20070417","44.55","44.61","44.37","44.57",81879736));
    series.addEntry(*createEquityEntry("20070418","44.34","44.64","44.24","44.42",82051504));
    series.addEntry(*createEquityEntry("20070419","44.22","44.66","44.13","44.56",95510366));
    series.addEntry(*createEquityEntry("20070420","44.94","45.08","44.61","44.81",122441399));
    series.addEntry(*createEquityEntry("20070423","44.85","45.01","44.75","44.88",85450450));
    series.addEntry(*createEquityEntry("20070424","45.04","45.24","44.74","45.11",108196954));
    series.addEntry(*createEquityEntry("20070425","45.26","45.73","45.11","45.72",106954392));
    series.addEntry(*createEquityEntry("20070426","45.83","46.06","45.74","45.96",99409986));
    series.addEntry(*createEquityEntry("20070427","45.78","46.11","45.70","45.98",96607259));
    series.addEntry(*createEquityEntry("20070430","45.91","45.94","45.33","45.37",93556683));
    series.addEntry(*createEquityEntry("20070501","45.38","45.55","45.07","45.48",135108913));
    series.addEntry(*createEquityEntry("20070502","45.50","45.99","45.46","45.83",91995829));
    series.addEntry(*createEquityEntry("20070503","45.94","46.13","45.84","46.00",98037633));
    series.addEntry(*createEquityEntry("20070504","46.17","46.30","45.83","46.04",93643063));
    series.addEntry(*createEquityEntry("20070507","46.06","46.19","45.98","46.04",47684367));
    series.addEntry(*createEquityEntry("20070508","45.88","46.18","45.71","46.14",95197296));
    series.addEntry(*createEquityEntry("20070509","45.90","46.38","45.87","46.24",116007860));
    series.addEntry(*createEquityEntry("20070510","46.07","46.19","45.48","45.60",171264643));
    series.addEntry(*createEquityEntry("20070511","45.65","46.19","45.59","46.19",103197326));
    series.addEntry(*createEquityEntry("20070514","46.18","46.29","45.60","45.87",118966989));
    return series;
  }

} // anonymous namespace

// Convenience alias for N2 to reduce boilerplate in each test case.
template <class Decimal>
using SyntheticN2 = SyntheticTimeSeries<
  Decimal,
  mkc_timeseries::LogNLookupPolicy<Decimal>,
  NoRounding,
  SyntheticNullModel::N2_BlockDays>;

// ============================================================
// Section 7: BlockShufflePolicy — Construction
// ============================================================

TEST_CASE("BlockShufflePolicy: default constructor gives blockSize 1", "[BlockShufflePolicy]")
{
  BlockShufflePolicy p;
  REQUIRE(p.getBlockSize() == 1u);
}

TEST_CASE("BlockShufflePolicy: explicit constructor stores blockSize correctly", "[BlockShufflePolicy]")
{
  BlockShufflePolicy p3(3u);
  BlockShufflePolicy p10(10u);
  REQUIRE(p3.getBlockSize()  == 3u);
  REQUIRE(p10.getBlockSize() == 10u);
}

TEST_CASE("BlockShufflePolicy: blockSize=0 is silently promoted to 1", "[BlockShufflePolicy]")
{
  // A zero-size block is nonsensical; the constructor clamps it to 1, which
  // degenerates safely to N0 per-day behaviour.
  BlockShufflePolicy p(0u);
  REQUIRE(p.getBlockSize() == 1u);
}

TEST_CASE("BlockShufflePolicy: copy constructor preserves blockSize", "[BlockShufflePolicy]")
{
  BlockShufflePolicy orig(5u);
  BlockShufflePolicy copy(orig);
  REQUIRE(copy.getBlockSize() == 5u);
}

TEST_CASE("BlockShufflePolicy: move constructor preserves blockSize", "[BlockShufflePolicy]")
{
  BlockShufflePolicy src(7u);
  BlockShufflePolicy moved(std::move(src));
  REQUIRE(moved.getBlockSize() == 7u);
}

TEST_CASE("BlockShufflePolicy: copy assignment preserves blockSize", "[BlockShufflePolicy]")
{
  BlockShufflePolicy a(4u);
  BlockShufflePolicy b(99u);
  b = a;
  REQUIRE(b.getBlockSize() == 4u);
}

// ============================================================
// Section 8: BlockShufflePolicy::apply() — Edge Cases and Size
// ============================================================

TEST_CASE("BlockShufflePolicy::apply: n<=2 returns unchanged copy", "[BlockShufflePolicy]")
{
  // The policy mirrors PairedDayShufflePolicy's guard: when n<=2 there is at
  // most one shuffleable element and nothing can be permuted, so the input is
  // returned unchanged (as a copy, not an alias).
  auto small = buildTestFactors2();
  RandomMersenne rng;
  BlockShufflePolicy p(2u);
  auto result = p.apply<DecimalType>(small, rng);

  REQUIRE(result.getOpen()  == small.getOpen());
  REQUIRE(result.getHigh()  == small.getHigh());
  REQUIRE(result.getLow()   == small.getLow());
  REQUIRE(result.getClose() == small.getClose());
}

TEST_CASE("BlockShufflePolicy::apply: output size always matches input size", "[BlockShufflePolicy]")
{
  // Verify for several block sizes that the output bar count is unchanged.
  auto orig = buildTestFactors7();  // n = 7
  RandomMersenne rng;

  for (size_t L : {1u, 2u, 3u, 6u, 10u})
  {
    BlockShufflePolicy p(L);
    for (int trial = 0; trial < 10; ++trial)
    {
      auto result = p.apply<DecimalType>(orig, rng);
      REQUIRE(result.size() == orig.size());
    }
  }
}

// ============================================================
// Section 9: BlockShufflePolicy::apply() — Anchor Invariants
// ============================================================

TEST_CASE("BlockShufflePolicy::apply: open[0] is always DecimalOne", "[BlockShufflePolicy]")
{
  // The anchor bar (index 0) is explicitly fixed by the policy.  Its open factor
  // must equal 1.0 (the normalisation baseline) across every shuffle.
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(2u);
  for (int trial = 0; trial < 50; ++trial)
  {
    auto result = p.apply<DecimalType>(orig, rng);
    REQUIRE(result.getOpen()[0] == DecimalConstants<DecimalType>::DecimalOne);
  }
}

TEST_CASE("BlockShufflePolicy::apply: anchor H, L, C at index 0 are unchanged", "[BlockShufflePolicy]")
{
  // blockOrder shuffles only indices [1..numBlocks-1] in src-block space.
  // Position 0 in the output is always written from srcOpen[0]/srcHigh[0] etc.,
  // so the anchor bar's intraday shape is never modified.
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(2u);
  for (int trial = 0; trial < 50; ++trial)
  {
    auto result = p.apply<DecimalType>(orig, rng);
    REQUIRE(result.getHigh()[0]  == orig.getHigh()[0]);
    REQUIRE(result.getLow()[0]   == orig.getLow()[0]);
    REQUIRE(result.getClose()[0] == orig.getClose()[0]);
  }
}

// ============================================================
// Section 10: BlockShufflePolicy::apply() — Permutation Property
// ============================================================

TEST_CASE("BlockShufflePolicy::apply: OHLC 4-tuple multiset at [1..n-1] is preserved", "[BlockShufflePolicy]")
{
  // The policy shuffles whole blocks; every day-unit from the original appears
  // in the output exactly once.  The sorted multiset of (O, H, L, C) 4-tuples
  // for positions [1..n-1] must therefore be invariant under any permutation.
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(2u);

  const auto expected = sortedOhlcTuples(orig, 1);
  for (int trial = 0; trial < 50; ++trial)
  {
    auto result = p.apply<DecimalType>(orig, rng);
    REQUIRE(sortedOhlcTuples(result, 1) == expected);
  }
}

TEST_CASE("BlockShufflePolicy::apply: permutation property holds for varying block sizes", "[BlockShufflePolicy]")
{
  // The multiset invariant must hold regardless of L, including L=1 (per-day)
  // and L > (n-1) (single block = identity).
  auto orig = buildTestFactors7();  // n=7, shuffleable range = 6 days
  RandomMersenne rng;

  const auto expected = sortedOhlcTuples(orig, 1);
  for (size_t L : {1u, 2u, 3u, 4u, 6u, 10u})
  {
    BlockShufflePolicy p(L);
    for (int trial = 0; trial < 20; ++trial)
    {
      auto result = p.apply<DecimalType>(orig, rng);
      REQUIRE(sortedOhlcTuples(result, 1) == expected);
    }
  }
}

TEST_CASE("BlockShufflePolicy::apply: non-divisible (n-1): all shuffleable days appear exactly once", "[BlockShufflePolicy]")
{
  // n=7, shuffleable=6, L=4 → 2 blocks: a full block of 4 and a short block of 2.
  // Despite the uneven partition, every day-unit must appear in the output.
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(4u);

  const auto expected = sortedOhlcTuples(orig, 1);
  for (int trial = 0; trial < 50; ++trial)
  {
    auto result = p.apply<DecimalType>(orig, rng);
    REQUIRE(result.size() == orig.size());
    REQUIRE(sortedOhlcTuples(result, 1) == expected);
  }
}

// ============================================================
// Section 11: BlockShufflePolicy::apply() — Block Atomicity
// ============================================================

TEST_CASE("BlockShufflePolicy::apply: days within each block maintain their original relative order (L=2, n=7)", "[BlockShufflePolicy]")
{
  // This is the defining invariant of N2: within each block the days always
  // appear in their original relative order.
  //
  // Setup: n=7, L=2 → 3 evenly-sized source blocks:
  //   B0 = (days 1, 2)   B1 = (days 3, 4)   B2 = (days 5, 6)
  //
  // After any apply(), the output at positions [1..6] must be a permutation of
  // {B0, B1, B2} where each block is emitted as a contiguous pair in its
  // original order.  We identify blocks by their unique close-value pairs.
  using DT = DecimalType;
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(2u);

  // Precompute the 3 source block close-value pairs.
  const std::vector<std::vector<DT>> srcBlocks = {
    srcBlockCloses(orig, 0, 2),   // closes at positions 1, 2
    srcBlockCloses(orig, 1, 2),   // closes at positions 3, 4
    srcBlockCloses(orig, 2, 2),   // closes at positions 5, 6
  };

  for (int trial = 0; trial < 100; ++trial)
  {
    auto result = p.apply<DT>(orig, rng);

    // Walk through the output in chunks of 2, verify each chunk is a known
    // source block and that each source block is used exactly once.
    std::vector<bool> blockSeen(3, false);
    size_t destPos = 1;

    for (size_t b = 0; b < 3; ++b, destPos += 2)
    {
      auto outChunk = outputCloseSlice(result, destPos, 2);

      bool matched = false;
      for (size_t s = 0; s < 3; ++s)
      {
        if (outChunk == srcBlocks[s])
        {
          REQUIRE_FALSE(blockSeen[s]);   // each source block used at most once
          blockSeen[s] = true;
          matched = true;
          break;
        }
      }
      REQUIRE(matched);   // every output chunk must correspond to a source block
    }

    // All three source blocks must appear.
    for (size_t s = 0; s < 3; ++s)
      REQUIRE(blockSeen[s]);
  }
}

TEST_CASE("BlockShufflePolicy::apply: block internal order preserved with non-divisible boundary (L=4, n=7)", "[BlockShufflePolicy]")
{
  // n=7, L=4 → B0=(days 1,2,3,4) and B1=(days 5,6).
  // The only valid outputs are:
  //   [anchor, B0, B1]  → positions 1-4 = days 1-4, positions 5-6 = days 5-6
  //   [anchor, B1, B0]  → positions 1-2 = days 5-6, positions 3-6 = days 1-4
  // Both preserve internal block order; no cross-block day interleaving is allowed.
  using DT = DecimalType;
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(4u);

  const auto& srcClose = orig.getClose();

  // Source block sequences (close values in order).
  const std::vector<DT> srcB0(srcClose.begin() + 1, srcClose.begin() + 5);  // days 1-4
  const std::vector<DT> srcB1(srcClose.begin() + 5, srcClose.begin() + 7);  // days 5-6

  for (int trial = 0; trial < 100; ++trial)
  {
    auto result     = p.apply<DT>(orig, rng);
    const auto& rc  = result.getClose();

    // Identify which block occupies the first output slot by matching close[1].
    if (rc[1] == srcB0[0])
    {
      // B0 was emitted first (positions 1..4), B1 second (positions 5..6).
      for (size_t i = 0; i < 4; ++i) REQUIRE(rc[1 + i] == srcB0[i]);
      for (size_t i = 0; i < 2; ++i) REQUIRE(rc[5 + i] == srcB1[i]);
    }
    else
    {
      // B1 was emitted first (positions 1..2), B0 second (positions 3..6).
      REQUIRE(rc[1] == srcB1[0]);
      for (size_t i = 0; i < 2; ++i) REQUIRE(rc[1 + i] == srcB1[i]);
      for (size_t i = 0; i < 4; ++i) REQUIRE(rc[3 + i] == srcB0[i]);
    }
  }
}

TEST_CASE("BlockShufflePolicy::apply with L=1: each day is its own block (N0 degenerate case)", "[BlockShufflePolicy]")
{
  // When L=1 every shuffleable day forms its own single-day block.
  // BlockShufflePolicy degenerates to PairedDayShufflePolicy behaviour:
  // days are permuted individually and the OHLC 4-tuple multiset is preserved.
  using DT = DecimalType;
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(1u);

  const auto expected = sortedOhlcTuples(orig, 1);
  for (int trial = 0; trial < 50; ++trial)
  {
    auto result = p.apply<DT>(orig, rng);
    REQUIRE(result.size() == orig.size());
    REQUIRE(result.getOpen()[0] == DecimalConstants<DT>::DecimalOne);
    REQUIRE(sortedOhlcTuples(result, 1) == expected);
  }
}

TEST_CASE("BlockShufflePolicy::apply with L >= shuffleable count: single block, output equals input", "[BlockShufflePolicy]")
{
  // n=7, shuffleable=6.  With L=6 the entire shuffleable range forms exactly
  // one block.  fisherYatesSubrange on a 1-element blockOrder vector is a
  // guaranteed no-op, so the output must be byte-for-byte identical to the input.
  using DT = DecimalType;
  auto orig = buildTestFactors7();
  RandomMersenne rng;

  for (size_t L : {6u, 7u, 100u})   // L == shuffleable, L > n, very large L
  {
    BlockShufflePolicy p(L);
    for (int trial = 0; trial < 10; ++trial)
    {
      auto result = p.apply<DT>(orig, rng);
      REQUIRE(result.getOpen()  == orig.getOpen());
      REQUIRE(result.getHigh()  == orig.getHigh());
      REQUIRE(result.getLow()   == orig.getLow());
      REQUIRE(result.getClose() == orig.getClose());
    }
  }
}

// ============================================================
// Section 12: BlockShufflePolicy::apply() — Produces Randomness
// ============================================================

TEST_CASE("BlockShufflePolicy::apply: produces different orderings across calls when numBlocks > 1", "[BlockShufflePolicy]")
{
  // n=7, L=2 → 3 blocks.  The identity permutation has probability 1/3! = 1/6.
  // Over 50 independent calls the probability that all return the identity is
  // (1/6)^50 < 10^{-38}, so we must observe at least one non-identity output.
  using DT = DecimalType;
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(2u);

  const auto origClose = orig.getClose();
  bool anyDifferent = false;
  for (int trial = 0; trial < 50 && !anyDifferent; ++trial)
  {
    auto result = p.apply<DT>(orig, rng);
    if (result.getClose() != origClose)
      anyDifferent = true;
  }
  REQUIRE(anyDifferent);
}

TEST_CASE("BlockShufflePolicy::apply with L=1: successive calls produce different orderings", "[BlockShufflePolicy]")
{
  // With n=7 and L=1 there are 6! = 720 possible permutations.  The probability
  // that two successive calls yield the same permutation is 1/720.  Over
  // 30 call-pairs that is astronomically unlikely to be always equal.
  using DT = DecimalType;
  auto orig = buildTestFactors7();
  RandomMersenne rng;
  BlockShufflePolicy p(1u);

  bool anyDifferent = false;
  auto prev = p.apply<DT>(orig, rng).getClose();
  for (int trial = 0; trial < 30 && !anyDifferent; ++trial)
  {
    auto next = p.apply<DT>(orig, rng).getClose();
    if (next != prev)
      anyDifferent = true;
    prev = next;
  }
  REQUIRE(anyDifferent);
}

// ============================================================
// Section 13: BlockShufflePolicy vs PairedDayShufflePolicy
// ============================================================

TEST_CASE("BlockShufflePolicy vs PairedDayShufflePolicy: N2 always preserves within-block adjacent pairs; N0 breaks them", "[BlockShufflePolicy][Compare]")
{
  // With L=2 and source blocks B0={(1,2)}, B1={(3,4)}, B2={(5,6)}:
  //
  //   N2 (BlockShufflePolicy, L=2): positions (1,2) in every output are always
  //   an original consecutive source pair — blocks travel as atomic units.
  //
  //   N0 (PairedDayShufflePolicy): individual days are shuffled independently,
  //   so positions (1,2) will frequently come from different source blocks.
  //
  // This test captures the core statistical difference between the two policies.
  using DT = DecimalType;
  auto orig = buildTestFactors7();
  RandomMersenne rng;

  // Helper: is the close pair (c1, c2) one of the three original adjacent pairs?
  const auto& sc = orig.getClose();
  auto isSourcePair = [&](DT c1, DT c2) {
    return (c1 == sc[1] && c2 == sc[2]) ||   // B0
           (c1 == sc[3] && c2 == sc[4]) ||   // B1
           (c1 == sc[5] && c2 == sc[6]);      // B2
  };

  // --- N2: positions (1,2) must ALWAYS be a source pair ---
  {
    BlockShufflePolicy blockPolicy(2u);
    for (int trial = 0; trial < 100; ++trial)
    {
      auto result = blockPolicy.apply<DT>(orig, rng);
      const auto& rc = result.getClose();
      REQUIRE(isSourcePair(rc[1], rc[2]));
    }
  }

  // --- N0: must SOMETIMES place non-source-pair at positions (1,2) ---
  // With 6 shuffleable days and 3 source pairs, P(same-block at pos 1,2) under
  // a uniform random permutation of individual days = P(day-2 immediately
  // follows day-1's block-partner) = 1/5 per position.  Over 200 trials the
  // probability that every result is still a source pair is (3/15)^200 ≈ 10^{-140}.
  {
    PairedDayShufflePolicy pairedPolicy;
    bool n0BrokePair = false;
    for (int trial = 0; trial < 200 && !n0BrokePair; ++trial)
    {
      auto result = pairedPolicy.apply<DT>(orig, rng);
      const auto& rc = result.getClose();
      if (!isSourcePair(rc[1], rc[2]))
        n0BrokePair = true;
    }
    REQUIRE(n0BrokePair);
  }
}

// ============================================================
// Section 14: shuffle_detail::computeBlockSize
// ============================================================

TEST_CASE("computeBlockSize: result is within [minBlock, maxBlock] for a normal series", "[computeBlockSize]")
{
  auto sample = createSampleEodSeries();
  const unsigned minBlock = 3u;
  const unsigned maxBlock = 20u;
  const size_t L = shuffle_detail::computeBlockSize(sample, minBlock, maxBlock);
  REQUIRE(L >= static_cast<size_t>(minBlock));
  REQUIRE(L <= static_cast<size_t>(maxBlock));
}

TEST_CASE("computeBlockSize: returns minBlock for a series too short to estimate ACF", "[computeBlockSize]")
{
  // A 3-entry series produces only 2 ROC returns (n=2 < 4 threshold).
  // The function must return minBlock immediately without attempting the ACF.
  OHLCTimeSeries<DecimalType> shortSeries(TimeFrame::DAILY, TradingVolume::SHARES);
  shortSeries.addEntry(*createEquityEntry("20070402","43.08","43.17","42.71","43.00",100));
  shortSeries.addEntry(*createEquityEntry("20070403","43.23","43.72","43.20","43.57",100));
  shortSeries.addEntry(*createEquityEntry("20070404","43.61","43.79","43.54","43.75",100));

  const size_t L = shuffle_detail::computeBlockSize(shortSeries, 3u, 20u);
  REQUIRE(L == 3u);
}

TEST_CASE("computeBlockSize: result is deterministic (no internal randomness)", "[computeBlockSize]")
{
  // The block size is derived from the ACF of squared returns — a purely
  // deterministic transformation of the series.  Two calls on the same data
  // must return the same value.
  auto sample = createSampleEodSeries();
  const size_t L1 = shuffle_detail::computeBlockSize(sample);
  const size_t L2 = shuffle_detail::computeBlockSize(sample);
  REQUIRE(L1 == L2);
}

TEST_CASE("computeBlockSize: default argument bounds are [3, 20]", "[computeBlockSize]")
{
  // Calling with no explicit bounds uses minBlock=3, maxBlock=20.
  auto sample = createSampleEodSeries();
  const size_t L = shuffle_detail::computeBlockSize(sample);
  REQUIRE(L >= 3u);
  REQUIRE(L <= 20u);
}

// ============================================================
// Section 15: SyntheticTimeSeries<N2_BlockDays> — Integration
// ============================================================

TEST_CASE("SyntheticTimeSeries N2: constructor initialises metadata correctly", "[SyntheticTimeSeries][N2]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN2<DT> synth(sample, tick, tickDiv2);

  REQUIRE(synth.getNumElements() == sample.getNumEntries());
  REQUIRE(synth.getTick()        == tick);
  REQUIRE(synth.getTickDiv2()    == tickDiv2);
  REQUIRE(synth.getFirstOpen()   == sample.beginSortedAccess()->getOpenValue());
  // No series generated yet — pointer must be null.
  REQUIRE(synth.getSyntheticTimeSeries() == nullptr);
}

TEST_CASE("SyntheticTimeSeries N2: createSyntheticSeries does not throw and returns non-null", "[SyntheticTimeSeries][N2]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN2<DT> synth(sample, tick, tickDiv2);
  REQUIRE_NOTHROW(synth.createSyntheticSeries());
  REQUIRE(synth.getSyntheticTimeSeries() != nullptr);
}

TEST_CASE("SyntheticTimeSeries N2: synthetic series preserves structural metadata", "[SyntheticTimeSeries][N2]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN2<DT> synth(sample, tick, tickDiv2);
  synth.createSyntheticSeries();
  auto synthPtr = synth.getSyntheticTimeSeries();
  REQUIRE(synthPtr != nullptr);

  REQUIRE(synthPtr->getNumEntries() == sample.getNumEntries());
  REQUIRE(synthPtr->getTimeFrame()  == sample.getTimeFrame());
  REQUIRE(synthPtr->getFirstDate()  == sample.getFirstDate());
  REQUIRE(synthPtr->getLastDate()   == sample.getLastDate());
}

TEST_CASE("SyntheticTimeSeries N2: every bar satisfies H >= O, H >= C, L <= O, L <= C, all positive", "[SyntheticTimeSeries][N2]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN2<DT> synth(sample, tick, tickDiv2);
  synth.createSyntheticSeries();
  auto synthPtr = synth.getSyntheticTimeSeries();
  REQUIRE(synthPtr != nullptr);

  const DT zero = DecimalConstants<DT>::DecimalZero;
  for (auto it = synthPtr->beginSortedAccess(); it != synthPtr->endSortedAccess(); ++it)
  {
    REQUIRE(it->getHighValue()  >= it->getOpenValue());
    REQUIRE(it->getHighValue()  >= it->getCloseValue());
    REQUIRE(it->getLowValue()   <= it->getOpenValue());
    REQUIRE(it->getLowValue()   <= it->getCloseValue());
    REQUIRE(it->getOpenValue()  >  zero);
    REQUIRE(it->getHighValue()  >  zero);
    REQUIRE(it->getLowValue()   >  zero);
    REQUIRE(it->getCloseValue() >  zero);
  }
}

TEST_CASE("SyntheticTimeSeries N2: relative-factor multisets match original (permutation property)", "[SyntheticTimeSeries][N2]")
{
  // N2 is a permutation of blocks; every original day-unit factor appears in
  // the synthetic series exactly once, so the sorted multisets of relative-open
  // and relative-close factors must be identical before and after shuffling.
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  // Capture the original (pre-shuffle) factor baseline.
  SyntheticN2<DT> ref(sample, tick, tickDiv2);
  auto origOpen  = ref.getRelativeOpen();
  auto origClose = ref.getRelativeClose();

  // Apply a shuffle and compare sorted factor multisets.
  SyntheticN2<DT> synth(sample, tick, tickDiv2);
  synth.createSyntheticSeries();
  auto shuffledOpen  = synth.getRelativeOpen();
  auto shuffledClose = synth.getRelativeClose();

  auto sortVec = [](std::vector<DT> v) { std::sort(v.begin(), v.end()); return v; };
  REQUIRE(sortVec(shuffledOpen)  == sortVec(origOpen));
  REQUIRE(sortVec(shuffledClose) == sortVec(origClose));
}

TEST_CASE("SyntheticTimeSeries N2: synthetic series differs from original", "[SyntheticTimeSeries][N2]")
{
  // With 30 bars and L in [3,20], there are at least ceil(29/20)=2 blocks,
  // giving at least 2! = 2 possible orderings.  The probability that the shuffled
  // result equals the original (identity permutation) on any single call is at
  // most 1/2 — over 30 independent calls, P(all identity) <= (1/2)^30 < 10^{-9}.
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  bool anyDifferent = false;
  for (int attempt = 0; attempt < 30 && !anyDifferent; ++attempt)
  {
    SyntheticN2<DT> synth(sample, tick, tickDiv2);
    synth.createSyntheticSeries();
    auto synthPtr = synth.getSyntheticTimeSeries();
    REQUIRE(synthPtr != nullptr);
    if (sample != *synthPtr)
      anyDifferent = true;
  }
  REQUIRE(anyDifferent);
}

TEST_CASE("SyntheticTimeSeries N2: copy constructor preserves series state", "[SyntheticTimeSeries][N2]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN2<DT> original(sample, tick, tickDiv2);
  original.createSyntheticSeries();

  SyntheticN2<DT> copy(original);

  REQUIRE(copy.getNumElements() == original.getNumElements());
  REQUIRE(copy.getFirstOpen()   == original.getFirstOpen());
  REQUIRE(copy.getTick()        == original.getTick());
  REQUIRE(copy.getTickDiv2()    == original.getTickDiv2());
  REQUIRE(*copy.getSyntheticTimeSeries() == *original.getSyntheticTimeSeries());
}

TEST_CASE("SyntheticTimeSeries N2: produces unique permutations across independent instances", "[SyntheticTimeSeries][N2][MonteCarlo]")
{
  // Every independent SyntheticN2 instance seeds its RNG from OS entropy
  // (Fix #5 from the copy-constructor review).  Over 20 independent instances
  // every pair of generated series must differ.
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  const int N = 20;
  std::vector<std::shared_ptr<const OHLCTimeSeries<DT>>> perms;
  perms.reserve(N);
  for (int i = 0; i < N; ++i)
  {
    SyntheticN2<DT> synth(sample, tick, tickDiv2);
    synth.createSyntheticSeries();
    perms.push_back(synth.getSyntheticTimeSeries());
  }

  for (int i = 0; i < N; ++i)
    for (int j = i + 1; j < N; ++j)
      REQUIRE(*perms[i] != *perms[j]);
}

TEST_CASE("SyntheticTimeSeries N2: repeated createSyntheticSeries calls on same instance produce different series", "[SyntheticTimeSeries][N2]")
{
  // The working factor set is replaced on every shuffleFactors() call;
  // the immutable baseline is never modified.  Repeated calls on the same
  // instance must therefore produce independent permutations.
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN2<DT> synth(sample, tick, tickDiv2);

  synth.createSyntheticSeries();
  auto first = *synth.getSyntheticTimeSeries();

  bool anyDifferent = false;
  for (int attempt = 0; attempt < 30 && !anyDifferent; ++attempt)
  {
    synth.createSyntheticSeries();
    if (*synth.getSyntheticTimeSeries() != first)
      anyDifferent = true;
  }
  REQUIRE(anyDifferent);
}
