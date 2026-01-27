// --------------------------- TradingBootstrapFactory tests ---------------------------
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PalStrategy.h"
#include "TradingBootstrapFactory.h"
#include "RngUtils.h"
#include "BiasCorrectedBootstrap.h"
#include "randutils.hpp"
#include "number.h"
#include "TestUtils.h"

using DecimalType = num::DefaultNumber;
using Decimal = DecimalType; // Add alias for compatibility

// Type aliases for testing
using Resampler = StationaryBlockResampler<Decimal, randutils::mt19937_rng>;

// Simple mean sampler for testing
struct MeanSampler {
    Decimal operator()(const std::vector<Decimal>& x) const {
        if (x.empty()) return Decimal(0.0);
        double sum = 0.0;
        for (const auto& v : x) sum += num::to_double(v);
        return Decimal(sum / static_cast<double>(x.size()));
    }
};

static PatternDescription *
createDescription (const std::string& fileName, unsigned int index, unsigned long indexDate,
		   const std::string& percLong, const std::string& percShort,
		   unsigned int numTrades, unsigned int consecutiveLosses)
{
  auto percentLong = std::make_shared<DecimalType>(createDecimal(percLong));
  auto percentShort = std::make_shared<DecimalType>(createDecimal(percShort));

  return new PatternDescription ((char *) fileName.c_str(), index, indexDate, percentLong, percentShort,
  		 numTrades, consecutiveLosses);
}

static std::shared_ptr<LongMarketEntryOnOpen>
createLongOnOpen()
{
  return std::make_shared<LongMarketEntryOnOpen>();
}

static std::shared_ptr<ShortMarketEntryOnOpen>
createShortOnOpen()
{
  return std::make_shared<ShortMarketEntryOnOpen>();
}

static std::shared_ptr<LongSideProfitTargetInPercent>
createLongProfitTarget(const std::string& targetPct)
{
  return std::make_shared<LongSideProfitTargetInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

static std::shared_ptr<LongSideStopLossInPercent>
createLongStopLoss(const std::string& targetPct)
{
  return std::make_shared<LongSideStopLossInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

static std::shared_ptr<ShortSideProfitTargetInPercent>
createShortProfitTarget(const std::string& targetPct)
{
  return std::make_shared<ShortSideProfitTargetInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

static std::shared_ptr<ShortSideStopLossInPercent>
createShortStopLoss(const std::string& targetPct)
{
  return std::make_shared<ShortSideStopLossInPercent>(std::make_shared<decimal7>(createDecimal(targetPct)));
}

static std::shared_ptr<PriceActionLabPattern>
createLongPattern1()
{
  // Create description using shared_ptr
  auto percentLong = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc = std::make_shared<PatternDescription>("C2_122AR.txt", 39, 20131217,
                                                   percentLong, percentShort, 21, 2);

  auto open5 = std::make_shared<PriceBarOpen>(5);
  auto close5 = std::make_shared<PriceBarClose>(5);
  auto gt1 = std::make_shared<GreaterThanExpr>(open5, close5);

  auto close6 = std::make_shared<PriceBarClose>(6);
  auto gt2 = std::make_shared<GreaterThanExpr>(close5, close6);

  // OPEN OF 5 BARS AGO > CLOSE OF 5 BARS AGO
  // AND CLOSE OF 5 BARS AGO > CLOSE OF 6 BARS AGO
  auto and1 = std::make_shared<AndExpr>(gt1, gt2);

  auto open6 = std::make_shared<PriceBarOpen>(6);
  auto gt3 = std::make_shared<GreaterThanExpr>(close6, open6);

  auto close8 = std::make_shared<PriceBarClose>(8);
  auto gt4 = std::make_shared<GreaterThanExpr>(open6, close8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  auto and2 = std::make_shared<AndExpr>(gt3, gt4);

  auto open8 = std::make_shared<PriceBarOpen>(8);
  auto gt5 = std::make_shared<GreaterThanExpr>(close8, open8);

  // CLOSE OF 6 BARS AGO > OPEN OF 6 BARS AGO
  // AND OPEN OF 6 BARS AGO > CLOSE OF 8 BARS AGO
  // CLOSE OF 8 BARS AGO > OPEN OF 8 BARS AGO

  auto and3 = std::make_shared<AndExpr>(and2, gt5);
  auto longPattern1 = std::make_shared<AndExpr>(and1, and3);
  auto entry = createLongOnOpen();
  auto target = createLongProfitTarget("2.56");
  auto stop = createLongStopLoss("1.28");

  // 2.56 profit target in points = 93.81
  return std::make_shared<PriceActionLabPattern>(desc, longPattern1,
                                                 entry,
                                                 target,
                                                 stop);
}

static std::shared_ptr<PriceActionLabPattern>
createShortPattern1()
{
  // Create description using shared_ptr
  auto percentLong = std::make_shared<DecimalType>(createDecimal("90.00"));
  auto percentShort = std::make_shared<DecimalType>(createDecimal("10.00"));
  auto desc = std::make_shared<PatternDescription>("C2_122AR.txt", 39, 20111017,
                                                   percentLong, percentShort, 21, 2);
  // Short pattern

  auto high4 = std::make_shared<PriceBarHigh>(4);
  auto high5 = std::make_shared<PriceBarHigh>(5);
  auto high3 = std::make_shared<PriceBarHigh>(3);
  auto high0 = std::make_shared<PriceBarHigh>(0);
  auto high1 = std::make_shared<PriceBarHigh>(1);
  auto high2 = std::make_shared<PriceBarHigh>(2);

  auto shortgt1 = std::make_shared<GreaterThanExpr>(high4, high5);
  auto shortgt2 = std::make_shared<GreaterThanExpr>(high5, high3);
  auto shortgt3 = std::make_shared<GreaterThanExpr>(high3, high0);
  auto shortgt4 = std::make_shared<GreaterThanExpr>(high0, high1);
  auto shortgt5 = std::make_shared<GreaterThanExpr>(high1, high2);

  auto shortand1 = std::make_shared<AndExpr>(shortgt1, shortgt2);
  auto shortand2 = std::make_shared<AndExpr>(shortgt3, shortgt4);
  auto shortand3 = std::make_shared<AndExpr>(shortgt5, shortand2);
  auto shortPattern1 = std::make_shared<AndExpr>(shortand1, shortand3);

  auto entry = createShortOnOpen();
  auto target = createShortProfitTarget("1.34");
  auto stop = createShortStopLoss("1.28");

  return std::make_shared<PriceActionLabPattern>(desc, shortPattern1,
                                                 entry,
                                                 target,
                                                 stop);
}

TEST_CASE("TradingBootstrapFactory: deterministic BCa (Stationary blocks)", "[BCaBootStrap][Factory][CRN][Determinism][Stationary]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    // Build a mildly dependent toy series
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER_SEED = 0xDEADBEEFCAFEBABEull;
    const uint64_t strategyId  = 0x1122334455667788ull; // e.g., strategy.hashCode()
    const uint64_t stageTag    = 1;                     // Bootstrap
    const unsigned L           = 3;
    const uint64_t fold        = 0;
    const unsigned B           = 1000;
    const double   CL          = 0.95;

    TradingBootstrapFactory<Eng> factory(MASTER_SEED);

    // Use mean as default statistic via the convenience overload
    Resamp sampler(L);
    auto bca1 = factory.makeBCa<D, Resamp>(returns, B, CL, sampler, strategyId, stageTag, L, fold);
    auto bca2 = factory.makeBCa<D, Resamp>(returns, B, CL, sampler, strategyId, stageTag, L, fold);

    REQUIRE(num::to_double(bca1.getLowerBound()) == Catch::Approx(num::to_double(bca2.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bca1.getUpperBound()) == Catch::Approx(num::to_double(bca2.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bca1.getMean())       == Catch::Approx(num::to_double(bca2.getMean())).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory: sensitivity to tag changes (L / fold affect streams)", "[BCaBootStrap][Factory][CRN][Sensitivity]") {
    using D     = DecimalType;
    using Eng   = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    // Mildly dependent toy returns
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER_SEED = 0xBADC0FFEE0DDF00Dull;
    const uint64_t strategyId  = 0xF00DFACE12345678ull;
    const uint64_t stageTag    = 1;   // Bootstrap
    const unsigned L3          = 3;
    const unsigned L4          = 4;
    const uint64_t fold0       = 0;
    const uint64_t fold1       = 1;
    const unsigned B           = 1200;
    const double   CL          = 0.95;

    TradingBootstrapFactory<Eng> factory(MASTER_SEED);

    // --- L sensitivity (statistical) ---
    auto bca_L3 = factory.makeBCa<D, Resamp>(returns, B, CL, Resamp(L3), strategyId, stageTag, L3, fold0);
    auto bca_L4 = factory.makeBCa<D, Resamp>(returns, B, CL, Resamp(L4), strategyId, stageTag, L4, fold0);

    const bool diff_L = (num::to_double(bca_L3.getLowerBound()) != num::to_double(bca_L4.getLowerBound())) ||
                        (num::to_double(bca_L3.getUpperBound()) != num::to_double(bca_L4.getUpperBound()));
    REQUIRE(diff_L);

    // --- fold sensitivity (deterministic RNG-stream check) ---
    using Prov = mkc_timeseries::rng_utils::CRNEngineProvider<Eng>;
    using mkc_timeseries::rng_utils::CRNKey;
    using mkc_timeseries::rng_utils::get_random_value;

    Prov p0( CRNKey(MASTER_SEED).with_tag(strategyId).with_tags({ stageTag, L3, fold0 }) );
    Prov p1( CRNKey(MASTER_SEED).with_tag(strategyId).with_tags({ stageTag, L3, fold1 }) );

    auto e0 = p0.make_engine(0);
    auto e1 = p1.make_engine(0);

    // Use get_random_value() to extract raw 64-bit values from the engines
    const uint64_t u0a = get_random_value(e0), u1a = get_random_value(e1);
    const uint64_t u0b = get_random_value(e0), u1b = get_random_value(e1);

    // Changing 'fold' must change the stream; compare a couple draws to avoid edge collisions
    REQUIRE( ((u0a != u1a) || (u0b != u1b)) );
}

TEST_CASE("TradingBootstrapFactory: works with IID resampler and custom statistic", "[BCaBootStrap][Factory][IID][CustomStat]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using ResampIID = IIDResampler<D, Eng>;

    // IID-ish sample
    std::vector<D> returns = {
        createDecimal("0.012"), createDecimal("-0.006"), createDecimal("0.007"),
        createDecimal("0.004"), createDecimal("-0.011"), createDecimal("0.018"),
        createDecimal("0.000"), createDecimal("0.009"),  createDecimal("0.010"),
        createDecimal("-0.003"), createDecimal("0.006"), createDecimal("0.013"),
        createDecimal("0.005"),  createDecimal("-0.002"), createDecimal("0.001"),
        createDecimal("0.006")
    };

    const uint64_t MASTER_SEED = 0xFACEFACEFACEFACEull;
    const uint64_t strategyId  = 0x0F1E2D3C4B5A6978ull;
    const uint64_t stageTag    = 1;
    const unsigned B           = 1500;
    const double   CL          = 0.95;

    TradingBootstrapFactory<Eng> factory(MASTER_SEED);

    // Custom statistic: trimmed mean (drop worst & best one)
    auto trimmedMean = [](const std::vector<D>& x) -> D {
        if (x.size() <= 2) return mkc_timeseries::StatUtils<D>::computeMean(x);
        std::vector<D> y = x;
        std::sort(y.begin(), y.end(), [](const D& a, const D& b){ return num::to_double(a) < num::to_double(b); });
        y.erase(y.begin());         // drop min
        y.pop_back();               // drop max
        return mkc_timeseries::StatUtils<D>::computeMean(y);
    };

    // Full-control overload (custom statFn)
    ResampIID iidSampler;
    auto bca1 = factory.makeBCa<D, ResampIID>(
        returns, B, CL,
        std::function<D(const std::vector<D>&)>(trimmedMean),
        iidSampler, strategyId, stageTag, /*L*/0, /*fold*/0
    );

    // Deterministic when parameters match
    auto bca2 = factory.makeBCa<D, ResampIID>(
        returns, B, CL,
        std::function<D(const std::vector<D>&)>(trimmedMean),
        iidSampler, strategyId, stageTag, /*L*/0, /*fold*/0
    );

    REQUIRE(num::to_double(bca1.getLowerBound()) == Catch::Approx(num::to_double(bca2.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bca1.getUpperBound()) == Catch::Approx(num::to_double(bca2.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bca1.getMean())       == Catch::Approx(num::to_double(bca2.getMean())).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory: makeMOutOfN produces deterministic results with CRN", "[Factory][MOutOfN][CRN][Determinism]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    // Mildly dependent toy series (like prior tests)
    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    // Parameters
    const uint64_t MASTER = 0xA17EA17EA17EA17Eull;
    const uint64_t sid    = 0x1111222233334444ull;
    const uint64_t stage  = 1;       // e.g., Bootstrap stage tag
    const unsigned L      = 3;
    const uint64_t fold   = 0;
    const std::size_t B   = 1200;
    const double CL       = 0.95;
    const double rho      = 0.70;    // m/n ratio

    // Mean sampler (explicit type for template argument)
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // Build two bootstraps & CRN RNGs with identical tags
    auto [mn1, crn1] = factory.makeMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, rho, Resamp(L), sid, stage, L, fold
    );
    auto [mn2, crn2] = factory.makeMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, rho, Resamp(L), sid, stage, L, fold
    );

    // Create engines from CRN providers for deterministic execution
    auto rng1 = crn1.make_engine(0);
    auto rng2 = crn2.make_engine(0);

    // Run both — should be bit-identical with CRN
    auto r1 = mn1.run(x, meanSampler, rng1 /*rng*/, /*m_sub_override=*/0);
    auto r2 = mn2.run(x, meanSampler, rng2 /*rng*/, /*m_sub_override=*/0);

    REQUIRE(num::to_double(r1.lower) == Catch::Approx(num::to_double(r2.lower)).epsilon(0));
    REQUIRE(num::to_double(r1.upper) == Catch::Approx(num::to_double(r2.upper)).epsilon(0));
    REQUIRE(num::to_double(r1.mean) == Catch::Approx(num::to_double(r2.mean)).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory: makeMOutOfN responds to tag changes (L, fold)", "[Factory][MOutOfN][CRN][Sensitivity]") {
    using D   = DecimalType;
    using Eng = std::mt19937_64;
    using Resamp = StationaryBlockResampler<D, Eng>;

    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER = 0xBADDCAFE1234C0DEull;
    const uint64_t sid    = 0x5555AAAAFFFF0000ull;
    const uint64_t stage  = 1;
    const unsigned L3     = 3;
    const unsigned L4     = 4;
    const uint64_t fold0  = 0, fold1 = 1;
    const std::size_t B   = 1000;
    const double CL       = 0.95;
    const double rho      = 0.75;

    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // Change only L
    auto [mn_L3, crn_L3] = factory.makeMOutOfN<D, decltype(meanSampler), Resamp>(B, CL, rho, Resamp(L3), sid, stage, L3, fold0);
    auto [mn_L4, crn_L4] = factory.makeMOutOfN<D, decltype(meanSampler), Resamp>(B, CL, rho, Resamp(L4), sid, stage, L4, fold0);

    auto rng_L3 = crn_L3.make_engine(0);
    auto rng_L4 = crn_L4.make_engine(0);

    auto rL3 = mn_L3.run(x, meanSampler, rng_L3, 0);
    auto rL4 = mn_L4.run(x, meanSampler, rng_L4, 0);

    // With overwhelming probability at least one bound differs
    const bool diffL = num::to_double(rL3.lower) != num::to_double(rL4.lower)
                    || num::to_double(rL3.upper) != num::to_double(rL4.upper);
    REQUIRE(diffL);

    // Change only fold (same L)
    auto [mn_f0, crn_f0] = factory.makeMOutOfN<D, decltype(meanSampler), Resamp>(B, CL, rho, Resamp(L3), sid, stage, L3, fold0);
    auto [mn_f1, crn_f1] = factory.makeMOutOfN<D, decltype(meanSampler), Resamp>(B, CL, rho, Resamp(L3), sid, stage, L3, fold1);

    auto rng_f0 = crn_f0.make_engine(0);
    auto rng_f1 = crn_f1.make_engine(0);

    auto rf0 = mn_f0.run(x, meanSampler, rng_f0, 0);
    auto rf1 = mn_f1.run(x, meanSampler, rng_f1, 0);

    const bool difffold = num::to_double(rf0.lower) != num::to_double(rf1.lower)
                       || num::to_double(rf0.upper) != num::to_double(rf1.upper);
    REQUIRE(difffold);
}

TEST_CASE("TradingBootstrapFactory: makePercentileT deterministic with CRN (default Executor)", "[Factory][PercentileT][CRN][Determinism]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    using PTExec = concurrency::SingleThreadExecutor; // default

    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER = 0xCAFED00DBABECAFEull;
    const uint64_t sid    = 0x0123456789ABCDEFull;
    const uint64_t stage  = 1;
    const unsigned L      = 3;
    const uint64_t fold   = 0;
    const std::size_t B_outer = 1000;
    const std::size_t B_inner = 200;
    const double CL = 0.95;
    const double rho_outer = 1.0;
    const double rho_inner = 1.0;

    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    auto [pt1, crn1] = factory.makePercentileT<D, decltype(meanSampler), Resamp, PTExec>(
        B_outer, B_inner, CL, Resamp(L), sid, stage, L, fold, rho_outer, rho_inner
    );
    auto [pt2, crn2] = factory.makePercentileT<D, decltype(meanSampler), Resamp, PTExec>(
        B_outer, B_inner, CL, Resamp(L), sid, stage, L, fold, rho_outer, rho_inner
    );

    auto rng1 = crn1.make_engine(0);
    auto rng2 = crn2.make_engine(0);

    auto r1 = pt1.run(x, meanSampler, rng1, /*m_outer_override=*/0, /*m_inner_override=*/0);
    auto r2 = pt2.run(x, meanSampler, rng2, /*m_outer_override=*/0, /*m_inner_override=*/0);

    REQUIRE(num::to_double(r1.lower) == Catch::Approx(num::to_double(r2.lower)).epsilon(0));
    REQUIRE(num::to_double(r1.upper) == Catch::Approx(num::to_double(r2.upper)).epsilon(0));
    REQUIRE(num::to_double(r1.mean) == Catch::Approx(num::to_double(r2.mean)).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory: makePercentileT responds to tag changes", "[Factory][PercentileT][CRN][Sensitivity]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER = 0xAAAABBBBCCCCDDDDull;
    const uint64_t sid    = 0x9999888877776666ull;
    const uint64_t stage  = 1;
    const unsigned L3     = 3;
    const unsigned L4     = 4;
    const uint64_t fold0  = 0, fold1 = 1;
    const std::size_t B_outer = 800;
    const std::size_t B_inner = 150;
    const double CL = 0.95;

    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // L sensitivity (keep fold same)
    auto [pt_L3, crn_L3] = factory.makePercentileT<D, decltype(meanSampler), Resamp>(
        B_outer, B_inner, CL, Resamp(L3), sid, stage, L3, fold0
    );
    auto [pt_L4, crn_L4] = factory.makePercentileT<D, decltype(meanSampler), Resamp>(
        B_outer, B_inner, CL, Resamp(L4), sid, stage, L4, fold0
    );

    auto rng_L3 = crn_L3.make_engine(0);
    auto rng_L4 = crn_L4.make_engine(0);

    auto rL3 = pt_L3.run(x, meanSampler, rng_L3, 0, 0);
    auto rL4 = pt_L4.run(x, meanSampler, rng_L4, 0, 0);

    const bool diffL = num::to_double(rL3.lower) != num::to_double(rL4.lower)
                    || num::to_double(rL3.upper) != num::to_double(rL4.upper);
    REQUIRE(diffL);

    // fold sensitivity (keep L same)
    auto [pt_f0, crn_f0] = factory.makePercentileT<D, decltype(meanSampler), Resamp>(
        B_outer, B_inner, CL, Resamp(L3), sid, stage, L3, fold0
    );
    auto [pt_f1, crn_f1] = factory.makePercentileT<D, decltype(meanSampler), Resamp>(
        B_outer, B_inner, CL, Resamp(L3), sid, stage, L3, fold1
    );

    auto rng_f0 = crn_f0.make_engine(0);
    auto rng_f1 = crn_f1.make_engine(0);

    auto rf0 = pt_f0.run(x, meanSampler, rng_f0, 0, 0);
    auto rf1 = pt_f1.run(x, meanSampler, rng_f1, 0, 0);

    const bool difffold = num::to_double(rf0.lower) != num::to_double(rf1.lower)
                       || num::to_double(rf0.upper) != num::to_double(rf1.upper);
    REQUIRE(difffold);
}

// --- New Test Cases for makeStudentizedT ---

TEST_CASE("TradingBootstrapFactory: makeStudentizedT deterministic with CRN", "[Factory][StudentizedT][CRN][Determinism]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    // Mildly dependent toy returns (same as existing tests)
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }

    // Mean sampler
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    const uint64_t MASTER_SEED = 0xD1CEACCE550DDC0Dul;
    const uint64_t strategyId  = 0x4893A0B2C7E5F6D1ull;
    const uint64_t stageTag    = 2;                     // Studentized T stage tag
    const unsigned L           = 3;
    const uint64_t fold        = 0;
    const unsigned B_OUTER     = 1000;
    const double   CL          = 0.95;

    TradingBootstrapFactory<Eng> factory(MASTER_SEED);

    // Call 1
    Resamp sampler(L);
    auto tboot1 = factory.makeStudentizedT<D, Resamp>(
        returns, B_OUTER, CL,
        std::function<D(const std::vector<D>&)>(meanSampler),
        sampler, strategyId, stageTag, L, fold
    );

    // Call 2 (identical parameters)
    auto tboot2 = factory.makeStudentizedT<D, Resamp>(
        returns, B_OUTER, CL,
        std::function<D(const std::vector<D>&)>(meanSampler),
        sampler, strategyId, stageTag, L, fold
    );
    
    // The bounds must be bit-identical due to Common Random Numbers (CRN)
    REQUIRE(num::to_double(tboot1.getLowerBound()) == Catch::Approx(num::to_double(tboot2.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(tboot1.getUpperBound()) == Catch::Approx(num::to_double(tboot2.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(tboot1.getStatistic())  == Catch::Approx(num::to_double(tboot2.getStatistic())).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory: makeStudentizedT responds to tag changes (L, fold)", "[Factory][StudentizedT][CRN][Sensitivity]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }

    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    const uint64_t MASTER_SEED = 0xAAFFBB00CC11DD22ull;
    const uint64_t strategyId  = 0x33445566778899AAull;
    const uint64_t stageTag    = 2;
    const unsigned L3          = 3;
    const unsigned L4          = 4;
    const uint64_t fold0       = 0, fold1 = 1;
    const unsigned B_OUTER     = 1200;
    const double   CL          = 0.95;

    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    
    // --- L sensitivity (change L) ---
    auto tboot_L3 = factory.makeStudentizedT<D, Resamp>(
        returns, B_OUTER, CL, std::function<D(const std::vector<D>&)>(meanSampler),
        Resamp(L3), strategyId, stageTag, L3, fold0
    );
    auto tboot_L4 = factory.makeStudentizedT<D, Resamp>(
        returns, B_OUTER, CL, std::function<D(const std::vector<D>&)>(meanSampler),
        Resamp(L4), strategyId, stageTag, L4, fold0
    );

    // With overwhelming probability, changing L changes the result (statistically and due to CRN tags)
    const bool diffL = num::to_double(tboot_L3.getLowerBound()) != num::to_double(tboot_L4.getLowerBound());
    REQUIRE(diffL);

    // --- Fold sensitivity (change fold) ---
    auto tboot_f0 = factory.makeStudentizedT<D, Resamp>(
        returns, B_OUTER, CL, std::function<D(const std::vector<D>&)>(meanSampler),
        Resamp(L3), strategyId, stageTag, L3, fold0
    );
    auto tboot_f1 = factory.makeStudentizedT<D, Resamp>(
        returns, B_OUTER, CL, std::function<D(const std::vector<D>&)>(meanSampler),
        Resamp(L3), strategyId, stageTag, L3, fold1
    );

    // Changing fold must change the RNG stream, leading to different bounds
    const bool difffold = num::to_double(tboot_f0.getLowerBound()) != num::to_double(tboot_f1.getLowerBound());
    REQUIRE(difffold);
}

TEST_CASE("TradingBootstrapFactory: makeAdaptiveMOutOfN produces deterministic results with CRN", "[Factory][MOutOfNAdaptive][CRN][Determinism]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    // Mildly dependent toy series (same pattern as other tests)
    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    // Parameters
    const uint64_t MASTER = 0xCAFEBABEDEADBEEFull;
    const uint64_t sid    = 0xABCDEF0123456789ull;
    const uint64_t stage  = 1;       // e.g., Bootstrap stage tag
    const unsigned L      = 3;
    const uint64_t fold   = 0;
    const std::size_t B   = 1200;
    const double CL       = 0.95;

    // Mean sampler (explicit type for template argument)
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // Build two adaptive m-out-of-n bootstraps & CRN RNGs with identical tags
    auto [mn1, crn1] = factory.makeAdaptiveMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), sid, stage, L, fold
    );
    auto [mn2, crn2] = factory.makeAdaptiveMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), sid, stage, L, fold
    );

    // Create engines from CRN providers for deterministic execution
    auto rng1 = crn1.make_engine(0);
    auto rng2 = crn2.make_engine(0);

    // Run both — should be bit-identical with CRN even in adaptive mode
    auto r1 = mn1.run(x, meanSampler, rng1 /*rng*/, /*m_sub_override=*/0);
    auto r2 = mn2.run(x, meanSampler, rng2 /*rng*/, /*m_sub_override=*/0);

    REQUIRE(num::to_double(r1.lower) == Catch::Approx(num::to_double(r2.lower)).epsilon(0));
    REQUIRE(num::to_double(r1.upper) == Catch::Approx(num::to_double(r2.upper)).epsilon(0));
    REQUIRE(num::to_double(r1.mean)  == Catch::Approx(num::to_double(r2.mean)).epsilon(0));

    // Sanity check on the adaptive ratio itself: must be well-defined in (0,1)
    REQUIRE(r1.computed_ratio == Catch::Approx(r2.computed_ratio).epsilon(0));
    REQUIRE(r1.computed_ratio > 0.0);
    REQUIRE(r1.computed_ratio < 1.0);
}

TEST_CASE("TradingBootstrapFactory: makeAdaptiveMOutOfN responds to tag changes (L, fold)", "[Factory][MOutOfNAdaptive][CRN][Sensitivity]") {
    using D   = DecimalType;
    using Eng = std::mt19937_64;
    using Resamp = StationaryBlockResampler<D, Eng>;

    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER = 0x0DDC0FFEE1234BEEull;
    const uint64_t sid    = 0x7777AAAABBBBCCCCull;
    const uint64_t stage  = 1;
    const unsigned L3     = 3;
    const unsigned L4     = 4;
    const uint64_t fold0  = 0, fold1 = 1;
    const std::size_t B   = 1000;
    const double CL       = 0.95;

    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // --- L sensitivity (change L, keep fold fixed) ---
    auto [mn_L3, crn_L3] = factory.makeAdaptiveMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L3), sid, stage, L3, fold0
    );
    auto [mn_L4, crn_L4] = factory.makeAdaptiveMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L4), sid, stage, L4, fold0
    );

    auto rng_L3 = crn_L3.make_engine(0);
    auto rng_L4 = crn_L4.make_engine(0);

    auto rL3 = mn_L3.run(x, meanSampler, rng_L3, 0);
    auto rL4 = mn_L4.run(x, meanSampler, rng_L4, 0);

    // With overwhelming probability at least one bound differs
    const bool diffL = num::to_double(rL3.lower) != num::to_double(rL4.lower)
                    || num::to_double(rL3.upper) != num::to_double(rL4.upper)
                    || rL3.computed_ratio        != rL4.computed_ratio;
    REQUIRE(diffL);

    // --- fold sensitivity (change fold, keep L fixed) ---
    auto [mn_f0, crn_f0] = factory.makeAdaptiveMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L3), sid, stage, L3, fold0
    );
    auto [mn_f1, crn_f1] = factory.makeAdaptiveMOutOfN<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L3), sid, stage, L3, fold1
    );

    auto rng_f0 = crn_f0.make_engine(0);
    auto rng_f1 = crn_f1.make_engine(0);

    auto rf0 = mn_f0.run(x, meanSampler, rng_f0, 0);
    auto rf1 = mn_f1.run(x, meanSampler, rng_f1, 0);

    const bool difffold = num::to_double(rf0.lower) != num::to_double(rf1.lower)
                       || num::to_double(rf0.upper) != num::to_double(rf1.upper)
                       || rf0.computed_ratio        != rf1.computed_ratio;
    REQUIRE(difffold);
}

TEST_CASE("TradingBootstrapFactory: makeBasic creates valid instance", "[TradingBootstrapFactory][Basic]")
{
    const uint64_t masterSeed = 12345;
    TradingBootstrapFactory<> factory(masterSeed);

    // Test Parameters
    const unsigned B = 1000;
    const double CL = 0.95;
    const uint64_t strategyId = 99;
    const uint64_t stageTag = 1;
    const uint64_t L = 10;
    const uint64_t fold = 0;
    
    Resampler resampler(5); // Block length 5

    // Call makeBasic (using raw ID overload)
    // We expect a pair: {BasicBootstrap, CRNRng}
    auto result = factory.makeBasic<Decimal, MeanSampler>(
        B, CL, resampler, strategyId, stageTag, L, fold
    );

    auto& bootstrap = result.first;
    auto& provider = result.second;

    SECTION("Bootstrap instance has correct configuration")
    {
        REQUIRE(bootstrap.B() == B);
        REQUIRE(bootstrap.CL() == CL);
        // Verify resampler was copied correctly
        REQUIRE(bootstrap.resampler().getL() == 5);
    }

    SECTION("CRN provider is functional")
    {
        // Provider should generate a valid engine for a given replicate 'b'
        auto rng = provider.make_engine(0);
        // Basic check: generating a number doesn't crash
        auto value = mkc_timeseries::rng_utils::get_random_value(rng);
        REQUIRE(value > 0);
    }

    SECTION("Bootstrap run integration")
    {
        // Create dummy data
        std::vector<Decimal> data(100);
        for (int i = 0; i < 100; ++i) {
            data[i] = Decimal(i + 1.0);
        }

        MeanSampler sampler;

        // Run bootstrap using the provider from the pair
        // This confirms the type compatibility between the factory-produced
        // bootstrap class and the factory-produced provider.
        auto res = bootstrap.run(data, sampler, provider);

        REQUIRE(res.B == B);
        REQUIRE(res.n == data.size());
        REQUIRE(num::to_double(res.mean) == Catch::Approx(50.5)); // Mean of 1..100 is 50.5
        REQUIRE(res.lower < res.mean);
        REQUIRE(res.upper > res.mean);
    }
}

TEST_CASE("TradingBootstrapFactory: makeBasic with ThreadPoolExecutor", "[TradingBootstrapFactory][Basic][Parallel]")
{
    // Verify that we can specify the Executor type explicitly via the factory
    const uint64_t masterSeed = 999;
    TradingBootstrapFactory<> factory(masterSeed);

    Resampler resampler(3);

    // Request a BasicBootstrap that uses ThreadPoolExecutor<2>
    auto result = factory.makeBasic<
        Decimal, 
        MeanSampler, 
        Resampler, 
        concurrency::ThreadPoolExecutor<2>
    >(500, 0.90, resampler, 101, 2, 5, 0);

    auto& bootstrap = result.first;
    auto& provider = result.second;
    
    // Just verify it runs without crashing (implies executor instantiation worked)
    std::vector<Decimal> data(50, Decimal(1.0)); // Constant data
    MeanSampler sampler;
    
    auto res = bootstrap.run(data, sampler, provider);
    REQUIRE(num::to_double(res.mean) == Catch::Approx(1.0));
    REQUIRE(num::to_double(res.lower) == Catch::Approx(1.0).margin(1e-9)); // Constant data -> no variance
    REQUIRE(num::to_double(res.upper) == Catch::Approx(1.0).margin(1e-9));
}

TEST_CASE("TradingBootstrapFactory: makeNormal creates valid instance", "[TradingBootstrapFactory][Normal]")
{
    const uint64_t masterSeed = 98765;
    TradingBootstrapFactory<> factory(masterSeed);

    // Test Parameters
    const unsigned B = 1000;
    const double CL = 0.95;
    const uint64_t strategyId = 199;
    const uint64_t stageTag = 1;
    const uint64_t L = 10;
    const uint64_t fold = 0;
    
    Resampler resampler(5); // Block length 5

    // Call makeNormal (using raw ID overload)
    // We expect a pair: {NormalBootstrap, CRNRng}
    auto result = factory.makeNormal<Decimal, MeanSampler>(
        B, CL, resampler, strategyId, stageTag, L, fold
    );

    auto& bootstrap = result.first;
    auto& provider = result.second;

    SECTION("Bootstrap instance has correct configuration")
    {
        REQUIRE(bootstrap.B() == B);
        REQUIRE(bootstrap.CL() == CL);
        // Verify resampler was copied correctly
        REQUIRE(bootstrap.resampler().getL() == 5);
    }

    SECTION("CRN provider is functional")
    {
        // Provider should generate a valid engine for a given replicate 'b'
        auto rng = provider.make_engine(0);
        // Basic check: generating a number doesn't crash
        auto value = mkc_timeseries::rng_utils::get_random_value(rng);
        REQUIRE(value > 0);
    }

    SECTION("Bootstrap run integration")
    {
        // Create dummy data
        std::vector<Decimal> data(100);
        for (int i = 0; i < 100; ++i) {
            data[i] = Decimal(i + 1.0);
        }

        MeanSampler sampler;

        // Run bootstrap using the provider from the pair
        // This confirms the type compatibility between the factory-produced
        // bootstrap class and the factory-produced provider.
        auto res = bootstrap.run(data, sampler, provider);

        REQUIRE(res.B == B);
        REQUIRE(res.n == data.size());
        REQUIRE(num::to_double(res.mean) == Catch::Approx(50.5)); // Mean of 1..100 is 50.5
        REQUIRE(res.lower < res.mean);
        REQUIRE(res.upper > res.mean);
        REQUIRE(res.effective_B > B/2); // Most replicates should be valid
    }
}

TEST_CASE("TradingBootstrapFactory: makeNormal deterministic with CRN", "[Factory][Normal][CRN][Determinism]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    // Mildly dependent toy series (same pattern as other tests)
    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    // Parameters
    const uint64_t MASTER = 0xDEADBEEFCAFEBABEull;
    const uint64_t sid    = 0x1111222233334444ull;
    const uint64_t stage  = 1;       // e.g., Bootstrap stage tag
    const unsigned L      = 3;
    const uint64_t fold   = 0;
    const std::size_t B   = 1200;
    const double CL       = 0.95;

    // Mean sampler
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // Build two NormalBootstrap instances & CRN RNGs with identical tags
    auto [nb1, crn1] = factory.makeNormal<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), sid, stage, L, fold
    );
    auto [nb2, crn2] = factory.makeNormal<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), sid, stage, L, fold
    );

    // Create engines from CRN providers for deterministic execution
    auto rng1 = crn1.make_engine(0);
    auto rng2 = crn2.make_engine(0);

    // Run both — should be bit-identical with CRN
    auto r1 = nb1.run(x, meanSampler, rng1);
    auto r2 = nb2.run(x, meanSampler, rng2);

    REQUIRE(num::to_double(r1.lower) == Catch::Approx(num::to_double(r2.lower)).epsilon(0));
    REQUIRE(num::to_double(r1.upper) == Catch::Approx(num::to_double(r2.upper)).epsilon(0));
    REQUIRE(num::to_double(r1.mean) == Catch::Approx(num::to_double(r2.mean)).epsilon(0));
    REQUIRE(r1.se_boot == Catch::Approx(r2.se_boot).epsilon(0));
}

TEST_CASE("TradingBootstrapFactory: makeNormal responds to tag changes (L, fold)", "[Factory][Normal][CRN][Sensitivity]") {
    using D   = DecimalType;
    using Eng = std::mt19937_64;
    using Resamp = StationaryBlockResampler<D, Eng>;

    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER = 0xBADDCAFE1234C0DEull;
    const uint64_t sid    = 0x5555AAAAFFFF0000ull;
    const uint64_t stage  = 1;
    const unsigned L3     = 3;
    const unsigned L4     = 4;
    const uint64_t fold0  = 0, fold1 = 1;
    const std::size_t B   = 1000;
    const double CL       = 0.95;

    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // Change only L
    auto [nb_L3, crn_L3] = factory.makeNormal<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L3), sid, stage, L3, fold0);
    auto [nb_L4, crn_L4] = factory.makeNormal<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L4), sid, stage, L4, fold0);

    auto rng_L3 = crn_L3.make_engine(0);
    auto rng_L4 = crn_L4.make_engine(0);

    auto rL3 = nb_L3.run(x, meanSampler, rng_L3);
    auto rL4 = nb_L4.run(x, meanSampler, rng_L4);

    // With overwhelming probability at least one bound differs
    const bool diffL = num::to_double(rL3.lower) != num::to_double(rL4.lower)
                    || num::to_double(rL3.upper) != num::to_double(rL4.upper);
    REQUIRE(diffL);

    // Change only fold (same L)
    auto [nb_f0, crn_f0] = factory.makeNormal<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L3), sid, stage, L3, fold0);
    auto [nb_f1, crn_f1] = factory.makeNormal<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L3), sid, stage, L3, fold1);

    auto rng_f0 = crn_f0.make_engine(0);
    auto rng_f1 = crn_f1.make_engine(0);

    auto rf0 = nb_f0.run(x, meanSampler, rng_f0);
    auto rf1 = nb_f1.run(x, meanSampler, rng_f1);

    const bool difffold = num::to_double(rf0.lower) != num::to_double(rf1.lower)
                       || num::to_double(rf0.upper) != num::to_double(rf1.upper);
    REQUIRE(difffold);
}

TEST_CASE("TradingBootstrapFactory: makeNormal with ThreadPoolExecutor", "[TradingBootstrapFactory][Normal][Parallel]")
{
    // Verify that we can specify the Executor type explicitly via the factory
    const uint64_t masterSeed = 777;
    TradingBootstrapFactory<> factory(masterSeed);

    Resampler resampler(3);

    // Request a NormalBootstrap that uses ThreadPoolExecutor<2>
    auto result = factory.makeNormal<
        Decimal,
        MeanSampler,
        Resampler,
        concurrency::ThreadPoolExecutor<2>
    >(500, 0.90, resampler, 201, 2, 5, 0);

    auto& bootstrap = result.first;
    auto& provider = result.second;
    
    // Just verify it runs without crashing (implies executor instantiation worked)
    std::vector<Decimal> data(50, Decimal(1.0)); // Constant data
    MeanSampler sampler;
    
    auto res = bootstrap.run(data, sampler, provider);
    REQUIRE(num::to_double(res.mean) == Catch::Approx(1.0));
    REQUIRE(num::to_double(res.lower) <= num::to_double(res.mean));
    REQUIRE(num::to_double(res.upper) >= num::to_double(res.mean));
    // Note: For constant data, confidence interval should be quite tight around the mean
}

TEST_CASE("TradingBootstrapFactory: makePercentile creates valid instance", "[TradingBootstrapFactory][Percentile]")
{
    const uint64_t masterSeed = 54321;
    TradingBootstrapFactory<> factory(masterSeed);

    // Test Parameters
    const unsigned B = 1000;
    const double CL = 0.95;
    const uint64_t strategyId = 299;
    const uint64_t stageTag = 1;
    const uint64_t L = 10;
    const uint64_t fold = 0;
    
    Resampler resampler(5); // Block length 5

    // Call makePercentile (using raw ID overload)
    // We expect a pair: {PercentileBootstrap, CRNRng}
    auto result = factory.makePercentile<Decimal, MeanSampler>(
        B, CL, resampler, strategyId, stageTag, L, fold
    );

    auto& bootstrap = result.first;
    auto& provider = result.second;

    SECTION("Bootstrap instance has correct configuration")
    {
        REQUIRE(bootstrap.B() == B);
        REQUIRE(bootstrap.CL() == CL);
        // Verify resampler was copied correctly
        REQUIRE(bootstrap.resampler().getL() == 5);
    }

    SECTION("CRN provider is functional")
    {
        // Provider should generate a valid engine for a given replicate 'b'
        auto rng = provider.make_engine(0);
        // Basic check: generating a number doesn't crash
        auto value = mkc_timeseries::rng_utils::get_random_value(rng);
        REQUIRE(value > 0);
    }

    SECTION("Bootstrap run integration")
    {
        // Create dummy data
        std::vector<Decimal> data(100);
        for (int i = 0; i < 100; ++i) {
            data[i] = Decimal(i + 1.0);
        }

        MeanSampler sampler;

        // Run bootstrap using the provider from the pair
        // This confirms the type compatibility between the factory-produced
        // bootstrap class and the factory-produced provider.
        auto res = bootstrap.run(data, sampler, provider);

        REQUIRE(res.B == B);
        REQUIRE(res.n == data.size());
        REQUIRE(num::to_double(res.mean) == Catch::Approx(50.5)); // Mean of 1..100 is 50.5
        REQUIRE(res.lower < res.mean);
        REQUIRE(res.upper > res.mean);
        REQUIRE(res.effective_B > B/2); // Most replicates should be valid
    }
}

TEST_CASE("TradingBootstrapFactory: makePercentile deterministic with CRN", "[Factory][Percentile][CRN][Determinism]") {
    using D   = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;

    // Mildly dependent toy series (same pattern as other tests)
    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    // Parameters
    const uint64_t MASTER = 0xCAFEBABEDEADBEEFull;
    const uint64_t sid    = 0x2222333344445555ull;
    const uint64_t stage  = 1;       // e.g., Bootstrap stage tag
    const unsigned L      = 3;
    const uint64_t fold   = 0;
    const std::size_t B   = 1200;
    const double CL       = 0.95;

    // Mean sampler
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // Build two PercentileBootstrap instances & CRN RNGs with identical tags
    auto [pb1, crn1] = factory.makePercentile<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), sid, stage, L, fold
    );
    auto [pb2, crn2] = factory.makePercentile<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), sid, stage, L, fold
    );

    // Create engines from CRN providers for deterministic execution
    auto rng1 = crn1.make_engine(0);
    auto rng2 = crn2.make_engine(0);

    // Run both — should be bit-identical with CRN
    auto r1 = pb1.run(x, meanSampler, rng1);
    auto r2 = pb2.run(x, meanSampler, rng2);

    REQUIRE(num::to_double(r1.lower) == Catch::Approx(num::to_double(r2.lower)).epsilon(0));
    REQUIRE(num::to_double(r1.upper) == Catch::Approx(num::to_double(r2.upper)).epsilon(0));
    REQUIRE(num::to_double(r1.mean) == Catch::Approx(num::to_double(r2.mean)).epsilon(0));
    REQUIRE(r1.effective_B == r2.effective_B);
}

TEST_CASE("TradingBootstrapFactory: makePercentile responds to tag changes (L, fold)", "[Factory][Percentile][CRN][Sensitivity]") {
    using D   = DecimalType;
    using Eng = std::mt19937_64;
    using Resamp = StationaryBlockResampler<D, Eng>;

    std::vector<D> x;
    for (int k = 0; k < 40; ++k) {
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("0.004"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("-0.003"));
        x.push_back(createDecimal("0.002"));
    }

    const uint64_t MASTER = 0x1337CAFE1234DEADull;
    const uint64_t sid    = 0x6666AAAABBBB0000ull;
    const uint64_t stage  = 1;
    const unsigned L3     = 3;
    const unsigned L4     = 4;
    const uint64_t fold0  = 0, fold1 = 1;
    const std::size_t B   = 1000;
    const double CL       = 0.95;

    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };

    TradingBootstrapFactory<Eng> factory(MASTER);

    // Change only L
    auto [pb_L3, crn_L3] = factory.makePercentile<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L3), sid, stage, L3, fold0);
    auto [pb_L4, crn_L4] = factory.makePercentile<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L4), sid, stage, L4, fold0);

    auto rng_L3 = crn_L3.make_engine(0);
    auto rng_L4 = crn_L4.make_engine(0);

    auto rL3 = pb_L3.run(x, meanSampler, rng_L3);
    auto rL4 = pb_L4.run(x, meanSampler, rng_L4);

    // With overwhelming probability at least one bound differs
    const bool diffL = num::to_double(rL3.lower) != num::to_double(rL4.lower)
                    || num::to_double(rL3.upper) != num::to_double(rL4.upper);
    REQUIRE(diffL);

    // Change only fold (same L)
    auto [pb_f0, crn_f0] = factory.makePercentile<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L3), sid, stage, L3, fold0);
    auto [pb_f1, crn_f1] = factory.makePercentile<D, decltype(meanSampler), Resamp>(B, CL, Resamp(L3), sid, stage, L3, fold1);

    auto rng_f0 = crn_f0.make_engine(0);
    auto rng_f1 = crn_f1.make_engine(0);

    auto rf0 = pb_f0.run(x, meanSampler, rng_f0);
    auto rf1 = pb_f1.run(x, meanSampler, rng_f1);

    const bool difffold = num::to_double(rf0.lower) != num::to_double(rf1.lower)
                       || num::to_double(rf0.upper) != num::to_double(rf1.upper);
    REQUIRE(difffold);
}

TEST_CASE("TradingBootstrapFactory: makePercentile with ThreadPoolExecutor", "[TradingBootstrapFactory][Percentile][Parallel]")
{
    // Verify that we can specify the Executor type explicitly via the factory
    const uint64_t masterSeed = 888;
    TradingBootstrapFactory<> factory(masterSeed);

    Resampler resampler(3);

    // Request a PercentileBootstrap that uses ThreadPoolExecutor<2>
    auto result = factory.makePercentile<
        Decimal,
        MeanSampler,
        Resampler,
        concurrency::ThreadPoolExecutor<2>
    >(500, 0.90, resampler, 301, 2, 5, 0);

    auto& bootstrap = result.first;
    auto& provider = result.second;
    
    // Just verify it runs without crashing (implies executor instantiation worked)
    std::vector<Decimal> data(50, Decimal(1.0)); // Constant data
    MeanSampler sampler;
    
    auto res = bootstrap.run(data, sampler, provider);
    REQUIRE(num::to_double(res.mean) == Catch::Approx(1.0));
    REQUIRE(num::to_double(res.lower) <= num::to_double(res.mean));
    REQUIRE(num::to_double(res.upper) >= num::to_double(res.mean));
    // Note: For constant data, percentile CI should be quite tight around the mean
}

TEST_CASE("TradingBootstrapFactory: uses deterministicHashCode for reproducible CRN streams",
          "[Factory][Strategy][CRN][Integration][deterministicHashCode]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    // Create test data
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0xDEADBEEFCAFEBABEull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const unsigned B = 1000;
    const double CL = 0.95;
    
    // Create two strategies with IDENTICAL pattern configurations
    auto pattern1 = createLongPattern1();
    auto pattern2 = createLongPattern1();
    
    auto portfolio1 = std::make_shared<mkc_timeseries::Portfolio<D>>("P1");
    auto portfolio2 = std::make_shared<mkc_timeseries::Portfolio<D>>("P2");
    
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    
    auto strategy1 = mkc_timeseries::makePalStrategy<D>("S1", pattern1, portfolio1, options);
    auto strategy2 = mkc_timeseries::makePalStrategy<D>("S2", pattern2, portfolio2, options);
    
    // Verify strategies have same deterministicHashCode but different hashCode
    REQUIRE(strategy1->deterministicHashCode() == strategy2->deterministicHashCode());
    REQUIRE(strategy1->hashCode() != strategy2->hashCode());
    
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    
    // Use strategy objects directly (factory internally calls deterministicHashCode)
    Resamp sampler(L);
    auto bca1 = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *strategy1, stageTag, L, fold);
    auto bca2 = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *strategy2, stageTag, L, fold);
    
    // CRITICAL: Results must be IDENTICAL because deterministicHashCode is the same
    REQUIRE(num::to_double(bca1.getLowerBound()) == 
            Catch::Approx(num::to_double(bca2.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bca1.getUpperBound()) == 
            Catch::Approx(num::to_double(bca2.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bca1.getMean()) == 
            Catch::Approx(num::to_double(bca2.getMean())).epsilon(0));
    
    std::cout << "Strategy 1 deterministicHashCode: 0x" << std::hex 
              << strategy1->deterministicHashCode() << std::dec << std::endl;
    std::cout << "Strategy 2 deterministicHashCode: 0x" << std::hex 
              << strategy2->deterministicHashCode() << std::dec << std::endl;
    std::cout << "BCa intervals match: VERIFIED ✓" << std::endl;
}

TEST_CASE("TradingBootstrapFactory: different patterns produce different CRN streams",
          "[Factory][Strategy][CRN][Integration][deterministicHashCode]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    // Create test data
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0xBADC0FFEE0DDF00Dull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const unsigned B = 1200;
    const double CL = 0.95;
    
    // Create DIFFERENT patterns (long vs short)
    auto longPattern = createLongPattern1();
    auto shortPattern = createShortPattern1();
    
    auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("Portfolio");
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    
    auto longStrategy = mkc_timeseries::makePalStrategy<D>("Long", longPattern, portfolio, options);
    auto shortStrategy = mkc_timeseries::makePalStrategy<D>("Short", shortPattern, portfolio, options);
    
    // Verify different patterns have different deterministicHashCode
    REQUIRE(longStrategy->deterministicHashCode() != shortStrategy->deterministicHashCode());
    
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    
    Resamp sampler(L);
    auto bcaLong = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *longStrategy, stageTag, L, fold);
    auto bcaShort = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *shortStrategy, stageTag, L, fold);
    
    // Different patterns should produce different results (with high probability)
    // Check at least one bound differs
    const bool different = 
        (num::to_double(bcaLong.getLowerBound()) != num::to_double(bcaShort.getLowerBound())) ||
        (num::to_double(bcaLong.getUpperBound()) != num::to_double(bcaShort.getUpperBound()));
    
    REQUIRE(different);
    
    std::cout << "Long strategy hash:  0x" << std::hex 
              << longStrategy->deterministicHashCode() << std::dec << std::endl;
    std::cout << "Short strategy hash: 0x" << std::hex 
              << shortStrategy->deterministicHashCode() << std::dec << std::endl;
    std::cout << "Different patterns → Different CRN streams: VERIFIED ✓" << std::endl;
}

TEST_CASE("TradingBootstrapFactory: strategy object vs raw ID equivalence",
          "[Factory][Strategy][CRN][Integration][deterministicHashCode]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0xFEEDFACEDEADBEEFull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const unsigned B = 1000;
    const double CL = 0.95;
    
    // Create a strategy
    auto pattern = createLongPattern1();
    auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("Portfolio");
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    auto strategy = mkc_timeseries::makePalStrategy<D>("Test", pattern, portfolio, options);
    
    // Get the deterministic hash
    uint64_t strategyId = strategy->deterministicHashCode();
    
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    Resamp sampler(L);
    
    // Call with strategy object (uses deterministicHashCode internally)
    auto bcaFromObject = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *strategy, stageTag, L, fold);
    
    // Call with raw ID (explicit deterministicHashCode value)
    auto bcaFromId = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, strategyId, stageTag, L, fold);
    
    // Both should produce IDENTICAL results
    REQUIRE(num::to_double(bcaFromObject.getLowerBound()) == 
            Catch::Approx(num::to_double(bcaFromId.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bcaFromObject.getUpperBound()) == 
            Catch::Approx(num::to_double(bcaFromId.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bcaFromObject.getMean()) == 
            Catch::Approx(num::to_double(bcaFromId.getMean())).epsilon(0));
    
    std::cout << "Strategy object and raw ID produce identical results: VERIFIED ✓" << std::endl;
}


TEST_CASE("TradingBootstrapFactory: cross-run reproducibility with strategies",
          "[Factory][Strategy][CRN][Integration][deterministicHashCode][Reproducibility]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0xC0FFEEC0FFEEC0FFull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const unsigned B = 1000;
    const double CL = 0.95;
    
    // Simulate "Run 1" - create strategy, run bootstrap, capture results
    uint64_t run1_strategyId;
    D run1_lower, run1_upper, run1_mean;
    {
        auto pattern = createLongPattern1();
        auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("P1");
        mkc_timeseries::StrategyOptions options(false, 0, 0);
        auto strategy = mkc_timeseries::makePalStrategy<D>("Test", pattern, portfolio, options);
        
        run1_strategyId = strategy->deterministicHashCode();
        
        TradingBootstrapFactory<Eng> factory(MASTER_SEED);
        Resamp sampler(L);
        auto bca = factory.makeBCa<D, Resamp>(
            returns, B, CL, sampler, *strategy, stageTag, L, fold);
        
        // Capture the values we need
        run1_lower = bca.getLowerBound();
        run1_upper = bca.getUpperBound();
        run1_mean = bca.getMean();
    }
    // Strategy and factory destroyed - simulates end of program
    
    // Simulate "Run 2" - create fresh strategy with SAME configuration
    uint64_t run2_strategyId;
    D run2_lower, run2_upper, run2_mean;
    {
        auto pattern = createLongPattern1();  // Same pattern config
        auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("P2");  // Different portfolio instance
        mkc_timeseries::StrategyOptions options(false, 0, 0);
        auto strategy = mkc_timeseries::makePalStrategy<D>("Test", pattern, portfolio, options);
        
        run2_strategyId = strategy->deterministicHashCode();
        
        TradingBootstrapFactory<Eng> factory(MASTER_SEED);  // Fresh factory
        Resamp sampler(L);
        auto bca = factory.makeBCa<D, Resamp>(
            returns, B, CL, sampler, *strategy, stageTag, L, fold);
        
        // Capture the values we need
        run2_lower = bca.getLowerBound();
        run2_upper = bca.getUpperBound();
        run2_mean = bca.getMean();
    }
    
    // CRITICAL: strategyId must be identical across "runs"
    REQUIRE(run1_strategyId == run2_strategyId);
    
    // CRITICAL: Results must be IDENTICAL (bit-for-bit reproducibility)
    REQUIRE(num::to_double(run1_lower) == 
            Catch::Approx(num::to_double(run2_lower)).epsilon(0));
    REQUIRE(num::to_double(run1_upper) == 
            Catch::Approx(num::to_double(run2_upper)).epsilon(0));
    REQUIRE(num::to_double(run1_mean) == 
            Catch::Approx(num::to_double(run2_mean)).epsilon(0));
    
    std::cout << "Run 1 strategyId: 0x" << std::hex << run1_strategyId << std::dec << std::endl;
    std::cout << "Run 2 strategyId: 0x" << std::hex << run2_strategyId << std::dec << std::endl;
    std::cout << "Run 1 CI: [" << num::to_double(run1_lower) 
              << ", " << num::to_double(run1_upper) << "]" << std::endl;
    std::cout << "Run 2 CI: [" << num::to_double(run2_lower) 
              << ", " << num::to_double(run2_upper) << "]" << std::endl;
    std::cout << "Cross-run reproducibility: VERIFIED ✓" << std::endl;
}

TEST_CASE("TradingBootstrapFactory: makePercentile with strategy object integration",
          "[Factory][Strategy][Percentile][CRN][Integration][deterministicHashCode]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0xA1B2C3D4E5F60718ull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const std::size_t B = 1000;
    const double CL = 0.95;
    
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };
    
    // Create two strategies with identical patterns
    auto pattern1 = createLongPattern1();
    auto pattern2 = createLongPattern1();
    
    auto portfolio1 = std::make_shared<mkc_timeseries::Portfolio<D>>("P1");
    auto portfolio2 = std::make_shared<mkc_timeseries::Portfolio<D>>("P2");
    
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    
    auto strategy1 = mkc_timeseries::makePalStrategy<D>("S1", pattern1, portfolio1, options);
    auto strategy2 = mkc_timeseries::makePalStrategy<D>("S2", pattern2, portfolio2, options);
    
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    
    // Use strategy objects with makePercentile
    auto [pb1, crn1] = factory.makePercentile<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), *strategy1, stageTag, L, fold);
    auto [pb2, crn2] = factory.makePercentile<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), *strategy2, stageTag, L, fold);
    
    // Run with same replicate
    auto rng1 = crn1.make_engine(0);
    auto rng2 = crn2.make_engine(0);
    
    auto r1 = pb1.run(returns, meanSampler, rng1);
    auto r2 = pb2.run(returns, meanSampler, rng2);
    
    // Results should be identical
    REQUIRE(num::to_double(r1.lower) == Catch::Approx(num::to_double(r2.lower)).epsilon(0));
    REQUIRE(num::to_double(r1.upper) == Catch::Approx(num::to_double(r2.upper)).epsilon(0));
    REQUIRE(num::to_double(r1.mean) == Catch::Approx(num::to_double(r2.mean)).epsilon(0));
    REQUIRE(r1.effective_B == r2.effective_B);
    
    std::cout << "makePercentile with strategy objects: VERIFIED ✓" << std::endl;
}

TEST_CASE("TradingBootstrapFactory: makeBasic with strategy object integration",
          "[Factory][Strategy][Basic][CRN][Integration][deterministicHashCode]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0x8899AABBCCDDEEFFull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const std::size_t B = 1000;
    const double CL = 0.95;
    
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };
    
    // Create strategy
    auto pattern = createLongPattern1();
    auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("Portfolio");
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    auto strategy = mkc_timeseries::makePalStrategy<D>("Test", pattern, portfolio, options);
    
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    
    // Use strategy object with makeBasic
    auto [basic, crn] = factory.makeBasic<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), *strategy, stageTag, L, fold);
    
    // Run bootstrap
    auto rng = crn.make_engine(0);
    auto result = basic.run(returns, meanSampler, rng);
    
    // Verify result is valid
    REQUIRE(result.B == B);
    REQUIRE(result.n == returns.size());
    REQUIRE(num::to_double(result.lower) <= num::to_double(result.mean));
    REQUIRE(num::to_double(result.upper) >= num::to_double(result.mean));
    
    std::cout << "makeBasic with strategy object: VERIFIED ✓" << std::endl;
}

TEST_CASE("TradingBootstrapFactory: makeNormal with strategy object integration",
          "[Factory][Strategy][Normal][CRN][Integration][deterministicHashCode]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0x1234567890ABCDEFull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const std::size_t B = 1000;
    const double CL = 0.95;
    
    auto meanSampler = [](const std::vector<D>& v) -> D {
        return mkc_timeseries::StatUtils<D>::computeMean(v);
    };
    
    // Create strategy
    auto pattern = createShortPattern1();
    auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("Portfolio");
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    auto strategy = mkc_timeseries::makePalStrategy<D>("Short", pattern, portfolio, options);
    
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    
    // Use strategy object with makeNormal
    auto [normal, crn] = factory.makeNormal<D, decltype(meanSampler), Resamp>(
        B, CL, Resamp(L), *strategy, stageTag, L, fold);
    
    // Run bootstrap
    auto rng = crn.make_engine(0);
    auto result = normal.run(returns, meanSampler, rng);
    
    // Verify result is valid
    REQUIRE(result.B == B);
    REQUIRE(result.n == returns.size());
    REQUIRE(num::to_double(result.lower) <= num::to_double(result.mean));
    REQUIRE(num::to_double(result.upper) >= num::to_double(result.mean));
    
    std::cout << "makeNormal with strategy object: VERIFIED ✓" << std::endl;
}

TEST_CASE("TradingBootstrapFactory: cloned strategies produce same CRN stream",
          "[Factory][Strategy][Clone][CRN][Integration][deterministicHashCode]")
{
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    
    const uint64_t MASTER_SEED = 0xFACEB00CFACEB00Cull;
    const uint64_t stageTag = 1;
    const unsigned L = 3;
    const uint64_t fold = 0;
    const unsigned B = 1000;
    const double CL = 0.95;
    
    // Create original strategy
    auto pattern = createLongPattern1();
    auto portfolio1 = std::make_shared<mkc_timeseries::Portfolio<D>>("P1");
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    auto original = std::make_shared<mkc_timeseries::PalLongStrategy<D>>(
        "Original", pattern, portfolio1, options);
    
    // Clone to different portfolio
    auto portfolio2 = std::make_shared<mkc_timeseries::Portfolio<D>>("P2");
    auto cloned = std::dynamic_pointer_cast<mkc_timeseries::PalLongStrategy<D>>(
        original->clone(portfolio2));
    REQUIRE(cloned);
    
    // Verify different instances (different UUIDs) but same deterministicHashCode
    REQUIRE(original->hashCode() != cloned->hashCode());
    REQUIRE(original->deterministicHashCode() == cloned->deterministicHashCode());
    
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    Resamp sampler(L);
    
    auto bcaOriginal = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *original, stageTag, L, fold);
    auto bcaCloned = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *cloned, stageTag, L, fold);
    
    // Results should be identical
    REQUIRE(num::to_double(bcaOriginal.getLowerBound()) == 
            Catch::Approx(num::to_double(bcaCloned.getLowerBound())).epsilon(0));
    REQUIRE(num::to_double(bcaOriginal.getUpperBound()) == 
            Catch::Approx(num::to_double(bcaCloned.getUpperBound())).epsilon(0));
    REQUIRE(num::to_double(bcaOriginal.getMean()) == 
            Catch::Approx(num::to_double(bcaCloned.getMean())).epsilon(0));
    
    std::cout << "Cloned strategies use same CRN stream: VERIFIED ✓" << std::endl;
}

// ============================================================================
// DOCUMENTATION TEST
// ============================================================================

TEST_CASE("TradingBootstrapFactory: full workflow documentation example",
          "[Factory][Strategy][Documentation][Integration][deterministicHashCode]")
{
    // This test demonstrates the complete workflow from pattern to bootstrap results
    // showing how deterministicHashCode enables reproducible analyses
    
    using D = DecimalType;
    using Eng = randutils::mt19937_rng;
    using Resamp = StationaryBlockResampler<D, Eng>;
    
    std::cout << "\n=== Complete CRN Workflow Example ===" << std::endl;
    
    // Step 1: Create trading strategy
    std::cout << "Step 1: Create trading strategy..." << std::endl;
    auto pattern = createLongPattern1();
    auto portfolio = std::make_shared<mkc_timeseries::Portfolio<D>>("MyPortfolio");
    mkc_timeseries::StrategyOptions options(false, 0, 0);
    auto strategy = mkc_timeseries::makePalStrategy<D>("MyStrategy", pattern, portfolio, options);
    
    uint64_t strategyId = strategy->deterministicHashCode();
    std::cout << "  Strategy ID (deterministicHashCode): 0x" << std::hex 
              << strategyId << std::dec << std::endl;
    
    // Step 2: Prepare data
    std::cout << "Step 2: Prepare return data..." << std::endl;
    std::vector<D> returns;
    for (int k = 0; k < 40; ++k) {
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("0.002"));
    }
    std::cout << "  Data size: " << returns.size() << " observations" << std::endl;
    
    // Step 3: Create bootstrap factory with master seed
    std::cout << "Step 3: Create factory with master seed..." << std::endl;
    const uint64_t MASTER_SEED = 0x1234567890ABCDEFull;
    TradingBootstrapFactory<Eng> factory(MASTER_SEED);
    std::cout << "  Master seed: 0x" << std::hex << MASTER_SEED << std::dec << std::endl;
    
    // Step 4: Configure bootstrap parameters
    std::cout << "Step 4: Configure bootstrap parameters..." << std::endl;
    const uint64_t stageTag = 1;  // e.g., BootstrapStages::GEO_MEAN
    const unsigned L = 3;          // Block length
    const uint64_t fold = 0;       // NO_FOLD
    const unsigned B = 1000;       // Bootstrap resamples
    const double CL = 0.95;        // Confidence level
    std::cout << "  B=" << B << ", CL=" << CL << ", L=" << L << std::endl;
    
    // Step 5: Run BCa bootstrap using strategy object
    std::cout << "Step 5: Run BCa bootstrap..." << std::endl;
    Resamp sampler(L);
    auto bca = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *strategy, stageTag, L, fold);
    
    std::cout << "  Mean:  " << num::to_double(bca.getMean()) << std::endl;
    std::cout << "  95% CI: [" << num::to_double(bca.getLowerBound()) 
              << ", " << num::to_double(bca.getUpperBound()) << "]" << std::endl;
    
    // Step 6: Verify reproducibility
    std::cout << "Step 6: Verify reproducibility..." << std::endl;
    auto bca2 = factory.makeBCa<D, Resamp>(
        returns, B, CL, sampler, *strategy, stageTag, L, fold);
    
    bool reproducible = 
        (num::to_double(bca.getLowerBound()) == num::to_double(bca2.getLowerBound())) &&
        (num::to_double(bca.getUpperBound()) == num::to_double(bca2.getUpperBound()));
    
    REQUIRE(reproducible);
    std::cout << "  Reproducibility: " << (reproducible ? "VERIFIED ✓" : "FAILED ✗") << std::endl;
    
    std::cout << "\nKey Insight: Using strategy.deterministicHashCode() ensures" << std::endl;
    std::cout << "that the same pattern configuration always produces the same" << std::endl;
    std::cout << "bootstrap results (given same master seed and parameters)." << std::endl;
    std::cout << "=== End Example ===\n" << std::endl;
}
