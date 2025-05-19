// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PERCENT_NUMBER_H
#define __PERCENT_NUMBER_H 1

#include <exception>
#include <memory>
#include <map>
#include <boost/thread/mutex.hpp>
#include "number.h"
#include "DecimalConstants.h"

using std::map;
using std::shared_ptr;


namespace mkc_timeseries
{
  /**
   * @brief Represents a number as a percentage, internally storing it as its decimal equivalent.
   *
   * This class is designed to handle percentage values using a specified Decimal type.
   * For example, a value like "50.0" (representing 50%) is stored internally as 0.50.
   * Instances are created via static factory methods which utilize a cache for efficiency.
   *
   * @tparam Decimal The underlying decimal type to be used for calculations and storage.
   */
  template <class Decimal> class PercentNumber
  {
  public:
    /**
     * @brief Factory method to create a PercentNumber from a Decimal value.
     *
     * This method checks a cache for an existing PercentNumber instance corresponding
     * to the given Decimal value. If found, a copy of the cached instance is returned.
     * Otherwise, a new instance is created, cached, and a copy is returned.
     * The input 'number' is treated as a percentage (e.g., 50.0 for 50%).
     *
     * @param number The Decimal value to be represented as a percentage.
     * @return A const PercentNumber object.
     */
    static const PercentNumber<Decimal> createPercentNumber (const Decimal& number)
    {
      boost::mutex::scoped_lock Lock(mNumberCacheMutex);

      typename map<Decimal, shared_ptr<PercentNumber>>::iterator it = mNumberCache.find (number);

      if (it != mNumberCache.end())
	return *(it->second);
      else
	{
	  std::shared_ptr<PercentNumber> p (new PercentNumber(number));
	  mNumberCache.insert(std::make_pair (number, p));
	  return *p;
	}
    }

    /**
     * @brief Factory method to create a PercentNumber from a string representation.
     *
     * Converts the input string to a Decimal value and then calls the
     * createPercentNumber(const Decimal&) factory method.
     * The input string 'numberString' is treated as a percentage (e.g., "50.0" for 50%).
     *
     * @param numberString A string representing the percentage value.
     * @return A const PercentNumber object.
     * @throw std::runtime_error if the string cannot be converted to Decimal via num::fromString.
     */
    static const PercentNumber<Decimal> createPercentNumber (const std::string& numberString)
    {
      Decimal decNum(num::fromString<Decimal>(numberString));
      return PercentNumber<Decimal>::createPercentNumber (decNum);
    }

    /**
     * @brief Gets the stored percentage value as its decimal equivalent.
     * For example, if the PercentNumber was created for 50% (e.g., from input 50.0 or "50.0"),
     * this method returns a Decimal representing 0.50.
     *
     * @return A const reference to the Decimal value representing the percentage (e.g., 0.50 for 50%).
     */
    const Decimal& getAsPercent() const
    {
      return mPercentNumber;
    }

    /**
     * @brief Copy constructor.
     * @param rhs The PercentNumber object to copy.
     */
    PercentNumber(const PercentNumber&)            = default;

    /**
     * @brief Assignment operator.
     * @param rhs The PercentNumber object to assign from.
     * @return A reference to this PercentNumber object.
     */
    PercentNumber& operator=(const PercentNumber&) = default;
    PercentNumber(PercentNumber&&)                 = default;
    PercentNumber& operator=(PercentNumber&&)      = default;

    /**
     * @brief Destructor.
     */
    ~PercentNumber()                               = default;

  private:
    /**
     * @brief Private constructor to enforce creation via factory methods.
     *
     * Initializes the internal decimal representation by dividing the input 'number'
     * by 100. For example, an input of 50.0 results in mPercentNumber being 0.50.
     *
     * @param number The Decimal value (interpreted as a percentage, e.g., 50.0 for 50%)
     * to initialize from.
     */
    PercentNumber (const Decimal& number) 
      : mPercentNumber (number / DecimalConstants<Decimal>::DecimalOneHundred)
    {}

  private:
    static boost::mutex  mNumberCacheMutex;
    static std::map<Decimal, std::shared_ptr<PercentNumber>> mNumberCache;

    Decimal mPercentNumber;
  };

  /**
   * @brief Less-than comparison operator for PercentNumber objects.
   * @tparam Decimal The underlying decimal type.
   * @param lhs The left-hand side PercentNumber.
   * @param rhs The right-hand side PercentNumber.
   * @return True if lhs is less than rhs, false otherwise.
   */
  template <class Decimal>
  bool operator< (const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs)
  {
    return lhs.getAsPercent() < rhs.getAsPercent();
   }

  /**
   * @brief Greater-than comparison operator for PercentNumber objects.
   * @tparam Decimal The underlying decimal type.
   * @param lhs The left-hand side PercentNumber.
   * @param rhs The right-hand side PercentNumber.
   * @return True if lhs is greater than rhs, false otherwise.
   */
  template <class Decimal>
  bool operator> (const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs){ return rhs < lhs; }

  /**
   * @brief Less-than-or-equal-to comparison operator for PercentNumber objects.
   * @tparam Decimal The underlying decimal type.
   * @param lhs The left-hand side PercentNumber.
   * @param rhs The right-hand side PercentNumber.
   * @return True if lhs is less than or equal to rhs, false otherwise.
   */
  template <class Decimal>
  bool operator<=(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs){ return !(lhs > rhs); }

  /**
   * @brief Greater-than-or-equal-to comparison operator for PercentNumber objects.
   * @tparam Decimal The underlying decimal type.
   * @param lhs The left-hand side PercentNumber.
   * @param rhs The right-hand side PercentNumber.
   * @return True if lhs is greater than or equal to rhs, false otherwise.
   */
  template <class Decimal>
  bool operator>=(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs){ return !(lhs < rhs); }

  /**
   * @brief Equality comparison operator for PercentNumber objects.
   * @tparam Decimal The underlying decimal type.
   * @param lhs The left-hand side PercentNumber.
   * @param rhs The right-hand side PercentNumber.
   * @return True if lhs is equal to rhs, false otherwise.
   */
  template <class Decimal>
  bool operator==(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs)
  {
    return (lhs.getAsPercent() == rhs.getAsPercent());
  }

  /**
   * @brief Inequality comparison operator for PercentNumber objects.
   * @tparam Decimal The underlying decimal type.
   * @param lhs The left-hand side PercentNumber.
   * @param rhs The right-hand side PercentNumber.
   * @return True if lhs is not equal to rhs, false otherwise.
   */
  template <class Decimal>
  bool operator!=(const PercentNumber<Decimal>& lhs, const PercentNumber<Decimal>& rhs)
  { return !(lhs == rhs); }

  template <class Decimal>
  boost::mutex PercentNumber<Decimal>::mNumberCacheMutex;

  template <class Decimal>
  std::map<Decimal, std::shared_ptr<PercentNumber<Decimal>>> PercentNumber<Decimal>::mNumberCache;

  /**
   * @brief Utility function to create a PercentNumber from a string.
   *
   * This function first creates a Decimal from the input string (presumably using
   * a function like `createADecimal` from the `mkc_timeseries` namespace or similar context)
   * and then uses it to create a PercentNumber via its static factory method.
   *
   * @tparam Decimal The underlying decimal type.
   * @param numStr A string representing the percentage value (e.g., "50.0" for 50%).
   * @return A PercentNumber object.
   * @see PercentNumber<Decimal>::createPercentNumber(const std::string&)
   */
  template <class Decimal>
  inline PercentNumber<Decimal>
  createAPercentNumber (const std::string& numStr)
  {
    return PercentNumber<Decimal>::createPercentNumber (createADecimal<Decimal>(numStr));
  }
}



#endif
