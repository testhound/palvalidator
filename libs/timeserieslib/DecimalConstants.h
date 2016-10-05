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
  template <int Prec>
  class DecimalConstants
    {
    public:
      static dec::decimal<Prec> DecimalZero;
      static dec::decimal<Prec> DecimalOneHundred;
      static dec::decimal<Prec> DecimalOne;
      static dec::decimal<Prec> DecimalMinusOne;
      static dec::decimal<Prec> DecimalTwo;
      static dec::decimal<Prec> DecimalMinusTwo;
      static dec::decimal<Prec> EquityTick;
      static dec::decimal<Prec> SignificantPValue;
      static dec::decimal<Prec> TwoThirds;
      static dec::decimal<Prec> createDecimal (const std::string& valueString)
      {
	return dec::fromString<dec::decimal<Prec>>(valueString);
      }
    };

  template <int Prec> dec::decimal<Prec> DecimalConstants<Prec>::DecimalZero(0.0);
  template <int Prec> dec::decimal<Prec> 
    DecimalConstants<Prec>::DecimalOneHundred(DecimalConstants<Prec>::createDecimal("100.0"));
  template <int Prec> dec::decimal<Prec> 
    DecimalConstants<Prec>::DecimalTwo(DecimalConstants<Prec>::createDecimal("2.0"));
  template <int Prec> dec::decimal<Prec> 
    DecimalConstants<Prec>::DecimalMinusTwo(DecimalConstants<Prec>::createDecimal("-2.0"));

  template <int Prec> dec::decimal<Prec> DecimalConstants<Prec>::DecimalOne(1.0);
  template <int Prec> dec::decimal<Prec> 
    DecimalConstants<Prec>::DecimalMinusOne(DecimalConstants<Prec>::createDecimal("-1.0"));
  template <int Prec> dec::decimal<Prec> DecimalConstants<Prec>::EquityTick(0.01);
  template <int Prec> dec::decimal<Prec> DecimalConstants<Prec>::SignificantPValue(0.05);


  template <int Prec>
  dec::decimal<Prec>
  createADecimal(const std::string& numString)
  {
    return DecimalConstants<Prec>::createDecimal(numString);
  }

  template <int Prec> dec::decimal<Prec> DecimalConstants<Prec>::TwoThirds(createADecimal<Prec>("66.6666667"));

  template <int Prec>
  class DecimalSqrtConstants
  {
  public:
    static dec::decimal<Prec> getSqrt (unsigned long num)
    {
      static dec::decimal<Prec> sqrtConstants[] =
	{
	  DecimalConstants<Prec>::createDecimal (std::string("0.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("1.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("1.414213")),
	  DecimalConstants<Prec>::createDecimal (std::string("1.7320508")),
	  DecimalConstants<Prec>::createDecimal (std::string("2.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("2.236068")),
	  DecimalConstants<Prec>::createDecimal (std::string("2.449490")),
	  DecimalConstants<Prec>::createDecimal (std::string("2.645751")),
	  DecimalConstants<Prec>::createDecimal (std::string("2.828427")),
	  DecimalConstants<Prec>::createDecimal (std::string("3.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("3.162278")),
	  DecimalConstants<Prec>::createDecimal (std::string("3.316625")),
	  DecimalConstants<Prec>::createDecimal (std::string("3.464102")),
	  DecimalConstants<Prec>::createDecimal (std::string("3.605551")),
	  DecimalConstants<Prec>::createDecimal (std::string("3.741657")),
	  DecimalConstants<Prec>::createDecimal (std::string("3.872983")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.123106")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.242641")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.358899")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.472136")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.582576")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.690416")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.795832")),
	  DecimalConstants<Prec>::createDecimal (std::string("4.898979")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.099020")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.196152")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.291503")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.385165")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.477226")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.567764")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.656854")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.744563")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.830952")),
	  DecimalConstants<Prec>::createDecimal (std::string("5.916080")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.082763")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.164414")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.244998")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.324555")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.403124")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.480741")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.557439")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.633250")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.708204")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.782330")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.855655")),
	  DecimalConstants<Prec>::createDecimal (std::string("6.928203")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.071068")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.141428")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.211103")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.280110")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.348469")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.416198")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.483315")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.549834")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.615773")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.681146")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.745967")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.810250")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.874008")),
	  DecimalConstants<Prec>::createDecimal (std::string("7.937254")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.062258")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.124038")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.185353")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.246211")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.306624")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.366600")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.426150")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.485281")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.544004")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.602325")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.660254")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.717798")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.774964")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.831761")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.888194")),
	  DecimalConstants<Prec>::createDecimal (std::string("8.944272")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.000000")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.055385")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.110434")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.165151")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.219544")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.273618")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.327379")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.380832")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.433981")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.486833")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.539392")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.591663")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.643651")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.695360")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.746794")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.797959")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.848858")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.899495")),
	  DecimalConstants<Prec>::createDecimal (std::string("9.949874")),
	  DecimalConstants<Prec>::createDecimal (std::string("10.000000"))
	};

      if ((num >= 0) && (num <= 100))
	return sqrtConstants[num];
      else
	{
	  double value = sqrt(num);
	  return dec::decimal<Prec>(value);
	}
    }
  };

}
#endif

