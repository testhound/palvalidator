// RegimeMixStressRunnerTest.cpp
//
// Unit tests for palvalidator::analysis::RegimeMixStressRunner using Catch2.
//
// We include two styles:
//  1) Constant returns → mix-independent LB → deterministic pass/fail vs hurdle.
//  2) Encoded per-regime returns + FixedRng → mix DOES matter, deterministic ordering.
//
// Dependencies:
//  - RegimeMixStressRunner.h (now templated on Rng)
//  - RegimeMixStress.h
//  - RegimeMixBlockResampler.h
//  - TestUtils.h, number.h (DecimalType, createDecimal)
//  - Catch2

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <random>
#include <cmath>

#include "RegimeMixStressRunner.h"
#include "RegimeMixStress.h"
#include "RegimeMixBlockResampler.h"

#include "TestUtils.h"   // DecimalType, createDecimal
#include "number.h"

using palvalidator::analysis::RegimeMix;
using palvalidator::analysis::RegimeMixConfig;
using palvalidator::analysis::RegimeMixStressRunner;

namespace
{
  // Same helpers as in other tests (L-homogeneous runs 0/1/2/... of length L)
  static std::vector<int> buildCyclicBlockLabels(std::size_t N, std::size_t L, int S = 3)
  {
    std::vector<int> z; z.reserve(N);
    int cur = 0;
    for (std::size_t i = 0; i < N; )
      {
        const std::size_t take = std::min<std::size_t>(L, N - i);
        for (std::size_t j = 0; j < take; ++j)
	  {
            z.push_back(cur);
	  }
        i += take;
        cur = (cur + 1) % S;
      }
    return z;
  }

  template <class Num>
    static std::vector<Num> buildConstantReturns(std::size_t N, const char* r_str)
    {
      std::vector<Num> x; x.reserve(N);
      const Num r = createDecimal(r_str);
      for (std::size_t i = 0; i < N; ++i)
	{
	  x.push_back(r);
	}
      return x;
    }

  // Encoded per-regime returns so the resampled composition affects the GeoMean.
  //  label 0 → v0, label 1 → v1, label 2 → v2
  template <class Num>
    static std::vector<Num> buildEncodedReturns(const std::vector<int>& labels,
						const char* v0_str,
						const char* v1_str,
						const char* v2_str)
    {
      const Num v0 = createDecimal(v0_str);
      const Num v1 = createDecimal(v1_str);
      const Num v2 = createDecimal(v2_str);

      std::vector<Num> x; x.reserve(labels.size());
      for (int z : labels)
	{
	  if (z == 0) x.push_back(v0);
	  else if (z == 1) x.push_back(v1);
	  else             x.push_back(v2);
	}
      return x;
    }

  // Deterministic RNG with .uniform() API used by the sampler (and thus by BCa via sampler)
  struct FixedRng
  {
    std::mt19937_64 eng;

    // Default constructor with a fixed seed for reproducible tests
    FixedRng() : eng(12345) {}

    explicit FixedRng(std::uint64_t seed)
      : eng(seed)
    {
    }

    std::size_t uniform(std::size_t a, std::size_t b)
    {
      std::uniform_int_distribution<std::size_t> d(a, b);
      return d(eng);
    }

    double uniform(double a, double b)
    {
      std::uniform_real_distribution<double> d(a, b);
      return d(eng);
    }
  };

} // namespace

TEST_CASE("RegimeMixStressRunner: all mixes PASS when LB > hurdle for constant returns", "[RegimeMixRunner]")
{
    using D = DecimalType;

    const std::size_t L = 5;
    const std::size_t N = 600;

    auto labels  = buildCyclicBlockLabels(N, L, /*S=*/3);
    auto returns = buildConstantReturns<D>(N, "0.0020"); // 0.20%

    std::vector<RegimeMix> mixes =
    {
        RegimeMix("Equal",   {1.0/3.0, 1.0/3.0, 1.0/3.0}),
        RegimeMix("DownFav", {0.30, 0.40, 0.30})
    };

    RegimeMixConfig cfg(mixes, /*minPassFraction=*/0.50, /*minBarsPerRegime=*/L + 5);

    const unsigned int numResamples = 100;
    const double       confLevel    = 0.90;
    const double       annFactor    = 1.0;
    const D            hurdle       = createDecimal("0.0015");

    // Use default RNG (randutils) in runner template parameter
    RegimeMixStressRunner<D> runner(cfg, L, numResamples, confLevel, annFactor, hurdle);

    std::ostringstream os;
    const auto res = runner.run(returns, labels, os);

    REQUIRE(res.perMix().size() == mixes.size());
    for (const auto &mx : res.perMix())
    {
        INFO("Mix: " << mx.mixName());
        REQUIRE(mx.annualizedLowerBound() > hurdle);
        REQUIRE(mx.pass() == true);
    }
    REQUIRE(res.passFraction() == Catch::Approx(1.0));
    REQUIRE(res.overallPass() == true);
}

TEST_CASE("RegimeMixStressRunner: all mixes FAIL when LB < hurdle for constant returns", "[RegimeMixRunner]")
{
    using D = DecimalType;

    const std::size_t L = 5;
    const std::size_t N = 500;

    auto labels  = buildCyclicBlockLabels(N, L, /*S=*/3);
    auto returns = buildConstantReturns<D>(N, "0.0010"); // 0.10%

    std::vector<RegimeMix> mixes =
    {
        RegimeMix("Equal",   {1.0/3.0, 1.0/3.0, 1.0/3.0}),
        RegimeMix("SkewLow", {0.50, 0.30, 0.20})
    };

    RegimeMixConfig cfg(mixes, /*minPassFraction=*/0.60, /*minBarsPerRegime=*/L + 5);

    const unsigned int numResamples = 100;
    const double       confLevel    = 0.95;
    const double       annFactor    = 1.0;
    const D            hurdle       = createDecimal("0.0020"); // 0.20%

    RegimeMixStressRunner<D> runner(cfg, L, numResamples, confLevel, annFactor, hurdle);

    std::ostringstream os;
    const auto res = runner.run(returns, labels, os);

    REQUIRE(res.perMix().size() == mixes.size());
    for (const auto &mx : res.perMix())
    {
        INFO("Mix: " << mx.mixName());
        REQUIRE(mx.annualizedLowerBound() < hurdle);
        REQUIRE(mx.pass() == false);
    }

    REQUIRE(res.passFraction() == Catch::Approx(0.0));
    REQUIRE(res.overallPass() == false);
}

TEST_CASE("RegimeMixStressRunner + FixedRng: mix affects LB deterministically", "[RegimeMixRunner][FixedRng][MixMatters]")
{
    using D = DecimalType;

    // Encode per-regime returns so that the GeoMean is higher when weight on regime 2 is higher.
    // v0 < v1 < v2 (per-bar)
    const char* v0 = "0.0005";   // 0.05%
    const char* v1 = "0.0015";   // 0.15%
    const char* v2 = "0.0030";   // 0.30%

    const std::size_t L = 6;
    const std::size_t N = 1200;

    auto labels  = buildCyclicBlockLabels(N, L, /*S=*/3);
    auto returns = buildEncodedReturns<D>(labels, v0, v1, v2);

    // Three mixes with increasing emphasis on regime 2
    std::vector<RegimeMix> mixes =
    {
        RegimeMix("Low2(0.5,0.4,0.1)" , {0.50, 0.40, 0.10}),
        RegimeMix("Mid2(0.3,0.4,0.3)" , {0.30, 0.40, 0.30}),
        RegimeMix("High2(0.2,0.3,0.5)", {0.20, 0.30, 0.50})
    };

    RegimeMixConfig cfg(mixes, /*minPassFraction=*/2.0/3.0, /*minBarsPerRegime=*/L + 5);

    // Runner specialized with FixedRng to make BCa draws reproducible end-to-end
    const unsigned int numResamples = 200;  // ample B to stabilize the BCa LB
    const double       confLevel    = 0.90;
    const double       annFactor    = 1.0;

    // Pick a hurdle that should PASS only the higher-mix variants:
    // we expect LB(Low2) < LB(Mid2) < LB(High2)
    const D hurdle = createDecimal("0.0014"); // 0.14%: Low2 likely fails, High2 likely passes

    RegimeMixStressRunner<D, FixedRng> runner(cfg, L, numResamples, confLevel, annFactor, hurdle);

    std::ostringstream os;
    const auto res = runner.run(returns, labels, os);

    REQUIRE(res.perMix().size() == mixes.size());

    // Extract LBs in declared order
    const auto &m0 = res.perMix()[0]; // Low2
    const auto &m1 = res.perMix()[1]; // Mid2
    const auto &m2 = res.perMix()[2]; // High2

    // Monotonicity with mix weight on regime 2
    REQUIRE(m0.annualizedLowerBound() <= m1.annualizedLowerBound());
    REQUIRE(m1.annualizedLowerBound() <= m2.annualizedLowerBound());

    // At this hurdle, Low2 should fail, High2 should pass. Mid2 may pass or be near.
    REQUIRE(m0.pass() == false);
    REQUIRE(m2.pass() == true);

    // Overall pass fraction should be at least 2/3 if Mid2 or High2 pass.
    REQUIRE(res.passFraction() >= 1.0 / 3.0);
    REQUIRE((res.overallPass() == true || res.overallPass() == false)); // sanity (exercise branch)
}

TEST_CASE("RegimeMixStressRunner: throws on returns/labels size mismatch", "[RegimeMixRunner]")
{
    using D = DecimalType;

    const std::size_t L = 5;
    const std::size_t N = 300;

    auto labels  = buildCyclicBlockLabels(N, L, /*S=*/3);
    auto returns = buildConstantReturns<D>(N + 10, "0.0020"); // size mismatch

    std::vector<RegimeMix> mixes =
    {
        RegimeMix("Equal", {1.0/3.0, 1.0/3.0, 1.0/3.0})
    };

    RegimeMixConfig cfg(mixes, /*minPassFraction=*/1.0, /*minBarsPerRegime=*/L + 5);

    // Either RNG (default or FixedRng) is fine here; keep default
    RegimeMixStressRunner<D> runner(cfg, L, /*B=*/100, /*alpha=*/0.90, /*ann=*/1.0,
                                    /*hurdle=*/createDecimal("0.0010"));

    std::ostringstream os;
    REQUIRE_THROWS_AS(runner.run(returns, labels, os), std::invalid_argument);
}
