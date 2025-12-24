#include "CsvBootstrapCollector.h"
#include <stdexcept>
#include <iostream>

namespace palvalidator::diagnostics {

CsvBootstrapCollector::CsvBootstrapCollector(const std::string& filepath)
    : m_filepath(filepath)
{
    // Check existence and size to decide header
    try {
        if (std::filesystem::exists(m_filepath) && std::filesystem::file_size(m_filepath) > 0) {
            m_headerWritten = true;
        }
    } catch (...) {
        // ignore filesystem errors; we'll attempt to open file and write header
    }

    m_ofs.open(m_filepath, std::ios::out | std::ios::app);
    if (!m_ofs.is_open()) {
        throw std::runtime_error("Failed to open diagnostic file: " + m_filepath);
    }

    if (!m_headerWritten) {
        writeHeaderIfNeeded();
    }
}

CsvBootstrapCollector::~CsvBootstrapCollector() {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_ofs.is_open()) m_ofs.close();
}

void CsvBootstrapCollector::writeHeaderIfNeeded()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_headerWritten) return;
    m_ofs << "Strategy,Symbol,Metric,Method,Score,BCa_Z0,BCa_Accel,BCa_StabPenalty,BCa_LenPenalty,BCa_RawLen,SE,Skew,LB,UB,N\n";
    m_ofs.flush();
    m_headerWritten = true;
}

void CsvBootstrapCollector::onBootstrapResult(const BootstrapDiagnosticRecord& r)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_ofs.is_open()) return;

    m_ofs << r.getStrategyName() << "," << r.getSymbol() << ","
          << (r.getMetricType() == MetricType::GeoMean ? "GeoMean" : "ProfitFactor") << ","
          << r.getChosenMethod() << "," << r.getScore() << ","
          << (r.isBcaAvailable() ? std::to_string(r.getBcaZ0()) : "") << ","
          << (r.isBcaAvailable() ? std::to_string(r.getBcaAccel()) : "") << ","
          << (r.isBcaAvailable() ? std::to_string(r.getBcaStabilityPenalty()) : "") << ","
          << (r.isBcaAvailable() ? std::to_string(r.getBcaLengthPenalty()) : "") << ","
          << (r.isBcaAvailable() ? std::to_string(r.getBcaRawLength()) : "") << ","
          << r.getStandardError() << "," << r.getSkewness() << ","
          << r.getChosenLowerBound() << "," << r.getChosenUpperBound() << ","
          << r.getSampleSize() << "\n";
    m_ofs.flush();
}

} // namespace palvalidator::diagnostics

