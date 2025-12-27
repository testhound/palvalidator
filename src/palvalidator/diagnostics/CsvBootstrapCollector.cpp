#include "CsvBootstrapCollector.h"
#include <stdexcept>
#include <iostream>
#include <filesystem>

namespace palvalidator::diagnostics
{
  CsvBootstrapCollector::CsvBootstrapCollector(const std::string& filepath)
    : m_filepath(filepath)
  {
    // Check existence and size to decide if we need a header
    try {
      if (std::filesystem::exists(m_filepath) && std::filesystem::file_size(m_filepath) > 0) {
        m_headerWritten = true;
      }
    } catch (...) {
      // ignore filesystem errors
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

    // UPDATED: Now 20 columns (added StrategyID as first column)
    m_ofs << "StrategyID,Strategy,Symbol,Metric,Method,IsChosen,Score,"
          << "BCa_Z0,BCa_Accel,BCa_StabPenalty,BCa_LenPenalty,BCa_RawLen,"
          << "SE,Skew,BootMedian,EffB,InnerFail,LB,UB,N\n";

    m_ofs.flush();
    m_headerWritten = true;
  }

  void CsvBootstrapCollector::onBootstrapResult(const BootstrapDiagnosticRecord& r)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_ofs.is_open()) return;

    // 1. Strategy Info (NOW INCLUDES UNIQUE ID)
    m_ofs << r.getStrategyUniqueId() << ","      // NEW: First column
          << r.getStrategyName() << "," 
          << r.getSymbol() << ","
          << (r.getMetricType() == MetricType::GeoMean ? "GeoMean" : "ProfitFactor") << ","
          << r.getChosenMethod() << "," 
          << (r.isChosen() ? "TRUE" : "FALSE") << ","
          << r.getScore() << ",";

    // 2. BCa Specifics (5 columns)
    if (r.isBcaAvailable()) {
        m_ofs << r.getBcaZ0() << ","
              << r.getBcaAccel() << ","
              << r.getBcaStabilityPenalty() << ","
              << r.getBcaLengthPenalty() << ","
              << r.getBcaRawLength() << ",";
    } else {
        m_ofs << ",,,,,"; // 5 empty commas for non-BCa methods
    }

    // 3. General Statistics (8 columns) - MUST MATCH HEADER
    m_ofs << r.getStandardError() << "," 
          << r.getSkewness() << ","
          << r.getBootMedian() << ","
          << r.getEffectiveB() << ","
          << r.getInnerFailureRate() << ","
          << r.getChosenLowerBound() << "," 
          << r.getChosenUpperBound() << ","
          << r.getSampleSize() << "\n";

    m_ofs.flush();
  }
} // namespace palvalidator::diagnostics
