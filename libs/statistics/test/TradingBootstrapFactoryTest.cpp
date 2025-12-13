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
