#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "number.h"
#include "DecimalConstants.h"
#include "Security.h"
#include "TimeSeries.h"
#include "TimeSeriesEntry.h"
#include "TimeSeriesCsvReader.h"
#include "SyntheticTimeSeries.h"
#include "SyntheticCache.h"        // <-- the new header you added
#include "TestUtils.h"            // for createEquityEntry(...)

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;

namespace
{

  // Helper: Create minimal daily series (1 entry)
  std::shared_ptr<OHLCTimeSeries<DecimalType>> makeSingleEntryDailySeries() {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    ts->addEntry(*createEquityEntry("20220103", "100.00", "101.00", "99.50", "100.40", 1000000));
    return ts;
  }

  // Helper: Create two-entry daily series
  std::shared_ptr<OHLCTimeSeries<DecimalType>> makeTwoEntryDailySeries() {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    ts->addEntry(*createEquityEntry("20220103", "100.00", "101.00", "99.50", "100.40", 1000000));
    ts->addEntry(*createEquityEntry("20220104", "101.00", "102.00", "100.50", "101.40", 1100000));
    return ts;
  }

  // Helper: Create series with identical bars
  std::shared_ptr<OHLCTimeSeries<DecimalType>> makeIdenticalBarsSeries() {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    const char* dates[] = {"20220103", "20220104", "20220105", "20220106"};
    for (int i = 0; i < 4; ++i) {
      ts->addEntry(*createEquityEntry(dates[i], "100.00", "101.00", "99.50", "100.40", 1000000));
    }
    return ts;
  }
  
  std::shared_ptr<OHLCTimeSeries<DecimalType>> makeDailySeries() {
    auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    // 2022-01-03 .. 2022-01-12 (8 trading days)
    const char* d[] = {"20220103","20220104","20220105","20220106",
		       "20220107","20220110","20220111","20220112"};
    DecimalType o = DecimalType("100.00");
    for (int i = 0; i < 8; ++i) {
      auto open  = o + DecimalType(i);               // 100,101,102,...
      auto high  = open + DecimalType("1.00");
      auto low   = open - DecimalType("0.50");
      auto close = open + DecimalType("0.40");
      auto vol   = 1000000 + i * 10000;
      ts->addEntry(*createEquityEntry(d[i], num::toString(open), num::toString(high),
				      num::toString(low), num::toString(close),
				      vol));
    }
    return ts;
  }
  
  // Try to load an intraday file used elsewhere in your tests
  std::shared_ptr<OHLCTimeSeries<DecimalType>> loadIntraday(const std::string& file) {
    TradeStationFormatCsvReader<DecimalType> reader(
						    file, TimeFrame::INTRADAY, TradingVolume::SHARES,
						    DecimalConstants<DecimalType>::EquityTick);
    reader.readFile();
    return reader.getTimeSeries();
  }
} // namespace

// ---- DAILY (EOD) ------------------------------------------------------------

TEST_CASE("SyntheticCache: EOD impl is chosen and Security is reused", "[SyntheticCache][EOD]") {
  const DecimalType tick     = DecimalConstants<DecimalType>::EquityTick;
  const DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;

  auto baseSeries = makeDailySeries();
  REQUIRE(baseSeries);
  REQUIRE(baseSeries->getTimeFrame() == TimeFrame::DAILY);

  // Build a base Security and a cache from it
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("MSFT", "Test Security", baseSeries);
  REQUIRE(baseSec);

  // NOTE: Adjust template args if your project uses a specific Lookup/Rounding policy type.
  using CacheT = SyntheticCache<DecimalType, /*LookupPolicy*/ LogNLookupPolicy<DecimalType>, /*RoundingPolicy*/ NoRounding>;
  CacheT cache(baseSec);

  RandomMersenne rng;
  rng.seed_u64(0xC0FFEEu);

  // First permutation
  auto& sec1 = cache.shuffleAndRebuild(rng);
  REQUIRE(sec1);
  auto ts1 = sec1->getTimeSeries();
  REQUIRE(ts1);
  REQUIRE(ts1->getTimeFrame() == TimeFrame::DAILY);
  REQUIRE(ts1->getNumEntries() == baseSeries->getNumEntries());

  // Second permutation (same Security object, different series pointer/content expected)
  auto sec1_addr = sec1.get();
  auto& sec2 = cache.shuffleAndRebuild(rng);
  REQUIRE(sec2.get() == sec1_addr); // same Security instance reused
  auto ts2 = sec2->getTimeSeries();
  REQUIRE(ts2);
  REQUIRE(ts2 != ts1);              // series pointer swapped
  REQUIRE(ts2->getTimeFrame() == TimeFrame::DAILY);
  REQUIRE(ts2->getNumEntries() == baseSeries->getNumEntries());

  // Basic content sanity: very likely the bars differ after a second shuffle
  // (Allow equality in the rare case RNG happened to produce same order.)
  bool anyDiff = false;
  auto it1 = ts1->beginSortedAccess(), it2 = ts2->beginSortedAccess();
  for (; it1 != ts1->endSortedAccess() && it2 != ts2->endSortedAccess(); ++it1, ++it2) {
    if (it1->getOpenValue()  != it2->getOpenValue() ||
        it1->getHighValue()  != it2->getHighValue() ||
        it1->getLowValue()   != it2->getLowValue()  ||
        it1->getCloseValue() != it2->getCloseValue()) {
      anyDiff = true; break;
    }
  }
  REQUIRE(anyDiff);
}

// ---- INTRADAY ---------------------------------------------------------------

TEST_CASE("SyntheticCache: Intraday impl is chosen and invariants are preserved", "[SyntheticCache][Intraday]") {
  // If this file is missing on a particular dev box, skip gracefully like your existing tests do.
  std::shared_ptr<OHLCTimeSeries<DecimalType>> baseSeriesPtr;
  try {
    baseSeriesPtr = loadIntraday("SSO_Hourly.txt");
  } catch (...) {
    WARN("SSO_Hourly.txt missing/unreadable; skipping SyntheticCache intraday test.");
    return;
  }
  REQUIRE(baseSeriesPtr);
  const auto& baseSeries = *baseSeriesPtr;
  REQUIRE(baseSeries.getNumEntries() > 0);
  REQUIRE(baseSeries.getTimeFrame() == TimeFrame::INTRADAY);

  const DecimalType tick     = DecimalConstants<DecimalType>::EquityTick;
  const DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;

  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Test Intraday", baseSeriesPtr);
  REQUIRE(baseSec);

  using CacheT = SyntheticCache<DecimalType, /*LookupPolicy*/ LogNLookupPolicy<DecimalType>, /*RoundingPolicy*/ NoRounding>;
  CacheT cache(baseSec);

  RandomMersenne rng;
  rng.seed_u64(0xBEEFCAFEu);

  auto& sec1 = cache.shuffleAndRebuild(rng);
  REQUIRE(sec1);
  auto ts1 = sec1->getTimeSeries();
  REQUIRE(ts1);
  REQUIRE(ts1->getTimeFrame() == TimeFrame::INTRADAY);
  REQUIRE(ts1->getNumEntries() == baseSeries.getNumEntries());
  REQUIRE(ts1->getFirstDateTime() == baseSeries.getFirstDateTime());
  REQUIRE(ts1->getLastDateTime()  == baseSeries.getLastDateTime());

  // Another build to ensure swapping works and object is reused
  auto sec_addr = sec1.get();
  auto& sec2 = cache.shuffleAndRebuild(rng);
  REQUIRE(sec2.get() == sec_addr);
  auto ts2 = sec2->getTimeSeries();
  REQUIRE(ts2);
  REQUIRE(ts2 != ts1);
  REQUIRE(ts2->getTimeFrame() == TimeFrame::INTRADAY);
  REQUIRE(ts2->getNumEntries() == baseSeries.getNumEntries());

  // Like your existing tests: interior bars (post basis day) should typically differ after shuffling
  bool interiorChanged = false;
  if (baseSeries.getNumEntries() == ts2->getNumEntries()) {
    auto origIt = baseSeries.beginSortedAccess();
    auto synIt  = ts2->beginSortedAccess();
    ptime basisDayEnd;
    if (origIt != baseSeries.endSortedAccess()) {
      const auto firstDay = origIt->getDateTime().date();
      auto tmp = origIt;
      while (tmp != baseSeries.endSortedAccess() && tmp->getDateTime().date() == firstDay) {
        basisDayEnd = tmp->getDateTime();
        ++tmp;
      }
    }
    for (; origIt != baseSeries.endSortedAccess() && synIt != ts2->endSortedAccess(); ++origIt, ++synIt) {
      if (origIt->getDateTime() > basisDayEnd) {
        if (origIt->getOpenValue()  != synIt->getOpenValue() ||
            origIt->getHighValue()  != synIt->getHighValue() ||
            origIt->getLowValue()   != synIt->getLowValue()  ||
            origIt->getCloseValue() != synIt->getCloseValue()) {
          interiorChanged = true;
          break;
        }
      }
    }
  }
  // If there’s only one day, no interior exists; otherwise we expect change.
  if (baseSeries.getFirstDate() != baseSeries.getLastDate()) {
    REQUIRE(interiorChanged);
  }
}

TEST_CASE("SyntheticCache: EOD N0_PairedDay preserves day-units (gap + H/O/L/O/C/O) up to permutation",
          "[SyntheticCache][EOD][N0]") {
  using DT = DecimalType;

  // Helper to extract per-day tuples (gap_t, H/O, L/O, C/O) from an EOD series
  auto day_factors = [](const OHLCTimeSeries<DT>& ts) {
    std::vector<std::tuple<DT,DT,DT,DT>> v;
    v.reserve(ts.getNumEntries() ? ts.getNumEntries()-1 : 0);

    bool first = true;
    DT prevClose{};
    for (auto it = ts.beginSortedAccess(); it != ts.endSortedAccess(); ++it) {
      const auto& e = *it;
      if (first) {
        first = false;
        prevClose = e.getCloseValue();
        continue; // no defined gap for first bar
      }
      const DT O = e.getOpenValue();
      const DT H = e.getHighValue();
      const DT L = e.getLowValue();
      const DT C = e.getCloseValue();
      const DT gap = O / prevClose;
      v.emplace_back(gap, H / O, L / O, C / O);
      prevClose = C;
    }
    return v;
  };

  auto as_multiset = [](std::vector<std::tuple<DT,DT,DT,DT>> v) {
    std::sort(v.begin(), v.end());
    return v;
  };

  const DT tick     = DecimalConstants<DT>::EquityTick;
  const DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  // Use local daily series helper already present in this file
  auto baseSeries = makeDailySeries();
  REQUIRE(baseSeries);
  REQUIRE(baseSeries->getTimeFrame() == TimeFrame::DAILY);

  // Original per-day tuples
  const auto orig_ms = as_multiset(day_factors(*baseSeries));

  // Build base Security and N0 cache (paired-day)
  auto baseSec = std::make_shared<EquitySecurity<DT>>("N0SYM", "N0 Test Security", baseSeries);
  using CacheN0 =
    SyntheticCache<
      DT,
      LogNLookupPolicy<DT>,
      NoRounding,
      SyntheticNullModel::N0_PairedDay
    >;

  CacheN0 cacheN0(baseSec);

  RandomMersenne rng;
  rng.seed_u64(0xA11CEu);

  // First permutation/build
  auto& sec1 = cacheN0.shuffleAndRebuild(rng);
  auto ts1 = sec1->getTimeSeries();
  REQUIRE(ts1);
  REQUIRE(ts1->getNumEntries() == baseSeries->getNumEntries());
  REQUIRE(ts1->getTimeFrame() == TimeFrame::DAILY);
  REQUIRE(ts1->getFirstDate() == baseSeries->getFirstDate());
  REQUIRE(ts1->getLastDate()  == baseSeries->getLastDate());

  // Under N0, the multiset of (gap, H/O, L/O, C/O) across days should be identical to original
  const auto ms1 = as_multiset(day_factors(*ts1));
  REQUIRE(ms1 == orig_ms);

  // Bar-level sanity: OHLC invariants hold
  for (auto it = ts1->beginSortedAccess(); it != ts1->endSortedAccess(); ++it) {
    const auto& b = *it;
    REQUIRE(b.getHighValue() >= std::max(b.getOpenValue(), b.getCloseValue()));
    REQUIRE(b.getLowValue()  <= std::min(b.getOpenValue(),  b.getCloseValue()));
  }
}

TEST_CASE("SyntheticCache: EOD N0_PairedDay reuses Security and swaps series pointer per shuffle",
          "[SyntheticCache][EOD][N0][Reuse]") {
  using DT = DecimalType;

  const DT tick     = DecimalConstants<DT>::EquityTick;
  const DT tickDiv2 = tick / DecimalConstants<DT>::DecimalTwo;

  auto baseSeries = makeDailySeries();
  auto baseSec    = std::make_shared<EquitySecurity<DT>>("N0SYM2", "N0 Reuse Test", baseSeries);

  using CacheN0 =
    SyntheticCache<
      DT,
      LogNLookupPolicy<DT>,
      NoRounding,
      SyntheticNullModel::N0_PairedDay
    >;

  CacheN0 cache(baseSec);

  RandomMersenne rng;
  rng.seed_u64(0xBADA55u);

  auto& sec1 = cache.shuffleAndRebuild(rng);
  REQUIRE(sec1);
  auto ts1 = sec1->getTimeSeries();
  REQUIRE(ts1);

  // Second build: same Security*, different series instance expected
  auto* sec_addr = sec1.get();
  auto& sec2 = cache.shuffleAndRebuild(rng);
  REQUIRE(sec2.get() == sec_addr);          // same Security reused
  auto ts2 = sec2->getTimeSeries();
  REQUIRE(ts2);
  REQUIRE(ts2 != ts1);                      // swapped series pointer

  // Both builds keep sizes & timeframe
  REQUIRE(ts2->getNumEntries() == baseSeries->getNumEntries());
  REQUIRE(ts2->getTimeFrame()  == TimeFrame::DAILY);
}

// ============================================================================
// EDGE CASES & ERROR CONDITIONS
// ============================================================================

TEST_CASE("SyntheticCache: Single-entry series doesn't crash", "[SyntheticCache][EdgeCase]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeSingleEntryDailySeries();
  REQUIRE(baseSeries->getNumEntries() == 1);
  
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("SINGLE", "Single Entry", baseSeries);
  CacheT cache(baseSec);
  
  RandomMersenne rng;
  rng.seed_u64(0x123456);
  
  // Should work without crash - no shuffling possible but should return valid series
  auto& sec = cache.shuffleAndRebuild(rng);
  REQUIRE(sec);
  auto ts = sec->getTimeSeries();
  REQUIRE(ts);
  REQUIRE(ts->getNumEntries() == 1);
  
  // Values should be unchanged (only one entry)
  auto it = ts->beginSortedAccess();
  auto origIt = baseSeries->beginSortedAccess();
  REQUIRE(it->getOpenValue() == origIt->getOpenValue());
  REQUIRE(it->getHighValue() == origIt->getHighValue());
  REQUIRE(it->getLowValue() == origIt->getLowValue());
  REQUIRE(it->getCloseValue() == origIt->getCloseValue());
}

TEST_CASE("SyntheticCache: Two-entry series edge case", "[SyntheticCache][EdgeCase]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeTwoEntryDailySeries();
  REQUIRE(baseSeries->getNumEntries() == 2);
  
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("TWO", "Two Entries", baseSeries);
  CacheT cache(baseSec);
  
  RandomMersenne rng;
  rng.seed_u64(0x789ABC);
  
  // With 2 entries, shuffle should work (first entry anchored, second can't move)
  auto& sec = cache.shuffleAndRebuild(rng);
  REQUIRE(sec);
  auto ts = sec->getTimeSeries();
  REQUIRE(ts);
  REQUIRE(ts->getNumEntries() == 2);
  REQUIRE(ts->getTimeFrame() == TimeFrame::DAILY);
}

TEST_CASE("SyntheticCache: Series with identical bars produces valid output", "[SyntheticCache][EdgeCase]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeIdenticalBarsSeries();
  REQUIRE(baseSeries->getNumEntries() == 4);
  
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("IDENT", "Identical Bars", baseSeries);
  CacheT cache(baseSec);
  
  RandomMersenne rng;
  rng.seed_u64(0xDEADBEEF);
  
  auto& sec = cache.shuffleAndRebuild(rng);
  REQUIRE(sec);
  auto ts = sec->getTimeSeries();
  REQUIRE(ts);
  REQUIRE(ts->getNumEntries() == 4);
  
  // Even though bars are identical, dates should be preserved
  REQUIRE(ts->getFirstDate() == baseSeries->getFirstDate());
  REQUIRE(ts->getLastDate() == baseSeries->getLastDate());
  
  // All bars should maintain OHLC invariants
  for (auto it = ts->beginSortedAccess(); it != ts->endSortedAccess(); ++it) {
    REQUIRE(it->getHighValue() >= it->getOpenValue());
    REQUIRE(it->getHighValue() >= it->getCloseValue());
    REQUIRE(it->getLowValue() <= it->getOpenValue());
    REQUIRE(it->getLowValue() <= it->getCloseValue());
  }
}

// ============================================================================
// RESETFROMBASE FUNCTIONALITY
// ============================================================================

TEST_CASE("SyntheticCache: resetFromBase with same timeframe", "[SyntheticCache][Reset]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto series1 = makeDailySeries();
  auto sec1 = std::make_shared<EquitySecurity<DecimalType>>("SYM1", "Series 1", series1);
  
  CacheT cache(sec1);
  RandomMersenne rng;
  rng.seed_u64(0x111);
  
  // Generate synthetic from first base
  auto& result1 = cache.shuffleAndRebuild(rng);
  auto ts1 = result1->getTimeSeries();
  REQUIRE(ts1->getNumEntries() == 8);
  
  // Create different base series (more entries)
  auto series2 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  const char* d[] = {"20220103","20220104","20220105","20220106","20220107"};
  for (int i = 0; i < 5; ++i) {
    DecimalType open = DecimalType("200.00") + DecimalType(i * 2);
    auto high  = open + DecimalType("2.00");
    auto low   = open - DecimalType("1.00");
    auto close = open + DecimalType("0.80");
    series2->addEntry(*createEquityEntry(d[i], num::toString(open), num::toString(high),
                                        num::toString(low), num::toString(close), 2000000));
  }
  auto sec2 = std::make_shared<EquitySecurity<DecimalType>>("SYM2", "Series 2", series2);
  
  // Reset to new base
  cache.resetFromBase(sec2);
  
  // Generate from new base
  auto& result2 = cache.shuffleAndRebuild(rng);
  auto ts2 = result2->getTimeSeries();
  REQUIRE(ts2->getNumEntries() == 5);  // New base has 5 entries
  REQUIRE(ts2->getTimeFrame() == TimeFrame::DAILY);
}

// ============================================================================
// RNG STATE & REPRODUCIBILITY
// ============================================================================

TEST_CASE("SyntheticCache: Same RNG seed produces deterministic results", "[SyntheticCache][RNG]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeDailySeries();
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("RNG1", "RNG Test", baseSeries);
  
  // Create two independent caches with identical base
  CacheT cache1(baseSec);
  CacheT cache2(baseSec);
  
  // Seed both RNGs identically
  RandomMersenne rng1, rng2;
  rng1.seed_u64(0xC0FFEE);
  rng2.seed_u64(0xC0FFEE);
  
  // Generate synthetics
  auto& sec1 = cache1.shuffleAndRebuild(rng1);
  auto& sec2 = cache2.shuffleAndRebuild(rng2);
  
  auto ts1 = sec1->getTimeSeries();
  auto ts2 = sec2->getTimeSeries();
  
  REQUIRE(ts1->getNumEntries() == ts2->getNumEntries());
  
  // Results should be identical
  auto it1 = ts1->beginSortedAccess();
  auto it2 = ts2->beginSortedAccess();
  for (; it1 != ts1->endSortedAccess(); ++it1, ++it2) {
    REQUIRE(it1->getOpenValue() == it2->getOpenValue());
    REQUIRE(it1->getHighValue() == it2->getHighValue());
    REQUIRE(it1->getLowValue() == it2->getLowValue());
    REQUIRE(it1->getCloseValue() == it2->getCloseValue());
    REQUIRE(it1->getDateTime() == it2->getDateTime());
  }
}

TEST_CASE("SyntheticCache: Different RNG seeds produce different results", "[SyntheticCache][RNG]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeDailySeries();
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("RNG2", "RNG Test 2", baseSeries);
  
  CacheT cache1(baseSec);
  CacheT cache2(baseSec);
  
  // Different seeds
  RandomMersenne rng1, rng2;
  rng1.seed_u64(0xAAAA);
  rng2.seed_u64(0xBBBB);
  
  auto& sec1 = cache1.shuffleAndRebuild(rng1);
  auto& sec2 = cache2.shuffleAndRebuild(rng2);
  
  auto ts1 = sec1->getTimeSeries();
  auto ts2 = sec2->getTimeSeries();
  
  // Find at least one difference in OHLC values
  bool foundDiff = false;
  auto it1 = ts1->beginSortedAccess();
  auto it2 = ts2->beginSortedAccess();
  for (; it1 != ts1->endSortedAccess(); ++it1, ++it2) {
    if (it1->getOpenValue() != it2->getOpenValue() ||
        it1->getHighValue() != it2->getHighValue() ||
        it1->getLowValue() != it2->getLowValue() ||
        it1->getCloseValue() != it2->getCloseValue()) {
      foundDiff = true;
      break;
    }
  }
  
  REQUIRE(foundDiff);  // Should find differences with different seeds
}

// ============================================================================
// N1 vs N0 BEHAVIORAL DIFFERENCES
// ============================================================================

TEST_CASE("SyntheticCache: N1 model changes day-unit structure (gap/H/L/C not preserved as unit)", 
          "[SyntheticCache][N1]") {
  using DT = DecimalType;
  using CacheN1 = SyntheticCache<DT, LogNLookupPolicy<DT>, NoRounding, SyntheticNullModel::N1_MaxDestruction>;
  
  // Helper to extract (gap, H/O, L/O, C/O) tuples
  auto day_factors = [](const OHLCTimeSeries<DT>& ts) {
    std::vector<std::tuple<DT,DT,DT,DT>> v;
    bool first = true;
    DT prevClose{};
    for (auto it = ts.beginSortedAccess(); it != ts.endSortedAccess(); ++it) {
      if (first) { first = false; prevClose = it->getCloseValue(); continue; }
      DT O = it->getOpenValue(), H = it->getHighValue();
      DT L = it->getLowValue(), C = it->getCloseValue();
      v.emplace_back(O / prevClose, H / O, L / O, C / O);
      prevClose = C;
    }
    return v;
  };
  
  auto baseSeries = makeDailySeries();
  auto baseSec = std::make_shared<EquitySecurity<DT>>("N1TEST", "N1 Test", baseSeries);
  
  auto orig_factors = day_factors(*baseSeries);
  
  CacheN1 cacheN1(baseSec);
  RandomMersenne rng;
  rng.seed_u64(0xFEEDFACE);
  
  // N1: independent shuffles - day structure should NOT be preserved
  auto& sec = cacheN1.shuffleAndRebuild(rng);
  auto ts = sec->getTimeSeries();
  auto syn_factors = day_factors(*ts);
  
  REQUIRE(syn_factors.size() == orig_factors.size());
  
  // The sets should differ (N1 destroys day-unit structure)
  // We can't guarantee they'll ALWAYS differ due to random chance, but with 7 day-tuples
  // the probability of identical order is extremely low (~1/5040)
  auto sorted_orig = orig_factors; std::sort(sorted_orig.begin(), sorted_orig.end());
  auto sorted_syn  = syn_factors;  std::sort(sorted_syn.begin(), sorted_syn.end());
  
  // The *values* might overlap (elements of one permutation), but for N1 
  // the specific day-tuples are independently reassembled, so we expect 
  // the vector of tuples to differ from original
  bool tuples_differ = (orig_factors != syn_factors);
  REQUIRE(tuples_differ);
}

TEST_CASE("SyntheticCache: N0 vs N1 produce different permutation characteristics", 
          "[SyntheticCache][N0][N1][Comparison]") {
  using DT = DecimalType;
  using CacheN0 = SyntheticCache<DT, LogNLookupPolicy<DT>, NoRounding, SyntheticNullModel::N0_PairedDay>;
  using CacheN1 = SyntheticCache<DT, LogNLookupPolicy<DT>, NoRounding, SyntheticNullModel::N1_MaxDestruction>;
  
  auto baseSeries = makeDailySeries();
  auto baseSecN0 = std::make_shared<EquitySecurity<DT>>("N0", "N0 Test", baseSeries);
  auto baseSecN1 = std::make_shared<EquitySecurity<DT>>("N1", "N1 Test", baseSeries);
  
  CacheN0 cacheN0(baseSecN0);
  CacheN1 cacheN1(baseSecN1);
  
  RandomMersenne rng0, rng1;
  rng0.seed_u64(0x12345);
  rng1.seed_u64(0x12345);  // Same seed to isolate model differences
  
  auto& secN0 = cacheN0.shuffleAndRebuild(rng0);
  auto& secN1 = cacheN1.shuffleAndRebuild(rng1);
  
  auto tsN0 = secN0->getTimeSeries();
  auto tsN1 = secN1->getTimeSeries();
  
  REQUIRE(tsN0->getNumEntries() == tsN1->getNumEntries());
  
  // The outputs will differ because N0 and N1 use different shuffling strategies
  // Even with same RNG seed, the shuffle algorithms themselves are different
  bool anyDiff = false;
  auto it0 = tsN0->beginSortedAccess();
  auto it1 = tsN1->beginSortedAccess();
  for (; it0 != tsN0->endSortedAccess(); ++it0, ++it1) {
    if (it0->getOpenValue() != it1->getOpenValue()) {
      anyDiff = true;
      break;
    }
  }
  
  REQUIRE(anyDiff);  // N0 and N1 should produce different results
}

// ============================================================================
// MULTI-PERMUTATION STABILITY
// ============================================================================

TEST_CASE("SyntheticCache: Many consecutive shuffles maintain stability", "[SyntheticCache][Stability]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeDailySeries();
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("STABLE", "Stability Test", baseSeries);
  
  CacheT cache(baseSec);
  RandomMersenne rng;
  rng.seed_u64(0x999999);
  
  const size_t numIterations = 100;
  
  for (size_t i = 0; i < numIterations; ++i) {
    auto& sec = cache.shuffleAndRebuild(rng);
    auto ts = sec->getTimeSeries();
    
    // Verify invariants hold across many shuffles
    REQUIRE(ts);
    REQUIRE(ts->getNumEntries() == baseSeries->getNumEntries());
    REQUIRE(ts->getTimeFrame() == TimeFrame::DAILY);
    
    // OHLC invariants
    for (auto it = ts->beginSortedAccess(); it != ts->endSortedAccess(); ++it) {
      REQUIRE(it->getHighValue() >= std::max(it->getOpenValue(), it->getCloseValue()));
      REQUIRE(it->getLowValue() <= std::min(it->getOpenValue(), it->getCloseValue()));
    }
  }
  
  // If we get here without crashes or assertion failures, stability is confirmed
  REQUIRE(true);
}

// ============================================================================
// SECURITY OBJECT PROPERTIES
// ============================================================================

TEST_CASE("SyntheticCache: Security symbol and name preserved", "[SyntheticCache][Security]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeDailySeries();
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("TESTSYM", "Test Security Name", baseSeries);
  
  CacheT cache(baseSec);
  RandomMersenne rng;
  rng.seed_u64(0x42);
  
  auto& sec = cache.shuffleAndRebuild(rng);
  
  // Symbol and name should be preserved from clone
  REQUIRE(sec->getSymbol() == "TESTSYM");
  REQUIRE(sec->getName() == "Test Security Name");
}

TEST_CASE("SyntheticCache: Tick parameters preserved", "[SyntheticCache][Security]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeDailySeries();
  const DecimalType customTick("0.05");
  const DecimalType customTickDiv2 = customTick / DecimalConstants<DecimalType>::DecimalTwo;
  
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("TICK", "Tick Test", baseSeries);
  // Note: This assumes Security has a way to set custom tick; adjust if needed
  
  CacheT cache(baseSec);
  RandomMersenne rng;
  rng.seed_u64(0x88);
  
  auto& sec = cache.shuffleAndRebuild(rng);
  
  // Tick values should match base security
  REQUIRE(sec->getTick() == baseSec->getTick());
  REQUIRE(sec->getTickDiv2() == baseSec->getTickDiv2());
}

// ============================================================================
// CONST CORRECTNESS & ACCESSOR TESTS
// ============================================================================

TEST_CASE("SyntheticCache: security() accessor returns valid reference", "[SyntheticCache][Accessor]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeDailySeries();
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("ACC", "Accessor Test", baseSeries);
  
  CacheT cache(baseSec);
  
  // Before any shuffle
  const auto& secBefore = cache.security();
  REQUIRE(secBefore);
  REQUIRE(secBefore->getSymbol() == "ACC");
  
  RandomMersenne rng;
  rng.seed_u64(0x77);
  
  // After shuffle
  cache.shuffleAndRebuild(rng);
  const auto& secAfter = cache.security();
  REQUIRE(secAfter);
  REQUIRE(secAfter.get() == secBefore.get());  // Same object
}

TEST_CASE("SyntheticCache: Multiple shuffles verify Security pointer stability", "[SyntheticCache][Stability]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  auto baseSeries = makeDailySeries();
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("PTR", "Pointer Test", baseSeries);
  
  CacheT cache(baseSec);
  RandomMersenne rng;
  rng.seed_u64(0x66);
  
  auto& sec1 = cache.shuffleAndRebuild(rng);
  void* addr1 = sec1.get();
  
  auto& sec2 = cache.shuffleAndRebuild(rng);
  void* addr2 = sec2.get();
  
  auto& sec3 = cache.shuffleAndRebuild(rng);
  void* addr3 = sec3.get();
  
  // All should point to same Security object
  REQUIRE(addr1 == addr2);
  REQUIRE(addr2 == addr3);
  
  // But series pointers should differ
  auto ts1 = sec1->getTimeSeries();
  auto ts2 = sec2->getTimeSeries();  
  auto ts3 = sec3->getTimeSeries();
  
  // Note: After sec2 is created, sec1's series is replaced, so comparing 
  // sec1->getTimeSeries() at this point gets the latest. Instead verify
  // that after each shuffle, a new series is installed.
  REQUIRE(ts3);  // Just verify latest is valid
}

TEST_CASE("SyntheticCache: Weekly timeframe uses EOD implementation", 
          "[SyntheticCache][Weekly]") {
  using CacheT = SyntheticCache<DecimalType, LogNLookupPolicy<DecimalType>, NoRounding>;
  
  // Create weekly series
  auto ts = std::make_shared<OHLCTimeSeries<DecimalType>>(
      TimeFrame::WEEKLY, TradingVolume::SHARES);
  
  const char* dates[] = {"20220103", "20220110", "20220117", "20220124"};
  for (int i = 0; i < 4; ++i) {
    DecimalType open = DecimalType("100.00") + DecimalType(i * 5);
    auto high  = open + DecimalType("2.00");
    auto low   = open - DecimalType("1.00");
    auto close = open + DecimalType("1.50");
    ts->addEntry(*createEquityEntry(dates[i], num::toString(open),
                                    num::toString(high), num::toString(low),
                                    num::toString(close), 1000000, TimeFrame::WEEKLY));
  }
  
  auto baseSec = std::make_shared<EquitySecurity<DecimalType>>("WK", "Weekly", ts);
  
  // Create cache - should not throw (verifies weekly uses EOD impl)
  CacheT cache(baseSec);
  
  RandomMersenne rng;
  rng.seed_u64(0x123);
  
  auto& sec = cache.shuffleAndRebuild(rng);
  auto synTs = sec->getTimeSeries();
  
  REQUIRE(synTs);
  REQUIRE(synTs->getTimeFrame() == TimeFrame::WEEKLY);
  REQUIRE(synTs->getNumEntries() == 4);
  
  // Verify OHLC invariants hold for weekly bars
  for (auto it = synTs->beginSortedAccess(); it != synTs->endSortedAccess(); ++it) {
    REQUIRE(it->getHighValue() >= std::max(it->getOpenValue(), it->getCloseValue()));
    REQUIRE(it->getLowValue() <= std::min(it->getOpenValue(), it->getCloseValue()));
  }
}
