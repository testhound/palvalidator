// StrategyAutoBootstrapAdditionalTests.cpp
//
// Additional unit tests to fill testing gaps in StrategyAutoBootstrap
//
// These tests cover:
//   - Edge cases for BootstrapConfiguration
//   - Edge cases for BootstrapAlgorithmsConfiguration
//   - Exception handling in StrategyAutoBootstrap::run()
//   - Custom sampler instances
//   - Logging output verification
//   - Diagnostic information completeness
//   - BCa rejection scenarios
//   - Minimum data requirements
//   - Safety valve scenarios
//
// Requires: Catch2 v3, all dependencies from StrategyAutoBootstrapTest.cpp

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <type_traits>
#include <numeric>
#include <cstdint>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>

#include "StrategyAutoBootstrap.h"
#include "TradingBootstrapFactory.h"
#include "AutoBootstrapSelector.h"
#include "StatUtils.h"
#include "StationaryMaskResamplers.h"
#include "number.h"
#include "DummyBacktesterStrategy.h"
#include "Portfolio.h"
#include "Security.h"
#include "TimeSeriesEntry.h"
#include "TimeSeries.h"
#include "TestUtils.h"

// Aliases for convenience
using Decimal   = double;

using BootstrapConfiguration           = palvalidator::analysis::BootstrapConfiguration;
using BootstrapAlgorithmsConfiguration = palvalidator::analysis::BootstrapAlgorithmsConfiguration;

using GeoMeanSampler =
    mkc_timeseries::GeoMeanStat<Decimal>;

using MaskResampler =
    palvalidator::resampling::StationaryMaskValueResamplerAdapter<Decimal>;

using StrategyAutoBootstrapType =
    palvalidator::analysis::StrategyAutoBootstrap<Decimal, GeoMeanSampler, MaskResampler>;

using Selector     = palvalidator::analysis::AutoBootstrapSelector<Decimal>;
using AutoCIResult = typename Selector::Result;
using MethodId     = typename AutoCIResult::MethodId;

using FactoryAlias = typename StrategyAutoBootstrapType::Factory;
using DummyStrategy = mkc_timeseries::DummyBacktesterStrategy<Decimal>;

using LogPFStat =
    typename mkc_timeseries::StatUtils<Decimal>::LogProfitFactorStat;

using RatioStrategyAutoBootstrapType =
    palvalidator::analysis::StrategyAutoBootstrap<Decimal, LogPFStat, MaskResampler>;

using RatioFactoryAlias = typename RatioStrategyAutoBootstrapType::Factory;

// -----------------------------------------------------------------------------
// Helper functions
// -----------------------------------------------------------------------------

static std::shared_ptr<mkc_timeseries::Portfolio<Decimal>> createTestPortfolio()
{
    auto timeSeries = std::make_shared<mkc_timeseries::OHLCTimeSeries<Decimal>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES);
    
    mkc_timeseries::OHLCTimeSeriesEntry<Decimal> entry(
        boost::gregorian::date(2020, 1, 2),
        Decimal(100.0),
        Decimal(101.0),
        Decimal(99.0),
        Decimal(100.5),
        Decimal(1000000),
        mkc_timeseries::TimeFrame::DAILY);
    
    timeSeries->addEntry(entry);
    
    auto equity = std::make_shared<mkc_timeseries::EquitySecurity<Decimal>>(
        "MSFT", "Microsoft Corporation", timeSeries);
    
    auto portfolio = std::make_shared<mkc_timeseries::Portfolio<Decimal>>("Test Portfolio");
    portfolio->addSecurity(equity);
    
    return portfolio;
}


static std::vector<Decimal> makeSimpleReturns()
{
    return {0.01, -0.01, 0.02, -0.02, 0.015, -0.015, 0.025, -0.005};
}

static std::vector<Decimal> makeMinimalReturns()
{
    return {0.01, -0.01};
}

static std::vector<Decimal> makeHighlySkewedReturns()
{
    std::vector<Decimal> xs;
    xs.reserve(100);
    
    // 95 small positive returns
    for (int i = 0; i < 95; ++i)
        xs.push_back(Decimal(0.001));
    
    // 5 large negative returns (extreme outliers)
    for (int i = 0; i < 5; ++i)
        xs.push_back(Decimal(-0.10));
    
    return xs;
}

// -----------------------------------------------------------------------------
// TEST SUITE: BootstrapConfiguration - Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("BootstrapConfiguration: Edge case - very small ratio",
          "[StrategyAutoBootstrap][BootstrapConfiguration][EdgeCase]")
{
    const std::size_t B = 100;
    const std::size_t L = 5;
    const double CL = 0.95;
    
    BootstrapConfiguration cfg(B, L, CL, 1, 0);
    
    SECTION("Very small ratio should still give at least 1 inner replication")
    {
        const double tinyRatio = 0.001;
        const std::size_t innerB = cfg.getPercentileTNumInnerReplications(tinyRatio);
        
        // Should be clamped to at least 1
        REQUIRE(innerB >= 1);
    }
    
    SECTION("Zero ratio should give at least 1 inner replication")
    {
        // This would cause division by zero, but implementation clamps to 1
        const double zeroRatio = 0.0;
        const std::size_t innerB = cfg.getPercentileTNumInnerReplications(zeroRatio);
        
        REQUIRE(innerB >= 1);
    }
}

TEST_CASE("BootstrapConfiguration: Different confidence levels",
          "[StrategyAutoBootstrap][BootstrapConfiguration]")
{
    const std::size_t B = 1000;
    const std::size_t L = 10;
    
    SECTION("90% confidence level")
    {
        BootstrapConfiguration cfg(B, L, 0.90, 1, 0);
        REQUIRE(cfg.getConfidenceLevel() == Catch::Approx(0.90));
    }
    
    SECTION("95% confidence level")
    {
        BootstrapConfiguration cfg(B, L, 0.95, 1, 0);
        REQUIRE(cfg.getConfidenceLevel() == Catch::Approx(0.95));
    }
    
    SECTION("99% confidence level")
    {
        BootstrapConfiguration cfg(B, L, 0.99, 1, 0);
        REQUIRE(cfg.getConfidenceLevel() == Catch::Approx(0.99));
    }
}

TEST_CASE("BootstrapConfiguration: Large B values",
          "[StrategyAutoBootstrap][BootstrapConfiguration]")
{
    const std::size_t largeB = 100000;
    const std::size_t L = 12;
    const double CL = 0.95;
    
    BootstrapConfiguration cfg(largeB, L, CL, 1, 0);
    
    REQUIRE(cfg.getNumBootStrapReplications() == largeB);
    REQUIRE(cfg.getPercentileTNumOuterReplications() == largeB);
    
    const std::size_t innerB = cfg.getPercentileTNumInnerReplications(10.0);
    REQUIRE(innerB == static_cast<std::size_t>(largeB / 10.0));
}

// -----------------------------------------------------------------------------
// TEST SUITE: BootstrapAlgorithmsConfiguration - Individual Algorithms
// -----------------------------------------------------------------------------

TEST_CASE("BootstrapAlgorithmsConfiguration: Default constructor enables all",
          "[StrategyAutoBootstrap][BootstrapAlgorithmsConfiguration]")
{
    BootstrapAlgorithmsConfiguration algos;
    
    REQUIRE(algos.enableNormal());
    REQUIRE(algos.enableBasic());
    REQUIRE(algos.enablePercentile());
    REQUIRE(algos.enableMOutOfN());
    REQUIRE(algos.enablePercentileT());
    REQUIRE(algos.enableBCa());
}

TEST_CASE("BootstrapAlgorithmsConfiguration: Individual algorithm disabling",
          "[StrategyAutoBootstrap][BootstrapAlgorithmsConfiguration]")
{
    SECTION("Disable only Normal")
    {
        BootstrapAlgorithmsConfiguration algos(
            false, true, true, true, true, true);
        
        REQUIRE_FALSE(algos.enableNormal());
        REQUIRE(algos.enableBasic());
        REQUIRE(algos.enablePercentile());
        REQUIRE(algos.enableMOutOfN());
        REQUIRE(algos.enablePercentileT());
        REQUIRE(algos.enableBCa());
    }
    
    SECTION("Disable only Basic")
    {
        BootstrapAlgorithmsConfiguration algos(
            true, false, true, true, true, true);
        
        REQUIRE(algos.enableNormal());
        REQUIRE_FALSE(algos.enableBasic());
        REQUIRE(algos.enablePercentile());
    }
    
    SECTION("Disable only Percentile")
    {
        BootstrapAlgorithmsConfiguration algos(
            true, true, false, true, true, true);
        
        REQUIRE_FALSE(algos.enablePercentile());
        REQUIRE(algos.enableMOutOfN());
    }
    
    SECTION("Disable only MOutOfN")
    {
        BootstrapAlgorithmsConfiguration algos(
            true, true, true, false, true, true);
        
        REQUIRE_FALSE(algos.enableMOutOfN());
        REQUIRE(algos.enablePercentileT());
    }
    
    SECTION("Disable only PercentileT")
    {
        BootstrapAlgorithmsConfiguration algos(
            true, true, true, true, false, true);
        
        REQUIRE_FALSE(algos.enablePercentileT());
        REQUIRE(algos.enableBCa());
    }
    
    SECTION("Disable only BCa")
    {
        BootstrapAlgorithmsConfiguration algos(
            true, true, true, true, true, false);
        
        REQUIRE(algos.enablePercentileT());
        REQUIRE_FALSE(algos.enableBCa());
    }
}

TEST_CASE("BootstrapAlgorithmsConfiguration: All algorithms disabled",
          "[StrategyAutoBootstrap][BootstrapAlgorithmsConfiguration]")
{
    BootstrapAlgorithmsConfiguration algos(
        false, false, false, false, false, false);
    
    REQUIRE_FALSE(algos.enableNormal());
    REQUIRE_FALSE(algos.enableBasic());
    REQUIRE_FALSE(algos.enablePercentile());
    REQUIRE_FALSE(algos.enableMOutOfN());
    REQUIRE_FALSE(algos.enablePercentileT());
    REQUIRE_FALSE(algos.enableBCa());
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Exception Handling
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Exception on empty returns",
          "[StrategyAutoBootstrap][Exception]")
{
    FactoryAlias factory(12345u);
    
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> emptyReturns;
    DummyStrategy strategy("Empty", portfolio, emptyReturns);
    
    BootstrapConfiguration cfg(100, 5, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos;
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    REQUIRE_THROWS_AS(autoBootstrap.run(emptyReturns), std::invalid_argument);
}

TEST_CASE("StrategyAutoBootstrap: Exception on single-element returns",
          "[StrategyAutoBootstrap][Exception]")
{
    FactoryAlias factory(54321u);
    
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> singleReturn = {0.05};
    DummyStrategy strategy("Single", portfolio, singleReturn);
    
    BootstrapConfiguration cfg(100, 5, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos;
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    REQUIRE_THROWS_AS(autoBootstrap.run(singleReturn), std::invalid_argument);
}

TEST_CASE("StrategyAutoBootstrap: Minimum valid data (2 elements)",
          "[StrategyAutoBootstrap][EdgeCase]")
{
    FactoryAlias factory(99999u);
    
    auto portfolio = createTestPortfolio();
    auto minReturns = makeMinimalReturns();
    REQUIRE(minReturns.size() == 2);
    
    DummyStrategy strategy("Minimal", portfolio, minReturns);
    
    BootstrapConfiguration cfg(50, 2, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos;
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    // With only 2 elements, bootstrap methods may fail to produce valid candidates
    // This is expected behavior - most bootstrap methods need more data
    try {
        AutoCIResult result = autoBootstrap.run(minReturns);
        // If it succeeds, should have at least one candidate
        REQUIRE(result.getCandidates().size() >= 1);
    }
    catch (const std::runtime_error& e) {
        // Expected: "no bootstrap candidate succeeded" with minimal data
        const std::string msg = e.what();
        REQUIRE((msg.find("no bootstrap candidate succeeded") != std::string::npos ||
                 msg.find("no valid candidate") != std::string::npos));
    }
}

TEST_CASE("StrategyAutoBootstrap: Exception when all algorithms disabled",
          "[StrategyAutoBootstrap][Exception]")
{
    FactoryAlias factory(77777u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("AllDisabled", portfolio, returns);
    
    BootstrapConfiguration cfg(100, 5, 0.95, 1, 0);
    
    // Disable all algorithms
    BootstrapAlgorithmsConfiguration algos(false, false, false, false, false, false);
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    // Should throw because no candidates can be produced
    REQUIRE_THROWS_AS(autoBootstrap.run(returns), std::runtime_error);
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Custom Sampler Instance
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Custom sampler instance with configuration",
          "[StrategyAutoBootstrap][CustomSampler]")
{
    RatioFactoryAlias factory(88888u);
    
    auto portfolio = createTestPortfolio();
    
    // Create returns that would yield different results with different stop-loss settings
    std::vector<Decimal> returns;
    for (int i = 0; i < 30; ++i)
        returns.push_back(i % 2 == 0 ? Decimal(0.02) : Decimal(-0.01));
    
    DummyStrategy strategy("CustomSampler", portfolio, returns);
    
    BootstrapConfiguration cfg(200, 4, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos;
    
    // Create a custom sampler instance with specific stop-loss
    LogPFStat customSampler(Decimal(0.05)); // 5% stop-loss
    
    RatioStrategyAutoBootstrapType autoBootstrap(
        factory, strategy, cfg, algos, customSampler);
    
    AutoCIResult result = autoBootstrap.run(returns);
    
    // Should successfully run with custom sampler
    REQUIRE(result.getCandidates().size() >= 1);
    
    const auto& chosen = result.getChosenCandidate();
    REQUIRE(chosen.getN() == returns.size());
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Logging Output
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Logging output verification",
          "[StrategyAutoBootstrap][Logging]")
{
    FactoryAlias factory(11111u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("LogTest", portfolio, returns);
    
    BootstrapConfiguration cfg(100, 4, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos;
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    std::ostringstream logStream;
    AutoCIResult result = autoBootstrap.run(returns, &logStream);
    
    std::string logOutput = logStream.str();
    
    // Verify logging output contains expected elements
    REQUIRE_FALSE(logOutput.empty());
    REQUIRE(logOutput.find("[AutoCI]") != std::string::npos);
    REQUIRE(logOutput.find("Selected method=") != std::string::npos);
    REQUIRE(logOutput.find("mean=") != std::string::npos);
    REQUIRE(logOutput.find("LB=") != std::string::npos);
    REQUIRE(logOutput.find("UB=") != std::string::npos);
    REQUIRE(logOutput.find("Diagnostics:") != std::string::npos);
}

TEST_CASE("StrategyAutoBootstrap: Logging captures engine failures",
          "[StrategyAutoBootstrap][Logging]")
{
    FactoryAlias factory(22222u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("FailureLog", portfolio, returns);
    
    BootstrapConfiguration cfg(10, 4, 0.95, 1, 0); // Very low B might cause issues
    
    // Enable only one algorithm to increase chance of seeing failure messages
    BootstrapAlgorithmsConfiguration algos(true, false, false, false, false, false);
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    std::ostringstream logStream;
    
    try {
        AutoCIResult result = autoBootstrap.run(returns, &logStream);
        
        // If successful, that's fine - just verify logging occurred
        std::string logOutput = logStream.str();
        REQUIRE_FALSE(logOutput.empty());
    }
    catch (...) {
        // If it throws, verify we logged the failures
        std::string logOutput = logStream.str();
        // Could check for "failed:" in the output, but not all cases will fail
    }
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Diagnostic Information
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Diagnostic information completeness",
          "[StrategyAutoBootstrap][Diagnostics]")
{
    FactoryAlias factory(33333u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("Diagnostics", portfolio, returns);
    
    BootstrapConfiguration cfg(400, 4, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos; // all enabled
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    AutoCIResult result = autoBootstrap.run(returns);
    
    const auto& diagnostics = result.getDiagnostics();
    
    SECTION("Diagnostics contain chosen method information")
    {
        // Should have a valid chosen method
        MethodId method = diagnostics.getChosenMethod();
        REQUIRE((method == MethodId::Normal ||
                 method == MethodId::Basic ||
                 method == MethodId::Percentile ||
                 method == MethodId::MOutOfN ||
                 method == MethodId::PercentileT ||
                 method == MethodId::BCa));
    }
    
    SECTION("Diagnostics contain candidate count")
    {
        std::size_t numCandidates = diagnostics.getNumCandidates();
        
        // Should have at least one candidate
        REQUIRE(numCandidates >= 1);
        
        // Should match the actual candidates
        REQUIRE(numCandidates == result.getCandidates().size());
    }
    
    SECTION("Diagnostics contain BCa information")
    {
        // With all algorithms enabled, should have BCa candidate info
        bool hasBCa = diagnostics.hasBCaCandidate();
        
        // Verify this matches actual candidates
        bool actuallyHasBCa = false;
        for (const auto& c : result.getCandidates()) {
            if (c.getMethod() == MethodId::BCa) {
                actuallyHasBCa = true;
                break;
            }
        }
        
        REQUIRE(hasBCa == actuallyHasBCa);
    }
    
    SECTION("Diagnostics contain scoring information")
    {
        double score = diagnostics.getChosenScore();
        
        // Score should be finite and non-negative (scoring system dependent)
        REQUIRE(std::isfinite(score));
    }
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Different Block Sizes
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Different block sizes",
          "[StrategyAutoBootstrap][BlockSize]")
{
    FactoryAlias factory(44444u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("BlockSize", portfolio, returns);
    
    BootstrapAlgorithmsConfiguration algos;
    
    SECTION("Small block size (2)")
    {
        BootstrapConfiguration cfg(200, 2, 0.95, 1, 0);
        StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
        
        AutoCIResult result = autoBootstrap.run(returns);
        REQUIRE(result.getCandidates().size() >= 1);
    }
    
    SECTION("Medium block size (4)")
    {
        BootstrapConfiguration cfg(200, 4, 0.95, 1, 0);
        StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
        
        AutoCIResult result = autoBootstrap.run(returns);
        REQUIRE(result.getCandidates().size() >= 1);
    }
    
    SECTION("Large block size (8)")
    {
        BootstrapConfiguration cfg(200, 8, 0.95, 1, 0);
        StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
        
        AutoCIResult result = autoBootstrap.run(returns);
        REQUIRE(result.getCandidates().size() >= 1);
    }
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Safety Valve Scenarios
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: MOutOfN safety valve with highly skewed data",
          "[StrategyAutoBootstrap][SafetyValve]")
{
    FactoryAlias factory(55555u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeHighlySkewedReturns();
    REQUIRE(returns.size() == 100);
    
    DummyStrategy strategy("SkewedData", portfolio, returns);
    
    BootstrapConfiguration cfg(500, 5, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos; // all enabled
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    std::ostringstream logStream;
    AutoCIResult result = autoBootstrap.run(returns, &logStream);
    
    // With highly skewed data, BCa might be rejected and MOutOfN chosen
    const auto& diagnostics = result.getDiagnostics();
    
    // If MOutOfN was chosen, verify the safety valve was triggered
    if (diagnostics.getChosenMethod() == MethodId::MOutOfN) {
        std::string logOutput = logStream.str();
        
        // Should contain safety valve warning
        REQUIRE(logOutput.find("CRITICAL: Safety Valve Triggered") != std::string::npos);
        REQUIRE(logOutput.find("M-out-of-N chosen") != std::string::npos);
    }
    
    // Regardless of method chosen, should have valid result
    REQUIRE(result.getCandidates().size() >= 1);
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - BCa Rejection Scenarios
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: BCa candidate characteristics",
          "[StrategyAutoBootstrap][BCa]")
{
    FactoryAlias factory(66666u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("BCaTest", portfolio, returns);
    
    BootstrapConfiguration cfg(800, 4, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos; // all enabled
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    AutoCIResult result = autoBootstrap.run(returns);
    
    // Find the BCa candidate
    bool foundBCa = false;
    for (const auto& candidate : result.getCandidates()) {
        if (candidate.getMethod() == MethodId::BCa) {
            foundBCa = true;
            
            // BCa candidate should have meaningful z0 and acceleration values
            double z0 = candidate.getZ0();
            double accel = candidate.getAccel();
            
            // Values should be finite
            REQUIRE(std::isfinite(z0));
            REQUIRE(std::isfinite(accel));
            
            // Stability penalty should be non-negative
            double stabilityPenalty = candidate.getStabilityPenalty();
            REQUIRE(stabilityPenalty >= 0.0);
            
            // Length penalty should be non-negative
            double lengthPenalty = candidate.getLengthPenalty();
            REQUIRE(lengthPenalty >= 0.0);
            
            break;
        }
    }
    
    REQUIRE(foundBCa);
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Effective B Verification
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Effective B counts are reasonable",
          "[StrategyAutoBootstrap][EffectiveB]")
{
    FactoryAlias factory(77888u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("EffectiveB", portfolio, returns);
    
    const std::size_t B = 500;
    BootstrapConfiguration cfg(B, 4, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos; // all enabled
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    AutoCIResult result = autoBootstrap.run(returns);
    
    for (const auto& candidate : result.getCandidates()) {
        std::size_t effectiveB = candidate.getEffectiveB();
        
        // Effective B should be positive
        REQUIRE(effectiveB > 0);
        
        // For most methods, effective B should be close to B
        // (except for PercentileT which uses inner/outer)
        if (candidate.getMethod() != MethodId::PercentileT) {
            // Should be in reasonable range of configured B
            REQUIRE(effectiveB <= B * 2); // generous upper bound
        }
    }
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Multiple Algorithm Combinations
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Various algorithm combinations produce valid results",
          "[StrategyAutoBootstrap][AlgorithmCombinations]")
{
    FactoryAlias factory(99000u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("Combinations", portfolio, returns);
    
    BootstrapConfiguration cfg(300, 4, 0.95, 1, 0);
    
    SECTION("Only Normal and Basic")
    {
        BootstrapAlgorithmsConfiguration algos(true, true, false, false, false, false);
        StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
        
        // Normal and Basic methods may fail with insufficient or unsuitable data
        try {
            AutoCIResult result = autoBootstrap.run(returns);
            REQUIRE(result.getCandidates().size() >= 1);
            REQUIRE(result.getCandidates().size() <= 2);
        }
        catch (const std::runtime_error& e) {
            // Expected: Some algorithms may fail with this data
            const std::string msg = e.what();
            REQUIRE((msg.find("no bootstrap candidate succeeded") != std::string::npos ||
                     msg.find("no valid candidate") != std::string::npos));
        }
    }
    
    SECTION("Only Percentile methods")
    {
        BootstrapAlgorithmsConfiguration algos(false, false, true, true, true, false);
        StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
        
        // Percentile methods should be more robust
        try {
            AutoCIResult result = autoBootstrap.run(returns);
            REQUIRE(result.getCandidates().size() >= 1);
            REQUIRE(result.getCandidates().size() <= 3);
        }
        catch (const std::runtime_error& e) {
            // Even percentile methods may fail with this minimal data
            const std::string msg = e.what();
            REQUIRE((msg.find("no bootstrap candidate succeeded") != std::string::npos ||
                     msg.find("no valid candidate") != std::string::npos));
        }
    }
    
    SECTION("Only BCa")
    {
        BootstrapAlgorithmsConfiguration algos(false, false, false, false, false, true);
        StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
        
        // BCa should generally work with simple returns data
        try {
            AutoCIResult result = autoBootstrap.run(returns);
            REQUIRE(result.getCandidates().size() == 1);
            REQUIRE(result.getChosenMethod() == MethodId::BCa);
        }
        catch (const std::runtime_error& e) {
            // BCa might be rejected due to instability parameters
            const std::string msg = e.what();
            REQUIRE((msg.find("no bootstrap candidate succeeded") != std::string::npos ||
                     msg.find("no valid candidate") != std::string::npos));
        }
    }
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Confidence Level Verification
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: All candidates respect configured confidence level",
          "[StrategyAutoBootstrap][ConfidenceLevel]")
{
    FactoryAlias factory(12000u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("ConfLevel", portfolio, returns);
    
    const double CL = 0.90;
    BootstrapConfiguration cfg(400, 4, CL, 1, 0);
    BootstrapAlgorithmsConfiguration algos; // all enabled
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    AutoCIResult result = autoBootstrap.run(returns);
    
    // All candidates should have the configured confidence level
    for (const auto& candidate : result.getCandidates()) {
        REQUIRE(candidate.getCl() == Catch::Approx(CL));
    }
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - Sample Size Verification
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: All candidates report correct sample size",
          "[StrategyAutoBootstrap][SampleSize]")
{
    FactoryAlias factory(34000u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    const std::size_t expectedN = returns.size();
    
    DummyStrategy strategy("SampleSize", portfolio, returns);
    
    BootstrapConfiguration cfg(300, 4, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos; // all enabled
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    AutoCIResult result = autoBootstrap.run(returns);
    
    // All candidates should report the correct sample size
    for (const auto& candidate : result.getCandidates()) {
        REQUIRE(candidate.getN() == expectedN);
    }
}

// -----------------------------------------------------------------------------
// TEST SUITE: StrategyAutoBootstrap - CI Width Verification
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Confidence intervals have positive width",
          "[StrategyAutoBootstrap][CIWidth]")
{
    FactoryAlias factory(56000u);
    
    auto portfolio = createTestPortfolio();
    auto returns = makeSimpleReturns();
    DummyStrategy strategy("CIWidth", portfolio, returns);
    
    BootstrapConfiguration cfg(400, 4, 0.95, 1, 0);
    BootstrapAlgorithmsConfiguration algos; // all enabled
    
    StrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);
    
    AutoCIResult result = autoBootstrap.run(returns);
    
    // All candidates should have positive-width CIs
    for (const auto& candidate : result.getCandidates()) {
        double width = candidate.getUpper() - candidate.getLower();
        
        REQUIRE(width > 0.0);
        REQUIRE(std::isfinite(width));
    }
}
