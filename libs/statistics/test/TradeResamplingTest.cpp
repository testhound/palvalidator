// Unit tests for Trade and TradeFlatteningAdapter classes
//
// Tests the trade-level resampling infrastructure for bootstrap analysis

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TradeResampling.h"
#include "TestUtils.h"
#include "number.h"

using namespace mkc_timeseries;

using D = DecimalType;

// ============================================================================
// Trade Class Tests
// ============================================================================

TEST_CASE("Trade::construction with single return", "[Trade][Construction]")
{
    std::vector<D> returns = {D("0.05")};
    Trade<D> trade(returns);
    
    REQUIRE(trade.getDuration() == 1);
    REQUIRE(trade.getDailyReturns().size() == 1);
    REQUIRE(num::to_double(trade.getDailyReturns()[0]) == Catch::Approx(0.05).epsilon(1e-9));
}

TEST_CASE("Trade::construction with multiple returns", "[Trade][Construction]")
{
    std::vector<D> returns = {D("0.02"), D("0.03"), D("-0.01")};
    Trade<D> trade(returns);
    
    REQUIRE(trade.getDuration() == 3);
    REQUIRE(trade.getDailyReturns().size() == 3);
    REQUIRE(num::to_double(trade.getDailyReturns()[0]) == Catch::Approx(0.02).epsilon(1e-9));
    REQUIRE(num::to_double(trade.getDailyReturns()[1]) == Catch::Approx(0.03).epsilon(1e-9));
    REQUIRE(num::to_double(trade.getDailyReturns()[2]) == Catch::Approx(-0.01).epsilon(1e-9));
}

TEST_CASE("Trade::construction with empty vector", "[Trade][Construction][Edge]")
{
    std::vector<D> returns;
    Trade<D> trade(returns);
    
    REQUIRE(trade.getDuration() == 0);
    REQUIRE(trade.getDailyReturns().empty());
}

TEST_CASE("Trade::construction with move semantics", "[Trade][Construction]")
{
    std::vector<D> returns = {D("0.01"), D("0.02"), D("0.03")};
    auto originalSize = returns.size();
    
    Trade<D> trade(std::move(returns));
    
    REQUIRE(trade.getDuration() == originalSize);
    // Original vector should be moved-from (implementation dependent, but typically empty)
}

TEST_CASE("Trade::getDailyReturns returns const reference", "[Trade][Accessor]")
{
    std::vector<D> returns = {D("0.01"), D("0.02")};
    Trade<D> trade(returns);
    
    const std::vector<D>& returnRef = trade.getDailyReturns();
    
    // Verify it's a const reference, not a copy
    REQUIRE(&returnRef == &trade.getDailyReturns());
}

TEST_CASE("Trade::getDuration for various trade lengths", "[Trade][Duration]")
{
    SECTION("Single bar") {
        Trade<D> trade({D("0.05")});
        REQUIRE(trade.getDuration() == 1);
    }
    
    SECTION("Two bars") {
        Trade<D> trade({D("0.02"), D("0.03")});
        REQUIRE(trade.getDuration() == 2);
    }
    
    SECTION("Eight bars (maximum typical)") {
        Trade<D> trade({
            D("0.01"), D("0.02"), D("0.03"), D("0.04"),
            D("0.05"), D("0.06"), D("0.07"), D("0.08")
        });
        REQUIRE(trade.getDuration() == 8);
    }
    
    SECTION("Empty trade") {
        Trade<D> trade(std::vector<D>{});
        REQUIRE(trade.getDuration() == 0);
    }
}

TEST_CASE("Trade::operator== for equality comparison", "[Trade][Equality]")
{
    SECTION("Identical trades are equal") {
        Trade<D> trade1({D("0.02"), D("0.03"), D("-0.01")});
        Trade<D> trade2({D("0.02"), D("0.03"), D("-0.01")});
        
        REQUIRE(trade1 == trade2);
        REQUIRE(trade2 == trade1);
    }
    
    SECTION("Different returns are not equal") {
        Trade<D> trade1({D("0.02"), D("0.03")});
        Trade<D> trade2({D("0.02"), D("0.04")});
        
        REQUIRE_FALSE(trade1 == trade2);
    }
    
    SECTION("Different lengths are not equal") {
        Trade<D> trade1({D("0.02"), D("0.03")});
        Trade<D> trade2({D("0.02"), D("0.03"), D("0.04")});
        
        REQUIRE_FALSE(trade1 == trade2);
    }
    
    SECTION("Empty trades are equal") {
        Trade<D> trade1(std::vector<D>{});
        Trade<D> trade2(std::vector<D>{});
        
        REQUIRE(trade1 == trade2);
    }
    
    SECTION("Trade equals itself") {
        Trade<D> trade({D("0.05")});
        REQUIRE(trade == trade);
    }
}

TEST_CASE("Trade::operator< for comparison", "[Trade][Comparison]")
{
    SECTION("Trade with smaller sum is less than") {
        Trade<D> trade1({D("0.01"), D("0.02")});  // sum = 0.03
        Trade<D> trade2({D("0.02"), D("0.03")});  // sum = 0.05
        
        REQUIRE(trade1 < trade2);
        REQUIRE_FALSE(trade2 < trade1);
    }
    
    SECTION("Trade with negative sum vs positive sum") {
        Trade<D> trade1({D("-0.02"), D("-0.03")});  // sum = -0.05
        Trade<D> trade2({D("0.01"), D("0.02")});    // sum = 0.03
        
        REQUIRE(trade1 < trade2);
        REQUIRE_FALSE(trade2 < trade1);
    }
    
    SECTION("Trades with equal sums are not less than") {
        Trade<D> trade1({D("0.05")});
        Trade<D> trade2({D("0.02"), D("0.03")});  // sum = 0.05
        
        // Neither should be less than the other
        REQUIRE_FALSE(trade1 < trade2);
        REQUIRE_FALSE(trade2 < trade1);
    }
    
    SECTION("Empty trade comparison") {
        Trade<D> emptyTrade(std::vector<D>{});
        Trade<D> nonEmptyTrade({D("0.01")});
        
        REQUIRE(emptyTrade < nonEmptyTrade);
        REQUIRE_FALSE(nonEmptyTrade < emptyTrade);
    }
    
    SECTION("Different lengths, same sum") {
        Trade<D> trade1({D("0.06")});                    // 1 bar, sum = 0.06
        Trade<D> trade2({D("0.02"), D("0.02"), D("0.02")});  // 3 bars, sum = 0.06
        
        // Equal sums, so neither is less than
        REQUIRE_FALSE(trade1 < trade2);
        REQUIRE_FALSE(trade2 < trade1);
    }
}

TEST_CASE("Trade::with zero returns", "[Trade][ZeroReturns]")
{
    SECTION("All zero returns") {
        Trade<D> trade({D("0.0"), D("0.0"), D("0.0")});
        
        REQUIRE(trade.getDuration() == 3);
        for (const auto& ret : trade.getDailyReturns()) {
            REQUIRE(num::to_double(ret) == Catch::Approx(0.0).epsilon(1e-12));
        }
    }
    
    SECTION("Mixed with zeros") {
        Trade<D> trade({D("0.02"), D("0.0"), D("0.03")});
        
        REQUIRE(trade.getDuration() == 3);
        REQUIRE(num::to_double(trade.getDailyReturns()[1]) == Catch::Approx(0.0).epsilon(1e-12));
    }
}

TEST_CASE("Trade::copy semantics", "[Trade][CopySemantics]")
{
    Trade<D> original({D("0.01"), D("0.02"), D("0.03")});
    Trade<D> copy = original;
    
    // Verify copy has same values
    REQUIRE(copy == original);
    REQUIRE(copy.getDuration() == original.getDuration());
    
    // Verify they're independent (different storage)
    REQUIRE(&copy.getDailyReturns() != &original.getDailyReturns());
}

// ============================================================================
// TradeFlatteningAdapter Tests
// ============================================================================

TEST_CASE("TradeFlatteningAdapter::construction with statistic function", 
          "[TradeFlatteningAdapter][Construction]")
{
    auto meanFunc = [](const std::vector<D>& v) -> D {
        if (v.empty()) return D(0);
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum / v.size());
    };
    
    TradeFlatteningAdapter<D> adapter(meanFunc);
    
    // Just verify construction succeeds
    REQUIRE(true);
}

TEST_CASE("TradeFlatteningAdapter::flatten and apply mean to single trade",
          "[TradeFlatteningAdapter][SingleTrade]")
{
    auto meanFunc = [](const std::vector<D>& v) -> D {
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum / v.size());
    };
    
    TradeFlatteningAdapter<D> adapter(meanFunc);
    
    Trade<D> trade({D("0.02"), D("0.04"), D("0.06")});
    std::vector<Trade<D>> trades = {trade};
    
    D result = adapter(trades);
    
    // Mean of [0.02, 0.04, 0.06] = 0.04
    REQUIRE(num::to_double(result) == Catch::Approx(0.04).epsilon(1e-9));
}

TEST_CASE("TradeFlatteningAdapter::flatten and apply mean to multiple trades",
          "[TradeFlatteningAdapter][MultipleTrades]")
{
    auto meanFunc = [](const std::vector<D>& v) -> D {
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum / v.size());
    };
    
    TradeFlatteningAdapter<D> adapter(meanFunc);
    
    Trade<D> trade1({D("0.02"), D("0.04")});
    Trade<D> trade2({D("0.06"), D("0.08")});
    Trade<D> trade3({D("0.10")});
    std::vector<Trade<D>> trades = {trade1, trade2, trade3};
    
    D result = adapter(trades);
    
    // Flattened: [0.02, 0.04, 0.06, 0.08, 0.10]
    // Mean = 0.30 / 5 = 0.06
    REQUIRE(num::to_double(result) == Catch::Approx(0.06).epsilon(1e-9));
}

TEST_CASE("TradeFlatteningAdapter::flatten and apply sum",
          "[TradeFlatteningAdapter][Sum]")
{
    auto sumFunc = [](const std::vector<D>& v) -> D {
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum);
    };
    
    TradeFlatteningAdapter<D> adapter(sumFunc);
    
    Trade<D> trade1({D("0.10"), D("0.20")});
    Trade<D> trade2({D("0.30")});
    std::vector<Trade<D>> trades = {trade1, trade2};
    
    D result = adapter(trades);
    
    // Sum of [0.10, 0.20, 0.30] = 0.60
    REQUIRE(num::to_double(result) == Catch::Approx(0.60).epsilon(1e-9));
}

TEST_CASE("TradeFlatteningAdapter::flatten preserves order",
          "[TradeFlatteningAdapter][Order]")
{
    // Statistic that returns the first element
    auto firstFunc = [](const std::vector<D>& v) -> D {
        return v.empty() ? D(0) : v[0];
    };
    
    TradeFlatteningAdapter<D> adapter(firstFunc);
    
    Trade<D> trade1({D("0.11"), D("0.22")});
    Trade<D> trade2({D("0.33"), D("0.44")});
    std::vector<Trade<D>> trades = {trade1, trade2};
    
    D result = adapter(trades);
    
    // First element of flattened [0.11, 0.22, 0.33, 0.44] is 0.11
    REQUIRE(num::to_double(result) == Catch::Approx(0.11).epsilon(1e-9));
}

TEST_CASE("TradeFlatteningAdapter::with empty trade vector",
          "[TradeFlatteningAdapter][Edge]")
{
    auto meanFunc = [](const std::vector<D>& v) -> D {
        if (v.empty()) return D(0);
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum / v.size());
    };
    
    TradeFlatteningAdapter<D> adapter(meanFunc);
    
    std::vector<Trade<D>> emptyTrades;
    D result = adapter(emptyTrades);
    
    // Empty input should produce 0
    REQUIRE(num::to_double(result) == Catch::Approx(0.0).epsilon(1e-12));
}

TEST_CASE("TradeFlatteningAdapter::with trades of varying durations",
          "[TradeFlatteningAdapter][VaryingDurations]")
{
    auto countFunc = [](const std::vector<D>& v) -> D {
        return D(static_cast<double>(v.size()));
    };
    
    TradeFlatteningAdapter<D> adapter(countFunc);
    
    Trade<D> trade1({D("0.01")});                           // 1 bar
    Trade<D> trade2({D("0.02"), D("0.03")});                // 2 bars
    Trade<D> trade3({D("0.04"), D("0.05"), D("0.06")});     // 3 bars
    std::vector<Trade<D>> trades = {trade1, trade2, trade3};
    
    D result = adapter(trades);
    
    // Total bars: 1 + 2 + 3 = 6
    REQUIRE(num::to_double(result) == Catch::Approx(6.0).epsilon(1e-9));
}

TEST_CASE("TradeFlatteningAdapter::with negative returns",
          "[TradeFlatteningAdapter][NegativeReturns]")
{
    auto sumFunc = [](const std::vector<D>& v) -> D {
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum);
    };
    
    TradeFlatteningAdapter<D> adapter(sumFunc);
    
    Trade<D> trade1({D("0.05"), D("-0.03")});
    Trade<D> trade2({D("-0.02"), D("0.04")});
    std::vector<Trade<D>> trades = {trade1, trade2};
    
    D result = adapter(trades);
    
    // Sum: 0.05 - 0.03 - 0.02 + 0.04 = 0.04
    REQUIRE(num::to_double(result) == Catch::Approx(0.04).epsilon(1e-9));
}

TEST_CASE("TradeFlatteningAdapter::geometric mean simulation",
          "[TradeFlatteningAdapter][GeometricMean]")
{
    // Simplified geometric mean: product of (1 + r) then take nth root
    auto geometricMeanFunc = [](const std::vector<D>& v) -> D {
        if (v.empty()) return D(0);
        double product = 1.0;
        for (const auto& r : v) {
            product *= (1.0 + num::to_double(r));
        }
        double geoMean = std::pow(product, 1.0 / v.size()) - 1.0;
        return D(geoMean);
    };
    
    TradeFlatteningAdapter<D> adapter(geometricMeanFunc);
    
    Trade<D> trade1({D("0.10"), D("0.05")});
    Trade<D> trade2({D("0.08")});
    std::vector<Trade<D>> trades = {trade1, trade2};
    
    D result = adapter(trades);
    
    // Flattened: [0.10, 0.05, 0.08]
    // Product: 1.10 * 1.05 * 1.08 = 1.2474
    // Geometric mean: 1.2474^(1/3) - 1 ≈ 0.0765
    REQUIRE(num::to_double(result) == Catch::Approx(0.07647).epsilon(1e-5));
}

TEST_CASE("TradeFlatteningAdapter::profit factor simulation",
          "[TradeFlatteningAdapter][ProfitFactor]")
{
    // Profit factor: sum of gains / abs(sum of losses)
    auto profitFactorFunc = [](const std::vector<D>& v) -> D {
        double gains = 0.0;
        double losses = 0.0;
        for (const auto& r : v) {
            double ret = num::to_double(r);
            if (ret > 0.0) gains += ret;
            else losses += ret;
        }
        if (losses == 0.0) return D(0);  // Avoid division by zero
        return D(gains / std::abs(losses));
    };
    
    TradeFlatteningAdapter<D> adapter(profitFactorFunc);
    
    Trade<D> trade1({D("0.05"), D("-0.02")});
    Trade<D> trade2({D("0.03"), D("-0.01")});
    std::vector<Trade<D>> trades = {trade1, trade2};
    
    D result = adapter(trades);
    
    // Gains: 0.05 + 0.03 = 0.08
    // Losses: -0.02 - 0.01 = -0.03
    // PF: 0.08 / 0.03 = 2.666...
    REQUIRE(num::to_double(result) == Catch::Approx(2.66667).epsilon(1e-4));
}

TEST_CASE("TradeFlatteningAdapter::multiple calls produce consistent results",
          "[TradeFlatteningAdapter][Consistency]")
{
    auto meanFunc = [](const std::vector<D>& v) -> D {
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum / v.size());
    };
    
    TradeFlatteningAdapter<D> adapter(meanFunc);
    
    Trade<D> trade1({D("0.02"), D("0.04")});
    Trade<D> trade2({D("0.06")});
    std::vector<Trade<D>> trades = {trade1, trade2};
    
    D result1 = adapter(trades);
    D result2 = adapter(trades);
    D result3 = adapter(trades);
    
    // All calls should produce identical results
    REQUIRE(num::to_double(result1) == num::to_double(result2));
    REQUIRE(num::to_double(result2) == num::to_double(result3));
}

TEST_CASE("TradeFlatteningAdapter::with single-bar trades",
          "[TradeFlatteningAdapter][SingleBarTrades]")
{
    auto sumFunc = [](const std::vector<D>& v) -> D {
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum);
    };
    
    TradeFlatteningAdapter<D> adapter(sumFunc);
    
    // Multiple single-bar trades (e.g., same-day positions)
    Trade<D> trade1({D("0.02")});
    Trade<D> trade2({D("0.03")});
    Trade<D> trade3({D("0.05")});
    std::vector<Trade<D>> trades = {trade1, trade2, trade3};
    
    D result = adapter(trades);
    
    // Sum: 0.02 + 0.03 + 0.05 = 0.10
    REQUIRE(num::to_double(result) == Catch::Approx(0.10).epsilon(1e-9));
}

TEST_CASE("TradeFlatteningAdapter::integration with realistic trade data",
          "[TradeFlatteningAdapter][Integration]")
{
    // Realistic scenario: mix of winning and losing trades
    auto meanFunc = [](const std::vector<D>& v) -> D {
        double sum = 0.0;
        for (const auto& val : v) sum += num::to_double(val);
        return D(sum / v.size());
    };
    
    TradeFlatteningAdapter<D> adapter(meanFunc);
    
    // Winner: 3-bar trade
    Trade<D> winner1({D("0.02"), D("0.03"), D("0.01")});
    
    // Loser: 2-bar trade
    Trade<D> loser1({D("-0.01"), D("-0.02")});
    
    // Winner: 1-bar trade
    Trade<D> winner2({D("0.04")});
    
    // Loser: 2-bar trade
    Trade<D> loser2({D("-0.01"), D("-0.01")});
    
    std::vector<Trade<D>> trades = {winner1, loser1, winner2, loser2};
    
    D result = adapter(trades);
    
    // Flattened: [0.02, 0.03, 0.01, -0.01, -0.02, 0.04, -0.01, -0.01]
    // Sum: 0.05
    // Count: 8
    // Mean: 0.05 / 8 = 0.00625
    REQUIRE(num::to_double(result) == Catch::Approx(0.00625).epsilon(1e-9));
}
