// AutoBootstrapSelectorNewTests.cpp
//
// Additional unit tests for AutoBootstrapSelector methods:
//  - summarizePercentileLike
//  - summarizePercentileT
//
// These tests complement the existing AutoBootstrapSelectorTest.cpp
//
// Requires:
//  - Catch2 v3
//  - AutoBootstrapSelector.h
//  - Mock bootstrap engine classes for testing

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>
#include <limits>
#include <cmath>
#include <numeric>

#include "AutoBootstrapSelector.h"
#include "number.h"

// Alias for convenience
using Decimal        = double;
using Selector       = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using Candidate      = Selector::Candidate;
using Result         = Selector::Result;
using ScoringWeights = Selector::ScoringWeights;
using MethodId       = Result::MethodId;

// =============================================================================
// MOCK BOOTSTRAP ENGINE CLASSES
// =============================================================================
// These mock classes provide the minimum interface required by the 
// summarizePercentileLike and summarizePercentileT methods for testing.
// =============================================================================

/**
 * @brief Mock engine for testing summarizePercentileLike.
 * 
 * This mock simulates the interface of percentile-like bootstrap engines
 * (e.g., Percentile, Basic, M-out-of-N, Normal methods).
 */
class MockPercentileLikeEngine
{
public:
    struct Result
    {
        Decimal mean;
        Decimal lower;
        Decimal upper;
        double cl;
        std::size_t n;
        std::size_t B;
        std::size_t effective_B;
        std::size_t skipped;
    };

    MockPercentileLikeEngine() : m_has_diagnostics(false) {}

    // Configure the mock with test data
    void setBootstrapStatistics(const std::vector<double>& stats)
    {
        m_bootstrap_stats = stats;
        m_has_diagnostics = true;
        
        // Compute mean and SE for the mock
        double sum = 0.0;
        for (double v : stats) sum += v;
        m_bootstrap_mean = sum / static_cast<double>(stats.size());
        
        double sum_sq = 0.0;
        for (double v : stats)
        {
            double diff = v - m_bootstrap_mean;
            sum_sq += diff * diff;
        }
        m_bootstrap_se = std::sqrt(sum_sq / static_cast<double>(stats.size() - 1));
    }

    void setResult(const Result& res)
    {
        m_result = res;
    }

    // Required interface methods
    bool hasDiagnostics() const { return m_has_diagnostics; }
    const std::vector<double>& getBootstrapStatistics() const { return m_bootstrap_stats; }
    double getBootstrapMean() const { return m_bootstrap_mean; }
    double getBootstrapSe() const { return m_bootstrap_se; }
    const Result& getResult() const { return m_result; }

private:
    bool m_has_diagnostics;
    std::vector<double> m_bootstrap_stats;
    double m_bootstrap_mean;
    double m_bootstrap_se;
    Result m_result;
};

/**
 * @brief Mock engine for testing summarizePercentileT.
 * 
 * This mock simulates the interface of the Percentile-T bootstrap engine,
 * which requires additional fields for double-bootstrap diagnostics.
 */
class MockPercentileTEngine
{
public:
    struct Result
    {
        Decimal mean;
        Decimal lower;
        Decimal upper;
        double cl;
        std::size_t n;
        std::size_t B_outer;
        std::size_t B_inner;
        std::size_t effective_B;
        std::size_t skipped_outer;
        std::size_t skipped_inner_total;
        std::size_t inner_attempted_total;
        double se_hat;
    };

    MockPercentileTEngine() : m_has_diagnostics(false) {}

    // Configure the mock with test data
    void setThetaStarStatistics(const std::vector<double>& stats)
    {
        m_theta_star_stats = stats;
        m_has_diagnostics = true;
    }

    void setResult(const Result& res)
    {
        m_result = res;
    }

    // Required interface methods
    bool hasDiagnostics() const { return m_has_diagnostics; }
    const std::vector<double>& getThetaStarStatistics() const { return m_theta_star_stats; }
    const Result& getResult() const { return m_result; }

private:
    bool m_has_diagnostics;
    std::vector<double> m_theta_star_stats;
    Result m_result;
};

// =============================================================================
// UNIT TESTS FOR summarizePercentileLike
// =============================================================================

TEST_CASE("summarizePercentileLike: Basic functionality with Percentile method",
          "[AutoBootstrapSelector][summarizePercentileLike][Basic]")
{
    MockPercentileLikeEngine engine;
    
    // Create a simple bootstrap distribution
    std::vector<double> boot_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
    engine.setBootstrapStatistics(boot_stats);
    
    MockPercentileLikeEngine::Result res;
    res.mean = 0.50;
    res.lower = 0.46;
    res.upper = 0.54;
    res.cl = 0.95;
    res.n = 100;
    res.B = 1000;
    res.effective_B = 990;
    res.skipped = 10;
    engine.setResult(res);
    
    SECTION("Successfully creates a Candidate for Percentile method")
    {
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        REQUIRE(c.getMethod() == MethodId::Percentile);
        REQUIRE(c.getMean() == res.mean);
        REQUIRE(c.getLower() == res.lower);
        REQUIRE(c.getUpper() == res.upper);
        REQUIRE(c.getCl() == res.cl);
        REQUIRE(c.getN() == res.n);
        REQUIRE(c.getBOuter() == res.B);
        REQUIRE(c.getBInner() == 0);  // N/A for percentile-like
        REQUIRE(c.getEffectiveB() == res.effective_B);
        REQUIRE(c.getSkippedTotal() == res.skipped);
    }
    
    SECTION("Computes bootstrap SE and skewness correctly")
    {
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        REQUIRE(c.getSeBoot() > 0.0);
        REQUIRE(std::isfinite(c.getSkewBoot()));
    }
    
    SECTION("Computes center shift penalty")
    {
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        // Center shift should be computed for Percentile method
        REQUIRE(std::isfinite(c.getCenterShiftInSe()));
    }
    
    SECTION("Computes ordering penalty for non-Basic methods")
    {
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        // Ordering penalty should be computed and non-negative
        REQUIRE(c.getOrderingPenalty() >= 0.0);
    }
    
    SECTION("Stability penalty is zero for percentile-like methods")
    {
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        // Percentile-like methods don't use stability penalty
        REQUIRE(c.getStabilityPenalty() == 0.0);
        REQUIRE(c.getZ0() == 0.0);
        REQUIRE(c.getAccel() == 0.0);
    }
}

TEST_CASE("summarizePercentileLike: Basic method skips ordering penalty",
          "[AutoBootstrapSelector][summarizePercentileLike][Basic]")
{
    MockPercentileLikeEngine engine;
    
    std::vector<double> boot_stats = {0.40, 0.45, 0.50, 0.55, 0.60};
    engine.setBootstrapStatistics(boot_stats);
    
    MockPercentileLikeEngine::Result res;
    res.mean = 0.50;
    res.lower = 0.42;
    res.upper = 0.58;
    res.cl = 0.95;
    res.n = 50;
    res.B = 500;
    res.effective_B = 495;
    res.skipped = 5;
    engine.setResult(res);
    
    SECTION("Basic method has zero ordering penalty")
    {
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Basic,
            engine,
            res
        );
        
        // Basic bootstrap uses reflection, so ordering penalty should be 0
        REQUIRE(c.getOrderingPenalty() == 0.0);
    }
}

TEST_CASE("summarizePercentileLike: Handles degenerate distributions",
          "[AutoBootstrapSelector][summarizePercentileLike][EdgeCases]")
{
    MockPercentileLikeEngine engine;
    
    SECTION("All bootstrap statistics identical (zero SE)")
    {
        // Degenerate case: all values are the same
        std::vector<double> boot_stats(100, 0.50);
        engine.setBootstrapStatistics(boot_stats);
        
        MockPercentileLikeEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.50;
        res.upper = 0.50;
        res.cl = 0.95;
        res.n = 100;
        res.B = 100;
        res.effective_B = 100;
        res.skipped = 0;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        // SE should be zero or near-zero
        REQUIRE(c.getSeBoot() == Catch::Approx(0.0).margin(1e-10));
        // Skewness should default to 0.0 for degenerate distribution
        REQUIRE(c.getSkewBoot() == 0.0);
    }
    
    SECTION("Negative interval length")
    {
        std::vector<double> boot_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
        engine.setBootstrapStatistics(boot_stats);
        
        MockPercentileLikeEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.60;  // Invalid: lower > upper
        res.upper = 0.40;
        res.cl = 0.95;
        res.n = 100;
        res.B = 1000;
        res.effective_B = 990;
        res.skipped = 10;
        engine.setResult(res);
        
        // Should not throw, but interval will be problematic
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        REQUIRE(c.getMethod() == MethodId::Percentile);
    }
}

TEST_CASE("summarizePercentileLike: Error handling",
          "[AutoBootstrapSelector][summarizePercentileLike][Errors]")
{
    MockPercentileLikeEngine engine;
    
    SECTION("Throws when diagnostics not available")
    {
        MockPercentileLikeEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.40;
        res.upper = 0.60;
        res.cl = 0.95;
        res.n = 100;
        res.B = 1000;
        res.effective_B = 990;
        res.skipped = 10;
        
        // Don't set diagnostics
        REQUIRE_THROWS_AS(
            Selector::summarizePercentileLike(MethodId::Percentile, engine, res),
            std::logic_error
        );
    }
    
    SECTION("Throws when insufficient bootstrap statistics")
    {
        // Only 1 statistic (need at least 2)
        std::vector<double> boot_stats = {0.50};
        engine.setBootstrapStatistics(boot_stats);
        
        MockPercentileLikeEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.40;
        res.upper = 0.60;
        res.cl = 0.95;
        res.n = 100;
        res.B = 1;
        res.effective_B = 1;
        res.skipped = 0;
        engine.setResult(res);
        
        REQUIRE_THROWS_AS(
            Selector::summarizePercentileLike(MethodId::Percentile, engine, res),
            std::logic_error
        );
    }
}

TEST_CASE("summarizePercentileLike: M-out-of-N method handling",
          "[AutoBootstrapSelector][summarizePercentileLike][MOutOfN]")
{
    MockPercentileLikeEngine engine;
    
    std::vector<double> boot_stats = {0.42, 0.46, 0.50, 0.54, 0.58};
    engine.setBootstrapStatistics(boot_stats);
    
    MockPercentileLikeEngine::Result res;
    res.mean = 0.50;
    res.lower = 0.44;
    res.upper = 0.56;
    res.cl = 0.95;
    res.n = 30;  // Small sample size appropriate for M-out-of-N
    res.B = 1000;
    res.effective_B = 980;
    res.skipped = 20;
    engine.setResult(res);
    
    SECTION("M-out-of-N method creates valid Candidate")
    {
        Candidate c = Selector::summarizePercentileLike(
            MethodId::MOutOfN,
            engine,
            res
        );
        
        REQUIRE(c.getMethod() == MethodId::MOutOfN);
        REQUIRE(c.getMean() == res.mean);
        // M-out-of-N uses different L_max in length penalty computation
        REQUIRE(std::isfinite(c.getLengthPenalty()));
    }
}

TEST_CASE("summarizePercentileLike: Skewed distributions",
          "[AutoBootstrapSelector][summarizePercentileLike][Skewness]")
{
    MockPercentileLikeEngine engine;
    
    SECTION("Positively skewed distribution")
    {
        // Right-skewed: long tail on the right
        std::vector<double> boot_stats = {
            0.40, 0.42, 0.44, 0.45, 0.46, 0.47, 0.48, 0.49,
            0.50, 0.52, 0.55, 0.60, 0.70, 0.80, 0.90
        };
        engine.setBootstrapStatistics(boot_stats);
        
        MockPercentileLikeEngine::Result res;
        res.mean = 0.55;
        res.lower = 0.43;
        res.upper = 0.75;
        res.cl = 0.95;
        res.n = 100;
        res.B = 1000;
        res.effective_B = 990;
        res.skipped = 10;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        // Should have positive skewness
        REQUIRE(c.getSkewBoot() > 0.0);
    }
    
    SECTION("Negatively skewed distribution")
    {
        // Left-skewed: long tail on the left
        std::vector<double> boot_stats = {
            0.10, 0.20, 0.30, 0.40, 0.45, 0.48, 0.49,
            0.50, 0.51, 0.52, 0.53, 0.54, 0.55, 0.56, 0.58
        };
        engine.setBootstrapStatistics(boot_stats);
        
        MockPercentileLikeEngine::Result res;
        res.mean = 0.45;
        res.lower = 0.25;
        res.upper = 0.57;
        res.cl = 0.95;
        res.n = 100;
        res.B = 1000;
        res.effective_B = 990;
        res.skipped = 10;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileLike(
            MethodId::Percentile,
            engine,
            res
        );
        
        // Should have negative skewness
        REQUIRE(c.getSkewBoot() < 0.0);
    }
}

// =============================================================================
// UNIT TESTS FOR summarizePercentileT
// =============================================================================

TEST_CASE("summarizePercentileT: Basic functionality",
          "[AutoBootstrapSelector][summarizePercentileT][Basic]")
{
    MockPercentileTEngine engine;
    
    // Create theta* statistics
    std::vector<double> theta_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
    engine.setThetaStarStatistics(theta_stats);
    
    MockPercentileTEngine::Result res;
    res.mean = 0.50;
    res.lower = 0.46;
    res.upper = 0.54;
    res.cl = 0.95;
    res.n = 100;
    res.B_outer = 1000;
    res.B_inner = 200;
    res.effective_B = 990;
    res.skipped_outer = 5;
    res.skipped_inner_total = 100;
    res.inner_attempted_total = 10000;  // Some inner loops stopped early
    res.se_hat = 0.05;
    engine.setResult(res);
    
    SECTION("Successfully creates a Candidate for PercentileT method")
    {
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        REQUIRE(c.getMethod() == MethodId::PercentileT);
        REQUIRE(c.getMean() == res.mean);
        REQUIRE(c.getLower() == res.lower);
        REQUIRE(c.getUpper() == res.upper);
        REQUIRE(c.getCl() == res.cl);
        REQUIRE(c.getN() == res.n);
        REQUIRE(c.getBOuter() == res.B_outer);
        REQUIRE(c.getBInner() == res.B_inner);
        REQUIRE(c.getEffectiveB() == res.effective_B);
        REQUIRE(c.getSkippedTotal() == res.skipped_outer + res.skipped_inner_total);
    }
    
    SECTION("Computes bootstrap SE correctly")
    {
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // Should use se_hat from result if valid
        REQUIRE(c.getSeBoot() > 0.0);
        REQUIRE(std::isfinite(c.getSkewBoot()));
    }
    
    SECTION("Center shift penalty is zero for PercentileT")
    {
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // PercentileT doesn't use center shift penalty
        REQUIRE(c.getCenterShiftInSe() == 0.0);
    }
    
    SECTION("Ordering penalty is zero for PercentileT")
    {
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // PercentileT currently doesn't compute ordering penalty
        REQUIRE(c.getOrderingPenalty() == 0.0);
    }
    
    SECTION("Computes stability penalty based on resample quality")
    {
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // Stability penalty should be computed and non-negative
        REQUIRE(c.getStabilityPenalty() >= 0.0);
    }
    
    SECTION("z0 and accel are N/A for PercentileT")
    {
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // BCa-specific parameters should be 0 for PercentileT
        REQUIRE(c.getZ0() == 0.0);
        REQUIRE(c.getAccel() == 0.0);
    }
    
    SECTION("Computes inner failure rate correctly")
    {
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        double expected_rate = static_cast<double>(res.skipped_inner_total) / 
                              static_cast<double>(res.inner_attempted_total);
        REQUIRE(c.getInnerFailureRate() == Catch::Approx(expected_rate));
    }
}

TEST_CASE("summarizePercentileT: Stability penalty scenarios",
          "[AutoBootstrapSelector][summarizePercentileT][Stability]")
{
    MockPercentileTEngine engine;
    
    std::vector<double> theta_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
    engine.setThetaStarStatistics(theta_stats);
    
    SECTION("Low failure rates produce low penalty")
    {
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 980;  // 98% effective
        res.skipped_outer = 5;   // 0.5% outer failure
        res.skipped_inner_total = 200;  // Need inner_attempted for rate
        res.inner_attempted_total = 20000;  // 1% inner failure rate
        res.se_hat = 0.05;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // All rates are below thresholds, penalty should be low or zero
        REQUIRE(c.getStabilityPenalty() < 0.1);
    }
    
    SECTION("High outer failure rate increases penalty")
    {
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 850;  // 85% effective
        res.skipped_outer = 150;  // 15% outer failure (above 10% threshold)
        res.skipped_inner_total = 200;
        res.inner_attempted_total = 20000;  // 1% inner failure
        res.se_hat = 0.05;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // High outer failure should increase penalty
        REQUIRE(c.getStabilityPenalty() > 0.1);
    }
    
    SECTION("High inner failure rate increases penalty")
    {
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 980;  // 98% effective
        res.skipped_outer = 5;  // 0.5% outer failure
        res.skipped_inner_total = 2000;
        res.inner_attempted_total = 20000;  // 10% inner failure (above 5% threshold)
        res.se_hat = 0.05;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // High inner failure should increase penalty
        REQUIRE(c.getStabilityPenalty() > 0.1);
    }
    
    SECTION("Low effective B increases penalty")
    {
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 600;  // Only 60% effective (below 70% threshold)
        res.skipped_outer = 400;  // Many failures
        res.skipped_inner_total = 200;
        res.inner_attempted_total = 20000;
        res.se_hat = 0.05;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // Low effective B should increase penalty
        REQUIRE(c.getStabilityPenalty() > 0.2);
    }
}

TEST_CASE("summarizePercentileT: Edge cases and error handling",
          "[AutoBootstrapSelector][summarizePercentileT][EdgeCases]")
{
    MockPercentileTEngine engine;
    
    SECTION("Throws when diagnostics not available")
    {
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 990;
        res.skipped_outer = 5;
        res.skipped_inner_total = 100;
        res.inner_attempted_total = 10000;
        res.se_hat = 0.05;
        
        // Don't set diagnostics
        REQUIRE_THROWS_AS(
            Selector::summarizePercentileT(engine, res),
            std::logic_error
        );
    }
    
    SECTION("Throws when insufficient theta* statistics")
    {
        std::vector<double> theta_stats = {0.50};  // Only 1 statistic
        engine.setThetaStarStatistics(theta_stats);
        
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1;
        res.B_inner = 200;
        res.effective_B = 1;
        res.skipped_outer = 0;
        res.skipped_inner_total = 0;
        res.inner_attempted_total = 100;
        res.se_hat = 0.05;
        engine.setResult(res);
        
        REQUIRE_THROWS_AS(
            Selector::summarizePercentileT(engine, res),
            std::logic_error
        );
    }
    
    SECTION("Handles invalid se_hat by falling back to calculated SE")
    {
        std::vector<double> theta_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
        engine.setThetaStarStatistics(theta_stats);
        
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 990;
        res.skipped_outer = 5;
        res.skipped_inner_total = 100;
        res.inner_attempted_total = 10000;
        res.se_hat = -1.0;  // Invalid SE
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // Should use calculated SE from theta_stats
        REQUIRE(c.getSeBoot() > 0.0);
    }
    
    SECTION("Handles zero inner_attempted_total")
    {
        std::vector<double> theta_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
        engine.setThetaStarStatistics(theta_stats);
        
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 990;
        res.skipped_outer = 5;
        res.skipped_inner_total = 0;
        res.inner_attempted_total = 0;  // No inner attempts
        res.se_hat = 0.05;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // Inner failure rate should be 0 when no attempts
        REQUIRE(c.getInnerFailureRate() == 0.0);
    }
    
    SECTION("Degenerate theta* distribution (all identical)")
    {
        std::vector<double> theta_stats(100, 0.50);  // All identical
        engine.setThetaStarStatistics(theta_stats);
        
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.50;
        res.upper = 0.50;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 100;
        res.B_inner = 200;
        res.effective_B = 100;
        res.skipped_outer = 0;
        res.skipped_inner_total = 0;
        res.inner_attempted_total = 1000;
        res.se_hat = 0.0;
        engine.setResult(res);
        
        Candidate c = Selector::summarizePercentileT(
            engine,
            res
        );
        
        // Skewness should default to 0 for degenerate distribution
        REQUIRE(c.getSkewBoot() == 0.0);
    }
}

TEST_CASE("summarizePercentileT: Logging output",
          "[AutoBootstrapSelector][summarizePercentileT][Logging]")
{
    MockPercentileTEngine engine;
    
    std::vector<double> theta_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
    engine.setThetaStarStatistics(theta_stats);
    
    SECTION("Logs when stability penalty is positive")
    {
        MockPercentileTEngine::Result res;
        res.mean = 0.50;
        res.lower = 0.46;
        res.upper = 0.54;
        res.cl = 0.95;
        res.n = 100;
        res.B_outer = 1000;
        res.B_inner = 200;
        res.effective_B = 600;  // Low effective B will produce penalty
        res.skipped_outer = 200;  // High failure rate
        res.skipped_inner_total = 1000;
        res.inner_attempted_total = 10000;  // 10% inner failure
        res.se_hat = 0.05;
        engine.setResult(res);
        
        std::ostringstream log;
        Candidate c = Selector::summarizePercentileT(
            engine,
            res,
            &log
        );
        
        // Should have logged something since penalty > 0
        std::string output = log.str();
        if (c.getStabilityPenalty() > 0.0)
        {
            REQUIRE_FALSE(output.empty());
            REQUIRE(output.find("stability penalty") != std::string::npos);
        }
    }
}

TEST_CASE("summarizePercentileT: Comparison with different quality levels",
          "[AutoBootstrapSelector][summarizePercentileT][Quality]")
{
    // Create two engines with different quality characteristics
    MockPercentileTEngine good_engine, poor_engine;
    
    std::vector<double> theta_stats = {0.45, 0.48, 0.50, 0.52, 0.55};
    good_engine.setThetaStarStatistics(theta_stats);
    poor_engine.setThetaStarStatistics(theta_stats);
    
    // Good quality result
    MockPercentileTEngine::Result good_res;
    good_res.mean = 0.50;
    good_res.lower = 0.46;
    good_res.upper = 0.54;
    good_res.cl = 0.95;
    good_res.n = 100;
    good_res.B_outer = 1000;
    good_res.B_inner = 200;
    good_res.effective_B = 980;  // 98% effective
    good_res.skipped_outer = 5;   // 0.5% outer failure
    good_res.skipped_inner_total = 200;
    good_res.inner_attempted_total = 20000;  // 1% inner failure
    good_res.se_hat = 0.05;
    good_engine.setResult(good_res);
    
    // Poor quality result
    MockPercentileTEngine::Result poor_res;
    poor_res.mean = 0.50;
    poor_res.lower = 0.46;
    poor_res.upper = 0.54;
    poor_res.cl = 0.95;
    poor_res.n = 100;
    poor_res.B_outer = 1000;
    poor_res.B_inner = 200;
    poor_res.effective_B = 600;  // Only 60% effective
    poor_res.skipped_outer = 200;  // 20% outer failure
    poor_res.skipped_inner_total = 2000;
    poor_res.inner_attempted_total = 20000;  // 10% inner failure
    poor_res.se_hat = 0.05;
    poor_engine.setResult(poor_res);
    
    Candidate good_candidate = Selector::summarizePercentileT(good_engine, good_res);
    Candidate poor_candidate = Selector::summarizePercentileT(poor_engine, poor_res);
    
    SECTION("Good quality has lower stability penalty than poor quality")
    {
        REQUIRE(good_candidate.getStabilityPenalty() < poor_candidate.getStabilityPenalty());
    }
    
    SECTION("Good quality has lower inner failure rate")
    {
        REQUIRE(good_candidate.getInnerFailureRate() < poor_candidate.getInnerFailureRate());
    }
}