#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// Bootstrap Tournament Summary (two-file schema)
//
// Inputs:
//   TournamentRuns.csv
//   Candidates.csv
//
// This tool is intentionally NOT backwards-compatible with the legacy single
// CSV schema.
// -----------------------------------------------------------------------------

namespace
{
  static inline std::string ltrim(std::string s)
  {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    s.erase(0, i);
    return s;
  }

  static inline std::string rtrim(std::string s)
  {
    std::size_t i = s.size();
    while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) --i;
    s.erase(i);
    return s;
  }

  static inline std::string trim(std::string s)
  {
    return rtrim(ltrim(std::move(s)));
  }

  static inline std::string toLower(std::string s)
  {
    for (char& c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }

  // Basic CSV parsing with quotes.
  static std::vector<std::string> splitCSVLine(const std::string& line)
  {
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i)
    {
      const char ch = line[i];
      if (inQuotes)
      {
        if (ch == '"')
        {
          // doubled quote => literal quote
          if (i + 1 < line.size() && line[i + 1] == '"')
          {
            cur.push_back('"');
            ++i;
          }
          else
          {
            inQuotes = false;
          }
        }
        else
        {
          cur.push_back(ch);
        }
      }
      else
      {
        if (ch == ',')
        {
          out.push_back(cur);
          cur.clear();
        }
        else if (ch == '"')
        {
          inQuotes = true;
        }
        else
        {
          cur.push_back(ch);
        }
      }
    }
    out.push_back(cur);
    return out;
  }

  static inline bool toBool(const std::string& s)
  {
    const std::string t = toLower(trim(s));
    return (t == "true" || t == "1" || t == "yes");
  }

  static inline std::uint64_t toU64(const std::string& s)
  {
    const std::string t = trim(s);
    if (t.empty())
      return 0u;
    return static_cast<std::uint64_t>(std::stoull(t));
  }

  static inline std::size_t toSize(const std::string& s)
  {
    const std::string t = trim(s);
    if (t.empty())
      return 0;
    return static_cast<std::size_t>(std::stoull(t));
  }

  static inline double toDouble(const std::string& s)
  {
    const std::string t = trim(s);
    if (t.empty())
      return std::numeric_limits<double>::quiet_NaN();
    return std::stod(t);
  }

  static void writeDoubleOrEmpty(std::ostream& os, double x)
  {
    if (std::isfinite(x))
      os << std::setprecision(17) << x;
  }

  static inline std::uint32_t toU32(const std::string& s)
  {
    const std::string t = trim(s);
    if (t.empty())
      return 0u;
    return static_cast<std::uint32_t>(std::stoul(t));
  }

  static std::vector<std::string> splitReasons(const std::string& text)
  {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : text)
    {
      if (ch == ';')
      {
        if (!cur.empty()) out.push_back(cur);
        cur.clear();
      }
      else
      {
        cur.push_back(ch);
      }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
  }

  struct DiffSummary
  {
    std::size_t n = 0;
    double mean = std::numeric_limits<double>::quiet_NaN();
    double median = std::numeric_limits<double>::quiet_NaN();
    double p90 = std::numeric_limits<double>::quiet_NaN();
    double min = std::numeric_limits<double>::quiet_NaN();
    double max = std::numeric_limits<double>::quiet_NaN();
  };

  static DiffSummary summarizeDiffs(std::vector<double> v)
  {
    DiffSummary s;
    // remove non-finite
    v.erase(std::remove_if(v.begin(), v.end(), [](double x){ return !std::isfinite(x); }), v.end());
    if (v.empty())
      return s;

    std::sort(v.begin(), v.end());
    s.n = v.size();
    s.min = v.front();
    s.max = v.back();
    s.mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
    s.median = v[v.size() / 2];
    const std::size_t p90idx = static_cast<std::size_t>(std::floor(0.90 * (v.size() - 1)));
    s.p90 = v[p90idx];
    return s;
  }

  static void printDiffSummary(const std::string& label, const DiffSummary& s)
  {
    std::cout << "  " << label << ": n=" << s.n;
    if (s.n > 0)
    {
      std::cout << " mean=" << std::setprecision(6) << s.mean
                << " median=" << s.median
                << " p90=" << s.p90
                << " min=" << s.min
                << " max=" << s.max;
    }
    std::cout << "\n";
  }

  struct TournamentRow
  {
    std::uint64_t runID = 0;
    std::uint64_t strategyUniqueId = 0;
    std::string strategyName;
    std::string symbol;
    std::string metric;
    double confidenceLevel = std::numeric_limits<double>::quiet_NaN();
    std::size_t sampleSize = 0;
    std::size_t numCandidates = 0;
    double tieEpsilon = std::numeric_limits<double>::quiet_NaN();
  };

  struct CandidateRow
  {
    std::uint64_t runID = 0;
    std::uint64_t candidateID = 0;
    std::string method;
    bool isChosen = false;
    std::size_t rank = 0;

    double finalScore = std::numeric_limits<double>::quiet_NaN();
    double lb = std::numeric_limits<double>::quiet_NaN();
    double ub = std::numeric_limits<double>::quiet_NaN();
    double intervalLen = std::numeric_limits<double>::quiet_NaN();

    std::uint32_t rejectionMask = 0;
    std::string rejectionText;
    bool passedGates = true;

    bool violatesSupport = false;
    double supportLower = std::numeric_limits<double>::quiet_NaN();
    double supportUpper = std::numeric_limits<double>::quiet_NaN();

    // Distribution / engine stats (may be 0/NaN if not logged)
    std::size_t B_outer = 0;
    std::size_t B_inner = 0;
    std::size_t effectiveB = 0;
    std::size_t skippedTotal = 0;
    double seBoot = std::numeric_limits<double>::quiet_NaN();
    double skewBoot = std::numeric_limits<double>::quiet_NaN();
    double medianBoot = std::numeric_limits<double>::quiet_NaN();
    double centerShiftInSe = std::numeric_limits<double>::quiet_NaN();
    double normalizedLength = std::numeric_limits<double>::quiet_NaN();

    // Penalties (raw/norm/contrib)
    double ordRaw = std::numeric_limits<double>::quiet_NaN();
    double ordNorm = std::numeric_limits<double>::quiet_NaN();

    double ordContrib = std::numeric_limits<double>::quiet_NaN();

    double lenRaw = std::numeric_limits<double>::quiet_NaN();
    double lenNorm = std::numeric_limits<double>::quiet_NaN();
    double lenContrib = std::numeric_limits<double>::quiet_NaN();

    double stabRaw = std::numeric_limits<double>::quiet_NaN();
    double stabNorm = std::numeric_limits<double>::quiet_NaN();
    double stabContrib = std::numeric_limits<double>::quiet_NaN();

    double domRaw = std::numeric_limits<double>::quiet_NaN();
    double domNorm = std::numeric_limits<double>::quiet_NaN();
    double domContrib = std::numeric_limits<double>::quiet_NaN();

    double centerRaw = std::numeric_limits<double>::quiet_NaN();
    double centerNorm = std::numeric_limits<double>::quiet_NaN();
    double centerContrib = std::numeric_limits<double>::quiet_NaN();

    double skewRaw = std::numeric_limits<double>::quiet_NaN();
    double skewNorm = std::numeric_limits<double>::quiet_NaN();
    double skewContrib = std::numeric_limits<double>::quiet_NaN();

    double bcaOverflowRaw = std::numeric_limits<double>::quiet_NaN();
    double bcaOverflowNorm = std::numeric_limits<double>::quiet_NaN();
    double bcaOverflowContrib = std::numeric_limits<double>::quiet_NaN();

    bool bcaAvailable = false;
    double bcaZ0 = std::numeric_limits<double>::quiet_NaN();
    double bcaAccel = std::numeric_limits<double>::quiet_NaN();

    bool pctTAvailable = false;
    std::size_t pctTOuterFailCount = 0;
    std::size_t pctTInnerFailCount = 0;
    double pctTInnerFailRate = std::numeric_limits<double>::quiet_NaN();

    bool hasReasonToken(const std::string& token) const
    {
      const auto reasons = splitReasons(rejectionText);
      for (const auto& r : reasons)
      {
        if (r.find(token) != std::string::npos)
          return true;
      }
      return false;
    }
  };

  static std::string sampleSizeBucket(std::size_t n)
  {
    if (n < 20) return "<20";
    if (n <= 24) return "20-24";
    if (n <= 29) return "25-29";
    if (n <= 39) return "30-39";
    return "40+";
  }

  // Map CandidateReject bits (keep aligned with CandidateReject.h; unknown bits will be shown as BIT_##).
  struct RejectBit { std::uint32_t bit; const char* name; };

  static const std::vector<RejectBit>& rejectBits()
  {
    static const std::vector<RejectBit> bits = {
      {1u << 0,  "SCORE_NON_FINITE"},
      {1u << 1,  "VIOLATES_SUPPORT"},
      {1u << 2,  "COMMON_GATE_FAILED"},
      {1u << 3,  "BCa_PARAMS_NON_FINITE"},
      {1u << 4,  "BCa_Z0_EXCEEDED"},
      {1u << 5,  "BCa_ACCEL_EXCEEDED"},
      {1u << 6,  "PCTT_OUTER_FAILURES"},
      {1u << 7,  "PCTT_INNER_FAILURES"},
      {1u << 8,  "PCTT_LOW_EFFECTIVE_B"},
      {1u << 9,  "OVER_LENGTH"},
      {1u << 10, "UNDER_LENGTH"},
      {1u << 11, "HIGH_STABILITY_PENALTY"},
    };
    return bits;
  }

  static void addRejectBitCounts(std::unordered_map<std::string, std::size_t>& out,
                                std::uint32_t mask)
  {
    if (mask == 0u) return;

    std::uint32_t remaining = mask;
    for (const auto& b : rejectBits())
    {
      if (mask & b.bit)
      {
        out[b.name]++;
        remaining &= ~b.bit;
      }
    }

    // Any unknown bits
    for (int bit = 0; bit < 32; ++bit)
    {
      const std::uint32_t v = (1u << bit);
      if (remaining & v)
      {
        out[std::string("BIT_") + std::to_string(bit)]++;
      }
    }
  }

  static void addRejectTokenCounts(std::unordered_map<std::string, std::size_t>& out,
                                  const std::string& rejectionText)
  {
    for (const auto& tok : splitReasons(rejectionText))
    {
      out[tok]++;
    }
  }

  static std::vector<std::pair<std::string, std::size_t>> sortedCounts(
    const std::unordered_map<std::string, std::size_t>& m)
  {
    std::vector<std::pair<std::string, std::size_t>> v(m.begin(), m.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b){ return a.second > b.second; });
    return v;
  }

  static void printTopCounts(const std::string& title,
                            const std::unordered_map<std::string, std::size_t>& m,
                            std::size_t topN = 10)
  {
    std::cout << title << "\n";
    auto v = sortedCounts(m);
    if (v.empty())
    {
      std::cout << "  (none)\n";
      return;
    }
    const std::size_t n = std::min(topN, v.size());
    for (std::size_t i = 0; i < n; ++i)
    {
      std::cout << "  " << std::left << std::setw(24) << v[i].first << " " << v[i].second << "\n";
    }
  }

  static std::string argmaxDelta(const std::vector<std::pair<std::string,double>>& deltas)
  {
    if (deltas.empty()) return "(none)";
    double best = -std::numeric_limits<double>::infinity();
    std::string bestName = "(none)";
    for (const auto& p : deltas)
    {
      if (p.second > best)
      {
        best = p.second;
        bestName = p.first;
      }
    }
    if (!(best > 0.0))
      return "(none)";
    return bestName;
  }

  static bool isExactZero(double x, double tol = 0.0)
  {
    if (!std::isfinite(x))
      return false;
    return std::fabs(x) <= tol;
  }

  // Approximate tie-break preference order used only for downstream reporting.
  // This is NOT used by the production selector; it exists to help interpret cases
  // where "Rank" (score sort) disagrees with "IsChosen" (tournament winner).
  static int preferenceRank(const std::string& method)
  {
    if (method == "BCa") return 0;
    if (method == "PercentileT") return 1;
    if (method == "Percentile") return 2;
    if (method == "Basic") return 3;
    if (method == "Normal") return 4;
    if (method == "MOutOfN") return 5;
    return 100;
  }

  static bool tournamentLess(const CandidateRow& a, const CandidateRow& b, double tieEpsilon)
  {
    const bool af = std::isfinite(a.finalScore);
    const bool bf = std::isfinite(b.finalScore);
    if (af != bf) return af; // finite scores first
    if (!af) return false;   // both non-finite -> stable (treat equal)

    const double diff = a.finalScore - b.finalScore;
    if (std::fabs(diff) > tieEpsilon)
      return diff < 0.0;

    // Tie within epsilon: apply preference order
    return preferenceRank(a.method) < preferenceRank(b.method);
  }

  static std::string lossDriver(const CandidateRow& loser, const CandidateRow& winner)
  {
    // Compare *contributions* (the weighted terms) when available.
    // We look at (loserContrib - winnerContrib) and pick the biggest positive.
    std::vector<std::pair<std::string,double>> deltas;
    deltas.reserve(7);

    auto add = [&](const char* name, double loserC, double winnerC) {
      if (std::isfinite(loserC) && std::isfinite(winnerC)) {
        deltas.push_back({name, loserC - winnerC});
      }
    };

    add("ordering",    loser.ordContrib, winner.ordContrib);
    add("length",      loser.lenContrib, winner.lenContrib);
    add("stability",   loser.stabContrib, winner.stabContrib);
    add("domain",      loser.domContrib, winner.domContrib);
    add("centerShift", loser.centerContrib, winner.centerContrib);
    add("skew",        loser.skewContrib, winner.skewContrib);
    add("bcaOverflow", loser.bcaOverflowContrib, winner.bcaOverflowContrib);

    return argmaxDelta(deltas);
  }

  struct Head2Head
  {
    std::size_t runsWhereBothExist = 0;

    // Wins (chosen among all tournaments, even if counterpart missing)
    std::size_t ptChosen = 0;
    std::size_t bcaChosen = 0;

    // If counterpart missing
    std::size_t ptChosen_bcaMissing = 0;
    std::size_t bcaChosen_ptMissing = 0;

    // If counterpart exists
    std::size_t ptChosen_bcaRejected = 0;
    std::size_t ptChosen_bcaPassedLostScore = 0;

    std::size_t bcaChosen_ptRejected = 0;
    std::size_t bcaChosen_ptPassedLostScore = 0;

    // Both passed gates (regardless of which method won)
    std::vector<double> scoreDiff_pt_minus_bca;
    std::vector<double> dOrd, dLen, dStab, dDom, dCenter, dSkew, dOverflow;

    // Tie stats (computed on both-pass runs)
    std::size_t tiesWithinEps = 0;
    std::size_t tiesPreferredDecided = 0; // BCa is preferred over PercentileT
    std::size_t tiesNonPreferredWon = 0;  // PercentileT won while within eps

    // Gate reason counts by bit/token (only when both exist)
    std::unordered_map<std::string, std::size_t> ptRejectBits;
    std::unordered_map<std::string, std::size_t> bcaRejectBits;
    std::unordered_map<std::string, std::size_t> ptRejectTokens;
    std::unordered_map<std::string, std::size_t> bcaRejectTokens;

    // Loss drivers (only when both pass and one wins)
    std::unordered_map<std::string, std::size_t> bcaLossDrivers;
    std::unordered_map<std::string, std::size_t> ptLossDrivers;
  };

  static void updateHead2Head(Head2Head& hh,
                             const TournamentRow& t,
                             const std::optional<CandidateRow>& ptOpt,
                             const std::optional<CandidateRow>& bcaOpt,
                             const std::optional<CandidateRow>& chosenOpt)
  {
    if (!ptOpt && !bcaOpt)
      return;

    // wins (chosen)
    if (chosenOpt)
    {
      const CandidateRow& chosen = *chosenOpt;
      if (chosen.method == "PercentileT")
      {
        hh.ptChosen++;
        if (!bcaOpt)
        {
          hh.ptChosen_bcaMissing++;
        }
        else
        {
          if (!bcaOpt->passedGates) hh.ptChosen_bcaRejected++;
          else hh.ptChosen_bcaPassedLostScore++;
        }
      }
      else if (chosen.method == "BCa")
      {
        hh.bcaChosen++;
        if (!ptOpt)
        {
          hh.bcaChosen_ptMissing++;
        }
        else
        {
          if (!ptOpt->passedGates) hh.bcaChosen_ptRejected++;
          else hh.bcaChosen_ptPassedLostScore++;
        }
      }
    }

    // head-to-head details require both exist
    if (!ptOpt || !bcaOpt)
      return;

    hh.runsWhereBothExist++;

    const CandidateRow& pt = *ptOpt;
    const CandidateRow& bca = *bcaOpt;

    if (!pt.passedGates)
    {
      addRejectBitCounts(hh.ptRejectBits, pt.rejectionMask);
      addRejectTokenCounts(hh.ptRejectTokens, pt.rejectionText);
    }
    if (!bca.passedGates)
    {
      addRejectBitCounts(hh.bcaRejectBits, bca.rejectionMask);
      addRejectTokenCounts(hh.bcaRejectTokens, bca.rejectionText);
    }

    if (pt.passedGates && bca.passedGates)
    {
      if (std::isfinite(pt.finalScore) && std::isfinite(bca.finalScore))
      {
        const double d = pt.finalScore - bca.finalScore;
        hh.scoreDiff_pt_minus_bca.push_back(d);

        auto push = [](std::vector<double>& v, double x){ if (std::isfinite(x)) v.push_back(x); };
        push(hh.dOrd, pt.ordContrib - bca.ordContrib);
        push(hh.dLen, pt.lenContrib - bca.lenContrib);
        push(hh.dStab, pt.stabContrib - bca.stabContrib);
        push(hh.dDom, pt.domContrib - bca.domContrib);
        push(hh.dCenter, pt.centerContrib - bca.centerContrib);
        push(hh.dSkew, pt.skewContrib - bca.skewContrib);
        push(hh.dOverflow, pt.bcaOverflowContrib - bca.bcaOverflowContrib);

        // tie check: using tournament-level tieEpsilon as an approximation
        if (std::isfinite(t.tieEpsilon) && std::fabs(d) <= t.tieEpsilon)
        {
          hh.tiesWithinEps++;
          if (chosenOpt && chosenOpt->method == "BCa") hh.tiesPreferredDecided++;
          if (chosenOpt && chosenOpt->method == "PercentileT") hh.tiesNonPreferredWon++;
        }
      }

      // loss driver (only when chosen is PT or BCa)
      if (chosenOpt)
      {
        if (chosenOpt->method == "PercentileT")
        {
          const std::string driver = lossDriver(bca, pt);
          hh.bcaLossDrivers[driver]++;
        }
        else if (chosenOpt->method == "BCa")
        {
          const std::string driver = lossDriver(pt, bca);
          hh.ptLossDrivers[driver]++;
        }
      }
    }
  }

  static std::unordered_map<std::uint64_t, TournamentRow> readTournamentRuns(const std::string& path)
  {
    std::ifstream in(path);
    if (!in.is_open())
      throw std::runtime_error("Failed to open " + path);

    std::string header;
    if (!std::getline(in, header))
      throw std::runtime_error("Empty file: " + path);

    const auto cols = splitCSVLine(header);
    std::unordered_map<std::string, std::size_t> idx;
    for (std::size_t i = 0; i < cols.size(); ++i)
      idx[trim(cols[i])] = i;

    auto col = [&](const std::vector<std::string>& row, const char* name) -> std::string {
      const auto it = idx.find(name);
      if (it == idx.end() || it->second >= row.size())
        return "";
      return row[it->second];
    };

    std::unordered_map<std::uint64_t, TournamentRow> out;
    std::string line;
    while (std::getline(in, line))
    {
      if (trim(line).empty())
        continue;
      const auto row = splitCSVLine(line);
      TournamentRow t;
      t.runID = toU64(col(row, "RunID"));
      t.strategyUniqueId = toU64(col(row, "StrategyUniqueId"));
      t.strategyName = col(row, "StrategyName");
      t.symbol = col(row, "Symbol");
      t.metric = col(row, "Metric");
      t.confidenceLevel = toDouble(col(row, "ConfidenceLevel"));
      t.sampleSize = toSize(col(row, "SampleSize"));
      t.numCandidates = toSize(col(row, "NumCandidates"));
      t.tieEpsilon = toDouble(col(row, "TieEpsilon"));
      out[t.runID] = std::move(t);
    }
    return out;
  }

  static std::vector<CandidateRow> readCandidates(const std::string& path)
  {
    std::ifstream in(path);
    if (!in.is_open())
      throw std::runtime_error("Failed to open " + path);

    std::string header;
    if (!std::getline(in, header))
      throw std::runtime_error("Empty file: " + path);

    const auto cols = splitCSVLine(header);
    std::unordered_map<std::string, std::size_t> idx;
    for (std::size_t i = 0; i < cols.size(); ++i)
      idx[trim(cols[i])] = i;

    auto col = [&](const std::vector<std::string>& row, const char* name) -> std::string {
      const auto it = idx.find(name);
      if (it == idx.end() || it->second >= row.size())
        return "";
      return row[it->second];
    };

    std::vector<CandidateRow> out;
    std::string line;
    while (std::getline(in, line))
    {
      if (trim(line).empty())
        continue;
      const auto row = splitCSVLine(line);
      CandidateRow c;
      c.runID = toU64(col(row, "RunID"));
      c.candidateID = toU64(col(row, "CandidateID"));
      c.method = col(row, "Method");
      c.isChosen = toBool(col(row, "IsChosen"));
      c.rank = toSize(col(row, "Rank"));

      c.finalScore = toDouble(col(row, "FinalScore"));
      c.lb = toDouble(col(row, "LowerBound"));
      c.ub = toDouble(col(row, "UpperBound"));
      c.intervalLen = toDouble(col(row, "IntervalLength"));

      c.rejectionMask = toU32(col(row, "RejectionMask"));
      c.rejectionText = col(row, "RejectionText");
      c.passedGates = toBool(col(row, "PassedGates"));
      c.violatesSupport = toBool(col(row, "ViolatesSupport"));
      c.supportLower = toDouble(col(row, "SupportLower"));
      c.supportUpper = toDouble(col(row, "SupportUpper"));

      c.B_outer = toSize(col(row, "BOuter"));
      c.B_inner = toSize(col(row, "BInner"));
      c.effectiveB = toSize(col(row, "EffectiveB"));
      c.skippedTotal = toSize(col(row, "SkippedTotal"));

      c.seBoot = toDouble(col(row, "SEBoot"));
      c.skewBoot = toDouble(col(row, "SkewBoot"));
      c.medianBoot = toDouble(col(row, "MedianBoot"));
      c.centerShiftInSe = toDouble(col(row, "CenterShiftInSE"));
      c.normalizedLength = toDouble(col(row, "NormalizedLength"));

      c.ordRaw = toDouble(col(row, "OrderingRaw"));
      c.ordNorm = toDouble(col(row, "OrderingNorm"));

      c.ordContrib = toDouble(col(row, "OrderingContrib"));

      c.lenRaw = toDouble(col(row, "LengthRaw"));
      c.lenNorm = toDouble(col(row, "LengthNorm"));
      c.lenContrib = toDouble(col(row, "LengthContrib"));

      c.stabRaw = toDouble(col(row, "StabilityRaw"));
      c.stabNorm = toDouble(col(row, "StabilityNorm"));
      c.stabContrib = toDouble(col(row, "StabilityContrib"));

      c.domRaw = toDouble(col(row, "DomainRaw"));
      c.domNorm = toDouble(col(row, "DomainNorm"));
      c.domContrib = toDouble(col(row, "DomainContrib"));

      c.centerRaw = toDouble(col(row, "CenterShiftRaw"));
      c.centerNorm = toDouble(col(row, "CenterShiftNorm"));
      c.centerContrib = toDouble(col(row, "CenterShiftContrib"));

      c.skewRaw = toDouble(col(row, "SkewRaw"));
      c.skewNorm = toDouble(col(row, "SkewNorm"));
      c.skewContrib = toDouble(col(row, "SkewContrib"));

      c.bcaOverflowRaw = toDouble(col(row, "BCaOverflowRaw"));
      c.bcaOverflowNorm = toDouble(col(row, "BCaOverflowNorm"));
      c.bcaOverflowContrib = toDouble(col(row, "BCaOverflowContrib"));

      c.bcaAvailable = toBool(col(row, "BCa_Available"));
      c.bcaZ0 = toDouble(col(row, "BCa_Z0"));
      c.bcaAccel = toDouble(col(row, "BCa_Accel"));

      c.pctTAvailable = toBool(col(row, "PercentileT_Available"));
      c.pctTOuterFailCount = toSize(col(row, "PercentileT_OuterFailCount"));
      c.pctTInnerFailCount = toSize(col(row, "PercentileT_InnerFailCount"));
      c.pctTInnerFailRate = toDouble(col(row, "PercentileT_InnerFailRate"));

      out.push_back(std::move(c));
    }
    return out;
  }

  static void printChosenFrequency(const std::unordered_map<std::string, std::size_t>& chosenCounts,
                                  std::size_t totalTournaments)
  {
    std::cout << "Method chosen frequency:\n";
    auto v = sortedCounts(chosenCounts);
    for (const auto& [m, c] : v)
    {
      const double pct = (totalTournaments > 0)
        ? (100.0 * static_cast<double>(c) / static_cast<double>(totalTournaments))
        : 0.0;
      std::cout << "  " << std::left << std::setw(20) << m << " "
                << std::right << std::setw(5) << c
                << "  (" << std::fixed << std::setprecision(1) << pct << "%)\n";
    }
    std::cout.unsetf(std::ios::fixed);
  }

  static void printHead2Head(const Head2Head& hh)
  {
    std::cout << "PercentileT vs BCa head-to-head:\n";
    std::cout << "  Runs where both exist: " << hh.runsWhereBothExist << "\n\n";

    std::cout << "  PercentileT chosen: " << hh.ptChosen << "\n";
    std::cout << "    - BCa missing:            " << hh.ptChosen_bcaMissing << "\n";
    std::cout << "    - BCa rejected by gates:  " << hh.ptChosen_bcaRejected << "\n";
    std::cout << "    - BCa passed, lost score: " << hh.ptChosen_bcaPassedLostScore << "\n\n";

    std::cout << "  BCa chosen: " << hh.bcaChosen << "\n";
    std::cout << "    - PercentileT missing:            " << hh.bcaChosen_ptMissing << "\n";
    std::cout << "    - PercentileT rejected by gates:  " << hh.bcaChosen_ptRejected << "\n";
    std::cout << "    - PercentileT passed, lost score: " << hh.bcaChosen_ptPassedLostScore << "\n\n";

    std::cout << "Tie statistics (approx, using TieEpsilon):\n";
    std::cout << "  ties within epsilon (both pass): " << hh.tiesWithinEps << "\n";
    std::cout << "  ties decided in favor of BCa (preferred): " << hh.tiesPreferredDecided << "\n";
    std::cout << "  ties where PercentileT won (unexpected if truly a tie): " << hh.tiesNonPreferredWon << "\n\n";

    std::cout << "Score delta (PercentileT_score - BCa_score), only when both pass gates:\n";
    printDiffSummary("score", summarizeDiffs(hh.scoreDiff_pt_minus_bca));

    std::cout << "Per-component delta (PercentileT_contrib - BCa_contrib), only when both pass gates:\n";
    printDiffSummary("ordering", summarizeDiffs(hh.dOrd));
    printDiffSummary("length", summarizeDiffs(hh.dLen));
    printDiffSummary("stability", summarizeDiffs(hh.dStab));
    printDiffSummary("domain", summarizeDiffs(hh.dDom));
    printDiffSummary("centerShift", summarizeDiffs(hh.dCenter));
    printDiffSummary("skew", summarizeDiffs(hh.dSkew));
    printDiffSummary("bcaOverflow", summarizeDiffs(hh.dOverflow));

    std::cout << "\nTop BCa rejection bits (when both exist):\n";
    printTopCounts("", hh.bcaRejectBits, 10);

    std::cout << "\nTop PercentileT rejection bits (when both exist):\n";
    printTopCounts("", hh.ptRejectBits, 10);

    std::cout << "\nTop BCa rejection tokens (when both exist):\n";
    printTopCounts("", hh.bcaRejectTokens, 10);

    std::cout << "\nTop PercentileT rejection tokens (when both exist):\n";
    printTopCounts("", hh.ptRejectTokens, 10);

    std::cout << "\nTop BCa loss drivers (BCa passed but lost to PercentileT):\n";
    printTopCounts("", hh.bcaLossDrivers, 10);

    std::cout << "\nTop PercentileT loss drivers (PercentileT passed but lost to BCa):\n";
    printTopCounts("", hh.ptLossDrivers, 10);
  }

} // namespace

int main(int argc, char** argv)
{
  try
  {
    if (argc < 3)
    {
      std::cerr << "Usage: " << argv[0] << " <TournamentRuns.csv> <Candidates.csv>\n";
      return 2;
    }

    const std::string tournamentPath = argv[1];
    const std::string candidatesPath = argv[2];

    const auto tournaments = readTournamentRuns(tournamentPath);
    const auto candidates = readCandidates(candidatesPath);

    // Group candidates by runID
    std::unordered_map<std::uint64_t, std::vector<CandidateRow>> byRun;
    byRun.reserve(tournaments.size());
    for (const auto& c : candidates)
      byRun[c.runID].push_back(c);

    // Global chosen counts + stratified
    std::unordered_map<std::string, std::size_t> chosenCounts;
    std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>> chosenByMetric;
    std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>> chosenByBucket;

    Head2Head hhAll;
    std::unordered_map<std::string, Head2Head> hhByMetric;
    std::unordered_map<std::string, Head2Head> hhByBucket;

    std::size_t tournamentsWithNoChosen = 0;
    std::size_t tournamentsWithMultiChosen = 0;

    for (const auto& [runID, t] : tournaments)
    {
      const auto it = byRun.find(runID);
      if (it == byRun.end())
        continue;

      const auto& rows = it->second;

      std::optional<CandidateRow> chosen;
      std::optional<CandidateRow> pt;
      std::optional<CandidateRow> bca;

      std::size_t chosenCountInRun = 0;
      for (const auto& r : rows)
      {
        if (r.isChosen)
        {
          chosen = r;
          ++chosenCountInRun;
        }
        if (r.method == "PercentileT") pt = r;
        if (r.method == "BCa") bca = r;
      }

      if (chosenCountInRun == 0) tournamentsWithNoChosen++;
      if (chosenCountInRun > 1) tournamentsWithMultiChosen++;

      if (chosen)
      {
        chosenCounts[chosen->method]++;
        chosenByMetric[t.metric][chosen->method]++;
        chosenByBucket[sampleSizeBucket(t.sampleSize)][chosen->method]++;
      }

      updateHead2Head(hhAll, t, pt, bca, chosen);
      updateHead2Head(hhByMetric[t.metric], t, pt, bca, chosen);
      updateHead2Head(hhByBucket[sampleSizeBucket(t.sampleSize)], t, pt, bca, chosen);
    }

    // -------------------------------------------------------------------------
    // Additional diagnostics (low-effort, high-signal)
    // -------------------------------------------------------------------------
    struct ZeroStats {
      std::size_t total = 0;
      std::size_t ordRawFinite = 0;
      std::size_t ordRawZero = 0;
      std::size_t ordContribFinite = 0;
      std::size_t ordContribZero = 0;

      std::size_t lenRawFinite = 0;
      std::size_t lenRawZero = 0;
      std::size_t stabRawFinite = 0;
      std::size_t stabRawZero = 0;
    };

    std::unordered_map<std::string, ZeroStats> zeroStatsByMethod;
    for (const auto& c : candidates)
    {
      auto& zs = zeroStatsByMethod[c.method];
      zs.total++;

      if (std::isfinite(c.ordRaw)) { zs.ordRawFinite++; if (isExactZero(c.ordRaw, 0.0)) zs.ordRawZero++; }
      if (std::isfinite(c.ordContrib)) { zs.ordContribFinite++; if (isExactZero(c.ordContrib, 0.0)) zs.ordContribZero++; }

      if (std::isfinite(c.lenRaw)) { zs.lenRawFinite++; if (isExactZero(c.lenRaw, 0.0)) zs.lenRawZero++; }
      if (std::isfinite(c.stabRaw)) { zs.stabRawFinite++; if (isExactZero(c.stabRaw, 0.0)) zs.stabRawZero++; }
    }

    // Rank mismatch diagnostics: Rank is score-sort rank; IsChosen may differ due to gates or tie-preference.
    struct RankMismatchExample {
      std::uint64_t runID = 0;
      std::string metric;
      std::size_t N = 0;
      std::string chosenMethod;
      std::size_t chosenRank = 0;
      std::string rank1Method;
      bool rank1PassedGates = true;
      double chosenScore = std::numeric_limits<double>::quiet_NaN();
      double rank1Score = std::numeric_limits<double>::quiet_NaN();
    };

    std::size_t rankMismatch = 0;
    std::size_t rankMismatchDueToGates = 0;
    std::size_t rankMismatchDueToTieOrOther = 0;
    std::vector<RankMismatchExample> rankMismatchExamples;
    rankMismatchExamples.reserve(10);

    // Worst head-to-head ordering deltas (PT vs BCa, both pass gates)
    struct WorstOrderingRun {
      double deltaOrdContrib = std::numeric_limits<double>::quiet_NaN(); // PT - BCa
      std::uint64_t runID = 0;
      std::string metric;
      std::size_t N = 0;
      CandidateRow pt;
      CandidateRow bca;
    };
    std::vector<WorstOrderingRun> worstOrderingRuns;
    worstOrderingRuns.reserve(tournaments.size());

    for (const auto& [runID, t] : tournaments)
    {
      const auto it = byRun.find(runID);
      if (it == byRun.end())
        continue;

      const auto& rows = it->second;

      const CandidateRow* chosenPtr = nullptr;
      const CandidateRow* rank1Ptr = nullptr;
      const CandidateRow* ptPtr = nullptr;
      const CandidateRow* bcaPtr = nullptr;

      for (const auto& r : rows)
      {
        if (r.isChosen)
          chosenPtr = &r;
        if (r.rank == 1)
          rank1Ptr = &r;
        if (r.method == "PercentileT")
          ptPtr = &r;
        if (r.method == "BCa")
          bcaPtr = &r;
      }

      if (chosenPtr && rank1Ptr && chosenPtr->candidateID != rank1Ptr->candidateID)
      {
        rankMismatch++;
        if (!rank1Ptr->passedGates)
          rankMismatchDueToGates++;
        else
          rankMismatchDueToTieOrOther++;

        if (rankMismatchExamples.size() < 10)
        {
          RankMismatchExample ex;
          ex.runID = runID;
          ex.metric = t.metric;
          ex.N = t.sampleSize;
          ex.chosenMethod = chosenPtr->method;
          ex.chosenRank = chosenPtr->rank;
          ex.rank1Method = rank1Ptr->method;
          ex.rank1PassedGates = rank1Ptr->passedGates;
          ex.chosenScore = chosenPtr->finalScore;
          ex.rank1Score = rank1Ptr->finalScore;
          rankMismatchExamples.push_back(std::move(ex));
        }
      }

      if (ptPtr && bcaPtr && ptPtr->passedGates && bcaPtr->passedGates &&
          std::isfinite(ptPtr->ordContrib) && std::isfinite(bcaPtr->ordContrib))
      {
        WorstOrderingRun wr;
        wr.deltaOrdContrib = ptPtr->ordContrib - bcaPtr->ordContrib;
        wr.runID = runID;
        wr.metric = t.metric;
        wr.N = t.sampleSize;
        wr.pt = *ptPtr;
        wr.bca = *bcaPtr;
        worstOrderingRuns.push_back(std::move(wr));
      }
    }

    std::sort(worstOrderingRuns.begin(), worstOrderingRuns.end(),
              [](const WorstOrderingRun& a, const WorstOrderingRun& b){
                return a.deltaOrdContrib < b.deltaOrdContrib; // most negative first
              });

    // -------------------------------------------------------------------------
    // Print report
    // -------------------------------------------------------------------------
    std::cout << "==============================================================================\n";
    std::cout << "Bootstrap Tournament Summary (two-file schema)\n";
    std::cout << "==============================================================================\n";
    std::cout << "Total tournaments: " << tournaments.size() << "\n";
    if (tournamentsWithNoChosen > 0 || tournamentsWithMultiChosen > 0)
    {
      std::cout << "(data quality) tournaments with no chosen candidate: " << tournamentsWithNoChosen << "\n";
      std::cout << "(data quality) tournaments with multiple chosen candidates: " << tournamentsWithMultiChosen << "\n";
    }
    std::cout << "\n";

    printChosenFrequency(chosenCounts, tournaments.size());
    std::cout << "\n";

    printHead2Head(hhAll);

    // Stratified summaries
    std::cout << "\n==============================================================================\n";
    std::cout << "Stratified by Metric\n";
    std::cout << "==============================================================================\n";

    for (const auto& [metric, counts] : chosenByMetric)
    {
      std::cout << "\n-- Metric: " << metric << "\n";
      // Compute total tournaments in this metric stratum
      std::size_t total_in_metric = 0;
      for (const auto& [method, count] : counts) {
        total_in_metric += count;
      }
      
      printChosenFrequency(counts, total_in_metric); // pct is over this metric's tournaments
      std::cout << "\n";
      printHead2Head(hhByMetric[metric]);
    }

    std::cout << "\n==============================================================================\n";
    std::cout << "Stratified by SampleSize bucket\n";
    std::cout << "==============================================================================\n";

    // Print buckets in natural order
    const std::vector<std::string> buckets = {"<20", "20-24", "25-29", "30-39", "40+"};
    for (const auto& bucket : buckets)
    {
      const auto it = chosenByBucket.find(bucket);
      if (it == chosenByBucket.end())
        continue;
      std::cout << "\n-- N bucket: " << bucket << "\n";
      // Compute total tournaments in this bucket
      std::size_t total_in_bucket = 0;
      for (const auto& [method, count] : it->second) {
        total_in_bucket += count;
      }
      
      printChosenFrequency(it->second, total_in_bucket); // pct is over this bucket's tournaments
      std::cout << "\n";
      printHead2Head(hhByBucket[bucket]);
    }

    // -------------------------------------------------------------------------
    // Rank vs selection consistency
    // -------------------------------------------------------------------------
    std::cout << "\n==============================================================================\n";
    std::cout << "Rank vs Selection Consistency\n";
    std::cout << "==============================================================================\n";
    std::cout << "Runs where Rank==1 candidate is NOT the chosen winner: " << rankMismatch << "\n";
    std::cout << "  - Rank==1 failed gates (gating mismatch): " << rankMismatchDueToGates << "\n";
    std::cout << "  - Rank==1 passed gates (likely tie/preference or other): " << rankMismatchDueToTieOrOther << "\n";
    if (!rankMismatchExamples.empty())
    {
      std::cout << "\nExamples (up to 10):\n";
      std::cout << "  RunID, Metric, N, ChosenMethod(ChosenRank), Rank1Method(Rank1Passed)\n";
      for (const auto& ex : rankMismatchExamples)
      {
        std::cout << "  " << ex.runID << ", " << ex.metric << ", " << ex.N << ", "
                  << ex.chosenMethod << "(" << ex.chosenRank << "), "
                  << ex.rank1Method << "(" << (ex.rank1PassedGates ? "PASS" : "FAIL") << ")\n";
      }
    }

    // -------------------------------------------------------------------------
    // How often ordering is literally zero (per method)
    // -------------------------------------------------------------------------
    std::vector<std::pair<std::string, ZeroStats>> zeroVec(zeroStatsByMethod.begin(), zeroStatsByMethod.end());
    std::sort(zeroVec.begin(), zeroVec.end(),
              [](const auto& a, const auto& b){ return a.second.total > b.second.total; });

    std::cout << "\n==============================================================================\n";
    std::cout << "How often ordering is zero (per method)\n";
    std::cout << "==============================================================================\n";
    std::cout << "Method, Candidates, %OrderingRaw==0, %OrderingContrib==0\n";
    for (const auto& [method, zs] : zeroVec)
    {
      const double pctRaw0 = (zs.ordRawFinite > 0) ? (100.0 * static_cast<double>(zs.ordRawZero) / static_cast<double>(zs.ordRawFinite)) : 0.0;
      const double pctC0   = (zs.ordContribFinite > 0) ? (100.0 * static_cast<double>(zs.ordContribZero) / static_cast<double>(zs.ordContribFinite)) : 0.0;
      std::cout << "  " << method << ", " << zs.total << ", "
                << std::fixed << std::setprecision(1) << pctRaw0 << "%, "
                << std::fixed << std::setprecision(1) << pctC0 << "%\n";
    }
    std::cout.unsetf(std::ios::floatfield);

    // -------------------------------------------------------------------------
    // Worst head-to-head runs by ordering delta
    // -------------------------------------------------------------------------
    std::cout << "\n==============================================================================\n";
    std::cout << "Worst PT vs BCa head-to-head runs by ordering delta\n";
    std::cout << "(both pass gates; sorted by PT.orderingContrib - BCa.orderingContrib, most negative first)\n";
    std::cout << "==============================================================================\n";

    const std::size_t kWorstToPrint = 20;
    const std::size_t worstCount = std::min<std::size_t>(kWorstToPrint, worstOrderingRuns.size());
    if (worstCount == 0)
    {
      std::cout << "(none)\n";
    }
    else
    {
      for (std::size_t i = 0; i < worstCount; ++i)
      {
        const auto& wr = worstOrderingRuns[i];
        std::cout << "\n#" << (i+1) << " RunID=" << wr.runID
                  << " Metric=" << wr.metric
                  << " N=" << wr.N
                  << " deltaOrdContrib=" << std::setprecision(17) << wr.deltaOrdContrib << "\n";

        auto printMethodLine = [&](const char* label, const CandidateRow& c){
          std::cout << "  " << label << " score=";
          writeDoubleOrEmpty(std::cout, c.finalScore);
          std::cout << "  orderingRaw=";
          writeDoubleOrEmpty(std::cout, c.ordRaw);
          std::cout << "  orderingNorm=";
          writeDoubleOrEmpty(std::cout, c.ordNorm);
          std::cout << "  orderingContrib=";
          writeDoubleOrEmpty(std::cout, c.ordContrib);
          std::cout << "\n";
        };

        printMethodLine("PT ", wr.pt);
        printMethodLine("BCa", wr.bca);

        std::cout << "  BCa z0=";
        writeDoubleOrEmpty(std::cout, wr.bca.bcaZ0);
        std::cout << "  accel=";
        writeDoubleOrEmpty(std::cout, wr.bca.bcaAccel);
        std::cout << "  stabilityRaw=";
        writeDoubleOrEmpty(std::cout, wr.bca.stabRaw);
        std::cout << "  stabilityContrib=";
        writeDoubleOrEmpty(std::cout, wr.bca.stabContrib);
        std::cout << "\n";
      }
    }

    std::cout << "\nNotes:\n";
    std::cout << "- 'rejected by gates' uses PassedGates=false (as logged), plus mask/text breakdowns.\n";
    std::cout << "- 'Score delta' is PercentileT_score - BCa_score on runs where both passed gates.\n";
    std::cout << "- 'Tie statistics' uses TournamentRuns.TieEpsilon as an approximation for the PT-vs-BCa tie check.\n";
    std::cout << "- 'loss drivers' compare weighted score contributions between the loser and winner.\n";
    std::cout << "- 'Rank' is the score-sort rank from the CSV; selection may differ due to gating and/or tie-break preferences.\n";

    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
