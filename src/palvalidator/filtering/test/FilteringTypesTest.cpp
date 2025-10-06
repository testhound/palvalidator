#include "catch2/catch_test_macros.hpp"
#include "filtering/FilteringTypes.h"
#include <boost/date_time/gregorian/gregorian.hpp>

using namespace palvalidator::filtering;

TEST_CASE("StrategyAnalysisContext: basic construction", "[FilteringTypes]")
{
  auto strat = std::shared_ptr<PalStrategy<Num>>(nullptr);
  auto sec = std::shared_ptr<mkc_timeseries::Security<Num>>(nullptr);

  // DateRange has no default constructor; construct a simple valid range for tests.
  using boost::gregorian::date;
  date d1(2020, 1, 1), d2(2020, 12, 31);
  mkc_timeseries::DateRange in(d1, d2), oos(d1, d2);

  StrategyAnalysisContext ctx(strat, sec, in, oos, mkc_timeseries::TimeFrame::DAILY);

  REQUIRE(ctx.strategy == strat);
  REQUIRE(ctx.baseSecurity == sec);
  REQUIRE(ctx.highResReturns.empty());
  REQUIRE(ctx.blockLength == 0);
}

TEST_CASE("BootstrapAnalysisResult: validity check", "[FilteringTypes]")
{
  BootstrapAnalysisResult r;
  r.annualizedLowerBoundGeo = Num(0);
  r.annualizedLowerBoundMean = Num(0);
  REQUIRE_FALSE(r.isValid());

  r.annualizedLowerBoundGeo = Num("0.01");
  r.computationSucceeded = true;
  REQUIRE(r.isValid());
}

TEST_CASE("HurdleAnalysisResult: passed flags", "[FilteringTypes]")
{
  HurdleAnalysisResult h;
  h.passedBase = true;
  h.passed1Qn = false;
  REQUIRE_FALSE(h.passed());
  h.passed1Qn = true;
  REQUIRE(h.passed());
}

TEST_CASE("FilterDecision: helper constructors", "[FilteringTypes]")
{
  auto p = FilterDecision::Pass("ok");
  REQUIRE(p.passed());
  REQUIRE(p.rationale == "ok");

  auto f = FilterDecision::Fail(FilterDecisionType::FailHurdle, "hurdle fail");
  REQUIRE_FALSE(f.passed());
  REQUIRE(f.decision == FilterDecisionType::FailHurdle);
  REQUIRE(f.rationale == "hurdle fail");
}