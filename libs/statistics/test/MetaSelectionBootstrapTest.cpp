#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cmath>
#include <numeric>
#include "MetaSelectionBootstrap.h"
#include "randutils.hpp"

using palvalidator::analysis::MetaSelectionBootstrap;

TEST_CASE("MetaSelectionBootstrap — basic selection-aware CI on equal-weight meta", "[MetaSel][Bootstrap]") {
  using Num = double;
  using Rng = randutils::mt19937_rng;

  // Two toy components with short-run dependence (simulate "trade returns")
  // compA: mild positive edge, low noise
  // compB: similar edge, slightly higher noise
  const std::size_t n = 60;
  std::vector<Num> compA(n), compB(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double epsA = 0.001 * std::sin(0.2 * i);
    const double epsB = 0.0015 * std::cos(0.17 * i);
    compA[i] = 0.001 + epsA;   // ~0.10% per period
    compB[i] = 0.001 + epsB;   // ~0.10% per period, noisier
  }

  typename MetaSelectionBootstrap<Num>::Matrix components{compA, compB};

  // Meta builder: equal-weight by index up to min length
  auto builder = [](const typename MetaSelectionBootstrap<Num>::Matrix& mats) {
    if (mats.empty()) return std::vector<Num>{};
    std::size_t m = std::numeric_limits<std::size_t>::max();
    for (auto& s : mats) m = std::min(m, s.size());
    if (m < 2) return std::vector<Num>{};
    std::vector<Num> out(m, 0.0);
    const double w = 1.0 / static_cast<double>(mats.size());
    for (auto& s : mats) {
      for (std::size_t i = 0; i < m; ++i) out[i] += w * s[i];
    }
    return out;
  };

  // Outer selection-aware bootstrap
  const std::size_t B = 800;          // small for test; production ~2000
  const double cl = 0.95;
  const std::size_t Lmean = 6;        // modest mean block length
  const double periodsPerYear = 252.0;

  MetaSelectionBootstrap<Num> msb(B, cl, Lmean, periodsPerYear);
  Rng rng;
  auto res = msb.run(components, builder, rng);

  // Basic sanity: bounds should be finite and LB <= typical gm
  REQUIRE(std::isfinite(res.lbPerPeriod));
  REQUIRE(std::isfinite(res.lbAnnualized));

  // A very loose expectation: annualized LB near or below ~25% (since ~0.1%*252 ≈ 25%)
  // We don’t assert a fixed value—just that it’s not pathological
  REQUIRE(res.lbAnnualized < 0.40);
  REQUIRE(res.lbAnnualized > -0.10);
}
