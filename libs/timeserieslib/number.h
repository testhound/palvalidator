#ifndef NUMBER_H
#define NUMBER_H

#include <iostream>

#ifndef USE_BLOOMBERG_DECIMALS

#include <cmath>
#include "decimal.h"

namespace num
{
using DefaultNumber = dec::decimal<7>;

  inline std::string toString(DefaultNumber d)
  {
    return dec::toString(d);
  }

  inline DefaultNumber abs(DefaultNumber d)
  {
    return d.abs();
  }

  inline double to_double(DefaultNumber d)
  {
    return d.getAsDouble();
  }

  template<class N>
  inline N fromString(const std::string& s)
  {
    return ::dec::fromString<N>(s);
  }

  inline DefaultNumber Round2Tick (DefaultNumber price, DefaultNumber tick)
  {
    return price;
    
    /*double priceAsDouble = to_double (price);
    double tickAsDouble = to_double (tick);

    double doubleCalc = fmod (priceAsDouble, tickAsDouble);
    DefaultNumber decimalMod(doubleCalc);
    
    return price - decimalMod + ((decimalMod < tick / fromString<DefaultNumber>(std::string("2.0"))) ? fromString<DefaultNumber>(std::string("0.0")) : tick); */
  }

  inline DefaultNumber Round2Tick (DefaultNumber price, DefaultNumber tick, DefaultNumber tickDiv2)
  {
    return price;
  }
} // num namespace

#else

#include <sstream>
#include <assert.h>
#include "bdldfp_decimal.h"
#include "bdldfp_decimalutil.h"
#include "bdldfp_decimalconvertutil.h"

namespace num
{
  namespace dfp = BloombergLP::bdldfp;

  using DefaultNumber = dfp::Decimal64;

  inline std::string toString(DefaultNumber d)
  {
    std::stringstream ss;
    ss << d;
    return ss.str();
  }

  namespace detail
  {
    template<class> struct parser;

    template<> struct parser<dfp::Decimal64> {
      static dfp::Decimal64 parse(const std::string& s) {
	return dfp::DecimalImpUtil::parse64(s.c_str());
      }
    };
  }

  template<class N>
  inline N fromString(const std::string& s) {
    return detail::parser<N>::parse(s);
  }

  inline DefaultNumber abs(DefaultNumber d) {
    return dfp::DecimalUtil::fabs(d);
  }

  inline double to_double(DefaultNumber d)
  {
    return dfp::DecimalConvertUtil::decimalToDouble(d);
  }

  inline DefaultNumber operator%(const DefaultNumber& num, const DefaultNumber& denom)
  {
    DefaultNumber x(num / denom);
    DefaultNumber xInt (dfp::DecimalUtil::trunc (x));

    return num - denom * xInt;
  }
  
  inline DefaultNumber Round2Tick (DefaultNumber price, DefaultNumber tick)
  {
    //return price;
    
    static DefaultNumber decTwo (fromString<DefaultNumber>(std::string("2.0")));
    static DefaultNumber decZero (fromString<DefaultNumber>(std::string("0.0")));
    DefaultNumber decimalMod(price % tick);

    return price - decimalMod + ((decimalMod < tick / decTwo) ? decZero : tick);
  }

  inline DefaultNumber Round2Tick (DefaultNumber price, DefaultNumber tick, DefaultNumber tickDiv2)
  {
    //return price;
    
    static DefaultNumber decTwo (fromString<DefaultNumber>(std::string("2.0")));
    static DefaultNumber decZero (fromString<DefaultNumber>(std::string("0.0")));
    DefaultNumber decimalMod(price % tick);

    return price - decimalMod + ((decimalMod < tickDiv2) ? decZero : tick);
  }
} // num namespace

#endif


#endif // ifndef NUMBER_H
