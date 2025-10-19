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

namespace {

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
