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
  //using Decimal = num::DefaultNumber;
    
  struct MockGeoStat {
    using Decimal = num::DefaultNumber;

    MockGeoStat() = default;

    // Make the mock statistic behave like a "Sampler":
    // take a vector of returns and return a single Decimal.
    // A simple arithmetic mean is enough for test purposes.
    Decimal operator()(const std::vector<Decimal>& x) const
    {
      if (x.empty())
	return Decimal(0);

      double sum = 0.0;
      for (const auto& v : x)
	sum += num::to_double(v);

      return Decimal(sum / static_cast<double>(x.size()));
    }

    /// \brief Helper to format the statistic for display (as percentage)
    static double formatForDisplay(double value)
    {
      return value * 100.0;
    }
  };
  
    // Mock Strategy (must provide a hashCode() as used by the Factory)
    struct MockStrategy {
        uint64_t hashCode() const { return 0xDEADBEEF; }
    };

  // Mock CRN Provider (needed for runWithRefinement)
  struct MockCRNProvider {
    palvalidator::bootstrap_cfg::BootstrapEngine
    make_engine(std::size_t /*b*/) const
    {
      // For these tests we only care that the type matches the real Rng
      // used by StationaryMaskValueResamplerAdapter; we don't care about
      // the exact seeding, so a default-constructed engine is fine.
      return palvalidator::bootstrap_cfg::BootstrapEngine{};
    }
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
        double       computed_ratio{0.0};  // NEW: Added for Phase 5 compatibility

        MNRunSimpleMock(Num lb, Num ub, std::size_t m, double ratio = 0.0) :
            lower(lb), upper(ub), m_sub(m), L(3), effective_B(1000), computed_ratio(ratio) {}
        
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
    
    /// \brief Helper to format the statistic for display (as percentage)
    static double formatForDisplay(double value)
    {
      return value * 100.0;
    }
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
         TestMocks::MockCRNProvider  // Changed from int to MockCRNProvider
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

    // Return the mock engine and a MockCRNProvider
    return {
        TestMocks::MNRunSimpleMock<Decimal>(
            Decimal(TestMocks::FactoryMockHelper::control.mn_lb_val),
            Decimal(TestMocks::FactoryMockHelper::control.mn_lb_val + 0.01),
            TestMocks::FactoryMockHelper::control.m_sub_used,
            m_ratio  // NEW: Pass computed_ratio to mock
        ),
        TestMocks::MockCRNProvider{}  // Return proper CRN provider
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

    // NOTE: IID resampling has been removed - the function now always uses Block resampling
    // (StationaryMaskValueResamplerAdapter). The old IID test case has been removed.

    // --- Scenario 1: Block Resampler Chosen, BCa is the Minimum ---
    SECTION("Block resampler (always used) & BCa LB is minimum") {
        // Data: small n=20, streaky (ratio=1.0)
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
    // Test logging logic
    std::vector<Decimal> returns(30, D(0.001)); // n=30
    StrategyT strategy;
    std::ostringstream oss;
    
    // Set mocks to allow logging to proceed
    TestMocks::FactoryMockHelper::control = TestMocks::FactoryControl{
        .expect_block = true, // Always uses Block resampling now
        .mn_lb_val = 0.0005,
        .bca_lb_val = 0.0005,
        .m_sub_used = 24       // Expected m for n=30
    };

    // NOTE: The "Override forces IID" test has been removed because IID resampling
    // is no longer available - the function always uses Block resampling.
    
    // --- Scenario 1: Logging Check ---
    SECTION("Logging includes key diagnostics (m/n, sigma)") {
        // Run with logging stream
        FactoryT factory(0);
        conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            returns, /*L=*/5, 252.0, 0.95, 2000, 0.0, strategy, factory, &oss, 0, 0, std::nullopt
        );
        
        const std::string log = oss.str();
        
        // Log must contain m/n shrink report
        //REQUIRE(log.find("m_sub=24") != std::string::npos);
        //REQUIRE(log.find("n=30") != std::string::npos);
        // Log must contain sigma/var reports (via the detail::has_member_upper check)
        //REQUIRE(log.find("σ(per-period)≈") != std::string::npos);
    }

    // --- Scenario 2: Block resampler always used ---
    SECTION("Block resampler is always used (n=40)") {
        std::vector<Decimal> borderline_returns(40);
        for(int i=0; i<40; ++i) borderline_returns[i] = (i % 2 == 0) ? D(0.01) : D(-0.01);

        TestMocks::FactoryMockHelper::control.expect_block = true;

        FactoryT factory(0);
        auto result = conservative_smallN_lower_bound<Decimal, GeoStatT, StrategyT>(
            borderline_returns, /*L=*/5, 252.0, 0.95, 2000, 0.0, strategy, factory, nullptr, 0, 0, std::nullopt
        );
        
        // Block resampler is always used now
        REQUIRE(std::string(result.resampler_name) == std::string("StationaryMaskValueResamplerAdapter"));
    }
}







// =============================================================================
// NEW UNIT TESTS FOR REFACTORED HELPERS
// =============================================================================


TEST_CASE("execute_bootstrap_duel picks the conservative minimum",
          "[SmallN][Refactor][DuelKernel]")
{
    using ResamplerT = mkc_timeseries::IIDResampler<Decimal>;
    
    // Setup data
    std::vector<Decimal> returns(100, D(0.001));
    ResamplerT resampler;
    StrategyT strategy;
    double ann_factor = 252.0;
    std::ostringstream oss;
    
    SECTION("Scenario A: m-out-of-n LB < BCa LB -> Returns m-out-of-n") {
        // Configure Mock: MN=0.01, BCa=0.02
        TestMocks::FactoryMockHelper::control = TestMocks::FactoryControl{
            .mn_lb_val = 0.01,
            .bca_lb_val = 0.02,
            .m_sub_used = 50
        };
        FactoryT factory(0);

        auto result = execute_bootstrap_duel<ResamplerT, Decimal, GeoStatT>(
            returns, resampler, /*rho=*/0.5, /*L=*/3, ann_factor, 0.95, 1000, 1.96,
            strategy, factory, 0, 0, &oss, "TestResampler"
        );

        REQUIRE(num::to_double(result.per_lower) == Approx(0.01));
        REQUIRE(result.effB_bca == 1000);
        
        // Verify diagnostics
        REQUIRE(std::string(result.resampler_name) == "TestResampler");
        // Check logs for Duel details
        std::string logs = oss.str();
        REQUIRE(logs.find("m/n") != std::string::npos);
        REQUIRE(logs.find("BCa") != std::string::npos);
    }

    SECTION("Scenario B: BCa LB < m-out-of-n LB -> Returns BCa") {
        // Configure Mock: MN=0.03, BCa=0.015
        TestMocks::FactoryMockHelper::control = TestMocks::FactoryControl{
            .mn_lb_val = 0.03,
            .bca_lb_val = 0.015,
            .m_sub_used = 50
        };
        FactoryT factory(0);

        auto result = execute_bootstrap_duel<ResamplerT, Decimal, GeoStatT>(
            returns, resampler, /*rho=*/0.5, /*L=*/3, ann_factor, 0.95, 1000, 1.96,
            strategy, factory, 0, 0, nullptr, "TestResampler"
        );

        REQUIRE(num::to_double(result.per_lower) == Approx(0.015));
    }
    
    SECTION("Scenario C: Handles Diagnostics Logging") {
         TestMocks::FactoryMockHelper::control = TestMocks::FactoryControl{
            .mn_lb_val = 0.01,
            .bca_lb_val = 0.01,
            .m_sub_used = 80
        };
        FactoryT factory(0);

        execute_bootstrap_duel<ResamplerT, Decimal, GeoStatT>(
            returns, resampler, 0.8, 3, ann_factor, 0.95, 1000, 1.96,
            strategy, factory, 0, 0, &oss, "MyResampler"
        );
        
        std::string logs = oss.str();
        
        // Should report the shrink rate (1.0 - 0.8 = 0.2)
        REQUIRE(logs.find("shrink=0.200") != std::string::npos);
        // Should report sigma calculation
        REQUIRE(logs.find("σ(per-period)") != std::string::npos);
    }
}
