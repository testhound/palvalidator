#pragma once

#include "PalSetupTypes.h"
#include "TimeSeries.h"
#include <iosfwd>

/**
 * @brief Analyzes bid/ask spreads using multiple estimation methods
 * 
 * This class centralizes the calculation of bid/ask spread estimates using:
 * - Corwin-Schultz high-low spread estimator (2-day pairs)
 * - Edge spread estimator (rolling window, default 30 days)
 * 
 * For each method, it calculates mean, median, and robust Qn statistics.
 */
class BidAskAnalyzer
{
public:
  BidAskAnalyzer();
  ~BidAskAnalyzer() = default;
  
  /**
   * @brief Analyze bid/ask spreads on a time series
   * 
   * @param series The OHLC time series to analyze (typically out-of-sample data)
   * @param securityTick The minimum tick size for the security
   * @param edgeWindowDays Rolling window size for Edge estimator (default 30)
   * @return Complete analysis results including both estimation methods
   */
  BidAskSpreadAnalysis analyzeSpreads(
    const mkc_timeseries::OHLCTimeSeries<Num>& series,
    const Num& securityTick,
    unsigned int edgeWindowDays = 30);
  
  /**
   * @brief Write spread analysis results to an output stream
   * 
   * @param outputStream Stream to write to (file or cout)
   * @param analysis The analysis results to write
   * @param verbose If true, include additional details and headers
   */
  static void writeAnalysisToStream(
    std::ostream& outputStream,
    const BidAskSpreadAnalysis& analysis,
    bool verbose = true);
  
  /**
   * @brief Display spread analysis results to console
   * 
   * @param analysis The analysis results to display
   */
  static void displayAnalysisToConsole(const BidAskSpreadAnalysis& analysis);

private:
  /**
   * @brief Calculate statistics for a vector of spread values
   * 
   * @param spreads Vector of proportional spread values
   * @param methodName Name of the estimation method
   * @return Results structure with mean, median, and Qn
   */
  SpreadEstimationResults calculateSpreadStatistics(
    const std::vector<Num>& spreads,
    const std::string& methodName);
  
  /**
   * @brief Write single method results to stream
   */
  static void writeMethodResults(
    std::ostream& outputStream,
    const SpreadEstimationResults& results,
    bool verbose);
};