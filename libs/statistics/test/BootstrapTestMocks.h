// BootstrapTestMocks.h
//
// Shared mock types for Percentile-T bootstrap unit tests.
//
// Previously each test file defined its own local MockPercentileTResult /
// MockPercentileTEngine struct, which caused repeated compile failures every
// time PercentileTBootstrap::Result gained new fields.  Centralising the
// definition here means only this file needs to change.
//
// USAGE
// -----
// In each test file that uses a Percentile-T mock, add:
//
//   #include "BootstrapTestMocks.h"
//   using MockPercentileTResult = ::MockPercentileTResult<Decimal>;
//   using MockPercentileTEngine = ::MockPercentileTEngine<Decimal>;
//
// where Decimal is whatever numeric type the test file is parameterised on
// (e.g. double, num::DefaultNumber).  The default template argument is double
// so files that only use MockPercentileTResult<> without specifying a type
// also compile cleanly.
//
// DESIGN NOTES
// ------------
// MockPercentileTResult
//   A plain aggregate struct whose fields match PercentileTBootstrap::Result
//   exactly.  All fields have default member initialisers so the struct can be
//   used in two ways:
//
//     1. Aggregate positional initialisation (AutoBootstrapSelectorAdditionalTests):
//          MockPercentileTResult<double> res{1.0, 0.9, 1.1, 0.95, 100,
//                                           1000, 200, 1000, 0, 0, 200000, 0.05};
//        Fields beyond se_hat (skew_pivot, reliability flags) receive their
//        defaults.  Works in C++17 because the struct has no user-provided
//        constructors.
//
//     2. Field-by-field assignment (BootstrapPenaltyCalculatorTest):
//          MockPercentileTResult<Num> res;   // all fields at safe defaults
//          res.skipped_outer = 150;          // override specific field
//
// MockPercentileTEngine
//   Wraps MockPercentileTResult and exposes the interface that
//   AutoBootstrapSelector::summarizePercentileT() requires.  Data members are
//   public to support direct-assignment tests (AutoBootstrapSelectorMedianTest)
//   while setters are also provided for fluent-API tests
//   (AutoBootstrapSelectorSummarizeTests).

#pragma once

#include <cstddef>
#include <vector>

// ============================================================================
// MockPercentileTResult
// ============================================================================

namespace test_mocks {

template <class Decimal = double>
struct MockPercentileTResult
{
    // Core result fields — match PercentileTBootstrap::Result in declaration
    // order so that positional aggregate initialisation works unchanged.
    Decimal     mean{};
    Decimal     lower{};
    Decimal     upper{};
    double      cl                        = 0.95;
    std::size_t n                         = 100;
    std::size_t B_outer                   = 1000;
    std::size_t B_inner                   = 200;
    std::size_t effective_B               = 1000;
    std::size_t skipped_outer             = 0;
    std::size_t skipped_inner_total       = 0;
    std::size_t inner_attempted_total     = 100000;
    double      se_hat                    = 0.05;

    // Fields added by the isReliable() work (PercentileTBootstrap.h).
    // Defaulted so all existing positional / field-assignment usages that
    // do not specify these fields compile and behave correctly:
    //
    //   skew_pivot = 0.0
    //     Below both the soft threshold (2.0) and the hard threshold (3.0),
    //     so computePercentileTStability() adds no pivot-skewness penalty and
    //     extreme_pivot_skewness does not fire.
    //
    //   reliability flags = false
    //     isReliable() returns true, matching the intended "healthy mock"
    //     state of all existing test instances.
    double      skew_pivot                = 0.0;
    bool        low_effective_replicates  = false;
    bool        high_inner_skip_rate      = false;
    bool        extreme_pivot_skewness    = false;

    bool isReliable() const noexcept
    {
        return !low_effective_replicates
            && !high_inner_skip_rate
            && !extreme_pivot_skewness;
    }
};

// ============================================================================
// MockPercentileTEngine
// ============================================================================

template <class Decimal = double>
class MockPercentileTEngine
{
public:
    // The Result type is the shared mock result so that test code can use
    // MockPercentileTEngine<D>::Result and MockPercentileTResult<D>
    // interchangeably.
    using Result = MockPercentileTResult<Decimal>;

    // -----------------------------------------------------------------------
    // Public data members — support direct-assignment test style:
    //   engine.theta_star_stats = { ... };
    //   engine.diagnosticsReady = true;
    // -----------------------------------------------------------------------
    bool                diagnosticsReady = false;
    std::vector<double> theta_star_stats;
    std::vector<double> t_stats;

    // -----------------------------------------------------------------------
    // Setters — support fluent test style:
    //   engine.setThetaStarStatistics(stats);
    //   engine.setResult(res);
    // Setting theta* stats via the setter also marks diagnostics as ready,
    // matching the behaviour expected by the "diagnostics not available" tests.
    // -----------------------------------------------------------------------
    void setThetaStarStatistics(const std::vector<double>& stats)
    {
        theta_star_stats = stats;
        diagnosticsReady = true;
    }

    void setTStatistics(const std::vector<double>& stats) { t_stats = stats; }

    // setResult stores the result internally; the result is typically also
    // passed directly to summarizePercentileT(), but some tests call setResult
    // for symmetry / future use.
    void setResult(const Result& res) { m_result = res; }

    // -----------------------------------------------------------------------
    // Interface required by AutoBootstrapSelector::summarizePercentileT()
    // -----------------------------------------------------------------------
    bool hasDiagnostics() const { return diagnosticsReady; }

    const std::vector<double>& getThetaStarStatistics() const
    {
        return theta_star_stats;
    }

    const std::vector<double>& getTStatistics() const { return t_stats; }

    // Convenience accessor (not called by the selector, but available for
    // tests that want to inspect the stored result).
    const Result& getResult() const { return m_result; }

private:
    Result m_result;
};

} // namespace test_mocks
