// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

/**
 * @file RobustnessTest.h
 * @brief Robustness testing framework for PriceActionLab trading patterns.
 *
 * Provides classes to evaluate whether a discovered trading pattern is
 * statistically robust by permuting profit-target and stop-loss parameters
 * around the reference values and verifying that profitability metrics remain
 * within tolerance. Tolerance functions use precomputed ln(n) and sqrt(n)
 * lookup tables so that permutations farther from the reference are allowed
 * progressively wider bands.
 *
 * Key classes:
 * - PatternRobustnessCriteria  -- threshold parameters for the robustness test.
 * - RobustnessPermutationAttributes -- abstract specification of permutation counts.
 * - RobustnessCalculator       -- aggregates permutation results and decides robustness.
 * - RobustnessTest             -- orchestrates backtesting across permutations.
 * - RobustnessTestMonteCarlo   -- variant that augments each permutation with a
 *                                 Monte Carlo payoff-ratio estimate.
 *
 * @author Michael K. Collison
 */

#ifndef __ROBUSTNESS_TEST_H
#define __ROBUSTNESS_TEST_H 1

#include <exception>
#include <string>
#include <map>
#include <boost/date_time.hpp>
#include "number.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PalAst.h"
#include "MonteCarloPermutationTest.h"
#include "Returns.h"
#include "SummaryStats.h"

namespace mkc_timeseries
{
  using boost::gregorian::date;
  using std::make_shared;

  /**
   * @brief Encapsulates the criteria used to evaluate pattern robustness.
   *
   * Holds minimum robustness index, desired profit factor, tolerance settings,
   * and a profitability safety factor that adjusts the PAL profitability formula
   * to account for commissions and slippage in short-term trading.
   *
   * Tolerance lookup methods use precomputed ln(n) and sqrt(n) tables indexed
   * by distance (in iterations or percent) from the reference permutation,
   * clamped to the range [1%, 10%].
   *
   * @tparam Decimal  The numeric type used for fixed-point arithmetic
   *                  (e.g., dec::decimal<7>).
   */
  template <class Decimal> class PatternRobustnessCriteria
    {
    public:
      /**
       * @brief Constructs robustness criteria from the given thresholds.
       *
       * @param minRobustnessIndex       Minimum acceptable robustness index (0-100).
       * @param desiredProfitFactor       Target profit factor for the pattern.
       * @param tolerance                 Base tolerance expressed as a percentage.
       * @param profitabilitySafetyFactor Safety factor (typically 0.7-0.9) applied to
       *                                  the payoff ratio when computing profitability.
       */
      PatternRobustnessCriteria (const Decimal& minRobustnessIndex,
				 const Decimal& desiredProfitFactor,
				 const PercentNumber<Decimal>& tolerance,
				 const Decimal& profitabilitySafetyFactor)
	: mMinRobustnessIndex (minRobustnessIndex),
	  mDesiredProfitFactor(desiredProfitFactor),
	  mRobustnessTolerance(tolerance),
	  mProfitabilitySafetyFactor(profitabilitySafetyFactor)
	{}

      /**
       * @brief Copy constructor.
       */
      PatternRobustnessCriteria (const PatternRobustnessCriteria<Decimal>& rhs)
	: mMinRobustnessIndex (rhs.mMinRobustnessIndex),
	  mDesiredProfitFactor(rhs.mDesiredProfitFactor),
	  mRobustnessTolerance(rhs.mRobustnessTolerance),
	  mProfitabilitySafetyFactor(rhs.mProfitabilitySafetyFactor)
      {}

      /**
       * @brief Copy assignment operator.
       */
      PatternRobustnessCriteria<Decimal>&
      operator=(const PatternRobustnessCriteria<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;

	mMinRobustnessIndex = rhs.mMinRobustnessIndex;
	mDesiredProfitFactor = rhs.mDesiredProfitFactor;
	mRobustnessTolerance = rhs.mRobustnessTolerance;
	mProfitabilitySafetyFactor = rhs.mProfitabilitySafetyFactor;

	return *this;
      }

      /// Destructor.
      ~PatternRobustnessCriteria()
      {}

      /// Returns the minimum robustness index threshold (0-100).
      const Decimal& getMinimumRobustnessIndex() const
      {
	return mMinRobustnessIndex;
      }

      /// Returns the desired (target) profit factor.
      const Decimal& getDesiredProfitFactor() const
      {
	return mDesiredProfitFactor;
      }

      /// Returns the base robustness tolerance as a percentage.
      const PercentNumber<Decimal>& getRobustnessTolerance() const
      {
	return mRobustnessTolerance;
      }

      /**
       * @brief Returns a ln-scaled tolerance as a raw Decimal for a given percent distance.
       *
       * Uses a precomputed table of ln(n) values for n in [0, 100]. Entries 0
       * and 1 are manually clamped to 1.0 so the tolerance never falls below 1%.
       * Values above index 100 are clamped to ln(100).
       *
       * @param percentDiffAsInteger  The integer percent difference from the reference
       *                              stop-loss value (0-100).
       * @return The ln-scaled tolerance as a Decimal.
       */
      const Decimal getDecimalToleranceForPercentDifference (unsigned long percentDiffAsInteger) const
      {
	// Note entries 0 and  was manually modified because we do not want a tolerance
	// less than 1%


	static Decimal lnConstants[] =
	  {
	    DecimalConstants<Decimal>::createDecimal (std::string("1.00000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("1.00000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("1.09861")),
	    DecimalConstants<Decimal>::createDecimal (std::string("1.38629")),
	    DecimalConstants<Decimal>::createDecimal (std::string("1.60944")),
	    DecimalConstants<Decimal>::createDecimal (std::string("1.79176")),
	    DecimalConstants<Decimal>::createDecimal (std::string("1.94591")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.07944")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.19722")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.30259")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.3979")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.48491")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.56495")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.63906")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.70805")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.77259")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.83321")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.89037")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.94444")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.99573")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.04452")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.09104")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.13549")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.17805")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.21888")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.2581")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.29584")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.3322")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.3673")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.4012")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.43399")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.46574")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.49651")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.52636")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.55535")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.58352")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.61092")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.63759")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.66356")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.68888")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.71357")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.73767")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.7612")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.78419")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.80666")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.82864")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.85015")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.8712")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.89182")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.91202")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.93183")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.95124")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.97029")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.98898")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.00733")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.02535")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.04305")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.06044")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.07754")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.09434")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.11087")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.12713")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.14313")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.15888")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.17439")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.18965")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.20469")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.21951")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.23411")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.2485")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.26268")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.27667")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.29046")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.30407")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.31749")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.33073")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.34381")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.35671")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.36945")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.38203")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.39445")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.40672")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.41884")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.43082")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.44265")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.45435")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.46591")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.47734")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.48864")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.49981")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.51086")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.52179")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.5326")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.54329")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.55388")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.56435")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.57471")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.58497")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.59512")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.60517"))
	  };

	if ((percentDiffAsInteger >=0) && (percentDiffAsInteger <= 100))
	  return lnConstants[percentDiffAsInteger];

	// We don't want tolerance greater than 10%
	return lnConstants[100];

      }

      /**
       * @brief Returns a ln-scaled tolerance as a PercentNumber for a given percent distance.
       *
       * Identical mapping to getDecimalToleranceForPercentDifference but returns a
       * PercentNumber wrapper. Entries 0 and 1 are clamped to 1%.
       *
       * @param percentDiffAsInteger  The integer percent difference from the reference
       *                              stop-loss value (0-100).
       * @return The ln-scaled tolerance as a PercentNumber.
       */
      const PercentNumber<Decimal> getToleranceForPercentDifference (unsigned long percentDiffAsInteger) const
      {
	// Note entries 0 and  was manually modified because we do not want a tolerance
	// less than 1%


	static PercentNumber<Decimal> lnConstants[] =
	  {
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.00000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.00000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.09861")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.38629")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.60944")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.79176")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.94591")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.07944")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.19722")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.30259")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.3979")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.48491")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.56495")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.63906")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.70805")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.77259")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.83321")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.89037")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.94444")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.99573")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.04452")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.09104")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.13549")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.17805")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.21888")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.2581")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.29584")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.3322")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.3673")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.4012")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.43399")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.46574")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.49651")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.52636")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.55535")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.58352")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.61092")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.63759")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.66356")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.68888")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.71357")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.73767")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.7612")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.78419")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.80666")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.82864")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.85015")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.8712")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.89182")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.91202")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.93183")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.95124")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.97029")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.98898")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.00733")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.02535")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.04305")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.06044")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.07754")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.09434")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.11087")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.12713")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.14313")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.15888")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.17439")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.18965")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.20469")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.21951")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.23411")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.2485")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.26268")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.27667")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.29046")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.30407")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.31749")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.33073")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.34381")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.35671")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.36945")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.38203")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.39445")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.40672")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.41884")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.43082")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.44265")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.45435")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.46591")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.47734")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.48864")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.49981")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.51086")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.52179")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.5326")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.54329")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.55388")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.56435")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.57471")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.58497")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.59512")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.60517"))
	  };

	if ((percentDiffAsInteger >=0) && (percentDiffAsInteger <= 100))
	  return lnConstants[percentDiffAsInteger];

	// We don't want tolerance greater than 10%
	return lnConstants[100];

      }

      /**
       * @brief Returns a sqrt-scaled tolerance as a raw Decimal for iteration distance.
       *
       * Uses a precomputed table of sqrt(n) values for n in [0, 100]. Entry 0
       * is clamped to 1% and entries 1-3 are clamped to 2% to prevent overly
       * tight tolerances near the reference. Values above index 100 are clamped
       * to sqrt(100) = 10%.
       *
       * @param numIterations  Number of permutation steps away from the reference (0-100).
       * @return The sqrt-scaled tolerance as a Decimal.
       */
      const Decimal getDecimalToleranceForIterations (unsigned long numIterations) const
      {
	static Decimal sqrtConstants[] =
	  {
	    // Note entry 0 was manually modified because we would like a 1%
	    // tolerance on the reference value

	    DecimalConstants<Decimal>::createDecimal (std::string("1.000000")),
	    // Note entries 1 to 3 were manually modified because we don't want
	    // tolerances less than 2% for these entries

	    DecimalConstants<Decimal>::createDecimal (std::string("2.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.236068")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.449490")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.645751")),
	    DecimalConstants<Decimal>::createDecimal (std::string("2.828427")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.162278")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.316625")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.464102")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.605551")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.741657")),
	    DecimalConstants<Decimal>::createDecimal (std::string("3.872983")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.123106")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.242641")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.358899")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.472136")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.582576")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.690416")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.795832")),
	    DecimalConstants<Decimal>::createDecimal (std::string("4.898979")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.099020")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.196152")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.291503")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.385165")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.477226")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.567764")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.656854")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.744563")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.830952")),
	    DecimalConstants<Decimal>::createDecimal (std::string("5.916080")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.082763")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.164414")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.244998")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.324555")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.403124")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.480741")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.557439")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.633250")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.708204")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.782330")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.855655")),
	    DecimalConstants<Decimal>::createDecimal (std::string("6.928203")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.071068")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.141428")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.211103")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.280110")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.348469")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.416198")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.483315")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.549834")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.615773")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.681146")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.745967")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.810250")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.874008")),
	    DecimalConstants<Decimal>::createDecimal (std::string("7.937254")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.062258")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.124038")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.185353")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.246211")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.306624")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.366600")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.426150")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.485281")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.544004")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.602325")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.660254")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.717798")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.774964")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.831761")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.888194")),
	    DecimalConstants<Decimal>::createDecimal (std::string("8.944272")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.000000")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.055385")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.110434")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.165151")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.219544")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.273618")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.327379")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.380832")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.433981")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.486833")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.539392")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.591663")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.643651")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.695360")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.746794")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.797959")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.848858")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.899495")),
	    DecimalConstants<Decimal>::createDecimal (std::string("9.949874")),
	    DecimalConstants<Decimal>::createDecimal (std::string("10.000000")),
	  };

	if ((numIterations >=0) && (numIterations <= 100))
	  return sqrtConstants[numIterations];

	// We don't want tolerance greater than 10%
	return sqrtConstants[100];
      }

      /**
       * @brief Returns a sqrt-scaled tolerance as a PercentNumber for iteration distance.
       *
       * Identical mapping to getDecimalToleranceForIterations but returns a
       * PercentNumber wrapper. Entry 0 is clamped to 1% and entries 1-3 to 2%.
       *
       * @param numIterations  Number of permutation steps away from the reference (0-100).
       * @return The sqrt-scaled tolerance as a PercentNumber.
       */
      const PercentNumber<Decimal> getToleranceForIterations (unsigned long numIterations) const
      {
	static PercentNumber<Decimal> sqrtConstants[] =
	  {
	    // Note entry 0 was manually modified because we would like a 1%
	    // tolerance on the reference value

	    PercentNumber<Decimal>::createPercentNumber (std::string("1.000000")),
	    // Note entries 1 to 3 were manually modified because we don't want
	    // tolerances less than 2% for these entries

	    PercentNumber<Decimal>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.236068")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.449490")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.645751")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.828427")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.162278")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.316625")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.464102")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.605551")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.741657")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.872983")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.123106")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.242641")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.358899")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.472136")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.582576")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.690416")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.795832")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.898979")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.099020")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.196152")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.291503")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.385165")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.477226")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.567764")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.656854")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.744563")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.830952")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.916080")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.082763")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.164414")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.244998")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.324555")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.403124")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.480741")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.557439")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.633250")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.708204")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.782330")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.855655")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("6.928203")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.071068")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.141428")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.211103")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.280110")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.348469")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.416198")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.483315")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.549834")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.615773")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.681146")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.745967")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.810250")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.874008")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("7.937254")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.062258")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.124038")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.185353")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.246211")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.306624")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.366600")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.426150")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.485281")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.544004")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.602325")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.660254")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.717798")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.774964")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.831761")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.888194")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("8.944272")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.055385")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.110434")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.165151")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.219544")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.273618")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.327379")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.380832")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.433981")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.486833")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.539392")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.591663")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.643651")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.695360")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.746794")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.797959")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.848858")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.899495")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("9.949874")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("10.000000")),
	  };

	if ((numIterations >=0) && (numIterations <= 100))
	  return sqrtConstants[numIterations];

	// We don't want tolerance greater than 10%
	return sqrtConstants[100];
      }

      /**
       * @brief Returns a half-sqrt-scaled tolerance as a PercentNumber for trade count.
       *
       * Uses a precomputed table of sqrt(n)/2 values for n in [0, 100].
       * Provides a gentler tolerance growth than getToleranceForIterations,
       * appropriate for adjusting expectations based on sample size (number of
       * trades). Values above index 100 are clamped to 5%.
       *
       * @param numTrades  The number of closed trades in the permutation result (0-100).
       * @return The half-sqrt-scaled tolerance as a PercentNumber.
       */
      const PercentNumber<Decimal> getToleranceForNumTrades (unsigned long numTrades) const
      {
	static PercentNumber<Decimal> halfSqrtConstants[] =
	  {
	    PercentNumber<Decimal>::createPercentNumber (std::string("0.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("0.500000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("0.707107")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("0.866025")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.118034")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.224745")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.322876")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.414214")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.500000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.581139")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.658312")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.732051")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.802776")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.870829")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("1.936492")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.061553")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.121320")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.179449")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.236068")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.291288")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.345208")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.397916")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.449490")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.500000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.549510")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.598076")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.645751")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.692582")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.738613")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.783882")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.828427")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.872281")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.915476")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("2.958040")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.041381")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.082207")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.122499")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.162278")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.201562")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.240370")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.278719")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.316625")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.354102")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.391165")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.427827")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.464102")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.500000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.535534")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.570714")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.605551")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.640055")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.674235")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.708099")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.741657")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.774917")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.807887")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.840573")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.872983")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.905125")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.937004")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("3.968627")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.000000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.031129")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.062019")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.092676")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.123106")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.153312")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.183300")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.213075")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.242641")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.272002")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.301163")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.330127")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.358899")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.387482")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.415880")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.444097")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.472136")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.500000")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.527693")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.555217")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.582576")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.609772")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.636809")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.663690")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.690416")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.716991")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.743416")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.769696")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.795832")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.821825")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.847680")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.873397")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.898979")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.924429")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.949747")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("4.974937")),
	    PercentNumber<Decimal>::createPercentNumber (std::string("5.000000"))
	  };

	if ((numTrades >=0) && (numTrades <= 100))
	  return halfSqrtConstants[numTrades];

	// We don't want tolerance greater than 5%
	return halfSqrtConstants[100];
      }

      /**
       * @brief Returns the profitability safety factor.
       *
       * PAL Profitability is defined as:
       * Profitability = ProfitFactor / (ProfitFactor + PayoffRatio).
       *
       * This is modified to account for commissions and slippage by introducing
       * a safety factor (typically 0.7-0.9 for short-term trading):
       * Profitability = ProfitFactor / (ProfitFactor + SafetyFactor * PayoffRatio).
       *
       * @return Reference to the safety factor Decimal.
       */
      const Decimal& getProfitabilitySafetyFactor() const
      {
	return mProfitabilitySafetyFactor;
      }

    private:
      Decimal mMinRobustnessIndex;
      Decimal mDesiredProfitFactor;
      PercentNumber<Decimal> mRobustnessTolerance;
      Decimal mProfitabilitySafetyFactor;
    };

  /**
   * @brief Abstract base class specifying permutation layout for robustness testing.
   *
   * Defines how many total permutations to generate, how many fall below and
   * above the reference stop-loss, and provides a divisor used to compute the
   * permutation increment. Derived classes supply the number of boundary
   * entries to inspect for early failure detection.
   */
  class RobustnessPermutationAttributes
  {
  public:
    /**
     * @brief Copy constructor.
     */
    RobustnessPermutationAttributes (const RobustnessPermutationAttributes& rhs)
      : mNumberOfPermutations(rhs.mNumberOfPermutations),
	mBelowRefPermutations(rhs.mBelowRefPermutations),
	mAboveRefPermutations(rhs.mAboveRefPermutations),
	mPermutationsDivisor(rhs.mPermutationsDivisor)
    {}

    /**
     * @brief Copy assignment operator.
     */
    RobustnessPermutationAttributes&
    operator=(const RobustnessPermutationAttributes &rhs)
    {
      if (this == &rhs)
	return *this;

      mNumberOfPermutations = rhs.mNumberOfPermutations;
      mBelowRefPermutations = rhs.mBelowRefPermutations;
      mAboveRefPermutations = rhs.mAboveRefPermutations;
      mPermutationsDivisor = rhs.mPermutationsDivisor;

      return *this;
    }

    /// Virtual destructor (pure).
    virtual ~RobustnessPermutationAttributes() = 0;

    /**
     * @brief Returns the number of leading permutations to check for failure.
     */
    virtual uint32_t numEntriesToTestAtBeginning() const = 0;

    /**
     * @brief Returns the number of trailing permutations to check for failure.
     */
    virtual uint32_t numEntriesToTestAtEnd() const = 0;

    /// Returns the total number of permutations in the test.
    uint32_t getNumberOfPermutations() const
    {
      return mNumberOfPermutations;
    }

    /// Returns the number of permutations below the reference stop.
    uint32_t getNumPermutationsBelowRef() const
    {
      return mBelowRefPermutations;
    }

    /// Returns the number of permutations above the reference stop.
    uint32_t getNumPermutationsAboveRef() const
    {
      return mAboveRefPermutations;
    }

    /// Returns the divisor used to compute the permutation increment from the original stop.
    uint32_t getPermutationsDivisor() const
    {
      return mPermutationsDivisor;
    }

  protected:
    /**
     * @brief Protected constructor for derived classes.
     *
     * @param numberOfPermutations  Total number of stop/target permutations.
     * @param belowRefPermutations  Count of permutations below the reference.
     * @param aboveRefPermutations  Count of permutations above the reference.
     * @param permutationsDivisor   Divisor to compute the permutation step size.
     */
    RobustnessPermutationAttributes (uint32_t numberOfPermutations,
				     uint32_t belowRefPermutations,
				     uint32_t aboveRefPermutations,
				     uint32_t permutationsDivisor)
      : mNumberOfPermutations(numberOfPermutations),
	mBelowRefPermutations(belowRefPermutations),
	mAboveRefPermutations(aboveRefPermutations),
	mPermutationsDivisor(permutationsDivisor)
    {}

  private:
    uint32_t mNumberOfPermutations;
    uint32_t mBelowRefPermutations;
    uint32_t mAboveRefPermutations;
    uint32_t mPermutationsDivisor;
  };


  inline RobustnessPermutationAttributes::~RobustnessPermutationAttributes()
  {}

  /**
   * @brief Permutation attributes mimicking PriceActionLab's robustness settings.
   *
   * Uses 19 total permutations (14 below, 4 above the reference) with a
   * divisor of 16 and tests 2 entries at each boundary for early failure.
   */
  class PALRobustnessPermutationAttributes
    : public RobustnessPermutationAttributes
  {
  public:
    /// Constructs with PriceActionLab default permutation settings.
    PALRobustnessPermutationAttributes()
      : RobustnessPermutationAttributes(19, 14, 4, 16)
    {}

    PALRobustnessPermutationAttributes (const RobustnessPermutationAttributes& rhs)
      : RobustnessPermutationAttributes(rhs)
    {}

    PALRobustnessPermutationAttributes&
    operator=(const PALRobustnessPermutationAttributes& rhs)
    {
      if (this == &rhs)
	return *this;

      RobustnessPermutationAttributes::operator=(rhs);
      return *this;
    }

    ~PALRobustnessPermutationAttributes()
    {}

    /**
     * @brief Returns 2, the number of leading permutations checked for failure.
     */
    uint32_t numEntriesToTestAtBeginning() const
    {
      return 2;
    }

    /**
     * @brief Returns 2, the number of trailing permutations checked for failure.
     */
    uint32_t numEntriesToTestAtEnd() const
    {
      return 2;
    }
  };

  /////////////////////////

  /**
   * @brief Permutation attributes for statistically significant robustness testing.
   *
   * Uses 30 total permutations (15 below, 14 above the reference) with a
   * divisor of 30 and tests 3 entries at each boundary for early failure.
   */
  class StatSignificantAttributes
    : public RobustnessPermutationAttributes
  {
  public:
    /// Constructs with statistically significant permutation settings.
    StatSignificantAttributes()
      : RobustnessPermutationAttributes(30, 15, 14, 30)
    {}

    StatSignificantAttributes (const RobustnessPermutationAttributes& rhs)
      : RobustnessPermutationAttributes(rhs)
    {}

    StatSignificantAttributes&
    operator=(const StatSignificantAttributes &rhs)
    {
      if (this == &rhs)
	return *this;

      RobustnessPermutationAttributes::operator=(rhs);
      return *this;
    }

    ~StatSignificantAttributes()
    {}

    /**
     * @brief Returns 3, the number of leading permutations checked for failure.
     */
   uint32_t numEntriesToTestAtBeginning() const
    {
      return 3;
    }

    /**
     * @brief Returns 3, the number of trailing permutations checked for failure.
     */
    uint32_t numEntriesToTestAtEnd() const
    {
      return 3;
    }
  };

  /**
   * @brief Holds a profit-target and protective-stop pair for a pattern permutation.
   *
   * @tparam Decimal  The numeric type (e.g., dec::decimal<7>).
   */
  template <class Decimal> class ProfitTargetStopPair
  {
  public:
    /**
     * @brief Constructs a pair from the given profit target and stop values.
     *
     * @param profitTarget  The profit target as a percentage.
     * @param stop          The protective stop loss as a percentage.
     */
    ProfitTargetStopPair (const Decimal& profitTarget,
			  const Decimal& stop)
      :  mProfitTarget(profitTarget),
	 mStop(stop)
    {}

    ProfitTargetStopPair(const ProfitTargetStopPair<Decimal>& rhs)
      : mProfitTarget(rhs.mProfitTarget),
	mStop(rhs.mStop)
    {}

    ProfitTargetStopPair<Decimal>&
    operator=(const ProfitTargetStopPair<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mProfitTarget = rhs.mProfitTarget;
      mStop = rhs.mStop;

      return *this;
    }

    ~ProfitTargetStopPair()
    {}

    /// Returns the profit target value.
    const Decimal& getProfitTarget() const
    {
      return mProfitTarget;
    }

    /// Returns the protective stop loss value.
    const Decimal& getProtectiveStop() const
    {
      return mStop;
    }

  private:
    Decimal mProfitTarget;
    Decimal mStop;
  };

  /**
   * @brief Stores the backtest metrics for a single robustness permutation.
   *
   * Captures win rate (PAL profitability), profit factor, number of trades,
   * payoff ratio, median payoff ratio, R-multiple expectancy, and optionally
   * a Monte Carlo estimated payoff ratio.
   *
   * @tparam Decimal  The numeric type (e.g., dec::decimal<7>).
   */
  template <class Decimal> class RobustnessTestResult
  {
  public:
    /**
     * @brief Constructs a result without a Monte Carlo payoff estimate.
     *
     * @param winRate           PAL profitability (win rate as a percentage).
     * @param profitFactor      Ratio of gross profit to gross loss.
     * @param numTrades         Number of closed trades in the backtest.
     * @param payOffRatio       Mean payoff ratio (average win / average loss).
     * @param medianPayOffRatio Median payoff ratio.
     * @param expectation       R-multiple expectancy.
     */
    RobustnessTestResult (const Decimal& winRate,
			  const Decimal& profitFactor,
			  unsigned long numTrades,
			  const Decimal& payOffRatio,
			  const Decimal& medianPayOffRatio,
			  const Decimal& expectation)
      : mWinRate(winRate),
	mProfitFactor(profitFactor),
	mNumTrades(numTrades),
	mPayOffRatio(payOffRatio),
	mMedianPayOffRatio(medianPayOffRatio),
	mExpectation(expectation),
	mMonteCarloPayoffRatio(DecimalConstants<Decimal>::DecimalZero)
    {}

    /**
     * @brief Constructs a result with a Monte Carlo payoff estimate.
     *
     * @param winRate           PAL profitability (win rate as a percentage).
     * @param profitFactor      Ratio of gross profit to gross loss.
     * @param numTrades         Number of closed trades in the backtest.
     * @param payOffRatio       Mean payoff ratio (average win / average loss).
     * @param medianPayOffRatio Median payoff ratio.
     * @param expectation       R-multiple expectancy.
     * @param monteCarloPayoff  Monte Carlo estimated payoff ratio.
     */
    RobustnessTestResult (const Decimal& winRate,
			  const Decimal& profitFactor,
			  unsigned long numTrades,
			  const Decimal& payOffRatio,
			  const Decimal& medianPayOffRatio,
			  const Decimal& expectation,
			  const Decimal& monteCarloPayoff)
      : mWinRate(winRate),
	mProfitFactor(profitFactor),
	mNumTrades(numTrades),
	mPayOffRatio(payOffRatio),
	mMedianPayOffRatio(medianPayOffRatio),
	mExpectation(expectation),
	mMonteCarloPayoffRatio(monteCarloPayoff)
    {}

     RobustnessTestResult(const  RobustnessTestResult<Decimal>& rhs)
       : mWinRate(rhs.mWinRate),
	 mProfitFactor(rhs.mProfitFactor),
	 mNumTrades(rhs.mNumTrades),
	 mPayOffRatio(rhs.mPayOffRatio),
	 mMedianPayOffRatio(rhs.mMedianPayOffRatio),
	 mExpectation(rhs.mExpectation),
	 mMonteCarloPayoffRatio(rhs.mMonteCarloPayoffRatio)
    {}

    ~RobustnessTestResult()
    {}

    RobustnessTestResult<Decimal>&
    operator=(const RobustnessTestResult<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mWinRate = rhs.mWinRate;
      mProfitFactor = rhs.mProfitFactor;
      mNumTrades = rhs.mNumTrades;
      mPayOffRatio = rhs.mPayOffRatio;
      mMedianPayOffRatio = rhs.mMedianPayOffRatio;
      mExpectation = rhs.mExpectation;
      mMonteCarloPayoffRatio = rhs.mMonteCarloPayoffRatio;
      return *this;
    }

    /// Returns the PAL profitability (win rate percentage).
    const Decimal& getPALProfitability() const
    {
      return mWinRate;
    }

    /**
     * @brief Computes Monte Carlo adjusted profitability.
     *
     * Uses the Monte Carlo payoff ratio in place of the standard payoff ratio
     * in the PAL profitability formula:
     * Profitability = (PF / (PF + MC_PayoffRatio)) * 100.
     *
     * @return The Monte Carlo profitability as a percentage, or zero if the
     *         denominator is non-positive.
     */
    const Decimal getMonteCarloProfitability() const
    {
      Decimal pf(getProfitFactor());
      Decimal denominator(pf + getMonteCarloPayOffRatio());

      if (denominator > DecimalConstants<Decimal>::DecimalZero)
	return ((pf/denominator) * DecimalConstants<Decimal>::DecimalOneHundred);
      else
	return (DecimalConstants<Decimal>::DecimalZero);
    }

    /// Returns the profit factor (gross profit / gross loss).
    const Decimal& getProfitFactor() const
    {
      return mProfitFactor;
    }

    /// Returns the number of closed trades.
    unsigned long getNumTrades() const
    {
      return mNumTrades;
    }

    /// Returns the mean payoff ratio.
    const Decimal& getPayOffRatio() const
    {
      return mPayOffRatio;
    }

    /// Returns the median payoff ratio.
    const Decimal& getMedianPayOffRatio() const
    {
      return mMedianPayOffRatio;
    }

    /// Returns the Monte Carlo estimated payoff ratio.
    const Decimal& getMonteCarloPayOffRatio() const
    {
      return mMonteCarloPayoffRatio;;
    }

    /// Returns the R-multiple expectancy.
    const Decimal& getRMultipleExpectancy() const
    {
      return mExpectation;
    }

  private:
    Decimal mWinRate;
    Decimal mProfitFactor;
    unsigned long mNumTrades;
    Decimal mPayOffRatio;
    Decimal mMedianPayOffRatio;
    Decimal mExpectation;
    Decimal mMonteCarloPayoffRatio;
  };

  /**
   * @brief Comparator that orders ProfitTargetStopPair by protective stop value.
   *
   * @tparam Decimal  The numeric type (e.g., dec::decimal<7>).
   */
  template <class Decimal> struct ProfitTargetStopComparator
  {
    bool operator() (const ProfitTargetStopPair<Decimal>& lhs,
		     const ProfitTargetStopPair<Decimal>& rhs) const
    {
      return (lhs.getProtectiveStop() < rhs.getProtectiveStop());
    }

  };

  /**
   * @brief Exception thrown by RobustnessCalculator on invalid state.
   */
  class RobustnessCalculatorException : public std::runtime_error
  {
  public:
    RobustnessCalculatorException(const std::string msg)
      : std::runtime_error(msg)
    {}

    ~RobustnessCalculatorException()
    {}

  };

  /**
   * @brief Aggregates robustness permutation results and determines pattern robustness.
   *
   * Collects backtest results keyed by profit-target/stop-loss pairs, then
   * evaluates whether the pattern meets minimum robustness, profitability,
   * and statistical dispersion criteria. Uses median profit factor, robust Qn
   * estimator, and tolerance-adjusted comparisons against neighbouring
   * permutations.
   *
   * @tparam Decimal  The numeric type (e.g., dec::decimal<7>).
   */
  template <class Decimal> class RobustnessCalculator
  {
  public:
    /// Const iterator over robustness test results ordered by stop value.
    typedef typename std::map<ProfitTargetStopPair<Decimal>,
			      std::shared_ptr<RobustnessTestResult<Decimal>>,
			      ProfitTargetStopComparator<Decimal>>::const_iterator RobustnessTestResultIterator;

  public:
    /**
     * @brief Constructs the calculator for a specific pattern and robustness criteria.
     *
     * @param thePattern              The PriceActionLab pattern under test.
     * @param permutationAttributes   Specification of permutation layout.
     * @param robustnessCriteria      Thresholds and tolerance settings.
     * @param debug                   If true, emits diagnostic output to stdout.
     */
    RobustnessCalculator(shared_ptr<PriceActionLabPattern> thePattern,
			 shared_ptr<RobustnessPermutationAttributes> permutationAttributes,
			 const PatternRobustnessCriteria<Decimal>& robustnessCriteria,
			 bool debug = false)
      : mPatternToTest (thePattern),
	mPermutationAttributes (permutationAttributes),
	mRobustnessCriteria(robustnessCriteria),
	mNumberProfitableResults(0),
	mDebug(debug),
	mRobustnessResults(),
	mRequiredProfitability(RobustnessCalculator<Decimal>::requiredPALProfitability(robustnessCriteria.getDesiredProfitFactor(),
										    thePattern->getPayoffRatio(),
										    robustnessCriteria.getProfitabilitySafetyFactor())),
      mNumberPALProfitableResults(0),
      mRobustnessStats()
	{}

    RobustnessCalculator(const RobustnessCalculator<Decimal>& rhs)
      : mPatternToTest (rhs.mPatternToTest),
	mPermutationAttributes (rhs.mPermutationAttributes),
	mRobustnessCriteria(rhs.mRobustnessCriteria),
	mNumberProfitableResults(rhs.mNumberProfitableResults),
	mDebug(rhs.mDebug),
	mRobustnessResults(rhs.mRobustnessResults),
	mRequiredProfitability(rhs.mRequiredProfitability),
	mNumberPALProfitableResults(rhs.mNumberPALProfitableResults),
	mRobustnessStats(rhs.mRobustnessStats)
    {}

    ~RobustnessCalculator()
      {}

    RobustnessCalculator<Decimal>&
    operator=(const RobustnessCalculator<Decimal> &rhs)
    {
      if (this == &rhs)
	return *this;

      mPatternToTest = rhs.mPatternToTest;
      mPermutationAttributes = rhs.mPermutationAttributes;
      mRobustnessCriteria = rhs.mRobustnessCriteria;
      mNumberProfitableResults = rhs.mNumberProfitableResults;
      mRobustnessResults = rhs.mRobustnessResults;
      mRequiredProfitability = rhs.mRequiredProfitability;
      mDebug = rhs.mDebug;
      mNumberPALProfitableResults = rhs.mNumberPALProfitableResults;
      mRobustnessStats = rhs.mRobustnessStats;
      return *this;
    }

    /**
     * @brief Adds a permutation backtest result to the calculator.
     *
     * Updates internal profit-factor statistics, profitable-result counters,
     * and the results map keyed by the pattern's profit-target/stop pair.
     *
     * @param testResult  Shared pointer to the permutation's backtest metrics.
     * @param pattern     The PriceActionLab pattern corresponding to the result.
     * @throws RobustnessCalculatorException If a result for the same stop already exists.
     */
    void addTestResult (std::shared_ptr<RobustnessTestResult<Decimal>> testResult,
			std::shared_ptr<PriceActionLabPattern> pattern)
    {
      ProfitTargetStopPair<Decimal> pairKey (pattern->getProfitTargetAsDecimal(),
					  pattern->getStopLossAsDecimal());

      mRobustnessStats.addValue (testResult->getProfitFactor());
      if (testResult->getProfitFactor() > DecimalConstants<Decimal>::DecimalOne)
	mNumberProfitableResults = mNumberProfitableResults + DecimalConstants<Decimal>::DecimalOne;

      if (equalWithTolerance (mRequiredProfitability, testResult->getMonteCarloProfitability(),
			       mRobustnessCriteria.getRobustnessTolerance()))
	mNumberPALProfitableResults = mNumberPALProfitableResults + DecimalConstants<Decimal>::DecimalOne;

      if (mRobustnessResults.find (pairKey) == endRobustnessTestResults())
	mRobustnessResults.insert (std::make_pair (pairKey, testResult));
      else
	throw RobustnessCalculatorException ("RobustnessCalculator::addTestResult - stop already exists");
    }

    /// Returns a const iterator to the first robustness test result.
    RobustnessCalculator<Decimal>::RobustnessTestResultIterator beginRobustnessTestResults() const
    {
      return  mRobustnessResults.begin();
    }

    /// Returns a const iterator past the last robustness test result.
    RobustnessCalculator<Decimal>::RobustnessTestResultIterator endRobustnessTestResults() const
    {
      return  mRobustnessResults.end();
    }

    /**
     * @brief Computes the robustness index as a percentage.
     *
     * Defined as (number of profitable permutations / total permutations) * 100.
     *
     * @return The robustness index in [0, 100], or zero if no entries exist.
     */
    Decimal getRobustnessIndex() const
    {
      unsigned long numEntries = getNumEntries();
      Decimal numEntriesDecimal = Decimal((unsigned int) numEntries);

      if (numEntries > 0)
	return (mNumberProfitableResults / numEntriesDecimal) * DecimalConstants<Decimal>::DecimalOneHundred;
      else
	return DecimalConstants<Decimal>::DecimalZero;
    }

    /**
     * @brief Computes the PAL profitability index as a percentage.
     *
     * Defined as (number of PAL-profitable permutations / total permutations) * 100,
     * where PAL-profitable means the Monte Carlo profitability meets the required
     * threshold within tolerance.
     *
     * @return The profitability index in [0, 100], or zero if no entries exist.
     */
    Decimal getProfitabilityIndex() const
    {
      unsigned long numEntries = getNumEntries();
      Decimal numEntriesDecimal = Decimal((unsigned int) numEntries);

      if (numEntries > 0)
	return (mNumberPALProfitableResults / numEntriesDecimal) * DecimalConstants<Decimal>::DecimalOneHundred;
      else
	return DecimalConstants<Decimal>::DecimalZero;
    }

    /**
     * @brief Determines whether the pattern is robust across all permutations.
     *
     * Checks, in order: that enough permutation entries exist, that the
     * robustness index meets the minimum, that the median profit factor
     * exceeds 1.5, that the median minus one Qn is above 1.0, and that
     * the six neighbouring permutations on each side of the reference pass
     * tolerance-adjusted profitability, payoff, and profit-factor tests.
     *
     * @return True if all robustness criteria are satisfied.
     * @throws RobustnessCalculatorException If the number of entries does not
     *         match the expected number of permutations.
     */
    //bool isRobust() const
    bool isRobust()
    {
      if (getNumEntries() < mPermutationAttributes->getNumberOfPermutations())
	throw RobustnessCalculatorException ("RobustnessCalculator::isRobust - number of entries does not match number of permutations specified in PermutationAttributes");

      if (getRobustnessIndex() < mRobustnessCriteria.getMinimumRobustnessIndex())
	return false;

      Decimal medianProfitFactor (mRobustnessStats.getMedian());
      if (medianProfitFactor < DecimalConstants<Decimal>::DecimalOnePointFive)
	return false;

      Decimal robustQn (mRobustnessStats.getRobustQn());
      if ((medianProfitFactor - robustQn) < DecimalConstants<Decimal>::DecimalOne)
	return false;

      // If the lower 2 standard deviation of the profit factor is < 1.0 and the smallest profit factor < 1.0
      // then reject as non-robust

      if ((medianProfitFactor - (robustQn * DecimalConstants<Decimal>::DecimalTwo)) < DecimalConstants<Decimal>::DecimalOne)
	{
	  if (mRobustnessStats.getSmallestValue() < DecimalConstants<Decimal>::DecimalOne)
	    return false;
	}

      // if (getProfitabilityIndex() < DecimalConstants<Decimal>::TwoThirds)
      // return false;

      unsigned long numSignificant = getNumNeighboringSignificantResults();
      if (mDebug)
	std::cout << "!! Num Significant neighboring results = " << numSignificant << std::endl;

      Decimal originalStop = getOriginalPatternStop();
      Decimal originalTarget = getOriginalPatternTarget();

      if (mDebug)
	{
	  std::cout << "RobustnessCalculator::isRobust - original pattern stop = " << originalStop << std::endl;
	  std::cout << "RobustnessCalculator::isRobust - original pattern target = " << originalTarget << std::endl;
	}

      ProfitTargetStopPair<Decimal> pairKey (originalTarget, originalStop);

      Decimal requiredProfitability = mRequiredProfitability;
      if (mDebug)
	std::cout << "!!@@@ Required profitability = " << requiredProfitability << std::endl;
      typename RobustnessCalculator<Decimal>::RobustnessTestResultIterator refPairIterator =
	mRobustnessResults.find(pairKey);

      if (refPairIterator == endRobustnessTestResults())
	{
	  throw RobustnessCalculatorException("isRobust: could not find results for reference pattern with stop = " +num::toString(originalStop));
	}
      if (!isPermutationResultRobust (refPairIterator->second,
				      requiredProfitability,
				      0))
	return false;

      if (mDebug)
	std::cout << "!!@@@ Reference pattern is robust " << std::endl;
      typename RobustnessCalculator<Decimal>::RobustnessTestResultIterator it = refPairIterator;

      // Start testing one result above reference result

      if (mDebug)
	std::cout << "Testing permutation above reference permutation" << std::endl << std::endl;
      it++;
      uint32_t i = 1;
      for (; ((i <= numSignificant) && (it != endRobustnessTestResults())); i++, it++)
	{
	  if (mDebug)
	    std::cout << "Permutation " << i << " above reference permutation" << std::endl;

	  if (!isPermutationResultRobust (it->second,
					  requiredProfitability,
					  i,
					  it->first.getProtectiveStop()))
	    {
	      if (mDebug)
		std::cout << "Failed testing above reference value" << std::endl;
	      return false;
	    }
	}


      //it = refPairIterator;
      it = mRobustnessResults.find(pairKey);

      // Test results below reference result
      // Start testing one result above reference result
      it--;

      i = 1;
      for (; (i <= numSignificant); i++, it--)
	{
	  if (mDebug)
	    {
	      std::cout << "Permutation " << i << " below reference permutation" << std::endl;

	      std::cout << "permutation stop = " << it->first.getProtectiveStop() << std::endl;
	      std::cout << "permutation target = " << it->first.getProfitTarget() << std::endl;

	    }
	  if (!isPermutationResultRobust (it->second,
					  requiredProfitability,
					  i,
					  it->first.getProtectiveStop()))
	    {
	      if (mDebug)
		std::cout << "Failed testing below reference value" << std::endl;

	      if (mDebug)
		std::cout << "Returning false for testing below reference value" << std::endl;
	      return false;
	    }
	}

      /*uint32_t numFailuresAtBeginning = getNumRobustnessFailuresAtBeginning (requiredProfitability);
      uint32_t numFailuresAtEnd = getNumRobustnessFailuresAtEnd (requiredProfitability);

      if ((numFailuresAtBeginning == mPermutationAttributes->numEntriesToTestAtBeginning()) &&
	  (numFailuresAtEnd == mPermutationAttributes->numEntriesToTestAtEnd()))
	{
	  return false;
	  } */

      return true;

    }

    /**
     * @brief Counts robustness failures among the leading permutation entries.
     *
     * @param theRequiredProfitability  The profitability threshold to test against.
     * @return Number of failed entries at the beginning of the permutation range.
     */
    uint32_t getNumRobustnessFailuresAtBeginning(Decimal theRequiredProfitability) const
    {
      typename RobustnessCalculator<Decimal>::RobustnessTestResultIterator it =
	beginRobustnessTestResults();
      uint32_t entriesToTest = mPermutationAttributes->numEntriesToTestAtBeginning();
      uint32_t numFailedEntries = 0;
      uint32_t entries = 0;
      uint32_t numIterationsAway = mPermutationAttributes->getNumPermutationsBelowRef();

      for (; ((entries < entriesToTest) && (it != endRobustnessTestResults())); entries++, it++)
	{
	  if (!isPermutationResultRobust (it->second,
					  theRequiredProfitability,
					  numIterationsAway))
	    {
	      numFailedEntries++;
	    }

	  numIterationsAway--;
	}

      return numFailedEntries;
    }

    /**
     * @brief Counts robustness failures among the trailing permutation entries.
     *
     * @param theRequiredProfitability  The profitability threshold to test against.
     * @return Number of failed entries at the end of the permutation range.
     */
    uint32_t getNumRobustnessFailuresAtEnd(Decimal theRequiredProfitability) const
    {
      typename RobustnessCalculator<Decimal>::RobustnessTestResultIterator it =
	endRobustnessTestResults();
      uint32_t entriesToTest = mPermutationAttributes->numEntriesToTestAtEnd();
      uint32_t numFailedEntries = 0;
      uint32_t entries = 0;
      uint32_t numIterationsAway = mPermutationAttributes->getNumPermutationsAboveRef();

      it--;
      for (; (entries < entriesToTest); entries++, it--)
	{
	  if (!isPermutationResultRobust (it->second,
					  theRequiredProfitability,
					  numIterationsAway))
	    {
	      numFailedEntries++;
	    }

	  numIterationsAway--;
	}

      return numFailedEntries;
    }

    /// Returns the number of permutation results stored in the calculator.
    unsigned long getNumEntries() const
    {
      return mRobustnessResults.size();
    }

    /**
     * @brief Computes the required PAL profitability for a given profit factor and payoff ratio.
     *
     * Formula: (profitFactor / (profitFactor + safetyFactor * payoffRatio)) * 100.
     *
     * @param profitFactor  The target profit factor.
     * @param payoffRatio   The pattern's payoff ratio.
     * @param safetyFactor  Safety factor for commission/slippage adjustment.
     * @return The required profitability as a percentage.
     */
    static Decimal requiredPALProfitability(const Decimal& profitFactor,
						const Decimal& payoffRatio,
						const Decimal& safetyFactor)
    {
      Decimal denom = profitFactor + (safetyFactor * payoffRatio);

      return ((profitFactor / denom) * DecimalConstants<Decimal>::DecimalOneHundred);
    }

  private:

    PercentNumber<Decimal> getToleranceForDistanceAndPercentage (unsigned long iterationsAwayFromRef,
						   const Decimal& stopLossCandidate) const
    {
      Decimal originalStop (getOriginalPatternStop());

      Decimal candidatePercentDistance;

      candidatePercentDistance = num::abs (PercentReturn (originalStop, stopLossCandidate));
      unsigned long intPercentDistance = (unsigned long) num::to_double(candidatePercentDistance);
      Decimal toleranceForPercentage (mRobustnessCriteria.getDecimalToleranceForPercentDifference
					    (intPercentDistance));

      Decimal toleranceForDistance (mRobustnessCriteria.getDecimalToleranceForIterations(iterationsAwayFromRef));

      return PercentNumber<Decimal>::createPercentNumber (toleranceForPercentage * toleranceForDistance);

    }

    bool isPermutationResultRobust (std::shared_ptr<RobustnessTestResult<Decimal>> result,
				    const Decimal& requiredProfitability,
				    unsigned long iterationsAwayFromRef,
				    const Decimal& stopLossCandidate) const
    {
      PercentNumber<Decimal> tolerance (getToleranceForDistanceAndPercentage (iterationsAwayFromRef, stopLossCandidate));
      //std::cout << "For " << iterationsAwayFromRef << " iterations away from reference, tolerance = " << tolerance.getAsPercent() << std::endl;

      if (!equalWithTolerance (requiredProfitability, result->getMonteCarloProfitability(),
			       tolerance))
	{
	  if (mDebug)
	    std::cout << "isPermutationResultRobust test failed with test for requiredProfitability " << requiredProfitability << " found profitability of " <<  result->getPALProfitability() << std::endl;
	  return false;
	}

      //getToleranceForNumTrades

      Decimal resultPayoff (result->getMonteCarloPayOffRatio());
      if (resultPayoff == DecimalConstants<Decimal>::DecimalZero)
	resultPayoff = result->getMedianPayOffRatio();

      if (!equalWithTolerance (mPatternToTest->getPayoffRatio(), resultPayoff,
			       mRobustnessCriteria.getToleranceForNumTrades (result->getNumTrades())))
	{
	  if (mDebug)
	    std::cout << "isPermutationResultRobust test failed with test for required payoff ratio " << mPatternToTest->getPayoffRatio() << " found payoff ratio of " <<  result->getMedianPayOffRatio() << std::endl;
	  return false;
	}

      if (!equalWithTolerance (mRobustnessCriteria.getDesiredProfitFactor(),
			       result->getProfitFactor(),
			       tolerance))
	return false;

      return true;

    }


    // Note this method is meant to be called on the results that 'neighbor' the original
    // profit target, stop pair

    bool isPermutationResultRobust (std::shared_ptr<RobustnessTestResult<Decimal>> result,
				    const Decimal& requiredProfitability,
				    unsigned long iterationsAwayFromRef) const
    {
      if (!equalWithTolerance (requiredProfitability, result->getMonteCarloProfitability(),
			       mRobustnessCriteria.getToleranceForIterations(iterationsAwayFromRef)))
	{
	  if (mDebug)
	    std::cout << "isPermutationResultRobust test failed with test for requiredProfitability " << requiredProfitability << " found profitability of " <<  result->getPALProfitability() << std::endl;
	  return false;
	}

      //getToleranceForNumTrades

      Decimal resultPayoff (result->getMonteCarloPayOffRatio());
      if (resultPayoff == DecimalConstants<Decimal>::DecimalZero)
	resultPayoff = result->getMedianPayOffRatio();

      if (!equalWithTolerance (mPatternToTest->getPayoffRatio(), resultPayoff,
			       mRobustnessCriteria.getToleranceForNumTrades (result->getNumTrades())))
	{
	  if (mDebug)
	    std::cout << "isPermutationResultRobust test failed with test for required payoff ratio " << mPatternToTest->getPayoffRatio() << " found payoff ratio of " <<  result->getMedianPayOffRatio() << std::endl;
	  return false;
	}

      if (!equalWithTolerance (mRobustnessCriteria.getDesiredProfitFactor(),
			       result->getProfitFactor(),
			       mRobustnessCriteria.getToleranceForIterations(iterationsAwayFromRef)))
	return false;

      return true;
    }

    const Decimal getOriginalPatternStop() const
    {
      return mPatternToTest->getStopLossAsDecimal();
    }

    const Decimal getOriginalPatternTarget() const
    {
      return mPatternToTest->getProfitTargetAsDecimal();
    }

    unsigned long getNumNeighboringSignificantResults() const
    {
      return 6;

      //Decimal originalPatternStop (mPatternToTest->getStopLossAsDecimal());
      //Decimal permutationIncrement(originalPatternStop / Decimal((unsigned int) mPermutationAttributes->getPermutationsDivisor()));

      //permutationIncrement = permutationIncrement * mPatternToTest->getPayoffRatio();
      //std::cout << "getNumNeighboringSignificantResults(): permutation increment = " << permutationIncrement << std::endl;

      // The rationale for number of signifant stop/target pairs to check is that at most
      // parameters 20% away from the original are significant. As we get farther away we
      // should not expect the same level of profitability

      //Decimal numSignificant (DecimalConstants<Decimal>::TwentyPercent/permutationIncrement);

      //std::cout << "getNumNeighboringSignificantResults(): numSignificant = " << numSignificant << std::endl;
      //return (unsigned long) num::to_double(numSignificant);
    }

    /*
    unsigned long getNumNeighboringSignificantResults() const
    {
      Decimal numPermutations =
	Decimal(mPermutationAttributes->getNumberOfPermutations());
      Decimal numSignificant7 = Decimal(7);

      Decimal numSignificant (numPermutations * RobustnessCalculator<Decimal>::TwentyFivePercent);
      if (mDebug)
	std::cout << "getNumNeighboringSignificantResults: numPermutations = " << numPermutations << std::endl;

      if (mPermutationAttributes->getNumberOfPermutations() == 30)
	numSignificant = numSignificant7;

      //return (unsigned long) numSignificant.getAsInteger();
      return (unsigned long) num::to_double(numSignificant);
    }
    */

    bool equalWithTolerance (const Decimal& referenceValue,
			     const Decimal& comparisonValue,
			     const PercentNumber<Decimal>& tolerance) const
    {
      Decimal lowerRefValue(referenceValue - (tolerance.getAsPercent() * referenceValue));
      return (comparisonValue >= lowerRefValue);
    }

  private:
    std::shared_ptr<PriceActionLabPattern> mPatternToTest;
    std::shared_ptr<RobustnessPermutationAttributes> mPermutationAttributes;
    PatternRobustnessCriteria<Decimal> mRobustnessCriteria;
    Decimal mNumberProfitableResults;
    bool mDebug;
    std::map<ProfitTargetStopPair<Decimal>,
	     std::shared_ptr<RobustnessTestResult<Decimal>>,
	     ProfitTargetStopComparator<Decimal>> mRobustnessResults;
    Decimal mRequiredProfitability;
    Decimal mNumberPALProfitableResults;
    static Decimal TwentyPercent;
    static Decimal TwentyFivePercent;
    SummaryStats<Decimal> mRobustnessStats;
  };

  template <class Decimal> Decimal
  RobustnessCalculator<Decimal>::TwentyPercent(DecimalConstants<Decimal>::createDecimal("0.20"));

  template <class Decimal> Decimal
  RobustnessCalculator<Decimal>::TwentyFivePercent(DecimalConstants<Decimal>::createDecimal("0.25"));

  /**
   * @brief Orchestrates a robustness test across stop/target permutations.
   *
   * Clones the backtester for each permutation of profit-target and stop-loss,
   * runs the backtest, and feeds results into a RobustnessCalculator to
   * determine whether the original pattern is robust.
   *
   * @tparam Decimal  The numeric type (e.g., dec::decimal<7>).
   */
  template <class Decimal> class RobustnessTest
  {
  public:
    /**
     * @brief Constructs the robustness test.
     *
     * @param backtester              Backtester instance to clone for each permutation.
     * @param aPalStrategy            The strategy wrapping the pattern under test.
     * @param permutationAttributes   Specification of permutation layout.
     * @param robustnessCriteria      Thresholds and tolerance settings.
     * @param factory                 AST factory for constructing permuted pattern nodes.
     */
    RobustnessTest (std::shared_ptr<BackTester<Decimal>> backtester,
		    std::shared_ptr<PalStrategy<Decimal>> aPalStrategy,
		    shared_ptr<RobustnessPermutationAttributes> permutationAttributes,
		    const PatternRobustnessCriteria<Decimal>& robustnessCriteria,
		    std::shared_ptr<AstFactory> factory)
      : mTheBacktester(backtester),
	mTheStrategy(aPalStrategy),
	mPermutationAttributes(permutationAttributes),
	mRobustnessCriteria(robustnessCriteria),
	mAstFactory(factory),
	mRobustnessQuality(aPalStrategy->getPalPattern(),
			   permutationAttributes,
			   robustnessCriteria, false)
    {}

    ~RobustnessTest()
    {}

    RobustnessTest (const RobustnessTest<Decimal>& rhs)
      : mTheBacktester(rhs.mTheBacktester),
	mTheStrategy(rhs.mTheStrategy),
	mPermutationAttributes(rhs.mPermutationAttributes),
	mRobustnessCriteria(rhs.mRobustnessCriteria),
	mAstFactory(rhs.mAstFactory),
	mRobustnessQuality(rhs.mRobustnessQuality)
    {}

    const RobustnessTest<Decimal>&
    operator=(const RobustnessTest<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;

      mTheBacktester = rhs.mTheBacktester;
      mTheStrategy = rhs.mTheStrategy;
      mPermutationAttributes = rhs.mPermutationAttributes;
      mRobustnessCriteria = rhs.mRobustnessCriteria;
      mAstFactory = rhs.mAstFactory;
      mRobustnessQuality = rhs.mRobustnessQuality;

      return *this;
    }

    /**
     * @brief Executes the robustness test across all permutations.
     *
     * @return True if the pattern passes all robustness criteria.
     */
    bool runRobustnessTest()
    {
      // std::cout << "RobustnessTest::runRobustnessTest" << std::endl;

      std::shared_ptr<PriceActionLabPattern> originalPattern = mTheStrategy->getPalPattern();
      Decimal originalPatternStop (originalPattern->getStopLossAsDecimal());
      Decimal permutationIncrement(originalPatternStop / Decimal((unsigned int) mPermutationAttributes->getPermutationsDivisor()));
      Decimal numProfitablePermutations(DecimalConstants<Decimal>::DecimalZero);
      Decimal requiredPayoffRatio (originalPattern->getPayoffRatio());
      Decimal requiredProfitability (RobustnessCalculator<Decimal>::requiredPALProfitability (mRobustnessCriteria.getDesiredProfitFactor(), requiredPayoffRatio, mRobustnessCriteria.getProfitabilitySafetyFactor()));
      bool originalPatternProfitabilityFailed = false;

      //std::cout << "RobustnessTest::runRobustnessTest - finished initializing variables" << std::endl;
      // Test original profit target, stop pair

      std::shared_ptr<BackTester<Decimal>> clonedBackTester = mTheBacktester->clone();
      clonedBackTester->addStrategy(mTheStrategy);
      clonedBackTester->backtest();

      addTestResult (createRobustnessTestResult (clonedBackTester),
		     originalPattern);

      //std::cout << "RobustnessTest::runRobustnessTest - finished adding reference (target, stop) pair test result" << std::endl;

      Decimal lowestStopToTest(calculateLowestStopPermutationValue (originalPatternStop,
								   permutationIncrement));
      Decimal stopToTest(lowestStopToTest);
      Decimal profitTargetToTest(stopToTest * requiredPayoffRatio);

      uint32_t i;
      // Test permutation below reference profit target, stop pair
      for (i = 1; i <= mPermutationAttributes->getNumPermutationsBelowRef(); i++)
	{
	  backTestNewPermutation (originalPattern,
				  stopToTest,
				  profitTargetToTest);

	  stopToTest = stopToTest + permutationIncrement;
	  profitTargetToTest = stopToTest * requiredPayoffRatio;
	}

      stopToTest = originalPatternStop + permutationIncrement;
      profitTargetToTest = (stopToTest * requiredPayoffRatio);

      // Test permutation above reference profit target, stop pair
      for (i = 1; i <= mPermutationAttributes->getNumPermutationsAboveRef(); i++)
	{
	  backTestNewPermutation (originalPattern,
				  stopToTest,
				  profitTargetToTest);

	  stopToTest = stopToTest + permutationIncrement;
	  profitTargetToTest = stopToTest * requiredPayoffRatio;
	}

      //std::cout << "RobustnessTest::runRobustnessTest - finished adding all test permutations." << std::endl;
      //std::cout << "RobustnessTest::runRobustnessTest - calling isRobust" << std::endl;

      return (mRobustnessQuality.isRobust ());
    }

    /// Returns a const reference to the underlying RobustnessCalculator.
    const RobustnessCalculator<Decimal>& getRobustnessCalculator() const
    {
      return mRobustnessQuality;
    }

  private:
    void backTestNewPermutation (std::shared_ptr<PriceActionLabPattern> aPattern,
				 const Decimal& newStopLoss,
				 const Decimal& newProfitTarget)
    {
      decimal7 *newStopLossPtr = mAstFactory->getDecimalNumber ((char *) num::toString(newStopLoss).c_str());
      decimal7 *newProfitTargetPtr = mAstFactory->getDecimalNumber ((char *)num::toString(newProfitTarget).c_str());

      std::shared_ptr<BackTester<Decimal>> clonedBackTester = mTheBacktester->clone();


      ProfitTargetInPercentExpression *profitTarget;
      StopLossInPercentExpression *stopLoss;
      std::shared_ptr<PriceActionLabPattern> clonedPattern;
      std::shared_ptr<PalLongStrategy<Decimal>> longStrategy;
      std::shared_ptr<PalShortStrategy<Decimal>> shortStrategy;

      if (aPattern->isLongPattern())
	{
	  profitTarget = mAstFactory->getLongProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getLongStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  longStrategy = std::make_shared<PalLongStrategy<Decimal>>(mTheStrategy->getStrategyName(),
								 clonedPattern,
								 mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(longStrategy);

	}
      else
	{
	  profitTarget = mAstFactory->getShortProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getShortStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  shortStrategy = std::make_shared<PalShortStrategy<Decimal>>(mTheStrategy->getStrategyName(),
								   clonedPattern,
								   mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(shortStrategy);
	}

      clonedBackTester->backtest();
      addTestResult (createRobustnessTestResult (clonedBackTester),
		     clonedPattern);


    }

    Decimal calculateLowestStopPermutationValue (const Decimal& originalStop,
						       const Decimal& permutationIncrement) const
    {
      return (originalStop -
	      (permutationIncrement *  Decimal (mPermutationAttributes->getNumPermutationsBelowRef())));
    }

    std::shared_ptr<RobustnessTestResult<Decimal>>
    createRobustnessTestResult(std::shared_ptr<BackTester<Decimal>> backtester)
    {
      //      ClosedPositionHistory<Decimal> closedPositions =
      //getClosedPositionHistory (backtester);

      ClosedPositionHistory<Decimal> closedPositions = backtester->getClosedPositionHistory();

      return
	make_shared<RobustnessTestResult<Decimal>> (closedPositions.getMedianPALProfitability(),
						 closedPositions.getProfitFactor(),
						 closedPositions.getNumPositions(),
						 closedPositions.getPayoffRatio(),
						 closedPositions.getMedianPayoffRatio(),
						 closedPositions.getRMultipleExpectancy());
    }

    void addTestResult (std::shared_ptr<RobustnessTestResult<Decimal>> testResult,
			std::shared_ptr<PriceActionLabPattern> pattern)
    {
      mRobustnessQuality.addTestResult (testResult, pattern);
    }

  private:
    std::shared_ptr<BackTester<Decimal>> mTheBacktester;
    std::shared_ptr<PalStrategy<Decimal>> mTheStrategy;
    shared_ptr<RobustnessPermutationAttributes> mPermutationAttributes;
    PatternRobustnessCriteria<Decimal> mRobustnessCriteria;
    std::shared_ptr<AstFactory> mAstFactory;
    RobustnessCalculator<Decimal> mRobustnessQuality;
  };


  /////////////////////////

  /**
   * @brief Monte Carlo variant of the robustness test.
   *
   * Same permutation logic as RobustnessTest, but each permutation result is
   * augmented with a 200-iteration Monte Carlo estimate of the payoff ratio,
   * providing a more stable payoff metric for the robustness evaluation.
   *
   * @tparam Decimal  The numeric type (e.g., dec::decimal<7>).
   */
  template <class Decimal> class RobustnessTestMonteCarlo
  {
  public:
    /**
     * @brief Constructs the Monte Carlo robustness test.
     *
     * @param backtester              Backtester instance to clone for each permutation.
     * @param aPalStrategy            The strategy wrapping the pattern under test.
     * @param permutationAttributes   Specification of permutation layout.
     * @param robustnessCriteria      Thresholds and tolerance settings.
     * @param factory                 AST factory for constructing permuted pattern nodes.
     */
    RobustnessTestMonteCarlo (std::shared_ptr<BackTester<Decimal>> backtester,
			      std::shared_ptr<PalStrategy<Decimal>> aPalStrategy,
			      shared_ptr<RobustnessPermutationAttributes> permutationAttributes,
			      const PatternRobustnessCriteria<Decimal>& robustnessCriteria,
			      std::shared_ptr<AstFactory> factory)
      : mTheBacktester(backtester),
	mTheStrategy(aPalStrategy),
	mPermutationAttributes(permutationAttributes),
	mRobustnessCriteria(robustnessCriteria),
	mAstFactory(factory),
	mRobustnessQuality(aPalStrategy->getPalPattern(),
			   permutationAttributes,
			   robustnessCriteria, false)
    {}

    ~RobustnessTestMonteCarlo()
    {}

    RobustnessTestMonteCarlo (const RobustnessTestMonteCarlo<Decimal>& rhs)
      : mTheBacktester(rhs.mTheBacktester),
	mTheStrategy(rhs.mTheStrategy),
	mPermutationAttributes(rhs.mPermutationAttributes),
	mRobustnessCriteria(rhs.mRobustnessCriteria),
	mAstFactory(rhs.mAstFactory),
	mRobustnessQuality(rhs.mRobustnessQuality)
    {}

    const RobustnessTestMonteCarlo<Decimal>&
    operator=(const RobustnessTestMonteCarlo<Decimal>& rhs)
    {
      if (this == &rhs)
	return *this;

      mTheBacktester = rhs.mTheBacktester;
      mTheStrategy = rhs.mTheStrategy;
      mPermutationAttributes = rhs.mPermutationAttributes;
      mRobustnessCriteria = rhs.mRobustnessCriteria;
      mAstFactory = rhs.mAstFactory;
      mRobustnessQuality = rhs.mRobustnessQuality;

      return *this;
    }

    /**
     * @brief Executes the Monte Carlo robustness test across all permutations.
     *
     * @return True if the pattern passes all robustness criteria.
     */
    bool runRobustnessTest()
    {
      std::shared_ptr<PriceActionLabPattern> originalPattern = mTheStrategy->getPalPattern();
      Decimal originalPatternStop (originalPattern->getStopLossAsDecimal());
      Decimal permutationIncrement(originalPatternStop / Decimal((unsigned int) mPermutationAttributes->getPermutationsDivisor()));
      Decimal numProfitablePermutations(DecimalConstants<Decimal>::DecimalZero);
      Decimal requiredPayoffRatio (originalPattern->getPayoffRatio());
      Decimal requiredProfitability (RobustnessCalculator<Decimal>::requiredPALProfitability (mRobustnessCriteria.getDesiredProfitFactor(), requiredPayoffRatio, mRobustnessCriteria.getProfitabilitySafetyFactor()));
      std::shared_ptr<BackTester<Decimal>> clonedBackTester = mTheBacktester->clone();
      clonedBackTester->addStrategy(mTheStrategy);
      clonedBackTester->backtest();

      addTestResult (clonedBackTester, originalPattern);

      Decimal lowestStopToTest(calculateLowestStopPermutationValue (originalPatternStop,
								   permutationIncrement));
      Decimal stopToTest(lowestStopToTest);
      Decimal profitTargetToTest(stopToTest * requiredPayoffRatio);

      uint32_t i;
      // Test permutation below reference profit target, stop pair
      for (i = 1; i <= mPermutationAttributes->getNumPermutationsBelowRef(); i++)
	{
	  backTestNewPermutation (originalPattern,
				  stopToTest,
				  profitTargetToTest);

	  stopToTest = stopToTest + permutationIncrement;
	  profitTargetToTest = stopToTest * requiredPayoffRatio;
	}

      stopToTest = originalPatternStop + permutationIncrement;
      profitTargetToTest = (stopToTest * requiredPayoffRatio);

      // Test permutation above reference profit target, stop pair
      for (i = 1; i <= mPermutationAttributes->getNumPermutationsAboveRef(); i++)
	{
	  backTestNewPermutation (originalPattern,
				  stopToTest,
				  profitTargetToTest);

	  stopToTest = stopToTest + permutationIncrement;
	  profitTargetToTest = stopToTest * requiredPayoffRatio;
	}

      return (mRobustnessQuality.isRobust ());
    }

    /// Returns a const reference to the underlying RobustnessCalculator.
    const RobustnessCalculator<Decimal>& getRobustnessCalculator() const
    {
      return mRobustnessQuality;
    }

  private:
    void backTestNewPermutation (std::shared_ptr<PriceActionLabPattern> aPattern,
				 const Decimal& newStopLoss,
				 const Decimal& newProfitTarget)
    {
      decimal7 *newStopLossPtr = mAstFactory->getDecimalNumber ((char *) num::toString(newStopLoss).c_str());
      decimal7 *newProfitTargetPtr = mAstFactory->getDecimalNumber ((char *)num::toString(newProfitTarget).c_str());

      std::shared_ptr<BackTester<Decimal>> clonedBackTester = mTheBacktester->clone();


      ProfitTargetInPercentExpression *profitTarget;
      StopLossInPercentExpression *stopLoss;
      std::shared_ptr<PriceActionLabPattern> clonedPattern;
      std::shared_ptr<PalLongStrategy<Decimal>> longStrategy;
      std::shared_ptr<PalShortStrategy<Decimal>> shortStrategy;

      if (aPattern->isLongPattern())
	{
	  profitTarget = mAstFactory->getLongProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getLongStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  longStrategy = std::make_shared<PalLongStrategy<Decimal>>(mTheStrategy->getStrategyName(),
								 clonedPattern,
								 mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(longStrategy);

	}
      else
	{
	  profitTarget = mAstFactory->getShortProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getShortStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  shortStrategy = std::make_shared<PalShortStrategy<Decimal>>(mTheStrategy->getStrategyName(),
								   clonedPattern,
								   mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(shortStrategy);
	}

      clonedBackTester->backtest();
      addTestResult (clonedBackTester, clonedPattern);
    }

    Decimal calculateLowestStopPermutationValue (const Decimal& originalStop,
						       const Decimal& permutationIncrement) const
    {
      return (originalStop -
	      (permutationIncrement *  Decimal (mPermutationAttributes->getNumPermutationsBelowRef())));
    }

    void addTestResult (std::shared_ptr<BackTester<Decimal>> backtester,
			std::shared_ptr<PriceActionLabPattern> pattern)
    {
      ClosedPositionHistory<Decimal> closedPositions = backtester->getClosedPositionHistory();

      Decimal profitability = closedPositions.getMedianPALProfitability();
      Decimal profitFactor = closedPositions.getProfitFactor();
      unsigned long numTrades = closedPositions.getNumPositions();
      Decimal payoffratio = closedPositions.getPayoffRatio();
      Decimal medianPayoff = closedPositions.getMedianPayoffRatio();
      Decimal expectancy = closedPositions.getRMultipleExpectancy();

      // Use Monte Carlo to (hopefully) get a better estimate of the payoff ratio
      //std::cout << "Running MonteCarloPayoffRatio for profit target " <<
      //pattern->getProfitTargetAsDecimal() << ", Stop = " << pattern->getStopLossAsDecimal() << std::endl << std::endl;

      MonteCarloPayoffRatio<Decimal> monteCarloPayoffCalculator (backtester, 200);
      Decimal monteCarloPayoff(monteCarloPayoffCalculator.runPermutationTest());

      std::shared_ptr<RobustnessTestResult<Decimal>> testResult =
	make_shared<RobustnessTestResult<Decimal>> (profitability,
						 profitFactor,
						 numTrades,
						 payoffratio,
						 medianPayoff,
						 expectancy,
						 monteCarloPayoff);

      mRobustnessQuality.addTestResult (testResult, pattern);
    }

  private:
    std::shared_ptr<BackTester<Decimal>> mTheBacktester;
    std::shared_ptr<PalStrategy<Decimal>> mTheStrategy;
    shared_ptr<RobustnessPermutationAttributes> mPermutationAttributes;
    PatternRobustnessCriteria<Decimal> mRobustnessCriteria;
    std::shared_ptr<AstFactory> mAstFactory;
    RobustnessCalculator<Decimal> mRobustnessQuality;
  };
}

#endif
