#include "BidAskAnalyzer.h"
#include "BidAskSpread.h"
#include "StatUtils.h"
#include "TimeSeriesIndicators.h"
#include "DecimalConstants.h"
#include <iostream>
#include <iomanip>

BidAskAnalyzer::BidAskAnalyzer() = default;

BidAskSpreadAnalysis BidAskAnalyzer::analyzeSpreads(const mkc_timeseries::OHLCTimeSeries<Num>& series,
						    const Num& securityTick,
						    unsigned int edgeWindowDays)
{
  BidAskSpreadAnalysis analysis;
  analysis.totalEntries = series.getNumEntries();
  
  try
  {
    // Check for sufficient data
    if (series.getNumEntries() < 2)
    {
      analysis.errorMessage = "Insufficient data for spread calculation (need at least 2 entries)";
      analysis.isValid = false;
      return analysis;
    }
    
    // Calculate Corwin-Schultz spreads
    using CorwinSchultzCalc = mkc_timeseries::CorwinSchultzSpreadCalculator<Num>;
    auto corwinSchultzSpreads = CorwinSchultzCalc::calculateProportionalSpreadsVector(
      series,
      securityTick,
      CorwinSchultzCalc::NegativePolicy::Epsilon);
    
    if (!corwinSchultzSpreads.empty())
    {
      analysis.corwinSchultz = calculateSpreadStatistics(
        corwinSchultzSpreads, "Corwin-Schultz");
    }
    
    // Calculate Edge spreads
    using EdgeCalc = mkc_timeseries::EdgeSpreadCalculator<Num>;
    auto edgeSpreads = EdgeCalc::calculateProportionalSpreadsVector(
      series,
      edgeWindowDays,
      securityTick,
      EdgeCalc::NegativePolicy::Epsilon);
    
    if (!edgeSpreads.empty())
    {
      analysis.edge = calculateSpreadStatistics(
        edgeSpreads, "Edge (30-day window)");
    }
    
    analysis.isValid = (analysis.corwinSchultz.hasResults || analysis.edge.hasResults);
    
  }
  catch (const std::exception& e)
  {
    analysis.errorMessage = std::string("Error in spread analysis: ") + e.what();
    analysis.isValid = false;
  }
  
  return analysis;
}

SpreadEstimationResults BidAskAnalyzer::calculateSpreadStatistics(const std::vector<Num>& spreads,
								  const std::string& methodName)
{
  SpreadEstimationResults results;
  results.methodName = methodName;
  results.measurementCount = spreads.size();
  
  if (spreads.empty())
  {
    results.hasResults = false;
    return results;
  }
  
  // Calculate mean
  results.meanSpread = mkc_timeseries::StatUtils<Num>::computeMean(spreads);
  
  // Calculate median
  results.medianSpread = mkc_timeseries::MedianOfVec(spreads);
  
  // Calculate robust Qn
  mkc_timeseries::RobustQn<Num> qnCalc;
  results.robustQnSpread = qnCalc.getRobustQn(spreads);
  
  results.hasResults = true;
  return results;
}

void BidAskAnalyzer::writeAnalysisToStream(std::ostream& outputStream,
					   const BidAskSpreadAnalysis& analysis,
					   bool verbose)
{
  if (verbose)
  {
    outputStream << "\n=== Bid/Ask Spread Analysis (Out-of-Sample) ===" << std::endl;
    outputStream << "Out-of-sample entries: " << analysis.totalEntries << std::endl;
  }
  
  if (!analysis.isValid)
  {
    outputStream << (verbose ? "Warning: " : "") << analysis.errorMessage << std::endl;
    if (verbose)
    {
      outputStream << "=== End Bid/Ask Spread Analysis ===" << std::endl;
    }
    return;
  }
  
  // Write Corwin-Schultz results
  if (analysis.corwinSchultz.hasResults)
  {
    writeMethodResults(outputStream, analysis.corwinSchultz, verbose);
  }
  else
  {
    outputStream << "\nCorwin-Schultz: No valid spread calculations could be performed" << std::endl;
  }
  
  // Write Edge results
  if (analysis.edge.hasResults)
  {
    writeMethodResults(outputStream, analysis.edge, verbose);
  }
  else
  {
    outputStream << "\nEdge: No valid spread calculations could be performed" << std::endl;
  }
  
  if (verbose)
  {
    outputStream << "\n(Note: Current slippage estimate assumption: 0.10%)" << std::endl;
    outputStream << "=== End Bid/Ask Spread Analysis ===" << std::endl;
  }
}

void BidAskAnalyzer::writeMethodResults(std::ostream& outputStream,
					const SpreadEstimationResults& results,
					bool verbose)
{
  outputStream << "\n" << results.methodName << " Spread Estimator:" << std::endl;
  outputStream << "  Calculated " << results.measurementCount 
               << " spread measurements" << std::endl;
  outputStream << "  Mean: " << results.getMeanPercent() << "%" << std::endl;
  outputStream << "  Median: " << results.getMedianPercent() << "%" << std::endl;
  outputStream << "  Robust Qn: " << results.getRobustQnPercent() << "%" << std::endl;
}

void BidAskAnalyzer::displayAnalysisToConsole(const BidAskSpreadAnalysis& analysis)
{
  std::cout << "\n=== Bid/Ask Spread Analysis ===" << std::endl;
  
  if (!analysis.isValid)
  {
    std::cout << "Warning: " << analysis.errorMessage << std::endl;
    std::cout << "================================" << std::endl;
    return;
  }
  
  if (analysis.corwinSchultz.hasResults)
  {
    const auto& cs = analysis.corwinSchultz;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\nCorwin-Schultz Estimator:" << std::endl;
    std::cout << "  Mean:      " << cs.getMeanPercent().getAsDouble() << "%" << std::endl;
    std::cout << "  Median:    " << cs.getMedianPercent().getAsDouble() << "%" << std::endl;
    std::cout << "  Robust Qn: " << cs.getRobustQnPercent().getAsDouble() << "%" << std::endl;
  }
  
  if (analysis.edge.hasResults)
  {
    const auto& edge = analysis.edge;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\nEdge Estimator (30-day):" << std::endl;
    std::cout << "  Mean:      " << edge.getMeanPercent().getAsDouble() << "%" << std::endl;
    std::cout << "  Median:    " << edge.getMedianPercent().getAsDouble() << "%" << std::endl;
    std::cout << "  Robust Qn: " << edge.getRobustQnPercent().getAsDouble() << "%" << std::endl;
  }
  
  std::cout << "\n================================" << std::endl;
}
