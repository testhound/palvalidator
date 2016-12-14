// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __DECIMAL_CONSTANT_H
#define __DECIMAL_CONSTANT_H 1

#include <string>

namespace mkc_timeseries
{
  template <class Decimal>
  class DecimalConstants
    {
    public:
      static Decimal DecimalZero;
      static Decimal DecimalOneHundred;
      static Decimal DecimalOne;
      static Decimal DecimalMinusOne;
      static Decimal DecimalTwo;
      static Decimal DecimalMinusTwo;
      static Decimal EquityTick;
      static Decimal SignificantPValue;
      static Decimal TwoThirds;
      static Decimal createDecimal (const std::string& valueString)
      {
	return num::fromString<Decimal>(valueString);
      }
    };

  template <class Decimal> Decimal DecimalConstants<Decimal>::DecimalZero(0.0);
  template <class Decimal> Decimal 
    DecimalConstants<Decimal>::DecimalOneHundred(DecimalConstants<Decimal>::createDecimal("100.0"));
  template <class Decimal> Decimal 
    DecimalConstants<Decimal>::DecimalTwo(DecimalConstants<Decimal>::createDecimal("2.0"));
  template <class Decimal> Decimal 
    DecimalConstants<Decimal>::DecimalMinusTwo(DecimalConstants<Decimal>::createDecimal("-2.0"));

  template <class Decimal> Decimal DecimalConstants<Decimal>::DecimalOne(1.0);
  template <class Decimal> Decimal 
    DecimalConstants<Decimal>::DecimalMinusOne(DecimalConstants<Decimal>::createDecimal("-1.0"));
  template <class Decimal> Decimal DecimalConstants<Decimal>::EquityTick(0.01);
  template <class Decimal> Decimal DecimalConstants<Decimal>::SignificantPValue(0.045);


  template <class Decimal>
  Decimal
  createADecimal(const std::string& numString)
  {
    return DecimalConstants<Decimal>::createDecimal(numString);
  }

  template <class Decimal> Decimal DecimalConstants<Decimal>::TwoThirds(createADecimal<Decimal>("66.6666667"));

  template <class Decimal>
  class DecimalSqrtConstants
  {
  public:
    static Decimal getSqrt (unsigned long num)
    {
      static Decimal sqrtConstants[] =
	{
	  DecimalConstants<Decimal>::createDecimal (std::string("0.000000")),
	  DecimalConstants<Decimal>::createDecimal (std::string("1.000000")),
	  DecimalConstants<Decimal>::createDecimal (std::string("1.414213")),
	  DecimalConstants<Decimal>::createDecimal (std::string("1.7320508")),
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
	  DecimalConstants<Decimal>::createDecimal (std::string("10.000000"))
	};

      if ((num >= 0) && (num <= 100))
	return sqrtConstants[num];
      else
	{
	  double value = sqrt(num);
	  return Decimal(value);
	}
    }
  };

}
#endif

