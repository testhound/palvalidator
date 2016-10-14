// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __ROBUSTNESS_TEST_H
#define __ROBUSTNESS_TEST_H 1

#include <exception>
#include <string>
#include <map>
#include <boost/date_time.hpp>
#include "decimal.h"
#include "DecimalConstants.h"
#include "PalStrategy.h"
#include "BackTester.h"
#include "PalAst.h"
#include "MonteCarloPermutationTest.h"

namespace mkc_timeseries
{
  using dec::decimal;
  using boost::gregorian::date;
  using std::make_shared;
  //
  // class PatternRobustnessCriteria
  //

  template <class Decimal> class PatternRobustnessCriteria
    {
    public:
      PatternRobustnessCriteria (const Decimal& minRobustnessIndex,
				 const Decimal& desiredProfitFactor,
				 const PercentNumber<Decimal>& tolerance,
				 const Decimal& profitabilitySafetyFactor)
	: mMinRobustnessIndex (minRobustnessIndex),
	  mDesiredProfitFactor(desiredProfitFactor),
	  mRobustnessTolerance(tolerance),
	  mProfitabilitySafetyFactor(profitabilitySafetyFactor)
	{}

      PatternRobustnessCriteria (const PatternRobustnessCriteria<Decimal>& rhs)
	: mMinRobustnessIndex (rhs.mMinRobustnessIndex),
	  mDesiredProfitFactor(rhs.mDesiredProfitFactor),
	  mRobustnessTolerance(rhs.mRobustnessTolerance),
	  mProfitabilitySafetyFactor(rhs.mProfitabilitySafetyFactor)
      {}

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

      ~PatternRobustnessCriteria()
      {}

      const Decimal& getMinimumRobustnessIndex() const
      {
	return mMinRobustnessIndex;
      }

      const Decimal& getDesiredProfitFactor() const
      {
	return mDesiredProfitFactor;
      }

      const PercentNumber<Decimal>& getRobustnessTolerance() const
      {
	return mRobustnessTolerance;
      }

      // Return the tolerance in percent for the number of iterations away we
      // are for from the original robustness target

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

      //
      // PAL Profitability is defined as:
      //
      // Profitability = ProfitFactory / (ProfitFactor + PayoffRatio)
      //
      // We modify Profitability to take commissions and slippage into
      // account by adding a safety factor in the range of 0.7 - 0.9 for
      // short term trading.
      //
      // Profitability = ProfitFactory / (ProfitFactor + SafetyFactor * PayoffRatio)

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

  //
  // class RobustnessPermutationAttributes
  //

  class RobustnessPermutationAttributes
  {
  public:
    RobustnessPermutationAttributes (const RobustnessPermutationAttributes& rhs)
      : mNumberOfPermutations(rhs.mNumberOfPermutations),
	mBelowRefPermutations(rhs.mBelowRefPermutations),
	mAboveRefPermutations(rhs.mAboveRefPermutations),
	mPermutationsDivisor(rhs.mPermutationsDivisor)
    {}

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

    virtual ~RobustnessPermutationAttributes() = 0;

    // First N permutations to test for robustness failure
    virtual uint32_t numEntriesToTestAtBeginning() const = 0;

    // Last N permutations to test for robustness failure
    virtual uint32_t numEntriesToTestAtEnd() const = 0;

    uint32_t getNumberOfPermutations() const
    {
      return mNumberOfPermutations;
    }

    uint32_t getNumPermutationsBelowRef() const
    {
      return mBelowRefPermutations;
    }

    uint32_t getNumPermutationsAboveRef() const
    {
      return mAboveRefPermutations;
    }

    uint32_t getPermutationsDivisor() const
    {
      return mPermutationsDivisor;
    }

  protected:
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

  //
  // class PALRobustnessPermutationAttributes
  //
  // mimics the settings that PriceActionLab uses for robustness testing

  class PALRobustnessPermutationAttributes 
    : public RobustnessPermutationAttributes
  {
  public:
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

    // First N permutations to test for robustness failure
    uint32_t numEntriesToTestAtBeginning() const
    {
      return 2;
    }

    // Last N permutations to test for robustness failure
    uint32_t numEntriesToTestAtEnd() const
    {
      return 2;
    }
  };

  /////////////////////////

  //
  // class StatSignificantAttributes
  //
  // mimics the settings that PriceActionLab uses for robustness testing

  class StatSignificantAttributes 
    : public RobustnessPermutationAttributes
  {
  public:
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

   // First N permutations to test for robustness failure
    uint32_t numEntriesToTestAtBeginning() const
    {
      return 3;
    }

    // Last N permutations to test for robustness failure
    uint32_t numEntriesToTestAtEnd() const
    {
      return 3;
    }
  };

  template <class Decimal> class ProfitTargetStopPair
  {
  public:
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

    const Decimal& getProfitTarget() const
    {
      return mProfitTarget;
    }

    const Decimal& getProtectiveStop() const
    {
      return mStop;
    }
    
  private:
    Decimal mProfitTarget;
    Decimal mStop;
  };

  template <class Decimal> class RobustnessTestResult
  {
  public:
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

    const Decimal& getPALProfitability() const
    {
      return mWinRate;
    }

    const Decimal getMonteCarloProfitability() const
    {
      Decimal pf(getProfitFactor());
      Decimal denominator(pf + getMonteCarloPayOffRatio());

      if (denominator > DecimalConstants<Decimal>::DecimalZero)
	return ((pf/denominator) * DecimalConstants<Decimal>::DecimalOneHundred);
      else
	return (DecimalConstants<Decimal>::DecimalZero);
    }
    
    const Decimal& getProfitFactor() const
    {
      return mProfitFactor;
    }

    unsigned long getNumTrades() const
    {
      return mNumTrades;
    }

    const Decimal& getPayOffRatio() const
    {
      return mPayOffRatio;
    }

    const Decimal& getMedianPayOffRatio() const
    {
      return mMedianPayOffRatio;
    }

    const Decimal& getMonteCarloPayOffRatio() const
    {
      return mMonteCarloPayoffRatio;;
    }

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

  //
  // class ProfitStopComparator
  //

  template <class Decimal> struct ProfitTargetStopComparator
  {
    bool operator() (const ProfitTargetStopPair<Decimal>& lhs,
		     const ProfitTargetStopPair<Decimal>& rhs) const
    {
      return (lhs.getProtectiveStop() < rhs.getProtectiveStop());
    }

  };

  class RobustnessCalculatorException : public std::runtime_error
  {
  public:
    RobustnessCalculatorException(const std::string msg) 
      : std::runtime_error(msg)
    {}
    
    ~RobustnessCalculatorException()
    {}
    
  };

  //
  // class RobustnessCalculator
  //
  // Purpose: to determine whether a PriceActionLab pattern is robust or not
  //

  template <class Decimal> class RobustnessCalculator
  {
  public:
    typedef typename std::map<ProfitTargetStopPair<Decimal>, 
			      std::shared_ptr<RobustnessTestResult<Decimal>>,
			      ProfitTargetStopComparator<Decimal>>::const_iterator RobustnessTestResultIterator;

  public:
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
	mNumberPALProfitableResults(0)
      {}

    RobustnessCalculator(const RobustnessCalculator<Decimal>& rhs)
      : mPatternToTest (rhs.mPatternToTest),
	mPermutationAttributes (rhs.mPermutationAttributes),
	mRobustnessCriteria(rhs.mRobustnessCriteria),
	mNumberProfitableResults(rhs.mNumberProfitableResults),
	mDebug(rhs.mDebug),
	mRobustnessResults(rhs.mRobustnessResults),
	mRequiredProfitability(rhs.mRequiredProfitability),
	mNumberPALProfitableResults(rhs.mNumberPALProfitableResults)
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

      return *this;
    }

    void addTestResult (std::shared_ptr<RobustnessTestResult<Decimal>> testResult,
			std::shared_ptr<PriceActionLabPattern> pattern)
    {
      ProfitTargetStopPair<Decimal> pairKey (pattern->getProfitTargetAsDecimal(),
					  pattern->getStopLossAsDecimal());
      
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

    RobustnessCalculator<Decimal>::RobustnessTestResultIterator beginRobustnessTestResults() const
    {
      return  mRobustnessResults.begin();
    }

    RobustnessCalculator<Decimal>::RobustnessTestResultIterator endRobustnessTestResults() const
    {
      return  mRobustnessResults.end();
    }

    Decimal getRobustnessIndex() const
    {
      unsigned long numEntries = getNumEntries();
      Decimal numEntriesDecimal = decimal_cast<Decimal::decimal_points>((unsigned int) numEntries);

      if (numEntries > 0)
	return (mNumberProfitableResults / numEntriesDecimal) * DecimalConstants<Decimal>::DecimalOneHundred;
      else
	return DecimalConstants<Decimal>::DecimalZero;
    }

    Decimal getProfitabilityIndex() const
    {
      unsigned long numEntries = getNumEntries();
      Decimal numEntriesDecimal = decimal_cast<Decimal::decimal_points>((unsigned int) numEntries);

      if (numEntries > 0)
	return (mNumberPALProfitableResults / numEntriesDecimal) * DecimalConstants<Decimal>::DecimalOneHundred;
      else
	return DecimalConstants<Decimal>::DecimalZero;
    }

    bool isRobust() const
    {
      if (getNumEntries() < mPermutationAttributes->getNumberOfPermutations())
	throw RobustnessCalculatorException ("RobustnessCalculator::isRobust - number of entries does not match number of permutations specified in PermutationAttributes");

      if (getRobustnessIndex() < mRobustnessCriteria.getMinimumRobustnessIndex())
	return false;

      if (getProfitabilityIndex() < DecimalConstants<Decimal>::TwoThirds)
	return false;

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

      ProfitTargetStopPair<Decimal> pairKey (originalTarget,
					  originalStop);
      /*
      Decimal requiredProfitability = 
	RobustnessCalculator<Decimal>::requiredPALProfitability(mRobustnessCriteria.getDesiredProfitFactor(),
						mPatternToTest->getPayoffRatio(),
							     mRobustnessCriteria.getProfitabilitySafetyFactor());
      */

      Decimal requiredProfitability = mRequiredProfitability;
      if (mDebug)
	std::cout << "!!@@@ Required profitability = " << requiredProfitability << std::endl;
      typename RobustnessCalculator<Decimal>::RobustnessTestResultIterator refPairIterator =
	mRobustnessResults.find(pairKey);

      if (refPairIterator == endRobustnessTestResults())
	{
	  throw RobustnessCalculatorException("isRobust: could not find results for reference pattern with stop = " +toString(originalStop));
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
					  i))
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
					  i))
	    {
	      if (mDebug)
		std::cout << "Failed testing below reference value" << std::endl;

	      if (mDebug)
		std::cout << "Returning false for testing below reference value" << std::endl;
	      return false;
	    }
	}

      uint32_t numFailuresAtBeginning = getNumRobustnessFailuresAtBeginning (requiredProfitability);
      uint32_t numFailuresAtEnd = getNumRobustnessFailuresAtEnd (requiredProfitability);

      if ((numFailuresAtBeginning == mPermutationAttributes->numEntriesToTestAtBeginning()) &&
	  (numFailuresAtEnd == mPermutationAttributes->numEntriesToTestAtEnd()))
	{
	  return false;
	}

      return true;

    }

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

    unsigned long getNumEntries() const
    {
      return mRobustnessResults.size();
    }

    static Decimal requiredPALProfitability(const Decimal& profitFactor,
						const Decimal& payoffRatio,
						const Decimal& safetyFactor)
    {
      Decimal denom = profitFactor + (safetyFactor * payoffRatio);

      return ((profitFactor / denom) * DecimalConstants<Decimal>::DecimalOneHundred);
    }

  private:
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
      Decimal numPermutations = 
	decimal_cast<Decimal::decimal_points>(mPermutationAttributes->getNumberOfPermutations());
      Decimal numSignificant7 = decimal_cast<Decimal::decimal_points>(7);

      Decimal numSignificant (numPermutations * RobustnessCalculator<Decimal>::TwentyFivePercent);
      if (mDebug)
	std::cout << "getNumNeighboringSignificantResults: numPermutations = " << numPermutations << std::endl;
 
      if (mPermutationAttributes->getNumberOfPermutations() == 30)
	numSignificant = numSignificant7;

      return (unsigned long) numSignificant.getAsInteger();
    }

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
  };

  template <class Decimal> Decimal 
  RobustnessCalculator<Decimal>::TwentyPercent(DecimalConstants<Decimal>::createDecimal("0.20"));

  template <class Decimal> Decimal 
  RobustnessCalculator<Decimal>::TwentyFivePercent(DecimalConstants<Decimal>::createDecimal("0.25"));

  //
  // class RobustnessTest
  //
  // Performs a robustness test of a PriceActionLab pattern
  //
  template <class Decimal> class RobustnessTest
  {
  public:
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

    // Returns boolean value indicating whether or not the PalStrategy is robust
    bool runRobustnessTest()
    {
      // std::cout << "RobustnessTest::runRobustnessTest" << std::endl;

      std::shared_ptr<PriceActionLabPattern> originalPattern = mTheStrategy->getPalPattern();
      Decimal originalPatternStop (originalPattern->getStopLossAsDecimal());
      Decimal permutationIncrement(originalPatternStop / decimal_cast<Decimal::decimal_points>((unsigned int) mPermutationAttributes->getPermutationsDivisor()));
      Decimal numProfitablePermutations(DecimalConstants<Decimal>::DecimalZero);
      Decimal requiredPayoffRatio (originalPattern->getPayoffRatio());
      Decimal requiredProfitability (RobustnessCalculator<Decimal>::requiredPALProfitability (mRobustnessCriteria.getDesiredProfitFactor(), requiredPayoffRatio, mRobustnessCriteria.getProfitabilitySafetyFactor())); 
      bool originalPatternProfitabilityFailed = false;

      //std::cout << "RobustnessTest::runRobustnessTest - finished initializing variables" << std::endl;
      // Test original profit target, stop pair

      std::shared_ptr<BackTester<Decimal>> clonedBackTester = mTheBacktester->clone();
      clonedBackTester->addStrategy(mTheStrategy);
      clonedBackTester->backtest();

      // The original pattern that we got from PAL should pass the minimum
      // required profitability
      //if (clonedBackTester->getClosedPositionHistory().getPALProfitability() < 
      //	  requiredProfitability)
      //	{
      //	  originalPatternProfitabilityFailed = true;
      //	}
      
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

    const RobustnessCalculator<Decimal>& getRobustnessCalculator() const
    {
      return mRobustnessQuality;
    }

  private: 
    void backTestNewPermutation (std::shared_ptr<PriceActionLabPattern> aPattern,
				 const Decimal& newStopLoss, 
				 const Decimal& newProfitTarget)
    {
      decimal7 *newStopLossPtr = mAstFactory->getDecimalNumber ((char *) toString(newStopLoss).c_str());
      decimal7 *newProfitTargetPtr = mAstFactory->getDecimalNumber ((char *)toString(newProfitTarget).c_str());
      
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

  //
  // class RobustnessTestMonteCarlo
  //
  // Performs a robustness test of a PriceActionLab pattern
  //
  template <class Decimal> class RobustnessTestMonteCarlo
  {
  public:
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

    // Returns boolean value indicating whether or not the PalStrategy is robust
    bool runRobustnessTest()
    {
      std::shared_ptr<PriceActionLabPattern> originalPattern = mTheStrategy->getPalPattern();
      Decimal originalPatternStop (originalPattern->getStopLossAsDecimal());
      Decimal permutationIncrement(originalPatternStop / decimal_cast<Decimal::decimal_points>((unsigned int) mPermutationAttributes->getPermutationsDivisor()));
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

    const RobustnessCalculator<Decimal>& getRobustnessCalculator() const
    {
      return mRobustnessQuality;
    }

  private: 
    void backTestNewPermutation (std::shared_ptr<PriceActionLabPattern> aPattern,
				 const Decimal& newStopLoss, 
				 const Decimal& newProfitTarget)
    {
      decimal7 *newStopLossPtr = mAstFactory->getDecimalNumber ((char *) toString(newStopLoss).c_str());
      decimal7 *newProfitTargetPtr = mAstFactory->getDecimalNumber ((char *)toString(newProfitTarget).c_str());
      
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
