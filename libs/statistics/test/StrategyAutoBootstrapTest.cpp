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
#include <optional>
#include <set>

#include "StrategyAutoBootstrap.h"
#include "TradingBootstrapFactory.h"
#include "AutoBootstrapSelector.h"
#include "StatUtils.h"
#include "StationaryMaskResamplers.h"
#include "TradeResampling.h"
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

// Ratio-style statistic alias (robust log profit factor)
using LogPFStat =
    typename mkc_timeseries::StatUtils<Decimal>::LogProfitFactorStat;

// StrategyAutoBootstrap instantiation for ratio statistics
using RatioStrategyAutoBootstrapType =
    palvalidator::analysis::StrategyAutoBootstrap<Decimal, LogPFStat, MaskResampler>;

// Factory alias for the ratio-stat StrategyAutoBootstrap
using RatioFactoryAlias = typename RatioStrategyAutoBootstrapType::Factory;

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

// Strongly profitable series designed to yield a clearly > 1 profit factor.
// 40 wins of +1% and 20 losses of -0.3% => PF ≈ 6.7
static std::vector<Decimal> makeStrongProfitFactorReturns()
{
    std::vector<Decimal> xs;
    xs.reserve(60);

    // 40 positive bars
    for (int i = 0; i < 40; ++i)
        xs.push_back(Decimal(0.01));   // +1%

    // 20 modest negative bars
    for (int i = 0; i < 20; ++i)
        xs.push_back(Decimal(-0.003)); // -0.3%

    return xs;
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
        REQUIRE(cfg.getConfidenceLevel()          == CL);
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
        const double      ratio   = 10.0;

        double inner = static_cast<double>(B_outer) / ratio;
        if (inner < 1.0)
            inner = 1.0;
        const std::size_t expected_inner = static_cast<std::size_t>(inner);

        REQUIRE(cfg.getPercentileTNumInnerReplications(ratio) == expected_inner);
    }
}

TEST_CASE("BootstrapConfiguration: Inner B falls back to MIN_INNER for tiny B",
          "[StrategyAutoBootstrap][BootstrapConfiguration]")
{
    // Use very small B to exercise the clamp(MIN_INNER, B_outer / ratio, MAX_INNER) logic.
    const std::size_t  B_small = 5;
    const std::size_t  L       = 4;
    const double       CL      = 0.90;
    const std::uint64_t stage  = 7;
    const std::uint64_t fold   = 1;

    BootstrapConfiguration cfg(B_small, L, CL, stage, fold);

    REQUIRE(cfg.getNumBootStrapReplications() == B_small);

    const double ratio = 10.0;

    REQUIRE(cfg.getPercentileTNumOuterReplications() == B_small);

    constexpr std::size_t kMinInnerReplications = palvalidator::analysis::percentile_t_constants::MIN_INNER;
    constexpr std::size_t kMaxInnerReplications = 2000; // must match getPercentileTNumInnerReplications cap

    const std::size_t ideal_inner =
        static_cast<std::size_t>(static_cast<double>(B_small) / ratio);

    const std::size_t expected_inner =
        std::min<std::size_t>(std::max<std::size_t>(ideal_inner, kMinInnerReplications),
                              kMaxInnerReplications);

    REQUIRE(cfg.getPercentileTNumInnerReplications(ratio) == expected_inner);
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

    // Basic alias sanity: ResultType should match AutoCIResult<Decimal>
    STATIC_REQUIRE(std::is_same<ResultType, AutoCIResult>::value);

    // Candidate and MethodId should be those from AutoCIResult as well
    STATIC_REQUIRE(std::is_same<CandidateType, typename AutoCIResult::Candidate>::value);
    STATIC_REQUIRE(std::is_same<MethodIdType,  typename AutoCIResult::MethodId>::value);

    // Note: BCaResampler is no longer hardcoded - it now uses the same Resampler
    // template parameter as other bootstrap methods for consistency in tournaments

    // Ensure configuration objects are usable with the StrategyAutoBootstrap type
    BootstrapConfiguration cfg(/*numBootStrapReplications*/ 500,
                               /*blockSize*/                10,
                               /*confidenceLevel*/          0.95,
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

TEST_CASE("StrategyAutoBootstrap integration: Ratio stats enforce positive lower bound",
          "[StrategyAutoBootstrap][Integration][Ratio]")
{
    // Sanity: LogPFStat must advertise itself as a ratio statistic
    STATIC_REQUIRE(LogPFStat::isRatioStatistic());

    // Use a dedicated factory for the ratio-stat SAB
    RatioFactoryAlias factory(/*masterSeed*/ 424242u);

    const std::size_t  B      = 800;   // plenty of replications for a stable CI
    const std::size_t  blockL = 4;     // modest stationary block length
    const double       CL     = 0.95;
    const std::uint64_t stage = 5;
    const std::uint64_t fold  = 0;

    auto returns = makeStrongProfitFactorReturns();
    REQUIRE(returns.size() >= 40);

    // Sanity check: underlying robust log-PF should be positive on this series.
    // This is *not* strictly required for the test, but it documents intent.
    {
        LogPFStat stat;
        const Decimal s = stat(returns);
        REQUIRE(num::to_double(s) > 0.0);
    }

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("DummyStrategy_RatioStat_PositivePF", portfolio, returns);

    BootstrapAlgorithmsConfiguration algos; // all enabled (Normal, Basic, Percentile, M-out-of-N, Percentile-T, BCa)

    // Build a StrategyAutoBootstrap instance wired for the ratio statistic
    palvalidator::analysis::BootstrapConfiguration cfg(
        B,
        blockL,
        CL,
        stage,
        fold
    );

    RatioStrategyAutoBootstrapType autoBootstrap(factory, strategy, cfg, algos);

    AutoCIResult result = autoBootstrap.run(returns);

    // There must be at least one candidate
    REQUIRE(result.getCandidates().size() >= 1);

    const auto& chosen = result.getChosenCandidate();

    // With isRatioStatistic()==true, StrategyAutoBootstrap should have
    // passed enforcePositive=true into the AutoBootstrapSelector, and
    // the domain-penalty logic should ensure that the *winning* candidate
    // has a strictly positive lower bound.
    //
    // Because LogPFStat lives on a log-PF scale, LB > 0 here implies
    // PF_LB > 1.0.
    const double lb = chosen.getLower();
    REQUIRE(lb > 0.0);
}


// =============================================================================
//
//                     TRADE-LEVEL BOOTSTRAP TESTS
//
// These tests exercise the SampleType = Trade<Decimal> code path through
// StrategyAutoBootstrap, TradingBootstrapFactory, and every bootstrap engine.
//
// Structural differences from bar-level tests:
//
//   1. Resampler  = IIDResampler<Trade<Decimal>>
//      Trades are assumed independent so IID resampling is correct.
//      IIDResampler has no constructor arguments (blockSize is irrelevant).
//      BootstrapConfiguration is constructed with blockSize=1 to document
//      that clearly, even though makeResampler() ignores the value at
//      trade level via if constexpr.
//
//   2. run() receives std::vector<Trade<Decimal>> instead of std::vector<Decimal>.
//
//   3. MOutOfN must use a FIXED subsample ratio.
//      Adaptive ratio computation (Hill estimator, skewness, kurtosis) requires
//      ~8+ scalar observations to be reliable and is blocked by a static_assert
//      inside MOutOfNPercentileBootstrap.  StrategyAutoBootstrap dispatches to
//      makeMOutOfN (fixed-ratio) when isTradeLevelBootstrappingEnabled()==true
//      and to makeAdaptiveMOutOfN (bar-level only) otherwise.
//
//   4. BCa receives the trade vector at construction time.
//      The factory resolves to the trade-level makeBCa overload via overload
//      resolution on the first argument type (vector<Trade<Decimal>> vs
//      vector<Decimal>).
//
//   5. BootstrapConfiguration is constructed with
//      enableTradeLevelBootstrapping=true.  All existing callers that omit
//      this argument default to false, so backward compatibility is preserved.
//
// =============================================================================

// ---------------------------------------------------------------------------
// Trade-level sampler
//
// GeoMeanStat<Decimal> accepts const std::vector<Decimal>&, so it cannot be
// used directly as a Sampler when SampleType = Trade<Decimal>.  We define a
// thin wrapper here that flattens the multi-bar daily returns stored in each
// Trade object into a single contiguous sequence using TradeFlatteningAdapter,
// then delegates to GeoMeanStat.
//
// This is semantically correct: the geometric mean is computed over the full
// bar-level return stream that constitutes all sampled trades, which is the
// same view that the BCa jackknife uses internally.
// ---------------------------------------------------------------------------

struct TradeGeoMeanSampler
{
    // Satisfy the interface expected by StrategyAutoBootstrap and
    // AutoBootstrapSelector.
    static constexpr bool isRatioStatistic() { return false; }

    mkc_timeseries::StatisticSupport support() const
    {
        // Delegate to the bar-level stat for the correct support descriptor.
        return GeoMeanSampler{}.support();
    }

    Decimal operator()(const std::vector<mkc_timeseries::Trade<Decimal>>& trades) const
    {
        // TradeFlatteningAdapter is constructed with the downstream stat function.
        // Its operator()(trades) concatenates all daily bar returns across the
        // sampled trades into a flat vector, then applies the stat function.
        mkc_timeseries::TradeFlatteningAdapter<Decimal> adapter(
            [](const std::vector<Decimal>& flat) -> Decimal
            {
                return GeoMeanSampler{}(flat);
            });
        return adapter(trades);
    }
};

// ---------------------------------------------------------------------------
// Trade-level type aliases
// ---------------------------------------------------------------------------

using TradeT            = mkc_timeseries::Trade<Decimal>;
using TradeIIDResampler = mkc_timeseries::IIDResampler<TradeT>;

// StrategyAutoBootstrap specialised for trade-level GeoMean bootstrapping.
// The fourth template parameter (SampleType) is Trade<Decimal>.
using TradeSABType = palvalidator::analysis::StrategyAutoBootstrap<
    Decimal,
    TradeGeoMeanSampler,
    TradeIIDResampler,
    TradeT>;

// The factory type is the same concrete class regardless of SampleType because
// TradingBootstrapFactory is only parameterised on the RNG engine type.
using TradeSABFactory = typename TradeSABType::Factory;

// ---------------------------------------------------------------------------
// Trade data helpers
//
// Each function returns >= 20 Trade<Decimal> objects with explicit multi-bar
// daily returns so that the direction of the aggregate statistic is obvious.
// 20+ trades is comfortable above the minimum-n requirements of all engines
// (BCa: n>=2; MOutOfN: n>=3; PercentileT: n>=3).
//
// Trades are constructed from a braced initialiser list of bar returns using
// Trade<Decimal>'s range/move constructor.
// ---------------------------------------------------------------------------

// 20 clearly profitable trades: all daily returns are positive.
// GeoMean of the flattened returns will be positive.
static std::vector<TradeT> makePositiveTrades()
{
    return {
        TradeT({ 0.012,  0.008}),
        TradeT({ 0.015,  0.010,  0.005}),
        TradeT({ 0.020,  0.018}),
        TradeT({ 0.008,  0.012,  0.016,  0.009}),
        TradeT({ 0.022,  0.014}),
        TradeT({ 0.010,  0.008,  0.006}),
        TradeT({ 0.018,  0.015}),
        TradeT({ 0.025,  0.010,  0.008}),
        TradeT({ 0.011,  0.013}),
        TradeT({ 0.019,  0.007,  0.009,  0.004}),
        TradeT({ 0.014,  0.016}),
        TradeT({ 0.009,  0.011,  0.013}),
        TradeT({ 0.021,  0.017}),
        TradeT({ 0.013,  0.008,  0.010}),
        TradeT({ 0.016,  0.012}),
        TradeT({ 0.023,  0.019}),
        TradeT({ 0.017,  0.011,  0.007}),
        TradeT({ 0.024,  0.013}),
        TradeT({ 0.010,  0.015,  0.008}),
        TradeT({ 0.018,  0.020}),
    };
}

// 20 clearly losing trades: all daily returns are negative.
// GeoMean of the flattened returns will be negative.
static std::vector<TradeT> makeNegativeTrades()
{
    return {
        TradeT({-0.012, -0.008}),
        TradeT({-0.015, -0.010, -0.005}),
        TradeT({-0.020, -0.018}),
        TradeT({-0.008, -0.012, -0.016, -0.009}),
        TradeT({-0.022, -0.014}),
        TradeT({-0.010, -0.008, -0.006}),
        TradeT({-0.018, -0.015}),
        TradeT({-0.025, -0.010, -0.008}),
        TradeT({-0.011, -0.013}),
        TradeT({-0.019, -0.007, -0.009, -0.004}),
        TradeT({-0.014, -0.016}),
        TradeT({-0.009, -0.011, -0.013}),
        TradeT({-0.021, -0.017}),
        TradeT({-0.013, -0.008, -0.010}),
        TradeT({-0.016, -0.012}),
        TradeT({-0.023, -0.019}),
        TradeT({-0.017, -0.011, -0.007}),
        TradeT({-0.024, -0.013}),
        TradeT({-0.010, -0.015, -0.008}),
        TradeT({-0.018, -0.020}),
    };
}

// 20 trades with mixed returns, net positive in aggregate.
static std::vector<TradeT> makeMixedTrades()
{
    return {
        TradeT({ 0.015,  0.010}),
        TradeT({-0.005, -0.003}),
        TradeT({ 0.020,  0.012,  0.008}),
        TradeT({-0.007, -0.004, -0.002}),
        TradeT({ 0.018,  0.015}),
        TradeT({ 0.011,  0.009,  0.006}),
        TradeT({-0.008, -0.006}),
        TradeT({ 0.022,  0.014,  0.010}),
        TradeT({-0.010, -0.008, -0.003}),
        TradeT({ 0.019,  0.013}),
        TradeT({ 0.016,  0.011,  0.007}),
        TradeT({-0.006, -0.004}),
        TradeT({ 0.023,  0.017,  0.009}),
        TradeT({ 0.014,  0.010}),
        TradeT({-0.009, -0.005, -0.002}),
        TradeT({ 0.021,  0.015}),
        TradeT({ 0.012,  0.008,  0.005}),
        TradeT({-0.004, -0.003}),
        TradeT({ 0.018,  0.014,  0.011}),
        TradeT({ 0.010,  0.007}),
    };
}

// ---------------------------------------------------------------------------
// Helper: build a TradeSABType with trade-level bootstrapping enabled
// ---------------------------------------------------------------------------

static TradeSABType
makeTradeLevelAutoBootstrap(
    TradeSABFactory&                        factory,
    DummyStrategy&                          strategy,
    std::size_t                             B,
    double                                  cl,
    std::uint64_t                           stageTag,
    std::uint64_t                           fold,
    const BootstrapAlgorithmsConfiguration& algos = BootstrapAlgorithmsConfiguration{})
{
    BootstrapConfiguration cfg(
        B,
        /*blockSize*/                1,   // IIDResampler ignores blockSize; 1 documents that
        cl,
        stageTag,
        fold,
        /*rescaleMOutOfN*/           true,
        /*enableTradeLevelBootstrap*/true);

    return TradeSABType(factory, strategy, cfg, algos);
}


// ---------------------------------------------------------------------------
// Unit tests: BootstrapConfiguration trade-level flag
// ---------------------------------------------------------------------------

TEST_CASE("BootstrapConfiguration: isTradeLevelBootstrappingEnabled defaults to false",
          "[StrategyAutoBootstrap][BootstrapConfiguration][TradeLevel]")
{
    // The 5-argument constructor predates trade-level support.
    // Verify that omitting the new flag leaves it disabled, preserving
    // backward compatibility for all existing call sites.
    BootstrapConfiguration cfg(500, 4, 0.95, 1u, 0u);

    REQUIRE_FALSE(cfg.isTradeLevelBootstrappingEnabled());
}

TEST_CASE("BootstrapConfiguration: isTradeLevelBootstrappingEnabled reflects constructor arg",
          "[StrategyAutoBootstrap][BootstrapConfiguration][TradeLevel]")
{
    SECTION("Explicitly disabled via sixth and seventh args")
    {
        BootstrapConfiguration cfg(500, 1, 0.95, 1u, 0u,
                                   /*rescaleMOutOfN*/            true,
                                   /*enableTradeLevelBootstrap*/ false);
        REQUIRE_FALSE(cfg.isTradeLevelBootstrappingEnabled());
    }

    SECTION("Explicitly enabled")
    {
        BootstrapConfiguration cfg(500, 1, 0.95, 1u, 0u,
                                   /*rescaleMOutOfN*/            true,
                                   /*enableTradeLevelBootstrap*/ true);
        REQUIRE(cfg.isTradeLevelBootstrappingEnabled());
    }

    SECTION("rescaleMOutOfN=false does not affect trade-level flag")
    {
        BootstrapConfiguration cfg(500, 1, 0.95, 1u, 0u,
                                   /*rescaleMOutOfN*/            false,
                                   /*enableTradeLevelBootstrap*/ true);
        REQUIRE(cfg.isTradeLevelBootstrappingEnabled());
        REQUIRE_FALSE(cfg.getRescaleMOutOfN());
    }
}


// ---------------------------------------------------------------------------
// Unit tests: trade-level type wiring
// ---------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap trade-level: Type aliases are correctly wired",
          "[StrategyAutoBootstrap][TradeLevel][TypeAliases]")
{
    // Static checks: TradeSABType must expose the same nested aliases as the
    // bar-level specialisation, regardless of SampleType.
    STATIC_REQUIRE(std::is_same_v<typename TradeSABType::Result,    AutoCIResult>);
    STATIC_REQUIRE(std::is_same_v<typename TradeSABType::Candidate, typename AutoCIResult::Candidate>);
    STATIC_REQUIRE(std::is_same_v<typename TradeSABType::MethodId,  typename AutoCIResult::MethodId>);

    // The factory type is the same concrete class for both bar-level and
    // trade-level because TradingBootstrapFactory is only parameterised on
    // the RNG engine type, not on SampleType.
    STATIC_REQUIRE(std::is_same_v<typename TradeSABType::Factory, TradeSABFactory>);
    STATIC_REQUIRE(std::is_same_v<TradeSABFactory, FactoryAlias>);

    // TradeGeoMeanSampler must satisfy the interface contracts that
    // StrategyAutoBootstrap queries at run time.
    STATIC_REQUIRE(TradeGeoMeanSampler::isRatioStatistic() == false);

    // Runtime: a BootstrapConfiguration built for trade-level must say so.
    BootstrapConfiguration cfg(500, 1, 0.95, 1u, 0u, true, true);
    REQUIRE(cfg.isTradeLevelBootstrappingEnabled());
}


// ---------------------------------------------------------------------------
// Integration tests: trade-level StrategyAutoBootstrap
// ---------------------------------------------------------------------------

TEST_CASE("StrategyAutoBootstrap trade-level integration: Mixed trades produce sane CI",
          "[StrategyAutoBootstrap][TradeLevel][Integration]")
{
    // Smoke test: all six algorithms run on a mixed-sign trade population
    // and produce at least one valid candidate with structural CI sanity.
    TradeSABFactory factory(/*masterSeed*/ 112233u);

    const std::size_t   B      = 500;
    const double        CL     = 0.95;
    const std::uint64_t stage  = 10;
    const std::uint64_t fold   = 0;

    auto trades = makeMixedTrades();
    REQUIRE(trades.size() >= 10);

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("TradeMixed", portfolio, {});

    BootstrapAlgorithmsConfiguration algos; // all enabled
    auto sab = makeTradeLevelAutoBootstrap(factory, strategy, B, CL, stage, fold, algos);

    AutoCIResult result = sab.run(trades);

    REQUIRE(result.getCandidates().size() >= 1);

    const auto& chosen = result.getChosenCandidate();

    // Structural CI checks
    REQUIRE(chosen.getUpper()  >= chosen.getLower());
    REQUIRE(chosen.getCl()     == Catch::Approx(CL));
    REQUIRE(chosen.getN()      == trades.size());

    // Every individual candidate must also be structurally valid
    for (const auto& c : result.getCandidates())
    {
        REQUIRE(c.getUpper() >= c.getLower());
        REQUIRE(c.getCl()    == Catch::Approx(CL));
        REQUIRE(c.getN()     == trades.size());
    }
}

TEST_CASE("StrategyAutoBootstrap trade-level integration: Positive trades yield positive bootstrap mean",
          "[StrategyAutoBootstrap][TradeLevel][Integration]")
{
    TradeSABFactory factory(/*masterSeed*/ 445566u);

    const std::size_t   B      = 500;
    const double        CL     = 0.95;
    const std::uint64_t stage  = 11;
    const std::uint64_t fold   = 0;

    auto trades = makePositiveTrades();
    REQUIRE(trades.size() >= 10);

    // Pre-condition: verify the sampler returns a positive value on this data.
    {
        TradeGeoMeanSampler stat;
        REQUIRE(stat(trades) > Decimal(0.0));
    }

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("TradePositive", portfolio, {});

    BootstrapAlgorithmsConfiguration algos; // all enabled
    auto sab = makeTradeLevelAutoBootstrap(factory, strategy, B, CL, stage, fold, algos);

    AutoCIResult result = sab.run(trades);

    REQUIRE(result.getChosenCandidate().getMean() > Decimal(0.0));

    // CI width must be non-degenerate
    const auto& chosen = result.getChosenCandidate();
    REQUIRE(chosen.getUpper() - chosen.getLower() > Decimal(0.0));
}

TEST_CASE("StrategyAutoBootstrap trade-level integration: Negative trades yield negative bootstrap mean",
          "[StrategyAutoBootstrap][TradeLevel][Integration]")
{
    TradeSABFactory factory(/*masterSeed*/ 778899u);

    const std::size_t   B      = 500;
    const double        CL     = 0.95;
    const std::uint64_t stage  = 12;
    const std::uint64_t fold   = 0;

    auto trades = makeNegativeTrades();
    REQUIRE(trades.size() >= 10);

    // Pre-condition: sampler returns a negative value on all-negative trades.
    {
        TradeGeoMeanSampler stat;
        REQUIRE(stat(trades) < Decimal(0.0));
    }

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("TradeNegative", portfolio, {});

    BootstrapAlgorithmsConfiguration algos; // all enabled
    auto sab = makeTradeLevelAutoBootstrap(factory, strategy, B, CL, stage, fold, algos);

    AutoCIResult result = sab.run(trades);

    REQUIRE(result.getChosenCandidate().getMean() < Decimal(0.0));
}

TEST_CASE("StrategyAutoBootstrap trade-level integration: MOutOfN takes fixed-ratio path",
          "[StrategyAutoBootstrap][TradeLevel][Integration]")
{
    // Isolate the MOutOfN engine to verify that StrategyAutoBootstrap dispatches
    // to makeMOutOfN (fixed ratio) rather than makeAdaptiveMOutOfN when
    // isTradeLevelBootstrappingEnabled()==true.
    //
    // If the dispatch were wrong, MOutOfNPercentileBootstrap would throw
    // std::logic_error at run time ("adaptive ratio mode is not supported for
    // trade-level bootstrapping") — making this test fail immediately with a
    // clear attribution.
    TradeSABFactory factory(/*masterSeed*/ 314159u);

    const std::size_t   B      = 500;
    const double        CL     = 0.95;
    const std::uint64_t stage  = 13;
    const std::uint64_t fold   = 0;

    auto trades = makeMixedTrades();

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("TradeMOutOfN_FixedRatio", portfolio, {});

    // Enable only MOutOfN so any failure is unambiguously attributable to it.
    BootstrapAlgorithmsConfiguration algos(
        /*enableNormal*/      false,
        /*enableBasic*/       false,
        /*enablePercentile*/  false,
        /*enableMOutOfN*/     true,
        /*enablePercentileT*/ false,
        /*enableBCa*/         false);

    auto sab = makeTradeLevelAutoBootstrap(factory, strategy, B, CL, stage, fold, algos);

    std::optional<AutoCIResult> resultOpt;
    REQUIRE_NOTHROW(resultOpt.emplace(sab.run(trades)));
    const AutoCIResult& result = *resultOpt;

    // The sole candidate must be MOutOfN.
    REQUIRE(result.getCandidates().size() == 1);
    REQUIRE(result.getChosenCandidate().getMethod() == MethodId::MOutOfN);

    // Structural sanity on the produced CI.
    REQUIRE(result.getChosenCandidate().getUpper() >= result.getChosenCandidate().getLower());
    REQUIRE(result.getChosenCandidate().getN()     == trades.size());
}

TEST_CASE("StrategyAutoBootstrap trade-level integration: BCa participates and succeeds",
          "[StrategyAutoBootstrap][TradeLevel][Integration]")
{
    // BCa is the only algorithm that receives data at construction time via
    // makeBCa.  Its trade-level dispatch path (overload resolution on the first
    // argument type) is structurally different from all other engines and
    // deserves an explicit presence check.
    TradeSABFactory factory(/*masterSeed*/ 161803u);

    const std::size_t   B      = 500;
    const double        CL     = 0.95;
    const std::uint64_t stage  = 14;
    const std::uint64_t fold   = 0;

    auto trades = makeMixedTrades();

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("TradeBCa", portfolio, {});

    // Enable only BCa to isolate its behaviour.
    BootstrapAlgorithmsConfiguration algos(
        /*enableNormal*/      false,
        /*enableBasic*/       false,
        /*enablePercentile*/  false,
        /*enableMOutOfN*/     false,
        /*enablePercentileT*/ false,
        /*enableBCa*/         true);

    auto sab = makeTradeLevelAutoBootstrap(factory, strategy, B, CL, stage, fold, algos);

    std::optional<AutoCIResult> resultOpt;
    REQUIRE_NOTHROW(resultOpt.emplace(sab.run(trades)));
    const AutoCIResult& result = *resultOpt;

    REQUIRE(result.getCandidates().size() == 1);
    REQUIRE(result.getChosenCandidate().getMethod() == MethodId::BCa);
    REQUIRE(result.getChosenCandidate().getUpper()  >= result.getChosenCandidate().getLower());
    REQUIRE(result.getChosenCandidate().getN()       == trades.size());
}

TEST_CASE("StrategyAutoBootstrap trade-level integration: Algorithm flags control candidates",
          "[StrategyAutoBootstrap][TradeLevel][Integration]")
{
    // Mirror the bar-level flags test: enable only Percentile + BCa and verify
    // no other method appears in the candidate set.
    TradeSABFactory factory(/*masterSeed*/ 271828u);

    const std::size_t   B      = 500;
    const double        CL     = 0.95;
    const std::uint64_t stage  = 15;
    const std::uint64_t fold   = 0;

    auto trades = makePositiveTrades();

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("TradeFlags", portfolio, {});

    BootstrapAlgorithmsConfiguration algos(
        /*enableNormal*/      false,
        /*enableBasic*/       false,
        /*enablePercentile*/  true,
        /*enableMOutOfN*/     false,
        /*enablePercentileT*/ false,
        /*enableBCa*/         true);

    auto sab = makeTradeLevelAutoBootstrap(factory, strategy, B, CL, stage, fold, algos);

    AutoCIResult result = sab.run(trades);

    bool hasPercentile = false;
    bool hasBCa        = false;

    for (const auto& c : result.getCandidates())
    {
        REQUIRE((c.getMethod() == MethodId::Percentile ||
                 c.getMethod() == MethodId::BCa));

        if (c.getMethod() == MethodId::Percentile) hasPercentile = true;
        if (c.getMethod() == MethodId::BCa)        hasBCa        = true;
    }

    REQUIRE(hasPercentile);
    REQUIRE(hasBCa);
}

TEST_CASE("StrategyAutoBootstrap trade-level integration: All six algorithms run without error",
          "[StrategyAutoBootstrap][TradeLevel][Integration]")
{
    // Verify that every algorithm in the tournament completes successfully on
    // trade data, producing at least one candidate per enabled method.  This
    // is the broadest smoke test for the full trade-level code path.
    TradeSABFactory factory(/*masterSeed*/ 299792u);

    const std::size_t   B      = 500;
    const double        CL     = 0.95;
    const std::uint64_t stage  = 16;
    const std::uint64_t fold   = 0;

    auto trades = makeMixedTrades();

    auto portfolio = createTestPortfolio();
    DummyStrategy strategy("TradeAllAlgos", portfolio, {});

    BootstrapAlgorithmsConfiguration algos; // all enabled
    auto sab = makeTradeLevelAutoBootstrap(factory, strategy, B, CL, stage, fold, algos);

    std::optional<AutoCIResult> resultOpt;
    REQUIRE_NOTHROW(resultOpt.emplace(sab.run(trades)));
    const AutoCIResult& result = *resultOpt;

    // With 20 trades and B=500, every algorithm should succeed.
    // Check each expected method is represented.
    std::set<MethodId> foundMethods;
    for (const auto& c : result.getCandidates())
        foundMethods.insert(c.getMethod());

    REQUIRE(foundMethods.count(MethodId::Normal)     > 0);
    REQUIRE(foundMethods.count(MethodId::Basic)      > 0);
    REQUIRE(foundMethods.count(MethodId::Percentile) > 0);
    REQUIRE(foundMethods.count(MethodId::MOutOfN)    > 0);
    REQUIRE(foundMethods.count(MethodId::PercentileT)> 0);
    REQUIRE(foundMethods.count(MethodId::BCa)        > 0);
}
