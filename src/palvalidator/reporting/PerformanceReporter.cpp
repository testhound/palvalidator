#include "PerformanceReporter.h"

namespace palvalidator
{
namespace reporting
{

void PerformanceReporter::writeBacktestReport(
    std::ofstream& file,
    std::shared_ptr<BackTester<Num>> backtester)
{
    auto positionHistory = backtester->getClosedPositionHistory();
    
    writeSectionHeader(file, "Backtest Performance Report");
    
    file << "Total Closed Positions: " << positionHistory.getNumPositions() << std::endl;
    file << "Number of Winning Trades: " << positionHistory.getNumWinningPositions() << std::endl;
    file << "Number of Losing Trades: " << positionHistory.getNumLosingPositions() << std::endl;
    file << "Total Bars in Market: " << positionHistory.getNumBarsInMarket() << std::endl;
    file << "Percent Winners: " << positionHistory.getPercentWinners() << "%" << std::endl;
    file << "Percent Losers: " << positionHistory.getPercentLosers() << "%" << std::endl;
    file << "Profit Factor: " << positionHistory.getProfitFactor() << std::endl;
    file << "High Resolution Profit Factor: " << positionHistory.getHighResProfitFactor() << std::endl;
    file << "PAL Profitability: " << positionHistory.getPALProfitability() << "%" << std::endl;
    file << "High Resolution Profitability: " << positionHistory.getHighResProfitability() << std::endl;
    
    writeSectionFooter(file);
    file << std::endl;
}

void PerformanceReporter::writeSectionHeader(std::ofstream& file, const std::string& title)
{
    file << "=== " << title << " ===" << std::endl;
}

void PerformanceReporter::writeSectionFooter(std::ofstream& file)
{
    file << "===================================" << std::endl;
}

} // namespace reporting
} // namespace palvalidator