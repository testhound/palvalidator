#include "QuantizationAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>
#include <limits>

QuantizationAnalyzer::QuantizationAnalyzer() = default;

double QuantizationAnalyzer::estimateTickFromRange(std::vector<double>::const_iterator begin,
                                                   std::vector<double>::const_iterator end,
                                                   int maxDecimals,
                                                   double integralThreshold) {
    std::vector<double> prices;
    for (auto it = begin; it != end; ++it) {
        if (std::isfinite(*it))
            prices.push_back(*it);
    }

    if (prices.size() < 2)
        return 1e-2; // Fallback for insufficient data

    // Helper to check if a value is very close to an integer
    auto looks_integral = [](double y) {
        double tol = std::max(1e-8, std::fabs(y) * 1e-12);
        return std::fabs(y - std::llround(y)) < tol;
    };

    // 1) Find smallest 10^k scale where most points look integral
    int bestK = 2; // Pragmatic fallback (pennies)
    for (int k = 0; k <= maxDecimals; ++k) {
        const double scale = std::pow(10.0, k);
        int ok_count = 0;
        for (double x : prices) {
            if (looks_integral(x * scale))
                ++ok_count;
        }
        if (static_cast<double>(ok_count) >= integralThreshold * static_cast<double>(prices.size())) {
            bestK = k;
            break;
        }
    }

    const double scale = std::pow(10.0, bestK);
    const double fallbackTick = std::pow(10.0, -bestK);

    // 2) Quantize to integers and get unique sorted levels
    std::vector<long long> levels;
    levels.reserve(prices.size());
    for (double x : prices)
        levels.push_back(std::llround(x * scale));

    if (levels.size() < 2) return fallbackTick;

    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());

    if (levels.size() < 2) return fallbackTick;

    // 3) Compute GCD of positive adjacent differences
    long long g = 0;
    for (size_t i = 1; i < levels.size(); ++i) {
        const long long diff = levels[i] - levels[i - 1];
        if (diff > 0)
            g = (g == 0) ? diff : std::gcd(g, diff);
    }
    if (g <= 0) g = 1; // Should not happen with >1 unique levels, but as a safeguard

    // 4) Convert GCD back to price units
    return static_cast<double>(g) / scale;
}

double QuantizationAnalyzer::calculateRelativeTick(std::vector<double>& window_prices, double tick) {
    if (window_prices.empty() || tick <= 0.0)
        return std::numeric_limits<double>::infinity();

    // In-place median calculation
    size_t m = window_prices.size() / 2;
    std::nth_element(window_prices.begin(), window_prices.begin() + m, window_prices.end());
    double med = window_prices[m];

    // For even-sized vectors, average the two middle elements
    if ((window_prices.size() % 2) == 0) {
        double prev_max = *std::max_element(window_prices.begin(), window_prices.begin() + m);
        med = 0.5 * (med + prev_max);
    }

    return (med > 0.0 && std::isfinite(med)) ? (tick / med) : std::numeric_limits<double>::infinity();
}

size_t QuantizationAnalyzer::countUniquePriceLevels(const std::vector<double>& window_prices, double tick) {
    if (tick <= 0.0) return 0;

    const double invTick = 1.0 / tick;
    std::unordered_set<long long> unique_levels;
    unique_levels.reserve(window_prices.size());

    for (double price : window_prices) {
        if (std::isfinite(price)) {
            unique_levels.insert(std::llround(price * invTick));
        }
    }
    return unique_levels.size();
}

double QuantizationAnalyzer::calculateZeroReturnFraction(const std::vector<double>& window_prices, double tick) {
    if (window_prices.size() < 2) return 1.0;

    int zero_moves = 0;
    int total_moves = 0;

    for (size_t i = 1; i < window_prices.size(); ++i) {
        const double p_prev = window_prices[i-1];
        const double p_curr = window_prices[i];

        if (std::isfinite(p_prev) && std::isfinite(p_curr)) {
            if (std::fabs(p_curr - p_prev) <= tick) {
                ++zero_moves;
            }
            ++total_moves;
        }
    }
    return (total_moves > 0) ? (static_cast<double>(zero_moves) / total_moves) : 1.0;
}

int QuantizationAnalyzer::computeIntradayBarsPerDay(int minutesPerBar) {
    if (minutesPerBar <= 0)
        return 390; // assume 1-min if unknown

    return std::max(1, static_cast<int>(std::round(390.0 / minutesPerBar)));
}

WindowParameters QuantizationAnalyzer::determineWindowParameters(mkc_timeseries::TimeFrame::Duration timeFrame, 
                                                                size_t seriesTotalBars, 
                                                                int intradayMinutes) {
    switch (timeFrame) {
        case mkc_timeseries::TimeFrame::DAILY:
            return WindowParameters(252, 20);

        case mkc_timeseries::TimeFrame::WEEKLY:
            return WindowParameters(260, 4);
            
        case mkc_timeseries::TimeFrame::MONTHLY:
            return WindowParameters(60, 3);

        case mkc_timeseries::TimeFrame::INTRADAY:
        default: {
            const int barsPerDay = computeIntradayBarsPerDay(intradayMinutes);
            int desiredDays = 20;
            // Gracefully shrink window for shorter intraday series
            while (barsPerDay * desiredDays >= static_cast<int>(seriesTotalBars) && desiredDays > 2) {
                desiredDays /= 2;
            }
            
            int window = std::max(3, barsPerDay * desiredDays);
            int buffer = std::max(60, barsPerDay * 10);
            return WindowParameters(window, buffer);
        }
    }
}