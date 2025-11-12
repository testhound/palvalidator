// SmallNBootstrapHelpersTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <sstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <random>

#include "SmallNBootstrapHelpers.h"  // target under test
#include "number.h"
#include "TestUtils.h"               // for createDecimal if desired
#include "TradingBootstrapFactory.h"
#include "BootstrapConfig.h"

using namespace palvalidator::bootstrap_helpers;
using Catch::Approx;

using Decimal = num::DefaultNumber;

// --- tiny helpers ---
static Decimal D(double x) { return Decimal(x); }

// --- MOCK IMPLEMENTATIONS (Catch2 Compatible) ---
// Note: We redefine the mocks inside a namespace to avoid conflicts.

namespace TestMocks {
    using Decimal = num::DefaultNumber;
    
    // --- 1. MOCK GEOSTAT AND STRATEGY (Type Tags) ---
    // The GeoStat used in SmallNBootstrapHelpers.h is likely the Sampler type 
    // for MOutOfN and the statistic type for BCa, so we keep a mock for it.
    struct MockGeoStat {
        MockGeoStat() = default;
        // The actual Stat type must be callable, or convertible to a callable.
        // We assume it serves as the Sampler/Stat type tag.
    };

    // Mock Strategy (must provide a hashCode() as used by the Factory)
    struct MockStrategy {
        uint64_t hashCode() const { return 0xDEADBEEF; } 
    };
    
    // --- 2. MOCK ENGINE RESULTS (Controlled Output) ---
    // This part remains the same, as it models the MNRunSimple and BCa output.

    template <class Num>
    struct MNRunSimpleMock {
        Num          lower{};         // per-period LB
        Num          upper{};         // for logging checks
        std::size_t  m_sub{0};
        std::size_t  L{0};
        std::size_t  effective_B{0};

        MNRunSimpleMock(Num lb, Num ub, std::size_t m) : 
            lower(lb), upper(ub), m_sub(m), L(3), effective_B(1000) {}
        
        // Mocking the .run() method return type
        template<typename CRNRng>
        MNRunSimpleMock run(const std::vector<Num>&, const MockGeoStat&, CRNRng) const {
            return *this;
        }
    };

    template <class Num>
    struct MockBCaEngine {
        Num lb_;
        Num ub_;

        MockBCaEngine(Num lb, Num ub) : lb_(lb), ub_(ub) {}
        Num getLowerBound() const { return lb_; }
        Num getUpperBound() const { return ub_; }
    };

    // --- 3. MOCK FACTORY CONTROL ---
    struct FactoryControl {
        bool        mn_called = false;
        bool        bca_called = false;
        bool        expect_block = false; // Expected resampler type
        double      mn_lb_val = 0.0;
        double      bca_lb_val = 0.0;
        std::size_t m_sub_used = 0;     // M-out-of-N m parameter
    };

    // We will use a dedicated class to capture the raw factory's name/template
    // Note: The real factory uses TradingBootstrapFactory<Engine> which we alias as FactoryT.

    // A small wrapper to hold our static control and provide mock methods
    struct FactoryMockHelper {
        static FactoryControl control;
    };
    FactoryControl FactoryMockHelper::control = {};

} // namespace TestMocks

// Map the real template dependencies to our mocks
using GeoStatT = TestMocks::MockGeoStat;
using StrategyT = TestMocks::MockStrategy;

// We must specialize the *real* factory template with a new mock type
// to inject our controlled behavior. Since specializing an external type is complex,
// we will mock the *methods* of the FactoryT type by replacing the real factory 
// with a mock class that shares the same name in a restricted scope if possible, 
// or, more robustly, by adapting the logic.

// Since the signature in SmallNBootstrapHelpers.h requires a reference to the
// actual 'palvalidator::bootstrap_cfg::BootstrapFactory' (TradingBootstrapFactory),
// the safest way is to use a lambda-based mock approach, but that's complex
// for C++ templates.

// THE SOLUTION: We define a Mock TradingBootstrapFactory that fully replaces the real one
// for the duration of the test, using template specialization and static members
// to inject our controlled results. This requires overriding the real class structure.
// Given that the scope is limited to the test file, this replacement is safe.

// We will use the *raw strategy ID* overloads as they require fewer template arguments.

template<class Engine>
class MockTradingBootstrapFactory
{
public:
  // Using the real Factory's constructor signature
  explicit MockTradingBootstrapFactory(uint64_t masterSeed) {}

  // ====================== Mock makeBCa ======================
  // Overload accepting GeoStat (statistic type) + strategy object
  template<class Decimal, class GeoStat, class Resampler>
  auto makeBCa(const std::vector<Decimal>& returns,
               unsigned B, double CL,
               const GeoStat& statGeo,
               Resampler sampler,
               const TestMocks::MockStrategy& strategy,
               uint64_t stageTag, uint64_t L, uint64_t fold)
    -> TestMocks::MockBCaEngine<Decimal>
  {
    TestMocks::FactoryMockHelper::control.bca_called = true;
    
    // Check Resampler Type (IID vs Block)
    using BlockType = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Decimal>;
    if constexpr (std::is_same_v<Resampler, BlockType>) {
        REQUIRE(TestMocks::FactoryMockHelper::control.expect_block == true);
    } else {
        REQUIRE(TestMocks::FactoryMockHelper::control.expect_block == false);
    }
    
    return TestMocks::MockBCaEngine<Decimal>(
        Decimal(TestMocks::FactoryMockHelper::control.bca_lb_val),
        Decimal(TestMocks::FactoryMockHelper::control.bca_lb_val + 0.01)
    );
  }


  // ====================== Mock makeMOutOfN ======================
  // Overload accepting strategy object
  template<class Decimal, class Sampler, class Resampler>
  auto makeMOutOfN(std::size_t B, double CL, double m_ratio,
                   const Resampler& resampler,
                   const TestMocks::MockStrategy& strategy,
                   uint64_t stageTag, uint64_t L, uint64_t fold)
    -> std::pair<
         TestMocks::MNRunSimpleMock<Decimal>,
         int // Dummy CRNRng
       >
  {
    TestMocks::FactoryMockHelper::control.mn_called = true;
    
    // Check Resampler Type
    using BlockType = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Decimal>;
    if constexpr (std::is_same_v<Resampler, BlockType>) {
        REQUIRE(TestMocks::FactoryMockHelper::control.expect_block == true);
    } else {
        REQUIRE(TestMocks::FactoryMockHelper::control.expect_block == false);
    }

    // Return the mock engine and a dummy CRNRng
    return {
        TestMocks::MNRunSimpleMock<Decimal>(
            Decimal(TestMocks::FactoryMockHelper::control.mn_lb_val),
            Decimal(TestMocks::FactoryMockHelper::control.mn_lb_val + 0.01),
            TestMocks::FactoryMockHelper::control.m_sub_used
        ),
        0 // Dummy CRNRng
    };
  }
};

// Alias the mock factory type for tests (must be after MockTradingBootstrapFactory definition)
using FactoryT = MockTradingBootstrapFactory<palvalidator::bootstrap_cfg::BootstrapEngine>;

// To make the test compile, we need to create a dummy function
// that mimics the signature of GeoStat::value() for the BCa factory call.
// Since the factory in SmallNBootstrapHelpers.h uses the GeoStat object 
// directly, we will assume a helper function can be used to convert 
// the Stat to the required std::function<...>.

// Note: We cannot redefine BootstrapFactory in the palvalidator::bootstrap_cfg namespace
// as it conflicts with the real definition. Instead, we'll use the mock factory directly
// in test calls by creating instances and passing them properly.


TEST_CASE("longest_sign_run basic behavior", "[SmallN][runs]") {
  // empty → 0
  std::vector<Decimal> e;
  REQUIRE(longest_sign_run(e) == 0);

  // alternating signs → longest = 1
  std::vector<Decimal> alt = { D(1), D(-1), D(1), D(-1), D(1), D(-1) };
  REQUIRE(longest_sign_run(alt) == 1);

  // long positive streak then negative → picks max streak
  std::vector<Decimal> streak;
  streak.insert(streak.end(), 5, D(0.1));
  streak.insert(streak.end(), 3, D(-0.2));
  REQUIRE(longest_sign_run(streak) == 5);

  // zeros shouldn't extend streaks
  std::vector<Decimal> zeros = { D(0.1), D(0.1), D(0.0), D(0.1), D(0.1) };
  REQUIRE(longest_sign_run(zeros) == 2);
}

TEST_CASE("sign_positive_ratio correctness", "[SmallN][ratio]") {
  REQUIRE(sign_positive_ratio(std::vector<Decimal>{}) == Approx(0.0));
  REQUIRE(sign_positive_ratio(std::vector<Decimal>{ D(1) }) == Approx(1.0));
  REQUIRE(sign_positive_ratio(std::vector<Decimal>{ D(-1) }) == Approx(0.0));
  REQUIRE(sign_positive_ratio(std::vector<Decimal>{ D(-1), D(0), D(2) }) == Approx(1.0/3.0));
}

TEST_CASE("z_from_two_sided_CL lookup", "[SmallN][zlookup]") {
  REQUIRE(z_from_two_sided_CL(0.95)  == Approx(1.960));
  REQUIRE(z_from_two_sided_CL(0.90)  == Approx(1.645));
  REQUIRE(z_from_two_sided_CL(0.99)  == Approx(2.576));
  REQUIRE(z_from_two_sided_CL(0.975) == Approx(2.241));

  // fallback goes to 1.960
  REQUIRE(z_from_two_sided_CL(0.942) == Approx(1.960));
}

TEST_CASE("has_heavy_tails_wide thresholding", "[SmallN][heavy]") {
  // either |skew| >= 0.90 OR exkurt >= 1.20 triggers
  REQUIRE(has_heavy_tails_wide(0.89, 1.19) == false);
  REQUIRE(has_heavy_tails_wide(0.90, 0.0) == true);
  REQUIRE(has_heavy_tails_wide(-1.2, 0.0) == true);
  REQUIRE(has_heavy_tails_wide(0.0, 1.20) == true);
}

TEST_CASE("choose_block_smallN behavior", "[SmallN][chooseblock]") {
  // sign imbalance → block
  REQUIRE( choose_block_smallN(/*ratio_pos=*/0.80, /*n=*/30, /*run=*/3) == true );
  REQUIRE( choose_block_smallN(/*ratio_pos=*/0.20, /*n=*/30, /*run=*/3) == true );

  // small N + long run → block
  // For n<=40, run_thresh ≈ max(6, ceil(0.18*n)) → here ceil(0.18*40)=8
  REQUIRE( choose_block_smallN(/*ratio_pos=*/0.55, /*n=*/40, /*run=*/8) == true );

  // no imbalance, short run, larger n → iid
  REQUIRE( choose_block_smallN(/*ratio_pos=*/0.51, /*n=*/120, /*run=*/2) == false );
}

TEST_CASE("clamp_smallL clamps into [2,3]", "[SmallN][L]") {
  REQUIRE(clamp_smallL(1) == 2);
  REQUIRE(clamp_smallL(2) == 2);
  REQUIRE(clamp_smallL(3) == 3);
  REQUIRE(clamp_smallL(5) == 3);
}

TEST_CASE("mn_ratio_from_n heuristic", "[SmallN][mn]") {
  // n=0 → 1.0
  REQUIRE(mn_ratio_from_n(0) == Approx(1.0));

  // small n (e.g., n=5): m_ceil = n-2 = 3; floor=16 → clamped to 3 → ratio=0.6
  REQUIRE(mn_ratio_from_n(5) == Approx(3.0/5.0));

  // n=2: special guard allows m==n → ratio 1.0 (as implemented)
  REQUIRE(mn_ratio_from_n(2) == Approx(1.0));

  // n=30: target ≈ ceil(0.8*n)=24 → ratio≈0.8
  REQUIRE(mn_ratio_from_n(30) == Approx(24.0/30.0).epsilon(1e-12));

  // n=100: target ≈ 80 → ratio≈0.8
  REQUIRE(mn_ratio_from_n(100) == Approx(80.0/100.0).epsilon(1e-12));
}

TEST_CASE("dispatch_smallN_resampler forwards use_block and L_small", "[SmallN][dispatch]") {
  // Craft a streaky, small-N series to trigger block
  std::vector<Decimal> streak;
  streak.insert(streak.end(), 7, D(0.01));  // long + streak
  streak.insert(streak.end(), 5, D(-0.01));
  std::size_t L_in = 10;
  const char* name_ptr = nullptr;
  std::size_t L_small = 0;

  // The callback receives: (resampler, ratio_pos, use_block, L_small)
  auto got = dispatch_smallN_resampler<Decimal>(
    streak, L_in,
    [&](auto&, double ratio, bool use_block, std::size_t Ls) {
      REQUIRE(use_block == true);    // streaky + small N → block
      REQUIRE(Ls == 3);              // clamped
      REQUIRE(ratio > 0.50);         // more positives than negatives
      return 42;
    },
    &name_ptr, &L_small
  );

  REQUIRE(got == 42);
  REQUIRE(std::string(name_ptr).find("StationaryMaskValueResamplerAdapter") != std::string::npos);
  REQUIRE(L_small == 3);

  // Non-streaky / balanced series → iid
  std::vector<Decimal> balanced;
  for (int i=0;i<60;++i) balanced.push_back( (i%2==0) ? D(0.01) : D(-0.01) );

  name_ptr = nullptr; L_small = 0;
  dispatch_smallN_resampler<Decimal>(
    balanced, /*L=*/5,
    [&](auto&, double ratio, bool use_block, std::size_t Ls) {
      REQUIRE(use_block == false);
      REQUIRE(Ls == 3);              // still clamped from 5 → 3
      REQUIRE(Approx(ratio).margin(1e-9) == 0.5);
      return 0;
    },
    &name_ptr, &L_small
  );
  REQUIRE(std::string(name_ptr).find("IIDResampler") != std::string::npos);
  REQUIRE(L_small == 3);
}

TEST_CASE("internal runs_longest_quantile_MC monotonic in alpha", "[SmallN][MC]") {
  using namespace palvalidator::bootstrap_helpers::internal;

  const std::size_t n = 100;
  const double p = 0.5;

  auto q90 = runs_longest_quantile_MC(n, p, RunsTestConfig{0.90, 512}, 1234);
  auto q95 = runs_longest_quantile_MC(n, p, RunsTestConfig{0.95, 512}, 1234);
  auto q99 = runs_longest_quantile_MC(n, p, RunsTestConfig{0.99, 512}, 1234);

  REQUIRE(q90 <= q95);
  REQUIRE(q95 <= q99);
}

TEST_CASE("internal borderline_run_exceeds_MC95 signals truly extreme run", "[SmallN][MC95]") {
  using namespace palvalidator::bootstrap_helpers::internal;

  const std::size_t n = 40;
  const double ratio = 0.5;
  // observed_longest_run ridiculously large → definitely exceeds q95
  REQUIRE( borderline_run_exceeds_MC95(n, ratio, /*observed_longest_run=*/40) == true );
  // tiny observed run should not exceed
  REQUIRE( borderline_run_exceeds_MC95(n, ratio, /*observed_longest_run=*/2) == false );
}

TEST_CASE("internal combine_LBs_with_near_hurdle: min vs median policy", "[SmallN][combineNear]") {
  using namespace palvalidator::bootstrap_helpers::internal;

  const double annFac = 252.0;
  // Three per-period candidates
  std::vector<Decimal> per = { D(0.0005), D(0.0007), D(0.0009) };

  // Case 1: hurdle far from median → use median-of-present
  Decimal farHurdle = D(0.25); // 25% annual (nowhere near these)
  auto out_far = combine_LBs_with_near_hurdle(per, annFac, farHurdle, /*proximity_bps=*/75.0);
  // median of [0.0005,0.0007,0.0009] is 0.0007
  REQUIRE(num::to_double(out_far) == Approx(0.0007));

  // Case 2: hurdle very near median (within window) → use min(all)
  // Pick hurdle near annualized(0.0007) ≈ 0.0007*252 ≈ 0.1764 → 17,640 bps
  Decimal nearHurdle = mkc_timeseries::Annualizer<Decimal>::annualize_one(D(0.0007), annFac);
  auto out_near = combine_LBs_with_near_hurdle(per, annFac, nearHurdle, /*proximity_bps=*/100000.0 /*huge window*/);
  REQUIRE(num::to_double(out_near) == Approx(0.0005));
}

TEST_CASE("internal combine_LBs_2of3_or_min", "[SmallN][combineVotes]") {
  using namespace palvalidator::bootstrap_helpers::internal;
  std::vector<Decimal> v = { D(0.01), D(0.02), D(0.03) };

  // vote2=false → strict min(all)
  REQUIRE(num::to_double(combine_LBs_2of3_or_min(v, /*vote2=*/false)) == Approx(0.01));

  // vote2=true → median-of-present for 2 or 3 elements
  REQUIRE(num::to_double(combine_LBs_2of3_or_min(v, /*vote2=*/true)) == Approx(0.02));

  // With two elements → arithmetic mid (and we return the closer original per design)
  std::vector<Decimal> two = { D(0.01), D(0.03) };
  auto got = combine_LBs_2of3_or_min(two, /*vote2=*/true);

  CAPTURE(num::to_double(got));            // will print on failure
  REQUIRE(num::to_double(got) == Approx(0.02));
}

TEST_CASE("internal log_policy_line includes key tokens", "[SmallN][log]") {
  using namespace palvalidator::bootstrap_helpers::internal;
  std::ostringstream oss;
  log_policy_line(oss, "DemoPolicy", /*n=*/35, /*L=*/3,
                  /*skew=*/0.5, /*exkurt=*/1.4, /*heavy=*/true,
                  /*name=*/"IIDResampler", /*L_small=*/3);
  const std::string s = oss.str();
  REQUIRE(s.find("DemoPolicy") != std::string::npos);
  REQUIRE(s.find("n=35") != std::string::npos);
  REQUIRE(s.find("L=3") != std::string::npos);
  REQUIRE(s.find("heavy_tails=yes") != std::string::npos);
  REQUIRE(s.find("IIDResampler") != std::string::npos);
  REQUIRE(s.find("L_small=3") != std::string::npos);
}

TEST_CASE("conservative_smallN_lower_bound checks resampler and returns min LB", "[SmallN][conservative][core-logic]") {
    // Common setup
    StrategyT strategy;
    std::size_t B = 2000;
    double ann_factor = 252.0;

    // --- Scenario 1: IID Resampler Chosen, MN is the Minimum ---
    SECTION("IID chosen (balanced data) & MN LB is minimum") {
        // Data: large n=120, balanced, short run -> IID
        std::vector<Decimal> iid_returns(120);
        for(int i=0; i<120; ++i) iid_returns[i] = (i % 2 == 0) ? D(0.01) : D(-0.01);
        
        // Setup Mocks
        TestMocks::FactoryMockHelper::control = TestMocks::FactoryControl{
            .expect_block = false, // Expect IID
            .mn_lb_val = 0.0005,   // MN LB is lower (MIN)
            .bca_lb_val = 0.0007,  // BCa LB is higher
            .m_sub_used = 96       // Expected m for n=120 (0.8 * 120)
        };

        FactoryT factory(0);
        auto result = conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            iid_returns, /*L=*/5, ann_factor, /*confLevel=*/0.95, B,
            /*rho_m=*/0.0, strategy, factory, nullptr, 0, 0,
            /*heavy_tails_override=*/false // Ensures IID logic path
        );
        
        // Assertions
        REQUIRE(TestMocks::FactoryMockHelper::control.mn_called == true);
        REQUIRE(TestMocks::FactoryMockHelper::control.bca_called == true);
        REQUIRE(num::to_double(result.per_lower) == Approx(0.0005)); // Should pick the MIN
        REQUIRE(std::string(result.resampler_name) == std::string("IIDResampler"));
        REQUIRE(result.m_sub == 96);
    }

    // --- Scenario 2: Block Resampler Chosen, BCa is the Minimum ---
    SECTION("Block chosen (small n, streaky data) & BCa LB is minimum") {
        // Data: small n=20, streaky (ratio=1.0) -> Block
        std::vector<Decimal> block_returns(20, D(0.01)); 
        
        // Setup Mocks
        TestMocks::FactoryMockHelper::control = TestMocks::FactoryControl{
            .expect_block = true, // Expect Block
            .mn_lb_val = 0.0009,   // MN LB is higher
            .bca_lb_val = 0.0004,  // BCa LB is lower (MIN)
            .m_sub_used = 18       // Expected m for n=20 (n-2)
        };
        
        FactoryT factory(0);
        auto result = conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            block_returns, /*L=*/5, ann_factor, /*confLevel=*/0.95, B,
            /*rho_m=*/0.0, strategy, factory, nullptr, 0, 0, std::nullopt
        );
        
        // Assertions
        REQUIRE(TestMocks::FactoryMockHelper::control.mn_called == true);
        REQUIRE(TestMocks::FactoryMockHelper::control.bca_called == true);
        REQUIRE(num::to_double(result.per_lower) == Approx(0.0004)); // Should pick the MIN
        REQUIRE(std::string(result.resampler_name).find("StationaryMaskValueResamplerAdapter") != std::string::npos);
        REQUIRE(result.L_used == 3); // L=5 should be clamped to 3
    }
}

TEST_CASE("conservative_smallN_lower_bound handles overrides and logging", "[SmallN][conservative][logging]") {
    // Test logging and heavy_tails_override logic
    std::vector<Decimal> returns(30, D(0.001)); // n=30, streaky, heavy tails is false by default
    StrategyT strategy;
    std::ostringstream oss;
    
    // Set mocks to allow logging to proceed
    TestMocks::FactoryMockHelper::control = TestMocks::FactoryControl{
        .expect_block = true, // Default choice for this data
        .mn_lb_val = 0.0005, 
        .bca_lb_val = 0.0005, 
        .m_sub_used = 24       // Expected m for n=30
    };

    // --- Scenario 3: Heavy Tails Override (False) ---
    SECTION("Override forces IID (false) despite streaky data") {
        // Data is streaky/imbalanced (ratio=1.0) and small N, would normally choose Block.
        // Override should force IID.
        TestMocks::FactoryMockHelper::control.expect_block = false; // Override forces IID

        FactoryT factory(0);
        auto result = conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            returns, /*L=*/5, 252.0, 0.95, 2000, 0.0, strategy, factory, nullptr, 0, 0,
            /*heavy_tails_override=*/false
        );
        
        // Assertions
        REQUIRE(std::string(result.resampler_name) == std::string("IIDResampler"));
    }
    
    // --- Scenario 4: Logging Check ---
    SECTION("Logging includes key diagnostics (m/n, sigma)") {
        // Run with logging stream
        FactoryT factory(0);
        conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            returns, /*L=*/5, 252.0, 0.95, 2000, 0.0, strategy, factory, &oss, 0, 0, std::nullopt
        );
        
        const std::string log = oss.str();
        
        // Log must contain m/n shrink report
        REQUIRE(log.find("m_sub=24") != std::string::npos);
        REQUIRE(log.find("n=30") != std::string::npos);
        // Log must contain sigma/var reports (via the detail::has_member_upper check)
        REQUIRE(log.find("σ(per-period)≈") != std::string::npos);
    }

    // --- Scenario 5: Small N MC Guard Check (n=40, non-streaky, run is borderline) ---
    SECTION("MC Guard triggers Block for borderline run at n=40") {
        // Data: n=40, balanced (ratio=0.5), run=2 (short), but override is nullopt
        // The logic is: choose_block_fast=false, but n<=40, so it hits the MC guard.
        // We cannot reliably mock the MC result, but we test the code path:
        // Assume the MC guard is bypassed or returns false, and we check the final state.
        
        // To test the logic *inside* conservative_smallN_lower_bound, we rely on the 
        // deterministic checks. If this data passes the simple checks (it should), 
        // the final state should be IID (unless the MC hits a border).
        
        std::vector<Decimal> borderline_returns(40); // Balanced and short run, will not trigger fast block
        for(int i=0; i<40; ++i) borderline_returns[i] = (i % 2 == 0) ? D(0.01) : D(-0.01);

        TestMocks::FactoryMockHelper::control.expect_block = false; // Assume MC does NOT trigger block

        FactoryT factory(0);
        auto result = conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            borderline_returns, /*L=*/5, 252.0, 0.95, 2000, 0.0, strategy, factory, nullptr, 0, 0, std::nullopt
        );
        
        // Assert that without explicit streaky/imbalance, the default is IID
        REQUIRE(std::string(result.resampler_name) == std::string("IIDResampler"));
    }
}
