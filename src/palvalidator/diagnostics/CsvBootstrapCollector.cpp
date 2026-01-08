#include "CsvBootstrapCollector.h"
#include "BootstrapDiagnosticRecord.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace palvalidator::diagnostics
{
  namespace
  {
    // Minimal CSV escaping: quote if contains comma/quote/newline; double-up quotes.
    static std::string csvEscape(const std::string& s)
    {
      bool needsQuotes = false;
      for (char ch : s)
      {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r')
        {
          needsQuotes = true;
          break;
        }
      }

      if (!needsQuotes)
        return s;

      std::string out;
      out.reserve(s.size() + 2);
      out.push_back('"');
      for (char ch : s)
      {
        if (ch == '"')
          out.push_back('"');
        out.push_back(ch);
      }
      out.push_back('"');
      return out;
    }

    static const char* metricTypeToString(MetricType m)
    {
      switch (m)
      {
      case MetricType::GeoMean:      return "GeoMean";
      case MetricType::ProfitFactor: return "ProfitFactor";
      }
      return "Unknown";
    }

    // Helper to print NaN/Inf as empty (common CSV style)
    static void writeDoubleOrEmpty(std::ostream& os, double x)
    {
      if (std::isfinite(x))
        os << std::setprecision(17) << x;
      // else empty field
    }

    static void writeSizeOrEmpty(std::ostream& os, std::size_t x, bool emptyIfZero = false)
    {
      if (emptyIfZero && x == 0)
        return;
      os << x;
    }
  }

  CsvBootstrapCollector::CsvBootstrapCollector(const std::string& tournamentRunsPath,
                                               const std::string& candidatesPath)
    : m_tournamentRunsPath(tournamentRunsPath),
      m_candidatesPath(candidatesPath)
  {
    // Ensure parent directories exist
    try
    {
      std::filesystem::path p1(m_tournamentRunsPath);
      std::filesystem::path p2(m_candidatesPath);

      if (p1.has_parent_path())
        std::filesystem::create_directories(p1.parent_path());
      if (p2.has_parent_path())
        std::filesystem::create_directories(p2.parent_path());
    }
    catch (...)
    {
      // Directory creation failures should not crash; file open will surface problems.
    }

    // Open in append mode.
    m_tournamentRunsFile.open(m_tournamentRunsPath, std::ios::out | std::ios::app);
    m_candidatesFile.open(m_candidatesPath, std::ios::out | std::ios::app);

    if (!m_tournamentRunsFile.is_open())
      throw std::runtime_error("CsvBootstrapCollector: failed to open TournamentRuns file: " + m_tournamentRunsPath);
    if (!m_candidatesFile.is_open())
      throw std::runtime_error("CsvBootstrapCollector: failed to open Candidates file: " + m_candidatesPath);

    writeTournamentRunsHeaderIfNeeded();
    writeCandidatesHeaderIfNeeded();
  }

  CsvBootstrapCollector::~CsvBootstrapCollector()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_tournamentRunsFile.is_open())
      m_tournamentRunsFile.flush();
    if (m_candidatesFile.is_open())
      m_candidatesFile.flush();
  }

  void CsvBootstrapCollector::onBootstrapResult(const BootstrapDiagnosticRecord& record)
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Ensure headers (defensive if files were rotated etc.)
    writeTournamentRunsHeaderIfNeeded();
    writeCandidatesHeaderIfNeeded();

    // Write tournament row once per runID.
    const std::uint64_t runID = record.getRunID();
    if (m_writtenRunIDs.find(runID) == m_writtenRunIDs.end())
    {
      writeTournamentRun(record);
      m_writtenRunIDs.insert(runID);
    }

    // Always write candidate row.
    writeCandidate(record);
  }

  void CsvBootstrapCollector::writeTournamentRunsHeaderIfNeeded()
  {
    if (m_tournamentRunsHeaderWritten)
      return;

    // If file is empty, write header.
    if (m_tournamentRunsFile.tellp() == std::streampos(0))
    {
      // TournamentContext now contains only invariants.
      m_tournamentRunsFile
        << "RunID"
        << ",StrategyUniqueId"
        << ",StrategyName"
        << ",Symbol"
        << ",Metric"
        << ",ConfidenceLevel"
        << ",SampleSize"
        << ",NumCandidates"
        << ",TieEpsilon"
        << "\n";
      m_tournamentRunsFile.flush();
    }

    m_tournamentRunsHeaderWritten = true;
  }

  void CsvBootstrapCollector::writeCandidatesHeaderIfNeeded()
  {
    if (m_candidatesHeaderWritten)
      return;

    if (m_candidatesFile.tellp() == std::streampos(0))
    {
      m_candidatesFile
        << "RunID"
        << ",CandidateID"
        << ",Method"
        << ",IsChosen"
        << ",Rank"

        // Final score + CI
        << ",FinalScore"
        << ",LowerBound"
        << ",UpperBound"
        << ",IntervalLength"

        // Rejection / gating
        << ",RejectionMask"
        << ",RejectionText"
        << ",PassedGates"

        // Support
        << ",ViolatesSupport"
        << ",SupportLower"
        << ",SupportUpper"

        // Candidate distribution/engine stats (moved out of TournamentContext)
        << ",BOuter"
        << ",BInner"
        << ",EffectiveB"
        << ",SkippedTotal"
        << ",SEBoot"
        << ",SkewBoot"
        << ",MedianBoot"
        << ",CenterShiftInSE"
        << ",NormalizedLength"

        // Penalties: raw/norm/contrib per component actually used in scoring
        << ",OrderingRaw,OrderingNorm,OrderingContrib"
        << ",LengthRaw,LengthNorm,LengthContrib"
        << ",StabilityRaw,StabilityNorm,StabilityContrib"
        << ",DomainRaw,DomainNorm,DomainContrib"
        << ",CenterShiftRaw,CenterShiftNorm,CenterShiftContrib"
        << ",SkewRaw,SkewNorm,SkewContrib"
        << ",BCaOverflowRaw,BCaOverflowNorm,BCaOverflowContrib"

        // BCa-specific (blank if not available)
        << ",BCa_Available"
        << ",BCa_Z0"
        << ",BCa_Accel"
        << ",BCa_Z0HardFail"
        << ",BCa_AccelHardFail"
        << ",BCa_RawLength"

        // PercentileT-specific (blank if not available)
        << ",PercentileT_Available"
        << ",PercentileT_BOuter"
        << ",PercentileT_BInner"
        << ",PercentileT_OuterFailCount"
        << ",PercentileT_InnerFailCount"
        << ",PercentileT_InnerFailRate"
        << ",PercentileT_EffectiveB"
        << "\n";

      m_candidatesFile.flush();
    }

    m_candidatesHeaderWritten = true;
  }

  void CsvBootstrapCollector::writeTournamentRun(const BootstrapDiagnosticRecord& record)
  {
    const auto& t = record.getTournament();

    m_tournamentRunsFile
      << t.getRunID() << ","
      << t.getStrategyUniqueId() << ","
      << csvEscape(t.getStrategyName()) << ","
      << csvEscape(t.getSymbol()) << ","
      << metricTypeToString(t.getMetricType()) << ",";

    writeDoubleOrEmpty(m_tournamentRunsFile, t.getConfidenceLevel());
    m_tournamentRunsFile << ",";

    m_tournamentRunsFile
      << t.getSampleSize() << ","
      << t.getNumCandidates() << ",";

    writeDoubleOrEmpty(m_tournamentRunsFile, t.getTieEpsilon());

    m_tournamentRunsFile << "\n";
    m_tournamentRunsFile.flush();
  }

  void CsvBootstrapCollector::writeCandidate(const BootstrapDiagnosticRecord& record)
  {
    const auto& t   = record.getTournament();
    const auto& id  = record.getIdentity();
    const auto& st  = record.getStats();
    const auto& in  = record.getInterval();
    const auto& rj  = record.getRejection();
    const auto& sp  = record.getSupport();
    const auto& pn  = record.getPenalties();
    const auto& bca = record.getBca();
    const auto& pt  = record.getPercentileT();

    // Base identity + interval
    m_candidatesFile
      << t.getRunID() << ","
      << id.getCandidateID() << ","
      << csvEscape(id.getMethodName()) << ","
      << (id.isChosen() ? "TRUE" : "FALSE") << ","
      << id.getRank() << ",";

    writeDoubleOrEmpty(m_candidatesFile, in.getFinalScore());
    m_candidatesFile << ",";

    writeDoubleOrEmpty(m_candidatesFile, in.getLowerBound());
    m_candidatesFile << ",";

    writeDoubleOrEmpty(m_candidatesFile, in.getUpperBound());
    m_candidatesFile << ",";

    writeDoubleOrEmpty(m_candidatesFile, in.getIntervalLength());
    m_candidatesFile << ",";

    // Rejection
    m_candidatesFile
      << static_cast<std::uint32_t>(rj.getRejectionMask()) << ","
      << csvEscape(rj.getRejectionText()) << ","
      << (rj.passedGates() ? "TRUE" : "FALSE") << ",";

    // Support
    m_candidatesFile << (sp.violatesSupport() ? "TRUE" : "FALSE") << ",";
    writeDoubleOrEmpty(m_candidatesFile, sp.getSupportLowerBound());
    m_candidatesFile << ",";
    writeDoubleOrEmpty(m_candidatesFile, sp.getSupportUpperBound());
    m_candidatesFile << ",";

    // Candidate distribution/engine stats
    m_candidatesFile
      << st.getBOuter() << ","
      << st.getBInner() << ","
      << st.getEffectiveB() << ","
      << st.getSkippedTotal() << ",";

    writeDoubleOrEmpty(m_candidatesFile, st.getSeBoot());
    m_candidatesFile << ",";
    writeDoubleOrEmpty(m_candidatesFile, st.getSkewBoot());
    m_candidatesFile << ",";
    writeDoubleOrEmpty(m_candidatesFile, st.getMedianBoot());
    m_candidatesFile << ",";
    writeDoubleOrEmpty(m_candidatesFile, st.getCenterShiftInSe());
    m_candidatesFile << ",";
    writeDoubleOrEmpty(m_candidatesFile, st.getNormalizedLength());
    m_candidatesFile << ",";

    // Penalties
    const auto& ord = pn.getOrdering();
    const auto& len = pn.getLength();
    const auto& stb = pn.getStability();
    const auto& dom = pn.getDomain();
    const auto& cen = pn.getCenterShift();
    const auto& skw = pn.getSkew();
    const auto& ovf = pn.getBcaOverflow();

    auto writePenaltyTriple = [&](const BootstrapDiagnosticRecord::PenaltyComponents& pc) {
      writeDoubleOrEmpty(m_candidatesFile, pc.getRaw());        m_candidatesFile << ",";
      writeDoubleOrEmpty(m_candidatesFile, pc.getNormalized()); m_candidatesFile << ",";
      writeDoubleOrEmpty(m_candidatesFile, pc.getContribution());
    };

    // Ordering
    writePenaltyTriple(ord); m_candidatesFile << ",";
    // Length
    writePenaltyTriple(len); m_candidatesFile << ",";
    // Stability
    writePenaltyTriple(stb); m_candidatesFile << ",";
    // Domain
    writePenaltyTriple(dom); m_candidatesFile << ",";
    // Center shift
    writePenaltyTriple(cen); m_candidatesFile << ",";
    // Skew
    writePenaltyTriple(skw); m_candidatesFile << ",";
    // BCa overflow penalty term
    writePenaltyTriple(ovf); m_candidatesFile << ",";

    // BCa fields
    m_candidatesFile << (bca.isAvailable() ? "TRUE" : "FALSE") << ",";
    if (bca.isAvailable())
    {
      writeDoubleOrEmpty(m_candidatesFile, bca.getZ0());    m_candidatesFile << ",";
      writeDoubleOrEmpty(m_candidatesFile, bca.getAccel()); m_candidatesFile << ",";
      m_candidatesFile << (bca.z0ExceedsHardLimit() ? "TRUE" : "FALSE") << ",";
      m_candidatesFile << (bca.accelExceedsHardLimit() ? "TRUE" : "FALSE") << ",";
      writeDoubleOrEmpty(m_candidatesFile, bca.getRawLength());
    }
    else
    {
      // Z0, Accel, Z0HardFail, AccelHardFail, RawLength
      m_candidatesFile << ",,,,"; // 4 empties + then raw length empty via fallthrough
      // RawLength empty
    }

    m_candidatesFile << ",";

    // PercentileT fields
    m_candidatesFile << (pt.isAvailable() ? "TRUE" : "FALSE") << ",";
    if (pt.isAvailable())
    {
      m_candidatesFile << pt.getBOuter() << ",";
      m_candidatesFile << pt.getBInner() << ",";
      m_candidatesFile << pt.getOuterFailCount() << ",";
      m_candidatesFile << pt.getInnerFailCount() << ",";
      writeDoubleOrEmpty(m_candidatesFile, pt.getInnerFailRate());
      m_candidatesFile << ",";
      writeDoubleOrEmpty(m_candidatesFile, pt.getEffectiveB());
    }
    else
    {
      // BOuter, BInner, OuterFailCount, InnerFailCount, InnerFailRate, EffectiveB
      m_candidatesFile << ",,,,,";
      // EffectiveB empty
    }

    m_candidatesFile << "\n";
    m_candidatesFile.flush();
  }

} // namespace palvalidator::diagnostics
