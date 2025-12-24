#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <iomanip>
#include <cmath>
#include "libs/timeseries/TimeSeriesIndicators.h"

using namespace std;

// Structure to hold vectors of statistics for a metric
struct StatsVecs {
    vector<double> score;
    vector<double> z0;
    vector<double> accel;
    vector<double> stab;
    vector<double> lenpen;
    vector<double> rawlen;
    vector<double> se;
    vector<double> skew;
    vector<double> lb;
    vector<double> ub;
    vector<double> n;
    
    // Method selection tracking
    map<string, int> methodCounts;
    int totalCount = 0;
    
    // BCa-specific diagnostics
    int bcaChosen = 0;
    int bcaRejectedHighZ0 = 0;
    int bcaRejectedHighAccel = 0;
    int bcaRejectedStability = 0;
    int bcaLostOnScore = 0;
    
    // NEW: Sample size correlation
    // Bins: 0=[0-30), 1=[30-50), 2=[50-100), 3=[100+)
    map<string, map<int, int>> methodByNBin;  // method -> {bin -> count}
    vector<int> nBinCounts = {0, 0, 0, 0};    // Total per bin
    
    // NEW: Skewness correlation
    // Bins: 0=[-inf, -2), 1=[-2, -1), 2=[-1, 1), 3=[1, 2), 4=[2, inf)
    map<string, map<int, int>> methodBySkewBin;
    vector<int> skewBinCounts = {0, 0, 0, 0, 0};
    
    // NEW: High skewness tracking (|skew| > 2.0)
    map<string, int> methodCountsHighSkew;
    int totalHighSkew = 0;
};

// Helper: Determine sample size bin
static int getSampleSizeBin(double n) {
    if (n < 30) return 0;
    if (n < 50) return 1;
    if (n < 100) return 2;
    return 3;
}

// Helper: Determine skewness bin
static int getSkewnessBin(double skew) {
    if (skew < -2.0) return 0;
    if (skew < -1.0) return 1;
    if (skew < 1.0) return 2;
    if (skew < 2.0) return 3;
    return 4;  // skew >= 2.0
}

// Helper: Get bin label
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

// Helper to parse string to double and add to vector
static inline void pushIfPresent(vector<double>& v, const string& s) {
    if (s.empty()) return;
    try {
        double d = stod(s);
        v.push_back(d);
    } catch (...) {
        // ignore parse errors
    }
}

// Helper to add all relevant fields from a tokenized line to a StatsVecs bucket
static void addToBucket(StatsVecs& s, const vector<string>& toks) {
    // Columns based on CSV format:
    // 0: Strategy, 1: Symbol, 2: Metric, 3: Method
    // 4: Score, 5: BCa_Z0, 6: BCa_Accel, 7: BCa_StabPenalty, 8: BCa_LenPenalty
    // 9: BCa_RawLen, 10: SE, 11: Skew, 12: LB, 13: UB, 14: N
    
    if (toks.size() > 4) pushIfPresent(s.score, toks[4]);
    if (toks.size() > 5) pushIfPresent(s.z0, toks[5]);
    if (toks.size() > 6) pushIfPresent(s.accel, toks[6]);
    if (toks.size() > 7) pushIfPresent(s.stab, toks[7]);
    if (toks.size() > 8) pushIfPresent(s.lenpen, toks[8]);
    if (toks.size() > 9) pushIfPresent(s.rawlen, toks[9]);
    if (toks.size() > 10) pushIfPresent(s.se, toks[10]);
    if (toks.size() > 11) pushIfPresent(s.skew, toks[11]);
    if (toks.size() > 12) pushIfPresent(s.lb, toks[12]);
    if (toks.size() > 13) pushIfPresent(s.ub, toks[13]);
    if (toks.size() > 14) pushIfPresent(s.n, toks[14]);
    
    // Track method selection
    if (toks.size() > 3 && !toks[3].empty()) {
        string method = toks[3];
        s.methodCounts[method]++;
        s.totalCount++;
        
        // Extract n and skew for correlation analysis
        double n_val = 0.0;
        double skew_val = 0.0;
        
        try {
            if (toks.size() > 14 && !toks[14].empty()) {
                n_val = stod(toks[14]);
            }
            if (toks.size() > 11 && !toks[11].empty()) {
                skew_val = stod(toks[11]);
            }
        } catch (...) {
            // Parse error, skip correlation tracking
        }
        
        // NEW: Track method by sample size bin
        if (n_val > 0) {
            int nBin = getSampleSizeBin(n_val);
            s.methodByNBin[method][nBin]++;
            s.nBinCounts[nBin]++;
        }
        
        // NEW: Track method by skewness bin
        int skewBin = getSkewnessBin(skew_val);
        s.methodBySkewBin[method][skewBin]++;
        s.skewBinCounts[skewBin]++;
        
        // NEW: Track high skewness cases (|skew| > 2.0)
        if (abs(skew_val) > 2.0) {
            s.methodCountsHighSkew[method]++;
            s.totalHighSkew++;
        }
        
        // Track BCa diagnostics
        bool bcaAvailable = (toks.size() > 5 && !toks[5].empty());
        
        if (method == "BCa") {
            s.bcaChosen++;
        } else if (bcaAvailable) {
            // BCa ran but didn't win - analyze why
            try {
                double z0 = (toks.size() > 5 && !toks[5].empty()) ? stod(toks[5]) : 0.0;
                double accel = (toks.size() > 6 && !toks[6].empty()) ? stod(toks[6]) : 0.0;
                double stabPen = (toks.size() > 7 && !toks[7].empty()) ? stod(toks[7]) : 0.0;
                
                const double Z0_HARD_LIMIT = 0.6;
                const double ACCEL_HARD_LIMIT = 0.25;
                const double STABILITY_THRESHOLD = 0.1;
                
                bool rejected = false;
                
                if (abs(z0) > Z0_HARD_LIMIT) {
                    s.bcaRejectedHighZ0++;
                    rejected = true;
                }
                
                if (abs(accel) > ACCEL_HARD_LIMIT) {
                    s.bcaRejectedHighAccel++;
                    rejected = true;
                }
                
                if (stabPen > STABILITY_THRESHOLD) {
                    s.bcaRejectedStability++;
                    rejected = true;
                }
                
                if (!rejected) {
                    s.bcaLostOnScore++;
                }
            } catch (...) {
                // Parse error, skip
            }
        }
    }
}

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

static void printSummary(const string& name, const vector<double>& v) {
    if (v.empty()) {
        cout << "  " << setw(20) << left << name << ": N/A\n";
        return;
    }
    double mn = minOf(v);
    double mx = maxOf(v);
    double med = medianOf(v);
    mkc_timeseries::RobustQn<double> rqn;
    double qn = rqn.getRobustQn(v);

    cout << "  " << setw(20) << left << name
         << ": min=" << mn
         << ", max=" << mx
         << ", median=" << med
         << ", qn=" << qn << "\n";
}

static void printMethodFrequency(const StatsVecs& sv) {
    if (sv.totalCount == 0) {
        cout << "\n  Method Selection: N/A (no data)\n";
        return;
    }
    
    cout << "\n  METHOD SELECTION FREQUENCY:\n";
    cout << "  " << string(60, '-') << "\n";
    
    vector<pair<string, int>> sorted;
    for (const auto& kv : sv.methodCounts) {
        sorted.push_back(kv);
    }
    sort(sorted.begin(), sorted.end(), 
         [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (const auto& kv : sorted) {
        double pct = 100.0 * kv.second / sv.totalCount;
        cout << "    " << setw(15) << left << kv.first 
             << ": " << setw(5) << right << kv.second
             << " / " << setw(5) << left << sv.totalCount
             << " (" << fixed << setprecision(1) << setw(5) << right << pct << "%)\n";
    }
    
    cout << "  " << string(60, '-') << "\n";
    cout << "  " << setw(15) << left << "TOTAL" 
         << ": " << sv.totalCount << "\n";
}

static void printBCaDiagnostics(const StatsVecs& sv) {
    int bcaTotal = sv.bcaChosen + sv.bcaRejectedHighZ0 + sv.bcaRejectedHighAccel 
                   + sv.bcaRejectedStability + sv.bcaLostOnScore;
    
    if (bcaTotal == 0) {
        cout << "\n  BCa DIAGNOSTICS: N/A (BCa never ran)\n";
        return;
    }
    
    cout << "\n  BCa DIAGNOSTICS:\n";
    cout << "  " << string(60, '-') << "\n";
    
    auto printPct = [&](const string& label, int count) {
        double pct = 100.0 * count / bcaTotal;
        cout << "    " << setw(30) << left << label 
             << ": " << setw(4) << right << count
             << " (" << fixed << setprecision(1) << setw(5) << right << pct << "%)\n";
    };
    
    printPct("BCa Chosen (Winner)", sv.bcaChosen);
    printPct("BCa Lost on Score", sv.bcaLostOnScore);
    printPct("Rejected: |z0| > 0.6", sv.bcaRejectedHighZ0);
    printPct("Rejected: |a| > 0.25", sv.bcaRejectedHighAccel);
    printPct("Rejected: Stability Penalty", sv.bcaRejectedStability);
    
    cout << "  " << string(60, '-') << "\n";
    cout << "  " << setw(30) << left << "BCa Total Attempts" 
         << ": " << bcaTotal << "\n";
    
    if (bcaTotal > 0) {
        double winRate = 100.0 * sv.bcaChosen / bcaTotal;
        cout << "  " << setw(30) << left << "BCa Win Rate" 
             << ": " << fixed << setprecision(1) << winRate << "%\n";
    }
}

static void printMethodBySampleSize(const StatsVecs& sv) {
    cout << "\n  METHOD SELECTION BY SAMPLE SIZE:\n";
    cout << "  " << string(70, '-') << "\n";
    
    // Iterate through bins
    for (int bin = 0; bin < 4; ++bin) {
        if (sv.nBinCounts[bin] == 0) continue;
        
        cout << "  " << getSampleSizeBinLabel(bin) << " (n=" << sv.nBinCounts[bin] << "):\n";
        
        // Collect methods for this bin and sort by count
        vector<pair<string, int>> methodsInBin;
        for (const auto& [method, binMap] : sv.methodByNBin) {
            auto it = binMap.find(bin);
            if (it != binMap.end()) {
                methodsInBin.push_back({method, it->second});
            }
        }
        sort(methodsInBin.begin(), methodsInBin.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Print each method's frequency in this bin
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
    cout << "\n  METHOD SELECTION BY SKEWNESS:\n";
    cout << "  " << string(70, '-') << "\n";
    
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

static void printHighSkewnessAnalysis(const StatsVecs& sv) {
    if (sv.totalHighSkew == 0) {
        cout << "\n  HIGH SKEWNESS (|skew| > 2.0): None detected\n";
        return;
    }
    
    cout << "\n  HIGH SKEWNESS (|skew| > 2.0) METHOD SELECTION:\n";
    cout << "  " << string(60, '-') << "\n";
    cout << "  Total high-skew cases: " << sv.totalHighSkew 
         << " (" << fixed << setprecision(1) 
         << (100.0 * sv.totalHighSkew / sv.totalCount) << "% of all cases)\n\n";
    
    vector<pair<string, int>> sorted;
    for (const auto& [method, count] : sv.methodCountsHighSkew) {
        sorted.push_back({method, count});
    }
    sort(sorted.begin(), sorted.end(),
         [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (const auto& [method, count] : sorted) {
        double pct = 100.0 * count / sv.totalHighSkew;
        cout << "    " << setw(15) << left << method
             << ": " << setw(4) << right << count
             << " (" << fixed << setprecision(1) << setw(5) << right << pct << "%)\n";
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "Usage: bcastats <csv_file>\n";
        return 1;
    }

    string path = argv[1];
    ifstream ifs(path);
    if (!ifs.is_open()) {
        cerr << "Failed to open file: " << path << "\n";
        return 2;
    }

    string line;
    if (!getline(ifs, line)) {
        cerr << "Empty file" << endl;
        return 3;
    }

    map<string, StatsVecs> buckets;

    while (getline(ifs, line)) {
        if (line.empty()) continue;
        vector<string> toks;
        string tok;
        stringstream ss(line);
        while (getline(ss, tok, ',')) toks.push_back(tok);

        if (toks.size() < 15) continue;

        string metric = toks[2];
        addToBucket(buckets[metric], toks);

        if (metric == "GeoMean" || metric == "ProfitFactor") {
            addToBucket(buckets["Combined"], toks);
        }
    }

    cout << "BCa Statistics Summary for file: " << path << "\n";
    cout << string(80, '=') << "\n\n";

    vector<string> reportOrder = {"GeoMean", "ProfitFactor", "Combined"};

    for (const auto& metric : reportOrder) {
        if (buckets.find(metric) == buckets.end()) continue;

        const auto& sv = buckets.at(metric);
        
        cout << "METRIC: " << metric << "\n";
        cout << string(80, '-') << "\n";
        
        // Original statistics
        printSummary("Score", sv.score);
        printSummary("BCa_Z0", sv.z0);
        printSummary("BCa_Accel", sv.accel);
        printSummary("BCa_StabPenalty", sv.stab);
        printSummary("BCa_LenPenalty", sv.lenpen);
        printSummary("BCa_RawLen", sv.rawlen);
        printSummary("SE", sv.se);
        printSummary("Skew", sv.skew);
        printSummary("LB", sv.lb);
        printSummary("UB", sv.ub);
        printSummary("N", sv.n);
        
        // Method selection frequency
        printMethodFrequency(sv);
        
        // BCa diagnostics
        printBCaDiagnostics(sv);
        
        // NEW: Sample size correlation
        printMethodBySampleSize(sv);
        
        // NEW: Skewness correlation
        printMethodBySkewness(sv);
        
        // NEW: High skewness analysis
        printHighSkewnessAnalysis(sv);
        
        cout << "\n" << string(80, '=') << "\n\n";
    }

    return 0;
}
