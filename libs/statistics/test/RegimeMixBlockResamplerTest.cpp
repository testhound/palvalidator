// RegimeMixBlockResamplerTest.cpp
//
// Unit tests for RegimeMixBlockResampler using Catch2.
// Mirrors the style of BiasCorrectedBootstrapTest.cpp (DecimalType/createDecimal, etc.)
//
// Requirements validated:
//  - Constructor validation (weights size, negativity, zero-sum)
//  - operator(): returns correct length; approximately honors target mix
//  - operator(): handles empty-pool regime by reassigning quota without crashing
//  - jackknife(): returns ceil(n/L) pseudo-values (delete-block style)
//  - meanBlockLen(): equals constructor L
//
// Notes:
//  - We construct returns whose values encode the regime (0→v0, 1→v1, 2→v2)
//    so we can infer mix directly from the resampled series.
//  - Tolerances are set with blockwise slack because quotas are per-block starts
//    and rounding/wrap behavior can slightly deviate from exact targets.
//
// Dependencies:
//  - RegimeMixBlockResampler.h
//  - RegimeLabeler.h (for label domain type, not strictly required here)
//  - TestUtils.h, number.h (DecimalType/createDecimal)
//  - Catch2
//  - randutils (rng)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <cmath>

#include "RegimeMixBlockResampler.h"
#include "TestUtils.h"   // DecimalType, createDecimal
#include "number.h"

// If your project already includes randutils globally, you can remove this.
// Otherwise, include your randutils header path here.
#include <randutils.hpp>

using palvalidator::resampling::RegimeMixBlockResampler;

namespace
{

  struct FixedRng
  {
    std::mt19937_64 eng;

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
  
  template <class Num>
  static std::vector<Num> buildEncodedReturns(const std::vector<int>& labels,
					      Num v0,
					      Num v1,
					      Num v2)
  {
    std::vector<Num> x; x.reserve(labels.size());
    for (int z : labels)
      {
        switch (z)
	  {
	  case 0: x.push_back(v0); break;
	  case 1: x.push_back(v1); break;
	  case 2: x.push_back(v2); break;
	  default: x.push_back(v1); break; // default mid-vol if any unexpected label
	  }
      }
    return x;
  }

  // Build a labels array of length N composed of consecutive blocks of length L,
  // cycling regimes 0,1,2,… so each regime has plenty of valid block starts.
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

  // Count occurrences of encoded values (v0/v1/v2) in a resampled vector y.
  // Uses a small absolute tolerance to match decimal types robustly.
  template <class Num>
  static std::tuple<std::size_t, std::size_t, std::size_t>
  countEncoded(const std::vector<Num>& y, Num v0, Num v1, Num v2, double tol = 1e-12)
  {
    auto eq = [&](Num a, Num b)
    {
      const double da = a.getAsDouble();
      const double db = b.getAsDouble();
      return std::fabs(da - db) <= tol;
    };
    std::size_t c0 = 0, c1 = 0, c2 = 0;
    for (const auto& v : y)
      {
        if (eq(v, v0)) ++c0;
        else if (eq(v, v1)) ++c1;
        else if (eq(v, v2)) ++c2;
      }
    return {c0, c1, c2};
  }

} // namespace

TEST_CASE("RegimeMixBlockResampler: constructor validation", "[RegimeMixResampler]")
{
    using D = DecimalType;

    const std::size_t L = 5;
    const std::size_t N = 60;
    auto labels = buildCyclicBlockLabels(N, L, /*S=*/3);

    SECTION("Weights size must match number of regimes")
    {
        // labels contain regimes 0,1,2 => three regimes
        std::vector<double> badW = {0.5, 0.5};
        REQUIRE_THROWS_AS((RegimeMixBlockResampler<D>(L, labels, badW)), std::invalid_argument);
    }

    SECTION("Weights cannot be negative and cannot sum to zero")
    {
        std::vector<double> negW = {0.5, -0.2, 0.7};
        REQUIRE_THROWS_AS((RegimeMixBlockResampler<D>(L, labels, negW)), std::invalid_argument);

        std::vector<double> zeroSum = {0.0, 0.0, 0.0};
        REQUIRE_THROWS_AS((RegimeMixBlockResampler<D>(L, labels, zeroSum)), std::invalid_argument);
    }

    SECTION("Valid weights are accepted and normalized internally")
    {
        std::vector<double> w = {2.0, 1.0, 1.0}; // will normalize to 0.5,0.25,0.25
        REQUIRE_NOTHROW((RegimeMixBlockResampler<D>(L, labels, w)));
    }
}

TEST_CASE("RegimeMixBlockResampler: operator() length and approximate mix adherence", "[RegimeMixResampler]")
{
    using D = DecimalType;

    const std::size_t L = 5;
    const std::size_t N = 300; // base series length
    const std::size_t n = 300; // resampled length

    // Build labels with plentiful valid starts for each regime
    auto labels = buildCyclicBlockLabels(N, L, /*S=*/3);

    // Encode regimes as distinct return magnitudes
    const D v0 = createDecimal("0.0010");
    const D v1 = createDecimal("0.0020");
    const D v2 = createDecimal("0.0030");
    auto returns = buildEncodedReturns<D>(labels, v0, v1, v2);

    // Target weights (normalized internally): 0.2, 0.5, 0.3
    std::vector<double> w = {0.2, 0.5, 0.3};

palvalidator::resampling::RegimeMixBlockResampler<D, FixedRng> sampler(L,
								       labels,
								       w,
								       L + 5);

    FixedRng rng(123456789ULL);
    auto y = sampler(returns, n, rng);

    REQUIRE(y.size() == n);

    // Count composition by encoded values
    auto [c0, c1, c2] = countEncoded(y, v0, v1, v2);

    const double p0 = static_cast<double>(c0) / n;
    const double p1 = static_cast<double>(c1) / n;
    const double p2 = static_cast<double>(c2) / n;

    // Because we sample in blocks with quotas in bars and rounding, allow generous block-level tolerance
    // Expect within ±0.12 absolute of targets.
    REQUIRE(p0 == Catch::Approx(0.20).margin(0.04));
    REQUIRE(p1 == Catch::Approx(0.50).margin(0.04));
    REQUIRE(p2 == Catch::Approx(0.30).margin(0.04));
}

TEST_CASE("RegimeMixBlockResampler: handles empty-pool regimes by reassigning quota", "[RegimeMixResampler]")
{
    using D = DecimalType;

    const std::size_t L = 6;
    const std::size_t N = 120;
    const std::size_t n = 120;

    // Build labels where regime 2 EXISTS but has NO valid starts:
    // - First N - (L - 1) bars are alternating blocks of 0 and 1, length L each,
    //   ensuring plenty of valid starts for regimes 0 and 1.
    // - Last (L - 1) bars are labeled 2, but cannot be block starts because
    //   t + L would exceed N for those indices.
    std::vector<int> labels; labels.reserve(N);

    // Fill alternating 0/1 blocks over the prefix
    std::size_t prefix = N - (L - 1);
    int cur = 0; // 0 then 1, repeat
    for (std::size_t i = 0; i < prefix; )
    {
        const std::size_t take = std::min<std::size_t>(L, prefix - i);
        for (std::size_t j = 0; j < take; ++j)
        {
            labels.push_back(cur);
        }
        i += take;
        cur = (cur + 1) % 2; // cycle 0,1
    }

    // Tail of length (L - 1) labeled as regime 2 (no valid starts in tail)
    for (std::size_t j = 0; j < (L - 1); ++j)
    {
        labels.push_back(2);
    }

    // Sanity: labels length N and contain regimes 0,1,2
    REQUIRE(labels.size() == N);
    REQUIRE(*std::max_element(labels.begin(), labels.end()) == 2);

    const D v0 = createDecimal("0.0100");
    const D v1 = createDecimal("0.0200");
    const D v2 = createDecimal("0.0300"); // regime 2 present but has no start pool

    auto returns = buildEncodedReturns<D>(labels, v0, v1, v2);

    // Ask for weight on regime 2; sampler should reassign when pool is empty and still produce n bars
    std::vector<double> w = {0.3, 0.3, 0.4};

    RegimeMixBlockResampler<D> sampler(L, labels, w, /*minBarsPerRegime=*/L + 5);

    randutils::mt19937_rng rng;

    auto y = sampler(returns, n, rng);
    REQUIRE(y.size() == n);

    // Since regime 2 had no pool, ensure the composition heavily favors regimes 0/1
    auto [c0, c1, c2] = countEncoded(y, v0, v1, v2);

    REQUIRE(c2 <= n * 0.25); // much less than requested 40%
    REQUIRE((c0 + c1) >= static_cast<std::size_t>(n * 0.70));
}

TEST_CASE("RegimeMixBlockResampler: jackknife returns ceil(n/L) pseudo-values", "[RegimeMixResampler]")
{
    using D = DecimalType;

    const std::size_t L = 5;
    const std::size_t N = 47;

    auto labels = buildCyclicBlockLabels(N, L, /*S=*/3);

    // Build simple increasing series
    std::vector<D> x; x.reserve(N);
    for (std::size_t i = 0; i < N; ++i)
    {
        x.push_back(createDecimal(std::to_string(0.001 * (1 + i))));
    }

    // Stateless mean for jackknife
    auto meanFn = [](const std::vector<D>& v) -> D
    {
      D s(0);
      for (const auto &e : v)
	s += e;
      return s / D(v.size());
    };

    RegimeMixBlockResampler<D> sampler(L, labels, /*weights=*/{1.0, 0.0, 0.0}, /*minBarsPerRegime=*/L+1);

    auto jk = sampler.jackknife(x, meanFn);

    const std::size_t expected = (N + L - 1) / L; // ceil(N/L)
    REQUIRE(jk.size() == expected);

    // Means should be finite and vary a bit
    for (const auto& th : jk)
    {
      REQUIRE(std::isfinite(th.getAsDouble()));
    }
}

TEST_CASE("RegimeMixBlockResampler: meanBlockLen reports constructor L", "[RegimeMixResampler]")
{
    using D = DecimalType;

    const std::size_t L = 7;
    const std::size_t N = 200;

    auto labels = buildCyclicBlockLabels(N, L, /*S=*/3);
    RegimeMixBlockResampler<D> sampler(L, labels, /*weights=*/{0.2, 0.5, 0.3}, /*minBarsPerRegime=*/L+5);

    REQUIRE(sampler.meanBlockLen() == L);
}

TEST_CASE("RegimeMixBlockResampler: produces L-homogeneous blocks when start pools require it",
          "[RegimeMixResampler][Homogeneity]")
{
    using D = DecimalType;

    // Choose L and sizes so quotas are multiples of L (avoids partial blocks)
    const std::size_t L = 5;
    const std::size_t N = 2000; // base series length
    const std::size_t n = 1500; // resampled length (multiple of L)

    // Labels are built as runs of length L cycling 0,1,2,...
    // => valid start pools only at run starts produce L-homogeneous blocks.
    auto labels = buildCyclicBlockLabels(N, L, /*S=*/3);

    // Encode regimes with distinct magnitudes so we can check homogeneity via equality
    const D v0 = createDecimal("0.0010");
    const D v1 = createDecimal("0.0020");
    const D v2 = createDecimal("0.0030");
    auto returns = buildEncodedReturns<D>(labels, v0, v1, v2);

    // Target weights chosen so quotas are exact multiples of L:
    // n = 1500 → quotas = (300, 900, 300) which are all multiples of 5
    std::vector<double> w = {0.20, 0.60, 0.20};

    // Use deterministic RNG specialization to keep the test reproducible
    palvalidator::resampling::RegimeMixBlockResampler<D, FixedRng> sampler(
        L, labels, w, /*minBarsPerRegime=*/L + 5
    );

    FixedRng rng(987654321ULL);

    auto y = sampler(returns, n, rng);
    REQUIRE(y.size() == n);

    // Check that every contiguous block of size L is pure-regime:
    // i.e., all L values in the block equal to one of {v0,v1,v2}
    auto eq = [](D a, D b)
    {
      return std::fabs(a.getAsDouble() - b.getAsDouble()) <= 1e-12;
    };

    const std::size_t numBlocks = n / L;
    REQUIRE(numBlocks * L == n); // sanity: exact multiple

    for (std::size_t b = 0; b < numBlocks; ++b)
    {
        const std::size_t start = b * L;
        const D first = y[start];

        // First, ensure it matches one of the encoded regime values
        bool isRegimeVal = eq(first, v0) || eq(first, v1) || eq(first, v2);
        REQUIRE(isRegimeVal);

        // Then, ensure all remaining elements in the block equal 'first'
        for (std::size_t j = 1; j < L; ++j)
        {
            REQUIRE(eq(y[start + j], first));
        }
    }
}
