// BCaVsPercentileTStationaryMaskTest.cpp

#include <catch2/catch_test_macros.hpp>
#include <random>
#include <vector>
#include <functional>
#include <algorithm>

#include "BiasCorrectedBootstrap.h"
#include "PercentileTBootstrap.h"
#include "TradingBootstrapFactory.h"
#include "StatUtils.h"
#include "StationaryMaskResamplers.h"
#include "number.h"

// Aliases
using Decimal       = num::DefaultNumber;
using MaskResampler = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Decimal>;
using Factory       = TradingBootstrapFactory<>;

// Simple mean as our statistic
static std::function<Decimal(const std::vector<Decimal>&)>
makeMeanStat()
{
  using Stat = mkc_timeseries::StatUtils<Decimal>;
  return std::function<Decimal(const std::vector<Decimal>&)>(&Stat::computeMean);
}

// --- Synthetic data helpers -------------------------------------------------

// "Heavy-tailed" sample: mixture of small normal noise + occasional large shocks.
static std::vector<Decimal> makeHeavyTailedSample(std::size_t n)
{
  std::mt19937_64 rng(42);
  std::normal_distribution<double> small(0.001, 0.01);
  std::cauchy_distribution<double> tail(0.0, 0.05);
  std::uniform_real_distribution<double> U(0.0, 1.0);

  std::vector<Decimal> x;
  x.reserve(n);

  for (std::size_t i = 0; i < n; ++i)
  {
    const double u = U(rng);
    double v;
    if (u < 0.90) {
      v = small(rng);       // most of the time
    } else {
      v = tail(rng);        // occasionally huge shocks
    }
    x.push_back(Decimal(v));
  }

  return x;
}

// Positively skewed mixture: many small losses, occasional big wins.
static std::vector<Decimal> makeSkewedMixtureSample(std::size_t n)
{
  std::mt19937_64 rng(1337);
  std::uniform_real_distribution<double> U(0.0, 1.0);
  std::normal_distribution<double> smallLoss(-0.002, 0.005);
  std::normal_distribution<double> bigWin(0.03, 0.02);

  std::vector<Decimal> x;
  x.reserve(n);

  for (std::size_t i = 0; i < n; ++i)
  {
    const double u = U(rng);
    double v;
    if (u < 0.9)
      v = smallLoss(rng);   // most of the time: small negative
    else
      v = bigWin(rng);      // rare large positive
    x.push_back(Decimal(v));
  }

  return x;
}

// Tiny n, hand-crafted skewed sample: mostly small moves, a few big winners.
static std::vector<Decimal> makeTinySkewedSample()
{
  const double data[] = {
    -0.004, -0.003, -0.002, -0.001, -0.002,
    -0.003, -0.001, -0.002, -0.001, -0.003,
     0.025,  0.030,  0.028,  0.027,  0.032,
    -0.002, -0.001,  0.020,  0.022,  0.026
  };
  constexpr std::size_t n = sizeof(data) / sizeof(data[0]);

  std::vector<Decimal> x;
  x.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    x.push_back(Decimal(data[i]));
  return x;
}

// --- Test -------------------------------------------------------------------

TEST_CASE("BCa vs Percentile-T (heavy-tailed) using StationaryMaskValueResamplerAdapter",
          "[bootstrap][BCa][PercentileT][stationary-mask]")
{
  // 1) Synthetic returns
  const std::vector<Decimal> returns = makeHeavyTailedSample(400);

  // 2) Common config
  const unsigned B_outer = 2000;
  const double   CL      = 0.95;
  const std::size_t blockSize = 10;  // stationary block length

  const uint64_t strategyId = 12345;
  const uint64_t stageTag   = 999;
  const uint64_t fold       = 0;

  Factory factory(/*masterSeed*/ 20250215ULL);
  auto statFn = makeMeanStat();

  // Use the SAME resampler style you use in production:
  MaskResampler sampler(blockSize);

  // 3) Build BCa and Percentile-T via TradingBootstrapFactory
  auto bca = factory.makeBCa<Decimal>(
      returns,
      B_outer,
      CL,
      statFn,
      sampler,
      strategyId,
      stageTag,
      blockSize,
      fold
  );

  auto pt = factory.makeStudentizedT<Decimal>(
      returns,
      B_outer,
      CL,
      statFn,
      sampler,
      strategyId,
      stageTag,
      blockSize,
      fold
  );

  // 4) Pull intervals; BCaBootStrap and BCaCompatibleTBootstrap are lazy,
  //    so calling accessors forces the computation.
  const Decimal bca_mean  = bca.getStatistic();
  const Decimal bca_lower = bca.getLowerBound();
  const Decimal bca_upper = bca.getUpperBound();

  const Decimal pt_mean   = pt.getStatistic();
  const Decimal pt_lower  = pt.getLowerBound();
  const Decimal pt_upper  = pt.getUpperBound();

  // 5) Basic sanity checks

  // Means should be finite and reasonably close.
  REQUIRE(std::isfinite(num::to_double(bca_mean)));
  REQUIRE(std::isfinite(num::to_double(pt_mean)));

  // Both CIs should be non-degenerate.
  REQUIRE(bca_upper > bca_lower);
  REQUIRE(pt_upper  > pt_lower);

  const double bca_len = num::to_double(bca_upper - bca_lower);
  const double pt_len  = num::to_double(pt_upper  - pt_lower);

  REQUIRE(bca_len > 0.0);
  REQUIRE(pt_len  > 0.0);

  // The two means should not be wildly different (they're both bootstrapping
  // the same statistic).
  const double mean_diff = std::fabs(
      num::to_double(bca_mean - pt_mean)
  );
  REQUIRE(mean_diff < 0.01);  // loose but catches obvious pathologies

  // Lengths should be within a sane factor of each other; heavy tails might
  // make BCa wider, but not orders of magnitude off.
  const double length_ratio = bca_len / pt_len;
  REQUIRE(length_ratio > 0.2);
  REQUIRE(length_ratio < 5.0);
}

TEST_CASE("BCa vs Percentile-T (skewed mixture) using StationaryMaskValueResamplerAdapter",
          "[bootstrap][BCa][PercentileT][stationary-mask][skewed]")
{
  // 1) Synthetic returns: strongly right-skewed mixture
  const std::vector<Decimal> returns = makeSkewedMixtureSample(400);
  REQUIRE(returns.size() == 400);

  // 2) Common config
  const unsigned     B_outer   = 2000;
  const double       CL        = 0.95;
  const std::size_t  blockSize = 10;  // stationary block length

  const uint64_t strategyId = 54321;
  const uint64_t stageTag   = 1001;
  const uint64_t fold       = 0;

  Factory factory(/*masterSeed*/ 20250216ULL);
  auto statFn = makeMeanStat();

  // Use the SAME resampler style as production
  MaskResampler sampler(blockSize);

  // 3) Build BCa and Studentized-T via TradingBootstrapFactory
  auto bca = factory.makeBCa<Decimal>(
      returns,
      B_outer,
      CL,
      statFn,
      sampler,
      strategyId,
      stageTag,
      blockSize,
      fold
  );

  auto pt = factory.makeStudentizedT<Decimal>(
      returns,
      B_outer,
      CL,
      statFn,
      sampler,
      strategyId,
      stageTag,
      blockSize,
      fold
  );

  // 4) Pull intervals (this forces computation)
  const Decimal bca_mean  = bca.getStatistic();
  const Decimal bca_lower = bca.getLowerBound();
  const Decimal bca_upper = bca.getUpperBound();

  const Decimal pt_mean   = pt.getStatistic();
  const Decimal pt_lower  = pt.getLowerBound();
  const Decimal pt_upper  = pt.getUpperBound();

  // 5) Basic sanity checks

  // Means should be finite.
  REQUIRE(std::isfinite(num::to_double(bca_mean)));
  REQUIRE(std::isfinite(num::to_double(pt_mean)));

  // Both CIs should be non-degenerate.
  REQUIRE(bca_upper > bca_lower);
  REQUIRE(pt_upper  > pt_lower);

  const double bca_len = num::to_double(bca_upper - bca_lower);
  const double pt_len  = num::to_double(pt_upper  - pt_lower);

  REQUIRE(bca_len > 0.0);
  REQUIRE(pt_len  > 0.0);

  // Verify the sample itself is positively skewed: mean > median.
  std::vector<double> asDouble;
  asDouble.reserve(returns.size());
  double sum = 0.0;
  for (const auto& d : returns)
  {
    const double v = num::to_double(d);
    asDouble.push_back(v);
    sum += v;
  }
  const double mean = sum / asDouble.size();

  auto tmp = asDouble;
  std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
  const double median = tmp[tmp.size() / 2];

  REQUIRE(mean > median);

  // Lengths should be within a sane factor of each other; under skew we often
  // expect the studentized-T interval to be at least as long as BCa, but we
  // don't enforce that strictly to avoid brittleness.
  const double length_ratio = bca_len / pt_len;
  REQUIRE(length_ratio > 0.2);
  REQUIRE(length_ratio < 5.0);
}

TEST_CASE("BCa vs Percentile-T (tiny-n skewed) using StationaryMaskValueResamplerAdapter",
          "[bootstrap][BCa][PercentileT][stationary-mask][tiny-n]")
{
  // 1) Tiny, hand-crafted skewed sample
  const std::vector<Decimal> returns = makeTinySkewedSample();
  REQUIRE(returns.size() == 20);

  // 2) Common config
  const unsigned     B_outer   = 2000;
  const double       CL        = 0.95;
  const std::size_t  blockSize = 5;   // smaller block for tiny-n

  const uint64_t strategyId = 77777;
  const uint64_t stageTag   = 1002;
  const uint64_t fold       = 0;

  Factory factory(/*masterSeed*/ 20250217ULL);
  auto statFn = makeMeanStat();

  MaskResampler sampler(blockSize);

  // 3) Build BCa and Studentized-T via TradingBootstrapFactory
  auto bca = factory.makeBCa<Decimal>(
      returns,
      B_outer,
      CL,
      statFn,
      sampler,
      strategyId,
      stageTag,
      blockSize,
      fold
  );

  auto pt = factory.makeStudentizedT<Decimal>(
      returns,
      B_outer,
      CL,
      statFn,
      sampler,
      strategyId,
      stageTag,
      blockSize,
      fold
  );

  // 4) Pull intervals
  const Decimal bca_mean  = bca.getStatistic();
  const Decimal bca_lower = bca.getLowerBound();
  const Decimal bca_upper = bca.getUpperBound();

  const Decimal pt_mean   = pt.getStatistic();
  const Decimal pt_lower  = pt.getLowerBound();
  const Decimal pt_upper  = pt.getUpperBound();

  // 5) Basic sanity checks

  REQUIRE(std::isfinite(num::to_double(bca_mean)));
  REQUIRE(std::isfinite(num::to_double(pt_mean)));

  REQUIRE(bca_upper > bca_lower);
  REQUIRE(pt_upper  > pt_lower);

  const double bca_len = num::to_double(bca_upper - bca_lower);
  const double pt_len  = num::to_double(pt_upper  - pt_lower);

  REQUIRE(bca_len > 0.0);
  REQUIRE(pt_len  > 0.0);

  // Again, confirm the tiny sample is actually skewed (mean > median).
  std::vector<double> asDouble;
  asDouble.reserve(returns.size());
  double sum = 0.0;
  for (const auto& d : returns)
  {
    const double v = num::to_double(d);
    asDouble.push_back(v);
    sum += v;
  }
  const double mean = sum / asDouble.size();

  auto tmp = asDouble;
  std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
  const double median = tmp[tmp.size() / 2];

  REQUIRE(mean > median);

  // With tiny-n and skew, we mainly want to ensure both bootstraps remain
  // numerically sane and do not diverge wildly in scale.
  const double length_ratio = bca_len / pt_len;
  REQUIRE(length_ratio > 0.1);
  REQUIRE(length_ratio < 10.0);
}
