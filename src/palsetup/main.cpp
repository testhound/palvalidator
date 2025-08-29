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

namespace
{
  namespace _detail
  {

    /**
     * @brief Estimates the effective price tick from a range of price data.
     *
     * This is the core implementation for tick estimation. It finds the smallest
     * power-of-ten scaling factor that makes most prices in the range look like
     * integers, then computes the GCD of the differences between unique integer levels.
     * This reveals the underlying price grid quantization.
     *
     * @param begin Iterator to the beginning of the price range.
     * @param end Iterator to the end of the price range.
     * @param maxDecimals Maximum decimal scaling exponent to try (e.g., 6 for 10^-6).
     * @param integralThreshold Fraction of points that must be near-integral to accept a scale.
     * @return The estimated effective tick size in price units.
     */
    double estimateTickFromRange(std::vector<double>::const_iterator begin,
				 std::vector<double>::const_iterator end,
				 int maxDecimals = 8,
				 double integralThreshold = 0.95)
    {
      std::vector<double> prices;
      for (auto it = begin; it != end; ++it)
	{
	  if (std::isfinite(*it))
	    prices.push_back(*it);
	}

      if (prices.size() < 2)
	return 1e-2; // Fallback for insufficient data

      // Helper to check if a value is very close to an integer
      auto looks_integral = [](double y) {
	double tol = std::max(1e-8, std::fabs(y) * 1e-12);
	return std::fabs(y - std::llround(y)) < tol;
      };

      // 1) Find smallest 10^k scale where most points look integral
      int bestK = 2; // Pragmatic fallback (pennies)
      for (int k = 0; k <= maxDecimals; ++k)
	{
	  const double scale = std::pow(10.0, k);
	  int ok_count = 0;
	  for (double x : prices)
	    {
	      if (looks_integral(x * scale))
		++ok_count;
	    }
	  if (static_cast<double>(ok_count) >= integralThreshold * static_cast<double>(prices.size()))
	    {
	      bestK = k;
	      break;
	    }
	}

      const double scale = std::pow(10.0, bestK);
      const double fallbackTick = std::pow(10.0, -bestK);

      // 2) Quantize to integers and get unique sorted levels
      std::vector<long long> levels;
      levels.reserve(prices.size());
      for (double x : prices)
	levels.push_back(std::llround(x * scale));

      if (levels.size() < 2) return fallbackTick;

      std::sort(levels.begin(), levels.end());
      levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
  
      if (levels.size() < 2) return fallbackTick;

      // 3) Compute GCD of positive adjacent differences
      long long g = 0;
      for (size_t i = 1; i < levels.size(); ++i)
	{
	  const long long diff = levels[i] - levels[i - 1];
	  if (diff > 0)
	    g = (g == 0) ? diff : std::gcd(g, diff);
	}
      if (g <= 0) g = 1; // Should not happen with >1 unique levels, but as a safeguard

      // 4) Convert GCD back to price units
      return static_cast<double>(g) / scale;
    }


    /**
     * @brief Calculates the relative tick size for a window of prices.
     *
     * It computes the median of the prices and then returns the ratio of the
     * tick to the median price. A high value indicates significant quantization.
     *
     * @param window_prices Vector of prices in the current window.
     * @param tick The effective tick size to use for the calculation.
     * @return The relative tick size (tick / median). Returns infinity if median is zero.
     */
    double calculateRelativeTick(std::vector<double>& window_prices, double tick)
    {
      if (window_prices.empty() || tick <= 0.0)
        return std::numeric_limits<double>::infinity();

      // In-place median calculation
      size_t m = window_prices.size() / 2;
      std::nth_element(window_prices.begin(), window_prices.begin() + m, window_prices.end());
      double med = window_prices[m];

      // For even-sized vectors, average the two middle elements
      if ((window_prices.size() % 2) == 0) {
	double prev_max = *std::max_element(window_prices.begin(), window_prices.begin() + m);
	med = 0.5 * (med + prev_max);
      }
    
      return (med > 0.0 && std::isfinite(med)) ? (tick / med) : std::numeric_limits<double>::infinity();
    }


    /**
     * @brief Counts the number of unique price levels on a specified tick grid.
     *
     * This metric helps ensure there is enough price variation in the window for
     * meaningful analysis. Low unique levels suggest a "stuck" or heavily quantized market.
     *
     * @param window_prices Vector of prices in the current window.
     * @param tick The effective tick size to define the price grid.
     * @return The count of unique quantized price levels.
     */
    size_t countUniquePriceLevels(const std::vector<double>& window_prices, double tick)
    {
      if (tick <= 0.0) return 0;
    
      const double invTick = 1.0 / tick;
      std::unordered_set<long long> unique_levels;
      unique_levels.reserve(window_prices.size());

      for(double price : window_prices) {
        if (std::isfinite(price)) {
	  unique_levels.insert(std::llround(price * invTick));
        }
      }
      return unique_levels.size();
    }

    /**
     * @brief Calculates the fraction of "zero returns" in a window.
     *
     * A zero return is a price change smaller than or equal to one effective tick.
     * A high fraction indicates that the price is frequently not moving enough to
     * overcome the quantization grid, which can distort volatility and other statistics.
     *
     * @param window_prices Vector of prices in the current window.
     * @param tick The effective tick size to define a "zero" move.
     * @return The fraction of returns that are effectively zero.
     */
    double calculateZeroReturnFraction(const std::vector<double>& window_prices, double tick)
    {
      if (window_prices.size() < 2) return 1.0;

      int zero_moves = 0;
      int total_moves = 0;

      for (size_t i = 1; i < window_prices.size(); ++i) {
        const double p_prev = window_prices[i-1];
        const double p_curr = window_prices[i];

        if (std::isfinite(p_prev) && std::isfinite(p_curr)) {
	  if (std::fabs(p_curr - p_prev) <= tick) {
	    ++zero_moves;
	  }
	  ++total_moves;
        }
      }
      return (total_moves > 0) ? (static_cast<double>(zero_moves) / total_moves) : 1.0;
    }

  } // namespace _detail
} // anonymous namespace


/**
 * @brief Estimate the *effective* price tick from the data (not the exchange rule).
 *
 * This infers the smallest price increment (grid step) that your *stored* prices
 * adhere to—useful when split-adjusted equity data was rounded to a coarse grid
 * (e.g., 2 decimals) or when futures/FX have fractional ticks. The returned tick
 * reflects the quantization actually present in the series, which is what affects
 * zero-returns and rounding artifacts in your statistics.
 *
 * @tparam Decimal Numeric wrapper used by your time-series (e.g., mkc::Num).
 * @param  series Input OHLC time series.
 * @param  maxDecimals Maximum scale exponent to try (default 6 → micro precision).
 * @param  integralThreshold Fraction of points that must look integral at a scale.
 * @return double Estimated effective tick size (price units).
 */
template <typename Decimal>
double estimateEffectiveTick(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                             int maxDecimals = 6,
                             double integralThreshold = 0.98)
{
  // Extract all valid close prices into a vector of doubles
  const auto& entries = series.getEntriesCopy();
  std::vector<double> closes;
  closes.reserve(entries.size());
  for (const auto& e : entries)
  {
    const double x = e.getCloseValue().getAsDouble();
    if (std::isfinite(x)) closes.push_back(x);
  }

  if (closes.size() < 2)
    return 1e-2; // Fallback for tiny series

  // Delegate to the core range-based implementation
  return _detail::estimateTickFromRange(closes.cbegin(), closes.cend(), maxDecimals, integralThreshold);
}

struct WindowParameters {
  int windowBars;
  int stabilityBufferBars;
};

struct CleanStartResult {
  size_t startIndex = 0;
  double tick       = 0.01;
  double relTick    = 0.0;
  double zeroFrac   = 0.0;
  bool   found      = false;
};

/**
 * @brief Determines the appropriate window and buffer sizes based on time frame.
 * @param timeFrame The time frame of the series (Daily, Weekly, etc.).
 * @param seriesTotalBars The total number of bars in the series (for intraday scaling).
 * @param intradayMinutes The bar cadence for intraday time frames.
 * @return A WindowParameters struct containing the calculated sizes.
 */
WindowParameters determineWindowParameters(TimeFrame::Duration timeFrame, size_t seriesTotalBars, int intradayMinutes)
{
  switch (timeFrame)
    {
    case TimeFrame::DAILY:
      return {252, 60};

    case TimeFrame::WEEKLY:
      return {260, 12};
      
    case TimeFrame::MONTHLY:
      return {60, 3};

    case TimeFrame::INTRADAY:
    default:
      {
	const int barsPerDay = computeIntradayBarsPerDay(intradayMinutes);
	int desiredDays = 252;
	// Gracefully shrink window for shorter intraday series
	while (barsPerDay * desiredDays >= static_cast<int>(seriesTotalBars) && desiredDays > 21)
	  desiredDays /= 2;
	
	int window = std::max(3, barsPerDay * desiredDays);
	int buffer = std::max(60, barsPerDay * 10);
	return {window, buffer};
      }
    }
}

/**
 * @brief Finds a suitable starting index for a time series to mitigate data quantization issues.
 *
 * **The Problem**: After many stock splits, a security's price can become very low.
 * When this happens, the minimum price increment (the "tick", e.g., $0.01) becomes
 * large relative to the price. Think of it like a photograph that is so "zoomed in"
 * that it becomes pixelated. A price of $0.04 can only move in huge 25% jumps
 * ($0.01 / $0.04), which distorts statistical analysis.
 *
 * **The Solution**: This function acts like an "auto-focus" for your data. It scans
 * the time series using a sliding window to find the first point where the data is
 * "sharp" enough for analysis—that is, where the price is high enough that the tick
 * size is no longer causing significant distortion.
 *
 * The algorithm finds the first window of data that meets three criteria:
 * 1.  **Low Relative Tick**: The tick size, relative to the median price, is small.
 * 2.  **Low Zero-Return Fraction**: The price is moving more than one tick on most bars.
 * 3.  **Sufficient Price Variation**: There are many unique price levels in the window.
 *
 * **Stability Buffer**: Once a "clean" window is found starting at index `L`, we don't
 * immediately return `L`. Instead, we add a `stabilityBufferBars` safety margin.
 * This ensures the clean period isn't a brief anomaly and that our analysis starts
 * well within the stable regime.
 *
 * @tparam Decimal The numeric type used for the time series data (e.g., Num, Decimal).
 * @param series The input OHLCTimeSeries to analyze.
 * @param cfg The configuration struct (`CleanStartConfig`) defining the parameters.
 * @param knownTick An optional known tick size. If provided, it's used as an upper
 * bound for the window-inferred tick.
 * @return A `CleanStartResult` struct with the found index and related metrics.
 */
template <typename Decimal>
CleanStartResult findCleanStartIndex(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                                     CleanStartConfig cfg,
                                     std::optional<double> knownTick)
{
  using namespace mkc_timeseries;

  const auto entries = series.getEntriesCopy();
  const size_t n = entries.size();

  auto params = determineWindowParameters(series.getTimeFrame(), n, cfg.intradayMinutesPerBar);
  cfg.windowBars = params.windowBars;
  cfg.stabilityBufferBars = params.stabilityBufferBars;
      
  CleanStartResult res;
  if (n < static_cast<size_t>(cfg.windowBars))
    {
      res.found = false;
      res.startIndex = 0;
      return res;
    }
  
  // Extract all close prices into a single vector for efficient slicing
  std::vector<double> all_closes;
  all_closes.reserve(n);
  for (const auto& entry : entries) {
      all_closes.push_back(entry.getCloseValue().getAsDouble());
  }

  const size_t W = static_cast<size_t>(cfg.windowBars);

  // --- Slide a window across the data and test each for "cleanliness" ---
  for (size_t L = 0, R = W - 1; R < n; ++L, ++R)
  {
    auto first = all_closes.cbegin() + L;
    auto last = all_closes.cbegin() + R + 1;
    
    // 1. Determine the effective tick for this specific window
    double winTick = _detail::estimateTickFromRange(first, last);

    if (knownTick && *knownTick > 0.0)
      {
	// Use the finer of the known tick vs. the locally-inferred one.
	// The inferred one can be larger due to post-split rounding.

	winTick = std::min(winTick, *knownTick);
      }
    
    // Create a temporary vector for the window's data to pass to helpers
    std::vector<double> window_prices(first, last);

    // 2. Calculate the three quality metrics using dedicated helpers
    const double relTick = _detail::calculateRelativeTick(window_prices, winTick);
    const double zeroFrac = _detail::calculateZeroReturnFraction(window_prices, winTick);
    const size_t uniqueLevels = _detail::countUniquePriceLevels(window_prices, winTick);
    
    // 3. Check if the window meets all quality criteria
    const bool is_clean = (relTick <= cfg.maxRelTick) &&
                          (zeroFrac <= cfg.maxZeroFrac) &&
                          (static_cast<int>(uniqueLevels) >= cfg.minUniqueLevels);

    if (is_clean)
      {
	const size_t bufferedStart = L + static_cast<size_t>(std::max(0, cfg.stabilityBufferBars));
      
	res.startIndex = std::min(n - 1, bufferedStart);
	res.tick       = winTick;
	res.relTick    = relTick;
	res.zeroFrac   = zeroFrac;
	res.found      = true;
	return res;
      }
  }

  // No qualifying window was found
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
