/////////////////////////////////////////////////////////////////////////////
// Name:        decimal.h
// Purpose:     Decimal data type support, for COBOL-like fixed-point
//              operations on currency values.
// Author:      Piotr Likus
// Created:     03/01/2011
// Last change: 18/04/2021
// Version:     1.18
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _DECIMAL_H__
#define _DECIMAL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file decimal.h
///
/// Decimal value type. Use for capital calculations.
/// Note: maximum handled value is: +9,223,372,036,854,775,807 (divided by prec)
///
/// Sample usage:
///   using namespace dec;
///   decimal<2> value(143125);
///   value = value / decimal_cast<2>(333);
///   cout << "Result is: " << value << endl;

// ----------------------------------------------------------------------------
// Config section
// ----------------------------------------------------------------------------
// - define DEC_EXTERNAL_INT64 if you do not want internal definition of "int64" data type
//   in this case define "DEC_INT64" somewhere
// - define DEC_EXTERNAL_ROUND if you do not want internal "round()" function
// - define DEC_CROSS_DOUBLE if you want to use double (instead of xdouble) for cross-conversions
// - define DEC_EXTERNAL_LIMITS to define by yourself DEC_MAX_INT32
// - define DEC_NO_CPP11 if your compiler does not support C++11
// - define DEC_ALLOW_SPACESHIP_OPER as 1 if your compiler supports spaceship operator
// - define DEC_TRIVIAL_DEFAULT_CONSTRUCTIBLE as 1 if you want to make default constructor trivial
//   use with caution because default constructor will not initialize the object
// - define DEC_TYPE_LEVEL as 0 for strong typing (same precision required for both arguments),
//   as 1 for allowing to mix lower or equal precision types
//   as 2 for automatic rounding when different precision is mixed

#include <iosfwd>
#include <iomanip>
#include <sstream>
#include <locale>
#include <cmath>

#ifndef DEC_TYPE_LEVEL
#define DEC_TYPE_LEVEL 2
#endif

// --> include headers for limits and int64_t

#ifndef DEC_NO_CPP11
#include <cstdint>
#include <limits>

#else

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#if defined(__GXX_EXPERIMENTAL_CXX0X) || (__cplusplus >= 201103L)
#include <cstdint>
#else
#include <stdint.h>
#endif // defined

#endif // DEC_NO_CPP11

#ifdef DEC_NO_CPP11
#define DEC_OVERRIDE
#else
#define DEC_OVERRIDE override
#endif

#ifdef DEC_NO_CPP11
#define DEC_CONSTEXPR const
#else
#define DEC_CONSTEXPR constexpr
#endif

#ifdef DEC_NO_CPP11
    #define DEC_MOVE(x) (x)
#else
    #include <utility>
    #define DEC_MOVE(x) std::move(x)
#endif

#if (DEC_ALLOW_SPACESHIP_OPER == 1) && (__cplusplus > 201703L)
#define DEC_USE_SPACESHIP_OPER 1
#else
#undef DEC_USE_SPACESHIP_OPER
#define DEC_USE_SPACESHIP_OPER 0
#endif

// <--

// --> define DEC_MAX_INTxx, DEC_MIN_INTxx if required

#ifndef DEC_NAMESPACE
#define DEC_NAMESPACE dec
#endif // DEC_NAMESPACE

#ifndef DEC_EXTERNAL_LIMITS
#ifndef DEC_NO_CPP11
//#define DEC_MAX_INT32 ((std::numeric_limits<int32_t>::max)())
#define DEC_MAX_INT64 ((std::numeric_limits<int64_t>::max)())
#define DEC_MIN_INT64 ((std::numeric_limits<int64_t>::min)())
#else
//#define DEC_MAX_INT32 INT32_MAX
#define DEC_MAX_INT64 INT64_MAX
#define DEC_MIN_INT64 INT64_MIN
#endif // DEC_NO_CPP11
#endif // DEC_EXTERNAL_LIMITS

// <--

namespace DEC_NAMESPACE {

#ifdef DEC_NO_CPP11
    template<bool Condition, class T>
    struct enable_if_type {
        typedef T type;
    };

    template<class T>
    struct enable_if_type<false, T> {
    };

    template<bool condition, class T>
    struct enable_if: public enable_if_type<condition, T> {
    };

#define ENABLE_IF dec::enable_if
#else
#define ENABLE_IF std::enable_if
#endif


// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// --> define DEC_INT64 if required
#ifndef DEC_EXTERNAL_INT64
#ifndef DEC_NO_CPP11
typedef int64_t DEC_INT64;
#else
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef signed __int64 DEC_INT64;
#else
typedef signed long long DEC_INT64;
#endif
#endif
#endif // DEC_EXTERNAL_INT64
// <--

// --> define DEC_HANDLE_LONG if const long meets ambiguous conversion.
#ifndef DEC_HANDLE_LONG
#if defined(__APPLE__) || defined(__MACH__)
#define DEC_HANDLE_LONG
#endif
#endif // DEC_HANDLE_LONG
// <--

#ifdef DEC_NO_CPP11
#define static_assert(a,b)
#endif

typedef DEC_INT64 int64;
// type for storing currency value internally
typedef int64 dec_storage_t;
typedef unsigned int uint;
// xdouble is an "extended double" - can be long double, __float128, _Quad - as you wish
typedef long double xdouble;

#ifdef DEC_CROSS_DOUBLE
typedef double cross_float;
#else
typedef xdouble cross_float;
#endif

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
class basic_decimal_format;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
enum {
    max_decimal_points = 18
};

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
template<int Prec> struct DecimalFactor {
    static DEC_CONSTEXPR int64 value = 10 * DecimalFactor<Prec - 1>::value;
};

template<> struct DecimalFactor<0> {
    static DEC_CONSTEXPR int64 value = 1;
};

template<> struct DecimalFactor<1> {
    static DEC_CONSTEXPR int64 value = 10;
};

template<int Prec, bool positive> struct DecimalFactorDiff_impl {
    static DEC_CONSTEXPR int64 value = DecimalFactor<Prec>::value;
};

template<int Prec> struct DecimalFactorDiff_impl<Prec, false> {
    static DEC_CONSTEXPR int64 value = INT64_MIN;
};

template<int Prec> struct DecimalFactorDiff {
    static DEC_CONSTEXPR int64 value = DecimalFactorDiff_impl<Prec, Prec >= 0>::value;
};

#ifndef DEC_EXTERNAL_ROUND

// round floating point value and convert to int64
template<class T>
inline int64 round(T value) {
    T val1;

    if (value < 0.0) {
        val1 = value - 0.5;
    } else {
        val1 = value + 0.5;
    }
    int64 intPart = static_cast<int64>(val1);

    return intPart;
}

// calculate output = round(a / b), where output, a, b are int64
inline bool div_rounded(int64 &output, int64 a, int64 b) {
    int64 divisorCorr = std::abs(b) / 2;
    if (a >= 0) {
        if (DEC_MAX_INT64 - a >= divisorCorr) {
            output = (a + divisorCorr) / b;
            return true;
        } else {
            const int64 i = a / b;
            const int64 r = a - i * b;
            if (r < divisorCorr) {
                output = i;
                return true;
            }
        }
    } else {
        if (-(DEC_MIN_INT64 - a) >= divisorCorr) {
            output = (a - divisorCorr) / b;
            return true;
        } else {
            const int64 i = a / b;
            const int64 r = a - i * b;
            if (r < divisorCorr) {
                output = i;
                return true;
            }
        }
    }

    output = 0;
    return false;
}

#endif // DEC_EXTERNAL_ROUND

template<class RoundPolicy>
class dec_utils {
public:
    // result = (value1 * value2) / divisor
    inline static int64 multDiv(const int64 value1, const int64 value2,
                                int64 divisor) {

        if (value1 == 0 || value2 == 0) {
            return 0;
        }

        if (divisor == 1) {
            return value1 * value2;
        }

        if (value1 == 1) {
            int64 result;
            if (RoundPolicy::div_rounded(result, value2, divisor)) {
                return result;
            }
        }

        if (value2 == 1) {
            int64 result;
            if (RoundPolicy::div_rounded(result, value1, divisor)) {
                return result;
            }
        }

        // we don't check for division by zero, the caller should - the next line will throw.
        const int64 value1int = value1 / divisor;
        int64 value1dec = value1 % divisor;
        const int64 value2int = value2 / divisor;
        int64 value2dec = value2 % divisor;

        int64 result = value1 * value2int + value1int * value2dec;

        if (value1dec == 0 || value2dec == 0) {
            return result;
        }

        if (!isMultOverflow(value1dec, value2dec)) { // no overflow
            int64 resDecPart = value1dec * value2dec;
            if (!RoundPolicy::div_rounded(resDecPart, resDecPart, divisor))
                resDecPart = 0;
            result += resDecPart;
            return result;
        }

        // minimize value1 & divisor
        {
            int64 c = gcd(value1dec, divisor);
            if (c != 1) {
                value1dec /= c;
                divisor /= c;
            }

            // minimize value2 & divisor
            c = gcd(value2dec, divisor);
            if (c != 1) {
                value2dec /= c;
                divisor /= c;
            }
        }

        if (!isMultOverflow(value1dec, value2dec)) { // no overflow
            int64 resDecPart = value1dec * value2dec;
            if (RoundPolicy::div_rounded(resDecPart, resDecPart, divisor)) {
                result += resDecPart;
                return result;
            }
        }

        // overflow can occur - use less precise version
        result += RoundPolicy::round(
                static_cast<cross_float>(value1dec)
                        * static_cast<cross_float>(value2dec)
                        / static_cast<cross_float>(divisor));
        return result;
    }

    static bool isMultOverflow(const int64 value1, const int64 value2) {
       if (value1 == 0 || value2 == 0) {
           return false;
       }

       if ((value1 < 0) != (value2 < 0)) { // different sign
           if (value1 == DEC_MIN_INT64) {
               return value2 > 1;
           } else if (value2 == DEC_MIN_INT64) {
               return value1 > 1;
           }
           if (value1 < 0) {
               return isMultOverflow(-value1, value2);
           }
           if (value2 < 0) {
               return isMultOverflow(value1, -value2);
           }
       } else if (value1 < 0 && value2 < 0) {
           if (value1 == DEC_MIN_INT64) {
               return value2 < -1;
           } else if (value2 == DEC_MIN_INT64) {
               return value1 < -1;
           }
           return isMultOverflow(-value1, -value2);
       }

       return (value1 > DEC_MAX_INT64 / value2);
    }

    static int64 pow10(int n) {
        static const int64 decimalFactorTable[] = { 1, 10, 100, 1000, 10000,
                100000, 1000000, 10000000, 100000000, 1000000000, 10000000000,
                100000000000, 1000000000000, 10000000000000, 100000000000000,
                1000000000000000, 10000000000000000, 100000000000000000,
                1000000000000000000 };

        if (n >= 0 && n <= max_decimal_points) {
            return decimalFactorTable[n];
        } else {
            return 0;
        }
    }

    template<class T>
    static int64 trunc(T value) {
        return static_cast<int64>(value);
    }

private:
    // calculate greatest common divisor
    static int64 gcd(int64 a, int64 b) {
        int64 c;
        while (a != 0) {
            c = a;
            a = b % a;
            b = c;
        }
        return b;
    }

};

// no-rounding policy (decimal places stripped)
class null_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        return static_cast<int64>(value);
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        output = a / b;
        return true;
    }
};

// default rounding policy - arithmetic, to nearest integer
class def_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        return DEC_NAMESPACE::round(value);
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        return DEC_NAMESPACE::div_rounded(output, a, b);
    }
};

class half_down_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        T val1;
        T decimals;

        if (value >= 0.0) {
            decimals = value - floor(value);
            if (decimals > 0.5) {
                val1 = ceil(value);
            } else {
                val1 = value;
            }
        } else {
            decimals = std::abs(value + floor(std::abs(value)));
            if (decimals < 0.5) {
                val1 = ceil(value);
            } else {
                val1 = value;
            }
        }

        return static_cast<int64>(floor(val1));
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        int64 divisorCorr = std::abs(b) / 2;
        int64 remainder = std::abs(a) % std::abs(b);

        if (a >= 0) {
            if (DEC_MAX_INT64 - a >= divisorCorr) {
                if (remainder > divisorCorr) {
                    output = (a + divisorCorr) / b;
                } else {
                    output = a / b;
                }
                return true;
            }
        } else {
            if (-(DEC_MIN_INT64 - a) >= divisorCorr) {
                output = (a - divisorCorr) / b;
                return true;
            }
        }

        output = 0;
        return false;
    }
};

class half_up_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        T val1;
        T decimals;

        if (value >= 0.0) {
            decimals = value - floor(value);
            if (decimals >= 0.5) {
                val1 = ceil(value);
            } else {
                val1 = value;
            }
        } else {
            decimals = std::abs(value + floor(std::abs(value)));
            if (decimals <= 0.5) {
                val1 = ceil(value);
            } else {
                val1 = value;
            }
        }

        return static_cast<int64>(floor(val1));
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        int64 divisorCorr = std::abs(b) / 2;
        int64 remainder = std::abs(a) % std::abs(b);

        if (a >= 0) {
            if (DEC_MAX_INT64 - a >= divisorCorr) {
                if (remainder >= divisorCorr) {
                    output = (a + divisorCorr) / b;
                } else {
                    output = a / b;
                }
                return true;
            }
        } else {
            if (-(DEC_MIN_INT64 - a) >= divisorCorr) {
                if (remainder < divisorCorr) {
                    output = (a - remainder) / b;
                } else if (remainder == divisorCorr) {
                    output = (a + divisorCorr) / b;
                } else {
                    output = (a + remainder - std::abs(b)) / b;
                }
                return true;
            }
        }

        output = 0;
        return false;
    }
};

// bankers' rounding
class half_even_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        T val1;
        T decimals;

        if (value >= 0.0) {
            decimals = value - floor(value);
            if (decimals > 0.5) {
                val1 = ceil(value);
            } else if (decimals < 0.5) {
                val1 = floor(value);
            } else {
                bool is_even = (static_cast<int64>(value - decimals) % 2 == 0);
                if (is_even) {
                    val1 = floor(value);
                } else {
                    val1 = ceil(value);
                }
            }
        } else {
            decimals = std::abs(value + floor(std::abs(value)));
            if (decimals > 0.5) {
                val1 = floor(value);
            } else if (decimals < 0.5) {
                val1 = ceil(value);
            } else {
                bool is_even = (static_cast<int64>(value + decimals) % 2 == 0);
                if (is_even) {
                    val1 = ceil(value);
                } else {
                    val1 = floor(value);
                }
            }
        }

        return static_cast<int64>(val1);
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        int64 divisorDiv2 = std::abs(b) / 2;
        int64 remainder = std::abs(a) % std::abs(b);

        if (remainder == 0) {
            output = a / b;
        } else {
            if (a >= 0) {

                if (remainder > divisorDiv2) {
                    output = (a - remainder + std::abs(b)) / b;
                } else if (remainder < divisorDiv2) {
                    output = (a - remainder) / b;
                } else {
                    bool is_even = std::abs(a / b) % 2 == 0;
                    if (is_even) {
                        output = a / b;
                    } else {
                        output = (a - remainder + std::abs(b)) / b;
                    }
                }
            } else {
                // negative value
                if (remainder > divisorDiv2) {
                    output = (a + remainder - std::abs(b)) / b;
                } else if (remainder < divisorDiv2) {
                    output = (a + remainder) / b;
                } else {
                    bool is_even = std::abs(a / b) % 2 == 0;
                    if (is_even) {
                        output = a / b;
                    } else {
                        output = (a + remainder - std::abs(b)) / b;
                    }
                }
            }
        }

        return true;
    }
};

// round towards +infinity
class ceiling_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        return static_cast<int64>(ceil(value));
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        int64 remainder = std::abs(a) % std::abs(b);
        if (remainder == 0) {
            output = a / b;
        } else {
            if (a >= 0) {
                output = (a + std::abs(b)) / b;
            } else {
                output = a / b;
            }
        }
        return true;
    }
};

// round towards -infinity
class floor_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        return static_cast<int64>(floor(value));
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        int64 remainder = std::abs(a) % std::abs(b);
        if (remainder == 0) {
            output = a / b;
        } else {
            if (a >= 0) {
                output = (a - remainder) / b;
            } else {
                output = (a + remainder - std::abs(b)) / b;
            }
        }
        return true;
    }
};

// round towards zero = truncate
class round_down_round_policy: public null_round_policy {
};

// round away from zero
class round_up_round_policy {
public:
    template<class T>
    static int64 round(T value) {
        if (value >= 0.0) {
            return static_cast<int64>(ceil(value));
        } else {
            return static_cast<int64>(floor(value));
        }
    }

    static bool div_rounded(int64 &output, int64 a, int64 b) {
        int64 remainder = std::abs(a) % std::abs(b);
        if (remainder == 0) {
            output = a / b;
        } else {
            if (a >= 0) {
                output = (a + std::abs(b)) / b;
            } else {
                output = (a - std::abs(b)) / b;
            }
        }
        return true;
    }
};

template<int Prec, class RoundPolicy = def_round_policy>
class decimal {
public:
    typedef dec_storage_t raw_data_t;
    enum {
        decimal_points = Prec
    };

#ifdef DEC_NO_CPP11
    #ifdef DEC_TRIVIAL_DEFAULT_CONSTRUCTIBLE
        decimal() {
        }
    #else
        decimal() {
            init(0);
        }
    #endif
    decimal(const decimal &src) {
        init(src);
    }
#else
    #ifdef DEC_TRIVIAL_DEFAULT_CONSTRUCTIBLE
        decimal() noexcept = default;
    #else
        decimal() noexcept : m_value(0) {}
    #endif
    decimal(const decimal &src) = default;
#endif
    explicit decimal(uint value) {
        init(value);
    }
    explicit decimal(int value) {
        init(value);
    }
#ifdef DEC_HANDLE_LONG
    explicit decimal(long int value) {
        init(value);
    }
#endif
    explicit decimal(int64 value) {
        init(value);
    }
  explicit decimal(std::size_t value) {
    init(value);
  }
    explicit decimal(xdouble value) {
        init(value);
    }
    explicit decimal(double value) {
        init(value);
    }
    explicit decimal(float value) {
        init(value);
    }
    explicit decimal(int64 value, int64 precFactor) {
        initWithPrec(value, precFactor);
    }
    explicit decimal(const std::string &value) {
        fromString(value, *this);
    }

    explicit decimal(const std::string &value, const basic_decimal_format &format) {
        fromString(value, format, *this);
    }

#ifdef DEC_NO_CPP11
    ~decimal() {
    }
#else
    ~decimal() = default;
#endif

    static int64 getPrecFactor() {
        return DecimalFactor<Prec>::value;
    }
    static int getDecimalPoints() {
        return Prec;
    }

#ifdef DEC_NO_CPP11
    decimal & operator=(const decimal &rhs) {
        if (&rhs != this)
            m_value = rhs.m_value;
        return *this;
    }
#else
    decimal & operator=(const decimal &rhs) = default;
#endif

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    typename ENABLE_IF<Prec >= Prec2, decimal>::type
    & operator=(const decimal<Prec2, RoundPolicy> &rhs) {
        m_value = rhs.getUnbiased() * DecimalFactorDiff<Prec - Prec2>::value;
        return *this;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    decimal & operator=(const decimal<Prec2, RoundPolicy> &rhs) {
        if (Prec2 > Prec) {
            RoundPolicy::div_rounded(m_value, rhs.getUnbiased(),
                    DecimalFactorDiff<Prec2 - Prec>::value);
        } else {
            m_value = rhs.getUnbiased()
                    * DecimalFactorDiff<Prec - Prec2>::value;
        }
        return *this;
    }
#endif

    decimal & operator=(int64 rhs) {
        m_value = DecimalFactor<Prec>::value * rhs;
        return *this;
    }

    decimal & operator=(int rhs) {
        m_value = DecimalFactor<Prec>::value * rhs;
        return *this;
    }

    decimal & operator=(double rhs) {
        m_value = fpToStorage(rhs);
        return *this;
    }

    decimal & operator=(xdouble rhs) {
        m_value = fpToStorage(rhs);
        return *this;
    }

    //template <typename T>
    //bool operator==(const T &rhs) const {
    //    return (*this == static_cast<decimal>(rhs));
    //}

    //template <typename T>
    //bool operator!=(const T &rhs) const {
    //    return !(*this == rhs);
    //}

#if DEC_USE_SPACESHIP_OPER
    template<typename T>
    auto operator<=>(const T &rhs) const {
        return (*this <=> static_cast<decimal>(rhs));
    }
#else
    template <typename T>
    bool operator<(const T &rhs) const {
        return (*this < static_cast<decimal>(rhs));
    }

    template <typename T>
    bool operator<=(const T &rhs) const {
        return (*this <= static_cast<decimal>(rhs));
    }

    template <typename T>
    bool operator>(const T &rhs) const {
        return (*this > static_cast<decimal>(rhs));
    }

    template <typename T>
    bool operator>=(const T &rhs) const {
        return (*this >= static_cast<decimal>(rhs));
    }
#endif

    bool operator==(const decimal &rhs) const {
        return (m_value == rhs.m_value);
    }

    bool operator!=(const decimal &rhs) const {
        return !(*this == rhs);
    }

#if DEC_USE_SPACESHIP_OPER
    auto operator<=>(const decimal &rhs) const {
        return m_value <=> rhs.m_value;
    }
#else

    bool operator<(const decimal &rhs) const {
        return (m_value < rhs.m_value);
    }

    bool operator<=(const decimal &rhs) const {
        return (m_value <= rhs.m_value);
    }

    bool operator>(const decimal &rhs) const {
        return (m_value > rhs.m_value);
    }

    bool operator>=(const decimal &rhs) const {
        return (m_value >= rhs.m_value);
    }
#endif

    template <typename T>
    const decimal operator+(const T &rhs) const {
      return *this + static_cast<decimal>(rhs);
    }

    const decimal operator+(const decimal &rhs) const {
        decimal result = *this;
        result.m_value += rhs.m_value;
        return result;
    }

#if DEC_TYPE_LEVEL == 1
template<int Prec2>
    const typename ENABLE_IF<Prec >= Prec2, decimal>::type
    operator+(const decimal<Prec2, RoundPolicy> &rhs) const {
        decimal result = *this;
        result.m_value += rhs.getUnbiased() * DecimalFactorDiff<Prec - Prec2>::value;
        return result;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    const decimal operator+(const decimal<Prec2, RoundPolicy> &rhs) const {
        decimal result = *this;
        if (Prec2 > Prec) {
            int64 val;
            RoundPolicy::div_rounded(val, rhs.getUnbiased(),
                    DecimalFactorDiff<Prec2 - Prec>::value);
            result.m_value += val;
        } else {
            result.m_value += rhs.getUnbiased()
                    * DecimalFactorDiff<Prec - Prec2>::value;
        }

        return result;
    }
#endif

    template <typename T>
    decimal & operator+=(const T &rhs) {
        *this += static_cast<decimal>(rhs);
        return *this;
    }

    decimal & operator+=(const decimal &rhs) {
        m_value += rhs.m_value;
        return *this;
    }

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    typename ENABLE_IF<Prec >= Prec2, decimal>::type
    & operator+=(const decimal<Prec2, RoundPolicy> &rhs) {
        m_value += rhs.getUnbiased() * DecimalFactorDiff<Prec - Prec2>::value;
        return *this;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    decimal & operator+=(const decimal<Prec2, RoundPolicy> &rhs) {
        if (Prec2 > Prec) {
            int64 val;
            RoundPolicy::div_rounded(val, rhs.getUnbiased(),
                    DecimalFactorDiff<Prec2 - Prec>::value);
            m_value += val;
        } else {
            m_value += rhs.getUnbiased()
                    * DecimalFactorDiff<Prec - Prec2>::value;
        }

        return *this;
    }
#endif

    const decimal operator+() const {
        return *this;
    }

    const decimal operator-() const {
        decimal result = *this;
        result.m_value = -result.m_value;
        return result;
    }

    template <typename T>
    const decimal operator-(const T &rhs) const {
        return *this - static_cast<decimal>(rhs);
    }

    const decimal operator-(const decimal &rhs) const {
        decimal result = *this;
        result.m_value -= rhs.m_value;
        return result;
    }

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    const typename ENABLE_IF<Prec >= Prec2, decimal>::type
    operator-(const decimal<Prec2, RoundPolicy> &rhs) const {
        decimal result = *this;
        result.m_value -= rhs.getUnbiased() * DecimalFactorDiff<Prec - Prec2>::value;
        return result;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    const decimal operator-(const decimal<Prec2, RoundPolicy> &rhs) const {
        decimal result = *this;
        if (Prec2 > Prec) {
            int64 val;
            RoundPolicy::div_rounded(val, rhs.getUnbiased(),
                    DecimalFactorDiff<Prec2 - Prec>::value);
            result.m_value -= val;
        } else {
            result.m_value -= rhs.getUnbiased()
                    * DecimalFactorDiff<Prec - Prec2>::value;
        }

        return result;
    }
#endif

    template <typename T>
    decimal & operator-=(const T &rhs) {
        *this -= static_cast<decimal>(rhs);
        return *this;
    }

    decimal & operator-=(const decimal &rhs) {
        m_value -= rhs.m_value;
        return *this;
    }

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    typename ENABLE_IF<Prec >= Prec2, decimal>::type
    & operator-=(const decimal<Prec2, RoundPolicy> &rhs) {
        m_value -= rhs.getUnbiased() * DecimalFactorDiff<Prec - Prec2>::value;
        return *this;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    decimal & operator-=(const decimal<Prec2, RoundPolicy> &rhs) {
        if (Prec2 > Prec) {
            int64 val;
            RoundPolicy::div_rounded(val, rhs.getUnbiased(),
                    DecimalFactorDiff<Prec2 - Prec>::value);
            m_value -= val;
        } else {
            m_value -= rhs.getUnbiased()
                    * DecimalFactorDiff<Prec - Prec2>::value;
        }

        return *this;
    }
#endif

    template<typename T>
    const decimal operator*(const T &rhs) const {
        return *this * static_cast<decimal>(rhs);
    }

    const decimal operator*(const decimal &rhs) const {
        decimal result = *this;
        result.m_value = dec_utils<RoundPolicy>::multDiv(result.m_value,
                rhs.m_value, DecimalFactor<Prec>::value);
        return result;
    }

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    const typename ENABLE_IF<Prec >= Prec2, decimal>::type
    operator*(const decimal<Prec2, RoundPolicy>& rhs) const {
        decimal result = *this;
        result.m_value = dec_utils<RoundPolicy>::multDiv(result.m_value,
                rhs.getUnbiased(), DecimalFactor<Prec2>::value);
        return result;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    const decimal operator*(const decimal<Prec2, RoundPolicy>& rhs) const {
        decimal result = *this;
        result.m_value = dec_utils<RoundPolicy>::multDiv(result.m_value,
                rhs.getUnbiased(), DecimalFactor<Prec2>::value);
        return result;
    }
#endif

    template <typename T>
    decimal & operator*=(const T &rhs) {
        *this *= static_cast<decimal>(rhs);
        return *this;
    }

    decimal & operator*=(const decimal &rhs) {
        m_value = dec_utils<RoundPolicy>::multDiv(m_value, rhs.m_value,
                DecimalFactor<Prec>::value);
        return *this;
    }

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    typename ENABLE_IF<Prec >= Prec2, decimal>::type
    & operator*=(const decimal<Prec2, RoundPolicy>& rhs) {
        m_value = dec_utils<RoundPolicy>::multDiv(m_value, rhs.getUnbiased(),
                DecimalFactor<Prec2>::value);
        return *this;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    decimal & operator*=(const decimal<Prec2, RoundPolicy>& rhs) {
        m_value = dec_utils<RoundPolicy>::multDiv(m_value, rhs.getUnbiased(),
                DecimalFactor<Prec2>::value);
        return *this;
    }
#endif

    template <typename T>
    const decimal operator/(const T &rhs) const {
        return *this / static_cast<decimal>(rhs);
    }

    const decimal operator/(const decimal &rhs) const {
        decimal result = *this;
        //result.m_value = (result.m_value * DecimalFactor<Prec>::value) / rhs.m_value;
        result.m_value = dec_utils<RoundPolicy>::multDiv(result.m_value,
                DecimalFactor<Prec>::value, rhs.m_value);

        return result;
    }

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    const typename ENABLE_IF<Prec >= Prec2, decimal>::type
    operator/(const decimal<Prec2, RoundPolicy>& rhs) const {
        decimal result = *this;
        result.m_value = dec_utils<RoundPolicy>::multDiv(result.m_value,
                DecimalFactor<Prec2>::value, rhs.getUnbiased());
        return result;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    const decimal operator/(const decimal<Prec2, RoundPolicy>& rhs) const {
        decimal result = *this;
        result.m_value = dec_utils<RoundPolicy>::multDiv(result.m_value,
                DecimalFactor<Prec2>::value, rhs.getUnbiased());
        return result;
    }
#endif

    template <typename T>
    decimal & operator/=(const T &rhs) {
        *this /= static_cast<decimal>(rhs);
        return *this;
    }

    decimal & operator/=(const decimal &rhs) {
        //m_value = (m_value * DecimalFactor<Prec>::value) / rhs.m_value;
        m_value = dec_utils<RoundPolicy>::multDiv(m_value,
                DecimalFactor<Prec>::value, rhs.m_value);

        return *this;
    }

#if DEC_TYPE_LEVEL == 1
    template<int Prec2>
    typename ENABLE_IF<Prec >= Prec2, decimal>::type
    & operator/=(const decimal<Prec2, RoundPolicy> &rhs) {
        m_value = dec_utils<RoundPolicy>::multDiv(m_value,
                DecimalFactor<Prec2>::value, rhs.getUnbiased());

        return *this;
    }
#elif DEC_TYPE_LEVEL > 1
    template<int Prec2>
    decimal & operator/=(const decimal<Prec2, RoundPolicy> &rhs) {
        m_value = dec_utils<RoundPolicy>::multDiv(m_value,
                                                  DecimalFactor<Prec2>::value, rhs.getUnbiased());

        return *this;
    }
#endif

    template <typename T>
    const decimal operator%(T n) const {
        return *this % static_cast<decimal>(n);
    }

    template <typename T>
    decimal & operator%=(T rhs) {
        *this %= static_cast<decimal>(rhs);
        return *this;
    }

    const decimal operator%(const decimal<Prec> &rhs) const {
        int64 resultPayload;
        resultPayload = this->m_value;
        resultPayload %= rhs.m_value;
        decimal<Prec> result;
        result.m_value = resultPayload;
        return result;
    }

    decimal & operator%=(const decimal<Prec> &rhs) {
        int64 resultPayload;
        resultPayload = this->m_value;
        resultPayload %= rhs.m_value;
        this->m_value = resultPayload;
        return *this;
    }

#if DEC_TYPE_LEVEL >= 1
    template<int Prec2>
    typename ENABLE_IF<Prec >= Prec2, decimal>::type
    operator%(const decimal<Prec2, RoundPolicy> &rhs) const {
        int64 rhsInThisPrec = rhs.getUnbiased() * DecimalFactorDiff<Prec - Prec2>::value;
        int64 resultPayload = this->m_value;
        resultPayload %= rhsInThisPrec;
        decimal<Prec> result;
        result.m_value = resultPayload;
        return result;
    }

    template<int Prec2>
    typename ENABLE_IF<Prec >= Prec2, decimal &>::type
    operator%=(const decimal<Prec2, RoundPolicy> &rhs) {
        int64 rhsInThisPrec = rhs.getUnbiased() * DecimalFactorDiff<Prec - Prec2>::value;
        int64 resultPayload = this->m_value;
        resultPayload %= rhsInThisPrec;
        this->m_value = resultPayload;
        return *this;
    }
#endif

#if DEC_TYPE_LEVEL > 1
    template<int Prec2>
    typename ENABLE_IF<Prec < Prec2, decimal>::type
    operator%(const decimal<Prec2, RoundPolicy> &rhs) const {
        int64 thisInRhsPrec = m_value * DecimalFactorDiff<Prec2 - Prec>::value;
        int64 resultPayload = thisInRhsPrec % rhs.getUnbiased();
        resultPayload /= DecimalFactorDiff<Prec2 - Prec>::value;
        decimal<Prec> result;
        result.m_value = resultPayload;
        return result;
    }

    template<int Prec2>
    typename ENABLE_IF<Prec < Prec2, decimal>::type
    operator%=(const decimal<Prec2, RoundPolicy> &rhs) {
        int64 thisInRhsPrec = m_value * DecimalFactorDiff<Prec2 - Prec>::value;
        int64 resultPayload = thisInRhsPrec % rhs.getUnbiased();
        resultPayload /= DecimalFactorDiff<Prec2 - Prec>::value;
        this->m_value = resultPayload;
        return *this;
    }
#endif

    /// Returns integer indicating sign of value
    /// -1 if value is < 0
    /// +1 if value is > 0
    /// 0  if value is 0
    int sign() const {
        return (m_value > 0) ? 1 : ((m_value < 0) ? -1 : 0);
    }

    double getAsDouble() const {
        return static_cast<double>(m_value) / getPrecFactorDouble();
    }

    void setAsDouble(double value) {
        m_value = fpToStorage(value);
    }

    xdouble getAsXDouble() const {
        return static_cast<xdouble>(m_value) / getPrecFactorXDouble();
    }

    void setAsXDouble(xdouble value) {
        m_value = fpToStorage(value);
    }

    // returns integer value = real_value * (10 ^ precision)
    // use to load/store decimal value in external memory
    int64 getUnbiased() const {
        return m_value;
    }
    void setUnbiased(int64 value) {
        m_value = value;
    }

    decimal<Prec> abs() const {
        if (m_value >= 0)
            return *this;
        else
            return (decimal<Prec>(0) - *this);
    }

    decimal<Prec> trunc() const {
        int64 beforeValue, afterValue;
        afterValue = m_value % DecimalFactor<Prec>::value;
        beforeValue = (m_value - afterValue);
        decimal<Prec> result;
        result.m_value = beforeValue;
        return result;
    }

    decimal<Prec> floor() const {
        int64 beforeValue, afterValue;
        afterValue = m_value % DecimalFactor<Prec>::value;
        beforeValue = (m_value - afterValue);

        if (afterValue < 0) beforeValue -= DecimalFactor<Prec>::value;

        decimal<Prec> result;
        result.m_value = beforeValue;
        return result;
    }

    decimal<Prec> ceil() const {
        int64 beforeValue, afterValue;
        afterValue = m_value % DecimalFactor<Prec>::value;
        beforeValue = (m_value - afterValue);

        if (afterValue > 0) beforeValue +=  DecimalFactor<Prec>::value;
        decimal<Prec> result;
        result.m_value = beforeValue;
        return result;
    }

    decimal<Prec> round() const {
        int64 resultPayload;
        RoundPolicy::div_rounded(resultPayload, this->m_value, DecimalFactor<Prec>::value);
        decimal<Prec> result(resultPayload);
        return result;
    }

    /// returns value rounded to integer using active rounding policy
    int64 getAsInteger() const {
        int64 result;
        RoundPolicy::div_rounded(result, m_value, DecimalFactor<Prec>::value);
        return result;
    }

    /// overwrites internal value with integer
    void setAsInteger(int64 value) {
        m_value = DecimalFactor<Prec>::value * value;
    }

    /// Returns two parts: before and after decimal point
    /// For negative values both numbers are negative or zero.
    void unpack(int64 &beforeValue, int64 &afterValue) const {
        afterValue = m_value % DecimalFactor<Prec>::value;
        beforeValue = (m_value - afterValue) / DecimalFactor<Prec>::value;
    }

    /// Combines two parts (before and after decimal point) into decimal value.
    /// Both input values have to have the same sign for correct results.
    /// Does not perform any rounding or input validation - afterValue must be less than 10^prec.
    /// \param[in] beforeValue value before decimal point
    /// \param[in] afterValue value after decimal point multiplied by 10^prec
    /// \result Returns *this
    decimal &pack(int64 beforeValue, int64 afterValue) {
        if (Prec > 0) {
            m_value = beforeValue * DecimalFactor<Prec>::value;
            m_value += (afterValue % DecimalFactor<Prec>::value);
        } else
            m_value = beforeValue * DecimalFactor<Prec>::value;
        return *this;
    }

    /// Version of pack() with rounding, sourcePrec specifies precision of source values.
    /// See also @pack.
    template<int sourcePrec>
    decimal &pack_rounded(int64 beforeValue, int64 afterValue) {
        decimal<sourcePrec> temp;
        temp.pack(beforeValue, afterValue);
        decimal<Prec> result(temp.getUnbiased(), temp.getPrecFactor());

        *this = result;
        return *this;
    }

    static decimal buildWithExponent(int64 mantissa, int exponent) {
        decimal result;
        result.setWithExponent(mantissa, exponent);
        return result;
    }

    static decimal &buildWithExponent(decimal &output, int64 mantissa,
            int exponent) {
        output.setWithExponent(mantissa, exponent);
        return output;
    }

    void setWithExponent(int64 mantissa, int exponent) {

        int exponentForPack = exponent + Prec;

        if (exponentForPack < 0) {
            int64 newValue;

            if (!RoundPolicy::div_rounded(newValue, mantissa,
                    dec_utils<RoundPolicy>::pow10(-exponentForPack))) {
                newValue = 0;
            }

            m_value = newValue;
        } else {
            m_value = mantissa * dec_utils<RoundPolicy>::pow10(exponentForPack);
        }
    }

    void getWithExponent(int64 &mantissa, int &exponent) const {
        int64 value = m_value;
        int exp = -Prec;

        if (value != 0) {
            // normalize
            while (value % 10 == 0) {
                value /= 10;
                exp++;
            }
        }

        mantissa = value;
        exponent = exp;
    }

protected:
    inline xdouble getPrecFactorXDouble() const {
        return static_cast<xdouble>(DecimalFactor<Prec>::value);
    }

    inline double getPrecFactorDouble() const {
        return static_cast<double>(DecimalFactor<Prec>::value);
    }

    void init(const decimal &src) {
        m_value = src.m_value;
    }

    void init(uint value) {
        m_value = DecimalFactor<Prec>::value * value;
    }

    void init(int value) {
        m_value = DecimalFactor<Prec>::value * value;
    }

#ifdef DEC_HANDLE_LONG
    void init(long int value) {
        m_value = DecimalFactor<Prec>::value * value;
    }
#endif

    void init(int64 value) {
        m_value = DecimalFactor<Prec>::value * value;
    }

  void init(std::size_t value)
  {
    m_value = DecimalFactor<Prec>::value * value;
  }

    void init(xdouble value) {
        m_value = fpToStorage(value);
    }

    void init(double value) {
        m_value = fpToStorage(value);
    }

    void init(float value) {
        m_value = fpToStorage(static_cast<double>(value));
    }

    void initWithPrec(int64 value, int64 precFactor) {
        int64 ownFactor = DecimalFactor<Prec>::value;

        if (ownFactor == precFactor) {
            // no conversion required
            m_value = value;
        }
        else if (ownFactor > precFactor) {
            m_value = value * (ownFactor / precFactor);
        }
        else {
            // conversion
            RoundPolicy::div_rounded(m_value, value, precFactor / ownFactor);
        }
    }

    template<typename T>
    static dec_storage_t fpToStorage(T value) {
        dec_storage_t intPart = dec_utils<RoundPolicy>::trunc(value);
        T fracPart = value - intPart;
        return RoundPolicy::round(
                static_cast<T>(DecimalFactor<Prec>::value) * fracPart) +
                  DecimalFactor<Prec>::value * intPart;
    }

    template<typename T>
    static T abs(T value) {
        if (value < 0)
            return -value;
        else
            return value;
    }

    static int sign(int64 value) {
        return (value > 0) ? 1 : ((value < 0) ? -1 : 0);
    }
protected:
    dec_storage_t m_value;
};

// ----------------------------------------------------------------------------
// Pre-defined types
// ----------------------------------------------------------------------------
typedef decimal<2> decimal2;
typedef decimal<4> decimal4;
typedef decimal<6> decimal6;

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------
template<int Prec, class T>
decimal<Prec> decimal_cast(const T &arg) {
    return decimal<Prec>(arg.getUnbiased(), arg.getPrecFactor());
}

// Example of use:
//   c = dec::decimal_cast<6>(a * b);
template<int Prec>
decimal<Prec> decimal_cast(uint arg) {
    decimal<Prec> result(arg);
    return result;
}

template<int Prec>
decimal<Prec> decimal_cast(int arg) {
    decimal<Prec> result(arg);
    return result;
}

template<int Prec>
decimal<Prec> decimal_cast(int64 arg) {
    decimal<Prec> result(arg);
    return result;
}

template<int Prec>
decimal<Prec> decimal_cast(double arg) {
    decimal<Prec> result(arg);
    return result;
}

template<int Prec>
decimal<Prec> decimal_cast(const std::string &arg) {
    decimal<Prec> result(arg);
    return result;
}

template<int Prec, int N>
decimal<Prec> decimal_cast(const char (&arg)[N]) {
    decimal<Prec> result(arg);
    return result;
}

// with rounding policy
template<int Prec, typename RoundPolicy>
decimal<Prec, RoundPolicy> decimal_cast(uint arg) {
    decimal<Prec, RoundPolicy> result(arg);
    return result;
}

template<int Prec, typename RoundPolicy>
decimal<Prec, RoundPolicy> decimal_cast(int arg) {
    decimal<Prec, RoundPolicy> result(arg);
    return result;
}

template<int Prec, typename RoundPolicy>
decimal<Prec, RoundPolicy> decimal_cast(int64 arg) {
    decimal<Prec, RoundPolicy> result(arg);
    return result;
}

template<int Prec, typename RoundPolicy>
decimal<Prec, RoundPolicy> decimal_cast(double arg) {
    decimal<Prec, RoundPolicy> result(arg);
    return result;
}

template<int Prec, typename RoundPolicy>
decimal<Prec, RoundPolicy> decimal_cast(const std::string &arg) {
    decimal<Prec, RoundPolicy> result(arg);
    return result;
}

template<int Prec, typename RoundPolicy, int N>
decimal<Prec, RoundPolicy> decimal_cast(const char (&arg)[N]) {
    decimal<Prec, RoundPolicy> result(arg);
    return result;
}

    // value format with constant default values
    class basic_decimal_format {
    public:
        virtual bool change_thousands_if_needed() const { return true; }

        virtual char decimal_point() const {
            return '.';
        }

        virtual char thousands_sep() const {
            return ',';
        }

        virtual bool thousands_grouping() const {
            return false;
        }

        virtual std::string grouping() const {
            return "";
        }
    };

    // value format with full specification stored in fields
    class decimal_format: public basic_decimal_format {
    public:

        decimal_format(char decimal_point) :
          m_decimal_point(decimal_point),
          m_thousands_sep(','),
          m_thousands_grouping(false),
          m_grouping("") {
        }

        decimal_format(char decimal_point, char thousands_sep) : m_decimal_point(decimal_point),
                                                                       m_thousands_sep(thousands_sep),
                                                                       m_thousands_grouping(thousands_sep != '\0'),
                                                                       m_grouping(thousands_sep != '\0' ? "\03" : "") {

        }

        decimal_format(char decimal_point, char thousands_sep, bool thousands_grouping) : m_decimal_point(decimal_point),
                                                                                                m_thousands_sep(thousands_sep),
                                                                                                m_thousands_grouping(thousands_grouping),
                                                                                                m_grouping(thousands_grouping ? "\03" : "") {

        }

        decimal_format(char decimal_point, char thousands_sep, bool thousands_grouping, const std::string &grouping) :
                m_decimal_point(decimal_point),
                m_thousands_sep(thousands_sep),
                m_thousands_grouping(thousands_grouping),
                m_grouping(grouping) {

        }

        decimal_format(const decimal_format &source): m_decimal_point(source.m_decimal_point),
                                                                  m_thousands_sep(source.m_thousands_sep),
                                                                  m_thousands_grouping(source.m_thousands_grouping),
                                                                  m_grouping(source.m_grouping) {

        }

        decimal_format & operator=(const decimal_format &rhs) {
            if (&rhs != this) {
                m_decimal_point = rhs.m_decimal_point;
                m_thousands_sep = rhs.m_thousands_sep;
                m_thousands_grouping = rhs.m_thousands_grouping;
                m_grouping = rhs.grouping();
            }
            return *this;
        }

        char decimal_point() const DEC_OVERRIDE {
            return m_decimal_point;
        }

        char thousands_sep() const DEC_OVERRIDE {
            return m_thousands_sep;
        }

        bool thousands_grouping() const DEC_OVERRIDE {
            return m_thousands_grouping;
        }

        std::string grouping() const DEC_OVERRIDE {
            return m_grouping;
        }

    private:
        char m_decimal_point;
        char m_thousands_sep;
        bool m_thousands_grouping;
        std::string m_grouping;
    };

    class decimal_format_punct : public std::numpunct<char>
    {
    public:
        decimal_format_punct(const basic_decimal_format &format): m_format(format) {}

    protected:
        virtual char do_thousands_sep() const { return m_format.thousands_sep(); }
        virtual std::string do_grouping() const { return m_format.grouping(); }
        const basic_decimal_format &m_format;
    };

    template<typename StreamType>
    decimal_format format_from_stream(StreamType &stream) {
        using namespace std;
        const numpunct<char> *facet =
                has_facet<numpunct<char> >(stream.getloc()) ?
                &use_facet<numpunct<char> >(stream.getloc()) : NULL;
        const char dec_point = (facet != NULL) ? facet->decimal_point() : '.';
        const bool thousands_grouping =
                (facet != NULL) ? (!facet->grouping().empty()) : false;
        const char thousands_sep = (facet != NULL) ? facet->thousands_sep() : ',';
        string grouping_spec =
                (facet != NULL) ? (facet->grouping()) : string("");
        return decimal_format(dec_point, thousands_sep, thousands_grouping, grouping_spec);
    }

    /// Exports decimal to stream
    /// Used format: {-}bbbb.aaaa where
    /// {-} is optional '-' sign character
    /// '.' is locale-dependent decimal point character
    /// bbbb is stream of digits before decimal point
    /// aaaa is stream of digits after decimal point
    template<class decimal_type, typename StreamType>
    void toStream(const decimal_type &arg, const basic_decimal_format &format, StreamType &output, bool formatFromStream = false) {
        using namespace std;

        int64 before, after;
        int sign;

        arg.unpack(before, after);
        sign = 1;

        if (before < 0) {
            sign = -1;
            before = -before;
        }

        if (after < 0) {
            sign = -1;
            after = -after;
        }

        if (sign < 0) {
            output << "-";
        }

        std::locale oldloc = output.getloc();
        if (!formatFromStream || (format.thousands_grouping() && format.change_thousands_if_needed())) {
            output.imbue( std::locale( std::locale::classic(), new decimal_format_punct(format) ) );
            output << before;
            output.imbue(oldloc);
        } else {
            output << before;
        }

        if (arg.getDecimalPoints() > 0) {
            output.imbue(std::locale::classic());
            output << format.decimal_point();
            output << setw(arg.getDecimalPoints()) << setfill('0') << right
                   << after;
            output.imbue(oldloc);
        }
    }

    template<class decimal_type, typename StreamType>
    void toStream(const decimal_type &arg, StreamType &output) {
        toStream(arg, format_from_stream(output), output, true);
    }

namespace details {

    /// Extract values from stream ready to be packed to decimal
    template<typename StreamType>
    bool parse_unpacked(StreamType &input, const basic_decimal_format &format, int &sign, int64 &before, int64 &after,
                        int &decimalDigits) {

        const char dec_point = format.decimal_point();
        const bool thousands_grouping = format.thousands_grouping();
        const char thousands_sep = format.thousands_sep();

        enum StateEnum {
            IN_SIGN, IN_BEFORE_FIRST_DIG, IN_BEFORE_DEC, IN_AFTER_DEC, IN_END
        } state = IN_SIGN;
        enum ErrorCodes {
            ERR_WRONG_CHAR = -1,
            ERR_NO_DIGITS = -2,
            ERR_WRONG_STATE = -3,
            ERR_STREAM_GET_ERROR = -4
        };

        before = after = 0;
        sign = 1;

        int error = 0;
        int digitsCount = 0;
        int afterDigitCount = 0;
        char c;

        while ((input) && (state != IN_END)) // loop while extraction from file is possible
        {
            c = static_cast<char>(input.get());

            switch (state) {
            case IN_SIGN:
                if (c == '-') {
                    sign = -1;
                    state = IN_BEFORE_FIRST_DIG;
                } else if (c == '+') {
                    state = IN_BEFORE_FIRST_DIG;
                } else if ((c >= '0') && (c <= '9')) {
                    state = IN_BEFORE_DEC;
                    before = static_cast<int>(c - '0');
                    digitsCount++;
                } else if (c == dec_point) {
                    state = IN_AFTER_DEC;
                } else if ((c != ' ') && (c != '\t')) {
                    state = IN_END;
                    error = ERR_WRONG_CHAR;
                }
                // else ignore char
                break;
            case IN_BEFORE_FIRST_DIG:
                if ((c >= '0') && (c <= '9')) {
                    before = 10 * before + static_cast<int>(c - '0');
                    state = IN_BEFORE_DEC;
                    digitsCount++;
                } else if (c == dec_point) {
                    state = IN_AFTER_DEC;
                } else {
                    state = IN_END;
                    error = ERR_WRONG_CHAR;
                }
                break;
            case IN_BEFORE_DEC:
                if ((c >= '0') && (c <= '9')) {
                    before = 10 * before + static_cast<int>(c - '0');
                    digitsCount++;
                } else if (c == dec_point) {
                    state = IN_AFTER_DEC;
                } else if (thousands_grouping && c == thousands_sep) {
                    ; // ignore the char
                } else {
                    state = IN_END;
                }
                break;
            case IN_AFTER_DEC:
                if ((c >= '0') && (c <= '9')) {
                    after = 10 * after + static_cast<int>(c - '0');
                    afterDigitCount++;
                    if (afterDigitCount >= DEC_NAMESPACE::max_decimal_points)
                        state = IN_END;
                } else {
                    state = IN_END;
                    if (digitsCount == 0) {
                        error = ERR_NO_DIGITS;
                    }
                }
                break;
            default:
                error = ERR_WRONG_STATE;
                state = IN_END;
                break;
            } // switch state
        } // while stream good & not end

        decimalDigits = afterDigitCount;

        if (error >= 0) {

            if (sign < 0) {
                before = -before;
                after = -after;
            }

        } else {
            before = after = 0;
        }

        return (error >= 0);
    } // function

    template<typename StreamType>
    bool parse_unpacked(StreamType &input, int &sign, int64 &before, int64 &after,
                        int &decimalDigits) {
        return parse_unpacked(input, format_from_stream(input), sign, before, after, decimalDigits);
    }
}
;
// namespace

/// Converts stream of chars to decimal
/// Handles the following formats ('.' is selected from locale info):
/// \code
/// 123
/// -123
/// 123.0
/// -123.0
/// 123.
/// .123
/// 0.
/// -.123
/// \endcode
/// Spaces and tabs on the front are ignored.
/// Performs rounding when provided value has higher precision than in output type.
/// \param[in] input input stream
/// \param[out] output decimal value, 0 on error
/// \result Returns true if conversion succeeded
template<typename decimal_type, typename StreamType>
bool fromStream(StreamType &input, const basic_decimal_format &format, decimal_type &output) {
    int sign, afterDigits;
    int64 before, after;
    bool result = details::parse_unpacked(input, format, sign, before, after,
            afterDigits);
    if (result) {
        if (afterDigits <= decimal_type::decimal_points) {
            // direct mode
            int corrCnt = decimal_type::decimal_points - afterDigits;
            while (corrCnt > 0) {
                after *= 10;
                --corrCnt;
            }
            output.pack(before, after);
        } else {
            // rounding mode
            int corrCnt = afterDigits;
            int64 decimalFactor = 1;
            while (corrCnt > 0) {
                before *= 10;
                decimalFactor *= 10;
                --corrCnt;
            }
            decimal_type temp(before + after, decimalFactor);
            output = temp;
        }
    } else {
        output = decimal_type(0);
    }
    return result;
}

    template<typename decimal_type, typename StreamType>
    bool fromStream(StreamType &input, decimal_type &output) {
        return fromStream(input, format_from_stream(input), output);
    }

    /// Exports decimal to string
    /// Used format: {-}bbbb.aaaa where
    /// {-} is optional '-' sign character
    /// '.' is locale-dependent decimal point character
    /// bbbb is stream of digits before decimal point
    /// aaaa is stream of digits after decimal point
    template<int prec, typename roundPolicy>
    std::string &toString(const decimal<prec, roundPolicy> &arg,
            const basic_decimal_format &format,
            std::string &output) {
        using namespace std;

        ostringstream out;
        toStream(arg, format, out);
        output = DEC_MOVE(out.str());
        return output;
    }

    template<int prec, typename roundPolicy>
    std::string &toString(const decimal<prec, roundPolicy> &arg,
                          std::string &output) {
        using namespace std;

        ostringstream out;
        toStream(arg, out);
        output = DEC_MOVE(out.str());
        return output;
    }

    /// Exports decimal to string
    /// Used format: {-}bbbb.aaaa where
    /// {-} is optional '-' sign character
    /// '.' is locale-dependent decimal point character
    /// bbbb is stream of digits before decimal point
    /// aaaa is stream of digits after decimal point
    template<int prec, typename roundPolicy>
    std::string toString(const decimal<prec, roundPolicy> &arg) {
        std::string res;
        toString(arg, res);
        return res;
    }

    template<int prec, typename roundPolicy>
    std::string toString(const decimal<prec, roundPolicy> &arg, const basic_decimal_format &format) {
        std::string res;
        toString(arg, format, res);
        return res;
    }

    // input
    template<class charT, class traits, int prec, typename roundPolicy>
    std::basic_istream<charT, traits> &
    operator>>(std::basic_istream<charT, traits> & is,
            decimal<prec, roundPolicy> & d) {
        if (!fromStream(is, d))
            d.setUnbiased(0);
        return is;
    }

    // output
    template<class charT, class traits, int prec, typename roundPolicy>
    std::basic_ostream<charT, traits> &
    operator<<(std::basic_ostream<charT, traits> & os,
            const decimal<prec, roundPolicy> & d) {
        toStream(d, os);
        return os;
    }

    /// Imports decimal from string
    /// Used format: {-}bbbb.aaaa where
    /// {-} is optional '-' sign character
    /// '.' is locale-dependent decimal point character
    /// bbbb is stream of digits before decimal point
    /// aaaa is stream of digits after decimal point
    template<typename T>
    T fromString(const std::string &str) {
        std::istringstream is(str);
        T t;

        if (!fromStream(is, t)) {
            t.setUnbiased(0);
        }

        return t;
    }

    template<typename T>
    T fromString(const std::string &str, const basic_decimal_format &format) {
        std::istringstream is(str);
        T t;

        if (!fromStream(is, format, t)) {
            t.setUnbiased(0);
        }

        return t;
    }

    template<typename T>
    void fromString(const std::string &str, const basic_decimal_format &format, T &out) {
        out = fromString<T>(str, format);
    }

    template<typename T>
    void fromString(const std::string &str, T &out) {
        out = fromString<T>(str);
    }

} // namespace
#endif // _DECIMAL_H__
