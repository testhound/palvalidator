// RegimeMixAndConfigTest.cpp
//
// Unit tests for palvalidator::analysis::RegimeMix and RegimeMixConfig using Catch2.
//
// Contract covered:
//
//  RegimeMix
//  ---------
//  - Stores name and weights verbatim (no normalization/validation inside RegimeMix).
//  - Allows empty weights (validation is deferred to higher layers).
//  - Copy/move semantics behave as expected.
//  - Accessors are const; weights() returns a const reference.
//
//  RegimeMixConfig
//  ---------------
//  - Requires a non-empty set of mixes.
//  - Requires minPassFraction in (0, 1] (both >0 and ≤1).
//  - Stores minBarsPerRegime verbatim (the runner/resampler interpret it).
//  - Preserves order and content of mixes.
//  - Copy/move semantics and const ref accessors.
//
// Dependencies:
//  - RegimeMixStress.h
//  - Catch2

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

#include "RegimeMixStress.h"

using palvalidator::analysis::RegimeMix;
using palvalidator::analysis::RegimeMixConfig;

TEST_CASE("RegimeMix: stores name and weights verbatim", "[RegimeMix]")
{
    std::string name = "Equal(1/3,1/3,1/3)";
    std::vector<double> w = { 1.0/3.0, 1.0/3.0, 1.0/3.0 };

    RegimeMix mix(name, w);

    REQUIRE(mix.name() == name);
    REQUIRE(mix.weights().size() == w.size());
    for (std::size_t i = 0; i < w.size(); ++i)
    {
        REQUIRE(mix.weights()[i] == Catch::Approx(w[i]).margin(1e-12));
    }
}

TEST_CASE("RegimeMix: allows empty weights (by design)", "[RegimeMix]")
{
    RegimeMix mix("Empty", {});
    REQUIRE(mix.name() == "Empty");
    REQUIRE(mix.weights().empty());
}

TEST_CASE("RegimeMix: copy construction and copy assignment", "[RegimeMix]")
{
    RegimeMix a("DownFav(0.3,0.4,0.3)", {0.3, 0.4, 0.3});

    // Copy construct
    RegimeMix b = a;
    REQUIRE(b.name() == a.name());
    REQUIRE(b.weights().size() == a.weights().size());
    for (std::size_t i = 0; i < a.weights().size(); ++i)
    {
        REQUIRE(b.weights()[i] == Catch::Approx(a.weights()[i]).margin(1e-12));
    }

    // Copy assign
    RegimeMix c("Other", {0.2, 0.5, 0.3});
    c = a;
    REQUIRE(c.name() == a.name());
    REQUIRE(c.weights().size() == a.weights().size());
    for (std::size_t i = 0; i < a.weights().size(); ++i)
    {
        REQUIRE(c.weights()[i] == Catch::Approx(a.weights()[i]).margin(1e-12));
    }
}

TEST_CASE("RegimeMix: move construction and move assignment", "[RegimeMix]")
{
    // Move construct
    {
        std::string nm = "MoveCtor";
        std::vector<double> w = {0.1, 0.2, 0.7};
        RegimeMix src(std::move(nm), std::move(w));
        RegimeMix dst(std::move(src));

        REQUIRE(dst.name() == "MoveCtor");
        REQUIRE(dst.weights().size() == 3);
        REQUIRE(dst.weights()[0] == Catch::Approx(0.1));
        REQUIRE(dst.weights()[1] == Catch::Approx(0.2));
        REQUIRE(dst.weights()[2] == Catch::Approx(0.7));
    }

    // Move assign
    {
        RegimeMix dst("Init", {0.5});
        RegimeMix tmp("MoveAssign", {0.25, 0.25, 0.5});
        dst = std::move(tmp);

        REQUIRE(dst.name() == "MoveAssign");
        REQUIRE(dst.weights().size() == 3);
        REQUIRE(dst.weights()[0] == Catch::Approx(0.25));
        REQUIRE(dst.weights()[1] == Catch::Approx(0.25));
        REQUIRE(dst.weights()[2] == Catch::Approx(0.5));
    }
}

TEST_CASE("RegimeMix: accessor types are const references", "[RegimeMix]")
{
    RegimeMix mix("ConstAccess", {0.2, 0.5, 0.3});

    // name() returns const std::string&
    static_assert(std::is_same_v<decltype(mix.name()), const std::string &>,
                  "RegimeMix::name() should return const std::string&");

    // weights() returns const std::vector<double>&
    static_assert(std::is_same_v<decltype(mix.weights()), const std::vector<double> &>,
                  "RegimeMix::weights() should return const std::vector<double>&");

    REQUIRE(mix.name() == "ConstAccess");
    REQUIRE(mix.weights().size() == 3);
    REQUIRE(mix.weights()[1] == Catch::Approx(0.5));
}

//
// RegimeMixConfig tests
//

TEST_CASE("RegimeMixConfig: constructor validation", "[RegimeMixConfig]")
{
    std::vector<RegimeMix> mixes =
    {
        RegimeMix("Equal",   {1.0/3.0, 1.0/3.0, 1.0/3.0}),
        RegimeMix("DownFav", {0.30, 0.40, 0.30})
    };

    SECTION("Requires non-empty mix set")
    {
        REQUIRE_THROWS_AS(RegimeMixConfig({}, /*minPassFraction=*/0.5, /*minBarsPerRegime=*/10),
                          std::invalid_argument);
    }

    SECTION("Requires minPassFraction in (0,1]")
    {
        REQUIRE_THROWS_AS(RegimeMixConfig(mixes, 0.0, 10), std::invalid_argument);
        REQUIRE_THROWS_AS(RegimeMixConfig(mixes, -0.1, 10), std::invalid_argument);
        REQUIRE_THROWS_AS(RegimeMixConfig(mixes, 1.0 + 1e-12, 10), std::invalid_argument);

        // Boundaries: >0 and ≤1 are allowed
        REQUIRE_NOTHROW(RegimeMixConfig(mixes, 1e-6, 10));
        REQUIRE_NOTHROW(RegimeMixConfig(mixes, 1.0, 10));
    }

    SECTION("minBarsPerRegime is stored verbatim (no validation here)")
    {
        RegimeMixConfig cfg(mixes, 0.66, 0);
        REQUIRE(cfg.minBarsPerRegime() == 0);

        RegimeMixConfig cfg2(mixes, 0.66, 17);
        REQUIRE(cfg2.minBarsPerRegime() == 17);
    }
}

TEST_CASE("RegimeMixConfig: preserves mix order and content", "[RegimeMixConfig]")
{
    std::vector<RegimeMix> mixes;
    mixes.emplace_back("Equal",   std::vector<double>{1.0/3.0, 1.0/3.0, 1.0/3.0});
    mixes.emplace_back("DownFav", std::vector<double>{0.30, 0.40, 0.30});
    mixes.emplace_back("SkewLow", std::vector<double>{0.50, 0.30, 0.20});

    RegimeMixConfig cfg(mixes, /*minPassFraction=*/2.0/3.0, /*minBarsPerRegime=*/15);

    const auto &mx = cfg.mixes();
    REQUIRE(mx.size() == mixes.size());

    for (std::size_t i = 0; i < mixes.size(); ++i)
    {
        REQUIRE(mx[i].name() == mixes[i].name());
        REQUIRE(mx[i].weights().size() == mixes[i].weights().size());
        for (std::size_t j = 0; j < mixes[i].weights().size(); ++j)
        {
            REQUIRE(mx[i].weights()[j] == Catch::Approx(mixes[i].weights()[j]).margin(1e-12));
        }
    }

    REQUIRE(cfg.minPassFraction() == Catch::Approx(2.0 / 3.0));
    REQUIRE(cfg.minBarsPerRegime() == 15);
}

TEST_CASE("RegimeMixConfig: copy and move semantics; const-ref accessors", "[RegimeMixConfig]")
{
    std::vector<RegimeMix> mixes;
    mixes.emplace_back("A", std::vector<double>{0.25, 0.50, 0.25});
    mixes.emplace_back("B", std::vector<double>{0.30, 0.40, 0.30});

    RegimeMixConfig cfg(mixes, 0.75, 11);

    // Accessor types (const-ref)
    static_assert(std::is_same_v<decltype(cfg.mixes()), const std::vector<RegimeMix> &>,
                  "RegimeMixConfig::mixes() should return const std::vector<RegimeMix>&");
    static_assert(std::is_same_v<decltype(cfg.minPassFraction()), double>,
                  "RegimeMixConfig::minPassFraction() should return double by value");
    static_assert(std::is_same_v<decltype(cfg.minBarsPerRegime()), std::size_t>,
                  "RegimeMixConfig::minBarsPerRegime() should return size_t by value");

    // Copy construct
    RegimeMixConfig copyCfg = cfg;
    REQUIRE(copyCfg.minPassFraction() == Catch::Approx(cfg.minPassFraction()));
    REQUIRE(copyCfg.minBarsPerRegime() == cfg.minBarsPerRegime());
    REQUIRE(copyCfg.mixes().size() == cfg.mixes().size());
    for (std::size_t i = 0; i < cfg.mixes().size(); ++i)
    {
        REQUIRE(copyCfg.mixes()[i].name() == cfg.mixes()[i].name());
    }

    // Move construct / assign
    RegimeMixConfig moveDst({RegimeMix("X", {1.0})}, 1.0, 1);
    RegimeMixConfig tmp(mixes, 0.9, 9);
    moveDst = std::move(tmp);
    REQUIRE(moveDst.mixes().size() == mixes.size());
    REQUIRE(moveDst.minPassFraction() == Catch::Approx(0.9));
    REQUIRE(moveDst.minBarsPerRegime() == 9);
}
