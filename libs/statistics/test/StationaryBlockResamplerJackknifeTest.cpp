#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

// Pull in StationaryBlockResampler
#include "number.h"
#include "BiasCorrectedBootstrap.h"

// ----- helpers ---------------------------------------------------------------

// Manual contiguous delete-block jackknife with wrap (delete L_eff; keep n-L_eff)
template <class T, class StatFn>
static std::vector<T>
manual_delete_block_jk_stat(const std::vector<T>& x, std::size_t L_eff, StatFn stat) {
  const std::size_t n = x.size();
  const std::size_t keep = n - L_eff;
  std::vector<T> out(n);
  std::vector<T> y(keep);

  for (std::size_t start = 0; start < n; ++start) {
    const std::size_t start_keep = (start + L_eff) % n;

    const std::size_t tail = std::min(keep, n - start_keep);
    std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
                static_cast<std::ptrdiff_t>(tail),
                y.begin());

    const std::size_t head = keep - tail;
    if (head) {
      std::copy_n(x.begin(), static_cast<std::ptrdiff_t>(head),
                  y.begin() + static_cast<std::ptrdiff_t>(tail));
    }
    out[start] = stat(y);
  }
  return out;
}

// Unbiased sample mean (templated)
template <class T>
static T mean_of(const std::vector<T>& v) {
  T s = T(0);
  for (const auto& a : v) s += a;
  return s / static_cast<T>(v.size());
}

// Fisher–Pearson adjusted skewness (returns double; used for double tests)
static double skewness_unbiased_double(const std::vector<double>& y) {
  const size_t m = y.size();
  if (m < 3) return 0.0; // define as 0 for degenerate cases
  double mu = 0.0;
  for (double v : y) mu += v;
  mu /= static_cast<double>(m);
  double m2 = 0.0, m3 = 0.0;
  for (double v : y) {
    const double d = v - mu;
    const double d2 = d * d;
    m2 += d2;
    m3 += d2 * d;
  }
  m2 /= static_cast<double>(m);
  m3 /= static_cast<double>(m);
  if (m2 == 0.0) return 0.0;
  const double g = m3 / std::pow(m2, 1.5);
  const double adj = std::sqrt(static_cast<double>(m) * (m - 1)) / (m - 2);
  return adj * g;
}

// Third central moment for Decimal tests (returns the same type as input converted from double)
// Avoids sqrt/pow on decimal by computing in double then casting back.
template <class D>
static D third_central_moment_decimal(const std::vector<D>& y) {
  const size_t m = y.size();
  if (m == 0) return D(0);
  double mu = 0.0;
  for (const auto& v : y) mu += v.getAsDouble();
  mu /= static_cast<double>(m);
  double m3 = 0.0;
  for (const auto& v : y) {
    const double d = v.getAsDouble() - mu;
    m3 += d * d * d;
  }
  m3 /= static_cast<double>(m);
  return D(m3);
}

// ----- tests -----------------------------------------------------------------

TEST_CASE("StationaryBlockResampler jackknife — nonlinear stat (skewness, double)", "[Resampler][Jackknife][Stationary][Nonlinear]") {
  using Policy = mkc_timeseries::StationaryBlockResampler<double>;

  const std::size_t n = 31, L = 6;   // L_eff = 6, keep = 25
  std::vector<double> x(n);
  // Create a mildly skewed sequence
  for (size_t i = 0; i < n; ++i) x[i] = std::sin(0.2 * i) + 0.03 * i; // trend adds skew

  Policy pol(L);

  // sampler jackknife with skewness
  auto jk = pol.jackknife(x, [](const std::vector<double>& y){
    return skewness_unbiased_double(y);
  });

  // manual jackknife reference
  auto ref = manual_delete_block_jk_stat<double>(x, L, [](const std::vector<double>& y){
    return skewness_unbiased_double(y);
  });

  REQUIRE(jk.size() == n);
  for (size_t i = 0; i < n; ++i) {
    REQUIRE(jk[i] == Catch::Approx(ref[i]).margin(1e-12));
  }

  // Ensure not all replicates equal (nonlinear variability present)
  bool all_equal = true;
  for (size_t i = 1; i < jk.size(); ++i) {
    if (jk[i] != jk[0]) { all_equal = false; break; }
  }
  REQUIRE_FALSE(all_equal);
}

TEST_CASE("StationaryBlockResampler jackknife — edge L_eff = n-1 (skewness defined)", "[Resampler][Jackknife][Stationary][Nonlinear][Edge]") {
  using Policy = mkc_timeseries::StationaryBlockResampler<double>;

  // Force L > n-1 -> L_eff = n-1, keep=1; skewness on 1 element → define as 0.0
  const std::size_t n = 9, L = 1000;
  std::vector<double> x(n);
  for (size_t i = 0; i < n; ++i) x[i] = i * 0.5 + ((i % 3) ? 0.2 : -0.1);

  Policy pol(L);

  auto jk = pol.jackknife(x, [](const std::vector<double>& y){
    return skewness_unbiased_double(y); // returns 0.0 for m<3
  });

  REQUIRE(jk.size() == n);
  for (size_t i = 0; i < n; ++i) {
    REQUIRE(jk[i] == Catch::Approx(0.0).margin(1e-12));
  }
}

TEST_CASE("StationaryBlockResampler jackknife — nonlinear stat (third central moment, decimal)", "[Resampler][Jackknife][Stationary][Nonlinear][Decimal]") {
  using D = dec::decimal<8>;
  using Policy = mkc_timeseries::StationaryBlockResampler<D>;

  const std::size_t n = 25, L = 7;  // L_eff = 7, keep = 18
  std::vector<D> x(n);
  for (size_t i = 0; i < n; ++i) {
    const double val = std::cos(0.25 * i) + 0.02 * (i*i); // asymmetric → nonzero m3
    x[i] = D(val);
  }

  Policy pol(L);

  auto jk = pol.jackknife(x, [](const std::vector<D>& y){
    return third_central_moment_decimal<D>(y);
  });

  auto ref = manual_delete_block_jk_stat<D>(x, L, [](const std::vector<D>& y){
    return third_central_moment_decimal<D>(y);
  });

  REQUIRE(jk.size() == n);
  for (size_t i = 0; i < n; ++i) {
    REQUIRE(jk[i].getAsDouble() == Catch::Approx(ref[i].getAsDouble()).margin(1e-10));
  }
}
