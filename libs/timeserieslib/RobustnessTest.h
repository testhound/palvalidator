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

  template <int Prec> class PatternRobustnessCriteria
    {
    public:
      PatternRobustnessCriteria (const decimal<Prec>& minRobustnessIndex,
				 const decimal<Prec>& desiredProfitFactor,
				 const PercentNumber<Prec>& tolerance,
				 const decimal<Prec>& profitabilitySafetyFactor)
	: mMinRobustnessIndex (minRobustnessIndex),
	  mDesiredProfitFactor(desiredProfitFactor),
	  mRobustnessTolerance(tolerance),
	  mProfitabilitySafetyFactor(profitabilitySafetyFactor)
	{}

      PatternRobustnessCriteria (const PatternRobustnessCriteria<Prec>& rhs)
	: mMinRobustnessIndex (rhs.mMinRobustnessIndex),
	  mDesiredProfitFactor(rhs.mDesiredProfitFactor),
	  mRobustnessTolerance(rhs.mRobustnessTolerance),
	  mProfitabilitySafetyFactor(rhs.mProfitabilitySafetyFactor)
      {}

      PatternRobustnessCriteria<Prec>& 
      operator=(const PatternRobustnessCriteria<Prec> &rhs)
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

      const decimal<Prec>& getMinimumRobustnessIndex() const
      {
	return mMinRobustnessIndex;
      }

      const decimal<Prec>& getDesiredProfitFactor() const
      {
	return mDesiredProfitFactor;
      }

      const PercentNumber<Prec>& getRobustnessTolerance() const
      {
	return mRobustnessTolerance;
      }

      // Return the tolerance in percent for the number of iterations away we
      // are for from the original robustness target

      const PercentNumber<Prec> getToleranceForIterations (unsigned long numIterations) const
      {
	static PercentNumber<Prec> sqrtConstants[] =
	  {
	    // Note entry 0 was manually modified because we would like a 1%
	    // tolerance on the reference value
	    
	    PercentNumber<Prec>::createPercentNumber (std::string("1.000000")),
	    // Note entries 1 to 3 were manually modified because we don't want
	    // tolerances less than 2% for these entries

	    PercentNumber<Prec>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.236068")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.449490")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.645751")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.828427")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.162278")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.316625")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.464102")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.605551")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.741657")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.872983")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.123106")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.242641")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.358899")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.472136")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.582576")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.690416")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.795832")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.898979")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.099020")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.196152")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.291503")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.385165")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.477226")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.567764")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.656854")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.744563")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.830952")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.916080")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.082763")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.164414")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.244998")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.324555")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.403124")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.480741")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.557439")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.633250")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.708204")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.782330")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.855655")),
	    PercentNumber<Prec>::createPercentNumber (std::string("6.928203")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.071068")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.141428")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.211103")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.280110")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.348469")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.416198")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.483315")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.549834")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.615773")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.681146")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.745967")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.810250")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.874008")),
	    PercentNumber<Prec>::createPercentNumber (std::string("7.937254")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.062258")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.124038")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.185353")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.246211")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.306624")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.366600")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.426150")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.485281")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.544004")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.602325")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.660254")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.717798")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.774964")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.831761")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.888194")),
	    PercentNumber<Prec>::createPercentNumber (std::string("8.944272")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.055385")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.110434")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.165151")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.219544")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.273618")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.327379")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.380832")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.433981")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.486833")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.539392")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.591663")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.643651")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.695360")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.746794")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.797959")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.848858")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.899495")),
	    PercentNumber<Prec>::createPercentNumber (std::string("9.949874")),
	    PercentNumber<Prec>::createPercentNumber (std::string("10.000000")),
	  };

	if ((numIterations >=0) && (numIterations <= 100))
	  return sqrtConstants[numIterations];

	// We don't want tolerance greater than 10%
	return sqrtConstants[100];
      }
      
      const PercentNumber<Prec> getToleranceForNumTrades (unsigned long numTrades) const
      {
	static PercentNumber<Prec> halfSqrtConstants[] =
	  {	
	    PercentNumber<Prec>::createPercentNumber (std::string("0.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("0.500000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("0.707107")),
	    PercentNumber<Prec>::createPercentNumber (std::string("0.866025")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.118034")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.224745")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.322876")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.414214")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.500000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.581139")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.658312")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.732051")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.802776")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.870829")),
	    PercentNumber<Prec>::createPercentNumber (std::string("1.936492")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.061553")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.121320")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.179449")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.236068")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.291288")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.345208")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.397916")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.449490")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.500000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.549510")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.598076")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.645751")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.692582")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.738613")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.783882")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.828427")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.872281")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.915476")),
	    PercentNumber<Prec>::createPercentNumber (std::string("2.958040")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.041381")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.082207")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.122499")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.162278")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.201562")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.240370")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.278719")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.316625")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.354102")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.391165")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.427827")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.464102")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.500000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.535534")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.570714")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.605551")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.640055")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.674235")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.708099")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.741657")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.774917")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.807887")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.840573")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.872983")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.905125")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.937004")),
	    PercentNumber<Prec>::createPercentNumber (std::string("3.968627")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.000000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.031129")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.062019")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.092676")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.123106")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.153312")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.183300")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.213075")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.242641")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.272002")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.301163")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.330127")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.358899")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.387482")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.415880")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.444097")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.472136")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.500000")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.527693")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.555217")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.582576")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.609772")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.636809")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.663690")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.690416")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.716991")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.743416")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.769696")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.795832")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.821825")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.847680")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.873397")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.898979")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.924429")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.949747")),
	    PercentNumber<Prec>::createPercentNumber (std::string("4.974937")),
	    PercentNumber<Prec>::createPercentNumber (std::string("5.000000"))
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

      const decimal<Prec>& getProfitabilitySafetyFactor() const
      {
	return mProfitabilitySafetyFactor;
      }

    private:
      decimal<Prec> mMinRobustnessIndex;
      decimal<Prec> mDesiredProfitFactor;
      PercentNumber<Prec> mRobustnessTolerance;
      decimal<Prec> mProfitabilitySafetyFactor;
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

  template <int Prec> class ProfitTargetStopPair
  {
  public:
    ProfitTargetStopPair (const decimal<Prec>& profitTarget,
			  const decimal<Prec>& stop)
      :  mProfitTarget(profitTarget),
	 mStop(stop)
    {}

    ProfitTargetStopPair(const ProfitTargetStopPair<Prec>& rhs)
      : mProfitTarget(rhs.mProfitTarget),
	mStop(rhs.mStop)
    {}

    ProfitTargetStopPair<Prec>& 
    operator=(const ProfitTargetStopPair<Prec> &rhs)
    {
      if (this == &rhs)
	return *this;

      mProfitTarget = rhs.mProfitTarget;
      mStop = rhs.mStop;

      return *this;
    }

    ~ProfitTargetStopPair()
    {}

    const decimal<Prec>& getProfitTarget() const
    {
      return mProfitTarget;
    }

    const decimal<Prec>& getProtectiveStop() const
    {
      return mStop;
    }
    
  private:
    decimal<Prec> mProfitTarget;
    decimal<Prec> mStop;
  };

  template <int Prec> class RobustnessTestResult
  {
  public:
    RobustnessTestResult (const decimal<Prec>& winRate,
			  const decimal<Prec>& profitFactor,
			  unsigned long numTrades,
			  const decimal<Prec>& payOffRatio,
			  const decimal<Prec>& medianPayOffRatio,
			  const decimal<Prec>& expectation)
      : mWinRate(winRate),
	mProfitFactor(profitFactor),
	mNumTrades(numTrades),
	mPayOffRatio(payOffRatio),
	mMedianPayOffRatio(medianPayOffRatio),
	mExpectation(expectation),
	mMonteCarloPayoffRatio(DecimalConstants<Prec>::DecimalZero)
    {}

    RobustnessTestResult (const decimal<Prec>& winRate,
			  const decimal<Prec>& profitFactor,
			  unsigned long numTrades,
			  const decimal<Prec>& payOffRatio,
			  const decimal<Prec>& medianPayOffRatio,
			  const decimal<Prec>& expectation,
			  const decimal<Prec>& monteCarloPayoff)
      : mWinRate(winRate),
	mProfitFactor(profitFactor),
	mNumTrades(numTrades),
	mPayOffRatio(payOffRatio),
	mMedianPayOffRatio(medianPayOffRatio),
	mExpectation(expectation),
	mMonteCarloPayoffRatio(monteCarloPayoff)
    {}

     RobustnessTestResult(const  RobustnessTestResult<Prec>& rhs)
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

    RobustnessTestResult<Prec>& 
    operator=(const RobustnessTestResult<Prec> &rhs)
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

    const decimal<Prec>& getPALProfitability() const
    {
      return mWinRate;
    }

    const decimal<Prec> getMonteCarloProfitability() const
    {
      decimal<Prec> pf(getProfitFactor());
      decimal <Prec> denominator(pf + getMonteCarloPayOffRatio());

      if (denominator > DecimalConstants<Prec>::DecimalZero)
	return ((pf/denominator) * DecimalConstants<Prec>::DecimalOneHundred);
      else
	return (DecimalConstants<Prec>::DecimalZero);
    }
    
    const decimal<Prec>& getProfitFactor() const
    {
      return mProfitFactor;
    }

    unsigned long getNumTrades() const
    {
      return mNumTrades;
    }

    const decimal<Prec>& getPayOffRatio() const
    {
      return mPayOffRatio;
    }

    const decimal<Prec>& getMedianPayOffRatio() const
    {
      return mMedianPayOffRatio;
    }

    const decimal<Prec>& getMonteCarloPayOffRatio() const
    {
      return mMonteCarloPayoffRatio;;
    }

    const decimal<Prec>& getRMultipleExpectancy() const
    {
      return mExpectation;
    }

  private:
    decimal<Prec> mWinRate;
    decimal<Prec> mProfitFactor;
    unsigned long mNumTrades;
    decimal<Prec> mPayOffRatio;
    decimal<Prec> mMedianPayOffRatio;
    decimal<Prec> mExpectation;
    decimal<Prec> mMonteCarloPayoffRatio;
  };

  //
  // class ProfitStopComparator
  //

  template <int Prec> struct ProfitTargetStopComparator
  {
    bool operator() (const ProfitTargetStopPair<Prec>& lhs,
		     const ProfitTargetStopPair<Prec>& rhs) const
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

  template <int Prec> class RobustnessCalculator
  {
  public:
    typedef typename std::map<ProfitTargetStopPair<Prec>, 
			      std::shared_ptr<RobustnessTestResult<Prec>>,
			      ProfitTargetStopComparator<Prec>>::const_iterator RobustnessTestResultIterator;

  public:
    RobustnessCalculator(shared_ptr<PriceActionLabPattern> thePattern,
			 shared_ptr<RobustnessPermutationAttributes> permutationAttributes,
			 const PatternRobustnessCriteria<Prec>& robustnessCriteria,
			 bool debug = false)
      : mPatternToTest (thePattern),
	mPermutationAttributes (permutationAttributes),
	mRobustnessCriteria(robustnessCriteria),
	mNumberProfitableResults(0),
	mDebug(debug),
	mRobustnessResults(),
	mRequiredProfitability(RobustnessCalculator<Prec>::requiredPALProfitability(robustnessCriteria.getDesiredProfitFactor(),
										    thePattern->getPayoffRatio(),
										    robustnessCriteria.getProfitabilitySafetyFactor())),
	mNumberPALProfitableResults(0)
      {}

    RobustnessCalculator(const RobustnessCalculator<Prec>& rhs)
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

    RobustnessCalculator<Prec>& 
    operator=(const RobustnessCalculator<Prec> &rhs)
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

    void addTestResult (std::shared_ptr<RobustnessTestResult<Prec>> testResult,
			std::shared_ptr<PriceActionLabPattern> pattern)
    {
      ProfitTargetStopPair<Prec> pairKey (pattern->getProfitTargetAsDecimal(),
					  pattern->getStopLossAsDecimal());
      
      if (testResult->getProfitFactor() > DecimalConstants<Prec>::DecimalOne)
	mNumberProfitableResults = mNumberProfitableResults + DecimalConstants<Prec>::DecimalOne;

      if (equalWithTolerance (mRequiredProfitability, testResult->getMonteCarloProfitability(),
			       mRobustnessCriteria.getRobustnessTolerance()))
	mNumberPALProfitableResults = mNumberPALProfitableResults + DecimalConstants<Prec>::DecimalOne;

      if (mRobustnessResults.find (pairKey) == endRobustnessTestResults())
	mRobustnessResults.insert (std::make_pair (pairKey, testResult));
      else
	throw RobustnessCalculatorException ("RobustnessCalculator::addTestResult - stop already exists");
    }

    RobustnessCalculator<Prec>::RobustnessTestResultIterator beginRobustnessTestResults() const
    {
      return  mRobustnessResults.begin();
    }

    RobustnessCalculator<Prec>::RobustnessTestResultIterator endRobustnessTestResults() const
    {
      return  mRobustnessResults.end();
    }

    decimal<Prec> getRobustnessIndex() const
    {
      unsigned long numEntries = getNumEntries();
      decimal<Prec> numEntriesDecimal = decimal_cast<Prec>((unsigned int) numEntries);

      if (numEntries > 0)
	return (mNumberProfitableResults / numEntriesDecimal) * DecimalConstants<Prec>::DecimalOneHundred;
      else
	return DecimalConstants<Prec>::DecimalZero;
    }

    decimal<Prec> getProfitabilityIndex() const
    {
      unsigned long numEntries = getNumEntries();
      decimal<Prec> numEntriesDecimal = decimal_cast<Prec>((unsigned int) numEntries);

      if (numEntries > 0)
	return (mNumberPALProfitableResults / numEntriesDecimal) * DecimalConstants<Prec>::DecimalOneHundred;
      else
	return DecimalConstants<Prec>::DecimalZero;
    }

    bool isRobust() const
    {
      if (getNumEntries() < mPermutationAttributes->getNumberOfPermutations())
	throw RobustnessCalculatorException ("RobustnessCalculator::isRobust - number of entries does not match number of permutations specified in PermutationAttributes");

      if (getRobustnessIndex() < mRobustnessCriteria.getMinimumRobustnessIndex())
	return false;

      if (getProfitabilityIndex() < DecimalConstants<Prec>::TwoThirds)
	return false;

      unsigned long numSignificant = getNumNeighboringSignificantResults();
      if (mDebug)
	std::cout << "!! Num Significant neighboring results = " << numSignificant << std::endl;

      decimal<Prec> originalStop = getOriginalPatternStop();
      decimal<Prec> originalTarget = getOriginalPatternTarget();

      if (mDebug)
	{
	  std::cout << "RobustnessCalculator::isRobust - original pattern stop = " << originalStop << std::endl;
	  std::cout << "RobustnessCalculator::isRobust - original pattern target = " << originalTarget << std::endl;
	}

      ProfitTargetStopPair<Prec> pairKey (originalTarget,
					  originalStop);
      /*
      decimal<Prec> requiredProfitability = 
	RobustnessCalculator<Prec>::requiredPALProfitability(mRobustnessCriteria.getDesiredProfitFactor(),
						mPatternToTest->getPayoffRatio(),
							     mRobustnessCriteria.getProfitabilitySafetyFactor());
      */

      decimal<Prec> requiredProfitability = mRequiredProfitability;
      if (mDebug)
	std::cout << "!!@@@ Required profitability = " << requiredProfitability << std::endl;
      typename RobustnessCalculator<Prec>::RobustnessTestResultIterator refPairIterator =
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
      typename RobustnessCalculator<Prec>::RobustnessTestResultIterator it = refPairIterator;

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

    uint32_t getNumRobustnessFailuresAtBeginning(decimal<Prec> theRequiredProfitability) const
    {
      typename RobustnessCalculator<Prec>::RobustnessTestResultIterator it =
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

    uint32_t getNumRobustnessFailuresAtEnd(decimal<Prec> theRequiredProfitability) const
    {
      typename RobustnessCalculator<Prec>::RobustnessTestResultIterator it =
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

    static dec::decimal<Prec> requiredPALProfitability(const decimal<Prec>& profitFactor,
						const decimal<Prec>& payoffRatio,
						const decimal<Prec>& safetyFactor)
    {
      decimal<Prec> denom = profitFactor + (safetyFactor * payoffRatio);

      return ((profitFactor / denom) * DecimalConstants<Prec>::DecimalOneHundred);
    }

  private:
    // Note this method is meant to be called on the results that 'neighbor' the original 
    // profit target, stop pair

    bool isPermutationResultRobust (std::shared_ptr<RobustnessTestResult<Prec>> result,
				    const decimal<Prec>& requiredProfitability,
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

      decimal<Prec> resultPayoff (result->getMonteCarloPayOffRatio());
      if (resultPayoff == DecimalConstants<Prec>::DecimalZero)
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

    const decimal<Prec> getOriginalPatternStop() const
    {
      return mPatternToTest->getStopLossAsDecimal();
    }

    const decimal<Prec> getOriginalPatternTarget() const
    {
      return mPatternToTest->getProfitTargetAsDecimal();
    }

    unsigned long getNumNeighboringSignificantResults() const
    {
      decimal<Prec> numPermutations = 
	decimal_cast<Prec>(mPermutationAttributes->getNumberOfPermutations());
      decimal<Prec> numSignificant7 = decimal_cast<Prec>(7);

      decimal<Prec> numSignificant (numPermutations * RobustnessCalculator<Prec>::TwentyFivePercent);
      if (mDebug)
	std::cout << "getNumNeighboringSignificantResults: numPermutations = " << numPermutations << std::endl;
 
      if (mPermutationAttributes->getNumberOfPermutations() == 30)
	numSignificant = numSignificant7;

      return (unsigned long) numSignificant.getAsInteger();
    }

    bool equalWithTolerance (const decimal<Prec>& referenceValue, 
			     const decimal<Prec>& comparisonValue,
			     const PercentNumber<Prec>& tolerance) const
    {
      decimal<Prec> lowerRefValue(referenceValue - (tolerance.getAsPercent() * referenceValue));
      return (comparisonValue >= lowerRefValue);
    }

  private:
    std::shared_ptr<PriceActionLabPattern> mPatternToTest;
    std::shared_ptr<RobustnessPermutationAttributes> mPermutationAttributes;
    PatternRobustnessCriteria<Prec> mRobustnessCriteria;
    decimal<Prec> mNumberProfitableResults;
    bool mDebug;
    std::map<ProfitTargetStopPair<Prec>, 
	     std::shared_ptr<RobustnessTestResult<Prec>>,
	     ProfitTargetStopComparator<Prec>> mRobustnessResults;
    decimal<Prec> mRequiredProfitability;
    decimal<Prec> mNumberPALProfitableResults;
    static decimal<Prec> TwentyPercent;
    static decimal<Prec> TwentyFivePercent;
  };

  template <int Prec> decimal<Prec> 
  RobustnessCalculator<Prec>::TwentyPercent(DecimalConstants<Prec>::createDecimal("0.20"));

  template <int Prec> decimal<Prec> 
  RobustnessCalculator<Prec>::TwentyFivePercent(DecimalConstants<Prec>::createDecimal("0.25"));

  //
  // class RobustnessTest
  //
  // Performs a robustness test of a PriceActionLab pattern
  //
  template <int Prec> class RobustnessTest
  {
  public:
    RobustnessTest (std::shared_ptr<BackTester<Prec>> backtester,
		    std::shared_ptr<PalStrategy<Prec>> aPalStrategy,
		    shared_ptr<RobustnessPermutationAttributes> permutationAttributes,
		    const PatternRobustnessCriteria<Prec>& robustnessCriteria,
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

    RobustnessTest (const RobustnessTest<Prec>& rhs)
      : mTheBacktester(rhs.mTheBacktester),
	mTheStrategy(rhs.mTheStrategy),
	mPermutationAttributes(rhs.mPermutationAttributes),
	mRobustnessCriteria(rhs.mRobustnessCriteria),
	mAstFactory(rhs.mAstFactory),
	mRobustnessQuality(rhs.mRobustnessQuality)
    {}

    const RobustnessTest<Prec>&
    operator=(const RobustnessTest<Prec>& rhs)
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
      dec::decimal<Prec> originalPatternStop (originalPattern->getStopLossAsDecimal());
      dec::decimal<Prec> permutationIncrement(originalPatternStop / decimal_cast<Prec>((unsigned int) mPermutationAttributes->getPermutationsDivisor()));
      dec::decimal<Prec> numProfitablePermutations(DecimalConstants<Prec>::DecimalZero);
      decimal<Prec> requiredPayoffRatio (originalPattern->getPayoffRatio());
      decimal<Prec> requiredProfitability (RobustnessCalculator<Prec>::requiredPALProfitability (mRobustnessCriteria.getDesiredProfitFactor(), requiredPayoffRatio, mRobustnessCriteria.getProfitabilitySafetyFactor())); 
      bool originalPatternProfitabilityFailed = false;

      //std::cout << "RobustnessTest::runRobustnessTest - finished initializing variables" << std::endl;
      // Test original profit target, stop pair

      std::shared_ptr<BackTester<Prec>> clonedBackTester = mTheBacktester->clone();
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

      decimal<Prec> lowestStopToTest(calculateLowestStopPermutationValue (originalPatternStop, 
									   permutationIncrement));
      decimal<Prec> stopToTest(lowestStopToTest);
      decimal<Prec> profitTargetToTest(stopToTest * requiredPayoffRatio);

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

    const RobustnessCalculator<Prec>& getRobustnessCalculator() const
    {
      return mRobustnessQuality;
    }

  private: 
    void backTestNewPermutation (std::shared_ptr<PriceActionLabPattern> aPattern,
				 const decimal<Prec>& newStopLoss, 
				 const decimal<Prec>& newProfitTarget)
    {
      decimal7 *newStopLossPtr = mAstFactory->getDecimalNumber ((char *) toString(newStopLoss).c_str());
      decimal7 *newProfitTargetPtr = mAstFactory->getDecimalNumber ((char *)toString(newProfitTarget).c_str());
      
      std::shared_ptr<BackTester<Prec>> clonedBackTester = mTheBacktester->clone();
	 

      ProfitTargetInPercentExpression *profitTarget;
      StopLossInPercentExpression *stopLoss;
      std::shared_ptr<PriceActionLabPattern> clonedPattern;
      std::shared_ptr<PalLongStrategy<Prec>> longStrategy;
      std::shared_ptr<PalShortStrategy<Prec>> shortStrategy;

      if (aPattern->isLongPattern())
	{
	  profitTarget = mAstFactory->getLongProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getLongStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  longStrategy = std::make_shared<PalLongStrategy<Prec>>(mTheStrategy->getStrategyName(),
								 clonedPattern,
								 mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(longStrategy);

	}
      else
	{
	  profitTarget = mAstFactory->getShortProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getShortStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  shortStrategy = std::make_shared<PalShortStrategy<Prec>>(mTheStrategy->getStrategyName(),
								   clonedPattern,
								   mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(shortStrategy);
	}

      clonedBackTester->backtest();
      addTestResult (createRobustnessTestResult (clonedBackTester),
		     clonedPattern);

      
    }

    decimal<Prec> calculateLowestStopPermutationValue (const decimal<Prec>& originalStop,
						       const decimal<Prec>& permutationIncrement) const
    {
      return (originalStop - 
	      (permutationIncrement *  decimal<Prec> (mPermutationAttributes->getNumPermutationsBelowRef())));
    }

    std::shared_ptr<RobustnessTestResult<Prec>> 
    createRobustnessTestResult(std::shared_ptr<BackTester<Prec>> backtester)
    {
      //      ClosedPositionHistory<Prec> closedPositions = 
      //getClosedPositionHistory (backtester);

      ClosedPositionHistory<Prec> closedPositions = backtester->getClosedPositionHistory();

      return 
	make_shared<RobustnessTestResult<Prec>> (closedPositions.getMedianPALProfitability(),
						 closedPositions.getProfitFactor(),
						 closedPositions.getNumPositions(),
						 closedPositions.getPayoffRatio(),
						 closedPositions.getMedianPayoffRatio(),
						 closedPositions.getRMultipleExpectancy());
    }

    void addTestResult (std::shared_ptr<RobustnessTestResult<Prec>> testResult,
			std::shared_ptr<PriceActionLabPattern> pattern)
    {
      mRobustnessQuality.addTestResult (testResult, pattern);
    }

  private:
    std::shared_ptr<BackTester<Prec>> mTheBacktester;
    std::shared_ptr<PalStrategy<Prec>> mTheStrategy;
    shared_ptr<RobustnessPermutationAttributes> mPermutationAttributes;
    PatternRobustnessCriteria<Prec> mRobustnessCriteria;
    std::shared_ptr<AstFactory> mAstFactory;
    RobustnessCalculator<Prec> mRobustnessQuality;
  };


  /////////////////////////

  //
  // class RobustnessTestMonteCarlo
  //
  // Performs a robustness test of a PriceActionLab pattern
  //
  template <int Prec> class RobustnessTestMonteCarlo
  {
  public:
    RobustnessTestMonteCarlo (std::shared_ptr<BackTester<Prec>> backtester,
			      std::shared_ptr<PalStrategy<Prec>> aPalStrategy,
			      shared_ptr<RobustnessPermutationAttributes> permutationAttributes,
			      const PatternRobustnessCriteria<Prec>& robustnessCriteria,
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

    RobustnessTestMonteCarlo (const RobustnessTestMonteCarlo<Prec>& rhs)
      : mTheBacktester(rhs.mTheBacktester),
	mTheStrategy(rhs.mTheStrategy),
	mPermutationAttributes(rhs.mPermutationAttributes),
	mRobustnessCriteria(rhs.mRobustnessCriteria),
	mAstFactory(rhs.mAstFactory),
	mRobustnessQuality(rhs.mRobustnessQuality)
    {}

    const RobustnessTestMonteCarlo<Prec>&
    operator=(const RobustnessTestMonteCarlo<Prec>& rhs)
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
      dec::decimal<Prec> originalPatternStop (originalPattern->getStopLossAsDecimal());
      dec::decimal<Prec> permutationIncrement(originalPatternStop / decimal_cast<Prec>((unsigned int) mPermutationAttributes->getPermutationsDivisor()));
      dec::decimal<Prec> numProfitablePermutations(DecimalConstants<Prec>::DecimalZero);
      decimal<Prec> requiredPayoffRatio (originalPattern->getPayoffRatio());
      decimal<Prec> requiredProfitability (RobustnessCalculator<Prec>::requiredPALProfitability (mRobustnessCriteria.getDesiredProfitFactor(), requiredPayoffRatio, mRobustnessCriteria.getProfitabilitySafetyFactor())); 
      std::shared_ptr<BackTester<Prec>> clonedBackTester = mTheBacktester->clone();
      clonedBackTester->addStrategy(mTheStrategy);
      clonedBackTester->backtest();

      addTestResult (clonedBackTester, originalPattern);

      decimal<Prec> lowestStopToTest(calculateLowestStopPermutationValue (originalPatternStop, 
									   permutationIncrement));
      decimal<Prec> stopToTest(lowestStopToTest);
      decimal<Prec> profitTargetToTest(stopToTest * requiredPayoffRatio);

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

    const RobustnessCalculator<Prec>& getRobustnessCalculator() const
    {
      return mRobustnessQuality;
    }

  private: 
    void backTestNewPermutation (std::shared_ptr<PriceActionLabPattern> aPattern,
				 const decimal<Prec>& newStopLoss, 
				 const decimal<Prec>& newProfitTarget)
    {
      decimal7 *newStopLossPtr = mAstFactory->getDecimalNumber ((char *) toString(newStopLoss).c_str());
      decimal7 *newProfitTargetPtr = mAstFactory->getDecimalNumber ((char *)toString(newProfitTarget).c_str());
      
      std::shared_ptr<BackTester<Prec>> clonedBackTester = mTheBacktester->clone();
	 

      ProfitTargetInPercentExpression *profitTarget;
      StopLossInPercentExpression *stopLoss;
      std::shared_ptr<PriceActionLabPattern> clonedPattern;
      std::shared_ptr<PalLongStrategy<Prec>> longStrategy;
      std::shared_ptr<PalShortStrategy<Prec>> shortStrategy;

      if (aPattern->isLongPattern())
	{
	  profitTarget = mAstFactory->getLongProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getLongStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  longStrategy = std::make_shared<PalLongStrategy<Prec>>(mTheStrategy->getStrategyName(),
								 clonedPattern,
								 mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(longStrategy);

	}
      else
	{
	  profitTarget = mAstFactory->getShortProfitTarget (newProfitTargetPtr);
	  stopLoss = mAstFactory->getShortStopLoss (newStopLossPtr);
	  clonedPattern = aPattern->clone (profitTarget, stopLoss);
	  shortStrategy = std::make_shared<PalShortStrategy<Prec>>(mTheStrategy->getStrategyName(),
								   clonedPattern,
								   mTheStrategy->getPortfolio());
	  clonedBackTester->addStrategy(shortStrategy);
	}

      clonedBackTester->backtest();
      addTestResult (clonedBackTester, clonedPattern);
    }

    decimal<Prec> calculateLowestStopPermutationValue (const decimal<Prec>& originalStop,
						       const decimal<Prec>& permutationIncrement) const
    {
      return (originalStop - 
	      (permutationIncrement *  decimal<Prec> (mPermutationAttributes->getNumPermutationsBelowRef())));
    }

    void addTestResult (std::shared_ptr<BackTester<Prec>> backtester,
			std::shared_ptr<PriceActionLabPattern> pattern)
    {
      ClosedPositionHistory<Prec> closedPositions = backtester->getClosedPositionHistory();

      decimal<Prec> profitability = closedPositions.getMedianPALProfitability();
      decimal<Prec> profitFactor = closedPositions.getProfitFactor();
      unsigned long numTrades = closedPositions.getNumPositions();
      decimal<Prec> payoffratio = closedPositions.getPayoffRatio();
      decimal<Prec> medianPayoff = closedPositions.getMedianPayoffRatio();
      decimal<Prec> expectancy = closedPositions.getRMultipleExpectancy();

      // Use Monte Carlo to (hopefully) get a better estimate of the payoff ratio
      //std::cout << "Running MonteCarloPayoffRatio for profit target " <<
      //pattern->getProfitTargetAsDecimal() << ", Stop = " << pattern->getStopLossAsDecimal() << std::endl << std::endl;
      
      MonteCarloPayoffRatio<Prec> monteCarloPayoffCalculator (backtester, 200);
      decimal<Prec> monteCarloPayoff(monteCarloPayoffCalculator.runPermutationTest());

      std::shared_ptr<RobustnessTestResult<Prec>> testResult =  
	make_shared<RobustnessTestResult<Prec>> (profitability,
						 profitFactor,
						 numTrades,
						 payoffratio,
						 medianPayoff,
						 expectancy,
						 monteCarloPayoff);

      mRobustnessQuality.addTestResult (testResult, pattern);
    }

  private:
    std::shared_ptr<BackTester<Prec>> mTheBacktester;
    std::shared_ptr<PalStrategy<Prec>> mTheStrategy;
    shared_ptr<RobustnessPermutationAttributes> mPermutationAttributes;
    PatternRobustnessCriteria<Prec> mRobustnessCriteria;
    std::shared_ptr<AstFactory> mAstFactory;
    RobustnessCalculator<Prec> mRobustnessQuality;
  };
}

#endif
