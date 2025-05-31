#include <catch2/catch_test_macros.hpp>
#include "number.h"
#include "TimeSeries.h"
#include "SyntheticTimeSeries.h"
#include "TimeSeriesCsvReader.h"
#include "TimeSeriesCsvWriter.h"
#include "DecimalConstants.h"
#include "TestUtils.h" // Assumed to contain createEquityEntry, getRandomPriceSeries

#include <vector>
#include <map>
#include <algorithm> // For std::sort, std::equal, std::shuffle
#include <numeric>   // For std::iota
#include <tuple>     // For std::make_tuple in helpers
#include <iostream>  // For std::cout (though most debug output will be removed)
#include <string>    // For std::string in tolerant comparison
#include <iomanip>   // For std::fixed, std::setprecision
#include <sstream>   // For std::ostringstream
#include <random>    // For std::default_random_engine with std::shuffle

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;

// Anonymous namespace for existing and new helpers
namespace
{
    
    

  // Helper function to create a 1-day intraday series
  OHLCTimeSeries<DecimalType> createOneDayIntradaySampleTimeSeries() {
    OHLCTimeSeries<DecimalType> ts(TimeFrame::INTRADAY, TradingVolume::SHARES);
    // Day 1 entries
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2022, 1, 3), hours(9) + minutes(30)),
        DecimalType("100.0"), DecimalType("101.0"), DecimalType("99.5"), DecimalType("100.5"),
        DecimalType("1000"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2022, 1, 3), hours(10) + minutes(30)),
        DecimalType("100.5"), DecimalType("102.0"), DecimalType("100.0"), DecimalType("101.0"),
        DecimalType("1100"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2022, 1, 3), hours(11) + minutes(0)),
        DecimalType("101.0"), DecimalType("102.5"), DecimalType("100.8"), DecimalType("101.2"),
        DecimalType("1200"), TimeFrame::INTRADAY));
    return ts;
  }

  // Helper function to create a 3-day intraday series with distinct characteristics
  OHLCTimeSeries<DecimalType> createThreeDayIntradaySampleTimeSeries() {
    OHLCTimeSeries<DecimalType> ts(TimeFrame::INTRADAY, TradingVolume::SHARES);
    // Day 1 (Basis): 2023-01-02, 2 bars
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2023, 1, 2), hours(9) + minutes(30)),
        DecimalType("100.0"), DecimalType("101.0"), DecimalType("99.5"), DecimalType("100.5"),
        DecimalType("1000"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2023, 1, 2), hours(10) + minutes(30)),
        DecimalType("100.5"), DecimalType("102.0"), DecimalType("100.0"), DecimalType("101.0"),
        DecimalType("1100"), TimeFrame::INTRADAY));

    // Day 2 (Permutable): 2023-01-03, 3 bars (unique bar count)
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2023, 1, 3), hours(9) + minutes(0)),
        DecimalType("102.0"), DecimalType("103.0"), DecimalType("101.5"), DecimalType("102.5"),
        DecimalType("1200"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2023, 1, 3), hours(10) + minutes(0)),
        DecimalType("102.5"), DecimalType("104.0"), DecimalType("102.0"), DecimalType("103.0"),
        DecimalType("1300"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2023, 1, 3), hours(11) + minutes(0)),
        DecimalType("103.0"), DecimalType("105.0"), DecimalType("102.5"), DecimalType("104.5"),
        DecimalType("1400"), TimeFrame::INTRADAY));

    // Day 3 (Permutable): 2023-01-04, 2 bars (different structure/values than Day 1)
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2023, 1, 4), hours(9) + minutes(15)),
        DecimalType("105.0"), DecimalType("106.0"), DecimalType("104.5"), DecimalType("105.5"),
        DecimalType("1500"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(
        ptime(date(2023, 1, 4), hours(10) + minutes(45)),
        DecimalType("105.5"), DecimalType("107.0"), DecimalType("105.0"), DecimalType("106.5"),
        DecimalType("1600"), TimeFrame::INTRADAY));
    return ts;
  }

  // Helper to get all bars for a specific date from a series
  std::vector<OHLCTimeSeriesEntry<DecimalType>> getBarsForDate(const OHLCTimeSeries<DecimalType>& series, const boost::gregorian::date& d) {
    std::vector<OHLCTimeSeriesEntry<DecimalType>> bars;
    if (series.getNumEntries() == 0) return bars;

    for (auto it = series.beginSortedAccess(); it != series.endSortedAccess(); ++it) {
        if (it->getDateTime().date() == d) {
            bars.push_back(*it);
        }
    }
    return bars;
  }

  // Helper to normalize a vector of bars relative to the first bar's open of that vector
  std::vector<OHLCTimeSeriesEntry<DecimalType>> normalizeBars(const std::vector<OHLCTimeSeriesEntry<DecimalType>>& bars) {
    if (bars.empty()) return {};
    std::vector<OHLCTimeSeriesEntry<DecimalType>> normalized;
    normalized.reserve(bars.size());
    DecimalType openAnchor = bars.front().getOpenValue();
    DecimalType one = DecimalConstants<DecimalType>::DecimalOne;
    DecimalType zero = DecimalConstants<DecimalType>::DecimalZero;

    if (openAnchor == zero) {
        for (const auto& bar : bars) {
            normalized.emplace_back(bar.getDateTime(), one, one, one, one,
                                    bar.getVolumeValue(),
                                    bar.getTimeFrame());
        }
    } else {
        for (const auto& bar : bars) {
            normalized.emplace_back(
                bar.getDateTime(),
                bar.getOpenValue() / openAnchor,
                bar.getHighValue() / openAnchor,
                bar.getLowValue() / openAnchor,
                bar.getCloseValue() / openAnchor,
                bar.getVolumeValue(),
                bar.getTimeFrame()
            );
        }
    }
    return normalized;
  }

  auto extractFactors = [](const std::vector<OHLCTimeSeriesEntry<DecimalType>>& bars) {
        std::vector<std::tuple<DecimalType,DecimalType,DecimalType,DecimalType>> f;
        for (auto& bar : normalizeBars(bars)) {
            f.emplace_back(bar.getOpenValue(),
                            bar.getHighValue(),
            bar.getLowValue(),
            bar.getCloseValue()
        );
    }
    return f;
    };

  OHLCTimeSeries<DecimalType> createSampleTimeSeries()
  {
    OHLCTimeSeries<DecimalType> series(TimeFrame::DAILY, TradingVolume::SHARES);
    series.addEntry(*createEquityEntry("20070402", "43.08", "43.17", "42.71", "43.00", 89658785));
    series.addEntry(*createEquityEntry("20070403", "43.23", "43.72", "43.20", "43.57", 105925137));
    series.addEntry(*createEquityEntry("20070404", "43.61", "43.79", "43.54", "43.75", 85200468));
    series.addEntry(*createEquityEntry("20070405", "43.70", "43.99", "43.64", "43.97", 54260779));
    series.addEntry(*createEquityEntry("20070409", "44.12", "44.16", "43.79", "43.86", 63074749));
    series.addEntry(*createEquityEntry("20070410", "43.85", "44.09", "43.82", "44.09", 76458129));
    series.addEntry(*createEquityEntry("20070411", "44.05", "44.07", "43.48", "43.62", 118359304));
    series.addEntry(*createEquityEntry("20070412", "43.55", "44.03", "43.36", "43.97", 114852449));
    series.addEntry(*createEquityEntry("20070413", "43.98", "44.13", "43.70", "44.06", 94594604));
    series.addEntry(*createEquityEntry("20070416", "44.23", "44.56", "44.23", "44.47", 73028087));
    series.addEntry(*createEquityEntry("20070417", "44.55", "44.61", "44.37", "44.57", 81879736));
    series.addEntry(*createEquityEntry("20070418", "44.34", "44.64", "44.24", "44.42", 82051504));
    series.addEntry(*createEquityEntry("20070419", "44.22", "44.66", "44.13", "44.56", 95510366));
    series.addEntry(*createEquityEntry("20070420", "44.94", "45.08", "44.61", "44.81", 122441399));
    series.addEntry(*createEquityEntry("20070423", "44.85", "45.01", "44.75", "44.88", 85450450));
    series.addEntry(*createEquityEntry("20070424", "45.04", "45.24", "44.74", "45.11", 108196954));
    series.addEntry(*createEquityEntry("20070425", "45.26", "45.73", "45.11", "45.72", 106954392));
    series.addEntry(*createEquityEntry("20070426", "45.83", "46.06", "45.74", "45.96", 99409986));
    series.addEntry(*createEquityEntry("20070427", "45.78", "46.11", "45.70", "45.98", 96607259));
    series.addEntry(*createEquityEntry("20070430", "45.91", "45.94", "45.33", "45.37", 93556683));
    series.addEntry(*createEquityEntry("20070501", "45.38", "45.55", "45.07", "45.48", 135108913));
    series.addEntry(*createEquityEntry("20070502", "45.50", "45.99", "45.46", "45.83", 91995829));
    series.addEntry(*createEquityEntry("20070503", "45.94", "46.13", "45.84", "46.00", 98037633));
    series.addEntry(*createEquityEntry("20070504", "46.17", "46.30", "45.83", "46.04", 93643063));
    series.addEntry(*createEquityEntry("20070507", "46.06", "46.19", "45.98", "46.04", 47684367));
    series.addEntry(*createEquityEntry("20070508", "45.88", "46.18", "45.71", "46.14", 95197296));
    series.addEntry(*createEquityEntry("20070509", "45.90", "46.38", "45.87", "46.24", 116007860));
    series.addEntry(*createEquityEntry("20070510", "46.07", "46.19", "45.48", "45.60", 171264643));
    series.addEntry(*createEquityEntry("20070511", "45.65", "46.19", "45.59", "46.19", 103197326));
    series.addEntry(*createEquityEntry("20070514", "46.18", "46.29", "45.60", "45.87", 118966989));
    series.addEntry(*createEquityEntry("20070515", "45.82", "46.11", "45.40", "45.51", 179489134));
    series.addEntry(*createEquityEntry("20070516", "45.62", "45.97", "45.32", "45.96", 144722516));
    series.addEntry(*createEquityEntry("20070517", "45.91", "45.98", "45.70", "45.74", 110308018));
    series.addEntry(*createEquityEntry("20070518", "45.89", "46.12", "45.79", "46.12", 104992456));
    series.addEntry(*createEquityEntry("20070521", "46.15", "46.65", "46.08", "46.42", 112895185));
    series.addEntry(*createEquityEntry("20070522", "46.45", "46.69", "46.28", "46.46", 98134419));
    series.addEntry(*createEquityEntry("20070523", "46.60", "46.78", "46.23", "46.24", 119434425));
    series.addEntry(*createEquityEntry("20070524", "46.27", "46.45", "45.38", "45.57", 206344362));
    series.addEntry(*createEquityEntry("20070525", "45.67", "45.97", "45.59", "45.86", 87154203));
    series.addEntry(*createEquityEntry("20070529", "45.90", "46.27", "45.82", "46.22", 99722016));
    series.addEntry(*createEquityEntry("20070530", "45.89", "46.61", "45.76", "46.60", 134482055));
    series.addEntry(*createEquityEntry("20070531", "46.73", "46.93", "46.62", "46.82", 110776572));
    series.addEntry(*createEquityEntry("20070601", "46.99", "47.18", "46.78", "46.85", 125134274));
    series.addEntry(*createEquityEntry("20070604", "46.68", "47.05", "46.65", "46.99", 65389891));
    series.addEntry(*createEquityEntry("20070605", "46.84", "46.99", "46.41", "46.99", 151176309));
    series.addEntry(*createEquityEntry("20070606", "46.78", "46.79", "46.37", "46.48", 183211104));
    series.addEntry(*createEquityEntry("20070607", "46.32", "46.62", "45.73", "45.75", 221629091));
    series.addEntry(*createEquityEntry("20070608", "45.74", "46.34", "45.57", "46.32", 177619187));
    series.addEntry(*createEquityEntry("20070611", "46.30", "46.60", "46.17", "46.23", 97154490));
    series.addEntry(*createEquityEntry("20070612", "45.99", "46.45", "45.86", "45.95", 154107381));
    series.addEntry(*createEquityEntry("20070613", "46.15", "46.50", "45.95", "46.46", 148025252));
    series.addEntry(*createEquityEntry("20070614", "46.51", "46.90", "46.50", "46.77", 101553670));
    series.addEntry(*createEquityEntry("20070615", "47.17", "47.28", "47.06", "47.14", 100410121));
    series.addEntry(*createEquityEntry("20070618", "47.26", "47.28", "47.08", "47.18", 75767348));
    series.addEntry(*createEquityEntry("20070619", "47.04", "47.27", "46.92", "47.17", 109680620));
    series.addEntry(*createEquityEntry("20070620", "47.25", "47.33", "46.65", "46.72", 161940982));
    series.addEntry(*createEquityEntry("20070621", "46.71", "47.20", "46.49", "47.15", 143198074));
    series.addEntry(*createEquityEntry("20070622", "47.05", "47.12", "46.55", "46.70", 153419868));
    series.addEntry(*createEquityEntry("20070625", "46.72", "46.99", "46.23", "46.50", 136618872));
    series.addEntry(*createEquityEntry("20070626", "46.73", "46.73", "46.15", "46.23", 128218934));
    series.addEntry(*createEquityEntry("20070627", "46.17", "46.97", "46.11", "46.95", 141238628));
    series.addEntry(*createEquityEntry("20070628", "46.93", "47.26", "46.88", "46.93", 116596740));
    series.addEntry(*createEquityEntry("20070629", "47.19", "47.29", "46.62", "47.01", 124794950));
    series.addEntry(*createEquityEntry("20070702", "47.13", "47.46", "47.09", "47.42", 80926013));
    series.addEntry(*createEquityEntry("20070703", "47.51", "47.73", "47.45", "47.72", 34803235));
    series.addEntry(*createEquityEntry("20070705", "47.76", "48.18", "47.70", "48.07", 73588499));
    series.addEntry(*createEquityEntry("20070706", "48.12", "48.32", "47.91", "48.27", 65812737));
    series.addEntry(*createEquityEntry("20070709", "48.31", "48.39", "48.13", "48.30", 66774416));
    series.addEntry(*createEquityEntry("20070710", "48.10", "48.31", "47.84", "47.89", 104397346));
    series.addEntry(*createEquityEntry("20070711", "47.78", "48.22", "47.73", "48.22", 99637596));
    series.addEntry(*createEquityEntry("20070712", "48.37", "49.08", "48.33", "48.97", 107667141));
    series.addEntry(*createEquityEntry("20070713", "49.03", "49.38", "48.98", "49.31", 74236703));
    series.addEntry(*createEquityEntry("20070716", "49.31", "49.49", "49.17", "49.26", 77799493));
    series.addEntry(*createEquityEntry("20070717", "49.38", "49.71", "49.33", "49.64", 124838546));
    series.addEntry(*createEquityEntry("20070718", "49.36", "49.58", "49.05", "49.58", 144933918));
    series.addEntry(*createEquityEntry("20070719", "49.82", "50.07", "49.72", "49.73", 116564064));
    series.addEntry(*createEquityEntry("20070720", "49.71", "49.77", "49.15", "49.46", 163721225));
    series.addEntry(*createEquityEntry("20070723", "49.62", "49.72", "49.33", "49.48", 111770046));
    series.addEntry(*createEquityEntry("20070724", "49.12", "49.51", "48.46", "48.74", 175550463));
    series.addEntry(*createEquityEntry("20070725", "48.98", "49.08", "48.47", "48.81", 170916771));
    series.addEntry(*createEquityEntry("20070726", "48.48", "48.88", "47.46", "48.39", 318659986));
    series.addEntry(*createEquityEntry("20070727", "48.31", "48.53", "47.40", "47.40", 246388645));
    series.addEntry(*createEquityEntry("20070730", "47.62", "48.14", "47.36", "47.96", 166523135));
    series.addEntry(*createEquityEntry("20070731", "48.36", "48.40", "46.84", "46.94", 260431810));
    series.addEntry(*createEquityEntry("20070801", "46.82", "47.34", "46.39", "47.31", 301213044));
    series.addEntry(*createEquityEntry("20070802", "47.35", "47.84", "47.20", "47.75", 177795538));
    series.addEntry(*createEquityEntry("20070803", "47.66", "47.72", "46.56", "46.83", 166048886));
    series.addEntry(*createEquityEntry("20070806", "46.87", "47.43", "46.44", "47.38", 190980274));
    series.addEntry(*createEquityEntry("20070807", "47.30", "48.03", "46.89", "47.72", 167576241));
    series.addEntry(*createEquityEntry("20070808", "47.92", "48.47", "47.81", "48.25", 165283054));
    series.addEntry(*createEquityEntry("20070809", "47.61", "48.38", "47.10", "47.12", 242875056));
    series.addEntry(*createEquityEntry("20070810", "46.63", "47.24", "46.04", "46.69", 247014358));
    series.addEntry(*createEquityEntry("20070813", "47.08", "47.29", "46.89", "47.01", 116963532));
    series.addEntry(*createEquityEntry("20070814", "47.09", "47.15", "46.14", "46.20", 153548717));
    series.addEntry(*createEquityEntry("20070815", "46.09", "46.50", "45.22", "45.31", 214860325));
    series.addEntry(*createEquityEntry("20070816", "44.96", "45.32", "43.80", "44.86", 362480242));
    series.addEntry(*createEquityEntry("20070817", "45.69", "46.00", "45.00", "45.72", 223247137));
    series.addEntry(*createEquityEntry("20070820", "45.89", "46.13", "45.49", "45.94", 144715090));
    series.addEntry(*createEquityEntry("20070821", "45.88", "46.54", "45.80", "46.43", 115029555));
    series.addEntry(*createEquityEntry("20070822", "46.78", "47.11", "46.68", "47.07", 115846679));
    series.addEntry(*createEquityEntry("20070823", "47.17", "47.26", "46.64", "46.94", 118663363));
    series.addEntry(*createEquityEntry("20070824", "46.80", "47.65", "46.73", "47.61", 88192858));
    series.addEntry(*createEquityEntry("20070827", "47.47", "47.61", "47.25", "47.29", 72491406));
    series.addEntry(*createEquityEntry("20070828", "47.01", "47.11", "46.12", "46.15", 106024956));
    series.addEntry(*createEquityEntry("20070829", "46.47", "47.53", "46.46", "47.49", 113203904));
    series.addEntry(*createEquityEntry("20070830", "47.24", "48.15", "47.16", "47.74", 138930137));
    series.addEntry(*createEquityEntry("20070831", "48.27", "48.47", "48.02", "48.28", 94692123));
    series.addEntry(*createEquityEntry("20070904", "48.33", "49.41", "48.32", "49.09", 98481769));
    series.addEntry(*createEquityEntry("20070905", "48.96", "49.06", "48.28", "48.59", 114049417));
    series.addEntry(*createEquityEntry("20070906", "48.64", "48.77", "48.22", "48.55", 99224087));
    series.addEntry(*createEquityEntry("20070907", "47.93", "48.01", "47.36", "47.64", 152438076));
    series.addEntry(*createEquityEntry("20070910", "48.03", "48.16", "47.22", "47.61", 125209015));
    series.addEntry(*createEquityEntry("20070911", "47.92", "48.40", "47.84", "48.34", 103064420));
    series.addEntry(*createEquityEntry("20070912", "48.25", "48.78", "48.19", "48.35", 94012155));
    series.addEntry(*createEquityEntry("20070913", "48.71", "48.76", "48.35", "48.59", 80807499));
    series.addEntry(*createEquityEntry("20070914", "48.21", "48.72", "48.14", "48.63", 99801280));
    series.addEntry(*createEquityEntry("20070917", "48.40", "48.51", "48.00", "48.22", 84882267));
    series.addEntry(*createEquityEntry("20070918", "48.49", "49.49", "48.24", "49.45", 151455795));
    series.addEntry(*createEquityEntry("20070919", "49.70", "50.00", "49.39", "49.58", 127025724));
    series.addEntry(*createEquityEntry("20070920", "49.47", "49.67", "49.33", "49.44", 109970577));
    series.addEntry(*createEquityEntry("20070921", "49.70", "49.93", "49.43", "49.77", 86837267));
    series.addEntry(*createEquityEntry("20070924", "49.92", "50.37", "49.74", "50.00", 107226720));
    series.addEntry(*createEquityEntry("20070925", "49.83", "50.48", "49.77", "50.48", 101116343));
    series.addEntry(*createEquityEntry("20070926", "50.77", "50.92", "50.53", "50.73", 96124510));
    series.addEntry(*createEquityEntry("20070927", "51.04", "51.06", "50.77", "50.99", 73430670));
    series.addEntry(*createEquityEntry("20070928", "50.96", "51.09", "50.59", "50.82", 75549164));
    series.addEntry(*createEquityEntry("20071001", "50.86", "51.57", "50.79", "51.41", 100406595));
    series.addEntry(*createEquityEntry("20071002", "51.45", "51.47", "51.13", "51.42", 71045968));
    series.addEntry(*createEquityEntry("20071003", "51.24", "51.48", "50.91", "51.06", 106790484));
    series.addEntry(*createEquityEntry("20071004", "51.16", "51.24", "50.75", "51.18", 84129214));
    series.addEntry(*createEquityEntry("20071005", "51.58", "52.31", "51.47", "52.23", 115681518));
    series.addEntry(*createEquityEntry("20071008", "52.21", "52.57", "52.12", "52.56", 63022560));
    series.addEntry(*createEquityEntry("20071009", "52.68", "52.86", "52.44", "52.79", 94211279));
    series.addEntry(*createEquityEntry("20071010", "52.80", "52.98", "52.62", "52.92", 91777573));
    series.addEntry(*createEquityEntry("20071011", "53.20", "53.35", "51.69", "52.07", 239787723));
    series.addEntry(*createEquityEntry("20071012", "52.31", "52.95", "52.21", "52.94", 131675908));
    series.addEntry(*createEquityEntry("20071015", "53.02", "53.12", "52.11", "52.53", 121579489));
    series.addEntry(*createEquityEntry("20071016", "52.20", "52.69", "52.09", "52.28", 193676904));
    series.addEntry(*createEquityEntry("20071017", "53.03", "53.07", "52.09", "52.96", 183985325));
    series.addEntry(*createEquityEntry("20071018", "52.75", "53.30", "52.53", "53.19", 144502434));
    series.addEntry(*createEquityEntry("20071019", "53.18", "53.18", "51.80", "51.85", 245111936));
    series.addEntry(*createEquityEntry("20071022", "51.57", "52.53", "51.43", "52.48", 207190459));
    series.addEntry(*createEquityEntry("20071023", "53.02", "53.62", "52.75", "53.59", 162560572));
    series.addEntry(*createEquityEntry("20071024", "53.15", "53.35", "52.02", "53.18", 298346284));
    series.addEntry(*createEquityEntry("20071025", "53.29", "53.39", "52.16", "52.46", 233841285));
    series.addEntry(*createEquityEntry("20071026", "53.49", "53.61", "52.85", "53.34", 146690641));
    series.addEntry(*createEquityEntry("20071029", "53.60", "53.74", "53.25", "53.57", 101897569));
    series.addEntry(*createEquityEntry("20071030", "53.35", "53.97", "53.31", "53.67", 114616598));
    series.addEntry(*createEquityEntry("20071031", "53.88", "54.48", "53.45", "54.44", 148431932));
    series.addEntry(*createEquityEntry("20071101", "54.09", "54.18", "53.38", "53.41", 181974177));
    series.addEntry(*createEquityEntry("20071102", "53.83", "53.96", "53.01", "53.83", 217884414));
    series.addEntry(*createEquityEntry("20071105", "53.27", "53.81", "53.00", "53.48", 150588814));
    series.addEntry(*createEquityEntry("20071106", "53.74", "54.10", "53.19", "54.09", 131884651));
    series.addEntry(*createEquityEntry("20071107", "53.63", "53.99", "52.72", "52.76", 197910988));
    series.addEntry(*createEquityEntry("20071108", "52.55", "52.74", "50.21", "51.14", 380626329));
    series.addEntry(*createEquityEntry("20071109", "50.14", "50.53", "49.41", "49.41", 309419820));
    series.addEntry(*createEquityEntry("20071112", "49.33", "49.65", "48.06", "48.14", 283215926));
    series.addEntry(*createEquityEntry("20071113", "48.80", "50.20", "48.76", "50.15", 255626493));
    series.addEntry(*createEquityEntry("20071114", "50.80", "50.85", "49.31", "49.50", 263286876));
    series.addEntry(*createEquityEntry("20071115", "49.41", "49.95", "48.78", "49.23", 251550792));
    series.addEntry(*createEquityEntry("20071116", "49.55", "49.90", "48.75", "49.69", 263623649));
    series.addEntry(*createEquityEntry("20071119", "49.58", "49.86", "48.82", "49.11", 198651125));
    series.addEntry(*createEquityEntry("20071120", "49.36", "50.19", "48.34", "49.31", 300763582));
    series.addEntry(*createEquityEntry("20071121", "48.84", "49.58", "48.26", "48.72", 212724152));
    series.addEntry(*createEquityEntry("20071123", "49.04", "49.38", "48.78", "49.25", 46472533));
    series.addEntry(*createEquityEntry("20071126", "49.39", "49.78", "48.31", "48.39", 150352664));
    series.addEntry(*createEquityEntry("20071127", "48.80", "49.46", "48.51", "49.37", 211369293));
    series.addEntry(*createEquityEntry("20071128", "50.04", "51.11", "50.01", "50.89", 216872308));
    series.addEntry(*createEquityEntry("20071129", "50.68", "51.30", "50.61", "51.11", 172091667));
    series.addEntry(*createEquityEntry("20071130", "51.62", "51.66", "50.36", "50.72", 171417928));
    series.addEntry(*createEquityEntry("20071203", "50.55", "50.94", "50.23", "50.29", 108625642));
    series.addEntry(*createEquityEntry("20071204", "49.87", "50.35", "49.79", "50.08", 113374086));
    series.addEntry(*createEquityEntry("20071205", "50.61", "51.41", "50.57", "50.98", 137625111));
    series.addEntry(*createEquityEntry("20071206", "51.04", "51.78", "51.02", "51.73", 99766111));
    series.addEntry(*createEquityEntry("20071207", "51.70", "51.95", "51.49", "51.74", 88992298));
    series.addEntry(*createEquityEntry("20071210", "51.85", "52.10", "51.73", "51.95", 75659874));
    series.addEntry(*createEquityEntry("20071211", "52.02", "52.25", "50.62", "50.73", 167064664));
    series.addEntry(*createEquityEntry("20071212", "51.66", "51.84", "50.30", "51.19", 162975311));
    series.addEntry(*createEquityEntry("20071213", "50.74", "51.04", "50.39", "50.91", 153682683));
    series.addEntry(*createEquityEntry("20071214", "50.50", "51.01", "50.33", "50.38", 128409935));
    series.addEntry(*createEquityEntry("20071217", "50.19", "50.40", "49.12", "49.14", 134381821));
    series.addEntry(*createEquityEntry("20071218", "49.65", "49.68", "48.59", "49.30", 154251626));
    series.addEntry(*createEquityEntry("20071219", "49.30", "49.61", "49.02", "49.29", 137971898));
    series.addEntry(*createEquityEntry("20071220", "50.08", "50.37", "49.61", "50.31", 167342283));
    series.addEntry(*createEquityEntry("20071221", "51.12", "51.31", "50.89", "51.26", 107613153));
    series.addEntry(*createEquityEntry("20071224", "51.42", "51.81", "51.36", "51.65", 34499893));
    series.addEntry(*createEquityEntry("20071226", "51.56", "52.04", "51.41", "51.91", 58492079));
    series.addEntry(*createEquityEntry("20071227", "51.74", "51.91", "51.12", "51.34", 65778895));
    series.addEntry(*createEquityEntry("20071228", "51.45", "51.57", "50.84", "51.27", 67559859));
    series.addEntry(*createEquityEntry("20071231", "50.97", "51.09", "50.62", "50.63", 70137773));
    series.addEntry(*createEquityEntry("20080102", "50.68", "50.88", "49.54", "49.86", 152344477));
    series.addEntry(*createEquityEntry("20080103", "49.81", "50.17", "49.56", "50.03", 114105510));
    series.addEntry(*createEquityEntry("20080104", "49.20", "49.24", "47.62", "47.81", 212668574));
    series.addEntry(*createEquityEntry("20080107", "47.82", "48.01", "46.84", "47.58", 235089689));
    series.addEntry(*createEquityEntry("20080108", "47.70", "48.17", "46.33", "46.33", 261701946));
    series.addEntry(*createEquityEntry("20080109", "46.50", "47.37", "45.87", "47.33", 254138782));
    series.addEntry(*createEquityEntry("20080110", "46.83", "47.82", "46.70", "47.40", 249963616));
    series.addEntry(*createEquityEntry("20080111", "47.13", "47.18", "46.11", "46.46", 211685127));
    series.addEntry(*createEquityEntry("20080114", "47.10", "47.42", "46.72", "47.28", 168048431));
    series.addEntry(*createEquityEntry("20080115", "46.79", "46.93", "45.76", "45.96", 241795161));
    series.addEntry(*createEquityEntry("20080116", "45.60", "46.32", "44.87", "45.46", 265725123));

    return series;
  }

  OHLCTimeSeries<DecimalType> createIntradaySampleTimeSeries() {
    OHLCTimeSeries<DecimalType> ts(TimeFrame::INTRADAY, TradingVolume::SHARES);
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(ptime(date(2022, 1, 3), hours(9) + minutes(30)), DecimalType("100.0"), DecimalType("101.0"), DecimalType("99.5"), DecimalType("100.5"), DecimalType("1000"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(ptime(date(2022, 1, 3), hours(10) + minutes(30)), DecimalType("100.5"), DecimalType("102.0"), DecimalType("100.0"), DecimalType("101.0"), DecimalType("1100"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(ptime(date(2022, 1, 4), hours(9) + minutes(30)), DecimalType("101.0"), DecimalType("103.0"), DecimalType("100.5"), DecimalType("102.0"), DecimalType("1200"), TimeFrame::INTRADAY));
    ts.addEntry(OHLCTimeSeriesEntry<DecimalType>(ptime(date(2022, 1, 4), hours(10) + minutes(30)), DecimalType("102.0"), DecimalType("104.0"), DecimalType("101.0"), DecimalType("103.0"), DecimalType("1300"), TimeFrame::INTRADAY));
    return ts;
  }

  std::shared_ptr<OHLCTimeSeries<DecimalType>> getIntradaySeries(const std::string& fileName)
  {
    static std::map<std::string, std::shared_ptr<OHLCTimeSeries<DecimalType>>> cache;
    auto it = cache.find(fileName);
    if (it != cache.end()) return it->second;
    TradeStationFormatCsvReader<DecimalType> reader(fileName, TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick);
    reader.readFile();
    cache[fileName] = reader.getTimeSeries();
    return cache[fileName];
  }
} // End Anonymous Namespace

TEST_CASE("inplaceShuffle shuffles values but preserves multiset", "[ShuffleUtils]") {
    std::vector<int> original = {1,2,3,4,5,6,7,8,9,10};
    auto v1 = original, v2 = original;
    RandomMersenne rng;
    mkc_timeseries::inplaceShuffle(v1, rng);
    mkc_timeseries::inplaceShuffle(v2, rng);

    // should still contain the same elements:
    REQUIRE(std::is_permutation(v1.begin(), v1.end(), original.begin()));

    // very likely it’s not exactly the same order:
    REQUIRE((v1 != original || v2 != original));
}

TEST_CASE ("SyntheticTimeSeriesTest", "[SyntheticTimeSeries]")
{
  DecimalType minimumTick = DecimalConstants<DecimalType>::EquityTick;
  DecimalType minimumTickDiv2 = DecimalConstants<DecimalType>::EquityTick / DecimalConstants<DecimalType>::DecimalTwo;
  OHLCTimeSeries<DecimalType> sampleSeries = createSampleTimeSeries();

  SECTION ("Constructor Tests")
    {
      SyntheticTimeSeries<DecimalType> syntheticSeries (sampleSeries, minimumTick, minimumTickDiv2);
      REQUIRE (syntheticSeries.getNumElements() == sampleSeries.getNumEntries());
      REQUIRE (syntheticSeries.getTick() == minimumTick);
      REQUIRE (syntheticSeries.getTickDiv2() == minimumTickDiv2);
      REQUIRE (syntheticSeries.getFirstOpen() == (*sampleSeries.beginSortedAccess()).getOpenValue());
    }

  SECTION ("Copy Constructor Test")
    {
      SyntheticTimeSeries<DecimalType> syntheticSeries1 (sampleSeries, minimumTick, minimumTickDiv2);
      syntheticSeries1.createSyntheticSeries();
      SyntheticTimeSeries<DecimalType> syntheticSeries2 (syntheticSeries1);
      REQUIRE (syntheticSeries2.getNumElements() == syntheticSeries1.getNumElements());
      REQUIRE (syntheticSeries2.getFirstOpen() == syntheticSeries1.getFirstOpen());
      REQUIRE (syntheticSeries2.getTick() == syntheticSeries1.getTick());
      REQUIRE (syntheticSeries2.getTickDiv2() == syntheticSeries1.getTickDiv2());
      REQUIRE (*syntheticSeries2.getSyntheticTimeSeries() == *syntheticSeries1.getSyntheticTimeSeries());
    }

  SECTION ("Assignment Operator Test")
    {
      OHLCTimeSeries<DecimalType> anotherSampleSeries = createSampleTimeSeries();
      anotherSampleSeries.addEntry (*createEquityEntry ("20080117", "45.65", "45.99", "44.61","44.82", 254455987));
      SyntheticTimeSeries<DecimalType> syntheticSeries1 (sampleSeries, minimumTick, minimumTickDiv2);
      syntheticSeries1.createSyntheticSeries();
      SyntheticTimeSeries<DecimalType> syntheticSeries2 (anotherSampleSeries, minimumTick, minimumTickDiv2);
      syntheticSeries2 = syntheticSeries1;
      REQUIRE (syntheticSeries2.getNumElements() == syntheticSeries1.getNumElements());
      REQUIRE (syntheticSeries2.getFirstOpen() == syntheticSeries1.getFirstOpen());
      REQUIRE (syntheticSeries2.getTick() == syntheticSeries1.getTick());
      REQUIRE (syntheticSeries2.getTickDiv2() == syntheticSeries1.getTickDiv2());
      REQUIRE (*syntheticSeries2.getSyntheticTimeSeries() == *syntheticSeries1.getSyntheticTimeSeries());
    }

  SECTION ("createSyntheticSeries() Tests")
    {
      SyntheticTimeSeries<DecimalType> syntheticSeries (sampleSeries, minimumTick, minimumTickDiv2);
      syntheticSeries.createSyntheticSeries();
      std::shared_ptr<const OHLCTimeSeries<DecimalType>> p = syntheticSeries.getSyntheticTimeSeries();
      SECTION ("Timeseries size test") { REQUIRE (p->getNumEntries() == sampleSeries.getNumEntries()); }
      SECTION ("Timeseries date test") { REQUIRE (sampleSeries.getFirstDate() == p->getFirstDate()); REQUIRE (sampleSeries.getLastDate() == p->getLastDate()); }
      SECTION ("Timeseries time frame test") { REQUIRE (sampleSeries.getTimeFrame() == p->getTimeFrame()); }
      SECTION ("SyntheticTimeSeries not equal to original") { REQUIRE (sampleSeries != *p); REQUIRE_FALSE (sampleSeries == *p); }
      SECTION ("Test First Open Value") { REQUIRE (syntheticSeries.getFirstOpen() == (*sampleSeries.beginSortedAccess()).getOpenValue()); }
    }

  SECTION ("Getter Method Tests")
    {
      SyntheticTimeSeries<DecimalType> syntheticSeries (sampleSeries, minimumTick, minimumTickDiv2);
      REQUIRE (syntheticSeries.getTick() == minimumTick);
      REQUIRE (syntheticSeries.getTickDiv2() == minimumTickDiv2);
      REQUIRE (syntheticSeries.getNumElements() == sampleSeries.getNumEntries());
      REQUIRE (syntheticSeries.getFirstOpen() == (*sampleSeries.beginSortedAccess()).getOpenValue());
      syntheticSeries.createSyntheticSeries();
      REQUIRE (syntheticSeries.getSyntheticTimeSeries() != nullptr);
    }

  SECTION ("Shuffling Method Tests")
    {
      SyntheticTimeSeries<DecimalType> syntheticSeries (sampleSeries, minimumTick, minimumTickDiv2);
      std::vector<DecimalType> initialRelativeOpen = syntheticSeries.getRelativeOpen();
      std::vector<DecimalType> initialRelativeHigh = syntheticSeries.getRelativeHigh();
      std::vector<DecimalType> initialRelativeLow = syntheticSeries.getRelativeLow();
      std::vector<DecimalType> initialRelativeClose = syntheticSeries.getRelativeClose();
#ifdef SYNTHETIC_VOLUME
      std::vector<DecimalType> initialRelativeVolume = syntheticSeries.getRelativeVolume();
#endif
      syntheticSeries.createSyntheticSeries();
      auto syntheticClosingSeries = syntheticSeries.getSyntheticTimeSeries()->CloseTimeSeries();
      std::vector<DecimalType> syntheticClosingVector = syntheticClosingSeries.getTimeSeriesAsVector();
      DecimalType syntheticLastClosePrice = syntheticClosingVector.back();
      auto originalClosingSeries = sampleSeries.CloseTimeSeries();
      std::vector<DecimalType> originalClosingVector = originalClosingSeries.getTimeSeriesAsVector();
      DecimalType lastClosePrice = originalClosingVector.back();
      REQUIRE (lastClosePrice == syntheticLastClosePrice);
      std::vector<DecimalType> shuffledRelativeOpen = syntheticSeries.getRelativeOpen();
      std::vector<DecimalType> shuffledRelativeHigh = syntheticSeries.getRelativeHigh();
      std::vector<DecimalType> shuffledRelativeLow = syntheticSeries.getRelativeLow();
      std::vector<DecimalType> shuffledRelativeClose = syntheticSeries.getRelativeClose();
#ifdef SYNTHETIC_VOLUME
      std::vector<DecimalType> shuffledRelativeVolume = syntheticSeries.getRelativeVolume();
#endif
      bool openChanged = false, highChanged = false, lowChanged = false, closeChanged = false;
#ifdef SYNTHETIC_VOLUME
      bool volumeChanged = false;
#endif
      REQUIRE(initialRelativeOpen.size() == shuffledRelativeOpen.size());
      REQUIRE(initialRelativeHigh.size() == shuffledRelativeHigh.size());
      REQUIRE(initialRelativeLow.size() == shuffledRelativeLow.size());
      REQUIRE(initialRelativeClose.size() == shuffledRelativeClose.size());
#ifdef SYNTHETIC_VOLUME
      REQUIRE(initialRelativeVolume.size() == shuffledRelativeVolume.size());
#endif
      for (size_t i = 0; i < initialRelativeOpen.size(); ++i) {
	      if (initialRelativeOpen[i] != shuffledRelativeOpen[i]) openChanged = true;
	      if (initialRelativeHigh[i] != shuffledRelativeHigh[i]) highChanged = true;
	      if (initialRelativeLow[i] != shuffledRelativeLow[i]) lowChanged = true;
	      if (initialRelativeClose[i] != shuffledRelativeClose[i]) closeChanged = true;
#ifdef SYNTHETIC_VOLUME
	      if (initialRelativeVolume[i] != shuffledRelativeVolume[i]) volumeChanged = true;
#endif
      }
      REQUIRE(openChanged); REQUIRE(highChanged); REQUIRE(lowChanged); REQUIRE(closeChanged);
#ifdef SYNTHETIC_VOLUME
      REQUIRE(volumeChanged);
#endif
    }
}

TEST_CASE("SyntheticTimeSeries produces unique permutations", "[SyntheticTimeSeries][MonteCarlo]")
{
    auto baseSeries = getRandomPriceSeries();
    DecimalType tick("0.01");
    DecimalType tickDiv2("0.005");
    const int numPermutations = 100; // Reduced for faster test runs during dev
    std::vector<std::shared_ptr<const OHLCTimeSeries<DecimalType>>> permutations;
    for (int i = 0; i < numPermutations; ++i) {
	    SyntheticTimeSeries<DecimalType> synth(*baseSeries, tick, tickDiv2);
	    synth.createSyntheticSeries();
	    permutations.push_back(synth.getSyntheticTimeSeries());
	}
    for (int i = 0; i < numPermutations; ++i) {
	    for (int j = i + 1; j < numPermutations; ++j) {
	        REQUIRE(*permutations[i] != *permutations[j]);
	    }
	}
}

TEST_CASE("SyntheticTimeSeries intraday constructor", "[SyntheticTimeSeries][Intraday]") {
    TradeStationFormatCsvReader<DecimalType> reader("SSO_RAD_Hourly.txt", TimeFrame::INTRADAY, TradingVolume::SHARES, DecimalConstants<DecimalType>::EquityTick);
    REQUIRE(reader.getFileName() == "SSO_RAD_Hourly.txt");
    REQUIRE(reader.getTimeFrame() == TimeFrame::INTRADAY);
    REQUIRE_NOTHROW(reader.readFile());
    auto ts_ptr = reader.getTimeSeries();
    auto& series = *ts_ptr;
    REQUIRE(series.getFirstDateTime() == ptime(date(2012, 4, 2), hours( 9)));
    REQUIRE(series.getLastDateTime()  == ptime(date(2021, 4, 1), hours(15)));
    DecimalType tick = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;
    SyntheticTimeSeries<DecimalType> synth(series, tick, tickDiv2);
    REQUIRE(synth.getNumElements() == series.getNumEntries());
    auto firstIt = series.beginSortedAccess();
    REQUIRE(synth.getFirstOpen() == firstIt->getOpenValue());
}

TEST_CASE("SyntheticTimeSeries intraday createSyntheticSeries()", "[SyntheticTimeSeries][Intraday]") {
    auto& series = *getIntradaySeries("SSO_Hourly.txt");
    if(series.getNumEntries() == 0) {
        WARN("SSO_Hourly.txt is empty or could not be read, skipping some assertions in SyntheticTimeSeries intraday createSyntheticSeries().");
        return;
    }
    DecimalType tick = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;
    SyntheticTimeSeries<DecimalType> synth(series, tick, tickDiv2);
    REQUIRE_NOTHROW(synth.createSyntheticSeries());
    auto syn_ptr = synth.getSyntheticTimeSeries();
    REQUIRE(syn_ptr != nullptr);
    auto& syn = *syn_ptr;
    REQUIRE(syn.getTimeFrame() == TimeFrame::INTRADAY);
    REQUIRE(syn.getNumEntries() == series.getNumEntries());
    REQUIRE(syn.getFirstDateTime() == series.getFirstDateTime());
    REQUIRE(syn.getLastDateTime()  == series.getLastDateTime());
    bool interiorChanged = false;
    if (series.getNumEntries() > 0 && syn.getNumEntries() == series.getNumEntries()) {
        auto origIt = series.beginSortedAccess();
        auto synthIt = syn.beginSortedAccess();
        ptime basisDayEndDateTime;
        if (origIt != series.endSortedAccess()) {
            boost::gregorian::date firstDayDate = origIt->getDateTime().date();
            auto tempIt = origIt;
            while(tempIt != series.endSortedAccess() && tempIt->getDateTime().date() == firstDayDate) {
                basisDayEndDateTime = tempIt->getDateTime();
                ++tempIt;
            }
        }
        for (; origIt != series.endSortedAccess(); ++origIt, ++synthIt) {
            if (origIt->getDateTime() > basisDayEndDateTime) {
                if (origIt->getOpenValue() != synthIt->getOpenValue() ||
                    origIt->getHighValue() != synthIt->getHighValue() ||
                    origIt->getLowValue() != synthIt->getLowValue() ||
                    origIt->getCloseValue() != synthIt->getCloseValue()) {
                    interiorChanged = true;
                    break;
                }
            }
        }
    }
    std::map<boost::gregorian::date, int> dayCounts;
    for(auto it = series.beginSortedAccess(); it != series.endSortedAccess(); ++it) {
        dayCounts[it->getDateTime().date()]++;
    }
    if (dayCounts.size() > 1) { REQUIRE(interiorChanged); }
    else { REQUIRE_FALSE(interiorChanged); }
}

TEST_CASE("SyntheticTimeSeries produces unique intraday permutations", "[SyntheticTimeSeries][Intraday][MonteCarlo]") {
    auto baseSeriesPtr = getIntradaySeries("SSO_RAD_Hourly.txt");
    if (!baseSeriesPtr || baseSeriesPtr->getNumEntries() == 0) {
        WARN("SSO_RAD_Hourly.txt is empty or could not be read, skipping SyntheticTimeSeries produces unique intraday permutations.");
        return;
    }
    const auto& baseSeries = *baseSeriesPtr;
    DecimalType tick = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;
    const int numPerms = 20; // Reduced for faster test runs during dev
    std::vector<std::shared_ptr<const OHLCTimeSeries<DecimalType>>> perms;
    perms.reserve(numPerms);
    for (int i = 0; i < numPerms; ++i) {
        SyntheticTimeSeries<DecimalType> synth(baseSeries, tick, tickDiv2);
        synth.createSyntheticSeries();
        perms.push_back(synth.getSyntheticTimeSeries());
    }
    for (int i = 0; i < numPerms; ++i) {
        for (int j = i + 1; j < numPerms; ++j) {
            REQUIRE(*perms[i] != *perms[j]);
        }
    }
}

TEST_CASE("Intraday SyntheticTimeSeries: Basic Invariants", "[SyntheticTimeSeries][Intraday]") {
    auto sampleSeries = createIntradaySampleTimeSeries();
    DecimalType tick    = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;
    SyntheticTimeSeries<DecimalType> syntheticSeries(sampleSeries, tick, tickDiv2);
    REQUIRE(syntheticSeries.getNumElements() == sampleSeries.getNumEntries());
    REQUIRE(syntheticSeries.getTick() == tick);
    REQUIRE(syntheticSeries.getTickDiv2() == tickDiv2);
    syntheticSeries.createSyntheticSeries();
    auto synthPtr = syntheticSeries.getSyntheticTimeSeries();
    REQUIRE(synthPtr != nullptr);
    const auto& synthSeries = *synthPtr;
    REQUIRE(syntheticSeries.getFirstOpen() == sampleSeries.beginSortedAccess()->getOpenValue());
    REQUIRE(synthSeries.getTimeFrame() == TimeFrame::INTRADAY);
    REQUIRE(synthSeries.getNumEntries() == sampleSeries.getNumEntries());
    REQUIRE(synthSeries.getFirstDateTime() == sampleSeries.getFirstDateTime());
    REQUIRE(synthSeries.getLastDateTime()  == sampleSeries.getLastDateTime());
}

TEST_CASE("Intraday SyntheticTimeSeries: Interior Permutation", "[SyntheticTimeSeries][Intraday]") {
    auto sampleSeriesPtr = getIntradaySeries("SSO_RAD_Hourly.txt");
     if (!sampleSeriesPtr || sampleSeriesPtr->getNumEntries() == 0) {
        WARN("SSO_RAD_Hourly.txt is empty or could not be read, skipping Intraday SyntheticTimeSeries: Interior Permutation.");
        return;
    }
    const auto& sampleSeries = *sampleSeriesPtr;
    DecimalType tick    = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;
    SyntheticTimeSeries<DecimalType> syntheticSeries(sampleSeries, tick, tickDiv2);
    syntheticSeries.createSyntheticSeries();
    auto synPtr = syntheticSeries.getSyntheticTimeSeries();
    REQUIRE(synPtr != nullptr);
    const auto& synSeries = *synPtr;
    bool openChanged = false, highChanged = false, lowChanged = false, closeChanged = false;
    REQUIRE(sampleSeries.getNumEntries() == synSeries.getNumEntries());
    auto origIt = sampleSeries.beginSortedAccess();
    auto synthIt = synSeries.beginSortedAccess();
    ptime basisDayEndDateTime;
     if (origIt != sampleSeries.endSortedAccess()) {
        boost::gregorian::date firstDayDate = origIt->getDateTime().date();
        auto tempIt = origIt;
        while(tempIt != sampleSeries.endSortedAccess() && tempIt->getDateTime().date() == firstDayDate) {
            basisDayEndDateTime = tempIt->getDateTime();
            ++tempIt;
        }
    } else { basisDayEndDateTime = not_a_date_time; }
    int permutableBarsChecked = 0;
    for (; origIt != sampleSeries.endSortedAccess() && synthIt != synSeries.endSortedAccess(); ++origIt, ++synthIt) {
        if (origIt->getDateTime() > basisDayEndDateTime) {
            permutableBarsChecked++;
            if (origIt->getOpenValue() != synthIt->getOpenValue()) openChanged = true;
            if (origIt->getHighValue() != synthIt->getHighValue()) highChanged = true;
            if (origIt->getLowValue() != synthIt->getLowValue()) lowChanged = true;
            if (origIt->getCloseValue() != synthIt->getCloseValue()) closeChanged = true;
        }
    }
    std::map<boost::gregorian::date, int> dayCounts;
    for(auto itMap = sampleSeries.beginSortedAccess(); itMap != sampleSeries.endSortedAccess(); ++itMap) {
        dayCounts[itMap->getDateTime().date()]++;
    }
    if (dayCounts.size() > 1 && permutableBarsChecked > 0) { REQUIRE((openChanged || highChanged || lowChanged || closeChanged)); }
    else { REQUIRE_FALSE((openChanged || highChanged || lowChanged || closeChanged)); }
}

TEST_CASE("Intraday SyntheticTimeSeries: Unique Permutations", "[SyntheticTimeSeries][Intraday][MonteCarlo]") {
    auto sampleSeriesPtr = getIntradaySeries("SSO_RAD_Hourly.txt");
    if (!sampleSeriesPtr || sampleSeriesPtr->getNumEntries() == 0) {
        WARN("SSO_RAD_Hourly.txt is empty or could not be read, skipping Intraday SyntheticTimeSeries: Unique Permutations.");
        return;
    }
    const auto& sampleSeries = *sampleSeriesPtr;
    DecimalType tick    = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tickDiv2 = tick / DecimalConstants<DecimalType>::DecimalTwo;
    const int numPermutations = 20; // Reduced for faster test runs
    std::vector<std::shared_ptr<const OHLCTimeSeries<DecimalType>>> permutations;
    permutations.reserve(numPermutations);
    for (int i = 0; i < numPermutations; ++i) {
        SyntheticTimeSeries<DecimalType> synth(sampleSeries, tick, tickDiv2);
        synth.createSyntheticSeries();
        permutations.push_back(synth.getSyntheticTimeSeries());
    }
    for (int i = 0; i < numPermutations; ++i) {
        for (int j = i + 1; j < numPermutations; ++j) {
            REQUIRE(*permutations[i] != *permutations[j]);
        }
    }
}

TEST_CASE("Intraday SyntheticTimeSeries: Detailed Permutation Tests", "[SyntheticTimeSeries][Intraday][Detailed]") {
    DecimalType minimumTick = DecimalConstants<DecimalType>::EquityTick;
    DecimalType minimumTickDiv2 = minimumTick / DecimalConstants<DecimalType>::DecimalTwo;

    SECTION("TestBasisDayPreservation") {
        auto originalSeries = createThreeDayIntradaySampleTimeSeries();
        REQUIRE(originalSeries.getNumEntries() > 0);
        SyntheticTimeSeries<DecimalType> synth(originalSeries, minimumTick, minimumTickDiv2);
        synth.createSyntheticSeries();
        auto syntheticSeriesPtr = synth.getSyntheticTimeSeries();
        REQUIRE(syntheticSeriesPtr != nullptr);
        const auto& syntheticSeries = *syntheticSeriesPtr;
        boost::gregorian::date basisDate = date(2023, 1, 2);
        auto originalBasisDayBars = getBarsForDate(originalSeries, basisDate);
        REQUIRE(!originalBasisDayBars.empty());
        auto syntheticBasisDayBars = getBarsForDate(syntheticSeries, basisDate);
        REQUIRE(syntheticBasisDayBars.size() == originalBasisDayBars.size());
        REQUIRE(syntheticBasisDayBars == originalBasisDayBars);
        auto syntheticIter = syntheticSeries.beginSortedAccess();
        for (const auto& originalBasisBar : originalBasisDayBars) {
            REQUIRE(syntheticIter != syntheticSeries.endSortedAccess());
            REQUIRE(*syntheticIter == originalBasisBar);
            ++syntheticIter;
        }
    }

    SECTION("TestNoPermutableDays (1-Day Series)") {
        auto originalSeries = createOneDayIntradaySampleTimeSeries();
        REQUIRE(originalSeries.getNumEntries() > 0);
        SyntheticTimeSeries<DecimalType> synth(originalSeries, minimumTick, minimumTickDiv2);
        synth.createSyntheticSeries();
        auto syntheticSeriesPtr = synth.getSyntheticTimeSeries();
        REQUIRE(syntheticSeriesPtr != nullptr);
        const auto& syntheticSeries = *syntheticSeriesPtr;
        REQUIRE(syntheticSeries.getNumEntries() == originalSeries.getNumEntries());
        REQUIRE(syntheticSeries == originalSeries);
    }

    SECTION("TestOvernightGapPermutation (3-Day Series)") {
        auto originalSeries = createThreeDayIntradaySampleTimeSeries();
        DecimalType one = DecimalConstants<DecimalType>::DecimalOne;
        DecimalType zero = DecimalConstants<DecimalType>::DecimalZero;
        auto extractGaps = [&](const OHLCTimeSeries<DecimalType>& series) {
            std::vector<DecimalType> gaps;
            std::map<boost::gregorian::date, std::vector<OHLCTimeSeriesEntry<DecimalType>>> dayMap;
            for (auto it = series.beginSortedAccess(); it != series.endSortedAccess(); ++it) { dayMap[it->getDateTime().date()].push_back(*it); }
            if (dayMap.size() < 2) return gaps;
            auto mapIt = dayMap.begin();
            if (mapIt->second.empty()) return gaps;
            DecimalType prevDayActualClose = mapIt->second.back().getCloseValue();
            ++mapIt;
            for (; mapIt != dayMap.end(); ++mapIt) {
                const auto& currentDayBars = mapIt->second;
                if (currentDayBars.empty()) { gaps.push_back(one); }
                else {
                    DecimalType currentDayOriginalOpen = currentDayBars.front().getOpenValue();
                    if (prevDayActualClose != zero) { gaps.push_back(currentDayOriginalOpen / prevDayActualClose); }
                    else { gaps.push_back(one); }
                    prevDayActualClose = currentDayBars.back().getCloseValue();
                }
            }
            return gaps;
        };
        std::vector<DecimalType> originalGaps = extractGaps(originalSeries);
        REQUIRE(originalGaps.size() == 2);
        SyntheticTimeSeries<DecimalType> synthGen(originalSeries, minimumTick, minimumTickDiv2);
        bool differentGapOrderObserved = false;
        std::vector<DecimalType> firstRunSyntheticGaps;
        const int numRuns = 30;
        for (int i = 0; i < numRuns; ++i) {
            synthGen.createSyntheticSeries();
            auto syntheticSeriesPtr = synthGen.getSyntheticTimeSeries();
            REQUIRE(syntheticSeriesPtr != nullptr);
            const auto& syntheticSeries = *syntheticSeriesPtr;
            std::vector<DecimalType> syntheticGaps = extractGaps(syntheticSeries);
            REQUIRE(syntheticGaps.size() == originalGaps.size());
            if (originalGaps.size() > 1) {
                if (i == 0) { firstRunSyntheticGaps = syntheticGaps; }
                else {
                    bool areDifferent = false;
                    if (firstRunSyntheticGaps.size() == syntheticGaps.size()) {
                        for(size_t k=0; k < syntheticGaps.size(); ++k) {
                            if (num::abs(firstRunSyntheticGaps[k] - syntheticGaps[k]) > DecimalType("0.0000001")) {
                                areDifferent = true; break;
                            }
                        }
                    } else { areDifferent = true; }
                    if (areDifferent) { differentGapOrderObserved = true; break; }
                }
            }
        }
        if (originalGaps.size() > 1) { REQUIRE(differentGapOrderObserved); }
        else if (originalGaps.size() == 1) { REQUIRE_FALSE(differentGapOrderObserved); }
    }

    SECTION("TestIntradayVolumePermutation (Checks current zero-volume behavior for permuted bars)") {
      auto originalSeries = createThreeDayIntradaySampleTimeSeries();
      REQUIRE(originalSeries.getNumEntries() > 0);
      ptime basisDayEndDateTime;
      if (originalSeries.getNumEntries() > 0) {
          auto it = originalSeries.beginSortedAccess();
          boost::gregorian::date firstDayDate = it->getDateTime().date();
           while(it != originalSeries.endSortedAccess() && it->getDateTime().date() == firstDayDate) {
               basisDayEndDateTime = it->getDateTime();
               ++it;
           }
      } else { basisDayEndDateTime = not_a_date_time; }
      SyntheticTimeSeries<DecimalType> synth(originalSeries, minimumTick, minimumTickDiv2);
      synth.createSyntheticSeries();
      auto syntheticSeriesPtr = synth.getSyntheticTimeSeries();
      REQUIRE(syntheticSeriesPtr != nullptr);
      const auto& syntheticSeries = *syntheticSeriesPtr;
      bool checkedPermutableBar = false;
      for (auto it = syntheticSeries.beginSortedAccess(); it != syntheticSeries.endSortedAccess(); ++it) {
        if (it->getDateTime() > basisDayEndDateTime) {
            checkedPermutableBar = true;
            REQUIRE(it->getVolumeValue() == DecimalConstants<DecimalType>::DecimalZero);
        } else {
            auto originalBar = originalSeries.getTimeSeriesEntry(it->getDateTime());
            REQUIRE(it->getVolumeValue() == originalBar.getVolumeValue());
        }
      }
      if (originalSeries.getNumEntries() > getBarsForDate(originalSeries, originalSeries.getFirstDate()).size()) {
          REQUIRE(checkedPermutableBar);
      }
    }
}

// 1) Per-day bar counts must be identical between original and synthetic
TEST_CASE("Intraday SyntheticTimeSeries: per-day bar counts preserved",
          "[SyntheticTimeSeries][Intraday]") {
    using namespace boost::gregorian;
    using namespace boost::posix_time;

    // Arrange: 3-day sample (Day 1 basis, Day 2+3 permutable)
    DecimalType tick   = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tick2  = tick / DecimalConstants<DecimalType>::DecimalTwo;
    auto original = createThreeDayIntradaySampleTimeSeries();
    
    // Act
    SyntheticTimeSeries<DecimalType> synth(original, tick, tick2);
    synth.createSyntheticSeries();
    auto synthetic = *synth.getSyntheticTimeSeries();

    // Count bars per date in original
    std::map<date,int> origCounts;
    for (auto it = original.beginSortedAccess(); it != original.endSortedAccess(); ++it) {
        ++origCounts[it->getDateTime().date()];
    }
    // Count bars per date in synthetic
    std::map<date,int> synCounts;
    for (auto it = synthetic.beginSortedAccess(); it != synthetic.endSortedAccess(); ++it) {
        ++synCounts[it->getDateTime().date()];
    }

    // They should cover the same set of dates…
    REQUIRE(synCounts.size() == origCounts.size());
    for (auto& kv : origCounts) {
        REQUIRE(synCounts.count(kv.first) == 1);
        REQUIRE(synCounts[kv.first] == kv.second);
    }
}

// 2) Two runs on the same input must differ in at least one bar
TEST_CASE("Intraday SyntheticTimeSeries: two runs produce different series",
          "[SyntheticTimeSeries][Intraday][MonteCarlo]") {
    using namespace boost::gregorian;
    using namespace boost::posix_time;

    // Arrange
    DecimalType tick   = DecimalConstants<DecimalType>::EquityTick;
    DecimalType tick2  = tick / DecimalConstants<DecimalType>::DecimalTwo;
    //auto original = createThreeDayIntradaySampleTimeSeries();
    auto& original = *getIntradaySeries("SSO_Hourly.txt");
    // Act: run #1
    SyntheticTimeSeries<DecimalType> s1(original, tick, tick2);
    s1.createSyntheticSeries();
    auto out1 = *s1.getSyntheticTimeSeries();

    // Act: run #2
    SyntheticTimeSeries<DecimalType> s2(original, tick, tick2);
    s2.createSyntheticSeries();
    auto out2 = *s2.getSyntheticTimeSeries();

    // Assert: same total count
    REQUIRE(out1.getNumEntries() == out2.getNumEntries());

    // They must differ on at least one bar (any field)
    bool sawDifference = false;
    auto it1 = out1.beginSortedAccess();
    auto it2 = out2.beginSortedAccess();
    for (; it1 != out1.endSortedAccess() && it2 != out2.endSortedAccess(); ++it1, ++it2) {
        if (!(*it1 == *it2)) {
            sawDifference = true;
            break;
        }
    }
    REQUIRE(sawDifference);
}
