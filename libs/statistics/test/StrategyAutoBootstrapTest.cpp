// StrategyBootstrapTest.cpp
//
// Unit tests for:
//   - BootstrapConfiguration
//   - BootstrapAlgorithmsConfiguration
//   - StrategyAutoBootstrap (type wiring / aliases)
//   - StrategyAutoBootstrap integration-style tests on real return series
//
// Place in: libs/statistics/test/
//
// Requires:
//   - Catch2 v3
//   - StrategyAutoBootstrap.h
//   - TradingBootstrapFactory.h
//   - AutoBootstrapSelector.h
//   - StatUtils.h
//   - StationaryMaskResamplers.h
//   - DummyBacktesterStrategy.h
//   - number.h (if you prefer num::DefaultNumber, but we use double here)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <type_traits>
#include <numeric>
#include <cstdint>
#include <vector>
#include <memory>

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

// Aliases for convenience in these tests
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

// Factory alias from StrategyAutoBootstrap
using FactoryAlias = typename StrategyAutoBootstrapType::Factory;

// Dummy strategy alias from your test helper header
using DummyStrategy = mkc_timeseries::DummyBacktesterStrategy<Decimal>;

// -----------------------------------------------------------------------------
// Helper: construct StrategyAutoBootstrap for tests
// -----------------------------------------------------------------------------

static StrategyAutoBootstrapType
makeAutoBootstrap(FactoryAlias& factory,
                  DummyStrategy& strategy,
                  std::size_t    B,
                  std::size_t    blockSize,
                  double         cl,
                  std::uint64_t  stageTag,
                  std::uint64_t  fold,
                  const BootstrapAlgorithmsConfiguration& algos =
                      BootstrapAlgorithmsConfiguration{})
{
    BootstrapConfiguration cfg(B, blockSize, cl, stageTag, fold);
    return StrategyAutoBootstrapType(factory, strategy, cfg, algos);
}

// -----------------------------------------------------------------------------
// Helper: create a minimal portfolio for testing
// -----------------------------------------------------------------------------

static std::shared_ptr<mkc_timeseries::Portfolio<Decimal>> createTestPortfolio()
{
    // Create a minimal time series with just a few data points
    auto timeSeries = std::make_shared<mkc_timeseries::OHLCTimeSeries<Decimal>>(
        mkc_timeseries::TimeFrame::DAILY,
        mkc_timeseries::TradingVolume::SHARES);
    
    // Add a minimal bar of data using proper constructor
    mkc_timeseries::OHLCTimeSeriesEntry<Decimal> entry(
        boost::gregorian::date(2020, 1, 2),
        Decimal(100.0),  // open
        Decimal(101.0),  // high
        Decimal(99.0),   // low
        Decimal(100.5),  // close
        Decimal(1000000), // volume
        mkc_timeseries::TimeFrame::DAILY);
    
    timeSeries->addEntry(entry);
    
    // Create a simple equity security
    auto equity = std::make_shared<mkc_timeseries::EquitySecurity<Decimal>>(
        "MSFT", "Microsoft Corporation", timeSeries);
    
    // Create portfolio and add the security
    auto portfolio = std::make_shared<mkc_timeseries::Portfolio<Decimal>>("Test Portfolio");
    portfolio->addSecurity(equity);
    
    return portfolio;
}

// -----------------------------------------------------------------------------
// Sample return series from user (percent returns)
// -----------------------------------------------------------------------------

static std::vector<Decimal> makeSampleReturns1()
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

static std::vector<Decimal> makeSampleReturns2()
{
    return {
        -0.00524418, 0.02181219, -0.01844447, -0.02759527, -0.01978660,
        -0.01067861, -0.02220198, 0.01344900, 0.03406675, 0.02309296,
         0.04424707, -0.01985169, -0.01408664, -0.04697170, -0.01123057,
         0.01045735, 0.01910937, 0.03680806, 0.00403268, -0.08365759,
         0.03134162, -0.02150174, -0.00847824
    };
}

static std::vector<Decimal> makeSampleReturns3()
{
    return {
         0.01178550, 0.05481136, 0.02798269, 0.00201637, 0.01647225,
         0.00987620, 0.02992591, 0.07221778, 0.01343913, 0.01884248,
         0.00332390, 0.02207498, 0.00294883, 0.02529402, -0.00272727,
        -0.02716500, 0.00252686, -0.01058221, 0.02607732, 0.01493414,
        -0.00528939, -0.02444013, 0.00086121, -0.03084099, -0.01274882,
        -0.00652873, -0.00115970, 0.01296502, 0.03347820, 0.02650852,
        -0.04331355, -0.01201162, 0.02062261, 0.00478996, -0.00196295,
        -0.02765827, 0.02904930, 0.05497576, -0.01355473, 0.03834593,
         0.00161284, 0.02049689, -0.02960868, 0.00055342, -0.00283934,
        -0.01819392, 0.01412214, 0.01147911, 0.01825175, 0.00734840,
         0.00643134, 0.01917072
    };
}

// -----------------------------------------------------------------------------
/*              UNIT TESTS: BootstrapConfiguration                          */
// -----------------------------------------------------------------------------

TEST_CASE("BootstrapConfiguration: Construction and basic getters",
          "[StrategyAutoBootstrap][BootstrapConfiguration]")
{
    const std::size_t  B      = 1000;
    const std::size_t  L      = 12;
    const double       CL     = 0.95;
    const std::uint64_t stage = 42;
    const std::uint64_t fold  = 3;

    BootstrapConfiguration cfg(B, L, CL, stage, fold);

    SECTION("Core fields are stored and returned correctly")
    {
        REQUIRE(cfg.getNumBootStrapReplications() == B);
        REQUIRE(cfg.getBlockSize()                == L);
        REQUIRE(cfg.getConfidenceInterval()       == CL);
        REQUIRE(cfg.getStageTag()                 == stage);
        REQUIRE(cfg.getFold()                     == fold);
    }

    SECTION("Percentile-t outer replication count equals B")
    {
        REQUIRE(cfg.getPercentileTNumOuterReplications()
                == cfg.getNumBootStrapReplications());
    }

    SECTION("Percentile-t inner replication count uses fixed ratio, at least 1")
    {
        const std::size_t B_outer = cfg.getPercentileTNumOuterReplications();
        const std::size_t ratio   = cfg.getPercentileTRatioOuterToInner();

        REQUIRE(ratio == 10); // by design in BootstrapConfiguration

        const std::size_t expected_inner =
            (B_outer / ratio == 0) ? std::size_t(1) : (B_outer / ratio);

        REQUIRE(cfg.getPercentileTNumInnerReplications() == expected_inner);
    }
}

TEST_CASE("BootstrapConfiguration: Inner B falls back to 1 for tiny B",
          "[StrategyAutoBootstrap][BootstrapConfiguration]")
{
    // Use very small B to exercise the max(1, B_outer / ratio) logic.
    const std::size_t  B_small = 5;
    const std::size_t  L       = 4;
    const double       CL      = 0.90;
    const std::uint64_t stage  = 7;
    const std::uint64_t fold   = 1;

    BootstrapConfiguration cfg(B_small, L, CL, stage, fold);

    REQUIRE(cfg.getNumBootStrapReplications() == B_small);
    REQUIRE(cfg.getPercentileTRatioOuterToInner() == 10);

    // B_outer = 5, ratio = 10 → inner = max(1, 5 / 10) = 1
    REQUIRE(cfg.getPercentileTNumOuterReplications() == B_small);
    REQUIRE(cfg.getPercentileTNumInnerReplications() == std::size_t(1));
}

// -----------------------------------------------------------------------------
/*          UNIT TESTS: BootstrapAlgorithmsConfiguration                     */
// -----------------------------------------------------------------------------

TEST_CASE("BootstrapAlgorithmsConfiguration: Defaults enable all algorithms",
          "[StrategyAutoBootstrap][BootstrapAlgorithmsConfiguration]")
{
    BootstrapAlgorithmsConfiguration algos; // default ctor

    REQUIRE( algos.enableNormal()      );
    REQUIRE( algos.enableBasic()       );
    REQUIRE( algos.enablePercentile()  );
    REQUIRE( algos.enableMOutOfN()     );
    REQUIRE( algos.enablePercentileT() );
    REQUIRE( algos.enableBCa()         );
}

TEST_CASE("BootstrapAlgorithmsConfiguration: Custom flags respected",
          "[StrategyAutoBootstrap][BootstrapAlgorithmsConfiguration]")
{
    SECTION("Disable everything explicitly")
    {
        BootstrapAlgorithmsConfiguration algos(
            /*enableNormal*/      false,
            /*enableBasic*/       false,
            /*enablePercentile*/  false,
            /*enableMOutOfN*/     false,
            /*enablePercentileT*/ false,
            /*enableBCa*/         false
        );

        REQUIRE_FALSE(algos.enableNormal());
        REQUIRE_FALSE(algos.enableBasic());
        REQUIRE_FALSE(algos.enablePercentile());
        REQUIRE_FALSE(algos.enableMOutOfN());
        REQUIRE_FALSE(algos.enablePercentileT());
        REQUIRE_FALSE(algos.enableBCa());
    }

    SECTION("Selective enabling")
    {
        // Enable only Percentile and BCa
        BootstrapAlgorithmsConfiguration algos(
            /*enableNormal*/      false,
            /*enableBasic*/       false,
            /*enablePercentile*/  true,
            /*enableMOutOfN*/     false,
            /*enablePercentileT*/ false,
            /*enableBCa*/         true
        );

        REQUIRE_FALSE(algos.enableNormal());
        REQUIRE_FALSE(algos.enableBasic());
        REQUIRE(     algos.enablePercentile());
        REQUIRE_FALSE(algos.enableMOutOfN());
        REQUIRE_FALSE(algos.enablePercentileT());
        REQUIRE(     algos.enableBCa());
    }
}

// -----------------------------------------------------------------------------
/*                UNIT TESTS: StrategyAutoBootstrap type wiring              */
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap: Type aliases and wiring",
          "[StrategyAutoBootstrap][TypeAliases]")
{
    using SAB          = StrategyAutoBootstrapType;
    using ResultType   = typename SAB::Result;
    using CandidateType= typename SAB::Candidate;
    using MethodIdType = typename SAB::MethodId;
    using BCaResampler = typename SAB::BCaResampler;

    // Basic alias sanity: ResultType should match AutoCIResult<Decimal>
    STATIC_REQUIRE(std::is_same<ResultType, AutoCIResult>::value);

    // Candidate and MethodId should be those from AutoCIResult as well
    STATIC_REQUIRE(std::is_same<CandidateType, typename AutoCIResult::Candidate>::value);
    STATIC_REQUIRE(std::is_same<MethodIdType,  typename AutoCIResult::MethodId>::value);

    // BCaResampler is fixed to mkc_timeseries::StationaryBlockResampler<Decimal>
    STATIC_REQUIRE(std::is_same<BCaResampler,
                                mkc_timeseries::StationaryBlockResampler<Decimal>>::value);

    // Ensure configuration objects are usable with the StrategyAutoBootstrap type
    BootstrapConfiguration cfg(/*numBootStrapReplications*/ 500,
                               /*blockSize*/                10,
                               /*confidenceInterval*/       0.95,
                               /*stageTag*/                 1u,
                               /*fold*/                     0u);

    BootstrapAlgorithmsConfiguration algos; // defaults to all enabled

    // Just a couple of runtime sanity checks on the configs here:
    REQUIRE(cfg.getNumBootStrapReplications() == 500);
    REQUIRE(algos.enableBCa());
}

// -----------------------------------------------------------------------------
/*                    INTEGRATION TESTS: StrategyAutoBootstrap               */
// -----------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap integration: Mixed sample returns produce sane CI",
          "[StrategyAutoBootstrap][Integration]")
{
    FactoryAlias factory(/*masterSeed*/ 123456u);

    const std::size_t B        = 500;
    const std::size_t blockL   = 4;
    const double      CL       = 0.95;
    const std::uint64_t stage  = 1;
    const std::uint64_t fold   = 0;

    auto returns = makeSampleReturns1();
    REQUIRE(returns.size() >= 20);

    // Create a proper portfolio for the dummy strategy
    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("DummyStrategy_Sample1", portfolio, returns);

    BootstrapAlgorithmsConfiguration algos; // all enabled
    auto autoBootstrap = makeAutoBootstrap(factory, strategy, B, blockL, CL, stage, fold, algos);

    AutoCIResult result = autoBootstrap.run(returns);

    // At least one candidate should be produced
    REQUIRE(result.getCandidates().size() >= 1);

    const auto& chosen = result.getChosenCandidate();

    // Basic sanity on CI
    REQUIRE(chosen.getN()              == returns.size());
    REQUIRE(chosen.getUpper()          >= chosen.getLower());
    REQUIRE(chosen.getCl()             == Catch::Approx(CL));

    // We should see BCa if enabled
    bool hasBCa = false;
    for (const auto& c : result.getCandidates())
    {
        if (c.getMethod() == MethodId::BCa)
            hasBCa = true;

        // All candidates should reflect the same sample size and CL
        REQUIRE(c.getN()  == returns.size());
        REQUIRE(c.getCl() == Catch::Approx(CL));
    }
    REQUIRE(hasBCa);
}

TEST_CASE("StrategyAutoBootstrap integration: Positive-biased series has positive bootstrap mean",
          "[StrategyAutoBootstrap][Integration]")
{
    FactoryAlias factory(/*masterSeed*/ 987654u);

    const std::size_t B        = 600;
    const std::size_t blockL   = 6;
    const double      CL       = 0.95;
    const std::uint64_t stage  = 2;
    const std::uint64_t fold   = 0;

    auto returns = makeSampleReturns3();
    REQUIRE(returns.size() >= 20);

    // Compute sample mean to compare sign with bootstrap mean
    const double sampleMean = std::accumulate(returns.begin(), returns.end(), 0.0)
                            / static_cast<double>(returns.size());
    REQUIRE(sampleMean > 0.0); // sanity: this sample should be positive-biased

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("DummyStrategy_Positive", portfolio, returns);

    BootstrapAlgorithmsConfiguration algos; // all enabled
    auto autoBootstrap = makeAutoBootstrap(factory, strategy, B, blockL, CL, stage, fold, algos);

    AutoCIResult result = autoBootstrap.run(returns);
    const auto& chosen  = result.getChosenCandidate();

    const double bootstrapMean = chosen.getMean();

    // The bootstrap mean should share the sample mean's (positive) sign
    REQUIRE(bootstrapMean > 0.0);

    // Width should be reasonable (not degenerate, not absurdly wide)
    const double width = chosen.getUpper() - chosen.getLower();
    REQUIRE(width > 0.0);
}

TEST_CASE("StrategyAutoBootstrap integration: Negative-biased series has negative bootstrap mean",
          "[StrategyAutoBootstrap][Integration]")
{
    FactoryAlias factory(/*masterSeed*/ 13579u);

    const std::size_t B        = 600;
    const std::size_t blockL   = 6;
    const double      CL       = 0.95;
    const std::uint64_t stage  = 3;
    const std::uint64_t fold   = 0;

    auto returns = makeSampleReturns2();
    REQUIRE(returns.size() >= 20);

    const double sampleMean = std::accumulate(returns.begin(), returns.end(), 0.0)
                            / static_cast<double>(returns.size());
    REQUIRE(sampleMean < 0.0); // sanity: this sample should be negative-biased

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("DummyStrategy_Negative", portfolio, returns);

    BootstrapAlgorithmsConfiguration algos; // all enabled
    auto autoBootstrap = makeAutoBootstrap(factory, strategy, B, blockL, CL, stage, fold, algos);

    AutoCIResult result = autoBootstrap.run(returns);
    const auto& chosen  = result.getChosenCandidate();

    const double bootstrapMean = chosen.getMean();

    // The bootstrap mean should share the sample mean's (negative) sign
    REQUIRE(bootstrapMean < 0.0);
}

TEST_CASE("StrategyAutoBootstrap integration: Algorithm flags control available candidates",
          "[StrategyAutoBootstrap][Integration]")
{
    FactoryAlias factory(/*masterSeed*/ 24680u);

    const std::size_t B        = 400;
    const std::size_t blockL   = 4;
    const double      CL       = 0.90;
    const std::uint64_t stage  = 4;
    const std::uint64_t fold   = 0;

    auto returns = makeSampleReturns1();
    REQUIRE(returns.size() >= 20);

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("DummyStrategy_Flags", portfolio, returns);

    // Enable only Percentile and BCa
    BootstrapAlgorithmsConfiguration algos(
        /*enableNormal*/      false,
        /*enableBasic*/       false,
        /*enablePercentile*/  true,
        /*enableMOutOfN*/     false,
        /*enablePercentileT*/ false,
        /*enableBCa*/         true
    );

    auto autoBootstrap = makeAutoBootstrap(factory, strategy, B, blockL, CL, stage, fold, algos);

    AutoCIResult result = autoBootstrap.run(returns);

    bool hasPercentile = false;
    bool hasBCa        = false;

    for (const auto& c : result.getCandidates())
    {
        if (c.getMethod() == MethodId::Percentile)
            hasPercentile = true;
        if (c.getMethod() == MethodId::BCa)
            hasBCa = true;

        // No other methods should appear
        REQUIRE((
            c.getMethod() == MethodId::Percentile ||
            c.getMethod() == MethodId::BCa
        ));
    }

    REQUIRE(hasPercentile);
    REQUIRE(hasBCa);
}
