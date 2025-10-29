// BCaBootStrapTest.cpp
//
// Unit tests for the BCaBootStrap class and resampling policies.
// Uses Catch2.
//
// New in this version:
//  - Tests for StationaryBlockResampler (policy-only & via BCaBootStrap)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

#include "BiasCorrectedBootstrap.h"
#include "TestUtils.h"
#include "number.h"
#include "randutils.hpp"
#include "ClosedPositionHistory.h"
#include "MonthlyReturnsBuilder.h"
#include "BoundFutureReturns.h"
#include "BoostDateHelper.h"
#include "TradingPosition.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

// Symbol constant used in tests
const static std::string myCornSymbol("@C");

TEST_CASE("createSliceIndicesForBootstrap Tests", "[Slicer]")
{
    using D = DecimalType;
    using SliceVector = std::vector<std::pair<std::size_t, std::size_t>>;

    SECTION("Failure modes return an empty vector")
    {
        // K < 2 is invalid
        std::vector<D> v10(10);
        REQUIRE(createSliceIndicesForBootstrap(v10, 1, 2).empty());

        // n < 2 is invalid
        std::vector<D> v1(1);
        REQUIRE(createSliceIndicesForBootstrap(v1, 2, 1).empty());

        // n < K * minLen is invalid
        std::vector<D> v19(19);
        REQUIRE(createSliceIndicesForBootstrap(v19, 10, 2).empty()); // 19 < 10*2

        // Empty vector
        std::vector<D> v0;
        REQUIRE(createSliceIndicesForBootstrap(v0, 2, 1).empty());
    }

    SECTION("Perfectly divisible input")
    {
        std::vector<D> v(100);
        std::size_t K = 5;
        std::size_t minLen = 10;
        auto slices = createSliceIndicesForBootstrap(v, K, minLen);

        REQUIRE(slices.size() == K);

        SliceVector expected = {{0, 20}, {20, 40}, {40, 60}, {60, 80}, {80, 100}};
        REQUIRE(slices == expected);

        // Check total coverage
        REQUIRE(slices.front().first == 0);
        REQUIRE(slices.back().second == v.size());
    }

    SECTION("Unevenly divisible input (remainder case)")
    {
        // n=10, K=3. base=3, rem=1.
        // First slice gets base+1=4, rest get base=3.
        std::vector<D> v(10);
        std::size_t K = 3;
        std::size_t minLen = 2;
        auto slices = createSliceIndicesForBootstrap(v, K, minLen);

        REQUIRE(slices.size() == K);

        SliceVector expected = {{0, 4}, {4, 7}, {7, 10}};
        REQUIRE(slices == expected);

        // Check slice lengths
        REQUIRE((slices[0].second - slices[0].first) == 4);
        REQUIRE((slices[1].second - slices[1].first) == 3);
        REQUIRE((slices[2].second - slices[2].first) == 3);

        // Check total coverage
        REQUIRE(slices.front().first == 0);
        REQUIRE(slices.back().second == v.size());
    }

    SECTION("Another unevenly divisible input")
    {
        // n=53, K=5. base=10, rem=3.
        // First 3 slices get 11, last 2 get 10.
        std::vector<D> v(53);
        std::size_t K = 5;
        std::size_t minLen = 10;
        auto slices = createSliceIndicesForBootstrap(v, K, minLen);

        REQUIRE(slices.size() == K);

        SliceVector expected = {{0, 11}, {11, 22}, {22, 33}, {33, 43}, {43, 53}};
        REQUIRE(slices == expected);

        // Check slice lengths
        REQUIRE((slices[0].second - slices[0].first) == 11);
        REQUIRE((slices[1].second - slices[1].first) == 11);
        REQUIRE((slices[2].second - slices[2].first) == 11);
        REQUIRE((slices[3].second - slices[3].first) == 10);
        REQUIRE((slices[4].second - slices[4].first) == 10);

        // Check total coverage
        REQUIRE(slices.front().first == 0);
        REQUIRE(slices.back().second == v.size());
    }

    SECTION("Minimum length check is respected")
    {
        // n=20, K=5 -> slice length is 4. minLen=5 should fail.
        std::vector<D> v(20);
        REQUIRE(createSliceIndicesForBootstrap(v, 5, 5).empty());
        // minLen=4 should pass.
        REQUIRE_FALSE(createSliceIndicesForBootstrap(v, 5, 4).empty());
    }

    SECTION("Slices are contiguous and non-overlapping")
    {
        std::vector<D> v(123);
        std::size_t K = 7;
        auto slices = createSliceIndicesForBootstrap(v, K, 1);

        REQUIRE_FALSE(slices.empty());
        REQUIRE(slices.size() == K);

        for (size_t i = 0; i < slices.size() - 1; ++i)
        {
            // The end of the current slice must be the start of the next slice
            REQUIRE(slices[i].second == slices[i+1].first);
        }
    }
}


// --------------------------- Existing BCa tests ---------------------------

TEST_CASE("BCaBootStrap Tests", "[BCaBootStrap]") {

    SECTION("Constructor validation") {
        std::vector<DecimalType> valid_returns = {DecimalType("0.1")};

        std::vector<DecimalType> empty_returns;
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(empty_returns, 1000), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 50), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 1000, 0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaBootStrap<DecimalType>(valid_returns, 1000, 1.0), std::invalid_argument);
    }

    SECTION("Basic functionality with a simple dataset") {
        std::vector<DecimalType> returns = {
            DecimalType("0.01"), DecimalType("-0.02"), DecimalType("0.03"),
            DecimalType("0.015"), DecimalType("-0.005"), DecimalType("0.025"),
            DecimalType("0.01"), DecimalType("0.00"), DecimalType("-0.01"),
            DecimalType("0.02")
        };

        unsigned int num_resamples = 2000;
        double confidence_level = 0.95;

        BCaBootStrap<DecimalType> bca(returns, num_resamples, confidence_level);

        DecimalType expected_mean =
            std::accumulate(returns.begin(), returns.end(), DecimalType(0)) / DecimalType(returns.size());
        REQUIRE(num::to_double(bca.getMean()) == Catch::Approx(num::to_double(expected_mean)));

        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
        REQUIRE(bca.getMean() >= bca.getLowerBound());
        REQUIRE(bca.getMean() <= bca.getUpperBound());
    }

    SECTION("Symmetric data should produce a roughly symmetric interval") {
        std::vector<DecimalType> symmetric_returns = {
            DecimalType("0.055"), DecimalType("0.047"), DecimalType("0.062"),
            DecimalType("0.051"), DecimalType("0.038"), DecimalType("0.069"),
            DecimalType("0.050"), DecimalType("0.042"), DecimalType("0.058"),
            DecimalType("0.031"), DecimalType("0.075"), DecimalType("0.045")
        };

        BCaBootStrap<DecimalType> bca(symmetric_returns, 2000, 0.95);

        DecimalType mean = bca.getMean();
        DecimalType lower = bca.getLowerBound();
        DecimalType upper = bca.getUpperBound();

        DecimalType lower_dist = mean - lower;
        DecimalType upper_dist = upper - mean;

        REQUIRE(num::to_double(lower_dist / upper_dist) == Catch::Approx(1.0).margin(0.35));
    }

    SECTION("Skewed data should produce an asymmetric interval") {
        std::vector<DecimalType> skewed_returns = {
            DecimalType("0.01"), DecimalType("0.02"), DecimalType("0.015"),
            DecimalType("-0.05"), DecimalType("0.03"), DecimalType("-0.04"),
            DecimalType("0.025"), DecimalType("0.15"),
            DecimalType("0.01"), DecimalType("0.02"), DecimalType("-0.03"),
            DecimalType("0.18")
        };

        BCaBootStrap<DecimalType> bca(skewed_returns, 3000, 0.95);

        DecimalType mean = bca.getMean();
        DecimalType lower = bca.getLowerBound();
        DecimalType upper = bca.getUpperBound();

        DecimalType lower_dist = mean - lower;
        DecimalType upper_dist = upper - mean;

        REQUIRE(upper_dist > lower_dist);
    }

    SECTION("Larger dataset behavior") {
        std::vector<DecimalType> returns;
        for (int i = 0; i < 100; ++i) {
            if (i % 5 == 0)
                returns.push_back(DecimalType("-0.03") + DecimalType(i) / DecimalType(2000));
            else
                returns.push_back(DecimalType("0.01") + DecimalType(i) / DecimalType(5000));
        }

        BCaBootStrap<DecimalType> bca(returns, 5000, 0.99);

        DecimalType expected_mean =
            std::accumulate(returns.begin(), returns.end(), DecimalType(0)) / DecimalType(returns.size());
        REQUIRE(num::to_double(bca.getMean()) == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(bca.getLowerBound() <= bca.getUpperBound());
        REQUIRE(bca.getMean() >= bca.getLowerBound());
        REQUIRE(bca.getMean() <= bca.getUpperBound());
    }
}

// --------------------------- New: Policy tests ---------------------------

TEST_CASE("StationaryBlockResampler basic behavior", "[Resampler][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    // Build a simple monotone sequence so we can infer indices from values
    const size_t n = 200;
    std::vector<D> x; x.reserve(n);
    for (size_t i = 0; i < n; ++i) x.push_back(D(static_cast<int>(i))); // values 0..n-1

    // Fixed-seed RNG for determinism in this policy-only test
    randutils::seed_seq_fe128 seed{12345u, 67890u, 13579u, 24680u};
    randutils::mt19937_rng rng(seed);

    SECTION("Throws on empty input") {
        Policy pol(4);
        std::vector<D> empty;
        REQUIRE_THROWS_AS(pol(empty, 10, rng), std::invalid_argument);
    }

    SECTION("Output size and domain are correct; contiguity is substantial") {
        const size_t L = 4;
        Policy pol(L);

        std::vector<D> y = pol(x, n, rng);

        // size
        REQUIRE(y.size() == n);

        // all values are from the domain 0..n-1
        for (const auto& v : y) {
            const double vd = num::to_double(v);
            REQUIRE(vd >= 0.0);
            REQUIRE(vd < static_cast<double>(n));
        }

        // contiguity: fraction of (y[t+1] == (y[t]+1) mod n) should be high (~ 1 - 1/L)
        // With L=4, expectation is ~0.75. Allow a safe lower bound.
        size_t adjacent = 0;
        for (size_t t = 0; t + 1 < y.size(); ++t) {
            int cur = static_cast<int>(num::to_double(y[t]));
            int nxt = static_cast<int>(num::to_double(y[t + 1]));
            if (nxt == (cur + 1) % static_cast<int>(n)) adjacent++;
        }
        const double frac_adjacent = static_cast<double>(adjacent) / static_cast<double>(y.size() - 1);
        REQUIRE(frac_adjacent > 0.60); // conservative threshold
    }

    SECTION("Mean block length is coerced to >= 2") {
        Policy pol1(1);  // should coerce to 2
        Policy pol2(2);  // stays 2
        Policy pol5(5);  // stays 5
        REQUIRE(pol1.meanBlockLen() == 2);
        REQUIRE(pol2.meanBlockLen() == 2);
        REQUIRE(pol5.meanBlockLen() == 5);
    }
}

TEST_CASE("BCaBootStrap works with StationaryBlockResampler", "[BCaBootStrap][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    // Build a small, autocorrelated-ish series: clusters of positives and negatives
    std::vector<D> returns;
    for (int k = 0; k < 30; ++k) {        // 180 points total
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("0.004"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
        returns.push_back(createDecimal("-0.003"));
    }

    // Geometric mean statistic to exercise the path with a custom stat as well
    GeoMeanStat<D> gstat;
    const unsigned int B = 1500;
    const double cl = 0.95;

    // IID baseline (default policy)
    BCaBootStrap<D> bca_iid(returns, B, cl, gstat);
    REQUIRE(bca_iid.getLowerBound() <= bca_iid.getUpperBound());
    REQUIRE(bca_iid.getStatistic()  >= bca_iid.getLowerBound());
    REQUIRE(bca_iid.getStatistic()  <= bca_iid.getUpperBound());

    // Stationary blocks with mean L = 3 (close to the run length we used)
    Policy pol(3);
    BCaBootStrap<D, Policy> bca_blk(returns, B, cl, gstat, pol);
    REQUIRE(bca_blk.getLowerBound() <= bca_blk.getUpperBound());
    REQUIRE(bca_blk.getStatistic()  >= bca_blk.getLowerBound());
    REQUIRE(bca_blk.getStatistic()  <= bca_blk.getUpperBound());

    // It's common (not guaranteed) that block bootstrap yields a wider CI than IID when dependence exists.
    // We assert a weak property: both intervals are non-degenerate and the block interval is
    // at least not smaller by a *large* margin. This avoids flakiness while still exercising the path.
    const D wid_iid = bca_iid.getUpperBound() - bca_iid.getLowerBound();
    const D wid_blk = bca_blk.getUpperBound() - bca_blk.getLowerBound();

    REQUIRE(num::to_double(wid_iid) > 0.0);
    REQUIRE(num::to_double(wid_blk) > 0.0);

    // Soft check: block width should not be dramatically smaller than IID width.
    REQUIRE(num::to_double(wid_blk) >= 0.50 * num::to_double(wid_iid));
}

TEST_CASE("Policy jackknife: IID delete-one", "[Resampler][Jackknife][IID]") {
    using D = DecimalType;
    using Policy = IIDResampler<D>;

    // x = [0,1,2,3,4]
    std::vector<D> x;
    for (int i = 0; i < 5; ++i) x.push_back(D(i));

    Policy pol;
    // Statistic = arithmetic mean
    typename Policy::StatFn stat = &StatUtils<D>::computeMean;

    auto jk = pol.jackknife(x, stat);

    // Size: n replicates
    REQUIRE(jk.size() == x.size());

    // Expected delete-one means: (sum - xi) / (n-1)
    const double sum = 0 + 1 + 2 + 3 + 4; // 10
    const double n1 = 4.0;
    std::vector<double> expected = {
        (sum - 0) / n1, // 2.5
        (sum - 1) / n1, // 2.25
        (sum - 2) / n1, // 2.0
        (sum - 3) / n1, // 1.75
        (sum - 4) / n1  // 1.5
    };

    for (size_t i = 0; i < jk.size(); ++i) {
        REQUIRE(num::to_double(jk[i]) == Catch::Approx(expected[i]).epsilon(1e-12));
    }
}

TEST_CASE("Policy jackknife: Stationary delete-one-block (L=2)", "[Resampler][Jackknife][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    // x = [0,1,2,3,4]
    std::vector<D> x;
    for (int i = 0; i < 5; ++i) x.push_back(D(i));

    Policy pol(2); // L = 2, L_eff = 2
    typename Policy::StatFn stat = &StatUtils<D>::computeMean;

    auto jk = pol.jackknife(x, stat);

    // We should get n replicates (overlapping, circular delete-2 blocks)
    REQUIRE(jk.size() == x.size());

    // Build expected means using the same Decimal statistic to match rounding:
    // start=0: keep [2,3,4]
    // start=1: keep [0,3,4]
    // start=2: keep [0,1,4]
    // start=3: keep [0,1,2]
    // start=4: keep [1,2,3]
    const int idx[][3] = {{2,3,4},{0,3,4},{0,1,4},{0,1,2},{1,2,3}};
    for (size_t i = 0; i < jk.size(); ++i) {
        std::vector<D> kept; kept.reserve(3);
        kept.push_back(x[idx[i][0]]);
        kept.push_back(x[idx[i][1]]);
        kept.push_back(x[idx[i][2]]);
        D expected = StatUtils<D>::computeMean(kept);
        REQUIRE(num::to_double(jk[i]) == Catch::Approx(num::to_double(expected)).epsilon(1e-12));
    }
}

TEST_CASE("Policy jackknife: Stationary clamps L to n-1", "[Resampler][Jackknife][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    // x = [0,1,2,3,4], n=5
    std::vector<D> x;
    for (int i = 0; i < 5; ++i) x.push_back(D(i));

    Policy pol(10); // L = 10 -> L_eff = min(10, n-1) = 4
    typename Policy::StatFn stat = &StatUtils<D>::computeMean;

    auto jk = pol.jackknife(x, stat);

    // n replicates; each replicate removes 4 elements, leaving 1 element -> mean equals the remaining element
    REQUIRE(jk.size() == x.size());

    // Expected remaining (circular delete-4):
    // start=0: delete [0,4) -> keep [4] -> mean 4
    // start=1: delete [1,0) -> keep [0] -> mean 0
    // start=2: keep [1] -> 1
    // start=3: keep [2] -> 2
    // start=4: keep [3] -> 3
    const double expected[] = {4.0, 0.0, 1.0, 2.0, 3.0};
    for (size_t i = 0; i < jk.size(); ++i) {
        REQUIRE(num::to_double(jk[i]) == Catch::Approx(expected[i]).epsilon(1e-12));
    }
}

TEST_CASE("StationaryBlockResampler jackknife with nonlinear statistic (variance)", 
          "[Resampler][Jackknife][Stationary][Nonlinear]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    // Small sequence for deterministic variance
    std::vector<D> x = { D(1), D(2), D(3), D(4), D(5) };

    // Statistic: population variance (nonlinear)
    typename Policy::StatFn stat = [](const std::vector<D>& v) -> D {
        const size_t n = v.size();
        D mean = StatUtils<D>::computeMean(v);
        D sumsq = D(0);
        for (const auto& val : v) {
            const D diff = val - mean;
            sumsq += diff * diff;
        }
        return sumsq / D(n);
    };

    Policy pol(2);
    auto jk = pol.jackknife(x, stat);

    // Expect n replicates
    REQUIRE(jk.size() == x.size());

    // Ensure results are finite and vary (i.e., not all identical)
    bool all_equal = true;
    for (size_t i = 1; i < jk.size(); ++i) {
        if (jk[i] != jk[0]) { all_equal = false; break; }
    }
    REQUIRE_FALSE(all_equal);

    // Sanity: mean of jackknife variances roughly near variance of full sample
    D fullVar = stat(x);
    D avgJk = StatUtils<D>::computeMean(jk);
    REQUIRE(num::to_double(avgJk) == Catch::Approx(num::to_double(fullVar)).margin(0.5));
}

// --------------------------- Annualizer tests ---------------------------

template<class Decimal>
class MockBCaBootStrapForAnnualizer : public BCaBootStrap<Decimal>
{
public:
    MockBCaBootStrapForAnnualizer()
      : BCaBootStrap<Decimal>(std::vector<Decimal>{Decimal("0.0"), Decimal("0.0")}, 100) {}

    void setTestResults(const Decimal& mean, const Decimal& lower, const Decimal& upper)
    {
        this->setMean(mean);
        this->setLowerBound(lower);
        this->setUpperBound(upper);
        this->m_is_calculated = true;
    }

protected:
    void calculateBCaBounds() override {
        // no-op for mock
    }
};

TEST_CASE("calculateAnnualizationFactor functionality", "[BCaAnnualizer]") {

    SECTION("Standard time frames") {
        REQUIRE(calculateAnnualizationFactor(TimeFrame::DAILY) == Catch::Approx(252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::WEEKLY) == Catch::Approx(52.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::MONTHLY) == Catch::Approx(12.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::QUARTERLY) == Catch::Approx(4.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::YEARLY) == Catch::Approx(1.0));
    }

    SECTION("Intraday time frames with standard US stock market hours") {
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 1) == Catch::Approx(6.5 * 60.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 15) == Catch::Approx(6.5 * 4.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 60) == Catch::Approx(6.5 * 1.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 90) == Catch::Approx(6.5 * (60.0 / 90.0) * 252.0));
    }

    SECTION("Intraday time frames with custom hours (e.g., 24-hour Forex)") {
        double forex_hours = 24.0;
        double trading_days = 252.0;
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 1, trading_days, forex_hours) == Catch::Approx(24.0 * 60.0 * 252.0));
        REQUIRE(calculateAnnualizationFactor(TimeFrame::INTRADAY, 60, trading_days, forex_hours) == Catch::Approx(24.0 * 1.0 * 252.0));
    }

    SECTION("Invalid arguments throw exceptions") {
        REQUIRE_THROWS_AS(calculateAnnualizationFactor(TimeFrame::INTRADAY, 0), std::invalid_argument);
    }
}

TEST_CASE("BCaAnnualizer functionality", "[BCaAnnualizer]") {

    MockBCaBootStrapForAnnualizer<DecimalType> mock_bca;

    SECTION("Annualizing positive returns") {
        DecimalType per_bar_mean  = createDecimal("0.001");
        DecimalType per_bar_lower = createDecimal("0.0005");
        DecimalType per_bar_upper = createDecimal("0.0015");
        mock_bca.setTestResults(per_bar_mean, per_bar_lower, per_bar_upper);

        double k = 252.0;
        BCaAnnualizer<DecimalType> annualizer(mock_bca, k);

        DecimalType expected_mean  = DecimalType(pow((DecimalType("1.0") + per_bar_mean ).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_lower = DecimalType(pow((DecimalType("1.0") + per_bar_lower).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_upper = DecimalType(pow((DecimalType("1.0") + per_bar_upper).getAsDouble(), k)) - DecimalType("1.0");

        REQUIRE(num::to_double(annualizer.getAnnualizedMean())       == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(num::to_double(annualizer.getAnnualizedLowerBound()) == Catch::Approx(num::to_double(expected_lower)));
        REQUIRE(num::to_double(annualizer.getAnnualizedUpperBound()) == Catch::Approx(num::to_double(expected_upper)));
    }

    SECTION("Annualizing negative returns") {
        DecimalType per_bar_mean  = createDecimal("-0.0005");
        DecimalType per_bar_lower = createDecimal("-0.001");
        DecimalType per_bar_upper = createDecimal("-0.0002");
        mock_bca.setTestResults(per_bar_mean, per_bar_lower, per_bar_upper);

        double k = 252.0;
        BCaAnnualizer<DecimalType> annualizer(mock_bca, k);

        DecimalType expected_mean  = DecimalType(pow((DecimalType("1.0") + per_bar_mean ).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_lower = DecimalType(pow((DecimalType("1.0") + per_bar_lower).getAsDouble(), k)) - DecimalType("1.0");
        DecimalType expected_upper = DecimalType(pow((DecimalType("1.0") + per_bar_upper).getAsDouble(), k)) - DecimalType("1.0");

        REQUIRE(num::to_double(annualizer.getAnnualizedMean())       == Catch::Approx(num::to_double(expected_mean)));
        REQUIRE(num::to_double(annualizer.getAnnualizedLowerBound()) == Catch::Approx(num::to_double(expected_lower)));
        REQUIRE(num::to_double(annualizer.getAnnualizedUpperBound()) == Catch::Approx(num::to_double(expected_upper)));
    }

    SECTION("Invalid annualization factor throws exception") {
        mock_bca.setTestResults(createDecimal("0.01"), createDecimal("0.0"), createDecimal("0.02"));
        REQUIRE_THROWS_AS(BCaAnnualizer<DecimalType>(mock_bca, 0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(BCaAnnualizer<DecimalType>(mock_bca, -252.0), std::invalid_argument);
    }
}

TEST_CASE("BCaBootStrap: interval widens with confidence level", "[BCaBootStrap][Monotonicity]") {
    using D = DecimalType;
    std::vector<D> x;
    for (int i = 0; i < 60; ++i) {
        x.push_back(createDecimal(i % 7 == 0 ? "-0.02" : "0.01"));
    }

    BCaBootStrap<D> bca90(x, 3000, 0.90);
    BCaBootStrap<D> bca99(x, 3000, 0.99);

    const double w90 = num::to_double(bca90.getUpperBound() - bca90.getLowerBound());
    const double w99 = num::to_double(bca99.getUpperBound() - bca99.getLowerBound());
    REQUIRE(w99 >= w90);
}

TEST_CASE("BCaBootStrap: n<2 throws when computing", "[BCaBootStrap][Validation]") {
    using D = DecimalType;
    std::vector<D> x = { createDecimal("0.01") }; // n = 1
    BCaBootStrap<D> bca(x, 200, 0.95);
    REQUIRE_THROWS_AS(bca.getLowerBound(), std::invalid_argument);
}

TEST_CASE("StationaryBlockResampler: estimated mean block length matches L", "[Resampler][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    const size_t n = 400;              // output size
    const size_t xn = 200;             // source size
    std::vector<D> x; x.reserve(xn);
    for (size_t i = 0; i < xn; ++i) x.push_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{111u, 222u, 333u, 444u};
    randutils::mt19937_rng rng(seed);

    const size_t L = 4;
    Policy pol(L);
    std::vector<D> y = pol(x, n, rng);

    // Count block boundaries: where contiguity (next == (cur+1)%xn) breaks
    size_t breaks = 1; // first block starts at t=0
    for (size_t t = 0; t + 1 < y.size(); ++t) {
        int cur = static_cast<int>(num::to_double(y[t]));
        int nxt = static_cast<int>(num::to_double(y[t + 1]));
        if (nxt != (cur + 1) % static_cast<int>(xn)) breaks++;
    }
    const double Lhat = static_cast<double>(n) / static_cast<double>(breaks);
    REQUIRE(Lhat == Catch::Approx(static_cast<double>(L)).margin(1.5)); // generous for randomness
}

TEST_CASE("StationaryBlockResampler: contiguity increases with L", "[Resampler][Stationary]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    const size_t n = 300, xn = 150;
    std::vector<D> x; x.reserve(xn);
    for (size_t i = 0; i < xn; ++i) x.push_back(D(static_cast<int>(i)));

    auto frac_adjacent = [&](size_t L, uint64_t s1, uint64_t s2, uint64_t s3, uint64_t s4){
        randutils::seed_seq_fe128 seed{s1, s2, s3, s4};
        randutils::mt19937_rng rng(seed);
        Policy pol(L);
        auto y = pol(x, n, rng);
        size_t adj = 0;
        for (size_t t = 0; t + 1 < y.size(); ++t) {
            int cur = static_cast<int>(num::to_double(y[t]));
            int nxt = static_cast<int>(num::to_double(y[t+1]));
            if (nxt == (cur + 1) % static_cast<int>(xn)) adj++;
        }
        return static_cast<double>(adj) / static_cast<double>(n - 1);
    };

    const double f2 = frac_adjacent(2, 10,20,30,40);  // ~0.5 expected
    const double f6 = frac_adjacent(6, 10,20,30,40);  // ~0.83 expected
    REQUIRE(f6 > f2 + 0.15);  // clear separation
}

TEST_CASE("StationaryBlockResampler: stability with very large L", "[Resampler][Stationary][Stress]") {
    using D = DecimalType;
    using Policy = StationaryBlockResampler<D>;

    const size_t xn = 7;
    std::vector<D> x; x.reserve(xn);
    for (size_t i = 0; i < xn; ++i) x.push_back(D(static_cast<int>(i)));

    randutils::seed_seq_fe128 seed{999u, 1u, 2u, 3u};
    randutils::mt19937_rng rng(seed);

    const size_t n = 80;
    Policy pol(1000);  // mean length >> xn and n
    auto y = pol(x, n, rng);

    REQUIRE(y.size() == n);

    // Ensure we saw more than one block (i.e., not a single giant copy)
    size_t breaks = 0;
    for (size_t t = 0; t + 1 < y.size(); ++t) {
        int cur = static_cast<int>(num::to_double(y[t]));
        int nxt = static_cast<int>(num::to_double(y[t + 1]));
        if (nxt != (cur + 1) % static_cast<int>(xn)) breaks++;
    }
    REQUIRE(breaks >= 1);
}

TEST_CASE("BCaBootStrap: degenerate dataset collapses interval", "[BCaBootStrap][Edge]") {
    using D = DecimalType;
    std::vector<D> x(25, createDecimal("0.0123"));  // all same
    BCaBootStrap<D> bca(x, 2000, 0.95);

    auto mu = bca.getMean();
    auto lo = bca.getLowerBound();
    auto hi = bca.getUpperBound();

    REQUIRE(num::to_double(hi - lo) == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(num::to_double(mu - lo) == Catch::Approx(0.0).margin(1e-15));
    REQUIRE(num::to_double(hi - mu) == Catch::Approx(0.0).margin(1e-15));
}

TEST_CASE("BoundFutureReturns: monthly aggregation and BCa bounds (Stationary blocks)", "[BoundFutureReturns][Monthly][Stationary]") {
    using D = DecimalType;

    // Fabricate a ClosedPositionHistory with 8 distinct months.
    // We use single-bar positions so each month's compounded return equals the intended value.
    ClosedPositionHistory<D> hist;
    TradingVolume one(1, TradingVolume::CONTRACTS);

    // Helper to add a 1-bar LONG position with return r in (year, month, day)
    auto add_long_1bar = [&](int y, int m, int d, const char* r_str) {
        D r = createDecimal(r_str);                   // e.g. "0.02" for +2%
        D entry = createDecimal("100");
        D exit  = entry * (D("1.0") + r);

        TimeSeriesDate de(y, m, d);
        auto e = createTimeSeriesEntry(de, entry, entry, entry, entry, 10);
        auto pos = std::make_shared<TradingPositionLong<D>>(myCornSymbol, e->getOpenValue(), *e, one);

        // Close next day in same month when possible (or same day if helper allows)
        int d_exit = std::min(d + 1, 28);
        TimeSeriesDate dx(y, m, d_exit);
        pos->ClosePosition(dx, exit);
        hist.addClosedPosition(pos);
    };

    // Jan..Aug 2021: [+2%, -1%, +1.5%, +0.5%, -0.8%, +3.0%, +0.2%, +1.0%]
    add_long_1bar(2021, Jan,  5, "0.02");   // Jan
    add_long_1bar(2021, Feb,  9, "-0.01");  // Feb
    add_long_1bar(2021, Mar,  3, "0.015");  // Mar
    add_long_1bar(2021, Apr, 12, "0.005");  // Apr
    add_long_1bar(2021, May,  6, "-0.008"); // May
    add_long_1bar(2021, Jun, 15, "0.03");   // Jun
    add_long_1bar(2021, Jul,  7, "0.002");  // Jul
    add_long_1bar(2021, Aug, 19, "0.01");   // Aug

    // 1) Verify monthly aggregation
    auto monthly = mkc_timeseries::buildMonthlyReturnsFromClosedPositions<D>(hist);
    REQUIRE(monthly.size() == 8);

    // Check chronological order and magnitudes (exact equality is fine with single-bar months)
    REQUIRE(monthly[0] == createDecimal("0.02"));
    REQUIRE(monthly[1] == createDecimal("-0.01"));
    REQUIRE(monthly[2] == createDecimal("0.015"));
    REQUIRE(monthly[3] == createDecimal("0.005"));
    REQUIRE(monthly[4] == createDecimal("-0.008"));
    REQUIRE(monthly[5] == createDecimal("0.03"));
    REQUIRE(monthly[6] == createDecimal("0.002"));
    REQUIRE(monthly[7] == createDecimal("0.01"));

    // 2) Run BoundFutureReturns with Stationary blocks (default Resampler)
    const unsigned B   = 2000;   // keep moderate for test runtime
    const double   cl  = 0.95;
    const unsigned L   = 3;      // mean block length

    mkc_timeseries::BoundFutureReturns<D> bfr(
        hist,
        /*blockLen=*/L,
        /*lowerQuantileP=*/0.10,
        /*upperQuantileP=*/0.90,
        /*numBootstraps=*/B,
        /*confLevel=*/cl
    );

    // Basic ordering invariants
    D lowerBound = bfr.getLowerBound();              // conservative lower (q10 CI lower)
    D upperBound = bfr.getUpperBound();              // conservative upper (q90 CI upper)
    D q10_point  = bfr.getLowerPointQuantile();      // point q10
    D q90_point  = bfr.getUpperPointQuantile();      // point q90

    REQUIRE(lowerBound <= q10_point);
    REQUIRE(q10_point  <= q90_point);
    REQUIRE(q90_point  <= upperBound);

    // Switching to point policy should set bounds == point quantiles
    bfr.usePointPolicy();
    REQUIRE(bfr.getLowerBound() == q10_point);
    REQUIRE(bfr.getUpperBound() == q90_point);
}

TEST_CASE("BoundFutureReturns: works with IID resampler as well", "[BoundFutureReturns][IID]") {
    using D = DecimalType;

    // Build 8 months again (simpler pattern)
    ClosedPositionHistory<D> hist;
    TradingVolume one(1, TradingVolume::CONTRACTS);
    auto add_long_1bar = [&](int y, int m, int d, const char* r_str) {
        D r = createDecimal(r_str), entry = createDecimal("100");
        D exit = entry * (D("1.0") + r);
        TimeSeriesDate de(y, m, d);
        auto e = createTimeSeriesEntry(de, entry, entry, entry, entry, 10);
        auto pos = std::make_shared<TradingPositionLong<D>>(myCornSymbol, e->getOpenValue(), *e, one);
        int d_exit = std::min(d + 1, 28);
        TimeSeriesDate dx(y, m, d_exit);
        pos->ClosePosition(dx, exit);
        hist.addClosedPosition(pos);
    };

    // Sep..Apr (8 months): mildly skewed mixture
    add_long_1bar(2021, Sep,  2,  "0.012");
    add_long_1bar(2021, Oct,  5,  "-0.006");
    add_long_1bar(2021, Nov, 10,  "0.007");
    add_long_1bar(2021, Dec, 14,  "0.004");
    add_long_1bar(2022, Jan,  6,  "-0.011");
    add_long_1bar(2022, Feb, 17,  "0.018");
    add_long_1bar(2022, Mar,  8,  "0.000");
    add_long_1bar(2022, Apr, 21,  "0.009");

    // Instantiate with IID resampler
    mkc_timeseries::BoundFutureReturns<D, IIDResampler<D>> bfr_iid(
        hist,
        /*blockLen=*/3,              // ignored by IID policy
        /*lowerQuantileP=*/0.10,
        /*upperQuantileP=*/0.90,
        /*numBootstraps=*/2000,
        /*confLevel=*/0.95
    );

    // Invariants
    REQUIRE(bfr_iid.getLowerBound() <= bfr_iid.getLowerPointQuantile());
    REQUIRE(bfr_iid.getUpperPointQuantile() <= bfr_iid.getUpperBound());
    REQUIRE(bfr_iid.getLowerPointQuantile() <= bfr_iid.getUpperPointQuantile());

    // Sanity: monthly returns available and size >= 8
    const auto& monthly = bfr_iid.getMonthlyReturns();
    REQUIRE(monthly.size() >= 8);
}

TEST_CASE("BoundFutureReturns: 20-month dataset yields stable bounds (Stationary blocks)", "[BoundFutureReturns][Monthly][20M]") {
    using D = DecimalType;

    // --- Fabricate 20 distinct months of returns ---
    // Jan 2020 .. Aug 2021 (inclusive) = 20 months
    // Mix of small positives/negatives to resemble mild skew + variance
    const char* rstrs[20] = {
        "0.012", "-0.006", "0.007", "0.004", "-0.011",
        "0.018", "0.000", "0.009", "0.013", "-0.004",
        "0.006", "0.008", "-0.007", "0.015", "0.003",
        "0.011", "-0.005", "0.010", "0.002", "0.014"
    };

    ClosedPositionHistory<D> hist;
    TradingVolume one(1, TradingVolume::CONTRACTS);

    auto add_long_1bar = [&](int y, int m, int d, const char* r_str) {
        D r = createDecimal(r_str);
        D entry = createDecimal("100");
        D exit  = entry * (D("1.0") + r);

        TimeSeriesDate de(y, m, d);
        auto e = createTimeSeriesEntry(de, entry, entry, entry, entry, 10);
        auto pos = std::make_shared<TradingPositionLong<D>>(myCornSymbol, e->getOpenValue(), *e, one);

        int d_exit = std::min(d + 1, 28);
        TimeSeriesDate dx(y, m, d_exit);
        pos->ClosePosition(dx, exit);
        hist.addClosedPosition(pos);
    };

    // Fill months Jan 2020 .. Aug 2021
    {
        int y = 2020;
        int m = static_cast<int>(Jan);
        for (int i = 0; i < 20; ++i) {
            add_long_1bar(y, m, 5 + (i % 10), rstrs[i]); // stagger day a bit (5..14)
            // Advance month/year
            if (++m > static_cast<int>(Dec)) {
                m = static_cast<int>(Jan);
                ++y;
            }
        }
    }

    // 1) Verify monthly aggregation
    auto monthly = mkc_timeseries::buildMonthlyReturnsFromClosedPositions<D>(hist);
    REQUIRE(monthly.size() == 20);

    // Spot-check a few exact values (single-bar months => exact equality)
    REQUIRE(monthly.front() == createDecimal("0.012"));   // Jan 2020
    REQUIRE(monthly[1]      == createDecimal("-0.006"));  // Feb 2020
    REQUIRE(monthly[10]     == createDecimal("0.006"));   // Nov 2020
    REQUIRE(monthly.back()  == createDecimal("0.014"));   // Aug 2021

    // 2) Run BoundFutureReturns with stationary blocks
    const unsigned B  = 1500;   // moderate for test runtime
    const double   cl = 0.95;
    const unsigned L  = 4;      // average block length

    mkc_timeseries::BoundFutureReturns<D> bfr(
        hist,
        /*blockLen=*/L,
        /*lowerQuantileP=*/0.10,
        /*upperQuantileP=*/0.90,
        /*numBootstraps=*/B,
        /*confLevel=*/cl
    );

    // 3) Ordering / policy invariants
    D lowerBound = bfr.getLowerBound();         // conservative lower (q10 CI lower)
    D upperBound = bfr.getUpperBound();         // conservative upper (q90 CI upper)
    D q10_point  = bfr.getLowerPointQuantile(); // point q10
    D q90_point  = bfr.getUpperPointQuantile(); // point q90

    REQUIRE(lowerBound <= q10_point);
    REQUIRE(q10_point  <= q90_point);
    REQUIRE(q90_point  <= upperBound);

    // Switch to point policy and verify bounds equal the point quantiles
    bfr.usePointPolicy();
    REQUIRE(bfr.getLowerBound() == q10_point);
    REQUIRE(bfr.getUpperBound() == q90_point);
}
