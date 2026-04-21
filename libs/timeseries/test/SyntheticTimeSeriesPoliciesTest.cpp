// SyntheticTimeSeriesNewPoliciesTest.cpp
//
// Unit tests for:
//   - shuffle_detail::fisherYatesSubrange
//   - shuffle_detail::generatePermutation
//   - EodFactors<Decimal>
//   - IndependentShufflePolicy
//   - PairedDayShufflePolicy
//   - SyntheticTimeSeries<..., SyntheticNullModel::N0_PairedDay>
//
// Compile alongside the existing SyntheticTimeSeriesTest.cpp.

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
  // Builds a small EodFactors set with n=5 bars (all values distinct)
  // suitable for structural testing of both shuffle policies.
  //   index 0 : anchor bar  (open = 1.0 as the initEodDataInternal convention)
  //   index 1-4 : permutable bars
  EodFactors<DecimalType> buildTestFactors5()
  {
    using DT = DecimalType;
    return EodFactors<DT>(
      /*open */  {DT("1.000"), DT("1.010"), DT("0.990"), DT("1.020"), DT("0.980")},
      /*high */  {DT("1.050"), DT("1.060"), DT("1.040"), DT("1.070"), DT("1.030")},
      /*low  */  {DT("0.950"), DT("0.960"), DT("0.940"), DT("0.970"), DT("0.930")},
      /*close*/  {DT("1.010"), DT("1.020"), DT("1.000"), DT("1.030"), DT("0.990")}
    );
  }

  // Builds a minimal 2-bar EodFactors (anchor-only for policy edge-case tests).
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

  // Returns a sorted vector of (H, L, C) 3-tuples from an EodFactors, which
  // is the invariant that IndependentShufflePolicy must preserve.
  using HlcTuple = std::tuple<DecimalType, DecimalType, DecimalType>;
  std::vector<HlcTuple> sortedHlcTuples(const EodFactors<DecimalType>& f)
  {
    std::vector<HlcTuple> t;
    t.reserve(f.size());
    for (size_t i = 0; i < f.size(); ++i)
      t.emplace_back(f.getHigh()[i], f.getLow()[i], f.getClose()[i]);
    std::sort(t.begin(), t.end());
    return t;
  }

  // Returns a sorted vector of (O, H, L, C) 4-tuples, the invariant that
  // PairedDayShufflePolicy preserves for positions [from, n).
  using OhlcTuple = std::tuple<DecimalType, DecimalType, DecimalType, DecimalType>;
  std::vector<OhlcTuple> sortedOhlcTuples(const EodFactors<DecimalType>& f, size_t from)
  {
    std::vector<OhlcTuple> t;
    for (size_t i = from; i < f.size(); ++i)
      t.emplace_back(f.getOpen()[i], f.getHigh()[i], f.getLow()[i], f.getClose()[i]);
    std::sort(t.begin(), t.end());
    return t;
  }

  // Builds the daily EOD sample series used by most existing EOD tests.
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

// ============================================================
// Section 1: EodFactors
// ============================================================

TEST_CASE("EodFactors: default-constructed instance is empty", "[EodFactors]")
{
  EodFactors<DecimalType> f;
  REQUIRE(f.empty());
  REQUIRE(f.size() == 0u);
  REQUIRE(f.getOpen().empty());
  REQUIRE(f.getHigh().empty());
  REQUIRE(f.getLow().empty());
  REQUIRE(f.getClose().empty());
}

TEST_CASE("EodFactors: four-vector constructor stores data correctly", "[EodFactors]")
{
  using DT = DecimalType;
  std::vector<DT> o = {DT("1.0"), DT("1.01"), DT("0.99")};
  std::vector<DT> h = {DT("1.02"), DT("1.03"), DT("1.00")};
  std::vector<DT> l = {DT("0.98"), DT("0.99"), DT("0.97")};
  std::vector<DT> c = {DT("1.00"), DT("1.01"), DT("0.98")};

  EodFactors<DT> f(o, h, l, c);

  REQUIRE_FALSE(f.empty());
  REQUIRE(f.size() == 3u);
  REQUIRE(f.getOpen()  == o);
  REQUIRE(f.getHigh()  == h);
  REQUIRE(f.getLow()   == l);
  REQUIRE(f.getClose() == c);
}

TEST_CASE("EodFactors: copy semantics produce independent copies", "[EodFactors]")
{
  using DT = DecimalType;
  auto orig = buildTestFactors5();
  EodFactors<DT> copy = orig;           // copy-construct

  REQUIRE(copy.size()     == orig.size());
  REQUIRE(copy.getOpen()  == orig.getOpen());
  REQUIRE(copy.getHigh()  == orig.getHigh());
  REQUIRE(copy.getLow()   == orig.getLow());
  REQUIRE(copy.getClose() == orig.getClose());
}

TEST_CASE("EodFactors: move semantics leave source empty", "[EodFactors]")
{
  using DT = DecimalType;
  auto src = buildTestFactors5();
  const size_t expectedSize = src.size();
  EodFactors<DT> moved = std::move(src);

  REQUIRE(moved.size() == expectedSize);
  REQUIRE(src.empty()); // NOLINT — intentional post-move check
}

// ============================================================
// Section 2: shuffle_detail::fisherYatesSubrange
// ============================================================

TEST_CASE("shuffle_detail::fisherYatesSubrange: empty vector does not crash", "[detail][FisherYates]")
{
  std::vector<int> v;
  RandomMersenne rng;
  REQUIRE_NOTHROW(shuffle_detail::fisherYatesSubrange(v, 0u, rng));
  REQUIRE(v.empty());
}

TEST_CASE("shuffle_detail::fisherYatesSubrange: no-op when n < firstShufflable+2", "[detail][FisherYates]")
{
  // n=3, firstShufflable=2 → n(3) < firstShufflable+2(4) → nothing shuffled
  std::vector<int> v = {10, 20, 30};
  const auto expected = v;
  RandomMersenne rng;
  shuffle_detail::fisherYatesSubrange(v, 2u, rng);
  REQUIRE(v == expected);
}

TEST_CASE("shuffle_detail::fisherYatesSubrange: single shufflable element is unchanged", "[detail][FisherYates]")
{
  // n=2, firstShufflable=1 → only one shufflable element — trivially fixed
  std::vector<int> v = {5, 7};
  const auto expected = v;
  RandomMersenne rng;
  shuffle_detail::fisherYatesSubrange(v, 1u, rng);
  REQUIRE(v == expected);
}

TEST_CASE("shuffle_detail::fisherYatesSubrange: anchor elements [0, firstShufflable) are unchanged", "[detail][FisherYates]")
{
  // firstShufflable=1: only index 0 is anchored
  std::vector<int> v = {99, 1, 2, 3, 4, 5, 6, 7};
  RandomMersenne rng;
  shuffle_detail::fisherYatesSubrange(v, 1u, rng);

  REQUIRE(v[0] == 99);

  std::vector<int> suffix(v.begin() + 1, v.end());
  std::sort(suffix.begin(), suffix.end());
  REQUIRE(suffix == std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
}

TEST_CASE("shuffle_detail::fisherYatesSubrange: two anchor elements are both preserved", "[detail][FisherYates]")
{
  // firstShufflable=2: indices 0 and 1 are anchored
  std::vector<int> v = {11, 22, 3, 4, 5, 6};
  RandomMersenne rng;
  shuffle_detail::fisherYatesSubrange(v, 2u, rng);

  REQUIRE(v[0] == 11);
  REQUIRE(v[1] == 22);

  std::vector<int> suffix(v.begin() + 2, v.end());
  std::sort(suffix.begin(), suffix.end());
  REQUIRE(suffix == std::vector<int>({3, 4, 5, 6}));
}

TEST_CASE("shuffle_detail::fisherYatesSubrange: result is always a valid permutation of input", "[detail][FisherYates]")
{
  std::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto v = original;
  RandomMersenne rng;
  shuffle_detail::fisherYatesSubrange(v, 1u, rng);
  REQUIRE(std::is_permutation(v.begin(), v.end(), original.begin()));
}

TEST_CASE("shuffle_detail::fisherYatesSubrange: repeated calls produce different orderings", "[detail][FisherYates]")
{
  // With 10 shufflable elements the chance of getting the same order twice is negligible.
  std::vector<int> base = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  RandomMersenne rng;
  auto v1 = base;
  auto v2 = base;
  shuffle_detail::fisherYatesSubrange(v1, 1u, rng);
  shuffle_detail::fisherYatesSubrange(v2, 1u, rng);
  // At least one of the two calls should differ from the base — both being
  // the identity simultaneously is astronomically unlikely.
  REQUIRE((v1 != base || v2 != base));
}

// ============================================================
// Section 3: shuffle_detail::generatePermutation
// ============================================================

TEST_CASE("shuffle_detail::generatePermutation: n=0 returns empty vector", "[detail][generatePermutation]")
{
  RandomMersenne rng;
  auto perm = shuffle_detail::generatePermutation(0u, 0u, rng);
  REQUIRE(perm.empty());
}

TEST_CASE("shuffle_detail::generatePermutation: identity when n < firstShufflable+2", "[detail][generatePermutation]")
{
  // n=3, firstShufflable=2 → n(3) < 4 → identity
  RandomMersenne rng;
  auto perm = shuffle_detail::generatePermutation(3u, 2u, rng);
  REQUIRE(perm == std::vector<size_t>({0u, 1u, 2u}));
}

TEST_CASE("shuffle_detail::generatePermutation: anchor indices [0, firstShufflable) are identity-mapped", "[detail][generatePermutation]")
{
  RandomMersenne rng;
  const size_t n = 8u, firstShuffle = 2u;
  auto perm = shuffle_detail::generatePermutation(n, firstShuffle, rng);

  REQUIRE(perm.size() == n);
  // Anchored portion must be [0, 1]
  for (size_t i = 0; i < firstShuffle; ++i)
    REQUIRE(perm[i] == i);
}

TEST_CASE("shuffle_detail::generatePermutation: result is a valid permutation of [0, n)", "[detail][generatePermutation]")
{
  RandomMersenne rng;
  const size_t n = 10u;
  auto perm = shuffle_detail::generatePermutation(n, 1u, rng);

  REQUIRE(perm.size() == n);
  // perm[0] fixed to 0
  REQUIRE(perm[0] == 0u);

  auto sorted = perm;
  std::sort(sorted.begin(), sorted.end());
  std::vector<size_t> expected(n);
  std::iota(expected.begin(), expected.end(), size_t{0});
  REQUIRE(sorted == expected);
}

TEST_CASE("shuffle_detail::generatePermutation: shuffled portion differs from identity across calls", "[detail][generatePermutation]")
{
  RandomMersenne rng;
  const size_t n = 8u;
  bool anyDifferent = false;
  auto first = shuffle_detail::generatePermutation(n, 1u, rng);
  for (int attempt = 0; attempt < 30 && !anyDifferent; ++attempt)
  {
    auto next = shuffle_detail::generatePermutation(n, 1u, rng);
    if (next != first) anyDifferent = true;
  }
  REQUIRE(anyDifferent);
}

// ============================================================
// Section 4: IndependentShufflePolicy
// ============================================================

TEST_CASE("IndependentShufflePolicy: output size matches input", "[IndependentShufflePolicy]")
{
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  auto result = IndependentShufflePolicy::apply<DecimalType>(orig, rng);
  REQUIRE(result.size() == orig.size());
}

TEST_CASE("IndependentShufflePolicy: anchor open (index 0) is unchanged across shuffles", "[IndependentShufflePolicy]")
{
  // The first element of the open vector is the anchor (overnight gap from day 0 to day 1
  // is excluded from shuffling); it must always equal the original value.
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  for (int i = 0; i < 20; ++i)
  {
    auto result = IndependentShufflePolicy::apply<DecimalType>(orig, rng);
    REQUIRE(result.getOpen()[0] == orig.getOpen()[0]);
  }
}

TEST_CASE("IndependentShufflePolicy: open-factor multiset is preserved", "[IndependentShufflePolicy]")
{
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  auto result = IndependentShufflePolicy::apply<DecimalType>(orig, rng);

  auto origOpen = orig.getOpen();
  auto resOpen  = result.getOpen();
  std::sort(origOpen.begin(), origOpen.end());
  std::sort(resOpen.begin(), resOpen.end());
  REQUIRE(origOpen == resOpen);
}

TEST_CASE("IndependentShufflePolicy: (High, Low, Close) tuple multiset is preserved", "[IndependentShufflePolicy]")
{
  // The intraday shape for each day (H, L, C normalised to that day's open) must
  // travel as an atomic unit so the intraday relationships are not broken.
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  auto result = IndependentShufflePolicy::apply<DecimalType>(orig, rng);
  REQUIRE(sortedHlcTuples(orig) == sortedHlcTuples(result));
}

TEST_CASE("IndependentShufflePolicy: open permutation is INDEPENDENT from HLC permutation", "[IndependentShufflePolicy]")
{
  // If open and HLC used the same permutation, the source index for open[k] and
  // high[k] would always coincide. After enough trials with distinct values we
  // expect to observe at least one position where they do NOT come from the same
  // original index, which is only possible with independent permutations.
  using DT = DecimalType;
  auto orig = buildTestFactors5();
  RandomMersenne rng;

  const auto& oOpen = orig.getOpen();
  const auto& oHigh = orig.getHigh();

  bool foundMismatch = false;
  for (int trial = 0; trial < 200 && !foundMismatch; ++trial)
  {
    auto result = IndependentShufflePolicy::apply<DT>(orig, rng);
    const auto& rOpen = result.getOpen();
    const auto& rHigh = result.getHigh();

    for (size_t i = 1; i < orig.size(); ++i)
    {
      // Find the source index for this open value.
      auto itO = std::find(oOpen.begin() + 1, oOpen.end(), rOpen[i]);
      // Find the source index for this high value.
      auto itH = std::find(oHigh.begin() + 1, oHigh.end(), rHigh[i]);
      if (itO != oOpen.end() && itH != oHigh.end())
      {
        // If they came from different original positions → independent shuffle confirmed.
        if (std::distance(oOpen.begin(), itO) != std::distance(oHigh.begin(), itH))
        {
          foundMismatch = true;
          break;
        }
      }
    }
  }
  REQUIRE(foundMismatch);
}

// ============================================================
// Section 5: PairedDayShufflePolicy
// ============================================================

TEST_CASE("PairedDayShufflePolicy: output size matches input", "[PairedDayShufflePolicy]")
{
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  auto result = PairedDayShufflePolicy::apply<DecimalType>(orig, rng);
  REQUIRE(result.size() == orig.size());
}

TEST_CASE("PairedDayShufflePolicy: returns unchanged copy when n <= 2", "[PairedDayShufflePolicy]")
{
  // The policy explicitly returns orig when n <= 2 (nothing to permute).
  auto small = buildTestFactors2();
  RandomMersenne rng;
  auto result = PairedDayShufflePolicy::apply<DecimalType>(small, rng);

  REQUIRE(result.getOpen()  == small.getOpen());
  REQUIRE(result.getHigh()  == small.getHigh());
  REQUIRE(result.getLow()   == small.getLow());
  REQUIRE(result.getClose() == small.getClose());
}

TEST_CASE("PairedDayShufflePolicy: open[0] is always forced to DecimalOne", "[PairedDayShufflePolicy]")
{
  // Regardless of what value was in orig.open[0], the policy overwrites position 0
  // with 1.0 to preserve the anchor-bar convention.
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  for (int i = 0; i < 30; ++i)
  {
    auto result = PairedDayShufflePolicy::apply<DecimalType>(orig, rng);
    REQUIRE(result.getOpen()[0] == DecimalConstants<DecimalType>::DecimalOne);
  }
}

TEST_CASE("PairedDayShufflePolicy: anchor bar H, L, C are unchanged", "[PairedDayShufflePolicy]")
{
  // generatePermutation with firstShufflable=1 fixes perm[0]=0, so the anchor
  // bar's high/low/close always originate from position 0 of the original.
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  for (int i = 0; i < 30; ++i)
  {
    auto result = PairedDayShufflePolicy::apply<DecimalType>(orig, rng);
    REQUIRE(result.getHigh()[0]  == orig.getHigh()[0]);
    REQUIRE(result.getLow()[0]   == orig.getLow()[0]);
    REQUIRE(result.getClose()[0] == orig.getClose()[0]);
  }
}

TEST_CASE("PairedDayShufflePolicy: OHLC 4-tuple multiset is preserved for positions 1..n-1", "[PairedDayShufflePolicy]")
{
  // The single shared permutation shuffles all four arrays together; therefore
  // every (O, H, L, C) day-unit from the original must appear in the result.
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  auto result = PairedDayShufflePolicy::apply<DecimalType>(orig, rng);

  REQUIRE(sortedOhlcTuples(orig, 1) == sortedOhlcTuples(result, 1));
}

TEST_CASE("PairedDayShufflePolicy: all four arrays use the SAME permutation", "[PairedDayShufflePolicy]")
{
  // For each position i >= 1, if result.open[i] == orig.open[src] then
  // result.high[i] must equal orig.high[src] (same src for all four arrays).
  // This is only guaranteed because all arrays apply the identical permutation.
  using DT = DecimalType;
  auto orig = buildTestFactors5();
  RandomMersenne rng;
  // Run enough trials to hit a non-trivial permutation.
  for (int trial = 0; trial < 50; ++trial)
  {
    auto result = PairedDayShufflePolicy::apply<DT>(orig, rng);
    const auto& rO = result.getOpen();
    const auto& rH = result.getHigh();
    const auto& rL = result.getLow();
    const auto& rC = result.getClose();
    const auto& oO = orig.getOpen();
    const auto& oH = orig.getHigh();
    const auto& oL = orig.getLow();
    const auto& oC = orig.getClose();

    for (size_t i = 1; i < orig.size(); ++i)
    {
      // Locate the source index via the open value (all values are distinct in test data).
      auto it = std::find(oO.begin() + 1, oO.end(), rO[i]);
      REQUIRE(it != oO.end()); // open value must exist in original
      const size_t src = static_cast<size_t>(std::distance(oO.begin(), it));
      // Verify that H, L, C at position i come from the same source.
      REQUIRE(rH[i] == oH[src]);
      REQUIRE(rL[i] == oL[src]);
      REQUIRE(rC[i] == oC[src]);
    }
  }
}

TEST_CASE("PairedDayShufflePolicy: open and HLC are NEVER from different source indices", "[PairedDayShufflePolicy]")
{
  // Complementary to the above: we must NEVER observe the independence that
  // IndependentShufflePolicy intentionally exhibits.  Over many trials with N0
  // the mismatch count should always be zero.
  using DT = DecimalType;
  auto orig = buildTestFactors5();
  RandomMersenne rng;

  const auto& oOpen = orig.getOpen();
  const auto& oHigh = orig.getHigh();

  int mismatchCount = 0;
  for (int trial = 0; trial < 200; ++trial)
  {
    auto result = PairedDayShufflePolicy::apply<DT>(orig, rng);
    const auto& rOpen = result.getOpen();
    const auto& rHigh = result.getHigh();

    for (size_t i = 1; i < orig.size(); ++i)
    {
      auto itO = std::find(oOpen.begin() + 1, oOpen.end(), rOpen[i]);
      auto itH = std::find(oHigh.begin() + 1, oHigh.end(), rHigh[i]);
      if (itO != oOpen.end() && itH != oHigh.end())
        if (std::distance(oOpen.begin(), itO) != std::distance(oHigh.begin(), itH))
          ++mismatchCount;
    }
  }
  REQUIRE(mismatchCount == 0);
}

// ============================================================
// Section 6: SyntheticTimeSeries with SyntheticNullModel::N0_PairedDay
// ============================================================

// Convenience alias for N0 to reduce boilerplate in each test case.
template <class Decimal>
using SyntheticN0 = SyntheticTimeSeries<
  Decimal,
  mkc_timeseries::LogNLookupPolicy<Decimal>,
  NoRounding,
  SyntheticNullModel::N0_PairedDay>;

TEST_CASE("SyntheticNullModel enum: values are as documented", "[SyntheticNullModel]")
{
  // Verifies the enum's underlying integer values are stable — important if
  // callers store or compare the enum numerically.
  REQUIRE(static_cast<int>(SyntheticNullModel::N1_MaxDestruction) == 0);
  REQUIRE(static_cast<int>(SyntheticNullModel::N0_PairedDay)      == 1);
  REQUIRE(static_cast<int>(SyntheticNullModel::N2_BlockDays)      == 2);
}

TEST_CASE("SyntheticTimeSeries N0: constructor initialises metadata correctly", "[SyntheticTimeSeries][N0]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN0<DT> synth(sample, tick, tickDiv2);

  REQUIRE(synth.getNumElements() == sample.getNumEntries());
  REQUIRE(synth.getTick()     == tick);
  REQUIRE(synth.getTickDiv2() == tickDiv2);
  REQUIRE(synth.getFirstOpen() == sample.beginSortedAccess()->getOpenValue());
  // No series generated yet.
  REQUIRE(synth.getSyntheticTimeSeries() == nullptr);
}

TEST_CASE("SyntheticTimeSeries N0: createSyntheticSeries does not throw", "[SyntheticTimeSeries][N0]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN0<DT> synth(sample, tick, tickDiv2);
  REQUIRE_NOTHROW(synth.createSyntheticSeries());
  REQUIRE(synth.getSyntheticTimeSeries() != nullptr);
}

TEST_CASE("SyntheticTimeSeries N0: synthetic series preserves structural metadata", "[SyntheticTimeSeries][N0]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN0<DT> synth(sample, tick, tickDiv2);
  synth.createSyntheticSeries();
  auto synthPtr = synth.getSyntheticTimeSeries();
  REQUIRE(synthPtr != nullptr);

  REQUIRE(synthPtr->getNumEntries() == sample.getNumEntries());
  REQUIRE(synthPtr->getTimeFrame()  == sample.getTimeFrame());
  REQUIRE(synthPtr->getFirstDate()  == sample.getFirstDate());
  REQUIRE(synthPtr->getLastDate()   == sample.getLastDate());
}

TEST_CASE("SyntheticTimeSeries N0: synthetic series differs from original", "[SyntheticTimeSeries][N0]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN0<DT> synth(sample, tick, tickDiv2);
  synth.createSyntheticSeries();
  auto synthPtr = synth.getSyntheticTimeSeries();
  REQUIRE(synthPtr != nullptr);
  REQUIRE(sample != *synthPtr);
}

TEST_CASE("SyntheticTimeSeries N0: every bar satisfies H >= O, H >= C, L <= O, L <= C", "[SyntheticTimeSeries][N0]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN0<DT> synth(sample, tick, tickDiv2);
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

TEST_CASE("SyntheticTimeSeries N0: relative-factor multisets match original (permutation property)", "[SyntheticTimeSeries][N0]")
{
  // The N0 shuffle is a permutation; therefore the multiset of relative-open factors
  // and the multiset of relative-close factors in the shuffled state must equal those
  // of the unshuffled (original) baseline.
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  // Capture the original (pre-shuffle) factors via an unshuffled N0 instance.
  SyntheticN0<DT> synthRef(sample, tick, tickDiv2);
  auto origOpen  = synthRef.getRelativeOpen();
  auto origClose = synthRef.getRelativeClose();

  // Apply a shuffle and capture the working state.
  SyntheticN0<DT> synth(sample, tick, tickDiv2);
  synth.createSyntheticSeries();
  auto shuffledOpen  = synth.getRelativeOpen();
  auto shuffledClose = synth.getRelativeClose();

  auto sort = [](std::vector<DT> v) { std::sort(v.begin(), v.end()); return v; };
  REQUIRE(sort(shuffledOpen)  == sort(origOpen));
  REQUIRE(sort(shuffledClose) == sort(origClose));
}

TEST_CASE("SyntheticTimeSeries N0: copy constructor preserves state", "[SyntheticTimeSeries][N0]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  SyntheticN0<DT> original(sample, tick, tickDiv2);
  original.createSyntheticSeries();

  SyntheticN0<DT> copy(original);

  REQUIRE(copy.getNumElements() == original.getNumElements());
  REQUIRE(copy.getFirstOpen()   == original.getFirstOpen());
  REQUIRE(copy.getTick()        == original.getTick());
  REQUIRE(copy.getTickDiv2()    == original.getTickDiv2());
  REQUIRE(*copy.getSyntheticTimeSeries() == *original.getSyntheticTimeSeries());
}

TEST_CASE("SyntheticTimeSeries N0: produces unique permutations across independent instances", "[SyntheticTimeSeries][N0][MonteCarlo]")
{
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  const int N = 20;
  std::vector<std::shared_ptr<const OHLCTimeSeries<DT>>> perms;
  perms.reserve(N);
  for (int i = 0; i < N; ++i)
  {
    SyntheticN0<DT> synth(sample, tick, tickDiv2);
    synth.createSyntheticSeries();
    perms.push_back(synth.getSyntheticTimeSeries());
  }

  for (int i = 0; i < N; ++i)
    for (int j = i + 1; j < N; ++j)
      REQUIRE(*perms[i] != *perms[j]);
}

TEST_CASE("SyntheticTimeSeries N0 vs N1: structural policy difference (OHLC tuple atomicity)", "[SyntheticTimeSeries][N0][N1][Compare]")
{
  // In N0 every day's four relative factors (O, H, L, C) travel together, so
  // the multiset of (O, H, L, C) factor 4-tuples is preserved.
  // In N1 open is shuffled independently from HLC, so the multiset of 4-tuples
  // is NOT generally preserved (it should differ from the original in almost all cases).
  using DT = DecimalType;
  auto sample = createSampleEodSeries();
  DT tick     = DecimalConstants<DT>::EquityTick;
  DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  // Capture original factor 4-tuples.
  SyntheticN0<DT> ref(sample, tick, tickDiv2);
  const auto origOpen  = ref.getRelativeOpen();
  const auto origHigh  = ref.getRelativeHigh();
  const auto origLow   = ref.getRelativeLow();
  const auto origClose = ref.getRelativeClose();

  auto makeTuples = [](const std::vector<DT>& o, const std::vector<DT>& h,
                       const std::vector<DT>& l, const std::vector<DT>& c) {
    std::vector<OhlcTuple> t;
    for (size_t i = 1; i < o.size(); ++i)
      t.emplace_back(o[i], h[i], l[i], c[i]);
    std::sort(t.begin(), t.end());
    return t;
  };

  const auto origTuples = makeTuples(origOpen, origHigh, origLow, origClose);

  // --- N0: 4-tuples must be preserved ---
  {
    SyntheticN0<DT> synth(sample, tick, tickDiv2);
    synth.createSyntheticSeries();
    auto sO = synth.getRelativeOpen();
    auto sH = synth.getRelativeHigh();
    auto sL = synth.getRelativeLow();
    auto sC = synth.getRelativeClose();

    REQUIRE(makeTuples(sO, sH, sL, sC) == origTuples);
  }

  // --- N1: 4-tuples are NOT generally preserved (independent open shuffle breaks them) ---
  {
    // Try up to 50 shuffles; the very first one should already break tuples.
    bool tupleBroken = false;
    for (int i = 0; i < 50 && !tupleBroken; ++i)
    {
      SyntheticTimeSeries<DT> synthN1(sample, tick, tickDiv2);
      synthN1.createSyntheticSeries();
      auto sO = synthN1.getRelativeOpen();
      auto sH = synthN1.getRelativeHigh();
      auto sL = synthN1.getRelativeLow();
      auto sC = synthN1.getRelativeClose();

      if (makeTuples(sO, sH, sL, sC) != origTuples)
        tupleBroken = true;
    }
    REQUIRE(tupleBroken);
  }
}

// --- Section 2 additions: fisherYatesSubrange precondition ---

TEST_CASE("shuffle_detail::fisherYatesSubrange: throws when firstShufflable > v.size()",
          "[detail][FisherYates]")
{
  std::vector<int> v = {1, 2, 3};
  RandomMersenne rng;
  // firstShufflable == 4, size == 3 → strict misuse
  REQUIRE_THROWS_AS(shuffle_detail::fisherYatesSubrange(v, 4u, rng), std::out_of_range);
}

TEST_CASE("shuffle_detail::fisherYatesSubrange: firstShufflable == v.size() is a valid no-op",
          "[detail][FisherYates]")
{
  // Boundary case: firstShufflable equal to size means "everything is anchored,
  // nothing to shuffle" — this is a well-defined request and must NOT throw.
  std::vector<int> v = {1, 2, 3};
  const auto expected = v;
  RandomMersenne rng;
  REQUIRE_NOTHROW(shuffle_detail::fisherYatesSubrange(v, 3u, rng));
  REQUIRE(v == expected);
}

// --- Section 3 addition: generatePermutation precondition ---

TEST_CASE("shuffle_detail::generatePermutation: throws when firstShufflable > n",
          "[detail][generatePermutation]")
{
  RandomMersenne rng;
  // n = 3, firstShufflable = 5 → strict misuse
  REQUIRE_THROWS_AS(shuffle_detail::generatePermutation(3u, 5u, rng), std::out_of_range);
}

TEST_CASE("shuffle_detail::generatePermutation: firstShufflable == n is a valid identity return",
          "[detail][generatePermutation]")
{
  RandomMersenne rng;
  auto perm = shuffle_detail::generatePermutation(3u, 3u, rng);
  REQUIRE(perm == std::vector<size_t>({0u, 1u, 2u}));
}

