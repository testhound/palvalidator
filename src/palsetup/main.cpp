#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <limits>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp> 

#include "TimeSeriesCsvReader.h"
#include "TimeSeriesCsvWriter.h"
#include "TimeSeriesIndicators.h"
#include "DecimalConstants.h"
#include "TimeFrameUtility.h"
#include "SecurityAttributesFactory.h"
#include <optional>
#include <unordered_set>
#include <set>
#include <numeric>
#include <cmath>


using namespace mkc_timeseries;
using std::shared_ptr;
using namespace boost::gregorian;
namespace fs = std::filesystem;

// ---------------- Quantization-aware start-date detector ----------------

struct CleanStartConfig {
  int   windowBars           = 252;   // ~1y for Daily; adjusted by timeframe
  int   stabilityBufferBars  = 60;    // safety buffer after first 'good' window
  double maxRelTick          = 0.005; // tick / medianPrice <= 0.5%
  double maxZeroFrac         = 0.30;  // <=30% effectively-zero returns in window
  int   minUniqueLevels      = 120;   // distinct close levels (at integerized grid) in window
  // intraday cadence. 0 means "unknown / ask user / estimate"
  int   intradayMinutesPerBar = 0;
};

inline int computeIntradayBarsPerDay(int minutesPerBar)
{
  if (minutesPerBar <= 0)
    return 390;                 // assume 1-min if unknown

  return std::max(1, static_cast<int>(std::round(390.0 / minutesPerBar)));
}

/**
 * @brief Estimate the *effective* price tick from the data (not the exchange rule).
 *
 * This infers the smallest price increment (grid step) that your *stored* prices
 * adhere to—useful when split-adjusted equity data was rounded to a coarse grid
 * (e.g., 2 decimals) or when futures/FX have fractional ticks. The returned tick
 * reflects the quantization actually present in the series, which is what affects
 * zero-returns and rounding artifacts in your statistics.
 *
 * Method (robust, data-driven):
 *  1) Auto-detect a decimal scaling factor 10^k (k in [0..maxDecimals]) such that
 *     most closes become integers when multiplied by 10^k. We choose the *smallest*
 *     k where ≥ integralThreshold of points are (close to) integers.
 *  2) At that scale, round closes to integers, gather unique levels, and compute
 *     the GCD of the positive adjacent differences between sorted unique levels.
 *  3) Return tick = GCD / 10^k (back to price units).
 *
 * Notes:
 *  - This does not need to match the exchange’s minimum tick. It captures how your
 *    vendor/file is quantized, which is the right signal for the clean-start detector.
 *  - If you *know* the true tick (e.g., via SecurityAttributesFactory), prefer that
 *    and use this only as a fallback.
 *
 * Complexity:
 *  - O(N log N) due to sorting unique levels once; the scale search is O(N * maxDecimals).
 *
 * Edge cases:
 *  - If the series is empty or has <2 unique levels, returns 10^{-k} as a conservative
 *    fallback at the chosen k.
 *  - Non-finite values are skipped.
 *
 * @tparam Decimal             Numeric wrapper used by your time-series (e.g., mkc::Num).
 * @param  series              Input OHLC time series.
 * @param  maxDecimals         Maximum scale exponent to try (default 6 → micro precision).
 * @param  integralThreshold   Fraction of points that must look integral at a scale
 *                             to accept it (default 0.98).
 * @return double              Estimated effective tick size (price units).
 */
template <typename Decimal>
double estimateEffectiveTick(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                             int maxDecimals = 6,
                             double integralThreshold = 0.98)
{
  using mkc_timeseries::OHLCTimeSeries;

  // 0) Extract closes
  const auto& entries = series.getEntriesCopy();
  std::vector<double> closes;
  closes.reserve(entries.size());
  for (const auto& e : entries)
  {
    const double x = e.getCloseValue().getAsDouble();
    if (std::isfinite(x)) closes.push_back(x);
  }

  if (closes.size() < 2)
    return 1e-2;

  // 1) Find smallest 10^k where most points look integral
  auto looks_integral = [](double y)
  {
    return std::fabs(y - std::llround(y)) < 1e-8;
  };

  int bestK = 2;  // keep pennies as pragmatic fallback
  for (int k = 0; k <= maxDecimals; ++k)
  {
    const double scale = std::pow(10.0, k);
    int ok = 0, tot = 0;
    for (double x : closes)
    {
      const double y = x * scale;
      if (!std::isfinite(y))
	continue;
      
      ++tot;
      
      if (looks_integral(y))
	++ok;
    }
    if (tot > 0 && static_cast<double>(ok) >= integralThreshold * static_cast<double>(tot))
    {
      bestK = k;    // choose the smallest sufficient k
      break;
    }
  }

  const double scale = std::pow(10.0, bestK);

  // 2) Quantize and compute GCD step
  std::vector<long long> levels;
  levels.reserve(closes.size());
  for (double x : closes)
  {
    const double y = x * scale;
    if (std::isfinite(y))
      levels.push_back(std::llround(y));
  }

  if (levels.size() < 2)
    return std::pow(10.0, -bestK);

  std::sort(levels.begin(), levels.end());
  levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
  if (levels.size() < 2)
    return std::pow(10.0, -bestK);

  long long g = 0;
  for (std::size_t i = 1; i < levels.size(); ++i)
  {
    const long long d = levels[i] - levels[i - 1];
    if (d > 0) g = (g == 0) ? d : std::gcd(g, d);
  }

  if (g <= 0)
    g = 1;

  // 3) Convert back to price units
  return static_cast<double>(g) / scale;
}

struct CleanStartResult {
  size_t startIndex = 0;
  double tick       = 0.01;
  double relTick    = 0.0;
  double zeroFrac   = 0.0;
  bool   found      = false;
};

/**
 * @brief Finds a suitable starting index for a time series to mitigate data quantization issues.
 *
 * This function finds the first index in an OHLC time series where price quantization
 * (e.g., 2-decimal rounding after split-adjustment) no longer dominates the signal.
 *
 * This function addresses the problem of price quantization, which occurs when a security's
 * price becomes very low after stock splits, leading to a large effective tick size
 * relative to the price. For example, a $0.04 price with a $0.01 tick results in a 25% price step,
 * which can distort statistical measures like median and Qn.
 *
 * The algorithm performs a data-driven cutoff to find the first "clean" window of data that
 * meets specific criteria for price stability and tick-related noise. It iterates through
 * the time series in a sliding window and checks if the data within the window is suitable for analysis.
 *
 * The criteria for a "clean" window are:
 * 1.  **Relative Tick Size**: The estimated effective tick, relative to the median price in the window,
 * is below a configured maximum threshold (`maxRelTick`).
 * 2.  **Zero-Return Fraction**: The fraction of effectively zero returns (moves smaller than one tick)
 * is below a configured maximum threshold (`maxZeroFrac`). This helps filter out periods
 * where prices are highly quantized.
 * 3.  **Unique Price Levels**: The number of unique price levels within the window is above a
 * configured minimum threshold (`minUniqueLevels`). This helps ensure there is enough
 * price variation to perform meaningful statistical analysis.
 *
 * Tick handling:
 *   - If @p knownTick is provided (e.g., from SecurityAttributesFactory), it is used.
 *   - Otherwise, the function falls back to estimating an effective tick from the data
 *     (e.g., by detecting the price grid via integer GCD on scaled closes).
 *
 * Window sizing:
 *   - DAILY: ~252 bars; WEEKLY: ~104; MONTHLY: ~60.
 *   - INTRADAY: scaled to ≈ one trading year using @p cfg.intradayMinutesPerBar and
 *     an equity regular session of 390 minutes/day. Bars/day ≈ round(390 / minutesPerBar),
 *     so windowBars ≈ barsPerDay * 252. For short files, the window shrinks gracefully
 *     (down to ≈ one month, ~21 trading days equivalent) so it always fits the data.
 *   - A stability buffer (e.g., ~10 trading days worth of bars) is added after the
 *     first qualifying window start before returning the final @c startIndex.
 *
 * Complexity:
 *   - Per window the code computes a median, unique-level count, and zero-return rate,
 *     each O(W) where W = cfg.windowBars, yielding O(N * W) total in the worst case.
 *     (Good defaults keep W moderate; rolling optimizations are possible if needed.)
 *
 * Edge cases and safety:
 *   - Empty or too-short series returns {found=false}.
 *   - The window size is clamped so it never exceeds the number of bars available.
 *   - Non-finite prices are skipped in return calculations.
 *
 * @tparam Decimal The numeric type used for the time series data (e.g., Num, Decimal).
 * @param series The input OHLCTimeSeries to analyze.
 * @param cfg The configuration struct (`CleanStartConfig`) defining the parameters for the search.
 * @param knownTick An optional known tick size. If provided, it will be used instead of
 * estimating the tick from the data.
 * @return A `CleanStartResult` struct containing the found starting index, estimated tick
 * properties, and a boolean indicating if a clean start was found.
 */
template <typename Decimal>
CleanStartResult findCleanStartIndex(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                                     CleanStartConfig cfg,
                                     std::optional<double> knownTick)
{
  using namespace mkc_timeseries;

  const auto entries = series.getEntriesCopy();
  const size_t n = entries.size();

  // Window sizing (same as before)
  switch (series.getTimeFrame()){
    case TimeFrame::DAILY:   cfg.windowBars = 252; cfg.stabilityBufferBars = 60; break;
    case TimeFrame::WEEKLY:  cfg.windowBars = 260; cfg.stabilityBufferBars = 12; break;
    case TimeFrame::MONTHLY: cfg.windowBars = 60;  cfg.stabilityBufferBars = 3;  break;
    case TimeFrame::INTRADAY:
    default: {
      const int barsPerDay = computeIntradayBarsPerDay(cfg.intradayMinutesPerBar);
      int desiredDays = 252;
      while (barsPerDay * desiredDays >= static_cast<int>(n) && desiredDays > 21)
        desiredDays /= 2;
      cfg.windowBars = std::max(3, barsPerDay * desiredDays);
      cfg.stabilityBufferBars = std::max(60, barsPerDay * 10);
      break;
    }
  }

  CleanStartResult res;
  if (n == 0) return res;
  if (n < static_cast<size_t>(std::max(3, cfg.windowBars))) return res;

  // Extract closes as doubles
  std::vector<double> close(n);
  for (size_t i = 0; i < n; ++i)
    close[i] = entries[i].getCloseValue().getAsDouble();

  // Helper: robust median over [L,R]
  auto medianSpan = [&](size_t L, size_t R){
    std::vector<double> tmp; tmp.reserve(R - L + 1);
    for (size_t i = L; i <= R; ++i) tmp.push_back(close[i]);
    size_t m = tmp.size() / 2;
    std::nth_element(tmp.begin(), tmp.begin() + m, tmp.end());
    double med = tmp[m];
    if ((tmp.size() & 1) == 0) {
      const double a = med;
      const double b = *std::max_element(tmp.begin(), tmp.begin() + m);
      med = 0.5 * (a + b);
    }
    return med;
  };

  // Helper: infer tick over [L,R] (window-local), up to 8 decimals
  auto inferTickOver = [&](size_t L, size_t R)->double {
    const int maxDecimals = 8;
    const double integralThreshold = 0.95;   // slightly looser than 0.98 to handle FP noise
    auto looks_integral = [](double y){
      // Abs tolerance scaled a bit by magnitude to be safe
      double tol = std::max(1e-8, std::fabs(y) * 1e-12);
      return std::fabs(y - std::llround(y)) < tol;
    };

    // 1) choose smallest k with ≥ integralThreshold points integral at 10^k
    int bestK = 2; // pragmatic fallback
    for (int k = 0; k <= maxDecimals; ++k){
      const double scale = std::pow(10.0, k);
      int ok=0, tot=0;
      for (size_t i=L; i<=R; ++i){
        const double y = close[i] * scale;
        if (!std::isfinite(y)) continue;
        ++tot;
        if (looks_integral(y)) ++ok;
      }
      if (tot > 0 && static_cast<double>(ok) >= integralThreshold * static_cast<double>(tot)){
        bestK = k;
        break;
      }
    }
    const double scale = std::pow(10.0, bestK);

    // 2) integerize and compute GCD of adjacent diffs
    std::vector<long long> levels; levels.reserve(R-L+1);
    for (size_t i=L; i<=R; ++i){
      const double y = close[i] * scale;
      if (std::isfinite(y)) levels.push_back(std::llround(y));
    }
    if (levels.size() < 2) return std::pow(10.0, -bestK);
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    if (levels.size() < 2) return std::pow(10.0, -bestK);

    long long g = 0;
    for (size_t i=1; i<levels.size(); ++i){
      const long long d = levels[i] - levels[i-1];
      if (d > 0) g = (g==0) ? d : std::gcd(g, d);
    }
    if (g <= 0) g = 1;

    return static_cast<double>(g) / scale;
  };

  const size_t W = static_cast<size_t>(std::max(3, cfg.windowBars));

  // Slide windows; infer a tick for each window and test it
  for (size_t L=0, R=W-1; R < n; ++L, ++R)
  {
    // Window-local effective tick (prefer finer of inferred vs known)
    double winTick = inferTickOver(L, R);
    if (knownTick && *knownTick > 0.0)
      winTick = std::min(winTick, *knownTick);

    // Median and relTick
    const double med = medianSpan(L, R);
    const double relTick = (med > 0.0 && std::isfinite(med)) ? (winTick / med)
                                                             : std::numeric_limits<double>::infinity();

    // Distinct levels on the window tick grid
    const double invTick = (winTick > 0.0) ? (1.0 / winTick) : 100.0;
    std::unordered_set<long long> uniq;
    uniq.reserve((R - L + 1) / 2 + 8);
    for (size_t i=L; i<=R; ++i)
      uniq.insert(std::llround(close[i] * invTick));

    // Zero moves: |Δprice| ≤ tick
    int zeros=0, denom=0;
    for (size_t i=std::max(L+1, static_cast<size_t>(1)); i<=R; ++i){
      const double a = close[i-1], b = close[i];
      if (!std::isfinite(a) || !std::isfinite(b)) continue;
      if (std::fabs(b - a) <= winTick) ++zeros;
      ++denom;
    }
    const double zeroFrac = (denom > 0) ? (static_cast<double>(zeros)/denom) : 1.0;

    const bool ok = (relTick <= cfg.maxRelTick) &&
                    (zeroFrac <= cfg.maxZeroFrac) &&
                    (static_cast<int>(uniq.size()) >= cfg.minUniqueLevels);

    if (ok){
      const size_t bufferedStart = L + static_cast<size_t>(std::max(0, cfg.stabilityBufferBars));
      res.startIndex = std::min(n - 1, bufferedStart);
      res.tick    = winTick;   // <- report the tick that made this window pass
      res.relTick = relTick;
      res.zeroFrac = zeroFrac;
      res.found = true;
      return res;
    }
  }

  // No qualifying window found—disable trimming.
  res.found = false;
  res.startIndex = 0;
  return res;
}

using Num = num::DefaultNumber;

// Writes a CSV configuration file for permutation testing into the given output directory
void writeConfigFile(const std::string& outputDir,
                     const std::string& tickerSymbol,
                     const OHLCTimeSeries<Num>& insampleSeries,
                     const OHLCTimeSeries<Num>& outOfSampleSeries,
                     const std::string& timeFrame){
  fs::path configFileName = fs::path(outputDir) / (tickerSymbol + "_config.csv");
  std::ofstream configFile(configFileName);
  if (!configFile.is_open()){
      std::cerr << "Error: Unable to open config file " << configFileName << std::endl;
      return;
    }

  std::string irPath     = "./" + tickerSymbol + "_IR.txt";
  std::string dataPath   = "./" + tickerSymbol + "_ALL.txt";
  std::string fileFormat = (timeFrame == "Intraday") ? "INTRADAY::TRADESTATION" : "PAL";

  // Dates in YYYYMMDD format - use DateTime for intraday to preserve time information
  std::string isDateStart, isDateEnd, oosDateStart, oosDateEnd;

  if (timeFrame == "Intraday"){
      // For intraday data, use full DateTime to avoid overlapping date ranges
      // Format as YYYYMMDDTHHMMSS for ptime
      isDateStart  = boost::posix_time::to_iso_string(insampleSeries.getFirstDateTime());
      isDateEnd    = boost::posix_time::to_iso_string(insampleSeries.getLastDateTime());
      oosDateStart = boost::posix_time::to_iso_string(outOfSampleSeries.getFirstDateTime());
      oosDateEnd   = boost::posix_time::to_iso_string(outOfSampleSeries.getLastDateTime());
    }
  else
    {
      // For EOD data, use Date methods as before
      isDateStart  = to_iso_string(insampleSeries.getFirstDate());
      isDateEnd    = to_iso_string(insampleSeries.getLastDate());
      oosDateStart = to_iso_string(outOfSampleSeries.getFirstDate());
      oosDateEnd   = to_iso_string(outOfSampleSeries.getLastDate());
    }

  // Write CSV line: Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame
  configFile << tickerSymbol << ","
	     << irPath       << ","
	     << dataPath     << ","
	     << fileFormat   << ","
	     << isDateStart  << ","
	     << isDateEnd    << ","
	     << oosDateStart << ","
	     << oosDateEnd   << ","
	     << timeFrame    << std::endl;

  configFile.close();
  std::cout << "Configuration file written: " << configFileName << std::endl;
}

// Generate timeframe directory name based on timeframe and minutes
std::string createTimeFrameDirectoryName(const std::string& timeFrameStr, int intradayMinutes = 0){
  if (timeFrameStr == "Intraday"){
    return "Intraday_" + std::to_string(intradayMinutes);
  }
  return timeFrameStr;
}

// Factory for CSV readers using specified time frame
shared_ptr< TimeSeriesCsvReader<Num> >
createTimeSeriesReader(int fileType,
                       const std::string& fileName,
                       const Num& tick,
                       TimeFrame::Duration timeFrame){
  switch (fileType){
    case 1:
      return std::make_shared<CSIErrorCheckingFuturesCsvReader<Num>>(fileName,
								     timeFrame,
								     TradingVolume::SHARES,
								     tick);
    case 2:
      return std::make_shared<CSIErrorCheckingExtendedFuturesCsvReader<Num>>(fileName,
									     timeFrame,
									     TradingVolume::SHARES,
									     tick);
    case 3:
      // Use TradeStationFormatCsvReader for intraday, TradeStationErrorCheckingFormatCsvReader for others
      if (timeFrame == TimeFrame::INTRADAY){
	  return std::make_shared<TradeStationFormatCsvReader<Num>>(fileName,
								    timeFrame,
								    TradingVolume::SHARES,
								    tick);
        }
      else
        {
	  return std::make_shared<TradeStationErrorCheckingFormatCsvReader<Num>>(fileName,
										 timeFrame,
										 TradingVolume::SHARES,
										 tick);
        }
    case 4:
      return std::make_shared<PinnacleErrorCheckingFormatCsvReader<Num>>(fileName,
									 timeFrame,
									 TradingVolume::SHARES,
									 tick);
    case 5:
      return std::make_shared<PALFormatCsvReader<Num>>(fileName,
    		       timeFrame,
    		       TradingVolume::SHARES,
    		       tick);
    case 6:
      return std::make_shared<WealthLabCsvReader<Num>>(fileName,
    		       timeFrame,
    		       TradingVolume::SHARES,
    		       tick);
    default:
      throw std::out_of_range("Invalid file type");
    }
}

int main(int argc, char** argv){
  // Parse command line arguments for -indicator flag
  bool indicatorMode = false;
  std::vector<std::string> args;
  
  // Separate flag from positional arguments
  for (int i = 1; i < argc; ++i){
    std::string arg = std::string(argv[i]);
    if (arg == "-indicator" || arg == "--indicator"){
      indicatorMode = true;
    } else {
      args.push_back(arg);
    }
  }
  
  if ((args.size() == 2) || (args.size() == 3)){
      // Command-line args: datafile, file-type, [securityTick]
      std::string historicDataFileName = args[0];
      int fileType = std::stoi(args[1]);
      Num securityTick(DecimalConstants<Num>::EquityTick);
      if (args.size() == 3)
	securityTick = Num(std::stof(args[2]));

      // 1. Extract default ticker symbol from filename and read ticker symbol
      std::string defaultTicker;
      fs::path filePath(historicDataFileName);
      std::string baseName = filePath.stem().string(); // Gets filename without extension
      size_t dotPos = baseName.find('.');
      if (dotPos != std::string::npos){
	  defaultTicker = baseName.substr(0, dotPos);
        }
      else
        {
	  defaultTicker = baseName;
        }

      std::string tickerSymbol;
      std::cout << "Enter ticker symbol [default " << defaultTicker << "]: ";
      std::getline(std::cin, tickerSymbol);
      if (tickerSymbol.empty()){
	  tickerSymbol = defaultTicker;
        }

      // 2. Read and parse time frame (default Daily)
      std::string timeFrameStr;
      TimeFrame::Duration timeFrame;
      bool validFrame = false;
      while (!validFrame){
	  std::cout << "Enter time frame ([D]aily, [W]eekly, [M]onthly, [I]ntraday) [default D]: ";
	  std::string tfInput;
	  std::getline(std::cin, tfInput);
	  if (tfInput.empty()) tfInput = "D";
	  char c = std::toupper(tfInput[0]);
	  switch (c){
	    case 'D':
	      timeFrameStr = "Daily";
	      validFrame = true;
	      break;

	    case 'W':
	      timeFrameStr = "Weekly";
	      validFrame = true;
	      break;

	    case 'M':
	      timeFrameStr = "Monthly";
	      validFrame = true;
	      break;

	    case 'I':
	      timeFrameStr = "Intraday";
	      validFrame = true;
	      break;

	    default:
	      std::cerr << "Invalid time frame. Please enter D, W, M, or I." << std::endl;
            }
        }
      timeFrame = getTimeFrameFromString(timeFrameStr);

      // Handle intraday minutes input
      int intradayMinutes = 0; // default
      if (timeFrameStr == "Intraday"){
        std::cout << "Enter number of minutes for intraday timeframe (1-1440, default 90): ";
        std::string minutesInput;
        std::getline(std::cin, minutesInput);
        if (!minutesInput.empty()){
          try {
            intradayMinutes = std::stoi(minutesInput);
            intradayMinutes = std::clamp(intradayMinutes, 1, 1440);
          } catch (...){
            std::cerr << "Invalid input for minutes. Using default 90." << std::endl;
            intradayMinutes = 90;
          }
        }
      }

      // 3. Handle indicator selection if in indicator mode
      std::string selectedIndicator;
      if (indicatorMode){
        std::cout << "Select indicator ([I]BS - Internal Bar Strength): ";
        std::string indicatorChoice;
        std::getline(std::cin, indicatorChoice);
        if (indicatorChoice.empty()) indicatorChoice = "I";
        
        char c = std::toupper(indicatorChoice[0]);
        switch (c){
          case 'I':
            selectedIndicator = "IBS";
            std::cout << "Selected: Internal Bar Strength (IBS)" << std::endl;
            break;
          default:
            std::cerr << "Invalid indicator selection. Defaulting to IBS." << std::endl;
            selectedIndicator = "IBS";
        }
      }

      // 4. Read and parse data split percentages with validation
      double insamplePercent, outOfSamplePercent, reservedPercent;
      bool validPercentages = false;
      
      while (!validPercentages){
          // Get in-sample percentage (default 80%)
          insamplePercent = 60.0;
          std::cout << "Enter percent of data for in-sample (0-100, default 60%): ";
          std::string insamplePercentStr;
          std::getline(std::cin, insamplePercentStr);
          if (!insamplePercentStr.empty()){
              try
                {
                  insamplePercent = std::stod(insamplePercentStr);
                }
              catch (...){
                  std::cerr << "Invalid input for in-sample percent. Using default 80%." << std::endl;
                  insamplePercent = 80.0;
                }
            }
          insamplePercent = std::clamp(insamplePercent, 0.0, 100.0);

          // Get out-of-sample percentage
          outOfSamplePercent = 40.0;
          std::cout << "Enter percent of data for out-of-sample (0-100, default 40%): ";
          std::string outOfSamplePercentStr;
          std::getline(std::cin, outOfSamplePercentStr);
          if (!outOfSamplePercentStr.empty()){
              try
                {
                  outOfSamplePercent = std::stod(outOfSamplePercentStr);
                }
              catch (...){
                  std::cerr << "Invalid input for out-of-sample percent. Using 0%." << std::endl;
                  outOfSamplePercent = 0.0;
                }
            }
          outOfSamplePercent = std::clamp(outOfSamplePercent, 0.0, 100.0);

          // Get reserved percentage (default 0%)
          reservedPercent = 0.0;
          std::cout << "Enter percent of data to reserve (0-100, default 0%): ";
          std::string reservedPercentStr;
          std::getline(std::cin, reservedPercentStr);
          if (!reservedPercentStr.empty()){
              try
                {
                  reservedPercent = std::stod(reservedPercentStr);
                }
              catch (...){
                  std::cerr << "Invalid input for reserved percent. Using default 5%." << std::endl;
                  reservedPercent = 5.0;
                }
            }
          reservedPercent = std::clamp(reservedPercent, 0.0, 100.0);

          // Validate that total doesn't exceed 100%
          double totalPercent = insamplePercent + outOfSamplePercent + reservedPercent;
          if (totalPercent <= 100.0){
              validPercentages = true;
            }
          else
            {
              std::cerr << "Error: Total percentage (" << totalPercent
                        << "%) exceeds 100%. Please enter the percentages again." << std::endl;
            }
        }

      // 4. Prepare output directories with timeframe differentiation
      // Holding period input
      int holdingPeriod = 1;
      std::cout << "Enter holding period (integer, default 1): ";
      std::string holdingPeriodStr;
      std::getline(std::cin, holdingPeriodStr);
      if (!holdingPeriodStr.empty()){
          try {
              holdingPeriod = std::stoi(holdingPeriodStr);
              holdingPeriod = std::max(1, holdingPeriod);
          } catch (...){
              std::cerr << "Invalid input for holding period. Using default 1." << std::endl;
              holdingPeriod = 1;
          }
      }
  
      fs::path baseDir = tickerSymbol + "_Validation";
      // Preserve existing directories and files; do not remove baseDir if it already exists
      
      // Create timeframe-specific subdirectory
      std::string timeFrameDirName = createTimeFrameDirectoryName(timeFrameStr, intradayMinutes);
      fs::path timeFrameDir = baseDir / timeFrameDirName;
      // Create Roc<holdingPeriod> subdirectory
      fs::path rocDir = timeFrameDir / ("Roc" + std::to_string(holdingPeriod));
      fs::path palDir = rocDir / "PAL_Files";
      fs::path valDir = rocDir / "Validation_Files";
      fs::create_directories(palDir);
      fs::create_directories(valDir);

      // Create risk-reward subdirectories within validation directory
      fs::path riskReward05Dir = valDir / "Risk_Reward_0_5";
      fs::path riskReward11Dir = valDir / "Risk_Reward_1_1";
      fs::path riskReward21Dir = valDir / "Risk_Reward_2_1";
      fs::create_directories(riskReward05Dir);
      fs::create_directories(riskReward11Dir);
      fs::create_directories(riskReward21Dir);

      // Create 8 subdirectories under palDir for parallel processing
      std::vector<fs::path> palSubDirs;
      for (int i = 1; i <= 8; ++i){
          fs::path subDir = palDir / ("pal_" + std::to_string(i));
          fs::create_directories(subDir);
          palSubDirs.push_back(subDir);
        }

      // 5. Create and read time series
      shared_ptr<TimeSeriesCsvReader<Num>> reader;
      if (fileType >= 1 && fileType <= 6){
   reader = createTimeSeriesReader(fileType,
       historicDataFileName,
       securityTick,
       timeFrame);
        }
      else
        {
	  throw std::out_of_range("Invalid file type");
        }

      try
        {
	  reader->readFile();
        }
      catch (const TimeSeriesException& e){
	  std::cerr << "ERROR: Data file contains duplicate timestamps." << std::endl;
	  std::cerr << "Details: " << e.what() << std::endl;
	  std::cerr << "Action: Please clean the data file and remove any duplicate entries." << std::endl;
	  std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
	  return 1;
        }
      catch (const TimeSeriesEntryException& e){
	  std::cerr << "ERROR: Data file contains invalid OHLC price relationships." << std::endl;
	  std::cerr << "Details: " << e.what() << std::endl;
	  std::cerr << "Action: Please check and correct the data file for invalid price entries." << std::endl;
	  std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
	  return 1;
        }
      auto aTimeSeries = reader->getTimeSeries();

      // --- Quantization-aware clean start detection ---

      std::optional<double> knownTick;

      // CLI override if third positional argument present
      if (args.size() == 3){
        knownTick = securityTick.getAsDouble();
      }
      // Try SecurityAttributesFactory if not provided via CLI
      if (!knownTick){
        try {
          auto attrs = mkc_timeseries::getSecurityAttributes<Num>(tickerSymbol);
          knownTick = attrs->getTick().getAsDouble();
        } catch (const mkc_timeseries::SecurityAttributesFactoryException&){
          // attributes not found; will infer from data
        }
      }

      CleanStartConfig trimCfg;
      if ((timeFrame == TimeFrame::INTRADAY) && (intradayMinutes >= 1))
	trimCfg.intradayMinutesPerBar = intradayMinutes;

      auto clean = findCleanStartIndex(*aTimeSeries, trimCfg, knownTick);
      size_t cleanStartIndex = clean.found ? clean.startIndex : 0;

      if (clean.found && cleanStartIndex > 0){
	  auto chosenDate = aTimeSeries->getEntriesCopy()[cleanStartIndex].getDateTime().date();
	  std::cout << "[Quantization-aware trim] Start index " << cleanStartIndex
		    << " (" << boost::gregorian::to_iso_extended_string(chosenDate) << ")"
		    << "  tick≈" << clean.tick
		    << "  relTick≈" << clean.relTick
		    << "  zeroFrac≈" << clean.zeroFrac << std::endl;
	  if (knownTick)
	    std::cout << "[Tick] from SecurityAttributes/CLI: " << *knownTick << std::endl;
	  else
	    std::cout << "[Tick] inferred from data: " << clean.tick << std::endl;
	}
      else
	{
	  std::ostringstream oss;
	  oss << "No clean start window found for symbol '" << tickerSymbol << "'. "
	      << "Bars=" << aTimeSeries->getNumEntries()
	      << ", windowBarsTried=" << trimCfg.windowBars
	      << ", thresholds={maxRelTick=" << trimCfg.maxRelTick
	      << ", maxZeroFrac=" << trimCfg.maxZeroFrac
	      << ", minUniqueLevels=" << trimCfg.minUniqueLevels << "}.";
	  throw std::runtime_error(oss.str());
	}


      // 6. Calculate indicator if in indicator mode
      NumericTimeSeries<Num> indicatorSeries(aTimeSeries->getTimeFrame());
      if (indicatorMode && selectedIndicator == "IBS"){
        std::cout << "Calculating Internal Bar Strength (IBS) indicator..." << std::endl;
        indicatorSeries = IBS1Series(*aTimeSeries);
        std::cout << "IBS calculation complete. Generated " << indicatorSeries.getNumEntries() << " indicator values." << std::endl;
      }

      // 7. Split into insample, out-of-sample, and reserved (last)
      //    Start counting from the quantization-aware clean index
      size_t totalSize        = aTimeSeries->getNumEntries();
      size_t usableSize       = (cleanStartIndex < totalSize) ? (totalSize - cleanStartIndex) : 0;

      size_t insampleSize     = static_cast<size_t>(usableSize * (insamplePercent / 100.0));
      size_t oosSize          = static_cast<size_t>(usableSize * (outOfSamplePercent / 100.0));
      size_t reservedSize     = static_cast<size_t>(usableSize * (reservedPercent / 100.0));

      OHLCTimeSeries<Num> reservedSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());
      OHLCTimeSeries<Num> insampleSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());
      OHLCTimeSeries<Num> outOfSampleSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());

      size_t globalIdx = 0;
      size_t usedIdx   = 0;
      for (auto it = aTimeSeries->beginSortedAccess(); it != aTimeSeries->endSortedAccess(); ++it, ++globalIdx){
          if (globalIdx < cleanStartIndex) continue; // skip early distorted years
          const auto& entry = *it;
          if (usedIdx < insampleSize)
            insampleSeries.addEntry(entry);
          else if (usedIdx < insampleSize + oosSize)
            outOfSampleSeries.addEntry(entry);
          else
            reservedSeries.addEntry(entry);
          ++usedIdx;
        }

      // Create insample indicator series if in indicator mode

      NumericTimeSeries<Num> insampleIndicatorSeries(aTimeSeries->getTimeFrame());
      if (indicatorMode && selectedIndicator == "IBS"){
        insampleIndicatorSeries = IBS1Series(insampleSeries);
        std::cout << "Generated " << insampleIndicatorSeries.getNumEntries() << " IBS values for insample data." << std::endl;
      }

      // 8. Insample stop and target calculation using robust asymmetric method
      Num profitTargetValue;
      Num stopValue;
      Num medianOfRoc;
      Num robustQn;
      Num MAD;
      Num StdDev;
      Num skew;

        
      try
        {
	  // Compute asymmetric profit target and stop values
	  auto targetStopPair = ComputeRobustStopAndTargetFromSeries(insampleSeries, holdingPeriod);
	  profitTargetValue = targetStopPair.first;
	  stopValue = targetStopPair.second;

	  // Still compute traditional statistics for reporting
	  NumericTimeSeries<Num> closingPrices(insampleSeries.CloseTimeSeries());
	  NumericTimeSeries<Num> rocOfClosingPrices(RocSeries(closingPrices, holdingPeriod));
	  medianOfRoc = Median(rocOfClosingPrices);
	  robustQn = RobustQn<Num>(rocOfClosingPrices).getRobustQn();
	  MAD = MedianAbsoluteDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
	  StdDev = StandardDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
	  skew = RobustSkewMedcouple(rocOfClosingPrices);

	  if ((robustQn * DecimalConstants<Num>::DecimalTwo) < StdDev)
	    std::cout << "***** Warning Standard Devition is > (2 * Qn) *****" << std::endl;
        }
      catch (const std::domain_error& e){
	  std::cerr << "ERROR: Intraday data contains duplicate timestamps preventing stop calculation." << std::endl;
	  std::cerr << "Details: " << e.what() << std::endl;
	  std::cerr << "Cause: NumericTimeSeries cannot handle multiple intraday bars with identical timestamps." << std::endl;
	  std::cerr << "Action: Clean the intraday data to ensure unique timestamps for each bar." << std::endl;
	  std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
	  return 1;
        }

      Num half(DecimalConstants<Num>::createDecimal("0.5"));
      
      // 8. Generate target/stop files and data files for each subdirectory
      for (const auto& currentPalDir : palSubDirs){
          // Generate target/stop files in current subdirectory using asymmetric values
          std::ofstream tsFile1((currentPalDir / (tickerSymbol + "_0_5_.TRS")).string());
          tsFile1 << (profitTargetValue * half) << std::endl << stopValue << std::endl;
          tsFile1.close();

          std::ofstream tsFile2((currentPalDir / (tickerSymbol + "_1_0_.TRS")).string());
          tsFile2 << profitTargetValue << std::endl << stopValue << std::endl;
          tsFile2.close();

          std::ofstream tsFile3((currentPalDir / (tickerSymbol + "_2_0_.TRS")).string());
          tsFile3 << (profitTargetValue * DecimalConstants<Num>::DecimalTwo) << std::endl << stopValue << std::endl;
          tsFile3.close();

          // Write data files to current subdirectory
          if (indicatorMode){
            // Write indicator-based PAL files
            if (timeFrameStr == "Intraday"){
              PalIndicatorIntradayCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries, insampleIndicatorSeries);
              insampleWriter.writeFile();
            } else {
              PalIndicatorEodCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries, insampleIndicatorSeries);
              insampleWriter.writeFile();
            }
          } else {
            // Write standard OHLC PAL files
            if (timeFrameStr == "Intraday"){
              // For intraday: insample uses PAL intraday format
              PalIntradayCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries);
              insampleWriter.writeFile();
            } else {
              // For non-intraday: all files use standard PAL EOD format
              PalTimeSeriesCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries);
              insampleWriter.writeFile();
            }
          }
        }

      // 9. Write validation files with risk-reward segregation
      // Write ALL.txt files to each risk-reward subdirectory
      std::vector<fs::path> riskRewardDirs = {riskReward05Dir, riskReward11Dir, riskReward21Dir};
      
      for (const auto& rrDir : riskRewardDirs){
          if (timeFrameStr == "Intraday"){
              TradeStationIntradayCsvWriter<Num> allWriter((rrDir / (tickerSymbol + "_ALL.txt")).string(), *aTimeSeries);
              allWriter.writeFile();
            }
          else
            {
              PalTimeSeriesCsvWriter<Num> allWriter((rrDir / (tickerSymbol + "_ALL.txt")).string(), *aTimeSeries);
              allWriter.writeFile();
            }
          
          // Write config file to each risk-reward subdirectory
          writeConfigFile(rrDir.string(), tickerSymbol, insampleSeries, outOfSampleSeries, timeFrameStr);
        }

      // Write OOS and reserved files to main validation directory
      if (timeFrameStr == "Intraday"){
          TradeStationIntradayCsvWriter<Num> oosWriter((valDir / (tickerSymbol + "_OOS.txt")).string(), outOfSampleSeries);
          oosWriter.writeFile();
          if (reservedSize > 0){
              TradeStationIntradayCsvWriter<Num> reservedWriter((valDir / (tickerSymbol + "_reserved.txt")).string(), reservedSeries);
              reservedWriter.writeFile();
            }
        }
      else
        {
          PalTimeSeriesCsvWriter<Num> oosWriter((valDir / (tickerSymbol + "_OOS.txt")).string(), outOfSampleSeries);
          oosWriter.writeFile();
          if (reservedSize > 0){
              PalTimeSeriesCsvWriter<Num> reservedWriter((valDir / (tickerSymbol + "_reserved.txt")).string(), reservedSeries);
              reservedWriter.writeFile();
            }
        }

      // 10. Output statistics
      std::cout << "In-sample% = " << insamplePercent << "%\n";
      std::cout << "Out-of-sample% = " << outOfSamplePercent << "%\n";
      std::cout << "Reserved% = " << reservedPercent << "%\n";
      std::cout << "Median = " << medianOfRoc << std::endl;
      std::cout << "Qn  = " << robustQn << std::endl;
      std::cout << "MAD = " << MAD << std::endl;
      std::cout << "Std = " << StdDev << std::endl;
      std::cout << "Profit Target = " << profitTargetValue << std::endl;
      std::cout << "Stop = " << stopValue << std::endl;
      std::cout << "Skew = " << skew << std::endl;

      fs::path detailsFilePath = valDir / (tickerSymbol + "_Palsetup_Details.txt");
      std::ofstream detailsFile(detailsFilePath);
      if (!detailsFile.is_open()){
	  std::cerr << "Error: Unable to open details file " << detailsFilePath << std::endl;
	}
      else
	{
	  detailsFile << "In-sample% = " << insamplePercent << "%\n";
	  detailsFile << "Out-of-sample% = " << outOfSamplePercent << "%\n";
	  detailsFile << "Reserved% = " << reservedPercent << "%\n";
	  detailsFile << "Median = " << medianOfRoc << std::endl;
	  detailsFile << "Qn  = " << robustQn << std::endl;
	  detailsFile << "MAD = " << MAD << std::endl;
	  detailsFile << "Std = " << StdDev << std::endl;
	  detailsFile << "Profit Target = " << profitTargetValue << std::endl;
	  detailsFile << "Stop = " << stopValue << std::endl;
	  detailsFile << "Skew = " << skew << std::endl;
	  detailsFile << "CleanStartIndex = " << cleanStartIndex << "\n";
	  if (clean.found){
	    auto chosenDate = aTimeSeries->getEntriesCopy()[cleanStartIndex].getDateTime().date();
	    detailsFile << "CleanStartDate = " << boost::gregorian::to_iso_extended_string(chosenDate) << "\n";
	    detailsFile << "InferredTick   = " << clean.tick << "\n";
	    detailsFile << "RelTick        = " << clean.relTick << "\n";
	    detailsFile << "ZeroFrac       = " << clean.zeroFrac << "\n";
	    detailsFile << "TickSource     = " << (knownTick ? "SecurityAttributes_or_CLI" : "Inferred") << "\n";
	  }

	  detailsFile.close();
	}
    }
  else
    {
      std::cout << "Usage (beta):: PalSetup [-indicator|--indicator] datafile file-type (1=CSI,2=CSI Ext,3=TradeStation,4=Pinnacle,5=PAL,6=WealthLab) [tick]" << std::endl;
      std::cout << "  -indicator|--indicator: Use indicator values (e.g., IBS) instead of close prices in PAL files" << std::endl;
    }
  return 0;
}
