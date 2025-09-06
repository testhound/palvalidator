#ifndef _DECIMAL_MATH_H__
#define _DECIMAL_MATH_H__

#include "decimal.h"
#include <cmath>

namespace DEC_NAMESPACE {

/**
 * @brief Mathematical function overloads for decimal types
 * 
 * This header provides mathematical function overloads for decimal types
 * by converting to double, performing the operation, and converting back.
 * This maintains compatibility with std::log, std::pow, std::exp, std::sqrt, etc.
 */

// Logarithm functions
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> log(const decimal<Prec, RoundPolicy>& value) {
    return decimal<Prec, RoundPolicy>(std::log(value.getAsDouble()));
}

// Power functions
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> pow(const decimal<Prec, RoundPolicy>& base, const decimal<Prec, RoundPolicy>& exponent) {
    return decimal<Prec, RoundPolicy>(std::pow(base.getAsDouble(), exponent.getAsDouble()));
}

template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> pow(const decimal<Prec, RoundPolicy>& base, int exponent) {
    return decimal<Prec, RoundPolicy>(std::pow(base.getAsDouble(), static_cast<double>(exponent)));
}

template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> pow(const decimal<Prec, RoundPolicy>& base, double exponent) {
    return decimal<Prec, RoundPolicy>(std::pow(base.getAsDouble(), exponent));
}

// Exponential function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> exp(const decimal<Prec, RoundPolicy>& value) {
    return decimal<Prec, RoundPolicy>(std::exp(value.getAsDouble()));
}

// Square root function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> sqrt(const decimal<Prec, RoundPolicy>& value) {
    return decimal<Prec, RoundPolicy>(std::sqrt(value.getAsDouble()));
}

// Absolute value function (already exists in decimal class, but provide std:: compatible version)
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> abs(const decimal<Prec, RoundPolicy>& value) {
    return value.abs();
}

// Maximum function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> max(const decimal<Prec, RoundPolicy>& a, const decimal<Prec, RoundPolicy>& b) {
    return (a > b) ? a : b;
}

// Minimum function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> min(const decimal<Prec, RoundPolicy>& a, const decimal<Prec, RoundPolicy>& b) {
    return (a < b) ? a : b;
}

} // namespace DEC_NAMESPACE

// Provide std namespace overloads for compatibility
namespace std {

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> log(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& value) {
    return DEC_NAMESPACE::log(value);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> pow(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& base, 
                                              const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& exponent) {
    return DEC_NAMESPACE::pow(base, exponent);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> pow(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& base, int exponent) {
    return DEC_NAMESPACE::pow(base, exponent);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> pow(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& base, double exponent) {
    return DEC_NAMESPACE::pow(base, exponent);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> exp(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& value) {
    return DEC_NAMESPACE::exp(value);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> sqrt(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& value) {
    return DEC_NAMESPACE::sqrt(value);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> abs(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& value) {
    return DEC_NAMESPACE::abs(value);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> max(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& a, 
                                              const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& b) {
    return DEC_NAMESPACE::max(a, b);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> min(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& a, 
                                              const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& b) {
    return DEC_NAMESPACE::min(a, b);
}

} // namespace std

#endif // _DECIMAL_MATH_H__