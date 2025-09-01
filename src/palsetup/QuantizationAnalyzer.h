#pragma once

#include "PalSetupTypes.h"
#include "TimeSeries.h"
#include "TimeFrame.h"
#include <vector>
#include <optional>
#include <iterator>

/**
 * @brief Handles quantization-aware analysis and clean start detection for time series data
 */
class QuantizationAnalyzer {
public:
    QuantizationAnalyzer();
    ~QuantizationAnalyzer() = default;
    
    /**
     * @brief Estimate the effective price tick from the data (not the exchange rule).
     * 
     * This infers the smallest price increment (grid step) that your stored prices
     * adhere to—useful when split-adjusted equity data was rounded to a coarse grid
     * (e.g., 2 decimals) or when futures/FX have fractional ticks.
     */
    template <typename Decimal>
    double estimateEffectiveTick(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                                int maxDecimals = 6,
                                double integralThreshold = 0.98);

    /**
     * @brief Finds a suitable starting index for a time series to mitigate data quantization issues.
     * 
     * Acts like an "auto-focus" for your data, finding the first point where the data is
     * "sharp" enough for analysis—where the price is high enough that the tick size
     * is no longer causing significant distortion.
     */
    template <typename Decimal>
    CleanStartResult findCleanStartIndex(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                                        const CleanStartConfig& cfg,
                                        std::optional<double> knownTick);

    /**
     * @brief Determines the appropriate window and buffer sizes based on time frame.
     */
    WindowParameters determineWindowParameters(mkc_timeseries::TimeFrame::Duration timeFrame, 
                                             size_t seriesTotalBars, 
                                             int intradayMinutes);

private:
    /**
     * @brief Estimates the effective price tick from a range of price data.
     * 
     * Core implementation for tick estimation. Finds the smallest power-of-ten 
     * scaling factor that makes most prices look like integers, then computes 
     * the GCD of differences between unique integer levels.
     */
    double estimateTickFromRange(std::vector<double>::const_iterator begin,
                               std::vector<double>::const_iterator end,
                               int maxDecimals = 8,
                               double integralThreshold = 0.95);
    
    /**
     * @brief Calculates the relative tick size for a window of prices.
     * 
     * Computes the median of prices and returns the ratio of tick to median price.
     * A high value indicates significant quantization.
     */
    double calculateRelativeTick(std::vector<double>& window_prices, double tick);
    
    /**
     * @brief Counts the number of unique price levels on a specified tick grid.
     * 
     * Helps ensure there is enough price variation in the window for meaningful analysis.
     * Low unique levels suggest a "stuck" or heavily quantized market.
     */
    size_t countUniquePriceLevels(const std::vector<double>& window_prices, double tick);
    
    /**
     * @brief Calculates the fraction of "zero returns" in a window.
     * 
     * A zero return is a price change smaller than or equal to one effective tick.
     * High fraction indicates price frequently not moving enough to overcome quantization.
     */
    double calculateZeroReturnFraction(const std::vector<double>& window_prices, double tick);
    
    /**
     * @brief Computes intraday bars per day based on minutes per bar.
     */
    int computeIntradayBarsPerDay(int minutesPerBar);
};

// Template implementations
template <typename Decimal>
double QuantizationAnalyzer::estimateEffectiveTick(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                                                   int maxDecimals,
                                                   double integralThreshold) {
    // Extract all valid close prices into a vector of doubles
    const auto& entries = series.getEntriesCopy();
    std::vector<double> closes;
    closes.reserve(entries.size());
    for (const auto& e : entries) {
        const double x = e.getCloseValue().getAsDouble();
        if (std::isfinite(x)) closes.push_back(x);
    }

    if (closes.size() < 2)
        return 1e-2; // Fallback for tiny series

    // Delegate to the core range-based implementation
    return estimateTickFromRange(closes.cbegin(), closes.cend(), maxDecimals, integralThreshold);
}

template <typename Decimal>
CleanStartResult QuantizationAnalyzer::findCleanStartIndex(const mkc_timeseries::OHLCTimeSeries<Decimal>& series,
                                                          const CleanStartConfig& cfg,
                                                          std::optional<double> knownTick) {
    using namespace mkc_timeseries;

    const auto entries = series.getEntriesCopy();
    const size_t n = entries.size();

    auto params = determineWindowParameters(series.getTimeFrame(), n, cfg.getIntradayMinutesPerBar());
    
    // Create adjusted config using the determined window parameters
    // This matches the original behavior: cfg.windowBars = params.windowBars
    CleanStartConfig adjustedCfg(
        params.getWindowBars(),           // windowBars from params
        params.getStabilityBufferBars(),  // stabilityBufferBars from params
        cfg.getMaxRelTick(),              // keep original maxRelTick
        cfg.getMaxZeroFrac(),             // keep original maxZeroFrac
        cfg.getMinUniqueLevels(),         // keep original minUniqueLevels
        cfg.getIntradayMinutesPerBar()    // keep original intradayMinutesPerBar
    );
    
    if (n < static_cast<size_t>(adjustedCfg.getWindowBars())) {
        return CleanStartResult(0, 0.01, 0.0, 0.0, false);
    }
    
    // Extract all close prices into a single vector for efficient slicing
    std::vector<double> all_closes;
    all_closes.reserve(n);
    for (const auto& entry : entries) {
        all_closes.push_back(entry.getCloseValue().getAsDouble());
    }

    const size_t W = static_cast<size_t>(adjustedCfg.getWindowBars());

    // Slide a window across the data and test each for "cleanliness"
    for (size_t L = 0, R = W - 1; R < n; ++L, ++R) {
        auto first = all_closes.cbegin() + L;
        auto last = all_closes.cbegin() + R + 1;
        
        // 1. Determine the effective tick for this specific window
        double winTick = estimateTickFromRange(first, last);

        if (knownTick && *knownTick > 0.0) {
            // Use the finer of the known tick vs. the locally-inferred one
            winTick = std::min(winTick, *knownTick);
        }
        
        // Create a temporary vector for the window's data to pass to helpers
        std::vector<double> window_prices(first, last);

        // 2. Calculate the three quality metrics using dedicated helpers
        const double relTick = calculateRelativeTick(window_prices, winTick);
        const double zeroFrac = calculateZeroReturnFraction(window_prices, winTick);
        const size_t uniqueLevels = countUniquePriceLevels(window_prices, winTick);
        
        // 3. Check if the window meets all quality criteria
        const bool is_clean = (relTick <= adjustedCfg.getMaxRelTick()) &&
                              (zeroFrac <= adjustedCfg.getMaxZeroFrac()) &&
                              (static_cast<int>(uniqueLevels) >= adjustedCfg.getMinUniqueLevels());

        if (is_clean) {
            const size_t bufferedStart = L + static_cast<size_t>(std::max(0, adjustedCfg.getStabilityBufferBars()));
            const size_t finalStartIndex = std::min(n - 1, bufferedStart);
            
            return CleanStartResult(finalStartIndex, winTick, relTick, zeroFrac, true);
        }
    }

    // No qualifying window was found
    return CleanStartResult(0, 0.01, 0.0, 0.0, false);
}