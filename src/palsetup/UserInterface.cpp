#include "UserInterface.h"
#include "TimeFrameUtility.h"
#include "DecimalConstants.h"
#include "TimeSeriesIndicators.h"
#include "BootStrapIndicators.h"
#include "TimeSeriesProcessor.h"
#include "BidAskAnalyzer.h"
#include "SecurityAttributesFactory.h"
#include <iostream>
#include <filesystem>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <boost/date_time/gregorian/gregorian.hpp>

namespace fs = std::filesystem;

namespace {

// Returns the alpha used by the current stop/target method.
// For FixedAlpha returns the constant; for Calibrated* it solves α* the same way as in the compute fns.
static double ResolveAlphaForMethod(const mkc_timeseries::OHLCTimeSeries<Num>& series,
                                    uint32_t period,
                                    mkc_timeseries::StopTargetMethod method,
                                    double fixedAlpha = 0.10)
{
    using namespace mkc_timeseries;

    if (method == StopTargetMethod::TypicalDayFixedAlpha) {
        return fixedAlpha;
    }

    // Build ROC and a winsorized working copy (same policy as compute fns)
    auto rocSeries = RocSeries(series.CloseTimeSeries(), period);
    auto rocVec    = rocSeries.getTimeSeriesAsVector();
    std::vector<Num> wv = rocVec;
    if (wv.size() >= 20) WinsorizeInPlace(wv, 0.01);

    const Num median = MedianOfVec(wv);

    // Legacy baseline (for calibration) — same helper logic as compute fns
    const auto legacy = ComputeLegacyBaselineLongWidths(series, period);
    const Num  T_old  = legacy.first;

    if (method == StopTargetMethod::TypicalDayCalibratedAlpha) {
        return CalibrateAlphaForTargetWidth(wv, median, T_old, 0.06, 0.16, 25);
    }

    // TypicalDayCalibratedAsymmetric: the "band" has no single α; pick α_up* for reporting
    // (since LongTarget = q_{1-α_up*}-median, ShortStop mirrors it)
    return CalibrateAlphaForTargetWidth(wv, median, T_old, 0.06, 0.16, 25);
}
  // Prints "typical-day" diagnostics for either long (asLong=true) or short (asLong=false)
  void PrintTypicalDayDiagnostics(const std::vector<Num>& rocVec,
                                  bool asLong,
                                  double alpha = 0.10,
                                  double winsorTail = 0.01,
                                  double PF = 2.0,
                                  const std::string& indent = "   ",
                                  bool printBandHeader = true)
  {
      using namespace mkc_timeseries;
      if (rocVec.empty()) return;

      // 1) Working copy for winsorized quantiles (same policy as compute fns)
      std::vector<Num> wv = rocVec;
      if (wv.size() >= 20) {
          WinsorizeInPlace(wv, winsorTail);  // 1% per tail by default
      }

      // 2) Center & quantiles (linear interpolation)
      const Num median = MedianOfVec(wv);
      const Num q_lo   = LinearInterpolationQuantile(wv, alpha);
      const Num q_hi   = LinearInterpolationQuantile(wv, 1.0 - alpha);

      // 3) One-sided central widths
      const Num upWidth   = q_hi - median;     // typical up wiggle
      const Num downWidth = median - q_lo;     // typical down move

      const double up   = upWidth.getAsDouble();
      const double down = downWidth.getAsDouble();

      const double eps = 1e-12;
      const double CAR = up / std::max(down, eps);   // (q90 - median) / (median - q10)
      const double RWL_long  = CAR;                  // ≈ target/stop for longs
      const double RWL_short = 1.0 / std::max(CAR, eps);
      const double RWL       = asLong ? RWL_long : RWL_short;
      const double Profit    = 100.0 * PF / (PF + RWL);

      // 4) Coverage of [q10, q90] on the ORIGINAL (unwinsorized) series
      std::size_t inside = 0;
      for (const auto& r : rocVec) if (r >= q_lo && r <= q_hi) ++inside;
      const double coverage = 100.0 * static_cast<double>(inside) / std::max<std::size_t>(rocVec.size(), 1);

      // 5) CAR interpretation / classification
      const double delta   = std::fabs(CAR - 1.0);
      const double stretch = delta * 100.0;                      // % stretch vs symmetry
      const bool   upside  = (CAR > 1.0);
      const char*  strength =
          (delta < 0.05) ? "≈ symmetric (±5%)" :
          (delta < 0.15) ? "mild" :
          (delta < 0.30) ? "moderate" : "strong";

      std::ostringstream interp;
      if (upside && delta >= 0.05) {
          interp << "Upside-stretched (" << strength << "): up ≈ "
                 << std::fixed << std::setprecision(1) << stretch
                 << "% larger than down. Implications → Long: target > stop; Short: stop > target.";
      } else if (!upside && delta >= 0.05) {
          interp << "Downside-stretched (" << strength << "): down ≈ "
                 << std::fixed << std::setprecision(1) << stretch
                 << "% larger than up. Implications → Long: stop > target; Short: target > stop.";
      } else {
          interp << "Center ≈ symmetric: up ~ down. Implications → Long: target ~ stop; Short: stop ~ target.";
      }

      // 6) Print
      std::cout << std::fixed;
      if (printBandHeader) {
          std::cout << indent << "[Typical-day diagnostics]\n";
          std::cout << indent << "   alpha per tail: " << std::setprecision(2) << (alpha * 100.0)
                    << "%, band coverage ≈ " << coverage << "%\n";
          std::cout << indent << "   q10="   << std::setprecision(4) << q_lo.getAsDouble()   << "%, "
                    << "median=" << median.getAsDouble() << "%, "
                    << "q90="    << q_hi.getAsDouble()   << "%\n";
          std::cout << indent << "   UpWidth="   << std::setprecision(2) << up   << "%, "
                    << "DownWidth=" << down << "%\n";
          std::cout << indent << "   CAR = UpWidth/DownWidth = " << std::setprecision(3) << CAR
                    << "  →  " << interp.str() << "\n";
      }

      // Per-side one-liner (always print)
      std::cout << indent << "   Implied RWL (" << (asLong ? "long" : "short") << ") ≈ "
                << std::setprecision(3) << RWL
                << " | Profitability (PF=" << std::setprecision(0) << PF << ") = "
                << std::setprecision(2) << Profit << "%\n";
  }
}

UserInterface::UserInterface() : indicatorMode_(false), statsOnlyMode_(false) {}

SetupConfiguration UserInterface::parseCommandLineArgs(int argc, char** argv) {
    // Parse command line arguments for flags
    positionalArgs_.clear();
    indicatorMode_ = false;
    statsOnlyMode_ = false;
    
    // Separate flags from positional arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = std::string(argv[i]);
        if (arg == "-indicator" || arg == "--indicator") {
            indicatorMode_ = true;
        } else if (arg == "-stats-only" || arg == "--stats-only") {
            statsOnlyMode_ = true;
        } else {
            positionalArgs_.push_back(arg);
        }
    }
    
    if (positionalArgs_.size() != 2) {
        displayUsage();
        throw std::invalid_argument("Invalid number of command line arguments");
    }
    
    // Extract basic parameters from command line
    std::string historicDataFileName = positionalArgs_[0];
    int fileType = std::stoi(positionalArgs_[1]);
    
    // Extract default ticker symbol from filename
    std::string defaultTicker = extractDefaultTicker(historicDataFileName);
    
    // Look up security tick from SecurityAttributesFactory
    Num securityTick(mkc_timeseries::DecimalConstants<Num>::EquityTick);
    auto& factory = mkc_timeseries::SecurityAttributesFactory<Num>::instance();
    auto it = factory.getSecurityAttributes(defaultTicker);
    
    if (it != factory.endSecurityAttributes())
    {
      securityTick = it->second->getTick();
    }
    else
    {
      std::cout << "[Warning] Security '" << defaultTicker
                << "' not found in SecurityAttributes. Using default EquityTick: "
                << securityTick << std::endl;
    }
    
    // Display data file date range before asking for user input
    try {
        TimeSeriesProcessor tsProcessor;
        auto reader = tsProcessor.createTimeSeriesReader(
            fileType,
            historicDataFileName,
            securityTick,
            mkc_timeseries::TimeFrame::DAILY); // Use default timeframe for preview
        auto timeSeries = tsProcessor.loadTimeSeries(reader);
        
        if (timeSeries->getNumEntries() > 0) {
            auto firstDate = timeSeries->getFirstDate();
            auto lastDate = timeSeries->getLastDate();
            std::cout << "[Data Range] " << historicDataFileName
                      << " contains " << timeSeries->getNumEntries() << " entries"
                      << " from " << boost::gregorian::to_iso_extended_string(firstDate)
                      << " to " << boost::gregorian::to_iso_extended_string(lastDate) << std::endl;
        } else {
            std::cout << "[Data Range] " << historicDataFileName
                      << " contains no data entries" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[Data Range] Could not read " << historicDataFileName
                  << " - " << e.what() << std::endl;
    }
    
    // Collect user input interactively
    std::string tickerSymbol = getTickerSymbol(defaultTicker);
    auto [timeFrameStr, timeFrame] = getTimeFrameInput();
    
    int intradayMinutes = 0;
    if (timeFrameStr == "Intraday") {
        intradayMinutes = getIntradayMinutes();
    }
    
    auto [indicatorModeSelected, selectedIndicator] = getIndicatorSelection();
    auto [insamplePercent, outOfSamplePercent, reservedPercent] = getDataSplitInput();
    int holdingPeriod = getHoldingPeriodInput();
    
    return SetupConfiguration(
        tickerSymbol,
        timeFrameStr,
        timeFrame,
        fileType,
        historicDataFileName,
        securityTick,
        intradayMinutes,
        indicatorModeSelected,
        selectedIndicator,
        insamplePercent,
        outOfSamplePercent,
        reservedPercent,
        holdingPeriod,
        statsOnlyMode_
    );
}

void UserInterface::displayResults(const StatisticsResults& stats, const CleanStartResult& cleanStart) {
    std::cout << "Median = " << stats.getMedianOfRoc() << std::endl;
    std::cout << "Qn  = " << stats.getRobustQn() << std::endl;
    std::cout << "MAD = " << stats.getMAD() << std::endl;
    std::cout << "Std = " << stats.getStdDev() << std::endl;
    std::cout << "Profit Target = " << stats.getProfitTargetValue() << std::endl;
    std::cout << "Stop = " << stats.getStopValue() << std::endl;
    std::cout << "Skew = " << stats.getSkew() << std::endl;
}

void UserInterface::displaySetupSummary(const SetupConfiguration& config) {
    std::cout << "\n=== Setup Configuration ===" << std::endl;
    std::cout << "Ticker: " << config.getTickerSymbol() << std::endl;
    std::cout << "Time Frame: " << config.getTimeFrameStr();
    if (config.getTimeFrameStr() == "Intraday") {
        std::cout << " (" << config.getIntradayMinutes() << " minutes)";
    }
    std::cout << std::endl;
    std::cout << "File Type: " << config.getFileType() << std::endl;
    std::cout << "Indicator Mode: " << (config.isIndicatorMode() ? "Yes (" + config.getSelectedIndicator() + ")" : "No") << std::endl;
    std::cout << "Data Split: " << config.getInsamplePercent() << "% / "
              << config.getOutOfSamplePercent() << "% / "
              << config.getReservedPercent() << "%" << std::endl;
    std::cout << "Holding Period: " << config.getHoldingPeriod() << std::endl;
    std::cout << "=========================" << std::endl;
}

void UserInterface::displaySetupSummary(const SetupConfiguration& config,
                                       const mkc_timeseries::OHLCTimeSeries<Num>& timeSeries,
                                       size_t cleanStartIndex) {
    std::cout << "\n=== Setup Configuration ===" << std::endl;
    std::cout << "Ticker: " << config.getTickerSymbol() << std::endl;
    std::cout << "Time Frame: " << config.getTimeFrameStr();
    if (config.getTimeFrameStr() == "Intraday") {
        std::cout << " (" << config.getIntradayMinutes() << " minutes)";
    }
    std::cout << std::endl;
    std::cout << "File Type: " << config.getFileType() << std::endl;
    std::cout << "Indicator Mode: " << (config.isIndicatorMode() ? "Yes (" + config.getSelectedIndicator() + ")" : "No") << std::endl;
    
    // Calculate date ranges for data splits
    if (timeSeries.getNumEntries() > 0) {
        auto entries = timeSeries.getEntriesCopy();
        size_t totalEntries = entries.size();

        // Compute usable entries after any quantization-aware trim (cleanStartIndex)
        size_t usableEntries = (cleanStartIndex < totalEntries) ? (totalEntries - cleanStartIndex) : 0;

        // Use the same split logic as TimeSeriesProcessor::splitTimeSeries which
        // computes in-sample and out-of-sample sizes by flooring the percentage
        // of the usable bars; whatever remains is reserved.

	size_t inSampleSize = static_cast<size_t>((config.getInsamplePercent() / 100.0) * usableEntries);
        size_t reservedSize = static_cast<size_t>((config.getReservedPercent() / 100.0) * usableEntries);
        size_t outOfSampleSize = (usableEntries > (inSampleSize + reservedSize)) ?
            (usableEntries - inSampleSize - reservedSize) : 0;
	
        std::cout << "Data Split: " << config.getInsamplePercent() << "% / "
                  << config.getOutOfSamplePercent() << "% / "
                  << config.getReservedPercent() << "%" << std::endl;
        
        // Display date ranges for each split
        if (inSampleSize > 0 && cleanStartIndex < totalEntries) {
            auto inSampleStart = entries[cleanStartIndex].getDateTime().date();
            auto inSampleEnd = entries[cleanStartIndex + inSampleSize - 1].getDateTime().date();
            std::cout << "  In-Sample:     " << boost::gregorian::to_iso_extended_string(inSampleStart)
                      << " to " << boost::gregorian::to_iso_extended_string(inSampleEnd)
                      << " (" << inSampleSize << " entries)" << std::endl;
        }
        
        if (outOfSampleSize > 0 && (cleanStartIndex + inSampleSize) < totalEntries) {
            auto outOfSampleStart = entries[cleanStartIndex + inSampleSize].getDateTime().date();
            auto outOfSampleEnd = entries[cleanStartIndex + inSampleSize + outOfSampleSize - 1].getDateTime().date();
            std::cout << "  Out-of-Sample: " << boost::gregorian::to_iso_extended_string(outOfSampleStart)
                      << " to " << boost::gregorian::to_iso_extended_string(outOfSampleEnd)
                      << " (" << outOfSampleSize << " entries)" << std::endl;
        }
        
        if (reservedSize > 0 && (cleanStartIndex + inSampleSize + outOfSampleSize) < totalEntries) {
            auto reservedStart = entries[cleanStartIndex + inSampleSize + outOfSampleSize].getDateTime().date();
            auto reservedEnd = entries[cleanStartIndex + inSampleSize + outOfSampleSize + reservedSize - 1].getDateTime().date();
            std::cout << "  Reserved:      " << boost::gregorian::to_iso_extended_string(reservedStart)
                      << " to " << boost::gregorian::to_iso_extended_string(reservedEnd)
                      << " (" << reservedSize << " entries)" << std::endl;
        }
    } else {
        std::cout << "Data Split: " << config.getInsamplePercent() << "% / "
                  << config.getOutOfSamplePercent() << "% / "
                  << config.getReservedPercent() << "% (no data available)" << std::endl;
    }
    
    std::cout << "Holding Period: " << config.getHoldingPeriod() << std::endl;
    std::cout << "=========================" << std::endl;
}

std::string UserInterface::extractDefaultTicker(const std::string& filename) {
    fs::path filePath(filename);
    std::string baseName = filePath.stem().string(); // Gets filename without extension
    
    // Extract only alphabetic characters from the beginning until first non-alphabetic character
    std::string ticker;
    for (char c : baseName) {
        if (std::isalpha(c)) {
            ticker += c;
        } else {
            // Stop at first non-alphabetic character
            break;
        }
    }
    
    // If we found alphabetic characters, return them; otherwise return the whole base name
    return ticker.empty() ? baseName : ticker;
}

std::string UserInterface::getTickerSymbol(const std::string& defaultTicker) {
    std::string tickerSymbol;
    std::cout << "Enter ticker symbol [default " << defaultTicker << "]: ";
    std::getline(std::cin, tickerSymbol);
    if (tickerSymbol.empty()) {
        tickerSymbol = defaultTicker;
    }
    return tickerSymbol;
}

std::pair<std::string, mkc_timeseries::TimeFrame::Duration> UserInterface::getTimeFrameInput() {
    std::string timeFrameStr;
    mkc_timeseries::TimeFrame::Duration timeFrame;
    bool validFrame = false;
    
    while (!validFrame) {
        std::cout << "Enter time frame ([D]aily, [W]eekly, [M]onthly, [I]ntraday) [default D]: ";
        std::string tfInput;
        std::getline(std::cin, tfInput);
        if (tfInput.empty()) tfInput = "D";
        
        char c = std::toupper(tfInput[0]);
        switch (c) {
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
    
    timeFrame = mkc_timeseries::getTimeFrameFromString(timeFrameStr);
    return {timeFrameStr, timeFrame};
}

int UserInterface::getIntradayMinutes() {
    std::cout << "Enter number of minutes for intraday timeframe (1-1440, default 90): ";
    std::string minutesInput;
    std::getline(std::cin, minutesInput);
    
    int intradayMinutes = 90; // default
    if (!minutesInput.empty()) {
        try {
            intradayMinutes = std::stoi(minutesInput);
            intradayMinutes = std::clamp(intradayMinutes, 1, 1440);
        } catch (...) {
            std::cerr << "Invalid input for minutes. Using default 90." << std::endl;
            intradayMinutes = 90;
        }
    }
    return intradayMinutes;
}

std::pair<bool, std::string> UserInterface::getIndicatorSelection() {
    if (!indicatorMode_) {
        return {false, ""};
    }
    
    std::cout << "Select indicator ([I]BS - Internal Bar Strength): ";
    std::string indicatorChoice;
    std::getline(std::cin, indicatorChoice);
    if (indicatorChoice.empty()) indicatorChoice = "I";
    
    char c = std::toupper(indicatorChoice[0]);
    switch (c) {
        case 'I':
            std::cout << "Selected: Internal Bar Strength (IBS)" << std::endl;
            return {true, "IBS"};
        default:
            std::cerr << "Invalid indicator selection. Defaulting to IBS." << std::endl;
            return {true, "IBS"};
    }
}

std::tuple<double, double, double> UserInterface::getDataSplitInput() {
    double insamplePercent, outOfSamplePercent, reservedPercent;
    bool validPercentages = false;
    
    while (!validPercentages) {
        // Get in-sample percentage (default 60%)
        insamplePercent = getValidatedDoubleInput(
            "Enter percent of data for in-sample (0-100, default 60%): ", 
            60.0, 0.0, 100.0);
        
        // Get out-of-sample percentage (default 40%)
        outOfSamplePercent = getValidatedDoubleInput(
            "Enter percent of data for out-of-sample (0-100, default 40%): ", 
            40.0, 0.0, 100.0);
        
        // Get reserved percentage (default 0%)
        reservedPercent = getValidatedDoubleInput(
            "Enter percent of data to reserve (0-100, default 0%): ", 
            0.0, 0.0, 100.0);
        
        // Validate that total equals 100%
        if (validatePercentages(insamplePercent, outOfSamplePercent, reservedPercent)) {
            validPercentages = true;
        } else {
            double totalPercent = insamplePercent + outOfSamplePercent + reservedPercent;
            std::cerr << "Error: Total percentage (" << totalPercent
                      << "%) must equal 100%. Please enter the percentages again." << std::endl;
        }
    }
    
    return {insamplePercent, outOfSamplePercent, reservedPercent};
}

int UserInterface::getHoldingPeriodInput() {
    return getValidatedIntInput(
        "Enter holding period (integer, default 1): ", 
        1, 1, std::numeric_limits<int>::max());
}

std::string UserInterface::getValidatedStringInput(const std::string& prompt, const std::string& defaultValue) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    if (input.empty() && !defaultValue.empty()) {
        return defaultValue;
    }
    return input;
}

int UserInterface::getValidatedIntInput(const std::string& prompt, int defaultValue, int minValue, int maxValue) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        return defaultValue;
    }
    
    try {
        int value = std::stoi(input);
        return std::clamp(value, minValue, maxValue);
    } catch (...) {
        std::cerr << "Invalid input. Using default " << defaultValue << "." << std::endl;
        return defaultValue;
    }
}

double UserInterface::getValidatedDoubleInput(const std::string& prompt, double defaultValue, double minValue, double maxValue) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        return defaultValue;
    }
    
    try {
        double value = std::stod(input);
        return std::clamp(value, minValue, maxValue);
    } catch (...) {
        std::cerr << "Invalid input. Using default " << defaultValue << "." << std::endl;
        return defaultValue;
    }
}

bool UserInterface::validatePercentages(double inSample, double outOfSample, double reserved) {
    double total = inSample + outOfSample + reserved;
    constexpr double epsilon = 1e-9; // Tolerance for floating-point precision
    
    // Check that all percentages are non-negative and total equals 100%
    return inSample >= 0.0 &&
           outOfSample >= 0.0 &&
           reserved >= 0.0 &&
           std::abs(total - 100.0) < epsilon;
}

void UserInterface::displayUsage() {
    std::cout << "Usage: PalSetup [options] datafile file-type" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -indicator|--indicator: Use indicator values (e.g., IBS) instead of close prices" << std::endl;
    std::cout << "  -stats-only|--stats-only: Print statistics only, do not write files" << std::endl;
    std::cout << "File types: 1=CSI, 2=CSI Ext, 3=TradeStation, 4=Pinnacle, 5=PAL, 6=WealthLab" << std::endl;
}

void UserInterface::displayCleanStartInfo(const CleanStartResult& cleanStart,
                                         const std::string& tickerSymbol,
                                         const mkc_timeseries::OHLCTimeSeries<Num>& series,
                                         std::optional<double> knownTick) {
    if (cleanStart.isFound() && cleanStart.getStartIndex() > 0) {
        auto chosenDate = series.getEntriesCopy()[cleanStart.getStartIndex()].getDateTime().date();
        std::cout << "[Quantization-aware trim] Start index " << cleanStart.getStartIndex()
                  << " (" << boost::gregorian::to_iso_extended_string(chosenDate) << ")"
                  << "  tick≈" << cleanStart.getTick()
                  << "  relTick≈" << cleanStart.getRelTick()
                  << "  zeroFrac≈" << cleanStart.getZeroFrac() << std::endl;
        if (knownTick) {
            std::cout << "[Tick] from SecurityAttributes/CLI: " << *knownTick << std::endl;
        } else {
            std::cout << "[Tick] inferred from data: " << cleanStart.getTick() << std::endl;
        }
    } else {
        std::ostringstream oss;
        oss << "No clean start window found for symbol '" << tickerSymbol << "'. "
            << "Bars=" << series.getNumEntries();
        throw std::runtime_error(oss.str());
    }
}

void UserInterface::displayStatisticsOnly(const mkc_timeseries::OHLCTimeSeries<Num>& inSampleSeries,
                                          const mkc_timeseries::OHLCTimeSeries<Num>& outOfSampleSeries,
                                          const SetupConfiguration& config)
{
    using namespace mkc_timeseries;

    const uint32_t period = static_cast<uint32_t>(config.getHoldingPeriod());

    std::cout << "\n=== Statistics-Only Analysis ===\n";
    std::cout << "Ticker: "      << config.getTickerSymbol()    << "\n";
    std::cout << "Time Frame: "  << config.getTimeFrameStr()     << "\n";
    std::cout << "In-Sample Bars: " << inSampleSeries.getNumEntries() << "\n";
    std::cout << "Holding Period: " << period << "\n";
    std::cout << "=================================\n";

    try {
        // Base ROC for diagnostics/summary (same horizon)
        auto rocSeries = RocSeries(inSampleSeries.CloseTimeSeries(), period);
        auto rocVec    = rocSeries.getTimeSeriesAsVector();

        // Compute stop/target via the new bootstrapped methods
        auto [longProfit,  longStop ] = ComputeBootStrappedLongStopAndTarget (inSampleSeries, period);
        auto [shortProfit, shortStop] = ComputeBootStrappedShortStopAndTarget(inSampleSeries, period);

        // Determine the method you’re using (if you pass it in; otherwise default)
        const StopTargetMethod method = kDefaultStopTargetMethod;

        // Resolve the effective alpha for this series/method
        const double alpha_used = ResolveAlphaForMethod(inSampleSeries, period, method);

        // 1) Typical-day band & diagnostics (computed once; helper prints band, CAR, RWL, Profitability)
        std::cout << "\n1. Typical-day band & diagnostics (q10 / median / q90):\n";
        PrintTypicalDayDiagnostics(rocVec, /*asLong=*/true,  /*alpha=*/alpha_used, /*winsor=*/0.01, /*PF=*/2.0, "   ", /*printBandHeader=*/true);
        PrintTypicalDayDiagnostics(rocVec, /*asLong=*/false, /*alpha=*/alpha_used, /*winsor=*/0.01, /*PF=*/2.0, "   ", /*printBandHeader=*/false);

        // 2) Long/Short widths (concise)
        std::cout << "\n2. Long Position Stop and Target (Typical-day q10/median/q90):\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "   Profit Target Width: " << longProfit.getAsDouble()  << "%\n";
        std::cout << "   Stop   Loss   Width: " << longStop.getAsDouble()    << "%\n";

        std::cout << "\n3. Short Position Stop and Target (Typical-day q10/median/q90):\n";
        std::cout << "   Profit Target Width: " << shortProfit.getAsDouble() << "%\n";
        std::cout << "   Stop   Loss   Width: " << shortStop.getAsDouble()   << "%\n";

        // 4) Summary table
        std::cout << "\n=== Summary Comparison ===\n";
        std::cout << "Position Type             | Profit Target | Stop Loss | Method\n";
        std::cout << "--------------------------|---------------|-----------|-------------------------\n";
        std::cout << "Long Position             |        " << longProfit.getAsDouble()
                  << "% |    " << longStop.getAsDouble()
                  << "% | Typical-day (q10–q90)\n";
        std::cout << "Short Position            |        " << shortProfit.getAsDouble()
                  << "% |    " << shortStop.getAsDouble()
                  << "% | Typical-day (q10–q90)\n";

        // 5) Data summary
        const Num zero = DecimalConstants<Num>::DecimalZero;
        std::size_t posCount = 0, negCount = 0;
        for (const auto& roc : rocVec) {
            if (roc > zero) ++posCount;
            else if (roc < zero) ++negCount;
        }
        std::cout << "\n=== Data Summary ===\n";
        std::cout << std::setprecision(0);
        std::cout << "Total ROC observations:   " << rocVec.size() << "\n";
        std::cout << std::setprecision(1);
        std::cout << "Positive ROC count:       " << posCount << " (" << (100.0 * posCount / std::max<std::size_t>(rocVec.size(),1)) << "%)\n";
        std::cout << "Negative ROC count:       " << negCount << " (" << (100.0 * negCount / std::max<std::size_t>(rocVec.size(),1)) << "%)\n";

    }
    catch (const std::domain_error& e) {
        std::cerr << "\nData Error: " << e.what() << "\n";
        std::cerr << "Suggestion: Ensure sufficient data for analysis (minimum ~25 bars recommended)\n";
    }
    catch (const std::exception& e) {
        std::cerr << "\nError calculating statistics: " << e.what() << "\n";
    }

    // Transaction cost analysis (unchanged)
    std::cout << "\n=== Transaction Cost Analysis ===\n";
    BidAskAnalyzer analyzer;
    auto spreadAnalysis = analyzer.analyzeSpreads(outOfSampleSeries, config.getSecurityTick());
    BidAskAnalyzer::displayAnalysisToConsole(spreadAnalysis);

    std::cout << "\n=================================\n";
    std::cout << "Note: All values are percentage widths from the median (center) of in-sample ROC.\n";
}

void UserInterface::displaySeparateResults(const CombinedStatisticsResults& stats,
                                          const CleanStartResult& cleanStart,
                                          const BidAskSpreadAnalysis& spreadAnalysis) {
    const auto& longResults = stats.getLongResults();
    const auto& shortResults = stats.getShortResults();
    
    std::cout << "\n2. Long Position Stop and Target (Typical-day q10/median/q90):" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "   Statistics - Pos: Med=" << longResults.getPosMedian().getAsDouble() << "%, Qn=" << longResults.getPosQn().getAsDouble() << "%, Skew=" << longResults.getPosSkew().getAsDouble() << std::endl;
    std::cout << "                Neg: Med=" << longResults.getNegMedian().getAsDouble() << "%, Skew=" << longResults.getNegSkew().getAsDouble() << std::endl;
    std::cout << std::setprecision(2);
    std::cout << "   Profit Target Width: " << longResults.getProfitTargetValue().getAsDouble() << "%" << std::endl;
    std::cout << "   Stop Loss Width:     " << longResults.getStopValue().getAsDouble() << "%" << std::endl;
    
    std::cout << "\n3. Short Position Stop and Target (Typical-day q10/median/q90):" << std::endl;
    std::cout << std::setprecision(4);
    std::cout << "   Statistics - Neg: Med=" << shortResults.getNegMedian().getAsDouble() << "%, Qn=" << shortResults.getNegQn().getAsDouble() << "%, Skew=" << shortResults.getNegSkew().getAsDouble() << std::endl;
    std::cout << "                Pos: Med=" << shortResults.getPosMedian().getAsDouble() << "%, Skew=" << shortResults.getPosSkew().getAsDouble() << std::endl;
    std::cout << std::setprecision(2);
    std::cout << "   Profit Target Width: " << shortResults.getProfitTargetValue().getAsDouble() << "%" << std::endl;
    std::cout << "   Stop Loss Width:     " << shortResults.getStopValue().getAsDouble() << "%" << std::endl;
    
    // Summary comparison
    std::cout << "\n=== Summary Comparison ===" << std::endl;
    std::cout << "Position Type             | Profit Target | Stop Loss | Data Partition" << std::endl;
    std::cout << "--------------------------|---------------|-----------|------------------" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Long Position             |        " << longResults.getProfitTargetValue().getAsDouble() << "% |    " << longResults.getStopValue().getAsDouble() << "% | Pos/Neg (" << longResults.getPosCount() << "/" << longResults.getNegCount() << ")" << std::endl;
    std::cout << "Short Position            |        " << shortResults.getProfitTargetValue().getAsDouble() << "% |    " << shortResults.getStopValue().getAsDouble() << "% | Neg/Pos (" << shortResults.getNegCount() << "/" << shortResults.getPosCount() << ")" << std::endl;
    
    // Additional summary statistics
    size_t totalObs = longResults.getPosCount() + longResults.getNegCount();
    std::cout << "\n=== Data Summary ===" << std::endl;
    std::cout << "Total ROC observations:   " << totalObs << std::endl;
    std::cout << std::setprecision(1);
    std::cout << "Positive ROC count:       " << longResults.getPosCount() << " (" << (100.0 * longResults.getPosCount() / totalObs) << "%)" << std::endl;
    std::cout << "Negative ROC count:       " << longResults.getNegCount() << " (" << (100.0 * longResults.getNegCount() / totalObs) << "%)" << std::endl;
    
    // Display bid/ask spread analysis
    if (spreadAnalysis.isValid)
    {
        BidAskAnalyzer::displayAnalysisToConsole(spreadAnalysis);
    }
    
    std::cout << "\n=================================" << std::endl;
    std::cout << "Note: All values are percentage widths from median/center point." << std::endl;
}
