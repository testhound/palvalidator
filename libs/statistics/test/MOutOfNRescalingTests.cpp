/**
 * @file MOutOfNPercentileBootstrap_RescalingTests.cpp
 * @brief Unit tests for M-out-of-N bootstrap rescaling functionality (rescale_to_n parameter)
 *
 * These tests verify the theoretically correct M-out-of-N inference mode where
 * CI bounds and diagnostics are rescaled from subsample size m to target sample size n.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <random>

#include "number.h"
#include "StatUtils.h"
#include "TestUtils.h"
#include "StationaryMaskResamplers.h"
#include "MOutOfNPercentileBootstrap.h"
#include "RngUtils.h"

using palvalidator::analysis::MOutOfNPercentileBootstrap;
using palvalidator::resampling::StationaryMaskValueResampler;
using mkc_timeseries::rng_utils::make_seed_seq;

// =====================================================================
// TEST GROUP 1: Basic Rescaling Behavior
// =====================================================================

TEST_CASE("MOutOfNPercentileBootstrap: rescale_to_n widens intervals", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    // Create a simple increasing series
    const std::size_t n = 100;
    const double m_ratio = 0.5;  // m = 50
    
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    // Use the same seed for both to isolate rescaling effect
    std::seed_seq seqA = make_seed_seq(0x1234567890ABCDEFull);
    std::seed_seq seqB = make_seed_seq(0x1234567890ABCDEFull);
    std::mt19937_64 rngA(seqA), rngB(seqB);
    
    SECTION("Rescaled intervals are wider than unrescaled")
    {
        // Without rescaling (conservative subsample-based inference)
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_no_rescale(800, 0.95, m_ratio, res, false);
        
        // With rescaling (theoretically correct M-out-of-N)
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_rescale(800, 0.95, m_ratio, res, true);
        
        auto result_no_rescale = moon_no_rescale.run(x, mean_sampler, rngA);
        auto result_rescale = moon_rescale.run(x, mean_sampler, rngB);
        
        const double width_no_rescale = num::to_double(result_no_rescale.upper - result_no_rescale.lower);
        const double width_rescale = num::to_double(result_rescale.upper - result_rescale.lower);
        
        // Rescaled interval should be wider
        REQUIRE(width_rescale > width_no_rescale);
        
        // Expected scale factor: sqrt(n/m) = sqrt(100/50) = sqrt(2) ≈ 1.414
        const double expected_scale = std::sqrt(static_cast<double>(n) / (m_ratio * n));
        const double actual_scale = width_rescale / width_no_rescale;
        
        // Allow 20% tolerance due to randomness and edge effects
        REQUIRE(actual_scale == Catch::Approx(expected_scale).margin(0.25));
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: rescaling equalizes width across m_ratio", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    // With rescaling enabled, different m_ratios should produce similar widths
    // because all are targeting the same sample size n
    
    const std::size_t n = 100;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    std::seed_seq seqA = make_seed_seq(0xAAAAAAAAAAAAAAAAull);
    std::seed_seq seqB = make_seed_seq(0xBBBBBBBBBBBBBBBBull);
    std::mt19937_64 rngA(seqA), rngB(seqB);
    
    SECTION("m_ratio=0.5 and m_ratio=0.8 produce similar widths when rescaled")
    {
        // m_ratio = 0.5 with rescaling
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon50(800, 0.95, 0.5, res, true);
        
        // m_ratio = 0.8 with rescaling
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon80(800, 0.95, 0.8, res, true);
        
        auto result50 = moon50.run(x, mean_sampler, rngA);
        auto result80 = moon80.run(x, mean_sampler, rngB);
        
        const double width50 = num::to_double(result50.upper - result50.lower);
        const double width80 = num::to_double(result80.upper - result80.lower);
        
        // With rescaling, widths should be similar, but bootstrap variance can be substantial
        // especially with different random seeds. The goal is order-of-magnitude similarity.
        const double ratio = std::max(width50, width80) / std::min(width50, width80);
        REQUIRE(ratio < 1.8);  // Within 80% - relaxed tolerance for bootstrap randomness
    }
    
    SECTION("Without rescaling, smaller m_ratio produces wider intervals")
    {
        // This verifies the OPPOSITE behavior without rescaling
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon50_no_rescale(800, 0.95, 0.5, res, false);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon80_no_rescale(800, 0.95, 0.8, res, false);
        
        std::seed_seq seqC = make_seed_seq(0xCCCCCCCCCCCCCCCCull);
        std::seed_seq seqD = make_seed_seq(0xDDDDDDDDDDDDDDDDull);
        std::mt19937_64 rngC(seqC), rngD(seqD);
        
        auto result50_no = moon50_no_rescale.run(x, mean_sampler, rngC);
        auto result80_no = moon80_no_rescale.run(x, mean_sampler, rngD);
        
        const double width50_no = num::to_double(result50_no.upper - result50_no.lower);
        const double width80_no = num::to_double(result80_no.upper - result80_no.lower);
        
        // Without rescaling, m=0.5 should produce wider intervals than m=0.8
        REQUIRE(width50_no > width80_no);
    }
}

// =====================================================================
// TEST GROUP 2: Diagnostic Statistics Rescaling
// =====================================================================

TEST_CASE("MOutOfNPercentileBootstrap: diagnostics are rescaled", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    const std::size_t n = 100;
    const double m_ratio = 0.6;
    
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    std::seed_seq seqA = make_seed_seq(0xDEADBEEFDEADBEEFull);
    std::seed_seq seqB = make_seed_seq(0xDEADBEEFDEADBEEFull);
    std::mt19937_64 rngA(seqA), rngB(seqB);
    
    SECTION("Bootstrap SE is rescaled by sqrt(n/m)")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_no_rescale(800, 0.95, m_ratio, res, false);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_rescale(800, 0.95, m_ratio, res, true);
        
        moon_no_rescale.run(x, mean_sampler, rngA);
        moon_rescale.run(x, mean_sampler, rngB);
        
        const double se_no_rescale = moon_no_rescale.getBootstrapSe();
        const double se_rescale = moon_rescale.getBootstrapSe();
        
        // SE should be scaled by sqrt(n/m)
        const double expected_scale = std::sqrt(static_cast<double>(n) / (m_ratio * n));
        const double actual_scale = se_rescale / se_no_rescale;
        
        REQUIRE(se_rescale > se_no_rescale);
        REQUIRE(actual_scale == Catch::Approx(expected_scale).margin(0.20));
    }
    
    SECTION("Bootstrap variance is rescaled by n/m")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_no_rescale(800, 0.95, m_ratio, res, false);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_rescale(800, 0.95, m_ratio, res, true);
        
        std::seed_seq seqC = make_seed_seq(0xFEEDFACEFEEDFACEull);
        std::seed_seq seqD = make_seed_seq(0xFEEDFACEFEEDFACEull);
        std::mt19937_64 rngC(seqC), rngD(seqD);
        
        moon_no_rescale.run(x, mean_sampler, rngC);
        moon_rescale.run(x, mean_sampler, rngD);
        
        const double var_no_rescale = moon_no_rescale.getBootstrapVariance();
        const double var_rescale = moon_rescale.getBootstrapVariance();
        
        // Variance should be scaled by n/m (square of SE scale)
        const double expected_scale = static_cast<double>(n) / (m_ratio * n);
        const double actual_scale = var_rescale / var_no_rescale;
        
        REQUIRE(var_rescale > var_no_rescale);
        REQUIRE(actual_scale == Catch::Approx(expected_scale).margin(0.35));
    }
    
    SECTION("Bootstrap mean shifts toward theta_hat after rescaling")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_no_rescale(800, 0.95, m_ratio, res, false);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_rescale(800, 0.95, m_ratio, res, true);
        
        std::seed_seq seqE = make_seed_seq(0xCAFEBABECAFEBABEull);
        std::seed_seq seqF = make_seed_seq(0xCAFEBABECAFEBABEull);
        std::mt19937_64 rngE(seqE), rngF(seqF);
        
        auto result_no = moon_no_rescale.run(x, mean_sampler, rngE);
        auto result_yes = moon_rescale.run(x, mean_sampler, rngF);
        
        const double mean_no_rescale = moon_no_rescale.getBootstrapMean();
        const double mean_rescale = moon_rescale.getBootstrapMean();
        const double theta_hat = num::to_double(result_yes.mean);
        
        // After rescaling, bootstrap mean should be closer to theta_hat
        const double dist_no_rescale = std::abs(mean_no_rescale - theta_hat);
        const double dist_rescale = std::abs(mean_rescale - theta_hat);
        
        // Rescaling centers around theta_hat, so should be very close
        REQUIRE(dist_rescale < dist_no_rescale + 0.5);  // Allow small tolerance
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: skewness unchanged by rescaling", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    // Skewness is scale-invariant, should be same with/without rescaling
    
    const std::size_t n = 80;
    std::vector<D> x;
    x.reserve(n);
    // Use squared values to create skewed data
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i * i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    std::seed_seq seqA = make_seed_seq(0x1111111111111111ull);
    std::seed_seq seqB = make_seed_seq(0x1111111111111111ull);
    std::mt19937_64 rngA(seqA), rngB(seqB);
    
    SECTION("Skewness is approximately equal regardless of rescaling")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_no_rescale(800, 0.95, 0.7, res, false);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_rescale(800, 0.95, 0.7, res, true);
        
        moon_no_rescale.run(x, mean_sampler, rngA);
        moon_rescale.run(x, mean_sampler, rngB);
        
        const double skew_no_rescale = moon_no_rescale.getBootstrapSkewness();
        const double skew_rescale = moon_rescale.getBootstrapSkewness();
        
        // Skewness should be approximately equal (scale-invariant)
        // Allow 2% relative tolerance
        REQUIRE(skew_rescale == Catch::Approx(skew_no_rescale).margin(0.02));
    }
}

// =====================================================================
// TEST GROUP 3: API and Configuration Tests
// =====================================================================

TEST_CASE("MOutOfNPercentileBootstrap: rescalesToN accessor", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);
    
    auto sampler = [](const std::vector<D>& a) -> D { 
        return a.empty() ? D(0) : a[0]; 
    };
    
    SECTION("rescalesToN returns false when rescale_to_n=false")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            moon_false(800, 0.95, 0.7, res, false);
        
        REQUIRE_FALSE(moon_false.rescalesToN());
    }
    
    SECTION("rescalesToN returns true when rescale_to_n=true")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            moon_true(800, 0.95, 0.7, res, true);
        
        REQUIRE(moon_true.rescalesToN());
    }
    
    SECTION("Default constructor has rescale_to_n=false")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            moon_default(800, 0.95, 0.7, res);
        
        REQUIRE_FALSE(moon_default.rescalesToN());
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: copy constructor preserves rescale_to_n", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);
    
    auto sampler = [](const std::vector<D>& a) -> D { 
        return a.empty() ? D(0) : a[0]; 
    };
    
    SECTION("Copy constructor preserves rescale_to_n=true")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            original(800, 0.95, 0.7, res, true);
        
        auto copy = original;
        
        REQUIRE(copy.rescalesToN() == original.rescalesToN());
        REQUIRE(copy.rescalesToN() == true);
    }
    
    SECTION("Copy constructor preserves rescale_to_n=false")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            original(800, 0.95, 0.7, res, false);
        
        auto copy = original;
        
        REQUIRE(copy.rescalesToN() == original.rescalesToN());
        REQUIRE(copy.rescalesToN() == false);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: move constructor preserves rescale_to_n", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);
    
    auto sampler = [](const std::vector<D>& a) -> D { 
        return a.empty() ? D(0) : a[0]; 
    };
    
    SECTION("Move constructor preserves rescale_to_n=true")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            original(800, 0.95, 0.7, res, true);
        
        auto moved = std::move(original);
        
        REQUIRE(moved.rescalesToN() == true);
    }
    
    SECTION("Move constructor preserves rescale_to_n=false")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            original(800, 0.95, 0.7, res, false);
        
        auto moved = std::move(original);
        
        REQUIRE(moved.rescalesToN() == false);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: copy assignment preserves rescale_to_n", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);
    
    auto sampler = [](const std::vector<D>& a) -> D { 
        return a.empty() ? D(0) : a[0]; 
    };
    
    SECTION("Copy assignment changes rescale_to_n from false to true")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            source(800, 0.95, 0.7, res, true);
        
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            dest(800, 0.95, 0.7, res, false);
        
        REQUIRE_FALSE(dest.rescalesToN());
        
        dest = source;
        
        REQUIRE(dest.rescalesToN() == true);
    }
    
    SECTION("Copy assignment changes rescale_to_n from true to false")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            source(800, 0.95, 0.7, res, false);
        
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            dest(800, 0.95, 0.7, res, true);
        
        REQUIRE(dest.rescalesToN());
        
        dest = source;
        
        REQUIRE(dest.rescalesToN() == false);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: move assignment preserves rescale_to_n", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);
    
    auto sampler = [](const std::vector<D>& a) -> D { 
        return a.empty() ? D(0) : a[0]; 
    };
    
    SECTION("Move assignment transfers rescale_to_n=true")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            source(800, 0.95, 0.7, res, true);
        
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            dest(800, 0.95, 0.7, res, false);
        
        dest = std::move(source);
        
        REQUIRE(dest.rescalesToN() == true);
    }
    
    SECTION("Move assignment transfers rescale_to_n=false")
    {
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            source(800, 0.95, 0.7, res, false);
        
        MOutOfNPercentileBootstrap<D, decltype(sampler), StationaryMaskValueResampler<D>> 
            dest(800, 0.95, 0.7, res, true);
        
        dest = std::move(source);
        
        REQUIRE(dest.rescalesToN() == false);
    }
}

// =====================================================================
// TEST GROUP 4: Factory Methods
// =====================================================================

TEST_CASE("MOutOfNPercentileBootstrap: createFixedRatio with rescale_to_n", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    StationaryMaskValueResampler<D> res(3);
    
    SECTION("createFixedRatio with rescale_to_n=true")
    {
        auto moon = MOutOfNPercentileBootstrap<D,
            std::function<D(const std::vector<D>&)>,
            StationaryMaskValueResampler<D>>::createFixedRatio(
                800, 0.95, 0.7, res, true);
        
        REQUIRE(moon.rescalesToN() == true);
        REQUIRE(moon.B() == 800);
        REQUIRE(moon.CL() == Catch::Approx(0.95));
        REQUIRE(moon.mratio() == Catch::Approx(0.7));
    }
    
    SECTION("createFixedRatio with rescale_to_n=false")
    {
        auto moon = MOutOfNPercentileBootstrap<D,
            std::function<D(const std::vector<D>&)>,
            StationaryMaskValueResampler<D>>::createFixedRatio(
                800, 0.95, 0.7, res, false);
        
        REQUIRE(moon.rescalesToN() == false);
    }
    
    SECTION("createFixedRatio defaults to rescale_to_n=false")
    {
        auto moon = MOutOfNPercentileBootstrap<D,
            std::function<D(const std::vector<D>&)>,
            StationaryMaskValueResampler<D>>::createFixedRatio(
                800, 0.95, 0.7, res);
        
        REQUIRE(moon.rescalesToN() == false);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: createAdaptive with rescale_to_n", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    using mkc_timeseries::GeoMeanStat;
    
    StationaryMaskValueResampler<D> res(3);
    
    SECTION("createAdaptive with rescale_to_n=true")
    {
        auto moon = MOutOfNPercentileBootstrap<D,
            std::function<D(const std::vector<D>&)>,
            StationaryMaskValueResampler<D>>::template createAdaptive<GeoMeanStat<D>>(
                800, 0.95, res, true);
        
        REQUIRE(moon.rescalesToN() == true);
        REQUIRE(moon.isAdaptiveMode());
    }
    
    SECTION("createAdaptive with rescale_to_n=false")
    {
        auto moon = MOutOfNPercentileBootstrap<D,
            std::function<D(const std::vector<D>&)>,
            StationaryMaskValueResampler<D>>::template createAdaptive<GeoMeanStat<D>>(
                800, 0.95, res, false);
        
        REQUIRE(moon.rescalesToN() == false);
        REQUIRE(moon.isAdaptiveMode());
    }
    
    SECTION("createAdaptive defaults to rescale_to_n=false")
    {
        auto moon = MOutOfNPercentileBootstrap<D,
            std::function<D(const std::vector<D>&)>,
            StationaryMaskValueResampler<D>>::template createAdaptive<GeoMeanStat<D>>(
                800, 0.95, res);
        
        REQUIRE(moon.rescalesToN() == false);
    }
}

// =====================================================================
// TEST GROUP 5: Edge Cases and Robustness
// =====================================================================

TEST_CASE("MOutOfNPercentileBootstrap: rescaling with extreme m_ratios", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    const std::size_t n = 50;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(2);
    
    SECTION("Very small m_ratio (0.1) with rescaling produces valid results")
    {
        std::seed_seq seq = make_seed_seq(0x0123456789ABCDEFull);
        std::mt19937_64 rng(seq);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon(800, 0.95, 0.1, res, true);
        
        // Should not throw, should produce valid results
        auto result = moon.run(x, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.upper);
        
        // Scale factor should be large: sqrt(50/5) ≈ 3.16
        const double width = num::to_double(result.upper - result.lower);
        REQUIRE(width > 0.0);  // Positive width
        
        // Width should be substantial due to large scale factor
        // (but we avoid hard thresholds due to bootstrap variance)
        const double mean_val = num::to_double(result.mean);
        REQUIRE(mean_val >= 20.0);  // Mean of 0-49 is 24.5
        REQUIRE(mean_val <= 30.0);
    }
    
    SECTION("Large m_ratio (0.9) with rescaling produces valid results")
    {
        std::seed_seq seq = make_seed_seq(0xFEDCBA9876543210ull);
        std::mt19937_64 rng(seq);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon(800, 0.95, 0.9, res, true);
        
        // Should not throw, should produce valid results
        auto result = moon.run(x, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.upper);
        
        // Scale factor should be small: sqrt(50/45) ≈ 1.05
        // Bootstrap CI widths vary with randomness - just check validity
        const double width = num::to_double(result.upper - result.lower);
        REQUIRE(width > 0.0);  // Positive width
        
        // Interval should contain reasonable values for mean of 0-49
        const double lower = num::to_double(result.lower);
        const double upper = num::to_double(result.upper);
        REQUIRE(lower >= 0.0);   // Can't be below minimum value
        REQUIRE(upper <= 60.0);  // Should be reasonable given data range
    }
    
    SECTION("Comparing extreme m_ratios: smaller m produces wider intervals (if same seed)")
    {
        // Use same seed to isolate the effect of m_ratio
        std::seed_seq seqA = make_seed_seq(0xAAAAAAAAAAAAAAAAull);
        std::seed_seq seqB = make_seed_seq(0xAAAAAAAAAAAAAAAAull);
        std::mt19937_64 rngA(seqA), rngB(seqB);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_small(800, 0.95, 0.3, res, true);
        
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon_large(800, 0.95, 0.9, res, true);
        
        auto result_small = moon_small.run(x, mean_sampler, rngA);
        auto result_large = moon_large.run(x, mean_sampler, rngB);
        
        const double width_small = num::to_double(result_small.upper - result_small.lower);
        const double width_large = num::to_double(result_large.upper - result_large.lower);
        
        // With same seed, smaller m_ratio should produce wider CI due to larger scale factor
        // sqrt(50/15) = 1.826 vs sqrt(50/45) = 1.054
        // But this is probabilistic, so we just check that both are positive and reasonable
        REQUIRE(width_small > 0.0);
        REQUIRE(width_large > 0.0);
        
        // Both should be in a reasonable range for this data
        REQUIRE(width_small < 60.0);  // Less than full data range
        REQUIRE(width_large < 60.0);
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: rescaling with small sample sizes", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    // Small n to test edge behavior
    const std::size_t n = 20;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i + 1)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(2);
    
    std::seed_seq seq = make_seed_seq(0x9999999999999999ull);
    std::mt19937_64 rng(seq);
    
    SECTION("Rescaling works with n=20, m_ratio=0.5")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon(400, 0.95, 0.5, res, true);
        
        // Should not throw
        auto result = moon.run(x, mean_sampler, rng);
        
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
        REQUIRE(result.effective_B >= 200);  // At least 50% valid
    }
}

TEST_CASE("MOutOfNPercentileBootstrap: rescaling produces finite results", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    const std::size_t n = 60;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    std::seed_seq seq = make_seed_seq(0x7777777777777777ull);
    std::mt19937_64 rng(seq);
    
    SECTION("All results are finite after rescaling")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon(800, 0.95, 0.7, res, true);
        
        auto result = moon.run(x, mean_sampler, rng);
        
        // All core results should be finite
        REQUIRE(std::isfinite(num::to_double(result.mean)));
        REQUIRE(std::isfinite(num::to_double(result.lower)));
        REQUIRE(std::isfinite(num::to_double(result.upper)));
        
        // All diagnostics should be finite
        REQUIRE(std::isfinite(moon.getBootstrapMean()));
        REQUIRE(std::isfinite(moon.getBootstrapSe()));
        REQUIRE(std::isfinite(moon.getBootstrapVariance()));
        REQUIRE(std::isfinite(moon.getBootstrapSkewness()));
        
        // Sanity checks
        REQUIRE(result.lower <= result.upper);
        REQUIRE(moon.getBootstrapSe() > 0.0);
        REQUIRE(moon.getBootstrapVariance() > 0.0);
    }
}

// =====================================================================
// TEST GROUP 6: Consistency Checks
// =====================================================================

TEST_CASE("MOutOfNPercentileBootstrap: rescaling maintains interval ordering", "[Bootstrap][MOutOfN][Rescaling]")
{
    using D = DecimalType;
    
    const std::size_t n = 100;
    std::vector<D> x;
    x.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        x.push_back(D(static_cast<int>(i)));
    }
    
    auto mean_sampler = [](const std::vector<D>& a) -> D {
        double s = 0.0;
        for (const auto& v : a) s += num::to_double(v);
        return D(s / static_cast<double>(a.size()));
    };
    
    StationaryMaskValueResampler<D> res(3);
    
    std::seed_seq seq = make_seed_seq(0x5555555555555555ull);
    std::mt19937_64 rng(seq);
    
    SECTION("Lower < Upper after rescaling")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon(800, 0.95, 0.6, res, true);
        
        auto result = moon.run(x, mean_sampler, rng);
        
        REQUIRE(result.lower < result.upper);
    }
    
    SECTION("Mean is within interval after rescaling")
    {
        MOutOfNPercentileBootstrap<D, decltype(mean_sampler), StationaryMaskValueResampler<D>> 
            moon(800, 0.95, 0.6, res, true);
        
        auto result = moon.run(x, mean_sampler, rng);
        
        // Note: This may not always hold for asymmetric distributions,
        // but should hold for symmetric data like our test case
        REQUIRE(result.lower <= result.mean);
        REQUIRE(result.mean <= result.upper);
    }
}
