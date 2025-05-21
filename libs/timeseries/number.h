#ifndef NUMBER_H
#define NUMBER_H

#include <string>
#include "decimal.h"
#include "DecimalConstants.h"

/**
 * @file number.h
 * @brief Provides utility functions for handling decimal numbers, including conversions and rounding.
 *
 * This namespace `num` offers type aliases for specific decimal types and
 * functions for common operations like string conversion, double conversion,
 * absolute value calculation, and rounding to the nearest tick.
 */
namespace num
{
  /**
   * @brief Default decimal type with 7 decimal places using the default rounding policy.
   * @see dec::decimal
   */
  using DefaultNumber  = dec::decimal<7>;

  /**
   * @brief Converts a DefaultNumber to its string representation.
   * @param d The DefaultNumber to convert.
   * @return A std::string representing the decimal number.
   * @see dec::toString()
   */
  inline std::string toString(const DefaultNumber& d) {
    return dec::toString(d);
  }

  /**
   * @brief Converts a DefaultNumber to a double.
   * @param d The DefaultNumber to convert.
   * @return A double representing the decimal number.
   * Note: This conversion may result in a loss of precision.
   * @see dec::decimal::getAsDouble()
   */
  inline double to_double(const DefaultNumber& d) {
    return d.getAsDouble();
  }

  /**
   * @brief Converts a string representation to a decimal type.
   * @tparam N The target decimal type (e.g., DefaultNumber, dec::decimal<P, RP>).
   * @param s The std::string to convert.
   * @return An instance of type N parsed from the string.
   * @see dec::fromString()
   */
  template<class N>
  inline N fromString(const std::string& s) {
    return ::dec::fromString<N>(s);
  }

  /**
   * @brief Calculates the absolute value of a decimal number.
   * @tparam Decimal The type of the decimal number (e.g., DefaultNumber, dec::decimal<P, RP>).
   * @param d The decimal number.
   * @return The absolute value of d as the same Decimal type.
   * @see dec::decimal::abs()
   */
  template<typename Decimal>
  inline Decimal abs(const Decimal& d) {
    return d.abs();
  }

  using mkc_timeseries::DecimalConstants;

  /**
   * @brief Rounds a price to the nearest tick value.
   *
   * This is the primary, three-argument generic implementation. It rounds the given
   * `price` to the nearest multiple of `tick`. The `tickDiv2` parameter is
   * pre-calculated as `tick / 2` and is used to determine whether to round
   * up or down.
   *
   * @tparam Decimal The type of the decimal numbers (e.g., DefaultNumber).
   * @param price The price to be rounded.
   * @param tick The tick size. The price will be rounded to a multiple of this value.
   * @param tickDiv2 Half of the tick size (`tick / 2`). This is used as the threshold for rounding.
   * @return The price rounded to the nearest tick.
   *
   * @details
   * The calculation is as follows:
   * 1. `rem = price % tick;`: Calculates the remainder when `price` is divided by `tick`.
   * This remainder represents how far the `price` is from the immediately lower multiple of `tick`.
   *
   * 2. `price - rem`: This subtracts the remainder from the `price`, effectively rounding down
   * to the nearest lower multiple of `tick`.
   *
   * 3. `((rem < tickDiv2) ? zero : tick)`: This part determines whether to add an additional `tick`
   * to round up or add `zero` to keep the rounded-down value.
   *
   * - If `rem` (the amount above the lower tick boundary) is less than `tickDiv2`
   * (half the tick size), the price is closer to the lower tick boundary, so `zero` is added.
   *
   * - If `rem` is greater than or equal to `tickDiv2`, the price is closer to
   * (or exactly at/above the midpoint towards)
   * the upper tick boundary, so a full `tick` is added to the already rounded-down price.
   */
  template<typename Decimal>
  inline Decimal Round2Tick(Decimal price,
                            Decimal tick,
                            Decimal tickDiv2)
  {
    static const Decimal zero = DecimalConstants<Decimal>::DecimalZero;
    Decimal rem = price % tick;

    // The core rounding logic:
    // 1. (price - rem): This effectively rounds 'price' down to the nearest multiple of 'tick'.
    //    For example, if price = 10.3 and tick = 1.0, rem = 0.3. (price - rem) = 10.0.
    //    If price = 10.8 and tick = 1.0, rem = 0.8. (price - rem) = 10.0.
    //
    // 2. ((rem < tickDiv2) ? zero : tick): This part decides whether to round up or stay
    //    rounded down.
    //    - If 'rem' is less than 'tickDiv2' (half the tick size), it means 'price' is closer
    //      to the lower tick boundary. So, we add 'zero', effectively keeping the rounded-down value.
    //    - If 'rem' is greater than or equal to 'tickDiv2', it means 'price' is closer to
    //      (or at/past the midpoint towards)
    //      the upper tick boundary. So, we add a full 'tick' to the rounded-down value,
    //      moving it to the next tick.
    return price - rem + ((rem < tickDiv2) ? zero : tick);
  }

  /**
   * @brief Rounds a price to the nearest tick value (two-argument generic version).
   *
   * This version calculates `tick / 2` internally and then calls the
   * three-argument `Round2Tick` function.
   *
   * @tparam Decimal The type of the decimal numbers (e.g., DefaultNumber).
   * @param price The price to be rounded.
   * @param tick The tick size. The price will be rounded to a multiple of this value.
   * @return The price rounded to the nearest tick.
   */
  template<typename Decimal>
  inline Decimal Round2Tick(Decimal price,
                            Decimal tick)
  {
    Decimal half = tick / DecimalConstants<Decimal>::DecimalTwo;
    return Round2Tick<Decimal>(price, tick, half);
  }

  /**
   * @brief Overload of Round2Tick for DefaultNumber (two arguments).
   *
   * This overload ensures that existing calls to `Round2Tick` with `DefaultNumber`
   * arguments (without explicit template specification) continue to work.
   * It forwards the call to the generic two-argument template version.
   *
   * @param price The DefaultNumber price to be rounded.
   * @param tick The DefaultNumber tick size.
   * @return The DefaultNumber price rounded to the nearest tick.
   */
  inline DefaultNumber Round2Tick(DefaultNumber price,
                                  DefaultNumber tick)
  {
    return Round2Tick<DefaultNumber>(price, tick);
  }

  /**
   * @brief Overload of Round2Tick for DefaultNumber (three arguments).
   *
   * This overload ensures that existing calls to `Round2Tick` with `DefaultNumber`
   * arguments (without explicit template specification) continue to work.
   * It forwards the call to the generic three-argument template version.
   *
   * @param price The DefaultNumber price to be rounded.
   * @param tick The DefaultNumber tick size.
   * @param tickDiv2 The DefaultNumber representing half of the tick size.
   * @return The DefaultNumber price rounded to the nearest tick.
   */
  inline DefaultNumber Round2Tick(DefaultNumber price,
                                  DefaultNumber tick,
                                  DefaultNumber tickDiv2)
  {
    return Round2Tick<DefaultNumber>(price, tick, tickDiv2);
  }

} // namespace num

#endif // NUMBER_H
