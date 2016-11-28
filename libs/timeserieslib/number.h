#ifndef NUMBER_H
#define NUMBER_H

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
    double priceAsDouble = to_double (price);
    double tickAsDouble = to_double (tick);

    double doubleCalc = fmod (priceAsDouble, tickAsDouble);
    DefaultNumber decimalMod(doubleCalc);
    
    return price - decimalMod + ((decimalMod < tick / fromString<DefaultNumber>(std::string("2.0"))) ? fromString<DefaultNumber>(std::string("0.0")) : tick);
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

  inline DefaultNumber Round2Tick (DefaultNumber price, DefaultNumber tick)
  {
    double priceAsDouble = to_double (price);
    double tickAsDouble = to_double (tick);

    double doubleCalc = fmod (priceAsDouble, tickAsDouble);
    DefaultNumber decimalMod(doubleCalc);
    
    return price - decimalMod + ((decimalMod < tick / fromString<DefaultNumber>(std::string("2.0"))) ? fromString<DefaultNumber>(std::string("0.0")) : tick);
  }
} // num namespace

#endif


#endif // ifndef NUMBER_H
