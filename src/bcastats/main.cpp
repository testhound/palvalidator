#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <iomanip>
#include <cmath>
#include <limits>
#include <optional>
#include <cstdint>
#include "AutoBootstrapConfiguration.h"

using namespace std;

// =================================================================================
// CONFIGURATION: Thresholds (must match AutoBootstrapSelector.h)
// =================================================================================
namespace Thresholds
{
  // BCa Hard Limits (from AutoBootstrapSelector.h)
  constexpr double BCa_Z0_HARD = AutoBootstrapConfiguration::kBcaZ0HardLimit;
  constexpr double BCa_ACCEL_HARD = AutoBootstrapConfiguration::kBcaAHardLimit;
  constexpr double BCa_LENGTH_REJECT = AutoBootstrapConfiguration::kBcaLengthPenaltyThreshold;
    
  // BCa Soft Limits (from AutoBootstrapSelector.h)
  constexpr double BCa_Z0_SOFT = AutoBootstrapConfiguration::kBcaZ0SoftThreshold;
  constexpr double BCa_ACCEL_SOFT = AutoBootstrapConfiguration::kBcaASoftThreshold;
  constexpr double BCa_STABILITY_SOFT = 0.1;    // Soft penalty threshold
    
  // BCa Viability Check (midpoint between soft and hard)
  constexpr double BCa_Z0_VIABLE = (BCa_Z0_SOFT + BCa_Z0_HARD) / 2.0;
  constexpr double BCa_ACCEL_VIABLE = (BCa_ACCEL_SOFT + BCa_ACCEL_HARD) / 2.0;
    
  // PercentileT Quality Checks
  // CORRECTED: Use values from AutoBootstrapConfiguration to match C++ tournament logic
  constexpr double PERCENTILET_WIDE_THRESHOLD = AutoBootstrapConfiguration::kLengthMaxStandard;  // 1.8 (was 1.5)
  constexpr double PERCENTILET_INNER_FAIL_THRESHOLD = AutoBootstrapConfiguration::kPercentileTInnerFailThreshold;  // 0.05 (was 0.03)
  constexpr double PERCENTILET_ASYMMETRY_THRESHOLD = 2.0;   // 2 standard errors off-center (forensic diagnostic only)
    
  // For normalized length calculation (95% confidence level)
  constexpr double Z_ALPHA_95 = 1.96;           // Two-sided 95% CI quantile
}

// =================================================================================
// 1. UTILITY & PARSING
// =================================================================================

// Trim whitespace from string
static string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

double parseDouble(const string& s, double def = 0.0) {
    string t = trim(s);
    if (t.empty()) return def;
    try { return stod(t); } catch (...) { return def; }
}

uint64_t parseUInt64(const string& s, uint64_t def = 0) {
    string t = trim(s);
    if (t.empty()) return def;
    try { return stoull(t); } catch (...) { return def; }
}

bool parseBool(const string& s) {
    string t = trim(s);
    transform(t.begin(), t.end(), t.begin(), ::toupper);
    return (t == "TRUE" || t == "1");
}

// Simple Median helper
static double medianOf(vector<double> v) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 1) return v[n/2];
    return (v[n/2 - 1] + v[n/2]) / 2.0;
}

static double minOf(const vector<double>& v) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    return *min_element(v.begin(), v.end());
}

static double maxOf(const vector<double>& v) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    return *max_element(v.begin(), v.end());
}

// =================================================================================
// 2. DATA STRUCTURES
// =================================================================================

struct CandidateRecord {
    uint64_t strategyId;   // NEW: Unique strategy identifier
    string strategy;
    string symbol;
    string metric;
    string method;
    bool isChosen;
    double score;
    
    // BCa specific
    double bcaZ0;
    double bcaAccel;
    double bcaStabPenalty;
    double bcaLenPenalty;
    
    // General Stats
    double se;
    double skew;
    double bootMedian;
    double effB;
    double innerFail;
    double lb;
    double ub;
    double n;
    
    // Helpers
    double rawLen() const { return ub - lb; }
    
    // Normalized length calculation
    double normalizedLen() const { 
        if (se < 1e-9) return 0.0;
        const double idealWidth = 2.0 * Thresholds::Z_ALPHA_95 * se;
        return rawLen() / idealWidth;
    }
    
    // Asymmetry relative to interval midpoint
    double asymmetry() const { 
        double mid = (lb + ub) / 2.0;
        return (se > 1e-9) ? (std::abs(mid - bootMedian) / se) : 0.0;
    }
};

struct TournamentGroup {
    string id;              // Human-readable ID (for display)
    uint64_t strategyId;    // Unique strategy ID
    vector<CandidateRecord> candidates;
    
    const CandidateRecord* getWinner() const {
        for (const auto& c : candidates) {
            if (c.isChosen) return &c;
        }
        return nullptr;
    }
    const CandidateRecord* getMethod(const string& methodName) const {
        for (const auto& c : candidates) {
            if (c.method == methodName) return &c;
        }
        return nullptr;
    }
};

// Tournament key: combines strategyId + metric
struct TournamentKey {
    uint64_t strategyId;
    string metric;
    
    bool operator<(const TournamentKey& other) const {
        if (strategyId != other.strategyId) return strategyId < other.strategyId;
        return metric < other.metric;
    }
};

struct StatsVecs {
    vector<double> score, z0, accel, stab, lenpen, rawlen, se, skew, lb, ub, n;
    
    // Tracking
    map<string, int> methodCounts;
    int totalCount = 0;
    
    // ENHANCED BCa Diagnostics
    int bcaChosen = 0;
    int bcaRejectedHighZ0 = 0;
    int bcaRejectedHighAccel = 0;
    int bcaRejectedStability = 0;
    int bcaRejectedLength = 0;
    int bcaRejectedNonFinite = 0;
    int bcaLostOnScore = 0;
    
    // Binning
    map<string, map<int, int>> methodByNBin;
    vector<int> nBinCounts = {0, 0, 0, 0};
    
    map<string, map<int, int>> methodBySkewBin;
    vector<int> skewBinCounts = {0, 0, 0, 0, 0};
    
    map<string, int> methodCountsHighSkew;
    int totalHighSkew = 0;
};

// =================================================================================
// 3. LOGIC FOR GENERAL REPORTING
// =================================================================================

static int getSampleSizeBin(double n) {
    if (n < 30) return 0;
    if (n < 50) return 1;
    if (n < 100) return 2;
    return 3;
}

static int getSkewnessBin(double skew) {
    if (skew < -2.0) return 0;
    if (skew < -1.0) return 1;
    if (skew < 1.0) return 2;
    if (skew < 2.0) return 3;
    return 4;
}

static string getSampleSizeBinLabel(int bin) {
    switch (bin) {
        case 0: return "n < 30";
        case 1: return "30 <= n < 50";
        case 2: return "50 <= n < 100";
        case 3: return "n >= 100";
        default: return "Unknown";
    }
}

static string getSkewnessBinLabel(int bin) {
    switch (bin) {
        case 0: return "skew < -2.0";
        case 1: return "-2.0 <= skew < -1.0";
        case 2: return "-1.0 <= skew < 1.0";
        case 3: return "1.0 <= skew < 2.0";
        case 4: return "skew >= 2.0";
        default: return "Unknown";
    }
}

static void updateStatsBucket(StatsVecs& s, const CandidateRecord& c) {
    
    // Track BCa rejections even when not chosen
    if (!c.isChosen && c.method == "BCa") {
        bool rejected = false;
        
        if (!std::isfinite(c.bcaZ0) || !std::isfinite(c.bcaAccel)) {
            s.bcaRejectedNonFinite++;
            rejected = true;
        }
        
        if (std::abs(c.bcaZ0) > Thresholds::BCa_Z0_HARD) {
            s.bcaRejectedHighZ0++;
            rejected = true;
        }
        
        if (std::abs(c.bcaAccel) > Thresholds::BCa_ACCEL_HARD) {
            s.bcaRejectedHighAccel++;
            rejected = true;
        }
        
        if (c.bcaStabPenalty > Thresholds::BCa_STABILITY_SOFT) {
            s.bcaRejectedStability++;
            rejected = true;
        }
        
        if (c.bcaLenPenalty > Thresholds::BCa_LENGTH_REJECT) {
            s.bcaRejectedLength++;
            rejected = true;
        }
        
        if (!rejected) {
            s.bcaLostOnScore++;
        }
        
        return;
    }
    
    // Only count winners for frequency statistics
    if (!c.isChosen) return;

    s.score.push_back(c.score);
    s.z0.push_back(c.bcaZ0);
    s.accel.push_back(c.bcaAccel);
    s.stab.push_back(c.bcaStabPenalty);
    s.lenpen.push_back(c.bcaLenPenalty);
    s.rawlen.push_back(c.rawLen());
    s.se.push_back(c.se);
    s.skew.push_back(c.skew);
    s.lb.push_back(c.lb);
    s.ub.push_back(c.ub);
    s.n.push_back(c.n);
    
    s.methodCounts[c.method]++;
    s.totalCount++;
    
    if (c.method == "BCa") s.bcaChosen++;
    
    // Binning
    if (c.n > 0) {
        int nBin = getSampleSizeBin(c.n);
        s.nBinCounts[nBin]++;
        s.methodByNBin[c.method][nBin]++;
    }
    
    int skewBin = getSkewnessBin(c.skew);
    s.skewBinCounts[skewBin]++;
    s.methodBySkewBin[c.method][skewBin]++;
    
    if (std::abs(c.skew) >= 2.0) {
        s.totalHighSkew++;
        s.methodCountsHighSkew[c.method]++;
    }
}

static void printSummary(const string& label, const vector<double>& v) {
    if (v.empty()) {
        cout << "  " << setw(20) << left << label << ": (no data)\n";
        return;
    }
    cout << "  " << setw(20) << left << label 
         << ": min=" << scientific << setprecision(6) << minOf(v)
         << " max=" << maxOf(v)
         << "   median=" << medianOf(v) << "\n";
}

static void printMethodFrequency(const StatsVecs& sv) {
    if (sv.totalCount == 0) return;
    
    cout << "\n  METHOD SELECTION FREQUENCY:\n  " << string(60, '-') << "\n";
    
    vector<pair<string, int>> sorted(sv.methodCounts.begin(), sv.methodCounts.end());
    sort(sorted.begin(), sorted.end(), 
         [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (const auto& [method, count] : sorted) {
        double pct = 100.0 * count / sv.totalCount;
        cout << "    " << setw(15) << left << method 
             << ": " << setw(5) << right << count 
             << " / " << setw(4) << sv.totalCount
             << " (" << fixed << setprecision(1) << pct << "%)\n";
    }
}

static void printMethodBySampleSize(const StatsVecs& sv) {
    cout << "\n  METHOD SELECTION BY SAMPLE SIZE:\n  " << string(70, '-') << "\n";
    
    for (int bin = 0; bin < 4; ++bin) {
        if (sv.nBinCounts[bin] == 0) continue;
        
        cout << "  " << getSampleSizeBinLabel(bin) << " (n=" << sv.nBinCounts[bin] << "):\n";
        
        vector<pair<string, int>> methodsInBin;
        for (const auto& [method, binMap] : sv.methodByNBin) {
            auto it = binMap.find(bin);
            if (it != binMap.end()) {
                methodsInBin.push_back({method, it->second});
            }
        }
        sort(methodsInBin.begin(), methodsInBin.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (const auto& [method, count] : methodsInBin) {
            double pct = 100.0 * count / sv.nBinCounts[bin];
            cout << "    " << setw(15) << left << method
                 << ": " << setw(4) << right << count
                 << " (" << fixed << setprecision(1) << setw(5) << right << pct << "%)\n";
        }
        cout << "\n";
    }
}

static void printMethodBySkewness(const StatsVecs& sv) {
    cout << "\n  METHOD SELECTION BY SKEWNESS:\n  " << string(70, '-') << "\n";
    
    for (int bin = 0; bin < 5; ++bin) {
        if (sv.skewBinCounts[bin] == 0) continue;
        
        cout << "  " << getSkewnessBinLabel(bin) << " (n=" << sv.skewBinCounts[bin] << "):\n";
        
        vector<pair<string, int>> methodsInBin;
        for (const auto& [method, binMap] : sv.methodBySkewBin) {
            auto it = binMap.find(bin);
            if (it != binMap.end()) {
                methodsInBin.push_back({method, it->second});
            }
        }
        sort(methodsInBin.begin(), methodsInBin.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (const auto& [method, count] : methodsInBin) {
            double pct = 100.0 * count / sv.skewBinCounts[bin];
            cout << "    " << setw(15) << left << method
                 << ": " << setw(4) << right << count
                 << " (" << fixed << setprecision(1) << setw(5) << right << pct << "%)\n";
        }
        cout << "\n";
    }
}

static void printBCaDiagnostics(const StatsVecs& sv) {
    int bcaTotal = sv.bcaChosen 
                 + sv.bcaRejectedHighZ0 
                 + sv.bcaRejectedHighAccel 
                 + sv.bcaRejectedStability 
                 + sv.bcaRejectedLength
                 + sv.bcaRejectedNonFinite
                 + sv.bcaLostOnScore;
                 
    if (bcaTotal == 0) return;
    
    cout << "\n  BCa DIAGNOSTICS:\n  " << string(60, '-') << "\n";
    auto printPct = [&](string l, int c) {
        cout << "    " << setw(30) << left << l << ": " << setw(4) << right << c
             << " (" << fixed << setprecision(1) << (100.0*c/bcaTotal) << "%)\n";
    };
    
    printPct("BCa Chosen (Winner)", sv.bcaChosen);
    printPct("BCa Lost on Score", sv.bcaLostOnScore);
    printPct("Rejected: |z0| > 0.6", sv.bcaRejectedHighZ0);
    printPct("Rejected: |a| > 0.25", sv.bcaRejectedHighAccel);
    printPct("Rejected: Stability", sv.bcaRejectedStability);
    printPct("Rejected: Length", sv.bcaRejectedLength);
    printPct("Rejected: Non-Finite", sv.bcaRejectedNonFinite);
    
    cout << "\n  NOTE: A single BCa candidate can be rejected for multiple reasons.\n";
    cout << "        Rejection counts may sum to more than total rejections.\n";
}

// =================================================================================
// 4. FORENSIC ANALYSIS LOGIC
// =================================================================================

struct ForensicReport {
    int totalGroups = 0;
    int percTWins = 0;
    
    int percT_Wins_BCa_Viable = 0;
    int percT_Wins_BCa_NotViable = 0;
    
    int percT_WideNormLen = 0;
    int percT_HighAsymmetry = 0;
    
    int percT_HighInnerFail = 0;
    
    int percT_vs_MOutOfN_Wide = 0;
    
    int smokingGun_BCa_FakeReject = 0;
};

void analyzeGroup(const TournamentGroup& group, ForensicReport& report) {
    const auto* winner = group.getWinner();
    if (!winner) return;
    
    report.totalGroups++;
    
    if (winner->method != "PercentileT") return;
    report.percTWins++;

    if (winner->normalizedLen() > Thresholds::PERCENTILET_WIDE_THRESHOLD) {
        report.percT_WideNormLen++;
    }
    
    if (std::abs(winner->skew) < 1.0 && 
        winner->asymmetry() > Thresholds::PERCENTILET_ASYMMETRY_THRESHOLD) {
        report.percT_HighAsymmetry++;
    }
    
    if (winner->innerFail > Thresholds::PERCENTILET_INNER_FAIL_THRESHOLD) {
        report.percT_HighInnerFail++;
    }

    const auto* bca = group.getMethod("BCa");
    if (bca) {
        bool bcaParamsViable = (std::abs(bca->bcaZ0) < Thresholds::BCa_Z0_VIABLE && 
                               std::abs(bca->bcaAccel) < Thresholds::BCa_ACCEL_VIABLE &&
                               std::isfinite(bca->bcaZ0) &&
                               std::isfinite(bca->bcaAccel));
        
        if (bcaParamsViable) {
            bool hardRejected = (std::abs(bca->bcaZ0) > Thresholds::BCa_Z0_HARD ||
                                std::abs(bca->bcaAccel) > Thresholds::BCa_ACCEL_HARD ||
                                bca->bcaLenPenalty > Thresholds::BCa_LENGTH_REJECT ||
                                !std::isfinite(bca->bcaZ0) ||
                                !std::isfinite(bca->bcaAccel));
            
            if (hardRejected) {
                report.smokingGun_BCa_FakeReject++;
            } else {
                report.percT_Wins_BCa_Viable++;
            }
        } else {
            report.percT_Wins_BCa_NotViable++;
        }
    }

    const auto* moon = group.getMethod("MOutOfN");
    if (moon && moon->rawLen() > 1e-9 && winner->rawLen() > 1e-9) {
        double ratio = winner->rawLen() / moon->rawLen();
        if (ratio > 2.0) {
            report.percT_vs_MOutOfN_Wide++;
        }
    }
}

// =================================================================================
// 5. VALIDATION & ERROR CHECKING
// =================================================================================

struct ValidationReport {
    int groupsWithNoWinner = 0;
    int groupsWithMultipleWinners = 0;
    int groupsWithNoCandidates = 0;
    vector<string> problematicGroups;
};

void validateTournaments(const map<TournamentKey, TournamentGroup>& tournaments, 
                        ValidationReport& valReport) {
    for (const auto& [key, group] : tournaments) {
        if (group.candidates.empty()) {
            valReport.groupsWithNoCandidates++;
            valReport.problematicGroups.push_back(group.id + " (no candidates)");
            continue;
        }
        
        int winnerCount = 0;
        for (const auto& c : group.candidates) {
            if (c.isChosen) winnerCount++;
        }
        
        if (winnerCount == 0) {
            valReport.groupsWithNoWinner++;
            valReport.problematicGroups.push_back(group.id + " (no winner)");
        } else if (winnerCount > 1) {
            valReport.groupsWithMultipleWinners++;
            valReport.problematicGroups.push_back(group.id + " (multiple winners)");
        }
    }
}

void printValidationReport(const ValidationReport& vr, size_t totalGroups) {
    if (vr.groupsWithNoCandidates == 0 && 
        vr.groupsWithNoWinner == 0 && 
        vr.groupsWithMultipleWinners == 0) {
        cout << "\n[VALIDATION] All tournament groups are valid.\n";
        return;
    }
    
    cout << "\n[VALIDATION WARNINGS]\n";
    cout << "  Groups with no candidates: " << vr.groupsWithNoCandidates << "\n";
    cout << "  Groups with no winner: " << vr.groupsWithNoWinner << "\n";
    cout << "  Groups with multiple winners: " << vr.groupsWithMultipleWinners << "\n";
    
    if (vr.groupsWithMultipleWinners > 0) {
        double pct = 100.0 * vr.groupsWithMultipleWinners / totalGroups;
        cout << "\n  NOTE: With StrategyID-based grouping, multiple winners should not occur.\n";
        cout << "        If present (" << fixed << setprecision(2) << pct 
             << "%), this indicates a data integrity issue.\n";
    }
    
    if (!vr.problematicGroups.empty()) {
        cout << "\n  Problematic groups (first 10):\n";
        for (size_t i = 0; i < min(size_t(10), vr.problematicGroups.size()); ++i) {
            cout << "    - " << vr.problematicGroups[i] << "\n";
        }
    }
}

// =================================================================================
// 6. REPORTING FUNCTIONS
// =================================================================================

void printForensicReport(const string& title, const ForensicReport& r) {
    cout << "\n==============================================================================\n";
    cout << "                         FORENSIC REPORT: " << title << "\n";
    cout << "==============================================================================\n";
    cout << " Total Groups:                 " << r.totalGroups << "\n";
    cout << " PercentileT Wins:             " << r.percTWins << "\n";
    
    if (r.percTWins == 0) return;

    double pct = 100.0 / r.percTWins;
    auto p = [&](int v) { 
        cout << setw(5) << v << " (" << fixed << setprecision(1) << v*pct << "%)"; 
    };

    cout << "\n [Q1] BCa Viability (when PercT won):\n";
    cout << "   BCa params were Viable:     "; p(r.percT_Wins_BCa_Viable); cout << "\n";
    cout << "   BCa params Not Viable:      "; p(r.percT_Wins_BCa_NotViable); cout << "\n";
    cout << "   [SMOKING GUN] Fake Reject:  "; p(r.smokingGun_BCa_FakeReject); cout << "\n";
    cout << "\n   NOTE: 'Fake Reject' means BCa had viable parameters (|z0|<0.4, |a|<0.175)\n";
    cout << "         but was hard-rejected anyway. This suggests overly strict thresholds.\n";

    cout << "\n [Q3] Interval Quality:\n";
    cout << "   Suspiciously Wide:          "; p(r.percT_WideNormLen); cout << "\n";
    cout << "   High Asymmetry:             "; p(r.percT_HighAsymmetry); cout << "\n";
    cout << "\n   NOTE: 'Suspiciously Wide' = normalized_length > " 
         << Thresholds::PERCENTILET_WIDE_THRESHOLD << "\n";
    cout << "         where normalized_length = interval_width / (2 * " 
         << Thresholds::Z_ALPHA_95 << " * SE)\n";

    cout << "\n [Q4] Inner Health:\n";
    cout << "   High Inner Fail (>" << (Thresholds::PERCENTILET_INNER_FAIL_THRESHOLD * 100) 
         << "%):      "; p(r.percT_HighInnerFail); cout << "\n";

    cout << "\n [Q6] vs M-Out-Of-N:\n";
    cout << "   PercT > 2x Width:           "; p(r.percT_vs_MOutOfN_Wide); cout << "\n";
}

// =================================================================================
// 7. MAIN
// =================================================================================

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <csv_file>\n";
        cerr << "\nExpected CSV format (20 columns):\n";
        cerr << "  StrategyID,Strategy,Symbol,Metric,Method,IsChosen,Score,\n";
        cerr << "  BCa_Z0,BCa_Accel,BCa_StabPenalty,BCa_LenPenalty,BCa_RawLen,\n";
        cerr << "  SE,Skew,BootMedian,EffB,InnerFail,LB,UB,N\n";
        return 1;
    }
    
    string path = argv[1];
    ifstream ifs(path);
    if (!ifs.is_open()) { 
        cerr << "ERROR: Failed to open " << path << endl; 
        return 2; 
    }

    string line;
    if (!getline(ifs, line)) {
        cerr << "ERROR: CSV file is empty\n";
        return 3;
    }

    // Validate header
    if (line.find("StrategyID") == string::npos) {
        cerr << "ERROR: Header does not contain 'StrategyID' as first column.\n";
        cerr << "       This tool requires the updated CSV format with StrategyID.\n";
        return 4;
    }
    if (line.find("IsChosen") == string::npos) {
        cerr << "WARNING: Header does not contain 'IsChosen'.\n";
    }
    if (line.find("InnerFail") == string::npos) {
        cerr << "WARNING: Header does not contain 'InnerFail'.\n";
    }

    // Tournament groups keyed by (StrategyID, Metric)
    map<TournamentKey, TournamentGroup> tournaments;
    map<string, StatsVecs> statsBuckets;
    
    int lineNumber = 1;
    int skippedLines = 0;

    // READ LOOP
    while (getline(ifs, line)) {
        lineNumber++;
        if (line.empty()) continue;
        
        stringstream ss(line);
        string tok;
        vector<string> toks;
        while (getline(ss, tok, ',')) toks.push_back(tok);

        // Expect exactly 20 columns (was 19, now +1 for StrategyID)
        if (toks.size() < 20) {
            cerr << "WARNING: Line " << lineNumber << " has only " << toks.size() 
                 << " columns (expected 20). Skipping.\n";
            skippedLines++;
            continue;
        }

        CandidateRecord c;
        c.strategyId = parseUInt64(toks[0]);  // NEW: First column
        c.strategy = trim(toks[1]);
        c.symbol   = trim(toks[2]);
        c.metric   = trim(toks[3]);
        c.method   = trim(toks[4]);
        c.isChosen = parseBool(toks[5]);
        
        c.score    = parseDouble(toks[6]);
        c.bcaZ0    = parseDouble(toks[7]);
        c.bcaAccel = parseDouble(toks[8]);
        c.bcaStabPenalty = parseDouble(toks[9]);
        c.bcaLenPenalty  = parseDouble(toks[10]);
        // toks[11] is BCa_RawLen (redundant)
        
        c.se       = parseDouble(toks[12]);
        c.skew     = parseDouble(toks[13]);
        c.bootMedian = parseDouble(toks[14]);
        c.effB     = parseDouble(toks[15]);
        c.innerFail= parseDouble(toks[16]);
        c.lb       = parseDouble(toks[17]);
        c.ub       = parseDouble(toks[18]);
        c.n        = parseDouble(toks[19]);

        // 1. Add to Tournament Group (keyed by StrategyID + Metric)
        TournamentKey key{c.strategyId, c.metric};
        
        if (tournaments.find(key) == tournaments.end()) {
            tournaments[key].id = c.strategy + "|" + c.symbol + "|" + c.metric;
            tournaments[key].strategyId = c.strategyId;
        }
        
        tournaments[key].candidates.push_back(c);

        // 2. Add to Stats Bucket (for General Report)
        updateStatsBucket(statsBuckets[c.metric], c);
        if (c.metric == "GeoMean" || c.metric == "ProfitFactor") {
            updateStatsBucket(statsBuckets["Combined"], c);
        }
    }
    
    ifs.close();

    // Print parsing summary
    cout << "\n[PARSING SUMMARY]\n";
    cout << "  Total lines read: " << lineNumber << "\n";
    cout << "  Skipped lines: " << skippedLines << "\n";
    cout << "  Tournament groups: " << tournaments.size() << "\n";
    cout << "  (Grouped by StrategyID + Metric for unique identification)\n";
    
    // Validation
    ValidationReport valReport;
    validateTournaments(tournaments, valReport);
    printValidationReport(valReport, tournaments.size());

    // REPORTING - PART 1: GENERAL STATS
    vector<string> reportOrder = {"GeoMean", "ProfitFactor", "Combined"};
    cout << "\n==============================================================================\n";
    cout << "                         GENERAL STATISTICAL REPORT                           \n";
    cout << "==============================================================================\n";

    for (const auto& metric : reportOrder) {
        if (statsBuckets.find(metric) == statsBuckets.end()) continue;
        const auto& sv = statsBuckets.at(metric);
        
        cout << "\n>>> METRIC: " << metric << "\n";
        printSummary("Score", sv.score);
        printSummary("SE", sv.se);
        printSummary("Skew", sv.skew);
        printSummary("N", sv.n);
        printMethodFrequency(sv);
        printBCaDiagnostics(sv);
        printMethodBySampleSize(sv);
        printMethodBySkewness(sv);
    }

    // REPORTING - PART 2: FORENSIC ANALYSIS
    ForensicReport statsGeo, statsPF;
    for (const auto& [key, group] : tournaments) {
        if (key.metric == "GeoMean") {
            analyzeGroup(group, statsGeo);
        } else if (key.metric == "ProfitFactor") {
            analyzeGroup(group, statsPF);
        }
    }

    printForensicReport("GeoMean", statsGeo);
    printForensicReport("ProfitFactor", statsPF);

    cout << "\n==============================================================================\n";
    cout << "                         CONFIGURATION USED                                   \n";
    cout << "==============================================================================\n";
    cout << "BCa Hard Limits:\n";
    cout << "  |z0| > " << Thresholds::BCa_Z0_HARD << "\n";
    cout << "  |accel| > " << Thresholds::BCa_ACCEL_HARD << "\n";
    cout << "  length_penalty > " << Thresholds::BCa_LENGTH_REJECT << "\n";
    cout << "\nBCa Viability Thresholds (for forensics):\n";
    cout << "  |z0| < " << Thresholds::BCa_Z0_VIABLE << "\n";
    cout << "  |accel| < " << Thresholds::BCa_ACCEL_VIABLE << "\n";
    cout << "\nPercentileT Quality Thresholds:\n";
    cout << "  normalized_length > " << Thresholds::PERCENTILET_WIDE_THRESHOLD << "\n";
    cout << "  inner_failure_rate > " << Thresholds::PERCENTILET_INNER_FAIL_THRESHOLD << "\n";
    cout << "\nGrouping Strategy:\n";
    cout << "  Using StrategyID from CSV (unique per strategy instance)\n";
    cout << "  Tournament Key = (StrategyID, Metric)\n";
    cout << "==============================================================================\n";

    return 0;
}
