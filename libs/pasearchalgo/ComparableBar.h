// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef COMPARABLEBAR_H
#define COMPARABLEBAR_H

#include <array>
#include <iostream>

namespace mkc_searchalgo {

  ///
  /// Representation of the idea of a templated bar structure for comparisons
  ///
  template <class Decimal, unsigned int Dim> class ComparableBar
  {
  public:
    ComparableBar()
    {
      throw;
    }

  private:
    std::array<Decimal, Dim> mOhlcArr[Dim];
    int mOffset;
  };


  ///
  /// A basic OHLC bar is a 4-valued comparable bar
  ///
  template <class Decimal> class ComparableBar<Decimal, 4>
  {
  public:
    ComparableBar(const Decimal& open, const Decimal& high, const Decimal& low, const Decimal& close, unsigned int offset):
      mOhlcArr{open, high, low, close},
      mOffset(offset)
    {}

    template <class D>
    inline friend std::ostream& operator<< (std::ostream& strng, const ComparableBar<D, 4>& obj)
    {
      return strng << obj.mOffset << ":" << obj.mOhlcArr[0] << "|" << obj.mOhlcArr[1] << "|" << obj.mOhlcArr[2] << "|" << obj.mOhlcArr[3];
    }

    template <class D>
    inline friend bool operator==(const ComparableBar<D,4>& lhs, const ComparableBar<D,4>& rhs) { return (lhs.mOhlcArr == rhs.mOhlcArr && lhs.mOffset == rhs.mOffset); }

    ComparableBar<Decimal, 4>&
    operator=(const ComparableBar<Decimal, 4> &rhs)
    {
      if (this == &rhs)
        return *this;

      mOhlcArr = rhs.mOhlcArr;
      mOffset = rhs.mOffset;
      return *this;
    }

    ~ComparableBar()
    {}

    const std::array<Decimal, 4>& getOhlcArr() const { return mOhlcArr; }
    const unsigned int& getOffset() const { return mOffset; }
    void incrementOffset() { mOffset++;}

  private:
    std::array<Decimal, 4> mOhlcArr;
    unsigned int mOffset;
  };

}

#endif // COMPARABLEBAR_H
