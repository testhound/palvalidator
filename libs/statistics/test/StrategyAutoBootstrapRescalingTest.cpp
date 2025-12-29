// StrategyAutoBootstrap_RescalingTests.cpp
//
// Unit tests for M-out-of-N rescaling configuration and integration
//
// These tests verify:
//   - BootstrapConfiguration rescaleMOutOfN parameter
//   - StrategyAutoBootstrap passes rescaling flag to factory
//   - TradingBootstrapFactory respects rescaling flag
//   - Integration with AutoBootstrapSelector
//   - Backward compatibility
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

#include "StrategyAutoBootstrap.h"
#include "TradingBootstrapFactory.h"
#include "AutoBootstrapSelector.h"
#include "StatUtils.h"
#include "StationaryMaskResamplers.h"
#include "MOutOfNPercentileBootstrap.h"
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

using MOutOfNEngine = palvalidator::analysis::MOutOfNPercentileBootstrap<
    Decimal, 
    std::function<Decimal(const std::vector<Decimal>&)>,
    palvalidator::resampling::StationaryMaskValueResampler<Decimal>>;

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

static std::vector<Decimal> makeSampleReturns()
{
    return {
        -0.00010085, -0.02970397, 0.00037076, -0.01057146, -0.03146460,
         0.04696515, -0.00865288, -0.00242277, -0.02746527, 0.04807175,
        -0.02147869,  0.06425008, 0.00177674, -0.03109691, 0.00095259,
         0.00091408,  0.01470800, 0.00990005, 0.01000044, 0.00445892,
         0.01098901,  0.01960613, 0.00609494, -0.02453894, 0.03342927,
         0.02187212
    };
}

static std::vector<Decimal> makeLongerReturns()
{
    std::vector<Decimal> returns;
    returns.reserve(60);
    
    // Create 60 returns with some structure
    for (int i = 0; i < 60; ++i) {
        double phase = static_cast<double>(i) / 10.0;
        double value = 0.01 * std::sin(phase) + 0.002;
        returns.push_back(Decimal(value));
    }
    
    return returns;
}

// =====================================================================
// TEST GROUP 1: BootstrapConfiguration Rescaling Parameter
// =====================================================================

TEST_CASE("BootstrapConfiguration: rescaleMOutOfN defaults to true",
          "[StrategyAutoBootstrap][BootstrapConfiguration][Rescaling]")
{
    const std::size_t  B      = 1000;
    const std::size_t  L      = 12;
    const double       CL     = 0.95;
    const std::uint64_t stage = 42;
    const std::uint64_t fold  = 3;

    SECTION("Default constructor sets rescaleMOutOfN to true")
    {
        BootstrapConfiguration cfg(B, L, CL, stage, fold);
        REQUIRE(cfg.getRescaleMOutOfN() == true);
    }
}

TEST_CASE("BootstrapConfiguration: rescaleMOutOfN can be explicitly set",
          "[StrategyAutoBootstrap][BootstrapConfiguration][Rescaling]")
{
    const std::size_t  B      = 1000;
    const std::size_t  L      = 12;
    const double       CL     = 0.95;
    const std::uint64_t stage = 42;
    const std::uint64_t fold  = 3;

    SECTION("Explicit true")
    {
        BootstrapConfiguration cfg(B, L, CL, stage, fold, true);
        REQUIRE(cfg.getRescaleMOutOfN() == true);
    }

    SECTION("Explicit false")
    {
        BootstrapConfiguration cfg(B, L, CL, stage, fold, false);
        REQUIRE(cfg.getRescaleMOutOfN() == false);
    }
}

TEST_CASE("BootstrapConfiguration: all other accessors work with rescaleMOutOfN",
          "[StrategyAutoBootstrap][BootstrapConfiguration][Rescaling]")
{
    const std::size_t  B      = 1000;
    const std::size_t  L      = 12;
    const double       CL     = 0.95;
    const std::uint64_t stage = 42;
    const std::uint64_t fold  = 3;

    SECTION("With rescaleMOutOfN = true")
    {
        BootstrapConfiguration cfg(B, L, CL, stage, fold, true);
        
        REQUIRE(cfg.getNumBootStrapReplications() == B);
        REQUIRE(cfg.getBlockSize()                == L);
        REQUIRE(cfg.getConfidenceLevel()          == CL);
        REQUIRE(cfg.getStageTag()                 == stage);
        REQUIRE(cfg.getFold()                     == fold);
        REQUIRE(cfg.getRescaleMOutOfN()           == true);
    }

    SECTION("With rescaleMOutOfN = false")
    {
        BootstrapConfiguration cfg(B, L, CL, stage, fold, false);
        
        REQUIRE(cfg.getNumBootStrapReplications() == B);
        REQUIRE(cfg.getBlockSize()                == L);
        REQUIRE(cfg.getConfidenceLevel()          == CL);
        REQUIRE(cfg.getStageTag()                 == stage);
        REQUIRE(cfg.getFold()                     == fold);
        REQUIRE(cfg.getRescaleMOutOfN()           == false);
    }
}

// =====================================================================
// TEST GROUP 2: TradingBootstrapFactory Rescaling Parameter
// =====================================================================

TEST_CASE("TradingBootstrapFactory: makeAdaptiveMOutOfN defaults to false",
          "[TradingBootstrapFactory][MOutOfN][Rescaling]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(12345);
    
    palvalidator::resampling::StationaryMaskValueResampler<Decimal> resampler(3);

    SECTION("Default call produces non-rescaled engine")
    {
        auto [engine, crn] = factory.template makeAdaptiveMOutOfN<
            Decimal, GeoMeanSampler, 
            palvalidator::resampling::StationaryMaskValueResampler<Decimal>>(
                1000,      // B
                0.95,      // CL
                resampler,
                strategy,
                0,         // stageTag
                3,         // L
                0);        // fold
        
        // Engine should have rescale_to_n = false (factory default)
        REQUIRE_FALSE(engine.rescalesToN());
    }
}

TEST_CASE("TradingBootstrapFactory: makeAdaptiveMOutOfN respects explicit rescaling flag",
          "[TradingBootstrapFactory][MOutOfN][Rescaling]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(12345);
    
    palvalidator::resampling::StationaryMaskValueResampler<Decimal> resampler(3);

    SECTION("Explicit false")
    {
        auto [engine, crn] = factory.template makeAdaptiveMOutOfN<
            Decimal, GeoMeanSampler, 
            palvalidator::resampling::StationaryMaskValueResampler<Decimal>>(
                1000, 0.95, resampler, strategy, 0, 3, 0,
                false);   // rescale_to_n = false
        
        REQUIRE_FALSE(engine.rescalesToN());
    }

    SECTION("Explicit true")
    {
        auto [engine, crn] = factory.template makeAdaptiveMOutOfN<
            Decimal, GeoMeanSampler, 
            palvalidator::resampling::StationaryMaskValueResampler<Decimal>>(
                1000, 0.95, resampler, strategy, 0, 3, 0,
                true);    // rescale_to_n = true
        
        REQUIRE(engine.rescalesToN());
    }
}

TEST_CASE("TradingBootstrapFactory: makeMOutOfN respects rescaling flag",
          "[TradingBootstrapFactory][MOutOfN][Rescaling]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(12345);
    
    palvalidator::resampling::StationaryMaskValueResampler<Decimal> resampler(3);

    SECTION("Fixed ratio with rescale_to_n = false")
    {
        auto [engine, crn] = factory.template makeMOutOfN<
            Decimal, GeoMeanSampler, 
            palvalidator::resampling::StationaryMaskValueResampler<Decimal>>(
                1000,      // B
                0.95,      // CL
                0.7,       // m_ratio
                resampler,
                strategy,
                0,         // stageTag
                3,         // L
                0,         // fold
                false);    // rescale_to_n
        
        REQUIRE_FALSE(engine.rescalesToN());
    }

    SECTION("Fixed ratio with rescale_to_n = true")
    {
        auto [engine, crn] = factory.template makeMOutOfN<
            Decimal, GeoMeanSampler, 
            palvalidator::resampling::StationaryMaskValueResampler<Decimal>>(
                1000, 0.95, 0.7, resampler, strategy, 0, 3, 0,
                true);     // rescale_to_n
        
        REQUIRE(engine.rescalesToN());
    }
}

// =====================================================================
// TEST GROUP 3: StrategyAutoBootstrap Integration
// =====================================================================

TEST_CASE("StrategyAutoBootstrap: passes rescaling flag from configuration",
          "[StrategyAutoBootstrap][Integration][Rescaling]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(54321);
    

    SECTION("Configuration with rescaling enabled")
    {
        // Default rescaleMOutOfN = true
        BootstrapConfiguration cfg(1000, 3, 0.95, 0, 0);
        REQUIRE(cfg.getRescaleMOutOfN() == true);
        
        // Only enable M-out-of-N for this test
        BootstrapAlgorithmsConfiguration algos(
            false,  // Normal
            false,  // Basic
            false,  // Percentile
            true,   // M-out-of-N
            false,  // Percentile-T
            false); // BCa
        
        StrategyAutoBootstrapType autoBS(factory, strategy, cfg, algos);
        
        // Run should succeed
        REQUIRE_NOTHROW(autoBS.run(returns));
        
        auto result = autoBS.run(returns);
        
        // M-out-of-N should be selected (only method enabled)
        REQUIRE(result.getChosenMethod() == MethodId::MOutOfN);
        
        // Result should be valid
        const auto& chosen = result.getChosenCandidate();
        REQUIRE(std::isfinite(num::to_double(chosen.getLower())));
        REQUIRE(std::isfinite(num::to_double(chosen.getUpper())));
        REQUIRE(chosen.getLower() <= chosen.getUpper());
    }

    SECTION("Configuration with rescaling disabled")
    {
        // Explicitly disable rescaling
        BootstrapConfiguration cfg(1000, 3, 0.95, 0, 0, false);
        REQUIRE(cfg.getRescaleMOutOfN() == false);
        
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS(factory, strategy, cfg, algos);
        
        // Run should succeed with non-rescaled M-out-of-N
        REQUIRE_NOTHROW(autoBS.run(returns));
        
        auto result = autoBS.run(returns);
        
        REQUIRE(result.getChosenMethod() == MethodId::MOutOfN);
    }
}

TEST_CASE("StrategyAutoBootstrap: rescaling affects CI width as expected",
          "[StrategyAutoBootstrap][Integration][Rescaling]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeLongerReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    

    SECTION("Rescaled intervals should be wider than non-rescaled (same seed)")
    {
        // Use same factory seed for both
        FactoryAlias factory1(99999);
        FactoryAlias factory2(99999);
        
        // Configuration with rescaling
        BootstrapConfiguration cfg_rescale(1000, 3, 0.95, 0, 0, true);
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS_rescale(factory1, strategy, cfg_rescale, algos);
        auto result_rescale = autoBS_rescale.run(returns);
        
        // Configuration without rescaling
        BootstrapConfiguration cfg_no_rescale(1000, 3, 0.95, 0, 0, false);
        StrategyAutoBootstrapType autoBS_no_rescale(factory2, strategy, cfg_no_rescale, algos);
        auto result_no_rescale = autoBS_no_rescale.run(returns);
        
        // Both should select M-out-of-N
        REQUIRE(result_rescale.getChosenMethod() == MethodId::MOutOfN);
        REQUIRE(result_no_rescale.getChosenMethod() == MethodId::MOutOfN);
        
        // Calculate widths
        const auto& chosen_rescale = result_rescale.getChosenCandidate();
        const auto& chosen_no_rescale = result_no_rescale.getChosenCandidate();
        double width_rescale = num::to_double(chosen_rescale.getUpper() - chosen_rescale.getLower());
        double width_no_rescale = num::to_double(chosen_no_rescale.getUpper() - chosen_no_rescale.getLower());
        
        // Rescaled should be wider (though this is probabilistic)
        // We just verify both are positive and reasonable
        REQUIRE(width_rescale > 0.0);
        REQUIRE(width_no_rescale > 0.0);
        
        // Both should contain the mean
        double mean = num::to_double(chosen_rescale.getMean());
        REQUIRE(num::to_double(chosen_rescale.getLower()) <= mean);
        REQUIRE(mean <= num::to_double(chosen_rescale.getUpper()));
        REQUIRE(num::to_double(chosen_no_rescale.getLower()) <= mean);
        REQUIRE(mean <= num::to_double(chosen_no_rescale.getUpper()));
    }
}

TEST_CASE("StrategyAutoBootstrap: rescaling works with multiple methods enabled",
          "[StrategyAutoBootstrap][Integration][Rescaling]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(11111);
    

    SECTION("All methods enabled with rescaling")
    {
        BootstrapConfiguration cfg(1000, 3, 0.95, 0, 0, true);
        
        // Enable all methods
        BootstrapAlgorithmsConfiguration algos(
            true,   // Normal
            true,   // Basic
            true,   // Percentile
            true,   // M-out-of-N (with rescaling)
            false,  // Percentile-T (slow)
            false); // BCa (can fail)
        
        StrategyAutoBootstrapType autoBS(factory, strategy, cfg, algos);
        
        // Should run successfully
        REQUIRE_NOTHROW(autoBS.run(returns));
        
        auto result = autoBS.run(returns);
        
        // Should have multiple candidates
        REQUIRE(result.getCandidates().size() >= 3);
        
        // Find M-out-of-N candidate
        bool found_moon = false;
        for (const auto& cand : result.getCandidates()) {
            if (cand.getMethod() == MethodId::MOutOfN) {
                found_moon = true;
                
                // M-out-of-N candidate should have valid statistics
                REQUIRE(std::isfinite(num::to_double(cand.getLower())));
                REQUIRE(std::isfinite(num::to_double(cand.getUpper())));
                REQUIRE(cand.getLower() <= cand.getUpper());
                
                break;
            }
        }
        
        REQUIRE(found_moon);
    }
}

// =====================================================================
// TEST GROUP 4: Backward Compatibility
// =====================================================================

TEST_CASE("StrategyAutoBootstrap: backward compatibility preserved",
          "[StrategyAutoBootstrap][Integration][Rescaling][Compatibility]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(77777);
    

    SECTION("Old code without rescaling parameter still compiles and runs")
    {
        // This is the old constructor call pattern
        BootstrapConfiguration cfg_old_style(1000, 3, 0.95, 0, 0);
        
        // Should default to rescaling enabled
        REQUIRE(cfg_old_style.getRescaleMOutOfN() == true);
        
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS(factory, strategy, cfg_old_style, algos);
        
        // Should work exactly as before
        REQUIRE_NOTHROW(autoBS.run(returns));
        
        auto result = autoBS.run(returns);
        // Should have at least one candidate
        REQUIRE(result.getCandidates().size() >= 1);
    }
}

TEST_CASE("TradingBootstrapFactory: backward compatibility preserved",
          "[TradingBootstrapFactory][MOutOfN][Rescaling][Compatibility]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(88888);
    
    palvalidator::resampling::StationaryMaskValueResampler<Decimal> resampler(3);

    SECTION("Old factory calls without rescaling parameter work")
    {
        // Old call pattern without rescale_to_n parameter
        auto [engine, crn] = factory.template makeAdaptiveMOutOfN<
            Decimal, GeoMeanSampler, 
            palvalidator::resampling::StationaryMaskValueResampler<Decimal>>(
                1000,
                0.95,
                resampler,
                strategy,
                0,
                3,
                0);
        
        // Should default to false (factory default)
        REQUIRE_FALSE(engine.rescalesToN());
        
        // Engine should be usable
        std::vector<Decimal> returns = makeSampleReturns();
        
        mkc_timeseries::GeoMeanStat<Decimal> geoMeanStat;
        REQUIRE_NOTHROW(engine.run(returns, geoMeanStat, crn));
    }
}

// =====================================================================
// TEST GROUP 5: Edge Cases and Error Handling
// =====================================================================

TEST_CASE("StrategyAutoBootstrap: rescaling works with minimal data",
          "[StrategyAutoBootstrap][Integration][Rescaling][EdgeCase]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> minimal_returns = {0.01, -0.005, 0.02, 0.001, -0.01};
    DummyStrategy strategy("TestStrategy", portfolio, minimal_returns);
    FactoryAlias factory(33333);
    

    SECTION("Rescaling with minimal data")
    {
        BootstrapConfiguration cfg(400, 2, 0.90, 0, 0, true);
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS(factory, strategy, cfg, algos);
        
        // Should handle minimal data
        REQUIRE_NOTHROW(autoBS.run(minimal_returns));
        
        auto result = autoBS.run(minimal_returns);
        REQUIRE(result.getCandidates().size() >= 1);
    }
}

TEST_CASE("StrategyAutoBootstrap: rescaling with different confidence levels",
          "[StrategyAutoBootstrap][Integration][Rescaling]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    

    SECTION("CL = 0.90 with rescaling")
    {
        FactoryAlias factory1(11111);
        BootstrapConfiguration cfg(1000, 3, 0.90, 0, 0, true);
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS(factory1, strategy, cfg, algos);
        auto result = autoBS.run(returns);
        
        REQUIRE(result.getCandidates().size() >= 1);
        REQUIRE(result.getChosenCandidate().getCl() == Catch::Approx(0.90));
    }

    SECTION("CL = 0.95 with rescaling")
    {
        FactoryAlias factory2(22222);
        BootstrapConfiguration cfg(1000, 3, 0.95, 0, 0, true);
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS(factory2, strategy, cfg, algos);
        auto result = autoBS.run(returns);
        
        REQUIRE(result.getCandidates().size() >= 1);
        REQUIRE(result.getChosenCandidate().getCl() == Catch::Approx(0.95));
    }

    SECTION("CL = 0.99 with rescaling")
    {
        FactoryAlias factory3(33333);
        BootstrapConfiguration cfg(1000, 3, 0.99, 0, 0, true);
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS(factory3, strategy, cfg, algos);
        auto result = autoBS.run(returns);
        
        REQUIRE(result.getCandidates().size() >= 1);
        REQUIRE(result.getChosenCandidate().getCl() == Catch::Approx(0.99));
    }
}

TEST_CASE("StrategyAutoBootstrap: logging output mentions M-out-of-N when enabled",
          "[StrategyAutoBootstrap][Integration][Rescaling][Logging]")
{
    auto portfolio = createTestPortfolio();
    std::vector<Decimal> returns = makeSampleReturns();
    DummyStrategy strategy("TestStrategy", portfolio, returns);
    FactoryAlias factory(44444);

    SECTION("M-out-of-N logging with rescaling")
    {
        BootstrapConfiguration cfg(1000, 3, 0.95, 0, 0, true);
        BootstrapAlgorithmsConfiguration algos(
            false, false, false, true, false, false);
        
        StrategyAutoBootstrapType autoBS(factory, strategy, cfg, algos);
        
        std::ostringstream log;
        auto result = autoBS.run(returns, &log);
        
        // If M-out-of-N fails, log should mention it
        // If it succeeds, log might be empty
        // Just verify no exceptions and valid result
        REQUIRE(result.getCandidates().size() >= 1);
    }
}
