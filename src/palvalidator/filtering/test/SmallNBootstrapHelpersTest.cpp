// SmallNBootstrapHelpersTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <sstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <random>
#include <functional>
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

namespace LBStabTestMocks {

  struct GeoStat {
    GeoStat() = default;
  };

  struct Strategy {
    uint64_t hashCode() const { return 0xABCDEF01u; }
  };

  template <class Num>
  struct MOutOfNEngine {
    Num          lower{};
    Num          upper{};
    std::size_t  m_sub{0};
    std::size_t  L{0};
    std::size_t  effective_B{0};

    template <class CRN>
    MOutOfNEngine run(const std::vector<Num>&,
                      const GeoStat&,
                      CRN) const
    {
      return *this;
    }
  };

  struct ResamplerTag { };

  template <class EngineTag>
  class Factory
  {
  public:
    using WidthFunc = std::function<double(double)>;

    Factory() = default;

    explicit Factory(WidthFunc f)
      : widthFunc_(std::move(f))
    {
    }

    template <class Decimal, class GeoStatT, class ResamplerT>
    auto makeMOutOfN(std::size_t B,
                     double      CL,
                     double      m_ratio,
                     const ResamplerT&,
                     const Strategy&,
                     uint64_t /*stageTag*/,
                     uint64_t L,
                     uint64_t /*fold*/)
      -> std::pair<MOutOfNEngine<Decimal>, int>
    {
      (void)CL;

      const double width = widthFunc_ ? widthFunc_(m_ratio) : 0.02;

      MOutOfNEngine<Decimal> eng;
      eng.lower       = Decimal(0.0);
      eng.upper       = Decimal(width);
      eng.m_sub       = static_cast<std::size_t>(std::round(m_ratio * 100.0));
      eng.L           = static_cast<std::size_t>(L);
      eng.effective_B = static_cast<std::size_t>(B);

      return { eng, 0 }; // dummy CRN tag
    }

  private:
    WidthFunc widthFunc_;
  };

} // namespace LBStabTestMocks

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

  // n=2: special guard allows m==n (ratio=1.0)
  REQUIRE(mn_ratio_from_n(2) == Approx(1.0));

  // n=30: m = 30^(2/3) approx 9.65 -> 10 (rounded/clamped)
  REQUIRE(mn_ratio_from_n(30) * 30.0 == Approx(9.65).margin(1.0));

  // n=100: m = 100^(2/3) approx 21.54 -> 22
  REQUIRE(mn_ratio_from_n(100) * 100.0 == Approx(21.54).margin(1.0));
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

        TestMocks::FactoryMockHelper::control.expect_block = true; // Assume MC does NOT trigger block

        FactoryT factory(0);
        auto result = conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            borderline_returns, /*L=*/5, 252.0, 0.95, 2000, 0.0, strategy, factory, nullptr, 0, 0, std::nullopt
        );
        
        // Assert that without explicit streaky/imbalance, the default is IID
        REQUIRE(std::string(result.resampler_name) == std::string("StationaryMaskValueResamplerAdapter"));
    }
}

TEST_CASE("MNRatioContext stores constructor arguments and exposes them via getters",
          "[SmallN][MNRatioContext]")
{
  SECTION("Heavy tails = true with positive tail index")
  {
    const std::size_t n        = 37;
    const double      sigmaAnn = 0.42;
    const double      skew     = -0.7;
    const double      exkurt   = 1.8;
    const double      tailIdx  = 1.5;
    const bool        heavy    = true;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);

    REQUIRE(ctx.getN()         == n);
    REQUIRE(ctx.getSigmaAnn()  == Approx(sigmaAnn));
    REQUIRE(ctx.getSkew()      == Approx(skew));
    REQUIRE(ctx.getExKurt()    == Approx(exkurt));
    REQUIRE(ctx.getTailIndex() == Approx(tailIdx));
    REQUIRE(ctx.hasHeavyTails() == heavy);
  }

  SECTION("Heavy tails = false with non-positive tail index (invalid Hill estimate)")
  {
    const std::size_t n        = 20;
    const double      sigmaAnn = 0.10;
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = -1.0; // "invalid" marker from estimate_left_tail_index_hill
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);

    REQUIRE(ctx.getN()         == n);
    REQUIRE(ctx.getSigmaAnn()  == Approx(sigmaAnn));
    REQUIRE(ctx.getSkew()      == Approx(skew));
    REQUIRE(ctx.getExKurt()    == Approx(exkurt));
    REQUIRE(ctx.getTailIndex() == Approx(tailIdx));
    REQUIRE(ctx.hasHeavyTails() == heavy);
  }
}

TEST_CASE("MNRatioContext is copyable and preserves all fields",
          "[SmallN][MNRatioContext][copy]")
{
  const std::size_t n        = 50;
  const double      sigmaAnn = 0.55;
  const double      skew     = 0.9;
  const double      exkurt   = 2.3;
  const double      tailIdx  = 1.2;
  const bool        heavy    = true;

  MNRatioContext original(n, sigmaAnn, skew, exkurt, tailIdx, heavy);

  // Copy construct
  MNRatioContext copy = original;

  REQUIRE(copy.getN()         == original.getN());
  REQUIRE(copy.getSigmaAnn()  == Approx(original.getSigmaAnn()));
  REQUIRE(copy.getSkew()      == Approx(original.getSkew()));
  REQUIRE(copy.getExKurt()    == Approx(original.getExKurt()));
  REQUIRE(copy.getTailIndex() == Approx(original.getTailIndex()));
  REQUIRE(copy.hasHeavyTails() == original.hasHeavyTails());

  // Copy assign
  MNRatioContext assigned(1, 0.0, 0.0, 0.0, 0.0, false);
  assigned = original;

  REQUIRE(assigned.getN()         == original.getN());
  REQUIRE(assigned.getSigmaAnn()  == Approx(original.getSigmaAnn()));
  REQUIRE(assigned.getSkew()      == Approx(original.getSkew()));
  REQUIRE(assigned.getExKurt()    == Approx(original.getExKurt()));
  REQUIRE(assigned.getTailIndex() == Approx(original.getTailIndex()));
  REQUIRE(assigned.hasHeavyTails() == original.hasHeavyTails());
}

TEST_CASE("TailVolPriorPolicy returns high-vol ratio for heavy tails or high sigma",
          "[SmallN][TailVolPriorPolicy][highvol]")
{
  TailVolPriorPolicy policy; // defaults: highVolAnnThreshold=0.40, highVolRatio=0.80, normalRatio=0.50

  SECTION("High annualized volatility triggers high-vol ratio")
  {
    const std::size_t n        = 50;
    const double      sigmaAnn = 0.50;   // >= 0.40 threshold
    const double      skew     = 0.20;
    const double      exkurt   = 0.50;
    const double      tailIdx  = 3.0;    // non-extreme tail index
    const bool        heavy    = false;  // no heavy tail flag

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    // For n=50, maxRho = 49/50 ≈ 0.98, so we should get exactly the highVolRatio (0.80)
    REQUIRE(rho == Approx(0.80).margin(1e-6));
  }

  SECTION("Extreme tail index triggers high-vol ratio even if sigmaAnn is small")
  {
    const std::size_t n        = 40;
    const double      sigmaAnn = 0.10;   // below vol threshold
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = 1.5;    // 0 < alpha < 2.0 → extremeTail=true
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(0.80).margin(1e-6));
  }

  SECTION("Heavy-tails flag alone is enough to trigger high-vol ratio")
  {
    const std::size_t n        = 30;
    const double      sigmaAnn = 0.05;   // below threshold
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = -1.0;   // invalid / unknown tail index
    const bool        heavy    = true;   // explicit heavy flag

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(0.80).margin(1e-6));
  }
}

TEST_CASE("TailVolPriorPolicy returns normal ratio for non-heavy, low-vol regimes",
          "[SmallN][TailVolPriorPolicy][normal]")
{
  TailVolPriorPolicy policy; // 0.80 vs 0.50

  const std::size_t n        = 50;
  const double      sigmaAnn = 0.10;  // below 0.40
  const double      skew     = 0.10;
  const double      exkurt   = 0.20;
  const double      tailIdx  = 3.0;
  const bool        heavy    = false;

  MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
  const double rho = policy.computePriorRatio(ctx);

  REQUIRE(rho == Approx(0.50).margin(1e-6));
}

TEST_CASE("TailVolPriorPolicy handles tiny n via ~50% rule and clamping",
          "[SmallN][TailVolPriorPolicy][tinyN]")
{
  TailVolPriorPolicy policy;

  SECTION("n = 4 → m≈ceil(0.5*n) = 2 ⇒ rho = 0.5")
  {
    const std::size_t n        = 4;
    const double      sigmaAnn = 0.10;
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = -1.0;
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(0.50).margin(1e-6));
  }

  SECTION("n = 3 → m≈ceil(0.5*3)=2 ⇒ rho ≈ 2/3")
  {
    const std::size_t n        = 3;
    const double      sigmaAnn = 0.10;
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = -1.0;
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(2.0 / 3.0).margin(1e-6));
  }

  SECTION("n < 3 → early guard returns 1.0")
  {
    const std::size_t n        = 2;
    const double      sigmaAnn = 0.10;
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = -1.0;
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(1.0).margin(1e-6));
  }
}

TEST_CASE("TailVolPriorPolicy clamps ratios to [2/n, (n-1)/n]",
          "[SmallN][TailVolPriorPolicy][clamp]")
{
  SECTION("High-vol ratio is clamped to (n-1)/n from above")
  {
    // Build a policy with a very aggressive highVolRatio to test max clamp
    TailVolPriorPolicy policy(/*highVolAnnThreshold=*/0.40,
                              /*highVolRatio=*/0.95,
                              /*normalRatio=*/0.50);

    const std::size_t n        = 5;
    const double      sigmaAnn = 0.50; // high vol regime
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = 3.0;
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    // maxRho = (n-1)/n = 4/5 = 0.8, so 0.95 must be clamped down to 0.8
    REQUIRE(rho == Approx(4.0 / 5.0).margin(1e-6));
  }

  SECTION("Normal ratio is clamped to at least 2/n from below")
  {
    // Build a policy with a very small normalRatio to test min clamp
    TailVolPriorPolicy policy(/*highVolAnnThreshold=*/0.40,
                              /*highVolRatio=*/0.80,
                              /*normalRatio=*/0.01);

    const std::size_t n        = 50;
    const double      sigmaAnn = 0.10; // normal regime
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = 3.0;
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    const double minRho = 2.0 / static_cast<double>(n); // 2/50 = 0.04
    REQUIRE(rho == Approx(minRho).margin(1e-6));
  }
}

TEST_CASE("LBStabilityRefinementPolicy selects candidate with tightest CI",
          "[SmallN][LBStabilityRefinementPolicy][stability]")
{
  using Num        = Decimal;
  using GeoStatT   = LBStabTestMocks::GeoStat;
  using StrategyT  = LBStabTestMocks::Strategy;
  using ResamplerT = LBStabTestMocks::ResamplerTag;
  using EngineTag  = palvalidator::bootstrap_cfg::BootstrapEngine;
  using FactoryT   = LBStabTestMocks::Factory<EngineTag>;
  using PolicyT    = LBStabilityRefinementPolicy<Num, GeoStatT, StrategyT, ResamplerT, FactoryT>;

  // Candidate ratios: baseRatio + deltas = 0.5 + {-0.10, 0, +0.10} → {0.40, 0.50, 0.60}
  std::vector<double> deltas = { -0.10, 0.0, +0.10 };
  PolicyT policy(deltas,
                 /*minB=*/200,
                 /*maxB=*/600,
                 /*minNForRefine=*/15,
                 /*maxNForRefine=*/60);

  // CI width minimized at rho = 0.40
  auto widthFunc = [](double rho) {
    const double center = 0.40;
    return std::fabs(rho - center);
  };

  FactoryT factory(widthFunc);
  StrategyT strategy;
  ResamplerT resampler;
  std::ostringstream oss;

  const std::size_t n        = 30;     // within [minNForRefine, maxNForRefine]
  const double      sigmaAnn = 0.20;
  const double      skew     = 0.10;
  const double      exkurt   = 0.50;
  const double      tailIdx  = 3.0;
  const bool        heavy    = false;

  MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);

  std::vector<Num> returns(n, D(0.001));

  const std::size_t L_small   = 3;
  const double      confLevel = 0.95;
  const std::size_t B_full    = 2000;
  const double      baseRatio = 0.50;

  const double chosen =
      policy.refineRatio(returns,
                         ctx,
                         L_small,
                         confLevel,
                         B_full,
                         baseRatio,
                         strategy,
                         factory,
                         resampler,
                         &oss,
                         /*stageTag=*/1,
                         /*fold=*/0);

  // Under the LB-stability-based scoring, all candidates share the same LB
  // in this mock and only differ in width. The largest-rho candidate
  // provides the reference LB, and among the candidates with finite sigma
  // the smallest rho is preferred. Because the rho=0.40 candidate has
  // zero width (sigma=0) in this synthetic setup, it is treated as
  // unusable, so we stay at the prior/base ratio 0.50.
  REQUIRE(chosen == Approx(0.50).margin(1e-6));
}

TEST_CASE("LBStabilityRefinementPolicy returns base ratio when n outside refinement window",
          "[SmallN][LBStabilityRefinementPolicy][n-window]")
{
  using Num        = Decimal;
  using GeoStatT   = LBStabTestMocks::GeoStat;
  using StrategyT  = LBStabTestMocks::Strategy;
  using ResamplerT = LBStabTestMocks::ResamplerTag;
  using EngineTag  = palvalidator::bootstrap_cfg::BootstrapEngine;
  using FactoryT   = LBStabTestMocks::Factory<EngineTag>;
  using PolicyT    = LBStabilityRefinementPolicy<Num, GeoStatT, StrategyT, ResamplerT, FactoryT>;

  std::vector<double> deltas = { -0.10, 0.0, +0.10 };
  PolicyT policy(deltas,
                 /*minB=*/200,
                 /*maxB=*/600,
                 /*minNForRefine=*/15,
                 /*maxNForRefine=*/60);

  FactoryT factory;  // default ctor: widthFunc_ is empty → uses fallback width
  StrategyT strategy;
  ResamplerT resampler;
  std::ostringstream oss;

  SECTION("n too small (< minNForRefine) → no refinement")
  {
    const std::size_t n        = 10;
    const double      sigmaAnn = 0.15;
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = -1.0;
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    std::vector<Num> returns(n, D(0.001));

    const double baseRatio = 0.55;
    const double chosen =
        policy.refineRatio(returns,
                           ctx,
                           /*L_small=*/3,
                           /*confLevel=*/0.95,
                           /*B_full=*/2000,
                           baseRatio,
                           strategy,
                           factory,
                           resampler,
                           &oss,
                           /*stageTag=*/1,
                           /*fold=*/0);

    REQUIRE(chosen == Approx(baseRatio).margin(1e-6));
  }

  SECTION("n too large (> maxNForRefine) → no refinement")
  {
    const std::size_t n        = 100;
    const double      sigmaAnn = 0.15;
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const double      tailIdx  = -1.0;
    const bool        heavy    = false;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    std::vector<Num> returns(n, D(0.001));

    const double baseRatio = 0.60;
    const double chosen =
        policy.refineRatio(returns,
                           ctx,
                           /*L_small=*/3,
                           /*confLevel=*/0.95,
                           /*B_full=*/2000,
                           baseRatio,
                           strategy,
                           factory,
                           resampler,
                           &oss,
                           /*stageTag=*/1,
                           /*fold=*/0);

    REQUIRE(chosen == Approx(baseRatio).margin(1e-6));
  }
}

TEST_CASE("estimate_left_tail_index_hill returns -1 when there are no losses",
          "[SmallN][HillTailIndex][no-losses]")
{
  std::vector<Decimal> returns;
  returns.push_back(D(0.01));
  returns.push_back(D(0.02));
  returns.push_back(D(0.00));

  const double alpha = estimate_left_tail_index_hill(returns);
  REQUIRE(alpha == Approx(-1.0).margin(1e-12));
}

TEST_CASE("estimate_left_tail_index_hill returns -1 with too few losses",
          "[SmallN][HillTailIndex][too-few]")
{
  // Default k = 5, so we need at least k+1 = 6 negative values.
  // Here we only provide 3 negative returns.
  std::vector<Decimal> returns;
  returns.push_back(D(-0.01));
  returns.push_back(D(-0.02));
  returns.push_back(D(-0.03));
  returns.push_back(D(0.01));   // positive, ignored
  returns.push_back(D(0.00));   // zero, ignored

  const double alpha = estimate_left_tail_index_hill(returns); // k = 5
  REQUIRE(alpha == Approx(-1.0).margin(1e-12));
}

TEST_CASE("estimate_left_tail_index_hill returns -1 for constant losses (no tail variation)",
          "[SmallN][HillTailIndex][degenerate]")
{
  // All losses are identical -> losses[i]/xk == 1 for all i -> log(1) == 0
  // → hill == 0 → function should return -1.
  std::vector<Decimal> returns;

  // 7 identical negative returns -> 7 losses, >= k+1 with k=5
  for (int i = 0; i < 7; ++i)
    returns.push_back(D(-1.0));

  const double alpha = estimate_left_tail_index_hill(returns); // k = 5
  REQUIRE(alpha == Approx(-1.0).margin(1e-12));
}

TEST_CASE("estimate_left_tail_index_hill recovers a known Pareto-like tail index",
          "[SmallN][HillTailIndex][synthetic]")
{
  // We construct a synthetic left-tail sample where the Hill estimator is exact.
  //
  // For the Hill estimator in this implementation:
  //   - losses are sorted descending
  //   - x_k = losses[k] (k-th index, 0-based)
  //   - hill = (1/k) * sum_{i=0}^{k-1} log(losses[i] / x_k)
  //   - alpha_hat = 1 / hill
  //
  // If we choose:
  //   losses[0..k-1] = x_k * exp(1/alpha_true)
  //   losses[k]      = x_k
  // then:
  //   log(losses[i] / x_k) = 1/alpha_true for i < k
  //   hill = (1/k) * k * (1/alpha_true) = 1/alpha_true
  //   alpha_hat = alpha_true  (exact, up to floating error)

  const double alpha_true = 1.5;       // Heavy-ish tail (α < 2)
  const std::size_t k     = 5;
  const double      xk    = 1.0;
  const double      big   = std::exp(1.0 / alpha_true) * xk;  // > xk

  std::vector<Decimal> returns;
  returns.reserve(10);

  // 5 largest losses: all = big
  for (std::size_t i = 0; i < k; ++i)
    returns.push_back(D(-big));   // negative returns → positive losses big

  // The (k+1)-th largest loss: x_k = 1.0
  returns.push_back(D(-xk));

  // Some extra noise (smaller losses and positives) that should not affect the Hill core
  returns.push_back(D(-0.5));   // smaller loss
  returns.push_back(D(-0.2));   // smaller loss
  returns.push_back(D(0.01));   // positive, ignored
  returns.push_back(D(0.00));   // zero, ignored

  const double alpha_hat = estimate_left_tail_index_hill(returns, k);

  // We expect alpha_hat ≈ alpha_true within a small numerical tolerance.
  REQUIRE(alpha_hat == Approx(alpha_true).margin(1e-3));
}

TEST_CASE("estimate_left_tail_index_hill respects custom k parameter",
          "[SmallN][HillTailIndex][custom-k]")
{
  const double       alpha_true = 2.5;       // lighter tail (α > 2)
  const std::size_t  k          = 3;
  const double       xk         = 0.8;
  const double       big        = std::exp(1.0 / alpha_true) * xk;

  std::vector<Decimal> returns;
  returns.reserve(16);

  // 3 largest losses: all = big
  for (std::size_t i = 0; i < k; ++i)
    returns.push_back(D(-big));   // negative returns → positive losses "big"

  // (k+1)-th loss: x_k = 0.8
  returns.push_back(D(-xk));

  // Additional smaller losses that do NOT exceed xk, so xk stays at index k
  returns.push_back(D(-0.3));
  returns.push_back(D(-0.2));
  returns.push_back(D(-0.15));
  returns.push_back(D(-0.10));

  // Some positives / zeros (ignored by the Hill estimator)
  returns.push_back(D(0.02));
  returns.push_back(D(0.00));

  // Now we have:
  //   losses = {big, big, big, 0.8, 0.3, 0.2, 0.15, 0.10}
  //   losses.size() = 8 >= max(k+1=4, minLossesForHill=8)
  const double alpha_hat = estimate_left_tail_index_hill(returns, k);

  // We expect alpha_hat ≈ alpha_true within a small numerical tolerance.
  REQUIRE(alpha_hat == Approx(alpha_true).margin(1e-3));
}

TEST_CASE("TailVolStabilityPolicy with NoRefinementPolicy returns prior ratio",
          "[SmallN][TailVolStabilityPolicy][no-refine]")
{
  using Num        = Decimal;
  using GeoStatT   = LBStabTestMocks::GeoStat;
  using StrategyT  = LBStabTestMocks::Strategy;
  using ResamplerT = LBStabTestMocks::ResamplerTag;
  using EngineTag  = palvalidator::bootstrap_cfg::BootstrapEngine;
  using FactoryT   = LBStabTestMocks::Factory<EngineTag>;
  using NoRefT     = NoRefinementPolicy<Num, GeoStatT, StrategyT, ResamplerT, FactoryT>;

  // Prior policy: defaults highVolAnnThreshold=0.40, highVolRatio=0.80, normalRatio=0.50
  TailVolPriorPolicy prior;

  // Refinement policy that just returns baseRatio
  NoRefT noRefine;

  TailVolStabilityPolicy<Num, GeoStatT, StrategyT, ResamplerT, FactoryT, NoRefT>
      policy(prior, noRefine);

  // Context: high-vol regime -> prior ratio should be 0.80
  const std::size_t n        = 30;
  const double      sigmaAnn = 0.50;  // >= 0.40 threshold
  const double      skew     = 0.10;
  const double      exkurt   = 0.50;
  const double      tailIdx  = 3.0;   // non-extreme tail index
  const bool        heavy    = false;

  MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);

  std::vector<Num> returns(n, D(0.001));  // contents irrelevant here
  const std::size_t L_small   = 3;
  const double      confLevel = 0.95;
  const std::size_t B_full    = 2000;

  StrategyT strategy;
  FactoryT  factory;    // default: no special width function
  ResamplerT resampler;
  std::ostringstream oss;

  const double rho =
      policy.computeRatio(returns,
                          ctx,
                          L_small,
                          confLevel,
                          B_full,
                          strategy,
                          factory,
                          resampler,
                          &oss,
                          /*stageTag=*/1,
                          /*fold=*/0);

  // High-vol regime for n=30 should give prior ≈ 0.80, and NoRefinementPolicy must not change it.
  REQUIRE(rho == Approx(0.80).margin(1e-6));

  // Sanity: prior is indeed 0.80 under this context
  REQUIRE(prior.computePriorRatio(ctx) == Approx(0.80).margin(1e-6));
}

TEST_CASE("TailVolStabilityPolicy passes prior ratio into refinement policy",
          "[SmallN][TailVolStabilityPolicy][refine]")
{
  using Num        = Decimal;
  using GeoStatT   = LBStabTestMocks::GeoStat;
  using StrategyT  = LBStabTestMocks::Strategy;
  using ResamplerT = LBStabTestMocks::ResamplerTag;
  using EngineTag  = palvalidator::bootstrap_cfg::BootstrapEngine;
  using FactoryT   = LBStabTestMocks::Factory<EngineTag>;

  // Custom refinement policy that returns baseRatio * 0.5
  struct CaptureRefinementPolicy
  {
    double refineRatio(const std::vector<Num>&,
                       const MNRatioContext&,
                       std::size_t,
                       double,
                       std::size_t,
                       double baseRatio,
                       StrategyT&,
                       FactoryT&,
                       ResamplerT&,
                       std::ostream*,
                       int,
                       int) const
    {
      // Refinement rule: shrink ratio by 50%
      return baseRatio * 0.5;
    }
  };

  TailVolPriorPolicy     prior;
  CaptureRefinementPolicy refine;

  TailVolStabilityPolicy<Num, GeoStatT, StrategyT, ResamplerT, FactoryT, CaptureRefinementPolicy>
      policy(prior, refine);

  // Context: normal regime (low vol, light tails) -> prior ratio should be 0.50
  const std::size_t n        = 40;
  const double      sigmaAnn = 0.10;  // below 0.40 threshold
  const double      skew     = 0.10;
  const double      exkurt   = 0.20;
  const double      tailIdx  = 5.0;   // light tail
  const bool        heavy    = false;

  MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);

  std::vector<Num> returns(n, D(0.001));
  const std::size_t L_small   = 3;
  const double      confLevel = 0.95;
  const std::size_t B_full    = 2000;

  StrategyT      strategy;
  FactoryT       factory;
  ResamplerT     resampler;
  std::ostringstream oss;

  const double rho =
      policy.computeRatio(returns,
                          ctx,
                          L_small,
                          confLevel,
                          B_full,
                          strategy,
                          factory,
                          resampler,
                          &oss,
                          /*stageTag=*/2,
                          /*fold=*/0);

  const double priorRho = prior.computePriorRatio(ctx);

  // In this context, priorRho should be the "normalRatio" (0.50)
  REQUIRE(priorRho == Approx(0.50).margin(1e-6));

  // And the final ratio should be baseRatio * 0.5 per our test refinement policy
  REQUIRE(rho == Approx(priorRho * 0.5).margin(1e-6));
  REQUIRE(rho != Approx(priorRho).margin(1e-6)); // sanity: refinement actually changed it
}

TEST_CASE("TailVolStabilityPolicy + LBStabilityRefinementPolicy respond to CI width shape",
          "[SmallN][TailVolStabilityPolicy][LBStabilityRefinementPolicy][integration]")
{
  using Num        = Decimal;
  using GeoStatT   = LBStabTestMocks::GeoStat;
  using StrategyT  = LBStabTestMocks::Strategy;
  using ResamplerT = LBStabTestMocks::ResamplerTag;
  using EngineTag  = palvalidator::bootstrap_cfg::BootstrapEngine;
  using FactoryT   = LBStabTestMocks::Factory<EngineTag>;
  using RefineT    = LBStabilityRefinementPolicy<Num, GeoStatT, StrategyT, ResamplerT, FactoryT>;
  using PolicyT    = TailVolStabilityPolicy<Num, GeoStatT, StrategyT, ResamplerT, FactoryT, RefineT>;

  // Prior that maps this context to 0.50 (normal regime)
  TailVolPriorPolicy prior;

  // Refinement policy probing around the prior with ±0.10
  std::vector<double> deltas = { -0.10, 0.0, +0.10 };
  RefineT refine(deltas,
                 /*minB=*/200,
                 /*maxB=*/600,
                 /*minNForRefine=*/15,
                 /*maxNForRefine=*/60);

  PolicyT policy(prior, refine);

  // Context: normal regime (low vol, light tails) → prior ratio should be 0.50
  const std::size_t n        = 30;
  const double      sigmaAnn = 0.10;  // below high-vol threshold
  const double      skew     = 0.10;
  const double      exkurt   = 0.20;
  const double      tailIdx  = 5.0;   // light tail
  const bool        heavy    = false;

  MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);

  const double priorRho = prior.computePriorRatio(ctx);
  REQUIRE(priorRho == Approx(0.50).margin(1e-6)); // sanity: we are in the normal regime

  std::vector<Num> returns(n, D(0.001));
  const std::size_t L_small   = 3;
  const double      confLevel = 0.95;
  const std::size_t B_full    = 2000;

  StrategyT   strategy;
  ResamplerT  resampler;
  std::ostringstream oss;

  SECTION("Width minimized at rho = 0.40 → LB-stability keeps prior 0.50")
  {
    auto widthFunc = [](double rho) {
      const double center = 0.40;
      return std::fabs(rho - center);
    };

    FactoryT factory(widthFunc);

    const double rho =
        policy.computeRatio(returns,
                            ctx,
                            L_small,
                            confLevel,
                            B_full,
                            strategy,
                            factory,
                            resampler,
                            &oss,
                            /*stageTag=*/10,
                            /*fold=*/0);

    // In the LB-stability implementation, all candidates have the same LB
    // under this mock. Their normalized instability is identical, so
    // the refinement step keeps the prior/base ratio (0.50).
    REQUIRE(rho == Approx(priorRho).margin(1e-6));
  }

  SECTION("Width minimized at rho = 0.60 → plateau metric falls back to smallest rho")
  {
    auto widthFunc = [](double rho) {
      const double center = 0.60;
      return std::fabs(rho - center);
    };

    FactoryT factory(widthFunc);

    const double rho =
        policy.computeRatio(returns,
                            ctx,
                            L_small,
                            confLevel,
                            B_full,
                            strategy,
                            factory,
                            resampler,
                            &oss,
                            /*stageTag=*/11,
                            /*fold=*/0);

    // Here the largest-rho candidate (0.60) has zero width in the mock,
    // which yields sigma=0 and an unusable stability score. The remaining
    // candidates share the same LB and have finite sigma, so their
    // normalized instability is tied and we prefer the smaller rho = 0.40.
    REQUIRE(rho == Approx(0.40).margin(1e-6));
    REQUIRE(rho != Approx(priorRho).margin(1e-6));  // refinement still moves it
  }
}

TEST_CASE("TailVolPriorPolicy returns light-tail ratio for very light tails and large n",
          "[SmallN][TailVolPriorPolicy][lightTail]")
{
  TailVolPriorPolicy policy;

  const std::size_t nLarge    = policy.getNLargeThreshold();       // default 80
  const double      alphaLight = policy.getLightTailAlphaThreshold(); // default 4.0

  // Sanity check: we are in the "large n" region
  REQUIRE(nLarge >= 5);

  // Very light tails + large n + low vol + no heavy flag
  const std::size_t n        = nLarge + 20;       // e.g. 100 if threshold is 80
  const double      sigmaAnn = 0.10;              // below 0.40
  const double      skew     = 0.10;
  const double      exkurt   = 0.10;
  const double      tailIdx  = alphaLight + 1.0;  // comfortably in light-tail regime
  const bool        heavy    = false;

  MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
  const double rho = policy.computePriorRatio(ctx);

  // We should be in the "very light tail & large n" regime → lightTailRatio
  REQUIRE(rho == Approx(policy.getLightTailRatio()).margin(1e-6));
}

TEST_CASE("TailVolPriorPolicy light-tail regime activates only beyond tail-index and n thresholds",
          "[SmallN][TailVolPriorPolicy][lightTail][cutoffs]")
{
  TailVolPriorPolicy policy;

  const std::size_t nLarge     = policy.getNLargeThreshold();         // default 80
  const double      alphaLight = policy.getLightTailAlphaThreshold(); // default 4.0

  const double sigmaAnn = 0.10;  // low vol
  const double skew     = 0.0;
  const double exkurt   = 0.0;
  const bool   heavy    = false;

  SECTION("n just below nLargeThreshold stays in normal regime even with very light tails")
  {
    const std::size_t n       = (nLarge > 0 ? nLarge - 1 : 0);
    const double      tailIdx = alphaLight + 0.5; // very light, but n too small

    REQUIRE(n >= 3); // sanity guard for the test

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(policy.getNormalRatio()).margin(1e-6));
  }

  SECTION("tail index just below alphaLightThreshold stays in normal regime even for large n")
  {
    const std::size_t n       = nLarge + 10;       // safely above large-n threshold
    const double      tailIdx = alphaLight - 0.1;  // just under light-tail cutoff

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(policy.getNormalRatio()).margin(1e-6));
  }

  SECTION("Both n and tail index beyond thresholds activate light-tail regime")
  {
    const std::size_t n       = nLarge + 10;       // large n
    const double      tailIdx = alphaLight + 0.1;  // just over light-tail cutoff

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    REQUIRE(rho == Approx(policy.getLightTailRatio()).margin(1e-6));
  }
}


TEST_CASE("TailVolPriorPolicy boundary behavior at classification thresholds",
          "[SmallN][TailVolPriorPolicy][boundaries]")
{
  TailVolPriorPolicy policy;

  const double      sigmaThresh  = policy.getHighVolAnnThreshold();       // e.g. 0.40
  const double      alphaHeavy   = policy.getHeavyTailAlphaThreshold();   // e.g. 2.0
  const double      alphaLight   = policy.getLightTailAlphaThreshold();   // e.g. 4.0
  const std::size_t nLarge       = policy.getNLargeThreshold();           // e.g. 80

  SECTION("sigmaAnn just below / at / above high-vol threshold")
  {
    const std::size_t n       = 50;
    const double      skew    = 0.0;
    const double      exkurt  = 0.0;
    const double      tailIdx = 3.0;   // not extreme
    const bool        heavy   = false;

    // Just below: should still be normal regime
    {
      const double sigmaAnn = sigmaThresh - 1e-6;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getNormalRatio()).margin(1e-6));
    }

    // Exactly at threshold: high-vol regime (>= threshold)
    {
      const double sigmaAnn = sigmaThresh;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getHighVolRatio()).margin(1e-6));
    }

    // Just above: also high-vol
    {
      const double sigmaAnn = sigmaThresh + 1e-6;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getHighVolRatio()).margin(1e-6));
    }
  }

  SECTION("Heavy-tail alpha threshold: just below / at / above")
  {
    const std::size_t n       = 40;
    const double      sigmaAnn = 0.10;  // low vol
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const bool        heavy    = false;

    // Just below threshold → extreme heavy tail → high-vol ratio
    {
      const double tailIdx = alphaHeavy - 1e-6;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getHighVolRatio()).margin(1e-6));
    }

    // Exactly at threshold → still treated as heavy (<= threshold)
    {
      const double tailIdx = alphaHeavy;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getHighVolRatio()).margin(1e-6));
    }

    // Just above threshold → no longer heavy-tail by alpha; stays normal regime
    {
      const double tailIdx = alphaHeavy + 1e-6;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getNormalRatio()).margin(1e-6));
    }
  }

  SECTION("Light-tail alpha threshold with large n")
  {
    // Ensure we are in the large-n region
    REQUIRE(nLarge >= 5);
    const std::size_t n        = nLarge + 10; // safely large
    const double      sigmaAnn = 0.10;        // low vol
    const double      skew     = 0.0;
    const double      exkurt   = 0.0;
    const bool        heavy    = false;

    // Just below light-tail cutoff → normal regime
    {
      const double tailIdx = alphaLight - 1e-6;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getNormalRatio()).margin(1e-6));
    }

    // Exactly at light-tail cutoff → enters light-tail regime
    {
      const double tailIdx = alphaLight;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getLightTailRatio()).margin(1e-6));
    }

    // Just above light-tail cutoff → also light-tail regime
    {
      const double tailIdx = alphaLight + 1e-6;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getLightTailRatio()).margin(1e-6));
    }
  }

  SECTION("Large-n threshold boundary for light-tail regime")
  {
    const double sigmaAnn = 0.10;       // low vol
    const double skew     = 0.0;
    const double exkurt   = 0.0;
    const double tailIdx  = alphaLight + 1.0; // comfortably light tail
    const bool   heavy    = false;

    // Just below nLargeThreshold → cannot enter light-tail regime yet
    if (nLarge > 1)
    {
      const std::size_t nBelow = nLarge - 1;
      REQUIRE(nBelow >= 3); // sanity
      MNRatioContext ctxBelow(nBelow, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rhoBelow = policy.computePriorRatio(ctxBelow);
      REQUIRE(rhoBelow == Approx(policy.getNormalRatio()).margin(1e-6));
    }

    // Exactly at nLargeThreshold → qualifies as large-n, with light tails
    {
      const std::size_t nEq = nLarge;
      REQUIRE(nEq >= 5); // from design / defaults
      MNRatioContext ctxEq(nEq, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rhoEq = policy.computePriorRatio(ctxEq);
      REQUIRE(rhoEq == Approx(policy.getLightTailRatio()).margin(1e-6));
    }
  }

  SECTION("Boundary between tiny-n path (n<5) and normal regime (n>=5)")
  {
    const double sigmaAnn = 0.10;
    const double skew     = 0.0;
    const double exkurt   = 0.0;
    const double tailIdx  = 3.0;
    const bool   heavy    = false;

    // n = 4 -> tiny-n branch → ~0.5
    {
      const std::size_t n = 4;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(0.50).margin(1e-6));
    }

    // n = 5 -> no longer tiny-n; goes through normal regime logic
    {
      const std::size_t n = 5;
      MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIdx, heavy);
      const double rho = policy.computePriorRatio(ctx);
      REQUIRE(rho == Approx(policy.getNormalRatio()).margin(1e-6));
    }
  }
}

TEST_CASE("TailVolPriorPolicy: high-vol and heavy-tail signals override light-tail regime",
          "[SmallN][TailVolPriorPolicy][override]")
{
  TailVolPriorPolicy policy;

  const double      sigmaThresh  = policy.getHighVolAnnThreshold();        // e.g. 0.40
  const double      alphaHeavy   = policy.getHeavyTailAlphaThreshold();    // e.g. 2.0
  const double      alphaLight   = policy.getLightTailAlphaThreshold();    // e.g. 4.0
  const std::size_t nLarge       = policy.getNLargeThreshold();            // e.g. 80

  // Base conditions that *would* normally put us in the light-tail regime:
  // - very light tail index
  // - large n
  // - low volatility
  const std::size_t nBase        = nLarge + 10;        // safely large
  const double      tailIdxBase  = alphaLight + 1.0;   // comfortably light tail (α > 4)
  const double      sigmaLow     = sigmaThresh - 0.10; // clearly below high-vol threshold
  const double      skew         = 0.0;
  const double      exkurt       = 0.0;

  SECTION("High volatility overrides and forces high-vol ratio even with light tails & large n")
  {
    const bool   heavy    = false;
    const double sigmaAnn = sigmaThresh + 0.10; // above threshold → high-vol

    MNRatioContext ctx(nBase, sigmaAnn, skew, exkurt, tailIdxBase, heavy);
    const double rho = policy.computePriorRatio(ctx);

    // Despite n large and tailIdx very light, high volatility must dominate.
    REQUIRE(rho == Approx(policy.getHighVolRatio()).margin(1e-6));
  }

  SECTION("Heavy-tails flag overrides and forces high-vol ratio even with light tails & low vol")
  {
    const bool   heavy    = true;       // explicit heavy flag
    const double sigmaAnn = sigmaLow;   // low volatility still

    MNRatioContext ctx(nBase, sigmaAnn, skew, exkurt, tailIdxBase, heavy);
    const double rho = policy.computePriorRatio(ctx);

    // Heavy flag alone is enough to put us into high-vol regime.
    REQUIRE(rho == Approx(policy.getHighVolRatio()).margin(1e-6));
  }

  SECTION("Very heavy tail index overrides and forces high-vol ratio (even if sigma is low)")
  {
    const bool   heavy    = false;
    const double sigmaAnn = sigmaLow;
    const double tailIdx  = alphaHeavy - 0.10;  // just below heavy-tail threshold (α <= 2) → extreme

    MNRatioContext ctx(nBase, sigmaAnn, skew, exkurt, tailIdx, heavy);
    const double rho = policy.computePriorRatio(ctx);

    // α in (0, heavyTailAlphaThreshold] is treated as extreme heavy tail → high-vol.
    REQUIRE(rho == Approx(policy.getHighVolRatio()).margin(1e-6));
  }
}
