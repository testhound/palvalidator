// RegimeMixStationaryResamplerTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <limits>
#include "randutils.hpp"
#include "RegimeMixStationaryResampler.h"

using palvalidator::resampling::RegimeMixStationaryResampler;

// Simple mean over a vector<double>
static double mean_of(const std::vector<double>& v) {
  double s = 0.0;
  for (double x : v) s += x;
  return s / static_cast<double>(v.size());
}

// Build a sampler with alternating labels and equal weights.
// labels.size() must match x.size() in your usage.
static RegimeMixStationaryResampler<double>
make_sampler(std::size_t L, std::size_t n, std::size_t minBarsPerRegime = 1) {
  std::vector<int> labels(n);
  for (std::size_t i = 0; i < n; ++i) labels[i] = static_cast<int>(i % 2);
  std::vector<double> w = {0.5, 0.5};
  return RegimeMixStationaryResampler<double>(L, labels, w, minBarsPerRegime);
}

// Manual contiguous delete-block jackknife with wrap (delete L_eff; keep n-L_eff)
static std::vector<double>
manual_delete_block_jk(const std::vector<double>& x, std::size_t L_eff) {
  const std::size_t n = x.size();
  const std::size_t keep = n - L_eff;
  std::vector<double> out(n);
  std::vector<double> y(keep);

  for (std::size_t start = 0; start < n; ++start) {
    const std::size_t start_keep = (start + L_eff) % n;

    const std::size_t tail = std::min(keep, n - start_keep);
    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
                static_cast<std::ptrdiff_t>(tail),
                y.begin());
    const std::size_t head = keep - tail;
    if (head != 0) {
      std::copy_n(x.begin(),
                  static_cast<std::ptrdiff_t>(head),
                  y.begin() + static_cast<std::ptrdiff_t>(tail));
    }

    out[start] = mean_of(y);
  }
  return out;
}

// Helpers
template <class T>
static std::vector<std::size_t> run_lengths(const std::vector<T>& v)
{
    std::vector<std::size_t> runs;
    if (v.empty()) return runs;
    std::size_t len = 1;
    for (std::size_t i = 1; i < v.size(); ++i)
    {
        if (v[i] == v[i-1]) { ++len; }
        else { runs.push_back(len); len = 1; }
    }
    runs.push_back(len);
    return runs;
}

template <class T>
static double mean_d(const std::vector<T>& x)
{
    if (x.empty()) return std::numeric_limits<double>::quiet_NaN();
    double s = 0.0;
    for (auto& v : x) s += static_cast<double>(v);
    return s / static_cast<double>(x.size());
}

// --- helper: unbiased sample variance ---
static double sample_var_unbiased(const std::vector<double>& y) {
  const size_t m = y.size();
  if (m < 2) return 0.0;
  double mu = 0.0;
  for (double v : y) mu += v;
  mu /= static_cast<double>(m);
  double s2 = 0.0;
  for (double v : y) {
    const double d = v - mu;
    s2 += d * d;
  }
  return s2 / static_cast<double>(m - 1);
}

// Manual jackknife with arbitrary statistic (contiguous delete-block with wrap)
template <class StatFn>
static std::vector<double>
manual_delete_block_jk_stat(const std::vector<double>& x, std::size_t L_eff, StatFn stat) {
  const size_t n = x.size();
  const size_t keep = n - L_eff;
  std::vector<double> out(n), y(keep);
  for (size_t start = 0; start < n; ++start) {
    const size_t start_keep = (start + L_eff) % n;
    const size_t tail = std::min(keep, n - start_keep);
    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
                static_cast<std::ptrdiff_t>(tail), y.begin());
    const size_t head = keep - tail;
    if (head) {
      std::copy_n(x.begin(), static_cast<std::ptrdiff_t>(head),
                  y.begin() + static_cast<std::ptrdiff_t>(tail));
    }
    out[start] = stat(y);
  }
  return out;
}

TEST_CASE("RegimeMixStationaryResampler: basic length & determinism", "[RegimeMix][Stationary]")
{
    // Source labels: long homogeneous runs to minimize truncation effects.
    // Build 2000 points: 1000 of regime 0, then 1000 of regime 1.
    const std::size_t xn = 2000;
    std::vector<int> labels(xn, 0);
    for (std::size_t i = 1000; i < xn; ++i) labels[i] = 1;

    // Returns x = labels as doubles, so the output encodes regimes directly.
    // This lets us inspect regime runs in the output.
    std::vector<double> x(xn);
    std::transform(labels.begin(), labels.end(), x.begin(),
                   [](int s){ return static_cast<double>(s); });

    // Target weights: 60% regime 0, 40% regime 1
    std::vector<double> w = {0.6, 0.4};

    const std::size_t L = 6;      // mean stationary block length
    const std::size_t n = 1000;   // desired resample length

    RegimeMixStationaryResampler<double> sampler(L, labels, w, /*minBarsPerRegime=*/8);

    // Determinism with fixed seed
    randutils::mt19937_rng rng1; rng1.seed(12345);
    auto y1 = sampler(x, n, rng1);

    randutils::mt19937_rng rng2; rng2.seed(12345);
    auto y2 = sampler(x, n, rng2);

    REQUIRE(y1.size() == n);
    REQUIRE(y2.size() == n);
    REQUIRE(y1 == y2); // same seed → same sample
}

TEST_CASE("RegimeMixStationaryResampler: regime homogeneity preserved", "[RegimeMix][Stationary]")
{
    // Three regimes with long runs: 0 (800), 1 (600), 2 (600) = 2000 total
    std::vector<int> labels;
    labels.insert(labels.end(), 800, 0);
    labels.insert(labels.end(), 600, 1);
    labels.insert(labels.end(), 600, 2);

    // Returns = labels as doubles
    std::vector<double> x(labels.size());
    std::transform(labels.begin(), labels.end(), x.begin(),
                   [](int s){ return static_cast<double>(s); });

    // Equal weights across 3 regimes
    std::vector<double> w = {1.0, 1.0, 1.0};

    const std::size_t L = 8;
    const std::size_t n = 1200;

    RegimeMixStationaryResampler<double> sampler(L, labels, w, /*minBarsPerRegime=*/4);
    randutils::mt19937_rng rng; rng.seed(999);

    auto y = sampler(x, n, rng);

    // Because y encodes labels, homogeneity means: it should be piecewise-constant,
    // and any change in value indicates a regime boundary (which is allowed),
    // but within each run it's constant (by construction).
    auto runs = run_lengths(y);

    // Sanity: every run has constant label (tautological given construction),
    // but we check that run lengths are positive and sum to n.
    REQUIRE(!runs.empty());
    std::size_t sum = std::accumulate(runs.begin(), runs.end(), std::size_t(0));
    REQUIRE(sum == n);

    // Additionally: no run should be zero; trivial but explicit
    REQUIRE(*std::min_element(runs.begin(), runs.end()) >= 1);
}

TEST_CASE("RegimeMixStationaryResampler: target weights approximately satisfied", "[RegimeMix][Stationary]")
{
    // Two regimes with very long runs to avoid truncation bias.
    const std::size_t xn = 4000;
    std::vector<int> labels(xn, 0);
    for (std::size_t i = 2000; i < xn; ++i) labels[i] = 1;

    // Returns = labels as doubles
    std::vector<double> x(labels.size());
    std::transform(labels.begin(), labels.end(), x.begin(),
                   [](int s){ return static_cast<double>(s); });

    // Asymmetric weights (70/30) to test quota tracking with variable lengths
    std::vector<double> w = {0.7, 0.3};

    const std::size_t L = 5;
    const std::size_t n = 3000;

    RegimeMixStationaryResampler<double> sampler(L, labels, w, /*minBarsPerRegime=*/8);
    randutils::mt19937_rng rng; rng.seed(2024);

    auto y = sampler(x, n, rng);

    // Measure realized weights in output:
    std::size_t c0 = 0, c1 = 0;
    for (double v : y) {
        if (v < 0.5) ++c0; else ++c1;
    }
    const double f0 = static_cast<double>(c0) / static_cast<double>(n);
    const double f1 = static_cast<double>(c1) / static_cast<double>(n);

    // Because lengths are stationary (random), enforce a fairly generous tolerance
    // that tightens as n grows. Here n=3000 — 2.5% tolerance is fine.
    REQUIRE(std::abs(f0 - 0.70) <= 0.025);
    REQUIRE(std::abs(f1 - 0.30) <= 0.025);
}

TEST_CASE("RegimeMixStationaryResampler: mean run length roughly equals L", "[RegimeMix][Stationary]")
{
    // Two regimes with very long homogeneous runs to avoid truncation bias.
    const std::size_t xn = 10000;
    std::vector<int> labels(xn, 0);
    for (std::size_t i = xn/2; i < xn; ++i) labels[i] = 1;

    // Returns = labels as doubles so we can read regime runs from output.
    std::vector<double> x(labels.size());
    std::transform(labels.begin(), labels.end(), x.begin(),
                   [](int s){ return static_cast<double>(s); });

    // Balanced weights to let the sampler alternate regimes by blocks.
    std::vector<double> w = {0.5, 0.5};

    const std::size_t L = 7;      // target mean stationary block length
    const std::size_t n = 4000;   // output length

    RegimeMixStationaryResampler<double> sampler(L, labels, w, /*minBarsPerRegime=*/1);
    randutils::mt19937_rng rng; rng.seed(77);

    auto y = sampler(x, n, rng);
    auto runs = run_lengths(y);

    // With two regimes and long source runs, output run lengths should be
    // driven by the geometric draw (mean ~= L). Allow modest tolerance.
    const double meanRun = mean_d(runs);
    REQUIRE(meanRun == Catch::Approx(static_cast<double>(L)).margin(1.5));
}

TEST_CASE("RegimeMixStationaryResampler: wrap-around correctness (no crash, full length)", "[RegimeMix][Stationary]")
{
    // Labels alternate in medium blocks so wrap is exercised frequently.
    const std::size_t xn = 257; // prime-ish forces wrap diversity
    std::vector<int> labels;
    for (std::size_t i = 0; i < xn; ++i) labels.push_back((i / 7) % 2); // blocks of 7

    std::vector<double> x(labels.size());
    std::transform(labels.begin(), labels.end(), x.begin(),
                   [](int s){ return static_cast<double>(s); });

    std::vector<double> w = {0.5, 0.5};

    const std::size_t L = 9;
    const std::size_t n = 1500;

    RegimeMixStationaryResampler<double> sampler(L, labels, w, /*minBarsPerRegime=*/4);
    randutils::mt19937_rng rng; rng.seed(314159);

    auto y = sampler(x, n, rng);

    REQUIRE(y.size() == n);

    // Very weak check: values must be 0 or 1 (since x == labels)
    auto ok = std::all_of(y.begin(), y.end(), [](double v){ return v == 0.0 || v == 1.0; });
    REQUIRE(ok);
}

TEST_CASE("RegimeMixStationaryResampler: scarcity fallback does not fail", "[RegimeMix][Stationary]")
{
    // Regime 0 abundant; regime 1 very scarce.
    const std::size_t xn = 1000;
    std::vector<int> labels(xn, 0);
    for (std::size_t i = 0; i < 5; ++i) labels[i] = 1; // only 5 starts for regime 1

    std::vector<double> x(labels.size());
    std::transform(labels.begin(), labels.end(), x.begin(),
                   [](int s){ return static_cast<double>(s); });

    // Still ask for 30% of regime 1 to force scarcity behavior
    std::vector<double> w = {0.7, 0.3};

    const std::size_t L = 6;
    const std::size_t n = 800;

    RegimeMixStationaryResampler<double> sampler(L, labels, w, /*minBarsPerRegime=*/8);
    randutils::mt19937_rng rng; rng.seed(4242);

    auto y = sampler(x, n, rng);

    // Ensure output has full length and contains both regimes when possible.
    REQUIRE(y.size() == n);

    std::size_t c0 = 0, c1 = 0;
    for (double v : y) { if (v < 0.5) ++c0; else ++c1; }

    // With scarcity, we do not assert tight adherence; just that sampler completes
    // and does not degenerate (both regimes present if any starts exist for 1).
    REQUIRE(c0 + c1 == n);
    REQUIRE(c1 > 0); // at least some regime-1 bars included
}

TEST_CASE("RegimeMixStationaryResampler: small-N safety", "[RegimeMix][Stationary]")
{
    const std::size_t xn = 20;
    std::vector<int> labels(xn, 0);
    for (std::size_t i = 10; i < xn; ++i) labels[i] = 1;

    std::vector<double> x(labels.size());
    std::transform(labels.begin(), labels.end(), x.begin(),
                   [](int s){ return static_cast<double>(s); });

    std::vector<double> w = {0.5, 0.5};

    const std::size_t L = 3;
    const std::size_t n = 18;

    RegimeMixStationaryResampler<double> sampler(L, labels, w, /*minBarsPerRegime=*/2);
    randutils::mt19937_rng rng; rng.seed(7);

    auto y = sampler(x, n, rng);

    REQUIRE(y.size() == n);
}

TEST_CASE("RegimeMixStationaryResampler::jackknife — basic correctness vs manual", "[RegimeMix][Stationary][Jackknife]") {
  // n=20, L=5 => L_eff=5, keep=15; no wrap for early starts
  const std::size_t n = 20, L = 5;
  std::vector<double> x(n);
  std::iota(x.begin(), x.end(), 1.0); // 1..20

  auto sampler = make_sampler(L, n);
  // stat = mean
  auto jk = sampler.jackknife(x, [](const std::vector<double>& y){ return mean_of(y); });

  // manual reference
  const auto ref = manual_delete_block_jk(x, L); // L_eff == L here
  REQUIRE(jk.size() == n);
  for (std::size_t i = 0; i < n; ++i) {
    REQUIRE(jk[i] == Catch::Approx(ref[i]).margin(1e-12));
  }
}

TEST_CASE("RegimeMixStationaryResampler::jackknife — L_eff = min(L, n-1) cap", "[RegimeMix][Stationary][Jackknife]") {
  // Force L > n-1 => L_eff = n-1, keep = 1. Each replicate mean equals the single kept value.
  const std::size_t n = 7;
  std::vector<double> x = {10, 20, 30, 40, 50, 60, 70};

  const std::size_t L = 1000; // > n-1
  auto sampler = make_sampler(L, n);

  auto jk = sampler.jackknife(x, [](const std::vector<double>& y){ return mean_of(y); });

  // With keep=1, for start s we keep x[(s + L_eff) % n] == x[(s + (n-1)) % n] == x[(s-1) % n].
  REQUIRE(jk.size() == n);
  for (std::size_t s = 0; s < n; ++s) {
    const std::size_t kept_idx = (s + (n - 1)) % n;
    REQUIRE(jk[s] == Catch::Approx(x[kept_idx]).margin(1e-12));
  }
}

TEST_CASE("RegimeMixStationaryResampler::jackknife — wrap-around delete block", "[RegimeMix][Stationary][Jackknife]") {
  // Choose n and L such that start near end wraps delete-block over index 0.
  const std::size_t n = 11, L = 4; // L_eff = 4, keep = 7
  std::vector<double> x(n);
  std::iota(x.begin(), x.end(), 0.0); // 0..10

  auto sampler = make_sampler(L, n);

  auto jk = sampler.jackknife(x, [](const std::vector<double>& y){ return mean_of(y); });
  const auto ref = manual_delete_block_jk(x, L);

  REQUIRE(jk.size() == n);
  for (std::size_t i = 0; i < n; ++i) {
    REQUIRE(jk[i] == Catch::Approx(ref[i]).margin(1e-12));
  }
}

TEST_CASE("RegimeMixStationaryResampler::jackknife — shape & determinism", "[RegimeMix][Stationary][Jackknife]") {
  // jackknife has no RNG; result depends only on x and L
  const std::size_t n = 13, L = 5;
  std::vector<double> x(n);
  std::iota(x.begin(), x.end(), -6.0); // -6..6

  auto sampler1 = make_sampler(L, n);
  auto sampler2 = make_sampler(L, n); // identical configuration

  auto jk1 = sampler1.jackknife(x, [](const std::vector<double>& y){ return mean_of(y); });
  auto jk2 = sampler2.jackknife(x, [](const std::vector<double>& y){ return mean_of(y); });

  REQUIRE(jk1.size() == n);
  REQUIRE(jk2.size() == n);
  REQUIRE(jk1 == jk2);
}

TEST_CASE("RegimeMixStationaryResampler::jackknife — nonlinear stat (variance)", "[RegimeMix][Stationary][Jackknife]") {
  const size_t n = 21, L = 6;           // L_eff = 6, keep = 15
  std::vector<double> x(n);
  // mildly heteroskedastic series to avoid trivial equal-variance segments
  for (size_t i = 0; i < n; ++i) x[i] = std::sin(0.3*i) + 0.1*(i%3);

  auto sampler = make_sampler(L, n);
  auto jk = sampler.jackknife(x, [](const std::vector<double>& y){ return sample_var_unbiased(y); });

  const auto ref = manual_delete_block_jk_stat(x, L, [](const std::vector<double>& y){
    return sample_var_unbiased(y);
  });

  REQUIRE(jk.size() == n);
  for (size_t i = 0; i < n; ++i) {
    REQUIRE(jk[i] == Catch::Approx(ref[i]).margin(1e-12));
  }
}
