// StatUtilsPermutationTest.cpp
//
// Regression tests for computeLogProfitFactorRobust_LogPF in the permutation
// testing context (LegacyNumerPolicy + HardMaxWinAnchorDenomPolicy,
// prior_strength = 0.01).
//
// BACKGROUND
// ----------
// The permutation testing path uses the legacy (hard-max) denominator policy:
//
//   prior_loss_mag = max(floor_d, prior_strength × sum_log_wins)
//   denom          = max(sum_loss_mag, prior_loss_mag)
//
// The prior is therefore completely inert whenever:
//
//   sum_loss_mag >= prior_strength × sum_log_wins
//
// which holds for the vast majority of permutations of a real series.  The
// prior only activates for the sparse-loss tail, capping log(PF) at
// log(1/prior_strength) = log(100) ≈ 4.605.
//
// After reviewing the code, the recommendation was to run an empirical check:
// construct the 10,000-permutation null distribution and verify no pileup
// occurs near log(100).  These tests convert that check into deterministic,
// reproducible unit tests by sweeping representative win-rate scenarios.
//
// THREE CONCERNS ADDRESSED
// ------------------------
// Section 1 — Structural: HardMaxWinAnchorDenomPolicy is inert for typical
//   permutation inputs and only activates below a well-defined threshold.
//
// Section 2 — End-to-end: Specific permutation-like scenarios produce the
//   expected log(PF) values through the full call path.
//
// Section 3 — Distribution: A deterministic win-rate sweep proves the cap
//   binds only for the pure no-loss case (prior=0.01), and a control test
//   proves the tests are sensitive — prior=0.5 (the default) would cause
//   7 out of 20 sweep cases to pile up, and these tests would catch it.
//
// FILE PLACEMENT
// --------------
// These tests are in a separate file from StatUtilsPolicyTest.cpp because:
//   • They operate at distribution level, not policy-unit level.
//   • The control test deliberately validates failure behavior (prior=0.5
//     causes pileup), which is an unusual pattern among unit tests.
//   • They can be run in isolation via the [Permutation] tag.
//
// Run just these tests:
//   ./test_runner [Permutation]
//
// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <cmath>
#include <limits>

#include "StatUtils.h"
#include "TestUtils.h"
#include "DecimalConstants.h"
#include "number.h"

using namespace mkc_timeseries;

using D    = DecimalType;
using DC   = DecimalConstants<D>;
using Stat = StatUtils<D>;

// Tolerance for log-domain comparisons
static constexpr double kLogTol    = 1e-7;

// The prior_strength used by the production permutation testing path.
static constexpr double kPermPrior = 0.01;

// The resulting cap on log(PF): log(1 / kPermPrior) = log(100) ≈ 4.605.
// Computed at runtime because std::log is not constexpr in C++17.
static const double kCapValue = std::log(1.0 / kPermPrior);

// Build a return series of n_wins wins and n_losses losses at the given
// per-bar return sizes.  Order does not matter for the accumulation policies.
static std::vector<D> makePermSeries(int n_wins, int n_losses,
                                      const char* win_ret,
                                      const char* loss_ret)
{
    std::vector<D> bars;
    bars.reserve(n_wins + n_losses);
    for (int i = 0; i < n_wins;   ++i) bars.push_back(D(win_ret));
    for (int i = 0; i < n_losses; ++i) bars.push_back(D(loss_ret));
    return bars;
}

// ============================================================================
// SECTION 1: HardMaxWinAnchorDenomPolicy — permutation disengagement
//
// Verifies the structural property that makes this policy suitable for
// permutation testing: the prior is inert when losses are adequate, and
// only activates below a clear threshold.
// ============================================================================

TEST_CASE("HardMaxWinAnchorDenomPolicy: prior is inert for typical permutation input",
          "[StatUtils][Permutation][HardMaxDenom][Disengagement]")
{
    // A permutation with ~12% loss bars (typical for many strategies):
    //   sum_log_wins  = 0.40
    //   sum_loss_mag  = 0.05
    //   threshold     = 0.01 × 0.40 = 0.004
    //   0.05 >> 0.004 → prior completely inert
    //   denom must equal sum_loss_mag exactly, not perturbed by the prior.

    const D wins(0.40), loss_mag(0.05), floor_d(D(Stat::DefaultDenomFloor));

    D denom = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
        wins, loss_mag, floor_d, kPermPrior, -1, -1);

    REQUIRE(num::to_double(denom) == Catch::Approx(0.05).margin(kLogTol));
}

TEST_CASE("HardMaxWinAnchorDenomPolicy: prior activates only below the threshold",
          "[StatUtils][Permutation][HardMaxDenom][Activation]")
{
    // Sparse-loss permutation: sum_loss_mag = 0.001, threshold = 0.004.
    // sum_loss_mag < threshold → prior takes over.
    // denom = max(0.001, max(floor_d, 0.004)) = 0.004.

    const D wins(0.40), loss_mag(0.001), floor_d(D(Stat::DefaultDenomFloor));
    const double expected_threshold = kPermPrior * 0.40; // 0.004

    D denom = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
        wins, loss_mag, floor_d, kPermPrior, -1, -1);

    REQUIRE(num::to_double(denom) == Catch::Approx(expected_threshold).margin(kLogTol));
}

TEST_CASE("HardMaxWinAnchorDenomPolicy: default prior_strength=0.5 clips typical permutation",
          "[StatUtils][Permutation][HardMaxDenom][DefaultPriorWrong]")
{
    // Documents WHY prior_strength=0.01 was chosen over the default 0.5.
    //
    // With prior_strength=0.5 and the same input (wins=0.40, loss_mag=0.05):
    //   threshold = 0.5 × 0.40 = 0.20
    //   loss_mag (0.05) < threshold (0.20) → prior IS active
    //   denom = 0.20 instead of 0.05
    //
    // This inflates the denominator 4× and silently clips log(PF) from
    // log(8) ≈ 2.08 down to log(2) ≈ 0.69 — a 3× error on a perfectly
    // typical profitable permutation.  The null distribution is distorted.
    //
    // With prior_strength=0.01: denom = 0.05 (correct, prior inert).

    const D wins(0.40), loss_mag(0.05), floor_d(D(Stat::DefaultDenomFloor));

    D denom_correct = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
        wins, loss_mag, floor_d, /*prior=*/0.01, -1, -1);

    D denom_default = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
        wins, loss_mag, floor_d, /*prior=*/0.5, -1, -1);

    // With 0.01: prior inert, denominator equals actual losses
    REQUIRE(num::to_double(denom_correct) == Catch::Approx(0.05).margin(kLogTol));

    // With 0.5: prior active, denominator inflated to 0.20
    REQUIRE(num::to_double(denom_default) == Catch::Approx(0.20).margin(kLogTol));

    // The distortion is severe: denominator is 4× larger with the default prior
    REQUIRE(num::to_double(denom_default) > num::to_double(denom_correct) * 3.5);
}

// ============================================================================
// SECTION 2: computeLogProfitFactorRobust_LogPF — end-to-end permutation path
// ============================================================================

TEST_CASE("computeLogProfitFactorRobust_LogPF: all-wins series caps at log(100)",
          "[StatUtils][Permutation][FullPath][Cap]")
{
    // A permutation that draws no losing bars produces:
    //   denom = prior_strength × sum_log_wins
    //   log(PF) = log(sum_log_wins / (prior × sum_log_wins)) = log(1/prior) = log(100)
    //
    // This is the intended upper bound: a finite, large-but-specific value
    // rather than an unbounded result that would corrupt the null distribution.

    const std::vector<D> all_wins(20, D("0.01"));

    D result = Stat::computeLogProfitFactorRobust_LogPF(
        all_wins, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, kPermPrior);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) == Catch::Approx(kCapValue).margin(kLogTol));
}

TEST_CASE("computeLogProfitFactorRobust_LogPF: balanced permutation returns raw log(PF)",
          "[StatUtils][Permutation][FullPath][Balanced]")
{
    // 50 wins and 50 losses of equal return magnitude.
    // sum_loss_mag (≈0.2506) >> threshold (0.01 × 0.2494 ≈ 0.0025) → prior inert.
    // Result must equal the analytically computed raw log(PF).
    //
    // Note: log(1+r) ≠ -log(1-r), so with equal returns the result is very
    // slightly negative (~-0.005), not exactly zero.

    const double r       = 0.005;
    const int    n       = 50;
    const double raw_w   = static_cast<double>(n) * std::log(1.0 + r);
    const double raw_l   = static_cast<double>(n) * (-std::log(1.0 - r));
    const double expected = std::log(raw_w) - std::log(raw_l);

    std::vector<D> bars;
    for (int i = 0; i < n; ++i) bars.push_back(D("0.005"));
    for (int i = 0; i < n; ++i) bars.push_back(D("-0.005"));

    D result = Stat::computeLogProfitFactorRobust_LogPF(
        bars, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, kPermPrior);

    REQUIRE(std::isfinite(num::to_double(result)));
    // Prior is inert: result equals the unregularized raw log(PF)
    REQUIRE(num::to_double(result) == Catch::Approx(expected).margin(kLogTol));
    // Approximately zero, well below the cap
    REQUIRE(std::abs(num::to_double(result)) < 0.01);
}

TEST_CASE("computeLogProfitFactorRobust_LogPF: profitable permutation is unregularized",
          "[StatUtils][Permutation][FullPath][Profitable]")
{
    // 60 wins and 40 losses of equal return magnitude (+/-1%).
    // sum_loss_mag (≈0.402) >> threshold (0.01 × 0.597 ≈ 0.006) → prior inert.
    // Result must equal the analytically computed raw log(PF) exactly.
    // Prior must contribute nothing.

    const double r        = 0.01;
    const int    n_wins   = 60;
    const int    n_losses = 40;
    const double raw_w    = static_cast<double>(n_wins)   * std::log(1.0 + r);
    const double raw_l    = static_cast<double>(n_losses) * (-std::log(1.0 - r));
    const double expected = std::log(raw_w) - std::log(raw_l);

    std::vector<D> bars;
    for (int i = 0; i < n_wins;   ++i) bars.push_back(D("0.01"));
    for (int i = 0; i < n_losses; ++i) bars.push_back(D("-0.01"));

    D result = Stat::computeLogProfitFactorRobust_LogPF(
        bars, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, kPermPrior);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) == Catch::Approx(expected).margin(kLogTol));
    // Must be well below the cap — prior did not interfere
    REQUIRE(num::to_double(result) < kCapValue - 1.0);
}

TEST_CASE("computeLogProfitFactorRobust_LogPF: all-losses series has large negative log(PF)",
          "[StatUtils][Permutation][FullPath][AllLoss]")
{
    // Permutation with no wins: numer = max(0, floor_d) = 1e-6 (tiny).
    // Large denominator → very negative log(PF).
    // Verifies the lower tail of the null distribution is correctly
    // unbounded — necessary so that losing permutations are clearly
    // distinguishable from the observed strategy result.

    const std::vector<D> all_losses(20, D("-0.01"));

    D result = Stat::computeLogProfitFactorRobust_LogPF(
        all_losses, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, kPermPrior);

    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) < -5.0);
}

// ============================================================================
// SECTION 3: Null distribution — pileup regression tests
//
// These are the unit-test equivalent of the recommended empirical check.
//
// A sweep of 20 win-rate scenarios (n_wins = 5, 10, ..., 100 out of 100 bars,
// equal win/loss magnitudes) is used to verify that:
//
//   1. The cap at log(100) binds ONLY for the pure no-loss case (n_wins=100).
//      For all other cases the prior is inert and the result is the raw log(PF).
//
//   2. A control test with prior_strength=0.5 (the default) confirms that 7
//      out of 20 cases pile up at log(2) ≈ 0.693, proving the pileup test is
//      genuinely sensitive to the key regression.
//
//   3. The null distribution is strictly monotone in win rate, confirming
//      there are no discontinuities or anomalies in the distribution shape.
//
// MATH VERIFICATION
// -----------------
// With r = 0.005 and 100 bars, the prior disengages when:
//   n_losses × (-log(0.995)) >= 0.01 × n_wins × log(1.005)
//   n_losses/n_wins >= 0.01 × (log(1.005) / (-log(0.995))) ≈ 0.00995
//
// For n_wins=99, n_losses=1: 1/99 ≈ 0.01010 > 0.00995 → inert.
// Therefore, prior is inert for ALL cases with n_losses >= 1 in a 100-bar
// equal-magnitude series.  Only n_wins=100 triggers the cap.
// ============================================================================

TEST_CASE("Null distribution: equal-magnitude sweep — cap binds only at 100% wins",
          "[StatUtils][Permutation][NullDist][PileupAbsence]")
{
    // Sweep n_wins = 5, 10, ..., 100 with equal win/loss magnitudes (r=0.005).
    // For each case with losses: verify result equals the analytically computed
    // raw log(PF) — confirming the prior is completely inert.
    // Count results near the cap (within 0.01): must be at most 1.

    const int    n_bars = 100;
    const double r      = 0.005;
    const double eps    = 0.01;

    int near_cap_count = 0;

    for (int n_wins = 5; n_wins <= 100; n_wins += 5)
    {
        const int n_losses = n_bars - n_wins;
        auto bars = makePermSeries(n_wins, n_losses, "0.005", "-0.005");

        D result = Stat::computeLogProfitFactorRobust_LogPF(
            bars, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, kPermPrior);

        const double rv = num::to_double(result);
        INFO("n_wins=" << n_wins << " n_losses=" << n_losses
                       << " → log(PF)=" << rv
                       << " cap=" << kCapValue);

        REQUIRE(std::isfinite(rv));
        REQUIRE(rv <= kCapValue + eps);

        if (n_losses > 0)
        {
            // Prior must be completely inert: result equals the raw log(PF)
            const double raw_w    = static_cast<double>(n_wins)   * std::log(1.0 + r);
            const double raw_l    = static_cast<double>(n_losses) * (-std::log(1.0 - r));
            const double expected = std::log(raw_w) - std::log(raw_l);
            REQUIRE(rv == Catch::Approx(expected).margin(kLogTol));
        }

        if (rv >= kCapValue - eps)
            ++near_cap_count;
    }

    // Only the pure no-loss case (n_wins=100) should reach the cap.
    // Any value > 1 indicates the prior is incorrectly activating for
    // permutations that contain real loss data.
    REQUIRE(near_cap_count <= 1);
}

TEST_CASE("Null distribution: prior_strength=0.5 causes pileup — regression sensitivity proof",
          "[StatUtils][Permutation][NullDist][DefaultPriorPileup]")
{
    // CONTROL TEST: proves the pileup test above is a meaningful regression guard.
    //
    // With prior_strength=0.5 and equal magnitudes (r=0.005), the prior is
    // active when:
    //   n_losses × (-log(0.995)) < 0.5 × n_wins × log(1.005)
    //   n_losses / n_wins < 0.5 × (log(1.005) / (-log(0.995))) ≈ 0.4975
    //
    // In the sweep, this activates for n_wins = 70, 75, 80, 85, 90, 95, 100
    // (7 cases), all of which are pulled to exactly log(2) ≈ 0.693.
    //
    // If this test passes (count >= 6), the prior_strength=0.01 test above is
    // confirmed as a genuine regression detector — not a vacuous assertion.

    const int    n_bars  = 100;
    const double cap_0_5 = std::log(1.0 / 0.5); // log(2) ≈ 0.693
    const double eps     = 0.05;

    int near_cap_count = 0;

    for (int n_wins = 5; n_wins <= 100; n_wins += 5)
    {
        const int n_losses = n_bars - n_wins;
        auto bars = makePermSeries(n_wins, n_losses, "0.005", "-0.005");

        D result = Stat::computeLogProfitFactorRobust_LogPF(
            bars, Stat::DefaultRuinEps, Stat::DefaultDenomFloor,
            /*prior_strength=*/0.5);  // DEFAULT — wrong for permutation testing

        const double rv = num::to_double(result);
        INFO("n_wins=" << n_wins << " (prior=0.5) → log(PF)=" << rv);

        REQUIRE(std::isfinite(rv));

        if (rv >= cap_0_5 - eps)
            ++near_cap_count;
    }

    // With prior_strength=0.5, 7 permutation outcomes pile up at log(2).
    // The assertion below will FAIL if prior_strength is correctly set to 0.01,
    // so this test must be run only with prior=0.5 as written above.
    REQUIRE(near_cap_count >= 6);
}

TEST_CASE("Null distribution: log(PF) is strictly monotone in win rate",
          "[StatUtils][Permutation][NullDist][Monotone]")
{
    // When the prior is inert, log(PF) = log(raw_wins / raw_losses) which is
    // strictly increasing in win rate.  Monotonicity across the sweep confirms
    // there are no discontinuities or anomalies in the null distribution shape
    // — a necessary (though not sufficient) condition for distributional smoothness.
    //
    // Sweep n_wins = 5 to 95 (all cases where prior is inert).

    const int n_bars = 100;
    double prev = -std::numeric_limits<double>::infinity();

    for (int n_wins = 5; n_wins <= 95; n_wins += 5)
    {
        const int n_losses = n_bars - n_wins;
        auto bars = makePermSeries(n_wins, n_losses, "0.005", "-0.005");

        D result = Stat::computeLogProfitFactorRobust_LogPF(
            bars, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, kPermPrior);

        const double rv = num::to_double(result);
        INFO("n_wins=" << n_wins << " → log(PF)=" << rv << " (prev=" << prev << ")");

        REQUIRE(rv > prev);
        prev = rv;
    }
}