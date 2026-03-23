// AutoBootstrapSelectorMOutOfNReliabilityTest.cpp
//
// Catch2 unit tests for M-out-of-N reliability gating added to
// AutoBootstrapSelector and AutoCIResult.
//
// Coverage plan
// ─────────────
// §1  Compilation fix: makeTestCandidate updated for algorithmIsReliable
// §2  Candidate: algorithmIsReliable field and accessor
// §3  MockMOutOfNEngine: engine with reliability flags on Result
// §4  summarizePercentileLike: hard gate (distribution_degenerate / insufficient_spread)
// §5  summarizePercentileLike: soft gate (excessive_bias / ratio_near_boundary)
// §6  summarizePercentileLike: combinations of flags
// §7  summarizePercentileLike: SFINAE no-op for non-MOutOfN engines
// §8  select(): lower bound haircut when MOutOfN wins unreliably
// §9  select(): hard gate disqualifies MOutOfN, other method wins
// §10 select(): soft penalty down-weights MOutOfN but does not eliminate it

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <cmath>
#include <limits>
#include <string>

#include "number.h"
#include "StatUtils.h"
#include "TestUtils.h"
#include "AutoCIResult.h"
#include "AutoBootstrapSelector.h"
#include "AutoBootstrapConfiguration.h"

using D         = DecimalType;
using Selector  = palvalidator::analysis::AutoBootstrapSelector<D>;
using Result    = palvalidator::analysis::AutoCIResult<D>;
using MethodId  = Result::MethodId;
using Candidate = Result::Candidate;
using ScoringWeights = Selector::ScoringWeights;
using mkc_timeseries::StatisticSupport;

// ─────────────────────────────────────────────────────────────────────────────
// §1  Updated makeTestCandidate — now includes algorithmIsReliable
//
// NOTE: This replaces the equivalent helper in AutoBootstrapSelectorRefactorTest.cpp.
// The existing helper omits the new algorithmIsReliable parameter and will fail
// to compile against the updated Candidate constructor. Either update that file's
// helper or use this one. Both default to true for backward compatibility.
// ─────────────────────────────────────────────────────────────────────────────

static Candidate makeTestCandidate(
    MethodId    method      = MethodId::Percentile,
    double      mean        = 5.0,
    double      lower       = 4.0,
    double      upper       = 6.0,
    double      cl          = 0.95,
    std::size_t n           = 100,
    std::size_t B_outer     = 1000,
    std::size_t B_inner     = 0,
    std::size_t effective_B = 950,
    std::size_t skipped     = 50,
    double      se_boot     = 0.5,
    double      skew_boot   = 0.2,
    double      median_boot = 5.0,
    double      center_shift= 0.1,
    double      norm_length = 1.0,
    double      order_pen   = 0.0,
    double      length_pen  = 0.0,
    double      stab_pen    = 0.0,
    double      z0          = 0.0,
    double      accel       = 0.0,
    double      inner_fail  = 0.0,
    bool        accel_rel   = true,
    bool        algo_rel    = true,
    bool        excessive_bias = false,
    double      excess_bias    = 0.0)   // max(0, bias_fraction - threshold), pre-computed
{
    return Candidate(
        method, D(mean), D(lower), D(upper), cl, n,
        B_outer, B_inner, effective_B, skipped,
        se_boot, skew_boot, median_boot, center_shift, norm_length,
        order_pen, length_pen, stab_pen, z0, accel, inner_fail,
        std::numeric_limits<double>::quiet_NaN(),  // score
        0, 0, false,                               // id, rank, is_chosen
        accel_rel, algo_rel, excessive_bias, excess_bias);
}

// ─────────────────────────────────────────────────────────────────────────────
// §2  Candidate: algorithmIsReliable field and accessor
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Candidate: algorithmIsReliable field defaults to true",
          "[AutoBootstrapSelector][Reliability][Candidate]")
{
    SECTION("Default constructor sets algorithmIsReliable = true")
    {
        auto c = makeTestCandidate();
        REQUIRE(c.getAlgorithmIsReliable());
    }

    SECTION("algorithmIsReliable = false is stored and returned correctly")
    {
        auto c = makeTestCandidate(MethodId::MOutOfN,
            5.0, 4.0, 6.0, 0.95, 30, 800, 0, 780, 20,
            0.5, 0.3, 5.0, 0.1, 1.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            /*accel_rel=*/true, /*algo_rel=*/false);
        REQUIRE_FALSE(c.getAlgorithmIsReliable());
    }

    SECTION("withScore preserves algorithmIsReliable")
    {
        auto c = makeTestCandidate(MethodId::MOutOfN,
            5.0, 4.0, 6.0, 0.95, 30, 800, 0, 780, 20,
            0.5, 0.3, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            true, false);
        auto c2 = c.withScore(1.5);
        REQUIRE_FALSE(c2.getAlgorithmIsReliable());
    }

    SECTION("withMetadata preserves algorithmIsReliable")
    {
        auto c = makeTestCandidate(MethodId::MOutOfN,
            5.0, 4.0, 6.0, 0.95, 30, 800, 0, 780, 20,
            0.5, 0.3, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            true, false);
        auto c2 = c.withMetadata(42, 1, true);
        REQUIRE_FALSE(c2.getAlgorithmIsReliable());
    }

    SECTION("accelIsReliable and algorithmIsReliable are independent")
    {
        // BCa: accelIsReliable can be false, algorithmIsReliable stays true
        auto bca = makeTestCandidate(MethodId::BCa,
            5.0, 4.0, 6.0, 0.95, 30, 800, 0, 780, 20,
            0.5, 0.3, 5.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.1, 0.05, 0.0,
            /*accel_rel=*/false, /*algo_rel=*/true, /*excessive_bias=*/false);
        REQUIRE_FALSE(bca.getAccelIsReliable());
        REQUIRE(bca.getAlgorithmIsReliable());
        REQUIRE_FALSE(bca.getExcessiveBias());

        // MOutOfN: algorithmIsReliable can be false, accelIsReliable stays true
        auto moon = makeTestCandidate(MethodId::MOutOfN,
            5.0, 4.0, 6.0, 0.95, 30, 800, 0, 780, 20,
            0.5, 0.3, 5.0, 0.1, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            /*accel_rel=*/true, /*algo_rel=*/false, /*excessive_bias=*/true);
        REQUIRE(moon.getAccelIsReliable());
        REQUIRE_FALSE(moon.getAlgorithmIsReliable());
        REQUIRE(moon.getExcessiveBias());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §3  MockMOutOfNEngine — engine with full reliability flag surface
// ─────────────────────────────────────────────────────────────────────────────

class MockMOutOfNEngine
{
public:
    // Mirrors MOutOfNPercentileBootstrap::RELIABILITY_BIAS_FRACTION_THRESHOLD
    // so summarizeMOutOfN can access it via MOutOfNEngine:: on the mock type.
    static constexpr double RELIABILITY_BIAS_FRACTION_THRESHOLD = 0.20;

    // Result mirrors the fields summarizePercentileLike reads plus the
    // reliability flags added by MOutOfNPercentileBootstrap.
    struct Result
    {
        D           mean;
        D           lower;
        D           upper;
        double      cl          = 0.95;
        std::size_t n           = 30;
        std::size_t B           = 800;
        std::size_t effective_B = 780;
        std::size_t skipped     = 20;
        double      skew_boot   = 0.0;
        double      computed_ratio = 0.75;

        // Reliability flags — matching MOutOfNPercentileBootstrap::Result
        bool distribution_degenerate = false;
        bool excessive_bias          = false;
        bool insufficient_spread     = false;
        bool ratio_near_boundary     = false;
        double bias_fraction         = 0.0;

        bool isReliable() const noexcept
        {
            return !distribution_degenerate
                && !excessive_bias
                && !insufficient_spread
                && !ratio_near_boundary;
        }
    };

    MockMOutOfNEngine()
        : m_has_diagnostics(false)
        , m_bootstrap_mean(0.0)
        , m_bootstrap_se(0.0)
        , m_is_reliable(true)
    {}

    void setBootstrapStatistics(const std::vector<double>& stats)
    {
        m_stats           = stats;
        m_has_diagnostics = true;

        double sum = 0.0;
        for (double v : stats) sum += v;
        m_bootstrap_mean = sum / static_cast<double>(stats.size());

        double sq = 0.0;
        for (double v : stats)
        {
            double d = v - m_bootstrap_mean;
            sq += d * d;
        }
        m_bootstrap_se = (stats.size() > 1)
            ? std::sqrt(sq / static_cast<double>(stats.size() - 1))
            : 0.0;
    }

    void setResult(const Result& r)     { m_result = r; }
    void setIsReliable(bool rel)        { m_is_reliable = rel; }

    // Standard diagnostic interface
    bool hasDiagnostics()                              const { return m_has_diagnostics; }
    const std::vector<double>& getBootstrapStatistics() const { return m_stats; }
    double getBootstrapMean()                          const { return m_bootstrap_mean; }
    double getBootstrapSe()                            const { return m_bootstrap_se; }

    // Reliability interface — detected by SFINAE helpers in AutoBootstrapSelector
    bool isReliable() const { return m_is_reliable; }

private:
    bool                 m_has_diagnostics;
    std::vector<double>  m_stats;
    double               m_bootstrap_mean;
    double               m_bootstrap_se;
    bool                 m_is_reliable;
    Result               m_result;
};

// Helper: build a MockMOutOfNEngine pre-loaded with varied bootstrap statistics
static MockMOutOfNEngine makeVariedMoonEngine(
    double mean_val = 0.50,
    double lower    = 0.44,
    double upper    = 0.56)
{
    MockMOutOfNEngine engine;
    std::vector<double> stats = {0.42, 0.46, 0.50, 0.54, 0.58, 0.48, 0.52};
    engine.setBootstrapStatistics(stats);

    MockMOutOfNEngine::Result res;
    res.mean        = D(mean_val);
    res.lower       = D(lower);
    res.upper       = D(upper);
    res.cl          = 0.95;
    res.n           = 30;
    res.B           = 800;
    res.effective_B = 780;
    res.skipped     = 20;
    engine.setResult(res);
    return engine;
}

// ─────────────────────────────────────────────────────────────────────────────
// §4  summarizePercentileLike: hard gate
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("summarizePercentileLike: hard gate fires for distribution_degenerate",
          "[AutoBootstrapSelector][Reliability][HardGate]")
{
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = true;   // hard gate
    res.insufficient_spread   = false;
    res.excessive_bias        = false;
    res.ratio_near_boundary   = false;
    engine.setResult(res);
    engine.setIsReliable(false);

    SECTION("Stability penalty is infinity when distribution_degenerate")
    {
        auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
        REQUIRE(std::isinf(c.getStabilityPenalty()));
    }

    SECTION("algorithmIsReliable is false when distribution_degenerate")
    {
        auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
        REQUIRE_FALSE(c.getAlgorithmIsReliable());
    }
}

TEST_CASE("summarizePercentileLike: hard gate fires for insufficient_spread",
          "[AutoBootstrapSelector][Reliability][HardGate]")
{
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = false;
    res.insufficient_spread   = true;    // hard gate
    res.excessive_bias        = false;
    res.ratio_near_boundary   = false;
    engine.setResult(res);
    engine.setIsReliable(false);

    SECTION("Stability penalty is infinity when insufficient_spread")
    {
        auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
        REQUIRE(std::isinf(c.getStabilityPenalty()));
    }

    SECTION("algorithmIsReliable is false when insufficient_spread")
    {
        auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
        REQUIRE_FALSE(c.getAlgorithmIsReliable());
    }
}

TEST_CASE("summarizePercentileLike: both hard gate flags produce infinity penalty",
          "[AutoBootstrapSelector][Reliability][HardGate]")
{
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = true;
    res.insufficient_spread   = true;
    res.excessive_bias        = false;
    res.ratio_near_boundary   = false;
    engine.setResult(res);
    engine.setIsReliable(false);

    auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
    REQUIRE(std::isinf(c.getStabilityPenalty()));
    REQUIRE_FALSE(c.getAlgorithmIsReliable());
}

// ─────────────────────────────────────────────────────────────────────────────
// §5  summarizePercentileLike: soft gate
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("summarizePercentileLike: soft gate fires for excessive_bias",
          "[AutoBootstrapSelector][Reliability][SoftGate]")
{
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = false;
    res.insufficient_spread   = false;
    res.excessive_bias        = true;    // soft gate
    res.ratio_near_boundary   = false;
    engine.setResult(res);
    engine.setIsReliable(false);

    SECTION("Stability penalty equals kMOutOfNUnreliabilityPenalty")
    {
        auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
        REQUIRE(c.getStabilityPenalty() ==
                Catch::Approx(AutoBootstrapConfiguration::kMOutOfNUnreliabilityPenalty));
    }

    SECTION("Stability penalty is finite (not infinity)")
    {
        auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
        REQUIRE(std::isfinite(c.getStabilityPenalty()));
    }

    SECTION("algorithmIsReliable is false when excessive_bias")
    {
        auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
        REQUIRE_FALSE(c.getAlgorithmIsReliable());
    }
}

TEST_CASE("summarizePercentileLike: soft gate fires for ratio_near_boundary",
          "[AutoBootstrapSelector][Reliability][SoftGate]")
{
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = false;
    res.insufficient_spread   = false;
    res.excessive_bias        = false;
    res.ratio_near_boundary   = true;    // soft gate
    engine.setResult(res);
    engine.setIsReliable(false);

    auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
    REQUIRE(c.getStabilityPenalty() ==
            Catch::Approx(AutoBootstrapConfiguration::kMOutOfNUnreliabilityPenalty));
    REQUIRE(std::isfinite(c.getStabilityPenalty()));
    REQUIRE_FALSE(c.getAlgorithmIsReliable());
}

// ─────────────────────────────────────────────────────────────────────────────
// §6  summarizePercentileLike: flag combinations
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("summarizePercentileLike: hard gate takes precedence over soft gate",
          "[AutoBootstrapSelector][Reliability][Combinations]")
{
    // When both a hard flag and a soft flag are set, the hard gate fires
    // and the penalty is infinity regardless of the soft flag value.
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = true;   // hard gate
    res.excessive_bias        = true;     // soft gate also set
    res.insufficient_spread   = false;
    res.ratio_near_boundary   = false;
    engine.setResult(res);
    engine.setIsReliable(false);

    auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);

    REQUIRE(std::isinf(c.getStabilityPenalty()));
    REQUIRE_FALSE(c.getAlgorithmIsReliable());
}

TEST_CASE("summarizePercentileLike: all four flags set produces infinity penalty",
          "[AutoBootstrapSelector][Reliability][Combinations]")
{
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = true;
    res.insufficient_spread   = true;
    res.excessive_bias        = true;
    res.ratio_near_boundary   = true;
    engine.setResult(res);
    engine.setIsReliable(false);

    auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
    REQUIRE(std::isinf(c.getStabilityPenalty()));
    REQUIRE_FALSE(c.getAlgorithmIsReliable());
}

TEST_CASE("summarizePercentileLike: no flags set produces zero stability penalty",
          "[AutoBootstrapSelector][Reliability][Combinations]")
{
    auto engine = makeVariedMoonEngine();

    MockMOutOfNEngine::Result res;
    res.mean                  = D(0.50);
    res.lower                 = D(0.44);
    res.upper                 = D(0.56);
    res.cl                    = 0.95;
    res.n                     = 30;
    res.B                     = 800;
    res.effective_B           = 780;
    res.skipped               = 20;
    res.distribution_degenerate = false;
    res.insufficient_spread   = false;
    res.excessive_bias        = false;
    res.ratio_near_boundary   = false;
    engine.setResult(res);
    engine.setIsReliable(true);

    auto c = Selector::summarizePercentileLike(MethodId::MOutOfN, engine, res);
    REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.0));
    REQUIRE(c.getAlgorithmIsReliable());
}

// ─────────────────────────────────────────────────────────────────────────────
// §7  SFINAE no-op for non-MOutOfN engines
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("summarizePercentileLike: reliability logic is no-op for non-MOutOfN engines",
          "[AutoBootstrapSelector][Reliability][SFINAE]")
{
    // Use the existing MockPercentileLikeEngine (no reliability flags) to
    // verify that the SFINAE helpers return safe defaults and the stability
    // penalty remains zero for Percentile, Basic, and Normal methods.
    //
    // This test exercises the if constexpr path in extractHasHardFailure /
    // extractHasSoftFailure / extractAlgorithmReliable that returns false/true
    // for engines without the reliability flag fields.

    // Minimal mock without reliability flags — simulates Percentile/Basic/Normal
    struct PlainEngine
    {
        struct Result
        {
            D           mean       = D(5.0);
            D           lower      = D(4.0);
            D           upper      = D(6.0);
            double      cl         = 0.95;
            std::size_t n          = 100;
            std::size_t B          = 1000;
            std::size_t effective_B= 950;
            std::size_t skipped    = 50;
            double      skew_boot  = 0.2;
            // No reliability flag fields
        };

        bool hasDiagnostics() const { return true; }
        double getBootstrapMean() const { return 5.0; }
        double getBootstrapSe()   const { return 0.5; }
        const std::vector<double>& getBootstrapStatistics() const { return m_stats; }

        std::vector<double> m_stats = {4.5, 4.8, 5.0, 5.2, 5.5,
                                       4.6, 4.9, 5.1, 5.3, 5.4};
    };

    PlainEngine engine;
    PlainEngine::Result res;

    SECTION("Percentile method has zero stability penalty")
    {
        auto c = Selector::summarizePercentileLike(
            MethodId::Percentile, engine, res);
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.0));
        REQUIRE(c.getAlgorithmIsReliable());
    }

    SECTION("Basic method has zero stability penalty")
    {
        auto c = Selector::summarizePercentileLike(
            MethodId::Basic, engine, res);
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.0));
        REQUIRE(c.getAlgorithmIsReliable());
    }

    SECTION("Normal method has zero stability penalty")
    {
        auto c = Selector::summarizePercentileLike(
            MethodId::Normal, engine, res);
        REQUIRE(c.getStabilityPenalty() == Catch::Approx(0.0));
        REQUIRE(c.getAlgorithmIsReliable());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §8  select(): lower bound haircut when MOutOfN wins unreliably
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("select: adaptive haircut applied when MOutOfN wins with excessive_bias",
          "[AutoBootstrapSelector][Reliability][Haircut]")
{
    ScoringWeights weights;
    StatisticSupport unbounded = StatisticSupport::unbounded();

    // Helper to compute expected haircut from excess_bias (= bias_fraction - threshold),
    // matching the formula in AutoBootstrapSelector::select() Phase 5b.
    // The threshold subtraction is done by summarizeMOutOfN before storing on Candidate,
    // so select() and this helper receive the pre-computed excess directly.
    auto expectedHaircut = [](double excess_bias) -> double
    {
        const double scale = AutoBootstrapConfiguration::kMOutOfNHaircutScale;
        const double cap   = AutoBootstrapConfiguration::kMOutOfNMaxHaircutFraction;
        const double raw   = excess_bias * scale;
        return std::min(raw, cap);
    };

    // Helper: given a raw bias_fraction, compute the excess stored by summarizeMOutOfN.
    // Uses MockMOutOfNEngine::RELIABILITY_BIAS_FRACTION_THRESHOLD — the same constant
    // that summarizeMOutOfN accesses via MOutOfNEngine::RELIABILITY_BIAS_FRACTION_THRESHOLD
    // when called with the mock engine type.
    auto toExcess = [](double bias_fraction) -> double {
        return std::max(0.0, bias_fraction
                           - MockMOutOfNEngine::RELIABILITY_BIAS_FRACTION_THRESHOLD);
    };

    SECTION("Mild bias produces small haircut (bias_fraction=0.23)")
    {
        const double bias_frac      = 0.23;
        const double excess         = toExcess(bias_frac);
        const double original_lower = 0.30;
        const double haircut        = expectedHaircut(excess);
        const double expected_lower = original_lower - std::abs(original_lower) * haircut;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, original_lower, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, false, true, excess)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::MOutOfN);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(expected_lower).margin(1e-10));
        // Mild bias → haircut should be small (< 1%)
        REQUIRE(haircut < 0.01);
    }

    SECTION("Moderate bias produces proportionate haircut (bias_fraction=0.49)")
    {
        const double bias_frac      = 0.49;
        const double excess         = toExcess(bias_frac);
        const double original_lower = 0.30;
        const double haircut        = expectedHaircut(excess);
        const double expected_lower = original_lower - std::abs(original_lower) * haircut;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, original_lower, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, false, true, excess)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(expected_lower).margin(1e-10));
        // Moderate bias → haircut should be ~2.9%
        REQUIRE(haircut == Catch::Approx(0.029).margin(1e-6));
    }

    SECTION("Large bias produces proportionate haircut (bias_fraction=0.77)")
    {
        const double bias_frac      = 0.77;
        const double excess         = toExcess(bias_frac);
        const double original_lower = 0.30;
        const double haircut        = expectedHaircut(excess);
        const double expected_lower = original_lower - std::abs(original_lower) * haircut;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, original_lower, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, false, true, excess)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(expected_lower).margin(1e-10));
        // Large bias → haircut should be ~5.7%
        REQUIRE(haircut == Catch::Approx(0.057).margin(1e-6));
    }

    SECTION("Extreme bias is capped at kMOutOfNMaxHaircutFraction (bias_fraction=4.98)")
    {
        const double bias_frac      = 4.98;
        const double excess         = toExcess(bias_frac);
        const double original_lower = 0.30;
        const double haircut        = expectedHaircut(excess);
        const double expected_lower = original_lower - std::abs(original_lower) * haircut;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, original_lower, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, false, true, excess)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(expected_lower).margin(1e-10));
        // Extreme bias → haircut is capped at kMOutOfNMaxHaircutFraction
        REQUIRE(haircut == Catch::Approx(
            AutoBootstrapConfiguration::kMOutOfNMaxHaircutFraction).margin(1e-10));
    }

    SECTION("Haircut scales monotonically with excess_bias up to cap")
    {
        // Use raw bias_fractions from observed logs; convert to excess for the helper
        const std::vector<double> fractions = {0.25, 0.40, 0.60, 0.80, 1.50, 4.98};
        double prev_haircut = -1.0;
        for (double frac : fractions)
        {
            const double h = expectedHaircut(toExcess(frac));
            REQUIRE(h >= prev_haircut);  // non-decreasing
            REQUIRE(h <= AutoBootstrapConfiguration::kMOutOfNMaxHaircutFraction);
            prev_haircut = h;
        }
        // Fractions above threshold produce positive haircut
        REQUIRE(expectedHaircut(toExcess(0.21)) > 0.0);
        // Fractions at or below threshold produce zero haircut
        REQUIRE(expectedHaircut(toExcess(0.20)) == Catch::Approx(0.0).margin(1e-10));
        REQUIRE(expectedHaircut(toExcess(0.10)) == Catch::Approx(0.0).margin(1e-10));
    }

    SECTION("Lower bound NOT reduced when MOutOfN wins reliably (excess_bias=0)")
    {
        const double original_lower = 0.10;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, original_lower, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, true, false, 0.0)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(original_lower).margin(1e-10));
    }

    SECTION("Lower bound NOT reduced when ratio_near_boundary fires but not excessive_bias")
    {
        const double original_lower = 0.10;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, original_lower, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0,
                AutoBootstrapConfiguration::kMOutOfNUnreliabilityPenalty,
                0.0, 0.0, 0.0,
                true, false, false, 0.0)  // excessive_bias=false, excess_bias=0
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(original_lower).margin(1e-10));
    }

    SECTION("Upper bound and mean are unchanged by haircut")
    {
        const double original_mean  = 0.50;
        const double original_upper = 0.60;
        const double excess         = toExcess(0.49);

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, original_mean, 0.10, original_upper, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, false, true, excess)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(num::to_double(result.getChosenCandidate().getMean())
                == Catch::Approx(original_mean).margin(1e-10));
        REQUIRE(num::to_double(result.getChosenCandidate().getUpper())
                == Catch::Approx(original_upper).margin(1e-10));
    }

    SECTION("Haircut correct for negative lower bound (bias_fraction=0.77)")
    {
        const double bias_frac      = 0.77;
        const double excess         = toExcess(bias_frac);
        const double original_lower = -0.05;
        const double haircut        = expectedHaircut(excess);
        const double expected_lower = original_lower - std::abs(original_lower) * haircut;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.10, original_lower, 0.30, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.10, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, false, true, excess)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        const double actual_lower =
            num::to_double(result.getChosenCandidate().getLower());

        REQUIRE(actual_lower < original_lower);  // more negative = more conservative
        REQUIRE(actual_lower == Catch::Approx(expected_lower).margin(1e-10));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §9  select(): hard gate disqualifies MOutOfN, other method wins
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("select: hard gate disqualifies MOutOfN and other method wins",
          "[AutoBootstrapSelector][Reliability][HardGate][Integration]")
{
    ScoringWeights weights;
    StatisticSupport unbounded = StatisticSupport::unbounded();

    SECTION("Percentile wins when MOutOfN has infinite stability penalty")
    {
        // MOutOfN with infinity stability penalty (simulating hard gate).
        // Percentile with zero penalties should win.
        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, 0.40, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0,
                std::numeric_limits<double>::infinity(), // hard gate penalty
                0.0, 0.0, 0.0,
                true, false, false),
            makeTestCandidate(
                MethodId::Percentile, 0.50, 0.42, 0.58, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.2, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, true, false)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }

    SECTION("Throws when all candidates have hard gate penalties")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, 0.40, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0,
                std::numeric_limits<double>::infinity(),
                0.0, 0.0, 0.0, true, false, false)
        };

        REQUIRE_THROWS(Selector::select(candidates, weights, unbounded));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §10  select(): soft penalty down-weights MOutOfN but does not eliminate it
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("select: soft penalty down-weights MOutOfN but preserves it as fallback",
          "[AutoBootstrapSelector][Reliability][SoftGate][Integration]")
{
    ScoringWeights weights;
    StatisticSupport unbounded = StatisticSupport::unbounded();

    SECTION("Percentile wins when MOutOfN has soft penalty and Percentile is clean")
    {
        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, 0.40, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0,
                AutoBootstrapConfiguration::kMOutOfNUnreliabilityPenalty,
                0.0, 0.0, 0.0,
                true, false, true, 0.29),  // excess_bias=0.49-0.20=0.29
            makeTestCandidate(
                MethodId::Percentile, 0.50, 0.42, 0.58, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.2, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, true, false, 0.0)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
    }

    SECTION("MOutOfN wins when it is the only candidate despite soft penalty")
    {
        // Soft penalty: finite, so MOutOfN is not disqualified.
        // It is the only candidate, so it wins and gets the adaptive haircut.
        // bias_fraction=0.49 → excess=0.49-0.20=0.29 → haircut=2.9%
        const double excess_bias    = 0.29;
        const double original_lower = 0.40;
        const double scale          = AutoBootstrapConfiguration::kMOutOfNHaircutScale;
        const double cap            = AutoBootstrapConfiguration::kMOutOfNMaxHaircutFraction;
        const double haircut        = std::min(excess_bias * scale, cap);
        const double expected_lower = original_lower - std::abs(original_lower) * haircut;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, original_lower, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0,
                AutoBootstrapConfiguration::kMOutOfNUnreliabilityPenalty,
                0.0, 0.0, 0.0,
                true, false, true, excess_bias)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::MOutOfN);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(expected_lower).margin(1e-10));
    }

    SECTION("Haircut not applied when Percentile wins instead of MOutOfN")
    {
        const double percentile_lower = 0.42;

        std::vector<Candidate> candidates = {
            makeTestCandidate(
                MethodId::MOutOfN, 0.50, 0.40, 0.60, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.3, 0.50, 0.1, 1.0,
                0.0, 0.0,
                AutoBootstrapConfiguration::kMOutOfNUnreliabilityPenalty,
                0.0, 0.0, 0.0, true, false, true, 0.29),
            makeTestCandidate(
                MethodId::Percentile, 0.50, percentile_lower, 0.58, 0.95,
                30, 800, 0, 780, 20,
                0.05, 0.2, 0.50, 0.1, 1.0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                true, true, false, 0.0)
        };

        auto result = Selector::select(candidates, weights, unbounded);
        REQUIRE(result.getChosenCandidate().getMethod() == MethodId::Percentile);
        REQUIRE(num::to_double(result.getChosenCandidate().getLower())
                == Catch::Approx(percentile_lower).margin(1e-10));
    }
}
