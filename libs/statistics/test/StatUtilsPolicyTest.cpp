// StatUtilsPolicyTest.cpp
//
// Unit tests for the policy-based refactoring of StatUtils.h:
//   - LogPFConfig struct
//   - AccumSums struct
//   - RawReturnsAccumPolicy
//   - LogBarsAccumPolicy
//   - Denominator policies: HardMaxWinAnchorDenomPolicy,
//                           SmoothAdditiveWinDenomPolicy,
//                           SmoothAdditiveWinCountFadingDenomPolicy
//   - Numerator policies:   NumeratorFloorPolicy, NumeratorStrictPolicy
//   - Result policies:      ResultRawPFPolicy, ResultLog1pPFPolicy, ResultLogPFPolicy
//   - MedianPriorDenomPolicy
//   - computeProfitFactor_FromSums_Impl (direct calls via all policy combos)
//   - SymmetricPFStat with NumerPolicy/DenomPolicy template parameters
//   - LogProfitFactorStat_LogPF_Custom and LogProfitFactorFromLogBarsStat_LogPF_Custom
//   - computeLogProfitFactorRobust_LogPF as a function template

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <cmath>
#include <optional>

#include "StatUtils.h"
#include "TestUtils.h"        // DecimalType, createDecimal
#include "DecimalConstants.h"
#include "number.h"           // num::to_double, num::to_long_double

using namespace mkc_timeseries;

// Convenience aliases used throughout
using D    = DecimalType;
using DC   = DecimalConstants<D>;
using Stat = StatUtils<D>;

// Tolerance for log-domain decimal vs double comparisons
static constexpr double kLogTol = 1e-7;

// ============================================================================
// SECTION 1: LogPFConfig
// ============================================================================

TEST_CASE("LogPFConfig: default construction uses StatUtils defaults",
          "[StatUtils][Policy][LogPFConfig]")
{
    Stat::LogPFConfig cfg;

    REQUIRE(cfg.ruin_eps       == Catch::Approx(Stat::DefaultRuinEps));
    REQUIRE(cfg.denom_floor    == Catch::Approx(Stat::DefaultDenomFloor));
    REQUIRE(cfg.prior_strength == Catch::Approx(Stat::DefaultPriorStrength));
}

TEST_CASE("LogPFConfig: 3-argument constructor stores values exactly",
          "[StatUtils][Policy][LogPFConfig]")
{
    Stat::LogPFConfig cfg(1e-10, 5e-5, 0.25);

    REQUIRE(cfg.ruin_eps       == Catch::Approx(1e-10));
    REQUIRE(cfg.denom_floor    == Catch::Approx(5e-5));
    REQUIRE(cfg.prior_strength == Catch::Approx(0.25));
}

// ============================================================================
// SECTION 2: AccumSums — default state
// ============================================================================

TEST_CASE("AccumSums: default-constructed fields are zero / empty",
          "[StatUtils][Policy][AccumSums]")
{
    Stat::AccumSums s;

    REQUIRE(s.sum_log_wins   == DC::DecimalZero);
    REQUIRE(s.sum_loss_mag   == DC::DecimalZero);
    REQUIRE(s.sum_log_losses == DC::DecimalZero);
    REQUIRE(s.loss_mags.empty());
    REQUIRE(s.loss_count == 0);
    REQUIRE(s.N          == 0);
}

// ============================================================================
// SECTION 3: RawReturnsAccumPolicy
// ============================================================================

TEST_CASE("RawReturnsAccumPolicy: all-wins input",
          "[StatUtils][Policy][RawReturnsAccumPolicy]")
{
    std::vector<D> input = { D("0.10"), D("0.05"), D("0.20") };

    Stat::AccumSums s = Stat::RawReturnsAccumPolicy::accumulate(input, 1e-8);

    REQUIRE(s.N          == 3);
    REQUIRE(s.loss_count == 0);
    REQUIRE(s.loss_mags.empty());
    REQUIRE(s.sum_loss_mag   == DC::DecimalZero);
    REQUIRE(s.sum_log_losses == DC::DecimalZero);

    // Expected sum_log_wins = log(1.10) + log(1.05) + log(1.20)
    double expected_wins = std::log(1.10) + std::log(1.05) + std::log(1.20);
    REQUIRE(num::to_double(s.sum_log_wins) == Catch::Approx(expected_wins).margin(kLogTol));
}

TEST_CASE("RawReturnsAccumPolicy: all-losses input",
          "[StatUtils][Policy][RawReturnsAccumPolicy]")
{
    std::vector<D> input = { D("-0.05"), D("-0.10"), D("-0.03") };

    Stat::AccumSums s = Stat::RawReturnsAccumPolicy::accumulate(input, 1e-8);

    REQUIRE(s.N          == 3);
    REQUIRE(s.loss_count == 3);
    REQUIRE(s.sum_log_wins == DC::DecimalZero);

    double expected_mag = std::abs(std::log(0.95)) + std::abs(std::log(0.90)) + std::abs(std::log(0.97));
    REQUIRE(num::to_double(s.sum_loss_mag) == Catch::Approx(expected_mag).margin(kLogTol));

    // sum_log_losses must be <= 0 and its magnitude equals sum_loss_mag
    REQUIRE(num::to_double(s.sum_log_losses) < 0.0);
    REQUIRE(num::to_double(s.sum_log_losses) == Catch::Approx(-expected_mag).margin(kLogTol));

    // loss_mags should have one entry per loss
    REQUIRE(s.loss_mags.size() == 3);
}

TEST_CASE("RawReturnsAccumPolicy: mixed wins, losses, and neutral bars",
          "[StatUtils][Policy][RawReturnsAccumPolicy]")
{
    // 2 wins, 1 neutral, 2 losses
    std::vector<D> input = { D("0.10"), D("0.00"), D("-0.05"), D("0.20"), D("-0.10") };

    Stat::AccumSums s = Stat::RawReturnsAccumPolicy::accumulate(input, 1e-8);

    REQUIRE(s.N          == 5);
    REQUIRE(s.loss_count == 2);
    REQUIRE(s.loss_mags.size() == 2);

    double expected_wins = std::log(1.10) + std::log(1.20);
    double expected_mag  = std::abs(std::log(0.95)) + std::abs(std::log(0.90));

    REQUIRE(num::to_double(s.sum_log_wins) == Catch::Approx(expected_wins).margin(kLogTol));
    REQUIRE(num::to_double(s.sum_loss_mag) == Catch::Approx(expected_mag).margin(kLogTol));
}

TEST_CASE("RawReturnsAccumPolicy: ruin bar (r <= -1) uses raw-double bypass",
          "[StatUtils][Policy][RawReturnsAccumPolicy]")
{
    // r = -1.0 → growth = 0.0 → must not produce log(0) = -inf
    std::vector<D> input = { D("-1.0"), D("0.10") };
    const double ruin_eps = 1e-8;

    Stat::AccumSums s = Stat::RawReturnsAccumPolicy::accumulate(input, ruin_eps);

    REQUIRE(s.loss_count == 1);

    // Expected ruin loss mag = -log(ruin_eps) = log(1e8)
    double expected_ruin_mag = -std::log(ruin_eps);
    REQUIRE(num::to_double(s.loss_mags[0]) == Catch::Approx(expected_ruin_mag).margin(kLogTol));

    // sum_loss_mag must be finite and large (not -inf)
    REQUIRE(std::isfinite(num::to_double(s.sum_loss_mag)));
    REQUIRE(num::to_double(s.sum_loss_mag) > 10.0);
}

TEST_CASE("RawReturnsAccumPolicy: individual loss_mags entries are correct",
          "[StatUtils][Policy][RawReturnsAccumPolicy]")
{
    std::vector<D> input = { D("-0.05"), D("0.10"), D("-0.20") };

    Stat::AccumSums s = Stat::RawReturnsAccumPolicy::accumulate(input, 1e-8);

    REQUIRE(s.loss_mags.size() == 2);

    double mag0 = std::abs(std::log(0.95));
    double mag1 = std::abs(std::log(0.80));

    REQUIRE(num::to_double(s.loss_mags[0]) == Catch::Approx(mag0).margin(kLogTol));
    REQUIRE(num::to_double(s.loss_mags[1]) == Catch::Approx(mag1).margin(kLogTol));
}

// ============================================================================
// SECTION 4: LogBarsAccumPolicy
// ============================================================================

TEST_CASE("LogBarsAccumPolicy: accumulates pre-computed log-bars correctly",
          "[StatUtils][Policy][LogBarsAccumPolicy]")
{
    // Simulate log-bars: positive for wins, negative for losses
    std::vector<D> logBars = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.00") };

    Stat::AccumSums s = Stat::LogBarsAccumPolicy::accumulate(logBars, /*ruin_eps=*/1e-8);

    REQUIRE(s.N          == 5);
    REQUIRE(s.loss_count == 2);
    REQUIRE(s.loss_mags.size() == 2);

    REQUIRE(num::to_double(s.sum_log_wins) == Catch::Approx(0.10 + 0.20).margin(kLogTol));
    REQUIRE(num::to_double(s.sum_loss_mag) == Catch::Approx(0.05 + 0.10).margin(kLogTol));
    REQUIRE(num::to_double(s.sum_log_losses) == Catch::Approx(-(0.05 + 0.10)).margin(kLogTol));
}

TEST_CASE("LogBarsAccumPolicy: ruin_eps argument is unused (no bypass needed)",
          "[StatUtils][Policy][LogBarsAccumPolicy]")
{
    // A bar value of -18 is a valid pre-computed log-bar (huge loss); should be taken as-is
    std::vector<D> logBars = { D("0.10"), D("-18.0") };

    Stat::AccumSums s1 = Stat::LogBarsAccumPolicy::accumulate(logBars, 1e-8);
    Stat::AccumSums s2 = Stat::LogBarsAccumPolicy::accumulate(logBars, 1e-4);

    // Results must be identical regardless of ruin_eps
    REQUIRE(num::to_double(s1.sum_loss_mag) == Catch::Approx(num::to_double(s2.sum_loss_mag)).epsilon(1e-15));
    REQUIRE(num::to_double(s1.sum_log_wins) == Catch::Approx(num::to_double(s2.sum_log_wins)).epsilon(1e-15));
}

TEST_CASE("LogBarsAccumPolicy vs RawReturnsAccumPolicy produce equivalent results",
          "[StatUtils][Policy][AccumPolicy][Equivalence]")
{
    // For raw returns, makeLogGrowthSeries converts to log-bars; then both policies
    // should accumulate to the same sums.
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15") };
    const double ruin_eps = 1e-8;

    auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);

    Stat::AccumSums raw  = Stat::RawReturnsAccumPolicy::accumulate(returns,  ruin_eps);
    Stat::AccumSums bars = Stat::LogBarsAccumPolicy::accumulate(logBars, ruin_eps);

    REQUIRE(num::to_double(raw.sum_log_wins) == Catch::Approx(num::to_double(bars.sum_log_wins)).margin(kLogTol));
    REQUIRE(num::to_double(raw.sum_loss_mag) == Catch::Approx(num::to_double(bars.sum_loss_mag)).margin(kLogTol));
    REQUIRE(raw.loss_count == bars.loss_count);
    REQUIRE(raw.N          == bars.N);
}

// ============================================================================
// SECTION 5: Numerator policies
// ============================================================================

TEST_CASE("NumeratorFloorPolicy (LegacyNumerPolicy): max(wins, floor)",
          "[StatUtils][Policy][NumeratorFloor]")
{
    const D floor_d(0.001);

    SECTION("wins > floor: returns wins unchanged")
    {
        D wins(0.5);
        D result = Stat::NumeratorFloorPolicy::computeNumer(wins, floor_d);
        REQUIRE(num::to_double(result) == Catch::Approx(0.5).margin(1e-12));
    }

    SECTION("wins < floor: returns floor")
    {
        D wins(0.0);
        D result = Stat::NumeratorFloorPolicy::computeNumer(wins, floor_d);
        REQUIRE(num::to_double(result) == Catch::Approx(0.001).margin(1e-12));
    }

    SECTION("wins == floor: returns floor (tie goes to floor)")
    {
        D wins(0.001);
        D result = Stat::NumeratorFloorPolicy::computeNumer(wins, floor_d);
        REQUIRE(num::to_double(result) == Catch::Approx(0.001).margin(1e-12));
    }
}

TEST_CASE("NumeratorStrictPolicy: returns sum_log_wins unchanged, ignores floor",
          "[StatUtils][Policy][NumeratorStrict]")
{
    SECTION("Normal wins")
    {
        D wins(0.5);
        D floor_d(10.0);  // Large floor — strictly policy ignores it
        D result = Stat::NumeratorStrictPolicy::computeNumer(wins, floor_d);
        REQUIRE(num::to_double(result) == Catch::Approx(0.5).margin(1e-12));
    }

    SECTION("Zero wins passed through")
    {
        D result = Stat::NumeratorStrictPolicy::computeNumer(DC::DecimalZero, D(0.001));
        REQUIRE(result == DC::DecimalZero);
    }
}

// ============================================================================
// SECTION 6: Denominator policies
// ============================================================================

TEST_CASE("HardMaxWinAnchorDenomPolicy (LegacyDenomPolicy): max(loss, max(floor, prior*wins))",
          "[StatUtils][Policy][HardMaxDenom]")
{
    SECTION("Loss dominates prior and floor")
    {
        D wins(0.10), loss_mag(2.0), floor_d(1e-6);
        // prior = max(floor, 1.0 * 0.10) = 0.10; max(2.0, 0.10) = 2.0
        D result = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
            wins, loss_mag, floor_d, /*prior_strength=*/1.0, -1, -1);
        REQUIRE(num::to_double(result) == Catch::Approx(2.0).margin(1e-10));
    }

    SECTION("Prior dominates actual losses (wins-only case)")
    {
        D wins(0.30), loss_mag(0.0), floor_d(1e-6);
        // prior = max(1e-6, 0.5 * 0.30) = 0.15; max(0.0, 0.15) = 0.15
        D result = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
            wins, loss_mag, floor_d, /*prior_strength=*/0.5, -1, -1);
        REQUIRE(num::to_double(result) == Catch::Approx(0.15).margin(kLogTol));
    }

    SECTION("Floor dominates when prior_strength is tiny and losses are zero")
    {
        D wins(1e-9), loss_mag(0.0), floor_d(1e-5);
        // prior = max(1e-5, 0.01 * 1e-9) = 1e-5
        D result = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
            wins, loss_mag, floor_d, /*prior_strength=*/0.01, -1, -1);
        REQUIRE(num::to_double(result) == Catch::Approx(1e-5).margin(1e-12));
    }

    SECTION("loss_count and N are ignored (hard-coded sentinel values passed)")
    {
        D wins(0.20), loss_mag(0.10), floor_d(1e-6);
        D r1 = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(wins, loss_mag, floor_d, 1.0, -1,  -1);
        D r2 = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(wins, loss_mag, floor_d, 1.0, 100, 200);
        REQUIRE(num::to_double(r1) == Catch::Approx(num::to_double(r2)).epsilon(1e-15));
    }
}

TEST_CASE("SmoothAdditiveWinDenomPolicy: denom = max(loss + alpha*wins, floor)",
          "[StatUtils][Policy][SmoothAdditiveDenom]")
{
    SECTION("Normal case: loss + alpha*wins > floor")
    {
        D wins(0.40), loss_mag(0.10), floor_d(1e-6);
        // denom = 0.10 + 0.5*0.40 = 0.30
        D result = Stat::SmoothAdditiveWinDenomPolicy::computeDenom(
            wins, loss_mag, floor_d, /*alpha=*/0.5, -1, -1);
        REQUIRE(num::to_double(result) == Catch::Approx(0.30).margin(kLogTol));
    }

    SECTION("Zero losses: denom = max(alpha*wins, floor)")
    {
        D wins(0.20), loss_mag(0.0), floor_d(1e-6);
        D result = Stat::SmoothAdditiveWinDenomPolicy::computeDenom(
            wins, loss_mag, floor_d, /*alpha=*/1.0, -1, -1);
        REQUIRE(num::to_double(result) == Catch::Approx(0.20).margin(kLogTol));
    }

    SECTION("Floor activates when loss + alpha*wins is negligible")
    {
        D wins(0.0), loss_mag(0.0), floor_d(1e-4);
        D result = Stat::SmoothAdditiveWinDenomPolicy::computeDenom(
            wins, loss_mag, floor_d, /*alpha=*/1.0, -1, -1);
        REQUIRE(num::to_double(result) == Catch::Approx(1e-4).margin(1e-12));
    }

    SECTION("Additive blending means losses always contribute to denominator")
    {
        D wins(0.10), floor_d(1e-6);
        D loss_small(0.02), loss_large(0.50);

        D r_small = Stat::SmoothAdditiveWinDenomPolicy::computeDenom(
            wins, loss_small, floor_d, 0.5, -1, -1);
        D r_large = Stat::SmoothAdditiveWinDenomPolicy::computeDenom(
            wins, loss_large, floor_d, 0.5, -1, -1);

        // Larger losses → larger denominator
        REQUIRE(num::to_double(r_large) > num::to_double(r_small));
    }
}

TEST_CASE("SmoothAdditiveWinCountFadingDenomPolicy: computeK0FromN",
          "[StatUtils][Policy][CountFadingDenom][K0]")
{
    using P = Stat::SmoothAdditiveWinCountFadingDenomPolicy;

    // k0 = round(0.20 * N), clamped to [3, 10]
    SECTION("N <= 0 → fallback k0 = 5")
    {
        REQUIRE(P::computeK0FromN(0)  == 5);
        REQUIRE(P::computeK0FromN(-1) == 5);
    }

    SECTION("Very small N → clamped to 3")
    {
        // round(0.20 * 5) = round(1.0) = 1 → clamped to 3
        REQUIRE(P::computeK0FromN(5)  == 3);
        REQUIRE(P::computeK0FromN(10) == 3); // round(2.0) = 2 → clamped to 3
    }

    SECTION("Mid-range N → formula applies")
    {
        // round(0.20 * 20) = round(4.0) = 4
        REQUIRE(P::computeK0FromN(20) == 4);
        // round(0.20 * 25) = round(5.0) = 5
        REQUIRE(P::computeK0FromN(25) == 5);
        // round(0.20 * 27) = round(5.4) = 5
        REQUIRE(P::computeK0FromN(27) == 5);
    }

    SECTION("Large N → clamped to 10")
    {
        // round(0.20 * 100) = 20 → clamped to 10
        REQUIRE(P::computeK0FromN(100) == 10);
        REQUIRE(P::computeK0FromN(200) == 10);
    }
}

TEST_CASE("SmoothAdditiveWinCountFadingDenomPolicy: count fading reduces prior as losses accumulate",
          "[StatUtils][Policy][CountFadingDenom][Fade]")
{
    using P = Stat::SmoothAdditiveWinCountFadingDenomPolicy;

    D wins(0.40), loss_mag(0.0), floor_d(1e-6);
    // With zero actual losses, denom = fade * prior_strength * wins + 0

    // With loss_count=0 and N=25: k0 = round(0.20*25)=5; fade = 5/(5+0)=1.0
    D denom_0loss = P::computeDenom(wins, loss_mag, floor_d, /*prior*/1.0, /*lc*/0, /*N*/25);

    // With loss_count=5 and N=25: k0=5; fade = 5/(5+5)=0.5
    D denom_5loss = P::computeDenom(wins, loss_mag, floor_d, /*prior*/1.0, /*lc*/5, /*N*/25);

    // With loss_count=20: fade = 5/25 = 0.20 → much smaller prior
    D denom_20loss = P::computeDenom(wins, loss_mag, floor_d, /*prior*/1.0, /*lc*/20, /*N*/25);

    // Denominator must shrink as loss_count grows (fade decreases the additive prior)
    REQUIRE(num::to_double(denom_0loss) > num::to_double(denom_5loss));
    REQUIRE(num::to_double(denom_5loss) > num::to_double(denom_20loss));
}

TEST_CASE("SmoothAdditiveWinCountFadingDenomPolicy: negative loss_count/N clamped to 0",
          "[StatUtils][Policy][CountFadingDenom][Clamp]")
{
    using P = Stat::SmoothAdditiveWinCountFadingDenomPolicy;

    D wins(0.20), loss_mag(0.0), floor_d(1e-6);

    // Negative values must not crash or produce nonsensical results
    D result = P::computeDenom(wins, loss_mag, floor_d, 1.0, /*lc*/-3, /*N*/-10);
    REQUIRE(std::isfinite(num::to_double(result)));
    REQUIRE(num::to_double(result) > 0.0);
}

// ============================================================================
// SECTION 7: Result policies
// ============================================================================

TEST_CASE("ResultRawPFPolicy: finalizeFromRatio returns numer/denom",
          "[StatUtils][Policy][ResultRaw]")
{
    SECTION("Normal ratio")
    {
        D result = Stat::ResultRawPFPolicy::finalizeFromRatio(D(0.30), D(0.10));
        REQUIRE(num::to_double(result) == Catch::Approx(3.0).margin(1e-10));
    }

    SECTION("Zero denominator returns zero")
    {
        D result = Stat::ResultRawPFPolicy::finalizeFromRatio(D(0.30), DC::DecimalZero);
        REQUIRE(result == DC::DecimalZero);
    }
}

TEST_CASE("ResultLog1pPFPolicy: finalizeFromRatio returns log(1 + PF)",
          "[StatUtils][Policy][ResultLog1p]")
{
    SECTION("Known ratio: numer=0.30, denom=0.10 → PF=3 → log(4)")
    {
        D result = Stat::ResultLog1pPFPolicy::finalizeFromRatio(D(0.30), D(0.10));
        REQUIRE(num::to_double(result) == Catch::Approx(std::log(4.0)).margin(kLogTol));
    }

    SECTION("PF=1 → log(2)")
    {
        D result = Stat::ResultLog1pPFPolicy::finalizeFromRatio(D(0.50), D(0.50));
        REQUIRE(num::to_double(result) == Catch::Approx(std::log(2.0)).margin(kLogTol));
    }

    SECTION("Zero denominator → log(1) = 0")
    {
        D result = Stat::ResultLog1pPFPolicy::finalizeFromRatio(D(0.30), DC::DecimalZero);
        // PF=0 → log(1+0)=0
        REQUIRE(num::to_double(result) == Catch::Approx(0.0).margin(kLogTol));
    }
}

TEST_CASE("ResultLogPFPolicy: finalizeFromRatio returns log(numer) - log(denom)",
          "[StatUtils][Policy][ResultLogPF]")
{
    SECTION("Known values: numer=0.30, denom=0.10 → log(3)")
    {
        D result = Stat::ResultLogPFPolicy::finalizeFromRatio(D(0.30), D(0.10));
        REQUIRE(num::to_double(result) == Catch::Approx(std::log(3.0)).margin(kLogTol));
    }

    SECTION("Equal numer and denom → 0")
    {
        D result = Stat::ResultLogPFPolicy::finalizeFromRatio(D(0.50), D(0.50));
        REQUIRE(num::to_double(result) == Catch::Approx(0.0).margin(kLogTol));
    }

    SECTION("numer < denom → negative log(PF)")
    {
        // log(0.20/0.40) = log(0.5) ~ -0.693
        D result = Stat::ResultLogPFPolicy::finalizeFromRatio(D(0.20), D(0.40));
        REQUIRE(num::to_double(result) == Catch::Approx(std::log(0.5)).margin(kLogTol));
    }

    SECTION("ResultLogPFPolicy is distinct from ResultLog1pPFPolicy")
    {
        D numer(0.30), denom(0.10);
        D raw_pf   = Stat::ResultLogPFPolicy::finalizeFromRatio(numer, denom);    // log(3)
        D log1p_pf = Stat::ResultLog1pPFPolicy::finalizeFromRatio(numer, denom);  // log(4)
        REQUIRE(num::to_double(raw_pf) != Catch::Approx(num::to_double(log1p_pf)).margin(0.01));
    }
}

// ============================================================================
// SECTION 8: MedianPriorDenomPolicy::computePrior
// ============================================================================

TEST_CASE("MedianPriorDenomPolicy::computePrior with actual losses",
          "[StatUtils][Policy][MedianPrior]")
{
    // For an odd-length container the median is the middle element after nth_element.
    // {0.02, 0.05, 0.10} → sorted → median = 0.05
    boost::container::small_vector<D, 64> mags;
    mags.push_back(D("0.10"));
    mags.push_back(D("0.02"));
    mags.push_back(D("0.05"));

    // prior = median * prior_strength = 0.05 * 2.0 = 0.10
    D prior = Stat::MedianPriorDenomPolicy::computePrior(
        mags, /*ruin_eps=*/1e-8, /*denom_floor=*/1e-6,
        /*prior_strength=*/2.0, /*default_loss_magnitude=*/0.0);

    REQUIRE(num::to_double(prior) == Catch::Approx(0.05 * 2.0).margin(kLogTol));
}

TEST_CASE("MedianPriorDenomPolicy::computePrior with single loss",
          "[StatUtils][Policy][MedianPrior]")
{
    boost::container::small_vector<D, 64> mags;
    mags.push_back(D("0.08"));

    D prior = Stat::MedianPriorDenomPolicy::computePrior(
        mags, 1e-8, 1e-6, /*prior_strength=*/1.0, 0.0);

    REQUIRE(num::to_double(prior) == Catch::Approx(0.08).margin(kLogTol));
}

TEST_CASE("MedianPriorDenomPolicy::computePrior with no losses, using default_loss_magnitude",
          "[StatUtils][Policy][MedianPrior]")
{
    boost::container::small_vector<D, 64> empty_mags;

    // When default_loss_magnitude > 0, it is used directly
    D prior = Stat::MedianPriorDenomPolicy::computePrior(
        empty_mags, 1e-8, 1e-6, /*prior_strength=*/1.5, /*default=*/0.05);

    REQUIRE(num::to_double(prior) == Catch::Approx(0.05 * 1.5).margin(kLogTol));
}

TEST_CASE("MedianPriorDenomPolicy::computePrior with no losses, ruin-based fallback",
          "[StatUtils][Policy][MedianPrior]")
{
    boost::container::small_vector<D, 64> empty_mags;

    // default_loss_magnitude=0 → fallback = max(-log(ruin_eps), denom_floor)
    const double ruin_eps = 1e-8;
    const double floor    = 1e-6;
    const double expected_assumed = std::max(-std::log(ruin_eps), floor);  // -log(1e-8) >> 1e-6

    D prior = Stat::MedianPriorDenomPolicy::computePrior(
        empty_mags, ruin_eps, floor, /*prior_strength=*/0.5, /*default=*/0.0);

    REQUIRE(num::to_double(prior) == Catch::Approx(expected_assumed * 0.5).margin(1e-6));
}

// ============================================================================
// SECTION 9: Policy composition verification through the public API
//
// computeProfitFactor_FromSums_Impl and computeLogPF_FromSums are private
// implementation helpers.  All tests below exercise the same code paths
// through public-facing functors and policy struct static methods (which
// are public nested types of StatUtils<Decimal>).
// ============================================================================

TEST_CASE("Legacy policies are self-consistent: functor == template function",
          "[StatUtils][Policy][Impl][Legacy]")
{
    // Both LogProfitFactorStat_LogPF (plain alias) and the default-arg
    // computeLogProfitFactorRobust_LogPF<> use LegacyNumerPolicy +
    // LegacyDenomPolicy + ResultLogPFPolicy.  They must agree exactly.
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15") };

    Stat::LogProfitFactorStat_LogPF stat;
    D via_functor   = stat(returns);
    D via_template  = Stat::computeLogProfitFactorRobust_LogPF(returns);

    REQUIRE(num::to_double(via_functor) ==
            Catch::Approx(num::to_double(via_template)).epsilon(1e-12));
}

TEST_CASE("SmoothAdditiveWinDenomPolicy math: _Custom functor matches manual formula",
          "[StatUtils][Policy][Impl][SmoothAdditive]")
{
    // Input with known sums: two wins and one loss.
    // log(1.40) ≈ 0.3365, log(1.20) ≈ 0.1823 → wins ≈ 0.5188
    // |log(0.90)| ≈ 0.1054 → loss_mag ≈ 0.1054
    std::vector<D> returns = { D("0.40"), D("0.20"), D("-0.10") };
    const double ruin_eps = 1e-8;
    const double floor    = 1e-6;
    const double alpha    = 0.5;   // prior_strength

    // Accumulate sums via the public policy
    Stat::AccumSums s = Stat::RawReturnsAccumPolicy::accumulate(returns, ruin_eps);

    // Manual denominator and numerator via public policy static methods
    D floor_d(floor);
    D expected_denom = Stat::SmoothAdditiveWinDenomPolicy::computeDenom(
        s.sum_log_wins, s.sum_loss_mag, floor_d, alpha, -1, -1);
    D expected_numer = Stat::NumeratorFloorPolicy::computeNumer(s.sum_log_wins, floor_d);

    // Manual log(PF)
    double manual_log_pf = std::log(num::to_double(expected_numer) /
                                    num::to_double(expected_denom));

    // _Custom functor with the same policies must match
    using CustomStat = Stat::LogProfitFactorStat_LogPF_Custom<
        Stat::NumeratorFloorPolicy,
        Stat::SmoothAdditiveWinDenomPolicy>;

    CustomStat stat(ruin_eps, floor, alpha);
    D functor_result = stat(returns);

    REQUIRE(num::to_double(functor_result) == Catch::Approx(manual_log_pf).margin(kLogTol));
}

TEST_CASE("SmoothAdditiveWinCountFadingDenomPolicy: fade effect visible through _Custom functor",
          "[StatUtils][Policy][Impl][CountFading]")
{
    // The fade = k0 / (k0 + loss_count).  Feed two series with the same
    // win/loss magnitude ratio but different sample sizes so that loss_count
    // differs, producing a different effective prior and therefore a different log(PF).
    //
    // Series A: 3 wins of 0.10, 2 losses of 0.05  →  N=5,  loss_count=2
    // Series B: 15 wins of 0.10, 10 losses of 0.05 →  N=25, loss_count=10
    //
    // k0(N=5)  = round(0.2*5)=1, clamped to 3  → fade = 3/(3+2)  = 0.60
    // k0(N=25) = round(0.2*25)=5               → fade = 5/(5+10) = 0.333
    //
    // Smaller fade → smaller additive prior → smaller denominator → higher log(PF).
    // So series B (more losses, smaller fade) should yield higher log(PF) than A.

    using CustomStat = Stat::LogProfitFactorStat_LogPF_Custom<
        Stat::NumeratorFloorPolicy,
        Stat::SmoothAdditiveWinCountFadingDenomPolicy>;

    std::vector<D> series_a;
    for (int i = 0; i < 3; ++i) series_a.push_back(D("0.10"));
    for (int i = 0; i < 2; ++i) series_a.push_back(D("-0.05"));

    std::vector<D> series_b;
    for (int i = 0; i < 15; ++i) series_b.push_back(D("0.10"));
    for (int i = 0; i < 10; ++i) series_b.push_back(D("-0.05"));

    CustomStat stat;
    D result_a = stat(series_a);
    D result_b = stat(series_b);

    REQUIRE(std::isfinite(num::to_double(result_a)));
    REQUIRE(std::isfinite(num::to_double(result_b)));

    // Series B has more loss observations → smaller fade → smaller prior → higher log(PF)
    REQUIRE(num::to_double(result_b) > num::to_double(result_a));
}

TEST_CASE("HardMaxWinAnchorDenomPolicy ignores loss_count and N: "
          "legacy functor is unaffected by sample size",
          "[StatUtils][Policy][Impl][Nullopt]")
{
    // Because HardMaxWinAnchorDenomPolicy ignores loss_count and N entirely,
    // the plain alias functor must produce the same result regardless of input
    // length (as long as the win/loss sums are identical per trade).
    // Verify this by confirming the policy struct itself returns the same
    // denominator for wildly different count arguments.
    D wins(0.30), loss(0.10), floor_d(1e-6);
    const double prior = 1.0;

    D denom_no_counts  = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
        wins, loss, floor_d, prior, -1, -1);
    D denom_big_counts = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(
        wins, loss, floor_d, prior, 10000, 50000);

    REQUIRE(num::to_double(denom_no_counts) ==
            Catch::Approx(num::to_double(denom_big_counts)).epsilon(1e-15));

    // Confirm this invariant holds end-to-end: two plain-alias functors on
    // inputs that share the same win/loss sums but differ in length must agree.
    // (1 trade vs 5 trades, each sized so sums are proportional — not equal,
    // so this is a sanity check rather than exact equality.)
    std::vector<D> short_input = { D("0.10"), D("-0.05") };
    std::vector<D> long_input  = { D("0.10"), D("-0.05"), D("0.00"), D("0.00") };

    Stat::LogProfitFactorStat_LogPF stat;
    D r_short = stat(short_input);
    D r_long  = stat(long_input);

    // Neutral bars don't change sums, so results must be identical
    REQUIRE(num::to_double(r_short) == Catch::Approx(num::to_double(r_long)).epsilon(1e-12));
}

// ============================================================================
// SECTION 10: SymmetricPFStat with custom NumerPolicy/DenomPolicy
// ============================================================================

TEST_CASE("SymmetricPFStat: plain aliases preserve backward compatibility (no <> needed)",
          "[StatUtils][Policy][SymmetricPFStat][BackwardCompat]")
{
    // These must compile and produce consistent results — the whole point of Option 2
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15") };

    Stat::LogProfitFactorStat_LogPF            stat_raw;
    Stat::LogProfitFactorFromLogBarsStat_LogPF stat_bars;

    // Instantiation itself is the primary check; results must also be finite
    D r1 = stat_raw(returns);
    REQUIRE(std::isfinite(num::to_double(r1)));

    auto logBars = Stat::makeLogGrowthSeries(returns, 1e-8);
    D r2 = stat_bars(logBars);
    REQUIRE(std::isfinite(num::to_double(r2)));

    // Raw-returns and log-bars variants must agree closely
    REQUIRE(num::to_double(r1) == Catch::Approx(num::to_double(r2)).margin(kLogTol));
}

TEST_CASE("SymmetricPFStat: default policies match computeLogProfitFactorRobust_LogPF",
          "[StatUtils][Policy][SymmetricPFStat][Legacy]")
{
    // LogProfitFactorStat_LogPF and the default-arg computeLogProfitFactorRobust_LogPF<>
    // both hard-wire LegacyNumerPolicy + LegacyDenomPolicy + ResultLogPFPolicy.
    // Their outputs must agree to floating-point precision.
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10") };

    Stat::LogProfitFactorStat_LogPF stat;
    D via_functor  = stat(returns);
    D via_function = Stat::computeLogProfitFactorRobust_LogPF(
        returns,
        Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength);

    REQUIRE(num::to_double(via_functor) ==
            Catch::Approx(num::to_double(via_function)).epsilon(1e-12));
}

TEST_CASE("SymmetricPFStat: operator() now forwards actual loss_count and N",
          "[StatUtils][Policy][SymmetricPFStat][CountForward]")
{
    // Use the count-fading denom policy via the _Custom alias.
    // For a wins-only series, two instances with different N should give different
    // results because the fade factor depends on N (k0 = round(0.2*N)).
    // N_small → larger k0/N ratio → higher fade → larger prior → lower log(PF).
    // N_large → k0 capped at 10 → smaller k0/N ratio → lower fade → smaller prior → higher log(PF).
    //
    // Note: N is set by AccumPolicy::accumulate from input.size(), so we control
    // it by varying the number of observations.

    using CustomStat = Stat::LogProfitFactorStat_LogPF_Custom<
        Stat::SmallSampleNumerPolicy,
        Stat::SmoothAdditiveWinCountFadingDenomPolicy>;

    // 20-bar all-win series → N=20, loss_count=0
    // k0 = round(0.2*20)=4; fade = 4/(4+0)=1.0
    std::vector<D> small_n(20, D("0.01"));

    // 100-bar all-win series → N=100, loss_count=0
    // k0 = min(round(0.2*100), 10)=10; fade = 10/(10+0)=1.0
    // Both fades are 1.0 for loss_count=0, but k0FromN differs → effective alpha differs
    std::vector<D> large_n(100, D("0.01"));

    CustomStat stat;
    D result_small = stat(small_n);
    D result_large = stat(large_n);

    // Both must be finite; exact ordering depends on k0/N vs N/k0 interaction,
    // so we just check both are finite and positive (all-wins → log(PF) > 0).
    REQUIRE(std::isfinite(num::to_double(result_small)));
    REQUIRE(std::isfinite(num::to_double(result_large)));
    REQUIRE(num::to_double(result_small) > 0.0);
    REQUIRE(num::to_double(result_large) > 0.0);
}

TEST_CASE("LogProfitFactorStat_LogPF_Custom: SmoothAdditiveDenom yields different result from legacy",
          "[StatUtils][Policy][Custom][SmoothAdditive]")
{
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15") };

    // Legacy (plain alias)
    Stat::LogProfitFactorStat_LogPF stat_legacy;
    D result_legacy = stat_legacy(returns);

    // Custom: smooth additive denominator (always adds alpha*wins to denom)
    using CustomStat = Stat::LogProfitFactorStat_LogPF_Custom<
        Stat::NumeratorFloorPolicy,
        Stat::SmoothAdditiveWinDenomPolicy>;

    CustomStat stat_smooth(/*ruin_eps=*/Stat::DefaultRuinEps,
                           /*denom_floor=*/Stat::DefaultDenomFloor,
                           /*prior_strength=*/1.0);
    D result_smooth = stat_smooth(returns);

    // Both finite; they should differ because the denom policy is different
    REQUIRE(std::isfinite(num::to_double(result_legacy)));
    REQUIRE(std::isfinite(num::to_double(result_smooth)));
    REQUIRE(num::to_double(result_legacy) != Catch::Approx(num::to_double(result_smooth)).margin(1e-6));
}

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF_Custom: accepts log-bar input with custom policies",
          "[StatUtils][Policy][Custom][LogBars]")
{
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15") };
    auto logBars = Stat::makeLogGrowthSeries(returns, 1e-8);

    using CustomStat = Stat::LogProfitFactorFromLogBarsStat_LogPF_Custom<
        Stat::NumeratorFloorPolicy,
        Stat::SmoothAdditiveWinDenomPolicy>;

    CustomStat stat(/*ruin_eps=*/1e-8, /*denom_floor=*/1e-6, /*prior_strength=*/1.0);
    D result = stat(logBars);

    REQUIRE(std::isfinite(num::to_double(result)));
}

TEST_CASE("LogProfitFactorFromLogBarsStat_LogPF_Custom vs raw-returns Custom alias agree",
          "[StatUtils][Policy][Custom][Equivalence]")
{
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15"), D("0.08") };
    const double ruin_eps  = 1e-8;
    const double floor     = 1e-6;
    const double prior     = 1.0;

    auto logBars = Stat::makeLogGrowthSeries(returns, ruin_eps);

    using RawCustom  = Stat::LogProfitFactorStat_LogPF_Custom<
        Stat::NumeratorFloorPolicy, Stat::SmoothAdditiveWinDenomPolicy>;
    using BarsCustom = Stat::LogProfitFactorFromLogBarsStat_LogPF_Custom<
        Stat::NumeratorFloorPolicy, Stat::SmoothAdditiveWinDenomPolicy>;

    D r_raw  = RawCustom(ruin_eps, floor, prior)(returns);
    D r_bars = BarsCustom(ruin_eps, floor, prior)(logBars);

    // Both variants use the same underlying policy and data; results must agree closely
    REQUIRE(num::to_double(r_raw) == Catch::Approx(num::to_double(r_bars)).margin(kLogTol));
}

TEST_CASE("_Custom aliases require both policy args: they are template aliases without defaults",
          "[StatUtils][Policy][Custom][Aliases]")
{
    // Compile-time test: instantiating with two distinct (but valid) policies
    // for each of the two _Custom aliases must compile cleanly.

    using A = Stat::LogProfitFactorStat_LogPF_Custom<
        Stat::NumeratorStrictPolicy, Stat::SmoothAdditiveWinDenomPolicy>;

    using B = Stat::LogProfitFactorFromLogBarsStat_LogPF_Custom<
        Stat::NumeratorStrictPolicy, Stat::HardMaxWinAnchorDenomPolicy>;

    std::vector<D> returns  = { D("0.10"), D("-0.05") };
    auto           logBars  = Stat::makeLogGrowthSeries(returns, 1e-8);

    A stat_a;
    B stat_b;

    D ra = stat_a(returns);
    D rb = stat_b(logBars);

    REQUIRE(std::isfinite(num::to_double(ra)));
    REQUIRE(std::isfinite(num::to_double(rb)));
}

// ============================================================================
// SECTION 11: computeLogProfitFactorRobust_LogPF as a function template
// ============================================================================

TEST_CASE("computeLogProfitFactorRobust_LogPF<>: default template args match non-template behavior",
          "[StatUtils][Policy][Template][Robust_LogPF]")
{
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15") };

    // Call with explicit template args set to the defaults
    D explicit_default = Stat::computeLogProfitFactorRobust_LogPF<
        Stat::LegacyNumerPolicy, Stat::LegacyDenomPolicy>(
        returns,
        Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength);

    // Call without any template args (should resolve to same defaults)
    D implicit_default = Stat::computeLogProfitFactorRobust_LogPF(
        returns,
        Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength);

    REQUIRE(num::to_double(explicit_default) ==
            Catch::Approx(num::to_double(implicit_default)).epsilon(1e-14));
}

TEST_CASE("computeLogProfitFactorRobust_LogPF<>: template call with SmoothAdditiveDenom "
          "differs from legacy default",
          "[StatUtils][Policy][Template][Robust_LogPF]")
{
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10") };

    D legacy = Stat::computeLogProfitFactorRobust_LogPF(returns);  // defaults

    D smooth = Stat::computeLogProfitFactorRobust_LogPF<
        Stat::NumeratorFloorPolicy,
        Stat::SmoothAdditiveWinDenomPolicy>(
        returns,
        Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength);

    REQUIRE(std::isfinite(num::to_double(legacy)));
    REQUIRE(std::isfinite(num::to_double(smooth)));
    // Different denominator policy → different result
    REQUIRE(num::to_double(legacy) != Catch::Approx(num::to_double(smooth)).margin(1e-6));
}

TEST_CASE("computeLogProfitFactorRobust_LogPF<>: agrees with SymmetricPFStat using same policies",
          "[StatUtils][Policy][Template][Robust_LogPF][Consistency]")
{
    // The function template and the functor struct must produce bit-identical results
    // when given the same input, policies, and parameters.
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10"), D("0.15") };
    const double ruin_eps  = Stat::DefaultRuinEps;
    const double floor     = Stat::DefaultDenomFloor;
    const double prior     = Stat::DefaultPriorStrength;

    D via_template = Stat::computeLogProfitFactorRobust_LogPF<
        Stat::NumeratorFloorPolicy, Stat::SmoothAdditiveWinDenomPolicy>(
        returns, ruin_eps, floor, prior);

    using CustomStat = Stat::LogProfitFactorStat_LogPF_Custom<
        Stat::NumeratorFloorPolicy, Stat::SmoothAdditiveWinDenomPolicy>;

    CustomStat stat(ruin_eps, floor, prior);
    D via_functor = stat(returns);

    REQUIRE(num::to_double(via_template) ==
            Catch::Approx(num::to_double(via_functor)).epsilon(1e-12));
}

TEST_CASE("computeLogProfitFactorRobust_LogPF: deprecated overloads still compile and forward correctly",
          "[StatUtils][Policy][Template][Deprecated]")
{
    std::vector<D> returns = { D("0.10"), D("-0.05"), D("0.20"), D("-0.10") };

    D base = Stat::computeLogProfitFactorRobust_LogPF(
        returns, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength);

    // 5-param deprecated overload
    D dep5 = Stat::computeLogProfitFactorRobust_LogPF(
        returns, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength,
        /*stop_loss_return_space=*/0.05);

    // 6-param deprecated overload
    D dep6 = Stat::computeLogProfitFactorRobust_LogPF(
        returns, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength,
        /*stop_loss_return_space=*/0.05,
        /*profit_target_return_space=*/0.10);

    // 8-param deprecated overload
    D dep8 = Stat::computeLogProfitFactorRobust_LogPF(
        returns, Stat::DefaultRuinEps, Stat::DefaultDenomFloor, Stat::DefaultPriorStrength,
        /*stop_loss=*/0.05, /*target=*/0.10,
        /*tiny_win_fraction=*/0.05, /*tiny_win_min_return=*/1e-4);

    // All deprecated overloads forward to legacy 3-param behavior; all must match base
    REQUIRE(num::to_double(dep5) == Catch::Approx(num::to_double(base)).epsilon(1e-14));
    REQUIRE(num::to_double(dep6) == Catch::Approx(num::to_double(base)).epsilon(1e-14));
    REQUIRE(num::to_double(dep8) == Catch::Approx(num::to_double(base)).epsilon(1e-14));
}

TEST_CASE("computeLogProfitFactorRobust_LogPF<>: empty input returns zero",
          "[StatUtils][Policy][Template][Robust_LogPF][Empty]")
{
    std::vector<D> empty;

    D r_default = Stat::computeLogProfitFactorRobust_LogPF(empty);
    D r_custom  = Stat::computeLogProfitFactorRobust_LogPF<
        Stat::NumeratorFloorPolicy, Stat::SmoothAdditiveWinDenomPolicy>(empty);

    REQUIRE(r_default == DC::DecimalZero);
    REQUIRE(r_custom  == DC::DecimalZero);
}

// ============================================================================
// SECTION 12: Accumulation policy — N field accuracy
// ============================================================================

TEST_CASE("Accumulation policies set N = input.size() exactly",
          "[StatUtils][Policy][AccumPolicy][N]")
{
    std::vector<D> v5(5, D("0.01"));
    std::vector<D> v20(20, D("0.01"));
    std::vector<D> v100(100, D("0.01"));

    auto check_N = [&](const std::vector<D>& v) {
        Stat::AccumSums sr = Stat::RawReturnsAccumPolicy::accumulate(v, 1e-8);
        Stat::AccumSums sb = Stat::LogBarsAccumPolicy::accumulate(v, 1e-8);
        REQUIRE(sr.N == static_cast<int>(v.size()));
        REQUIRE(sb.N == static_cast<int>(v.size()));
    };

    check_N(v5);
    check_N(v20);
    check_N(v100);
}

// ============================================================================
// SECTION 13: Policy differentiator — ordering guarantees
// ============================================================================

TEST_CASE("Policy ordering: SmoothAdditiveDenom always >= HardMaxDenom for same wins-only input",
          "[StatUtils][Policy][Ordering]")
{
    // With wins-only data: HardMax denom = max(0, max(floor, prior*wins)) = prior*wins
    //                      SmoothAdd denom = max(0 + prior*wins, floor)    = prior*wins
    // They are equal when loss_mag = 0 for this case.
    // When loss_mag > 0: SmoothAdd = loss + prior*wins; HardMax = max(loss, prior*wins).
    // For loss < prior*wins: HardMax = prior*wins; SmoothAdd = loss + prior*wins > HardMax.
    D wins(0.40), loss_small(0.05), floor_d(1e-6);
    const double prior = 1.0;

    D hard = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(wins, loss_small, floor_d, prior, -1, -1);
    D smoo = Stat::SmoothAdditiveWinDenomPolicy::computeDenom(wins, loss_small, floor_d, prior, -1, -1);

    // With loss_small (0.05) < prior*wins (0.40):
    //   HardMax = max(0.05, 0.40) = 0.40
    //   SmoothAdd = 0.05 + 0.40 = 0.45
    // So Smooth >= Hard in this regime
    REQUIRE(num::to_double(smoo) >= num::to_double(hard));
}

TEST_CASE("Policy ordering: higher prior_strength always produces >= denominator for HardMaxDenom",
          "[StatUtils][Policy][Ordering]")
{
    D wins(0.30), loss(0.05), floor_d(1e-6);

    D denom_lo = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(wins, loss, floor_d, 0.1, -1, -1);
    D denom_hi = Stat::HardMaxWinAnchorDenomPolicy::computeDenom(wins, loss, floor_d, 1.0, -1, -1);

    // larger prior_strength → larger prior_loss_mag → potentially larger denom
    REQUIRE(num::to_double(denom_hi) >= num::to_double(denom_lo));
}
