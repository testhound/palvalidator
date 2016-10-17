#ifndef NUMBER_H
#define NUMBER_H


#ifndef USE_BLOOMBERG_DECIMALS

#include "decimal.h"

namespace num {
using DefaultNumber = dec::decimal<7>;

inline std::string toString(DefaultNumber d) {
      return dec::toString(d);
}


inline DefaultNumber abs(DefaultNumber d) {
      return d.abs();
}

inline double to_double(DefaultNumber d) {
      return d.getAsDouble();
}

} // num namespace

#else

#include "bdldfp_decimal.h"
#include "bdldfp_decimalutil.h"
#include "bdldfp_decimalconvertutil.h"

namespace num {

using DefaultNumber = BloombergLP::bdldfp::Decimal64;

inline std::string toString(DefaultNumber d) {
      std::stringstream ss;
      ss << d;
      return ss.str();
}


inline DefaultNumber abs(DefaultNumber d) {
      namespace blm = BloombergLP::bdldfp;
      return blm::DecimalUtil::fabs(d);
}

inline double to_double(DefaultNumber d) {
      namespace blm = BloombergLP::bdldfp;
      return blm::DecimalConvertUtil::decimalToDouble(d);
}

} // num namespace

#endif


#endif // ifndef NUMBER_H