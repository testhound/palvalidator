#pragma once

#include <fstream>
#include <memory>
#include "number.h"
#include "BackTester.h"

namespace palvalidator
{
namespace reporting
{

using namespace mkc_timeseries;
using Num = num::DefaultNumber;

/**
 * @brief Reporter for backtesting performance metrics
 * 
 * This class provides functionality to write detailed backtest performance
 * reports including trade statistics, profit metrics, and other key indicators.
 */
class PerformanceReporter
{
public:
    /**
     * @brief Write comprehensive backtest performance report to file
     * @param file Output file stream to write the report to
     * @param backtester Shared pointer to the backtester containing results
     */
    static void writeBacktestReport(
        std::ofstream& file,
        std::shared_ptr<BackTester<Num>> backtester
    );

private:
    /**
     * @brief Write section header to the report
     * @param file Output file stream
     * @param title Title of the section
     */
    static void writeSectionHeader(std::ofstream& file, const std::string& title);

    /**
     * @brief Write section footer to the report
     * @param file Output file stream
     */
    static void writeSectionFooter(std::ofstream& file);
};

} // namespace reporting
} // namespace palvalidator