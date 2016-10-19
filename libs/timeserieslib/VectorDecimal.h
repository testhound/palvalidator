// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __VECTOR_DECIMAL_H
#define __VECTOR_DECIMAL_H 1

#include <vector>
#include <string>
#include <boost/date_time.hpp>
#include "number.h"

using std::vector;
using std::string;

namespace mkc_timeseries
{
  class VectorDate
  {
  public:
  VectorDate() : mDateSeries()
      {}

  explicit VectorDate(unsigned long numElements) 
    : mDateSeries()
      { mDateSeries.reserve(numElements); }

    ~VectorDate() {}

    unsigned long getNumElements() const 
    { return mDateSeries.size(); }

    void addElement(const boost::gregorian::date& dateValue) 
    { mDateSeries.push_back (dateValue); }

    const boost::gregorian::date& getDate (unsigned long index) const
    {
      return mDateSeries.at(index);
    }

  private:
    vector<boost::gregorian::date> mDateSeries;
  };

  template <class Decimal>
    class VectorDecimal
    {
    public:
    VectorDecimal() : mTimeSeries()
	{}

    explicit VectorDecimal(unsigned long numElements) : mTimeSeries()
	{ mTimeSeries.reserve(numElements); }
      ~VectorDecimal() {}

      unsigned long getNumElements() const { return mTimeSeries.size(); }
      void addElement(const Decimal& value) { mTimeSeries.push_back (value); }
      void setElementAtIndex (const Decimal& value, unsigned long index)
      {
	mTimeSeries.at(index) = value;
      }
  
      const Decimal& getElement (unsigned long index) const
      {
	return mTimeSeries.at(index);
      }

      void swap(size_t i, size_t j) {
        std::swap(mTimeSeries[i], mTimeSeries[j]);
      }

    private:
      vector<Decimal> mTimeSeries;
    };
}
#endif
