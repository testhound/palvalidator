// --------------------------- TradingBootstrapFactory tests ---------------------------
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TradingBootstrapFactory.h"
#include "RngUtils.h"
#include "BiasCorrectedBootstrap.h"
#include "randutils.hpp"
#include "number.h"
#include "TestUtils.h"

using DecimalType = num::DefaultNumber;

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
