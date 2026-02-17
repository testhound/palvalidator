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

// Non-overlapping delete-block jackknife reference implementation.
// Mirrors the corrected StationaryBlockResampler::jackknife exactly:
//   - L_eff = min(L, n - minKeep)  where minKeep = 2
//   - numBlocks = floor(n / L_eff)
//   - steps by L_eff each iteration (non-overlapping)
//
// Previously this helper looped n times (sliding window). It now matches
// the production implementation so it can serve as a genuine reference.
template <class T, class StatFn>
static std::vector<T>
manual_delete_block_jk_stat(const std::vector<T>& x,
                             std::size_t           L,
                             StatFn                stat)
{
    const std::size_t n       = x.size();
    const std::size_t minKeep = 2;
    const std::size_t L_eff   = std::min(L, n - minKeep);
    const std::size_t keep    = n - L_eff;
    const std::size_t numBlocks = n / L_eff;

    std::vector<T> out(numBlocks);
    std::vector<T> y(keep);

    for (std::size_t b = 0; b < numBlocks; ++b)
    {
        const std::size_t start      = b * L_eff;          // non-overlapping
        const std::size_t start_keep = (start + L_eff) % n;

        const std::size_t tail = std::min(keep, n - start_keep);
        std::copy_n(x.begin() + static_cast<std::ptrdiff_t>(start_keep),
                    static_cast<std::ptrdiff_t>(tail),
                    y.begin());

        const std::size_t head = keep - tail;
        if (head)
        {
            std::copy_n(x.begin(),
                        static_cast<std::ptrdiff_t>(head),
                        y.begin() + static_cast<std::ptrdiff_t>(tail));
        }
        out[b] = stat(y);
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

TEST_CASE("StationaryBlockResampler jackknife - nonlinear stat (skewness, double)",
          "[Resampler][Jackknife][Stationary][Nonlinear]")
{
    using Policy = mkc_timeseries::StationaryBlockResampler<double>;

    // n=31, L=6 → L_eff=min(6,29)=6, keep=25, numBlocks=floor(31/6)=5
    const std::size_t n = 31, L = 6;
    std::vector<double> x(n);
    for (size_t i = 0; i < n; ++i)
        x[i] = std::sin(0.2 * i) + 0.03 * i;

    auto skew_fn = [](const std::vector<double>& y) {
        return skewness_unbiased_double(y);
    };

    Policy pol(L);
    auto jk  = pol.jackknife(x, skew_fn);
    auto ref = manual_delete_block_jk_stat<double>(x, L, skew_fn);

    // Non-overlapping: floor(n / L_eff) = floor(31/6) = 5 pseudo-values
    const std::size_t minKeep    = 2;
    const std::size_t L_eff      = std::min(L, n - minKeep);  // 6
    const std::size_t numBlocks  = n / L_eff;                   // 5
    REQUIRE(jk.size()  == numBlocks);
    REQUIRE(ref.size() == numBlocks);

    for (size_t b = 0; b < numBlocks; ++b)
        REQUIRE(jk[b] == Catch::Approx(ref[b]).margin(1e-12));

    // Pseudo-values must not all be identical (nonlinear variability present)
    bool all_equal = true;
    for (size_t b = 1; b < jk.size(); ++b)
        if (jk[b] != jk[0]) { all_equal = false; break; }
    REQUIRE_FALSE(all_equal);
}

TEST_CASE("StationaryBlockResampler jackknife - L larger than n-minKeep clamps correctly",
          "[Resampler][Jackknife][Stationary][Nonlinear][Edge]")
{
    using Policy = mkc_timeseries::StationaryBlockResampler<double>;

    // n=9, L=1000 → L_eff = min(1000, n-minKeep) = min(1000, 7) = 7
    // keep=2, numBlocks=floor(9/7)=1
    //
    // Old behaviour clamped to n-1=8, giving keep=1 — degenerate.
    // New behaviour clamps to n-minKeep=7, guaranteeing keep >= 2.
    //
    // skewness_unbiased_double returns 0.0 for m<3, so with keep=2
    // the single pseudo-value is still 0.0. The intent of this test
    // is now to verify the minKeep clamp, not the n-1 clamp.
    const std::size_t n = 9, L = 1000;
    std::vector<double> x(n);
    for (size_t i = 0; i < n; ++i)
        x[i] = i * 0.5 + ((i % 3) ? 0.2 : -0.1);

    Policy pol(L);
    auto jk = pol.jackknife(x, [](const std::vector<double>& y) {
        return skewness_unbiased_double(y);
    });

    // numBlocks = floor(9/7) = 1
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min(L, n - minKeep);  // 7
    const std::size_t numBlocks = n / L_eff;                   // 1
    REQUIRE(jk.size() == numBlocks);

    // keep=2 < 3, so skewness returns 0.0 for the single replicate
    REQUIRE(jk[0] == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("StationaryBlockResampler jackknife - nonlinear stat (third central moment, decimal)",
          "[Resampler][Jackknife][Stationary][Nonlinear][Decimal]")
{
    using D      = dec::decimal<8>;
    using Policy = mkc_timeseries::StationaryBlockResampler<D>;

    // n=25, L=7 → L_eff=min(7,23)=7, keep=18, numBlocks=floor(25/7)=3
    const std::size_t n = 25, L = 7;
    std::vector<D> x(n);
    for (size_t i = 0; i < n; ++i)
    {
        const double val = std::cos(0.25 * i) + 0.02 * (i * i);
        x[i] = D(val);
    }

    auto m3_fn = [](const std::vector<D>& y) {
        return third_central_moment_decimal<D>(y);
    };

    Policy pol(L);
    auto jk  = pol.jackknife(x, m3_fn);
    auto ref = manual_delete_block_jk_stat<D>(x, L, m3_fn);

    // numBlocks = floor(25/7) = 3
    const std::size_t minKeep   = 2;
    const std::size_t L_eff     = std::min(L, n - minKeep);  // 7
    const std::size_t numBlocks = n / L_eff;                   // 3
    REQUIRE(jk.size()  == numBlocks);
    REQUIRE(ref.size() == numBlocks);

    for (size_t b = 0; b < numBlocks; ++b)
        REQUIRE(jk[b].getAsDouble() ==
                Catch::Approx(ref[b].getAsDouble()).margin(1e-10));
}
