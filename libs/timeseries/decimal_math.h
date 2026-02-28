#ifndef _DECIMAL_MATH_H__
#define _DECIMAL_MATH_H__

#include "decimal.h"
#include <cmath>

namespace DEC_NAMESPACE {

/**
 * @brief Mathematical function overloads for decimal types using high-precision xdouble.
 * * This header provides mathematical function overloads for decimal types
 * by converting to xdouble (long double), performing the operation, and converting back.
 * This preserves the 8+ digits of precision required for decimal<8> types.
 */

// Logarithm functions
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> log(const decimal<Prec, RoundPolicy>& value) {
    // getAsXDouble() retrieves the full 64-bit integer potential as a long double
    return decimal<Prec, RoundPolicy>(std::log(value.getAsXDouble()));
}

// Power functions
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> pow(const decimal<Prec, RoundPolicy>& base, const decimal<Prec, RoundPolicy>& exponent) {
    return decimal<Prec, RoundPolicy>(std::pow(base.getAsXDouble(), exponent.getAsXDouble()));
}

template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> pow(const decimal<Prec, RoundPolicy>& base, int exponent) {
    // std::pow is overloaded for (long double, int)
    return decimal<Prec, RoundPolicy>(std::pow(base.getAsXDouble(), exponent));
}

template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> pow(const decimal<Prec, RoundPolicy>& base, double exponent) {
    return decimal<Prec, RoundPolicy>(std::pow(base.getAsXDouble(), static_cast<xdouble>(exponent)));
}

// Exponential function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> exp(const decimal<Prec, RoundPolicy>& value) {
    return decimal<Prec, RoundPolicy>(std::exp(value.getAsXDouble()));
}

// Square root function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> sqrt(const decimal<Prec, RoundPolicy>& value) {
    return decimal<Prec, RoundPolicy>(std::sqrt(value.getAsXDouble()));
}

// Absolute value function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> abs(const decimal<Prec, RoundPolicy>& value) {
    return value.abs(); // Uses internal exact sign manipulation
}

// Maximum function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> max(const decimal<Prec, RoundPolicy>& a, const decimal<Prec, RoundPolicy>& b) {
    return (a > b) ? a : b; // Uses exact int64 comparison
}

// Minimum function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> min(const decimal<Prec, RoundPolicy>& a, const decimal<Prec, RoundPolicy>& b) {
    return (a < b) ? a : b; // Uses exact int64 comparison
}

// Floor function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> floor(const decimal<Prec, RoundPolicy>& value) {
    return value.floor(); // Uses internal fixed-point floor
}

// Ceiling function
template<int Prec, class RoundPolicy>
decimal<Prec, RoundPolicy> ceil(const decimal<Prec, RoundPolicy>& value) {
    return value.ceil(); // Uses internal fixed-point ceil
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

// Note: std::abs is often specialized; we ensure our decimal version is called
template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> abs(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& value) {
    return DEC_NAMESPACE::abs(value);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> floor(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& value) {
    return DEC_NAMESPACE::floor(value);
}

template<int Prec, class RoundPolicy>
DEC_NAMESPACE::decimal<Prec, RoundPolicy> ceil(const DEC_NAMESPACE::decimal<Prec, RoundPolicy>& value) {
    return DEC_NAMESPACE::ceil(value);
}

} // namespace std

#endif // _DECIMAL_MATH_H__
